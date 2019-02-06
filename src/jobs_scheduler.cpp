/*
  libjobs - Simple coroutine based job scheduling.
  Copyright (C) 2019 Tim Leonard <me@timleonard.uk>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.
  
  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/

#include "jobs_scheduler.h"
#include "jobs_thread.h"
#include "jobs_fiber.h"
#include "jobs_event.h"
#include "jobs_counter.h"
#include "jobs_utils.h"

#include <stdarg.h>
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <stdlib.h>

#if defined(JOBS_PLATFORM_WINDOWS)
#include <malloc.h>
#endif

#if defined(JOBS_PLATFORM_PS4)
#include <libsysmodule.h>
#include <razorcpu.h>
#endif

namespace jobs {

thread_local size_t scheduler::m_worker_job_index = 0;
thread_local bool scheduler::m_worker_job_completed = false;
thread_local bool scheduler::m_worker_job_supress_requeue = false;
thread_local internal::job_context scheduler::m_worker_job_context;
thread_local internal::job_context* scheduler::m_worker_active_job_context = nullptr;
thread_local internal::fixed_queue<internal::profile_scope_definition*, 32> scheduler::m_profile_scope_thread_local_cache;

scheduler::scheduler()
{
    m_raw_memory_functions.user_alloc = default_alloc;
    m_raw_memory_functions.user_free = default_free;

    m_profile_functions.enter_scope = nullptr;
    m_profile_functions.leave_scope = nullptr;
}

scheduler::~scheduler()
{
    m_destroying = true;

    // Wake up all threads.
    notify_job_available();

    // Join all threads.
    for (size_t i = 0; i < m_thread_pool_count; i++)
    {
        thread_pool& pool = m_thread_pools[i];
        for (size_t j = 0; j < pool.pool.capacity(); j++)
        {
            pool.pool.get_index(j)->join();
        }
    }

    // Destroy all fibers
    // Allocate fibers.
    for (size_t i = 0; i < m_fiber_pool_count; i++)
    {
        fiber_pool& pool = m_fiber_pools[i];
        for (size_t j = 0; j < pool.pool.capacity(); j++)
        {
            pool.pool.get_index(j)->destroy();
        }
    }

    // Platform destruction.
#if defined(JOBS_PLATFORM_PS4)

    // Ensure fiber prx is loaded.
    if (m_owns_scesysmodule_fiber)
    {
        if (sceSysmoduleUnloadModule(SCE_SYSMODULE_FIBER) != SCE_OK)
        {
            write_log(debug_log_verbosity::error, debug_log_group::scheduler, "failed to unload SCE_SYSMODULE_FIBER");
        }

        m_owns_scesysmodule_fiber = false;
    }

#endif
}

void* scheduler::default_alloc(size_t size, size_t alignment)
{
#if defined(JOBS_PLATFORM_WINDOWS)
    void* ptr = _aligned_malloc(size, alignment);
#else
    void* ptr = memalign(alignment, size);
#endif
    return ptr;
}

void scheduler::default_free(void* ptr)
{
#if defined(JOBS_PLATFORM_WINDOWS)
    _aligned_free(ptr);
#else
    free(ptr);
#endif
}

result scheduler::set_memory_functions(const memory_functions& functions)
{
    if (m_initialized)
    {
        return result::already_initialized;
    }

    m_raw_memory_functions = functions;

    return result::success;
}

result scheduler::set_profile_functions(const profile_functions& functions)
{
    if (m_initialized)
    {
        return result::already_initialized;
    }

    m_profile_functions = functions;

    return result::success;
}

result scheduler::set_debug_output(const debug_output_function& function, debug_log_verbosity max_verbosity)
{
    if (m_initialized)
    {
        return result::already_initialized;
    }

    m_debug_output_function = function;
    m_debug_output_max_verbosity = max_verbosity;

    return result::success;
}

result scheduler::set_max_jobs(size_t max_jobs)
{
    if (m_initialized)
    {
        return result::already_initialized;
    }

    m_max_jobs = max_jobs;

    return result::success;
}

result scheduler::set_max_dependencies(size_t max_dependencies)
{
    if (m_initialized)
    {
        return result::already_initialized;
    }

    m_max_dependencies = max_dependencies;

    return result::success;
}

result scheduler::set_max_profile_scopes(size_t max_scopes)
{
    if (m_initialized)
    {
        return result::already_initialized;
    }

    m_max_profile_scopes = max_scopes;

    return result::success;
}

result scheduler::set_max_counters(size_t max_counters)
{
    if (m_initialized)
    {
        return result::already_initialized;
    }

    m_max_counters = max_counters;

    return result::success;
}

result scheduler::set_max_callbacks(size_t max_callbacks)
{
    if (m_initialized)
    {
        return result::already_initialized;
    }

    m_max_callbacks = max_callbacks;

    return result::success;
}

result scheduler::add_thread_pool(size_t thread_count, priority job_priorities)
{
    if (m_initialized)
    {
        return result::already_initialized;
    }
    if (m_thread_pool_count == max_thread_pools)
    {
        return result::maximum_exceeded;
    }

    thread_pool& pool = m_thread_pools[m_thread_pool_count++];
    pool.job_priorities = job_priorities;
    pool.thread_count = thread_count;

    return result::success;
}
    
result scheduler::add_fiber_pool(size_t fiber_count, size_t stack_size)
{
    if (m_initialized)
    {
        return result::already_initialized;
    }
    if (m_fiber_pool_count == max_fiber_pools)
    {
        return result::maximum_exceeded;
    }

    fiber_pool& pool = m_fiber_pools[m_fiber_pool_count++];
    pool.stack_size = stack_size;
    pool.fiber_count = fiber_count;

    return result::success;
}
    
result scheduler::init()
{
    if (m_initialized)
    {
        return result::already_initialized;
    }
    if (m_thread_pool_count == 0)
    {
        return result::no_thread_pools;
    }
    if (m_fiber_pool_count == 0)
    {
        return result::no_fiber_pools;
    }

    // @todo Might be worth changing this so we can initialize
    // multiple times if the first time fails for some reason? Would need
    // to adjust the initialization below so we don't do anything twice.
    m_initialized = true;

    // Trampoline the memory functions so we can log allocations.
    m_memory_functions.user_alloc = [=](size_t size, size_t alignment) -> void* {

        void* ptr = m_raw_memory_functions.user_alloc(size, alignment);

        m_total_memory_allocated += size;
        write_log(debug_log_verbosity::verbose, debug_log_group::memory, "allocated memory block, size=%zi ptr=0x%08p total=%zi", size, ptr, m_total_memory_allocated.load());

        return reinterpret_cast<char*>(ptr);
    };
    m_memory_functions.user_free = [=](void* ptr) {
        return m_raw_memory_functions.user_free(ptr);
    };

    // Platform initialization.
#if defined(JOBS_PLATFORM_PS4)

    // Ensure fiber prx is loaded.
    if (sceSysmoduleIsLoaded(SCE_SYSMODULE_FIBER))
    {
        if (sceSysmoduleLoadModule(SCE_SYSMODULE_FIBER) != SCE_OK)
        {
            write_log(debug_log_verbosity::error, debug_log_group::scheduler, "failed to load SCE_SYSMODULE_FIBER");
            return result::platform_error;
        }
        else
        {
            m_owns_scesysmodule_fiber = true;
        }
    }

    // We do not want razor markers being tagged to fibers, this is dealt with manually.
    sceRazorCpuDisableFiberUserMarkers();

#endif

    // Allocate jobs.
    result result = m_job_pool.init(m_memory_functions, m_max_jobs, [this](internal::job_definition* instance, size_t index)
    {
        new(instance) internal::job_definition(index);
        instance->context.scheduler = this;
        return result::success;
    });

    if (result != result::success)
    {
        return result;
    }

    // Allocate job dependencies.
    result = m_job_dependency_pool.init(m_memory_functions, m_max_dependencies, [](internal::job_dependency* instance, size_t index)
    {
        new(instance) internal::job_dependency(index);
        return result::success;
    });

    if (result != result::success)
    {
        return result;
    }

    // Allocate counters.
    result = m_counter_pool.init(m_memory_functions, m_max_counters, [](internal::counter_definition* instance, size_t index)
    {
        new(instance) internal::counter_definition();
        return result::success;
    });

    if (result != result::success)
    {
        return result;
    }

    // Allocate profile scopes.
    result = m_profile_scope_pool.init(m_memory_functions, m_max_profile_scopes, [](internal::profile_scope_definition* instance, size_t index)
    {
        new(instance) internal::profile_scope_definition();
        return result::success;
    });

    if (result != result::success)
    {
        return result;
    }

    // Allocate callbacks.
    result = m_callback_scheduler.init(this, m_max_callbacks, m_memory_functions);
    if (result != result::success)
    {
        return result;
    }

    // Allocate task queues.
    for (size_t i = 0; i < (int)priority::count; i++)
    {
        result = m_pending_job_queues[i].pending_job_indicies.init(m_memory_functions, m_max_jobs);
        if (result != result::success)
        {
            return result;
        }
    }

    // Allocate fibers.
    for (size_t i = 0; i < m_fiber_pool_count; i++)
    {
        fiber_pool& pool = m_fiber_pools[i];
        m_fiber_pools_sorted_by_stack[i] = &pool;

        result = pool.pool.init(m_memory_functions, pool.fiber_count, [&](internal::fiber* instance, size_t index)
        {
            new(instance) internal::fiber(m_memory_functions);

            char buffer[128];
            memset(buffer, 0, sizeof(buffer));
#if defined(JOBS_PLATFORM_WINDOWS)
            sprintf_s(buffer, 127, "Job (Pool=%zi Index=%zi)", i, index);
#else
            sprintf(buffer, "Job (Pool=%zi Index=%zi)", i, index);
#endif
            return instance->init(pool.stack_size, [this, i, index]()
            {
                worker_fiber_entry_point(i, index);
            }, buffer);
        });

        if (result != result::success)
        {
            return result;
        }
    }

    // Sort fiber pools by stack size, smallest to largest, saves
    // redundent iteration when looking for smallest fitting stack size.
    auto fiber_sort_predicate = [](const scheduler::fiber_pool* lhs, const scheduler::fiber_pool* rhs)
    {
        return lhs->stack_size < rhs->stack_size;
    };
    std::sort(m_fiber_pools_sorted_by_stack, m_fiber_pools_sorted_by_stack + m_fiber_pool_count, fiber_sort_predicate);

    // Allocate threads.
    for (size_t i = 0; i < m_thread_pool_count; i++)
    {
        thread_pool& pool = m_thread_pools[i];

        result = pool.pool.init(m_memory_functions, pool.thread_count, [&](internal::thread* instance, size_t index)
        {
            new(instance) internal::thread(m_memory_functions);

            char buffer[128];
            memset(buffer, 0, sizeof(buffer));
#if defined(JOBS_PLATFORM_WINDOWS)
            sprintf_s(buffer, 127, "Worker (Pool=%zi Index=%zi)", i, index);
#else
            sprintf(buffer, "Worker (Pool=%zi Index=%zi)", i, index);
#endif

            return instance->init([&]()
            {
                worker_entry_point(i, index, *instance, pool);
            }, buffer);
        });

        if (result != result::success)
        {
            return result;
        }
    }

    // Dump out some general logs describing the scheduler setup.
    write_log(debug_log_verbosity::message, debug_log_group::scheduler, "scheduler initialized");
    write_log(debug_log_verbosity::message, debug_log_group::scheduler, "\t%zi bytes allocated", m_total_memory_allocated.load());
    write_log(debug_log_verbosity::message, debug_log_group::scheduler, "\t%i max jobs", m_max_jobs);
    write_log(debug_log_verbosity::message, debug_log_group::scheduler, "\t%i max dependencies", m_max_dependencies);
    write_log(debug_log_verbosity::message, debug_log_group::scheduler, "\t%i max profile scopes", m_max_profile_scopes);
    write_log(debug_log_verbosity::message, debug_log_group::scheduler, "\t%i max counters", m_max_counters);
    write_log(debug_log_verbosity::message, debug_log_group::scheduler, "\t%i max callbacks", m_max_callbacks);
    write_log(debug_log_verbosity::message, debug_log_group::scheduler, "\t%i thread pools", m_thread_pool_count);
    for (size_t i = 0; i < m_thread_pool_count; i++)
    {
        thread_pool& pool = m_thread_pools[i];
        write_log(debug_log_verbosity::message, debug_log_group::scheduler, "\t\t[%i] workers=%i priorities=0x%08x", i, pool.thread_count, pool.job_priorities);
    }
    write_log(debug_log_verbosity::message, debug_log_group::scheduler, "\t%i fiber pools", m_fiber_pool_count);
    for (size_t i = 0; i < m_fiber_pool_count; i++)
    {
        fiber_pool& pool = *m_fiber_pools_sorted_by_stack[i];
        write_log(debug_log_verbosity::message, debug_log_group::scheduler, "\t\t[%i] fibers=%i stack_size=%i", i, pool.fiber_count, pool.stack_size);
    }

    return result::success;
}

result scheduler::create_job(job_handle& instance)
{
    size_t index = 0;

    result res = m_job_pool.alloc(index);
    if (res != result::success)
    {
        write_log(debug_log_verbosity::warning, debug_log_group::scheduler, "attempt to create job, but job pool is empty. Try increasing scheduler::set_max_jobs.");
        return res;
    }

    internal::job_definition& def = get_job_definition(index);

    write_log(debug_log_verbosity::verbose, debug_log_group::scheduler, "job handle allocated, index=%zi ptr=0x%08x", index, &def);

    instance = job_handle(this, index);
    return result::success;
}

void scheduler::free_job(size_t index)
{
    internal::job_definition& def = get_job_definition(index);
    if (def.context.has_fiber)
    {
        free_fiber(def.context.fiber_index, def.context.fiber_pool_index);
        def.context.has_fiber = false;
    }
    clear_job_dependencies(index);
    def.reset();

    write_log(debug_log_verbosity::verbose, debug_log_group::scheduler, "job handle freed, index=%zi ptr=0x%08x", index, &def);

    m_job_pool.free(index);
}

result scheduler::create_event(event_handle& instance, bool auto_reset)
{
    counter_handle counter;

    result res = create_counter(counter);
    if (res != result::success)
    {
        return res;
    }

    instance = event_handle(this, counter, auto_reset);
    return result::success;
}

result scheduler::create_counter(counter_handle& instance)
{
    size_t index = 0;

    result res = m_counter_pool.alloc(index);
    if (res != result::success)
    {
        write_log(debug_log_verbosity::warning, debug_log_group::scheduler, "attempt to create counter, but counter pool is empty. Try increasing scheduler::set_max_counters.");
        return res;
    }

    internal::counter_definition& def = get_counter_definition(index);

    write_log(debug_log_verbosity::verbose, debug_log_group::scheduler, "counter handle allocated, index=%zi ptr=0x%08x", index, &def);

    instance = counter_handle(this, index);
    return result::success;
}

void scheduler::free_counter(size_t index)
{
    internal::counter_definition& def = get_counter_definition(index);
    def.reset();

    write_log(debug_log_verbosity::verbose, debug_log_group::scheduler, "counter handle freed, index=%zi ptr=0x%08x", index, &def);

    m_counter_pool.free(index);
}

internal::counter_definition& scheduler::get_counter_definition(size_t index)
{
    return *m_counter_pool.get_index(index);
}

void scheduler::increase_counter_ref_count(size_t index)
{
    internal::counter_definition& def = get_counter_definition(index);
    size_t new_ref_count = ++def.ref_count;
    //write_log(debug_log_verbosity::verbose, debug_log_group::job, "job handle ++, count=%zi index=%zi ptr=0x%08x", new_ref_count, index, &def);
}

void scheduler::decrease_counter_ref_count(size_t index)
{
    internal::counter_definition& def = get_counter_definition(index);
    size_t new_ref_count = --def.ref_count;
    //write_log(debug_log_verbosity::verbose, debug_log_group::job, "job handle --, count=%zi index=%zi ptr=0x%08x", new_ref_count, index, &def);
    if (new_ref_count == 0)
    {
        free_counter(index);
    }
}

void scheduler::leave_context(internal::job_context& context)
{
    // Remove everything from the profile scope stack.
    if (m_profile_functions.leave_scope != nullptr)
    {
        //printf("[leave_context] leaving %i\n", context.profile_scope_depth);
        for (size_t i = 0; i < context.profile_scope_depth; i++)
        {
            m_profile_functions.leave_scope();
        }
    }
}

void scheduler::enter_context(internal::job_context& context)
{
    internal::fiber* job_fiber = nullptr;
    if (context.is_fiber_raw)
    {
        job_fiber = &context.raw_fiber;
    }	
    else
    {
        job_fiber = m_fiber_pools_sorted_by_stack[context.fiber_pool_index]->pool.get_index(context.fiber_index);
    }

    // Recreate the profile scope stack.
    if (m_profile_functions.enter_scope != nullptr)
    {
        //printf("[enter_context] entering %i\n", context.profile_scope_depth);
        internal::profile_scope_definition* scope = context.profile_stack_head;
        while (scope != nullptr)
        {
            m_profile_functions.enter_scope(scope->type, scope->tag);
            scope = scope->next;
        }
    }

    m_worker_active_job_context = &context;

//	assert(context.job_def == nullptr || context.job_def->status == internal::job_status::running);
    job_fiber->switch_to();
//	assert(context.job_def == nullptr || context.job_def->status == internal::job_status::running);
}

void scheduler::switch_context(internal::job_context& new_context)
{
    leave_context(*m_worker_active_job_context);
    enter_context(new_context);
}

void scheduler::increase_job_ref_count(size_t index)
{
    internal::job_definition& def = get_job_definition(index);
    size_t new_ref_count = ++def.ref_count;
    //write_log(debug_log_verbosity::verbose, debug_log_group::job, "job handle ++, count=%zi index=%zi ptr=0x%08x", new_ref_count, index, &def);
}

void scheduler::decrease_job_ref_count(size_t index)
{
    internal::job_definition& def = get_job_definition(index);
    size_t new_ref_count = --def.ref_count;
    //write_log(debug_log_verbosity::verbose, debug_log_group::job, "job handle --, count=%zi index=%zi ptr=0x%08x", new_ref_count, index, &def);
    if (new_ref_count == 0)
    {
        free_job(index);
    }
}

void scheduler::clear_job_dependencies(size_t job_index)
{
    internal::job_definition& def = get_job_definition(job_index);

    // Clear up predecessors.
    {
        internal::job_dependency* dep = def.first_predecessor;
        while (dep != nullptr)
        {
            size_t pool_index = dep->pool_index;

            internal::job_dependency* next = dep->next;

            dep->reset();
            m_job_dependency_pool.free(pool_index);

            dep = next;
        }
        def.first_predecessor = nullptr;
    }

    // Clear up successors.
    {
        internal::job_dependency* dep = def.first_successor;
        while (dep != nullptr)
        {
            size_t pool_index = dep->pool_index;

            internal::job_dependency* next = dep->next;

            dep->reset();
            m_job_dependency_pool.free(pool_index);

            dep = next;
        }
        def.first_successor = nullptr;
    }
}

result scheduler::add_job_dependency(size_t successor, size_t predecessor)
{
    internal::job_definition& successor_def = get_job_definition(successor);
    internal::job_definition& predecessor_def = get_job_definition(predecessor);

    size_t successor_dep_index;
    size_t predecessor_dep_index;

    result res1 = m_job_dependency_pool.alloc(successor_dep_index);
    result res2 = m_job_dependency_pool.alloc(predecessor_dep_index);

    if (res1 != result::success || res2 != result::success)
    {
        write_log(debug_log_verbosity::warning, debug_log_group::job, "attempt to add job dependency, but dependency pool is empty, if unhandled may cause incorrect job ordering behaviour. Try increasing scheduler::set_max_dependencies.");
        return res1;
    }

    internal::job_dependency* successor_dep = m_job_dependency_pool.get_index(successor_dep_index);
    successor_dep->job = job_handle(this, successor);
    successor_dep->next = predecessor_def.first_successor;

    internal::job_dependency* predecessor_dep = m_job_dependency_pool.get_index(predecessor_dep_index);
    predecessor_dep->job = job_handle(this, predecessor);
    predecessor_dep->next = successor_def.first_predecessor;

    successor_def.first_predecessor = predecessor_dep;
    predecessor_def.first_successor = successor_dep;

    successor_def.pending_predecessors++;

    return result::success;
}

void scheduler::write_log(debug_log_verbosity level, debug_log_group group, const char* message, ...)
{
    if (m_debug_output_function == nullptr)
    {
        return;
    }
    if ((int)level > (int)m_debug_output_max_verbosity)
    {
        return;
    }

    std::lock_guard<std::mutex> lock(m_log_mutex);

    // Format message.
    va_list list;
    va_start(list, message);

    // @todo
    // microsofts behaviour of vsnprintf is significantly different from the standard, so to make sure
    // we're just memsetting this here. Needs to be done correctly.
    memset(m_log_buffer, 0, max_log_size);
    vsnprintf(m_log_buffer, max_log_size - 1, message, list);

    va_end(list);

    // Format log in format:
    //	[verbose] warning: message

    // @todo
    // microsofts behaviour of vsnprintf is significantly different from the standard, so to make sure
    // we're just memsetting this here. Needs to be done correctly.
    memset(m_log_format_buffer, 0, max_log_size);
    snprintf(m_log_format_buffer, max_log_size - 1, "[%s] %s: %s\n", 
        internal::debug_log_group_strings[(int)group], 
        internal::debug_log_verbosity_strings[(int)level], 
        m_log_buffer);

    m_debug_output_function(level, group, m_log_format_buffer);
}

void scheduler::worker_entry_point(size_t pool_index, size_t worker_index, const internal::thread& this_thread, const thread_pool& thread_pool)
{
    m_worker_job_index = 0;
    m_worker_job_context.reset();
    m_worker_job_context.scheduler = this;
    m_worker_job_context.has_fiber = true;
    m_worker_job_context.is_fiber_raw = true;
    m_worker_job_context.raw_fiber = internal::fiber::convert_thread_to_fiber();
    m_worker_job_context.job_def = nullptr;
    m_worker_active_job_context = &m_worker_job_context;

    write_log(debug_log_verbosity::verbose, debug_log_group::worker, "worker started, pool=%zi worker=%zi priorities=0x%08x", pool_index, worker_index, thread_pool.job_priorities);

    m_worker_active_job_context->enter_scope(profile_scope_type::worker, false, "Worker (pool=%zi, index=%zi)", pool_index, worker_index);

    while (!m_destroying)
    {
        execute_next_job(thread_pool.job_priorities, true);
    }

    m_worker_active_job_context->leave_scope();

    write_log(debug_log_verbosity::verbose, debug_log_group::worker, "worker terminated, pool=%zi worker=%zi", pool_index, worker_index);

    internal::fiber::convert_fiber_to_thread();
}

void scheduler::worker_fiber_entry_point(size_t pool_index, size_t worker_index)
{
    write_log(debug_log_verbosity::verbose, debug_log_group::worker, "fiber started, pool=%zi worker=%zi", pool_index, worker_index);

    while (true)
    {
        internal::job_definition& def = get_job_definition(m_worker_job_index);

        // Execute the job assigned to this thread.
        write_log(debug_log_verbosity::verbose, debug_log_group::job, "executing job, index=%zi", m_worker_job_index);

        m_worker_active_job_context->enter_scope(profile_scope_type::fiber, true, def.tag);

        def.work();
        m_worker_job_completed = true;

        m_worker_active_job_context->leave_scope();

        // Switch back to the main worker thread fiber.
        switch_context(m_worker_job_context);
    }

    write_log(debug_log_verbosity::verbose, debug_log_group::worker, "fiber terminated, pool=%zi worker=%zi", pool_index, worker_index);
}

internal::job_definition& scheduler::get_job_definition(size_t index)
{
    return *m_job_pool.get_index(index);
}

result scheduler::dispatch_job(size_t index)
{
    profile_scope scope_(profile_scope_type::user_defined, "dispatch_job");

    internal::job_definition& def = get_job_definition(index);

    if (def.status != internal::job_status::initialized &&
        def.status != internal::job_status::completed)
    {
        return result::already_dispatched;
    }

    write_log(debug_log_verbosity::verbose, debug_log_group::job, "dispatching job, index=%zi", index);

    // Jobs always get an extra ref count until they are complete so they don't get freed while running.
    increase_job_ref_count(index);
    def.status = internal::job_status::pending;
    def.context.queues_contained_in = 0;
    def.context.job_def = &def;

    // Keep track of number of active jobs for idle monitoring. 
    m_active_job_count++;

    // If not dependent on anything, enqueue into queues right now.
    // Put job into a job queue for each priority it holds (not sure why you would want multiple priorities, but might as well support it ...).
    if (def.pending_predecessors == 0)
    {
        requeue_job(index);
    }

    // Wake all workers up so they can pickup task.
    notify_job_available();

    return result::success;
}

result scheduler::requeue_job(size_t index)
{
    profile_scope scope(profile_scope_type::user_defined, "requeue_job");

    internal::job_definition& def = get_job_definition(index);

    //printf("requeue job[%zi]: %s\n", index, def.tag);

    assert(def.pending_predecessors == 0);
    assert(def.status != internal::job_status::waiting_on_counter);
    assert(def.status != internal::job_status::waiting_on_job);
    assert(def.status != internal::job_status::sleeping);

    if (def.status != internal::job_status::sleeping &&
        def.status != internal::job_status::waiting_on_job &&
        def.status != internal::job_status::waiting_on_counter)
    {
        def.status = internal::job_status::pending;
    }

    // Put job into a job queue for each priority it holds (not sure why you would want multiple priorities, but might as well support it ...).
    for (size_t i = 0; i < (int)priority::count; i++)
    {
        size_t mask = 1 << i;

        if (((size_t)def.job_priority & mask) != 0 && (def.context.queues_contained_in & mask) == 0)
        {
            def.context.queues_contained_in |= mask;

            result res = m_pending_job_queues[i].pending_job_indicies.push(index);
            assert(res == result::success);
        }
    }

    return result::success;
}

bool scheduler::get_next_job_from_queue(size_t& output_job_index, job_queue& queue, size_t queue_mask)
{
    bool shifted_last_iteration = false;

    profile_scope scope(profile_scope_type::user_defined, "get_next_job_from_queue");

    //printf("get_next_job_from_queue\n");

    // @todo: this is a failure, we could end up not checking jobs if they are enqueued during? 
    //        We should probably seperate this into a waiting queue for those waiting on dependencies 
    //        and the pending_job_indicies queue which is always available.
    size_t count = queue.pending_job_indicies.count();

    for (size_t i = 0; i < count; i++)
    {
        size_t job_index;

        result res = queue.pending_job_indicies.pop(job_index);
        if (res == result::empty)
        {
            break;
        }

        internal::job_definition& def = get_job_definition(job_index);

        // If job hasn't already started running (because its been picked up from another queue),
        // the mark it as running and return it.
        if (def.status == internal::job_status::pending)
        {
            def.context.queues_contained_in &= ~queue_mask;
            assert(def.pending_predecessors == 0);

            write_log(debug_log_verbosity::verbose, debug_log_group::worker, "Picked up %zi from queue %i", job_index, queue_mask);

            // Make job as running so nothing else attempts to pick it up.
            def.status = internal::job_status::running;

            output_job_index = job_index;
            return true;
        }
    }

    return false;
}

bool scheduler::get_next_job(size_t& job_index, priority priorities, bool can_block)
{
    profile_scope scope(profile_scope_type::user_defined, "get_next_job");

    while (!m_destroying)
    {
        {
            profile_scope scope(profile_scope_type::user_defined, "dequeueing");

            // Look for work in each priority queue we can execute.
            for (size_t i = 0; i < (int)priority::count; i++)
            {
                size_t mask = 1 << i;

                if (((size_t)priorities & mask) != 0)
                {
                    if (get_next_job_from_queue(job_index, m_pending_job_queues[i], mask))
                    {
                        return true;
                    }
                }
            }
        }

        if (!can_block)
        {
            break;
        }

        // Wait for next job.
        wait_for_job_available();
    } 
 
    return false;
}

void scheduler::complete_job(size_t job_index)
{
    profile_scope scope(profile_scope_type::worker, "complete_job");

    internal::job_definition& def = get_job_definition(job_index);

    bool needs_to_wake_up_successors = false;
    
    assert(def.status == internal::job_status::running);
    def.status = internal::job_status::completed;

    // For each job waiting on this one, set it back to pending and requeue it.
    {
        internal::multiple_writer_single_reader_list<internal::job_definition*>::iterator iter;
        for (def.wait_list.iterate(iter); iter; iter++)
        {
            internal::job_definition* wait_def = iter.value();

            internal::job_status expected = internal::job_status::waiting_on_job;
            if (wait_def->status.compare_exchange_strong(expected, internal::job_status::pending))
            {
                requeue_job(wait_def->index);
            }
        }        
    }

    // For each successor, reduce its pending predecessor count.
    internal::job_dependency* dep = def.first_successor;
    while (dep != nullptr)
    {
        internal::job_definition& successor_def = get_job_definition(dep->job.m_index);
        size_t num = --successor_def.pending_predecessors;
        if (num <= 0)
        {
            // Successor is still waiting on dispatch. We can't enqueue it until its 
            // initial setup has completed.
            while (successor_def.status == internal::job_status::initialized)
            {
                // Note: if this is showing up in your profiles, you've fucked up somewhere and not dispatched a dependent job.
                JOBS_YIELD();
            }

            requeue_job(successor_def.index);
            needs_to_wake_up_successors = true;
        }

        dep = dep->next;
    }

    // Clear up the fiber now, even if our handle is going to hang around for a while.
    if (def.context.has_fiber)
    {
        free_fiber(def.context.fiber_index, def.context.fiber_pool_index);
        def.context.has_fiber = false;
    }

    // Clean up dependencies, we no longer need them at this point.
    clear_job_dependencies(job_index);

    // Remove the ref count we added on dispatch.
    decrease_job_ref_count(job_index);

    // Keep track of number of active jobs for idle monitoring. 
    m_active_job_count--;

    // If job had successors, invoke task available cvar to wake everyone up.
    if (needs_to_wake_up_successors)
    {
        notify_job_available();
    }

    // Wake up anyone waiting for tasks to complete.
    {
        notify_job_complete();
    }
}

bool scheduler::execute_next_job(priority job_priorities, bool can_block)
{	
    // Grab next job to run.
    size_t job_index;
    if (get_next_job(job_index, job_priorities, can_block))
    {
        internal::job_definition& def = get_job_definition(job_index);

        // If job does not have a fiber allocated, grab a fiber to run job on.
        if (!def.context.has_fiber)
        {
            result res = allocate_fiber(def.stack_size, def.context.fiber_index, def.context.fiber_pool_index);
            if (res == result::success)
            {
                def.context.has_fiber = true;
            }
            else
            {
                write_log(debug_log_verbosity::warning, debug_log_group::job, "requeuing job as no fibers available, index=%zi", job_index);

                // No fiber available? Back into the queue you go.
                requeue_job(job_index);

                // There is further work to do so return true here.
                return true;
            }
        }

        // To to switcharoo to fiber land.
        m_worker_job_index = job_index;
        m_worker_job_completed = false;
        m_worker_job_supress_requeue = false;

        internal::fiber* job_fiber = m_fiber_pools_sorted_by_stack[def.context.fiber_pool_index]->pool.get_index(def.context.fiber_index);

        write_log(debug_log_verbosity::verbose, debug_log_group::job, "switching job=%zi fiber=%zi:%zi", job_index, def.context.fiber_pool_index, def.context.fiber_index);
        switch_context(def.context);

        // If not complete yet, requeue, we probably have some sync point/dependencies to deal with.
        if (!m_worker_job_completed)
        {
            if (!m_worker_job_supress_requeue)
            {
                // No fiber available? Back into the queue you go.
                requeue_job(job_index);
            }

            // There is further work to do so return true here.
            return true;		
        }
        // Else mark as completed.
        else
        {
            complete_job(m_worker_job_index);
        }
        
        return true;
    }

    return false;
}

result scheduler::allocate_fiber(size_t required_stack_size, size_t& fiber_index, size_t& fiber_pool_index)
{
    bool any_suitable_pools = false;

    for (size_t i = 0; i < m_fiber_pool_count; i++)
    {
        fiber_pool& pool = *m_fiber_pools_sorted_by_stack[i];
        if (pool.stack_size >= required_stack_size)
        {
            any_suitable_pools = true;

            result res = pool.pool.alloc(fiber_index);
            if (res == result::success)
            {
                write_log(debug_log_verbosity::verbose, debug_log_group::job, "fiber allocated, pool=%zi index=%zi", i, fiber_index);

                fiber_pool_index = i;
                return result::success;
            }
        }
    }

    if (!any_suitable_pools)
    {
        write_log(debug_log_verbosity::error, debug_log_group::job, "no fiber pools have large enough stack size to fulfil request for of %zi. job will never complete.", required_stack_size);
        return result::maximum_exceeded;
    }
    else
    {
        return result::out_of_fibers;
    }
}

result scheduler::free_fiber(size_t fiber_index, size_t fiber_pool_index)
{
    fiber_pool& pool = *m_fiber_pools_sorted_by_stack[fiber_pool_index];
    write_log(debug_log_verbosity::verbose, debug_log_group::job, "fiber freed, pool=%zi index=%zi", fiber_pool_index, fiber_index);
    pool.pool.free(fiber_index);
    return result::success;
}

result scheduler::wait_until_idle(timeout wait_timeout)// , priority assist_on_tasks)
{
    internal::stopwatch timer;
    timer.start();

    while (!is_idle() || m_destroying)
    {
        if (timer.get_elapsed_ms() > wait_timeout.duration)
        {
            return result::timeout;
        }

#if 0 // Not supported right now as helper thread must be converted to a fiber.
        if (!execute_next_job(assist_on_tasks, false))
#endif
        {
            std::unique_lock<std::mutex> lock(m_task_complete_mutex);
            
            if (is_idle() || m_destroying)
            {
                break;
            }
            else
            {
                if (wait_timeout.is_infinite())
                {
                    m_task_complete_cvar.wait(lock);
                }
                else
                {
                    size_t ms_remaining = wait_timeout.duration - timer.get_elapsed_ms();
                    if (ms_remaining > 0)
                    {
                        m_task_complete_cvar.wait_for(lock, std::chrono::milliseconds(ms_remaining));
                    }
                }
            }
        }
    }

    return result::success;
}

result scheduler::wait_for_job(job_handle job_handle_in, timeout wait_timeout)//, priority assist_on_tasks)
{
    profile_scope scope(profile_scope_type::worker, "scheduler::wait_for_job");

    // @todo if we are already signaled and not auto-reset, just return.
    internal::job_context* context = get_active_job_context();
    internal::job_context* worker_context = get_worker_job_context();

    // If we have a job context, adds this event to its dependencies and put it to sleep.
    if (context != nullptr)
    {
        assert(worker_context != nullptr);

        volatile bool timeout_called = false;

        // Put job to sleep.
        context->job_def->status = internal::job_status::waiting_on_job;
        context->job_def->wait_job = job_handle_in;

        // Queue a wakeup.
        size_t schedule_handle;
        if (!wait_timeout.is_infinite())
        {
            result res = m_callback_scheduler.schedule(wait_timeout, schedule_handle, [&]() {

                // Do this atomatically to make sure we don't set it to pending after the scheduler 
                // has already done that due to this event being signalled.
                internal::job_status expected = internal::job_status::waiting_on_job;
                if (context->job_def->status.compare_exchange_strong(expected, internal::job_status::pending))
                {
                    timeout_called = true;
                    requeue_job(context->job_def->index);
                    notify_job_available();
                }

            });

            // Failed to schedule a wakeup? Abort.
            if (res != result::success)
            {
                context->job_def->status = internal::job_status::running;
                return res;
            }
        }

        // Attach this to wakeup queue for job.
        bool is_complete = false;
        {
            internal::job_definition& other_job_def = get_job_definition(job_handle_in.m_index);

            std::shared_lock<std::shared_timed_mutex> lock(other_job_def.wait_list.get_mutex());

            // Check it hasn't completed while acquiring lock.
            if (other_job_def.status == internal::job_status::completed)
            {
                context->job_def->status = internal::job_status::running;
                is_complete = true;
            }
            else
            {
                context->job_def->wait_list_link.value = context->job_def;
                other_job_def.wait_list.add(&context->job_def->wait_list_link, false);
            }
        }

        if (!is_complete)
        {
            // Supress requeueing the job, we will do this when the callback returns.
            m_worker_job_supress_requeue = true;

            // Switch back to worker which will requeue fiber for execution later.			
            switch_context(*worker_context);
        }

        // Cleanup
        context->job_def->wait_job = job_handle();

        // If we timed out, just escape here.
        if (timeout_called && !is_complete)
        {
            return result::timeout;
        }

        // Cancel callback and clear our job handle.
        if (!wait_timeout.is_infinite())
        {
            m_callback_scheduler.cancel(schedule_handle);
        }

        return result::success;
    }

    // If we have no job context, we just have to do a blocking wait.
    else
    {
        internal::stopwatch timer;
        timer.start();

        while (!job_handle_in.is_complete())
        {
            if (timer.get_elapsed_ms() > wait_timeout.duration)
            {
                return result::timeout;
            }

    #if 0 // Not supported right now as helper thread must be converted to a fiber.
            if (!execute_next_job(assist_on_tasks, false))
    #endif
            {
                std::unique_lock<std::mutex> lock(m_task_complete_mutex);

                if (job_handle_in.is_complete())
                {
                    break;
                }
                else
                {
                    if (wait_timeout.is_infinite())
                    {
                        m_task_complete_cvar.wait(lock);
                    }
                    else
                    {
                        size_t ms_remaining = wait_timeout.duration - timer.get_elapsed_ms();
                        if (ms_remaining > 0)
                        {
                            m_task_complete_cvar.wait_for(lock, std::chrono::milliseconds(ms_remaining));
                        }
                    }
                }
            }
        }
    }

    return result::success;
}

bool scheduler::is_idle() const
{
    return (m_active_job_count.load() == 0);
}

result scheduler::alloc_scope(internal::profile_scope_definition*& output)
{
    // We maintain a small thread-local list for speedy allocations.
    result res = m_profile_scope_thread_local_cache.pop(output);
    if (res == result::success)
    {
        return res;
    }

    // Otherwise fall back to the main synchronized pool.
    size_t index = 0;
    res = m_profile_scope_pool.alloc(index);
    if (res != result::success)
    {
        return res;
    }

    output = m_profile_scope_pool.get_index(index);
    return result::success;
}

result scheduler::free_scope(internal::profile_scope_definition* scope)
{
    // Free to the small thread-local list for speedy allocations
    result res = m_profile_scope_thread_local_cache.push(scope);
    if (res == result::success)
    {
        return res;
    }

    // Return to main synchronized pool.
    return m_profile_scope_pool.free(scope);
}

result scheduler::sleep(timeout duration)
{
    // I hope this isn't intentional ...
    assert(!duration.is_infinite());

    internal::job_definition* definition = get_active_job_definition();

    // If we have a job context, put it to sleep.
    if (definition != nullptr)
    {
        volatile bool timeout_called = false;

        definition->context.scheduler->write_log(debug_log_verbosity::verbose, debug_log_group::job, "sleeping fiber=%zi:%zi", definition->context.fiber_pool_index, definition->context.fiber_index);

        // Put job to sleep.
        definition->status = internal::job_status::sleeping;

        // Queue a wakeup.
        size_t schedule_handle;
        result res = definition->context.scheduler->m_callback_scheduler.schedule(duration, schedule_handle, [&]() {
            definition->context.scheduler->write_log(debug_log_verbosity::verbose, debug_log_group::job, "wakeup fiber=%zi:%zi", definition->context.fiber_pool_index, definition->context.fiber_index);

            timeout_called = true;

            //printf("Woke up!\n");
            definition->status = internal::job_status::pending;

            definition->context.scheduler->requeue_job(definition->index);

            //printf("Wake workers.\n");
            definition->context.scheduler->notify_job_available();
        });

        // Failed to schedule a wakeup? Abort.
        if (res != result::success)
        {
            definition->status = internal::job_status::pending;
            return res;
        }

        // Supress requeueing the job, we will do this when the callback returns.
        m_worker_job_supress_requeue = true;

        // Switch back to worker which will requeue fiber for execution later.
        definition->context.scheduler->switch_context(m_worker_job_context);

        // We shouldn't be able to get back here without a timeout.
        assert(timeout_called);

        return result::timeout;
    }

    // No job context? Blocking sleep then I guess?
    else
    {
        // Not really sure why you would want this ... But better to support all situations I guess.
        std::this_thread::sleep_for(std::chrono::milliseconds(duration.duration));

        return result::timeout;
    }
}

internal::job_context* scheduler::get_active_job_context()
{
    return m_worker_active_job_context;
}

internal::job_context* scheduler::get_worker_job_context()
{
    return &m_worker_job_context;
}

internal::job_definition* scheduler::get_active_job_definition()
{
    if (m_worker_active_job_context != nullptr)
    {
        return m_worker_active_job_context->job_def;
    }
    else
    {
        return nullptr;
    }
}

void scheduler::notify_job_available()
{
    profile_scope scope_(profile_scope_type::user_defined, "notify_job_available");

    m_task_available_cvar.notify_all();
}

void scheduler::notify_job_complete()
{
    profile_scope scope_(profile_scope_type::user_defined, "notify_job_complete");

    m_task_complete_cvar.notify_all();
}

void scheduler::wait_for_job_available()
{
    profile_scope scope_(profile_scope_type::user_defined, "wait_for_job_available");

    std::unique_lock<std::mutex> lock(m_task_available_mutex);

    if (m_destroying)
    {
        return;
    }

    m_task_available_cvar.wait(lock);
}

size_t scheduler::get_logical_core_count()
{
    return std::thread::hardware_concurrency();
}

}; /* namespace jobs */