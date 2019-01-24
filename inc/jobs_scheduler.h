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

/**
 *  \file jobs_scheduler.h
 *
 *  Include header for job scheduler functionality.
 */

#ifndef __JOBS_SCHEDULER_H__
#define __JOBS_SCHEDULER_H__

#include "jobs_enums.h"
#include "jobs_memory.h"
#include "jobs_job.h"

#include <functional>
#include <condition_variable>
#include <memory>

namespace jobs {
    
class thread;
class fiber;
class job_definition;
class job_dependency;
class job_handle;

/**
 *  \brief User-defined callback function for debugging information.
 *
 *  Function prototype that can be passed into a job scheduler through
 *  set_debug_output, which will be piped all internal output of the scheduler.
 *
 *  \param level verbosity level of the log.
 *  \param group semantic group a log belongs to.
 *  \param message actual text being logged.
 */
typedef std::function<void(debug_log_verbosity level, debug_log_group group, const char* message)> debug_output_function;

/**
 *  The scheduler is the heart of the library. Its responsible for managing the 
 *  creation and execution of all threads, fibers and jobs.
 */
class scheduler
{
public:

	/** Default constructor. */
	scheduler();

    /** Destructor. */
    ~scheduler();

    /**
     * \brief Overrides the default memory allocation functions used by the scheduler.
     *
     * \param functions Struct containing all the memory allocation functions to override.
     * 
     * \return Value indicating the success of this function.
     */
    result set_memory_functions(const memory_functions& functions);

	/**
	 * \brief Provides a function which all debug output will be passed.
	 *
	 * \param function Function to pass all debug output.
	 *
	 * \return Value indicating the success of this function.
	 */
	result set_debug_output(const debug_output_function& function);

    /**
     * \brief Sets the maximum number of jobs.
     *
     * Sets the maximum number of jobs that can be concurrently managed by the scheduler. This 
     * has a direct effect on the quantity of memory allocated by the scheduler when initialized.
     *
     * \param max_jobs New maximum number of jobs.
     * 
     * \return Value indicating the success of this function.
     */
    result set_max_jobs(size_t max_jobs);

	/**
	 * \brief Sets the maximum number of job dependencies.
	 *
	 * Sets the maximum number of jobs dependencies shared between all jobs at a given time.
	 * This has a direct effect on the quantity of memory allocated by the scheduler when initialized.
	 *
	 * \param max_dependencies New maximum number of job dependencies.
	 *
	 * \return Value indicating the success of this function.
	 */
	result set_max_dependencies(size_t max_dependencies);
    
    /**
     * \brief Adds a new pool of worker threads to the scheduler.
     *
     * This creates a new pool of worker threads that can execute jobs queued on this scheduler. 
     * Thread pools can be assigned multiple job priorities, and will only execute jobs queued with one of
     * these priorities. This can be used to segregate long-running and time-critical work onto different threads.
     *
     * \param thread_count Number of threads to create in the new pool.
     * \param job_priorities Bitmask of all the job priorities this thread pool will execute.
     * 
     * \return Value indicating the success of this function.
     */
    result add_thread_pool(size_t thread_count, priority job_priorities = priority::all);
    
    /**
     * \brief Adds a new pool of fibers threads to the scheduler.
     *
     * This creates a new pool of fibers that are used to hold the state of currently executing jobs.
     *
     * Fibers hold the state of a currently executing job, the maximum concurrently running/sleeping/waiting jobs is 
     * equal to that of the number of fibers. 
     *
     * New jobs will always allocate a fiber from the pool with the smallest stack size that fulfills its requested stack size.
     *
     * Multiple pools with different granularities of stack sizes should be created to reduce the memory overhead. Creating a single 
     * large pool with a large stack size will result in unnecessarily high memory usage (as the memory allocated is equal to fiber_count * stack_size).
     *
     * \param fiber_count Number of fibers to create in the new pool.
     * \param stack_size Size of the stack each fiber is allocated.
     * 
     * \return Value indicating the success of this function.
     */
    result add_fiber_pool(size_t fiber_count, size_t stack_size);
    
    /**
     * \brief Initializes this scheduler so it's ready to accept jobs.
     *
     * Initializes this scheduler so it's ready to accept jobs. All allocation of memory is done up-front
     * during the execution of this function.
     * 
     * \return Value indicating the success of this function.
     */
    result init();

	/**
	 * \brief Creates a new job that can later be enqueued for execution.
	 *
	 * Ones a job is fully created and setup, it can be queued for execution by calling queue_job.
	 *
	 * \param instance On success the created job will be stored here.
	 *
	 * \return Value indicating the success of this function.
	 */
	result create_job(job_handle& instance);

	/** @todo */
	result wait_until_idle(timeout wait_timeout = timeout::infinite, priority assist_on_tasks = priority::all_but_slow);

	/** @todo */
	bool is_idle() const;

private:

	/** Internal representation of a thread pool. */
	struct thread_pool
	{
		/** Job priorities this pool can execute. */
		priority job_priorities = priority::all;

		/** Number of threads in this pool. */
		size_t thread_count = 0;

		/** Pool of threads. */
		fixed_pool<thread> pool;
	};

	/** Internal representation of a fiber pool. */
	struct fiber_pool
	{
		/** Size of stack for each fiber in this pool. */
		size_t stack_size = 0;

		/** Number of fibers in this pool. */
		size_t fiber_count = 0;

		/** Pool of fibers. */
		fixed_pool<fiber> pool;
	};

	/** Internal representation of a queue of pending tasks */
	struct job_queue
	{
		/**  @todo */
		size_t* pending_job_indicies = nullptr;

		/**  @todo */
		size_t pending_job_count = 0;
	};

protected:

	friend class job_handle;

	/** @todo */
	job_definition& get_job_definition(size_t index);

	/** @todo */
	void free_job(size_t index);

	/** @todo */
	void increase_job_ref_count(size_t index);

	/** @todo */
	void decrease_job_ref_count(size_t index);

	/** @todo */
	result dispatch_job(size_t index);

	/** @todo */
	bool get_next_job(size_t& job_index, priority priorities, bool can_block);

	/** @todo */
	bool get_next_job_from_queue(size_t& job_index, job_queue& queue);

	/** @todo */
	void complete_job(size_t job_index);

	/** @todo */
	result wait_for_job(job_handle job_handle, timeout wait_timeout = timeout::infinite, priority assist_on_tasks = priority::all_but_slow);

	/** @todo */
	void write_log(debug_log_verbosity level, debug_log_group group, const char* message, ...);

	/** @todo */
	void clear_job_dependencies(size_t job_index);

	/** @todo */
	result add_job_dependency(size_t successor, size_t predecessor);

private:

    /** Default memory allocation function */
    static void* default_alloc(size_t size);

    /** Default memory deallocation function */
    static void default_free(void* ptr);

	/** Entry point for all worker threads. */
	void worker_entry_point(size_t pool_index, size_t worker_index, const thread& this_thread, const thread_pool& thread_pool);

	/** Entry point for all worker job fibers. */
	void worker_fiber_entry_point(size_t pool_index, size_t worker_index, const fiber& this_fiber);

	/**
	 * Executes the next job in the work queue.
	 *
	 * \param job_priorities Bitmask of priorities that can be executed.
	 * \param can_block True if the function can block until a job is available to execute.
	 *
	 * \return True if a job was executed.
	 */
	bool execute_next_job(priority job_priorities, bool can_block);

private:  

    /** Maximum number of threads pools that can be added. */
    const static size_t max_thread_pools = 16;

    /** Maximum number of fiber pools that can be added. */
    const static size_t max_fiber_pools = 16;

    /** User-defined memory allocation functions. */
	memory_functions m_raw_memory_functions;

	/** Trampolined memory functions that also log allocations. */
	memory_functions m_memory_functions;

    /** Maximum number of jobs this scheduler can handle concurrently. */
    size_t m_max_jobs = 0;

    /** Number of thread pools that have been added. */
    size_t m_thread_pool_count = 0;

    /** Array of thread pools that have been added. */
    thread_pool m_thread_pools[max_thread_pools];

    /** Number of fiber pools that have been added. */
    size_t m_fiber_pool_count = 0;

    /** Array of fiber pools that have been added. */
    fiber_pool m_fiber_pools[max_fiber_pools];

	/** Array of pointers to fiber pools, sorted by stack size */
	fiber_pool* m_fiber_pools_sorted_by_stack[max_fiber_pools];

    /** True if this scheduler has been initialized.  */
    bool m_initialized = false;

	/** True if the scheduler is being torn down and threads need to exit. */
	bool m_destroying = false;

	/** Pool of jobs that can be allocated. */
	fixed_pool<job_definition> m_job_pool;

	/** Function to pass all debug output */
	debug_output_function m_debug_output_function = nullptr;

	/** Maximum size of each log message. */
	static const int max_log_size = 1024;

	/** Buffer used for creating log message formats. */
	char m_log_format_buffer[max_log_size];

	/** Buffer used for creating log messages. */
	char m_log_buffer[max_log_size];

	/** Mutex for writing to the log */
	std::mutex m_log_mutex;

	/** Total memory alloacted */
	std::atomic<size_t> m_total_memory_allocated = 0;

	/** Pending job queues, one for each priority. */
	job_queue m_pending_job_queues[(int)priority::count];

	/** Task available mutex */
	std::mutex m_task_available_mutex;

	/** Task available condition variable */
	std::condition_variable m_task_available_cvar;

	/** Task complete mutex */
	std::mutex m_task_complete_mutex;

	/** Task complete condition variable */
	std::condition_variable m_task_complete_cvar;

	/** Number of jobs that have been dispatched but not completed yet. */
	std::atomic<size_t> m_active_job_count = 0;

	/** Maximum number of dependencies jobs can have. */
	size_t m_max_dependencies = 1024;

	/** Pool of dependencies to be allocated. */
	fixed_pool<job_dependency> m_job_dependency_pool;
};

}; /* namespace Jobs */

#endif /* __JOBS_SCHEDULER_H__ */