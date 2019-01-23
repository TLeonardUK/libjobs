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

#include <algorithm>

namespace jobs {

scheduler::scheduler()
{
	// Default memory allocation functions.
	m_memory_functions.user_alloc = default_alloc;
	m_memory_functions.user_free = default_free;
}

scheduler::~scheduler()
{
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

	m_memory_functions = functions;

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

    // Allocate threads.
    for (size_t i = 0; i < m_thread_pool_count; i++)
    {
        thread_pool& pool = m_thread_pools[i];

		result result = pool.pool.init(m_memory_functions, pool.thread_count, [&](thread* instance, int index) 
		{
			new(instance) thread(m_memory_functions);

			return instance->init([&]() 
			{
				worker_entry_point(*instance, pool);
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

		result result = pool.pool.init(m_memory_functions, pool.fiber_count, [&](fiber* instance, int index) 
		{
			new(instance) fiber(m_memory_functions);

			return instance->init(pool.stack_size, [&]() 
			{
				worker_fiber_entry_point(*instance);
			});
		});

		if (result != result::success)
		{
			return result;
		}
    }

    // Allocate jobs.
	result result = m_job_pool.init(m_memory_functions, m_max_jobs, [](job* instance, int index)
	{
		new(instance) job(index);
		return result::success;
	});

	if (result != result::success)
	{
		return result;
	}

    // Sort fiber pools by stack size, smallest to largest, saves
    // redundent iteration when looking for smallest fitting stack size.
    auto fiber_sort_predicate = [](const scheduler::fiber_pool* lhs, const scheduler::fiber_pool* rhs) 
    {
        return lhs->stack_size < rhs->stack_size;
    };
    std::sort(m_fiber_pools_sorted_by_stack, m_fiber_pools_sorted_by_stack + m_fiber_pool_count, fiber_sort_predicate);

    return result::success;
}

result scheduler::create_job(job*& instance)
{
	return m_job_pool.alloc(instance);
}

void scheduler::worker_entry_point(const thread& this_thread, const thread_pool& thread_pool)
{
	printf("Worker entered...\n");
	while (!m_destroying)
	{
		execute_next_job(thread_pool.job_priorities, true);
	}
}

void scheduler::worker_fiber_entry_point(const fiber& this_fiber)
{
	printf("Fiber entered...\n");
}

// DEBUG DEBUG DEBUG
#include <Windows.h>
// DEBUG DEBUG DEBUG

bool scheduler::execute_next_job(priority job_priorities, bool can_block)
{	
	/*
	// Grab next job to run.
	job work;
	if (m_jobQueue.get_work(work, job_priorities, can_block))
	{
		// If job does not have a fiber allocated, grab a fiber to run job on.
		if (work.fiber != nullptr)
		{
			work.fiber = allocate_fiber(work.required_stack_size);
		}

		// 
	}
	*/

	// DEBUG DEBUG DEBUG
	Sleep(1);
	// DEBUG DEBUG DEBUG

	return true;
}

}; /* namespace Jobs */