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

#include "jobs_job.h"

#include <cstdio>
#include <cstdarg>
#include <cassert>
#include <algorithm>

namespace jobs {
namespace internal {

job_context::job_context()
{
    reset();
}

void job_context::reset()
{
    queues_contained_in = 0;
    fiber_pool_index = 0;
    fiber_index = 0;
    is_fiber_raw = false;
    profile_scope_depth = 0;

    // Should have been cleaned up by scheduler at this point ...
    assert(profile_stack_head == nullptr);
    assert(profile_stack_tail == nullptr);
    assert(has_fiber == false);
}

result job_context::enter_scope(profile_scope_type type, bool unformatted, const char* tag, ...)
{
    if (scheduler->m_profile_functions.leave_scope == nullptr)
    {
        return result::success;
    }

    profile_scope_definition* scope = nullptr;
    result res = scheduler->alloc_scope(scope);
    if (res != result::success)
    {
        scheduler->write_log(debug_log_verbosity::error, debug_log_group::memory, "Failed to allocate profiling marker. Profiling results may be unprdictable.");
        return res;
    }

    if (unformatted)
    {
        strncpy(scope->tag, tag, profile_scope_definition::max_tag_length - 1);
        scope->tag[profile_scope_definition::max_tag_length - 1] = '\0';
    }
    else
    {
        va_list list;
        va_start(list, tag);
        vsnprintf(scope->tag, profile_scope_definition::max_tag_length, tag, list);
        va_end(list);
    }

    scope->type = type;
    scope->prev = profile_stack_tail;
    scope->next = nullptr;

    if (profile_stack_head == nullptr)
    {
        profile_stack_head = scope;
    }

    if (profile_stack_tail != nullptr)
    {
        profile_stack_tail->next = scope;
    }
    profile_stack_tail = scope;

    profile_scope_depth++;

    scheduler->m_profile_functions.enter_scope(scope->type, scope->tag);

    return result::success;
}

result job_context::leave_scope()
{
    if (scheduler->m_profile_functions.leave_scope == nullptr)
    {
        return result::success;
    }

    profile_scope_definition* original = profile_stack_tail;

    if (profile_stack_tail->prev != nullptr)
    {
        profile_stack_tail->prev->next = nullptr;
    }
    if (profile_stack_tail == profile_stack_head)
    {
        profile_stack_head = nullptr;
    }	

    profile_stack_tail = profile_stack_tail->prev;
    profile_scope_depth--;

    scheduler->m_profile_functions.leave_scope();

    return scheduler->free_scope(original);
}

job_definition::job_definition(size_t in_index)
{
    index = in_index;

    reset();
}	

void job_definition::reset()
{
    ref_count = 0;
    work = nullptr;
    stack_size = 0;
    job_priority = priority::normal;
    status = job_status::initialized;
    tag[0] = '\0';
    pending_predecessors = 0;

    completion_counter = counter_handle();

    wait_counter = counter_handle();
    wait_event = event_handle();
    wait_job = job_handle();

    context.reset();

    // Should have been cleaned up by scheduler at this point ...
    assert(first_predecessor == nullptr);
    assert(first_successor == nullptr);
}

profile_scope_internal::profile_scope_internal(jobs::profile_scope_type type, const char* tag, jobs::scheduler* scheduler)
{
    if (!scheduler::is_profiling_active())
    {
        return;
    }

    internal::job_context* context = scheduler::get_active_job_context();
    if (context != nullptr)
    {
        context->enter_scope(type, true, tag);
    }
    else if (scheduler != nullptr)
    {
        scheduler->m_profile_functions.enter_scope(type, tag);
    }

    m_scheduler = scheduler;
    m_context = context;
}

profile_scope_internal::~profile_scope_internal()
{
    if (!scheduler::is_profiling_active())
    {
        return;
    }

    internal::job_context* context = scheduler::get_active_job_context();

    // Not sure if this situation is even possible, but make sure we always
    // leave on the same context we enter on.
    assert(context == m_context);

    if (context != nullptr)
    {
        context->leave_scope();
    }
    else if (m_scheduler != nullptr)
    {
        m_scheduler->m_profile_functions.leave_scope();
    }
}

}; // namespace internal

job_handle::job_handle(scheduler* scheduler, size_t index)
    : m_scheduler(scheduler)
    , m_index(index)
{
    increase_ref();
}

job_handle::job_handle()
    : m_scheduler(nullptr)
    , m_index(0)
{
}

job_handle::job_handle(const job_handle& other)
{
    m_scheduler = other.m_scheduler;
    m_index = other.m_index;

    increase_ref();
}

job_handle::~job_handle()
{
    decrease_ref();
}

job_handle& job_handle::operator=(const job_handle& other)
{
    m_scheduler = other.m_scheduler;
    m_index = other.m_index;

    increase_ref();

    return *this;
}

void job_handle::increase_ref()
{
    if (m_scheduler != nullptr)
    {
        m_scheduler->increase_job_ref_count(m_index);
    }
}

void job_handle::decrease_ref()
{
    if (m_scheduler != nullptr)
    {
        m_scheduler->decrease_job_ref_count(m_index);
    }
}

result job_handle::set_work(const job_entry_point& job_work)
{
    if (!is_valid())
    {
        return result::invalid_handle;
    }
    if (!is_mutable())
    {
        return result::not_mutable;
    }

    internal::job_definition& definition = m_scheduler->get_job_definition(m_index);
    definition.work = job_work;

    return result::success;
}

result job_handle::set_tag(const char* tag)
{
    if (!is_valid())
    {
        return result::invalid_handle;
    }
    if (!is_mutable())
    {
        return result::not_mutable;
    }

    internal::job_definition& definition = m_scheduler->get_job_definition(m_index);
    size_t tag_len = strlen(tag);
    size_t to_copy = JOBS_MIN(tag_len, internal::job_definition::max_tag_length - 1);
    strncpy(definition.tag, tag, to_copy);
    definition.tag[to_copy] = '\0';

    return result::success;
}

result job_handle::set_stack_size(size_t stack_size)
{
    if (!is_valid())
    {
        return result::invalid_handle;
    }
    if (!is_mutable())
    {
        return result::not_mutable;
    }

    internal::job_definition& definition = m_scheduler->get_job_definition(m_index);
    definition.stack_size = stack_size;

    return result::success;
}

result job_handle::set_priority(priority job_priority)
{
    if (!is_valid())
    {
        return result::invalid_handle;
    }
    if (!is_mutable())
    {
        return result::not_mutable;
    }

    internal::job_definition& definition = m_scheduler->get_job_definition(m_index);
    definition.job_priority = job_priority;

    return result::success;
}

result job_handle::set_completion_counter(const counter_handle& counter)
{
    if (!is_valid())
    {
        return result::invalid_handle;
    }
    if (!is_mutable())
    {
        return result::not_mutable;
    }

    internal::job_definition& definition = m_scheduler->get_job_definition(m_index);
    definition.completion_counter = counter;

    return result::success;
}

result job_handle::clear_dependencies()
{
    if (!is_valid())
    {
        return result::invalid_handle;
    }
    if (!is_mutable())
    {
        return result::not_mutable;
    }

    m_scheduler->clear_job_dependencies(m_index);

    return result::success;
}

result job_handle::add_predecessor(job_handle other)
{
    if (!is_valid())
    {
        return result::invalid_handle;
    }
    if (!is_mutable())
    {
        return result::not_mutable;
    }
    if (!other.is_valid())
    {
        return result::invalid_handle;
    }
    if (!other.is_mutable())
    {
        return result::not_mutable;
    }

    return m_scheduler->add_job_dependency(m_index, other.m_index);
}

result job_handle::add_successor(job_handle other)
{
    if (!is_valid())
    {
        return result::invalid_handle;
    }
    if (!is_mutable())
    {
        return result::not_mutable;
    }
    if (!other.is_valid())
    {
        return result::invalid_handle;
    }
    if (!other.is_mutable())
    {
        return result::not_mutable;
    }

    return m_scheduler->add_job_dependency(other.m_index, m_index);
}

bool job_handle::is_pending()
{
    if (!is_valid())
    {
        return false;
    }

    internal::job_definition& definition = m_scheduler->get_job_definition(m_index);
    return (definition.status == internal::job_status::pending);
}

bool job_handle::is_running()
{
    if (!is_valid())
    {
        return false;
    }

    internal::job_definition& definition = m_scheduler->get_job_definition(m_index);
    return (definition.status == internal::job_status::running);
}

bool job_handle::is_complete()
{
    if (!is_valid())
    {
        return false;
    }

    internal::job_definition& definition = m_scheduler->get_job_definition(m_index);
    return (definition.status == internal::job_status::completed);
}

bool job_handle::is_mutable()
{
    if (!is_valid())
    {
        return false;
    }

    internal::job_definition& definition = m_scheduler->get_job_definition(m_index);

    return definition.status == internal::job_status::initialized ||
           definition.status == internal::job_status::completed;
}

bool job_handle::is_valid()
{
    return (m_scheduler != nullptr);
}

result job_handle::wait(timeout in_timeout)
{
    if (!is_valid())
    {
        return result::invalid_handle;
    }

    return m_scheduler->wait_for_job(*this, in_timeout);
}

result job_handle::dispatch()
{
    if (!is_valid())
    {
        return result::invalid_handle;
    }
    if (!is_mutable())
    {
        return result::not_mutable;
    }

    return m_scheduler->dispatch_job(m_index);
}

bool job_handle::operator==(const job_handle& rhs) const
{
    return (m_scheduler == rhs.m_scheduler && m_index == rhs.m_index);
}

bool job_handle::operator!=(const job_handle& rhs) const
{
    return !(*this == rhs);
}

}; /* namespace jobs */