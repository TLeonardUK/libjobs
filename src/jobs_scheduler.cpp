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
    
scheduler::~scheduler()
{
    for (size_t i = 0; i < m_thread_pool_count; i++)
    {
        thread_pool& pool = m_thread_pools[i];

        if (pool.threads != nullptr)
        {
            m_user_free(pool.threads);
        }
    }

    for (size_t i = 0; i < m_fiber_pool_count; i++)
    {
        fiber_pool& pool = m_fiber_pools[i];

        if (pool.fibers != nullptr)
        {
            m_user_free(pool.fibers);
        }
    }
}

void* scheduler::default_alloc(size_t size)
{
    void* ptr = malloc(size);
    printf("Default Allocated: %zi (%p)\n", size, ptr);
    return ptr;
}

void scheduler::default_free(void* ptr)
{
    free(ptr);
    printf("Default Freed: %p\n", ptr);
}

result scheduler::set_memory_functions(const memory_alloc_function& alloc, const memory_free_function& free)
{
    if (m_initialized)
    {
        return result::already_initialized;
    }

    m_user_alloc = alloc;
    m_user_free = free;

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

    // Allocate threads.
    for (size_t i = 0; i < m_thread_pool_count; i++)
    {
        thread_pool& pool = m_thread_pools[i];

        if (pool.threads == nullptr)
        {
            pool.threads = static_cast<thread*>(m_user_alloc(sizeof(thread) * pool.thread_count));
            if (pool.threads == nullptr)
            {
                return result::out_of_memory;
            }
        }
    }

    // Allocate fibers.
    for (size_t i = 0; i < m_fiber_pool_count; i++)
    {
        fiber_pool& pool = m_fiber_pools[i];

        if (pool.fibers == nullptr)
        {
            pool.fibers = static_cast<fiber*>(m_user_alloc(sizeof(fiber) * pool.fiber_count));
            if (pool.fibers == nullptr)
            {
                return result::out_of_memory;
            }
        }
    }

    // Allocate jobs.
    // todo

    // Sort fiber pools by stack size, smallest to largest, saves
    // redundent iteration when looking for smallest fitting stack size.
    auto fiber_sorter = [](const scheduler::fiber_pool& lhs, const scheduler::fiber_pool& rhs) 
    {
        return lhs.stack_size < rhs.stack_size;
    };
    std::sort(m_fiber_pools, m_fiber_pools + m_fiber_pool_count, fiber_sorter);

    m_initialized = true;

    return result::success;
}

}; /* namespace Jobs */