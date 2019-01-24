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

#include <stdarg.h>
#include <algorithm>
#include <cassert>

namespace jobs {

scheduler::scheduler()
{
	// Default memory allocation functions.
	m_raw_memory_functions.user_alloc = default_alloc;
	m_raw_memory_functions.user_free = default_free;
}

scheduler::~scheduler()
{
	// Allocate task queues.
	for (size_t i = 0; i < (int)priority::count; i++)
	{
		if (m_pending_job_queues[i].pending_job_indicies != nullptr)
		{
			m_memory_functions.user_free(m_pending_job_queues[i].pending_job_indicies);
		}
	}

	m_destroying = true;
}

void* scheduler::default_alloc(size_t size)
{
    return malloc(size);
}

void scheduler::default_free(void* ptr)
{
    free(ptr);
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

result scheduler::set_debug_output(const debug_output_function& function)
{
	if (m_initialized)
	{
		return result::already_initialized;
	}

	m_debug_output_function = function;

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
	m_memory_functions.user_alloc = [=](size_t size) -> void* {

		/** @todo: Do we care about alignment here? size_t alignment should be fine? */
		void* ptr = m_raw_memory_functions.user_alloc(size + sizeof(size_t));
		*reinterpret_cast<size_t*>(ptr) = size;

		m_total_memory_allocated += size;

		write_log(debug_log_verbosity::verbose, debug_log_group::memory, "allocated memory block, size=%zi ptr=0x%08p total=%zi", size, ptr, m_total_memory_allocated.load());

		return reinterpret_cast<char*>(ptr) + sizeof(size_t);
	};
	m_memory_functions.user_free = [=](void* ptr) {

		void* base_ptr = reinterpret_cast<char*>(ptr) - sizeof(size_t);

		size_t size = *reinterpret_cast<size_t*>(base_ptr);

		m_total_memory_allocated -= size;

		write_log(debug_log_verbosity::verbose, debug_log_group::memory, "freeing memory block, size=%zi ptr=0x%08p total=%zi", size, base_ptr, m_total_memory_allocated.load());
		return m_raw_memory_functions.user_free(base_ptr);
	};

    // Allocate threads.
    for (size_t i = 0; i < m_thread_pool_count; i++)
    {
        thread_pool& pool = m_thread_pools[i];

		result result = pool.pool.init(m_memory_functions, pool.thread_count, [&](thread* instance, size_t index)
		{
			new(instance) thread(m_memory_functions);

			return instance->init([&]() 
			{
				worker_entry_point(i, index, *instance, pool);
			});
		});

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

		result result = pool.pool.init(m_memory_functions, pool.fiber_count, [&](fiber* instance, size_t index)
		{
			new(instance) fiber(m_memory_functions);

			return instance->init(pool.stack_size, [&]() 
			{
				worker_fiber_entry_point(i, index, *instance);
			});
		});

		if (result != result::success)
		{
			return result;
		}
    }

    // Allocate jobs.
	result result = m_job_pool.init(m_memory_functions, m_max_jobs, [](job_definition* instance, size_t index)
	{
		new(instance) job_definition();
		return result::success;
	});

	if (result != result::success)
	{
		return result;
	}

	// Allocate job dependencies.
	result = m_job_dependency_pool.init(m_memory_functions, m_max_dependencies, [](job_dependency* instance, size_t index)
	{
		new(instance) job_dependency(index);
		return result::success;
	});

	if (result != result::success)
	{
		return result;
	}

	// Allocate task queues.
	for (size_t i = 0; i < (int)priority::count; i++)
	{
		m_pending_job_queues[i].pending_job_count = 0;
		m_pending_job_queues[i].pending_job_indicies = (size_t*)m_memory_functions.user_alloc(sizeof(size_t) * m_max_jobs);
	}

    // Sort fiber pools by stack size, smallest to largest, saves
    // redundent iteration when looking for smallest fitting stack size.
    auto fiber_sort_predicate = [](const scheduler::fiber_pool* lhs, const scheduler::fiber_pool* rhs) 
    {
        return lhs->stack_size < rhs->stack_size;
    };
    std::sort(m_fiber_pools_sorted_by_stack, m_fiber_pools_sorted_by_stack + m_fiber_pool_count, fiber_sort_predicate);

	// Dump out some general logs describing the scheduler setup.
	write_log(debug_log_verbosity::message, debug_log_group::scheduler, "scheduler initialized");
	write_log(debug_log_verbosity::message, debug_log_group::scheduler, "\t%zi bytes allocated", m_total_memory_allocated.load());
	write_log(debug_log_verbosity::message, debug_log_group::scheduler, "\t%i max jobs", m_max_jobs);
	write_log(debug_log_verbosity::message, debug_log_group::scheduler, "\t%i max dependencies", m_max_dependencies);
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
		write_log(debug_log_verbosity::message, debug_log_group::scheduler, "\t\t[%i] workers=%i stack_size=%i", i, pool.fiber_count, pool.stack_size);
	}

    return result::success;
}

result scheduler::create_job(job_handle& instance)
{
	size_t index = 0;

	result res = m_job_pool.alloc(index);
	if (res != result::success)
	{
		return res;
	}

	write_log(debug_log_verbosity::verbose, debug_log_group::job, "job handle allocated, index=%zi", index);

	instance = job_handle(this, index);
	return result::success;
}

void scheduler::free_job(size_t index)
{
	job_definition& def = get_job_definition(index);
	clear_job_dependencies(index);
	def.reset();

	write_log(debug_log_verbosity::verbose, debug_log_group::job, "job handle freed, index=%zi", index);

	m_job_pool.free(index);
}

void scheduler::increase_job_ref_count(size_t index)
{
	job_definition& def = get_job_definition(index);
	def.ref_count++;
}

void scheduler::decrease_job_ref_count(size_t index)
{
	job_definition& def = get_job_definition(index);
	size_t new_ref_count = --def.ref_count;
	if (new_ref_count == 0)
	{
		free_job(index);
	}
}

void scheduler::clear_job_dependencies(size_t job_index)
{
	job_definition& def = get_job_definition(job_index);

	job_dependency* dep = def.first_dependency;
	while (dep != nullptr)
	{
		size_t pool_index = dep->pool_index;

		job_dependency* next = dep->next;

		dep->reset();
		m_job_dependency_pool.free(pool_index);

		dep = next;
	}

	def.first_dependency = nullptr;
}

result scheduler::add_job_dependency(size_t successor, size_t predecessor)
{
	job_definition& successor_def = get_job_definition(successor);
	job_definition& predecessor_def = get_job_definition(predecessor);

	size_t dependency_index;
	result res = m_job_dependency_pool.alloc(dependency_index);
	if (res != result::success)
	{
		return res;
	}

	job_dependency* dependency = m_job_dependency_pool.get_index(dependency_index);
	dependency->job = job_handle(this, predecessor);
	dependency->next = successor_def.first_dependency;

	predecessor_def.has_successors = true;
	successor_def.first_dependency = dependency;

	return result::success;
}

void scheduler::write_log(debug_log_verbosity level, debug_log_group group, const char* message, ...)
{
	if (m_debug_output_function == nullptr)
	{
		return;
	}

	std::lock_guard<std::mutex> lock(m_log_mutex);

	// Format message.
	va_list list;
	va_start(list, message);
	int length = vsnprintf(m_log_buffer, max_log_size, message, list);
	m_log_buffer[length] = '\0';
	va_end(list);

	// Format log in format:
	//	[verbose] warning: message
	length = snprintf(m_log_format_buffer, max_log_size, "[%s] %s: %s\n", debug_log_group_strings[(int)group], debug_log_verbosity_strings[(int)level], m_log_buffer);
	m_log_format_buffer[length] = '\0';

	m_debug_output_function(level, group, m_log_format_buffer);
}

void scheduler::worker_entry_point(size_t pool_index, size_t worker_index, const thread& this_thread, const thread_pool& thread_pool)
{
	write_log(debug_log_verbosity::verbose, debug_log_group::worker, "worker started, pool=%zi worker=%zi priorities=0x%08x", pool_index, worker_index, thread_pool.job_priorities);

	while (!m_destroying)
	{
		execute_next_job(thread_pool.job_priorities, true);
	}

	write_log(debug_log_verbosity::verbose, debug_log_group::worker, "worker terminated, pool=%zi worker=%zi", pool_index, worker_index);
}

void scheduler::worker_fiber_entry_point(size_t pool_index, size_t worker_index, const fiber& this_fiber)
{
	write_log(debug_log_verbosity::verbose, debug_log_group::worker, "fiber started, pool=%zi worker=%zi", pool_index, worker_index);

	// @todo

	write_log(debug_log_verbosity::verbose, debug_log_group::worker, "fiber terminated, pool=%zi worker=%zi", pool_index, worker_index);
}

job_definition& scheduler::get_job_definition(size_t index)
{
	return *m_job_pool.get_index(index);
}

result scheduler::dispatch_job(size_t index)
{
	std::unique_lock<std::mutex> lock(m_task_available_mutex);

	job_definition& def = get_job_definition(index);

	if (def.status == job_status::running ||
		def.status == job_status::pending)
	{
		return result::already_dispatched;
	}

	write_log(debug_log_verbosity::verbose, debug_log_group::job, "dispatching job, index=%zi", index);

	// Jobs always get an extra ref count until they are complete so they don't get freed while running.
	increase_job_ref_count(index);
	def.status = job_status::pending;

	// Keep track of number of active jobs for idle monitoring. 
	m_active_job_count++;

	// Put job into a job queue for each priority it holds (not sure why you would want multiple priorities, but might as well support it ...).
	for (size_t i = 0; i < (int)priority::count; i++)
	{
		size_t mask = 1i64 << i;

		if (((size_t)def.job_priority & mask) != 0)
		{
			// We should only be able to allocate max_jobs jobs, and each one can only be dispatched 
			// at most once at a time. So we should never run out of space in any queues ...
			assert(m_pending_job_queues[i].pending_job_count < m_max_jobs);

			size_t write_index = m_pending_job_queues[i].pending_job_count++;
			m_pending_job_queues[i].pending_job_indicies[write_index] = index;
		}
	}

	// Wake all workers up so they can pickup task.
	m_task_available_cvar.notify_all();

	return result::success;
}

bool scheduler::get_next_job_from_queue(size_t& output_job_index, job_queue& queue)
{
	bool shifted_last_iteration = false;

	for (size_t i = 0; i < queue.pending_job_count; /* No increment */)
	{
		size_t job_index = queue.pending_job_indicies[i];
		job_definition& def = get_job_definition(job_index);

		bool remove_job = false;
		bool return_job = false;
		bool shift_job_to_back = false;

		// If job has already started running (because its been picked up from another queue), 
		// then remove it from the job list.
		if (def.status != job_status::pending)
		{
			remove_job = true;
		}
		else
		{
			bool dependencies_satisfied = true;

			// Check dependencies are all satisfied.
			job_dependency* dep = def.first_dependency;
			while (dep != nullptr)
			{
				if (!dep->job.is_complete())
				{
					dependencies_satisfied = false;
					break;
				}
				dep = dep->next;
			}

			if (dependencies_satisfied)
			{
				remove_job = true;
				return_job = true;
			}
			else
			{
				shift_job_to_back = true;
			}
		}

		bool did_shift_last_iteration = shifted_last_iteration;
		shifted_last_iteration = false;

		if (remove_job)
		{
			queue.pending_job_indicies[i] = queue.pending_job_indicies[queue.pending_job_count - 1];
			queue.pending_job_count--;

			// Next iteration we stay on the same index to check the new job we just placed in it.
		}
		else if (shift_job_to_back)
		{
			// If the dependencies for this job has failed, shift it to the back of the queue so we don't
			// check it uneccessarily in future.
			queue.pending_job_indicies[i] = queue.pending_job_indicies[queue.pending_job_count - 1];
			queue.pending_job_indicies[queue.pending_job_count - 1] = job_index;

			// Next iteration we stay on the same index to check the new job we just placed in it.
			// Except if last iteration was a shift as well, otherwise we will just keep shifting.
			if (did_shift_last_iteration)
			{
				i++;
			}

			shifted_last_iteration = true;
		}
		else
		{
			// Next iteration should move to next job.
			i++;
		}

		if (return_job)
		{
			// Make job as running so nothing else attempts to pick it up.
			def.status = job_status::running;

			output_job_index = job_index;
			return true;
		}
	}

	return false;
}

bool scheduler::get_next_job(size_t& job_index, priority priorities, bool can_block)
{
	std::unique_lock<std::mutex> lock(m_task_available_mutex);

	while (!m_destroying)
	{
		// Look for work in each priority queue we can execute.
		for (size_t i = 0; i < (int)priority::count; i++)
		{
			size_t mask = 1i64 << i;

			if (((size_t)priorities & mask) != 0)
			{
				if (get_next_job_from_queue(job_index, m_pending_job_queues[i]))
				{
					return true;
				}
			}
		}

		if (!can_block)
		{
			break;
		}

		m_task_available_cvar.wait(lock);
	} 
 
	return false;
}

void scheduler::complete_job(size_t job_index)
{
	job_definition& def = get_job_definition(job_index);

	def.status = job_status::completed;

	bool has_successors = def.has_successors;

	// Remove the ref count we added on dispatch.
	decrease_job_ref_count(job_index);

	// Keep track of number of active jobs for idle monitoring. 
	m_active_job_count--;

	// If job had successors, invoke task available cvar to wake everyone up.
	if (has_successors)
	{
		std::unique_lock<std::mutex> lock(m_task_available_mutex);
		m_task_available_cvar.notify_all();
	}

	// Wake up anyone waiting for tasks to complete.
	{
		std::unique_lock<std::mutex> lock(m_task_complete_mutex);
		m_task_complete_cvar.notify_all();
	}
}

bool scheduler::execute_next_job(priority job_priorities, bool can_block)
{	
	// Grab next job to run.
	size_t job_index;
	if (get_next_job(job_index, job_priorities, can_block))
	{
		job_definition& def = get_job_definition(job_index);

		// If job does not have a fiber allocated, grab a fiber to run job on.
		//if (work.fiber != nullptr)
		//{
			//work.fiber = allocate_fiber(work.required_stack_size);
		//}

		// @todo: do properly.
		write_log(debug_log_verbosity::verbose, debug_log_group::job, "executing job, index=%zi", job_index);
		def.work();

		// Mark as completed.
		complete_job(job_index);

		return true;
	}

	return false;
}

result scheduler::wait_until_idle(timeout wait_timeout, priority assist_on_tasks)
{
	stopwatch timer;
	timer.start();

	while (!is_idle())
	{
		if (timer.get_elapsed_ms() > wait_timeout.duration)
		{
			return result::timeout;
		}

		if (!execute_next_job(assist_on_tasks, false))
		{
			std::unique_lock<std::mutex> lock(m_task_complete_mutex);
			
			if (is_idle())
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

result scheduler::wait_for_job(job_handle job_handle, timeout wait_timeout, priority assist_on_tasks)
{
	stopwatch timer;
	timer.start();

	while (!job_handle.is_complete())
	{
		if (timer.get_elapsed_ms() > wait_timeout.duration)
		{
			return result::timeout;
		}

		if (!execute_next_job(assist_on_tasks, false))
		{
			std::unique_lock<std::mutex> lock(m_task_complete_mutex);

			if (job_handle.is_complete())
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

bool scheduler::is_idle() const
{
	return (m_active_job_count.load() == 0);
}

}; /* namespace Jobs */