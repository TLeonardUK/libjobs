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

#include "jobs_defines.h"
#include "jobs_enums.h"
#include "jobs_memory.h"
#include "jobs_job.h"
#include "jobs_utils.h"
#include "jobs_callback_scheduler.h"

#include <functional>
#include <condition_variable>
#include <memory>

namespace jobs {
    
class job_handle;
class event_handle;
class counter_handle;

namespace internal {
    
class profile_scope_definition;
class job_definition;
class job_dependency;
class job_context;
class thread;
class fiber;
class counter_definition;
class callback_scheduler;
class profile_scope_internal;

}; /* namespace internal */

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
 *  \brief User-defined function called when a new profiling scope is entered.
 *
 *  \param type context-specific type of the scope that was entered.
 *  \param tag descriptive tag of the scope that was entered.
 */
typedef std::function<void(profile_scope_type type, const char* tag)> profile_enter_scope_function;

/**
 *  \brief User-defined function called when the last entered profiling scope is left.
 */
typedef std::function<void()> profile_leave_scope_function;

/**
 *  \brief Holds all callback functions for profiling purposes.
 */
struct profile_functions
{
    profile_enter_scope_function enter_scope = nullptr;
    profile_leave_scope_function leave_scope = nullptr;
};

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
     * \brief Overrides the default profiling functions used by the scheduler.
     *
     * \param functions Struct containing all the profiling functions to override.
     *
     * \return Value indicating the success of this function.
     */
    result set_profile_functions(const profile_functions& functions);

    /**
     * \brief Provides a function which all debug output will be passed.
     *
     * \param function Function to pass all debug output.
     * \param max_verbosity Any debug output more verbose than this will be dropped. Saves perf formatting the logs.
     *
     * \return Value indicating the success of this function.
     */
    result set_debug_output(const debug_output_function& function, debug_log_verbosity max_verbosity = debug_log_verbosity::message);

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
     * \brief Sets the maximum number of profile scopes that can be tracked.
     *
     * If you have heavily nested profile scope call graphs, you should increase this value.
     * This has a direct effect on the quantity of memory allocated by the scheduler when initialized.
     *
     * This value is per worker thread.
     *
     * \param max_scopes New maximum number of profile scopes.
     *
     * \return Value indicating the success of this function.
     */
    result set_max_profile_scopes(size_t max_scopes);

    /**
     * \brief Sets the maximum number of counters that can be created and used for syncronization.
     *
     * This capacity is used for both raw counters and events.
     * This has a direct effect on the quantity of memory allocated by the scheduler when initialized.
     *
     * \param max_counters New maximum number of counters.
     *
     * \return Value indicating the success of this function.
     */
    result set_max_counters(size_t max_counters);

    /**
     * \brief Sets the maximum number of latent callbacks that can be scheduld and used for syncronization.
     *
     * A latent callback is created for each wait/sleep call that is provided with a timeout value.
     * This has a direct effect on the quantity of memory allocated by the scheduler when initialized.
     *
     * \param max_callbacks New maximum number of callbacks.
     *
     * \return Value indicating the success of this function.
     */
    result set_max_callbacks(size_t max_callbacks);

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

    /**
     * \brief Creates a new event that can be used for job syncronization.
     *
     * \param instance On success the created event will be stored here.
     * \param auto_reset If the event automatically resets after being signalled, or needs to be manually reset.
     *
     * \return Value indicating the success of this function.
     */
    result create_event(event_handle& instance, bool auto_reset = false);

    /**
     * \brief Creates a new counter that can be used for job syncronization.
     *
     * \param instance On success the created counter will be stored here.
     *
     * \return Value indicating the success of this function.
     */
    result create_counter(counter_handle& instance);
    
    /**
     * \brief Dispatches multiple jobs for execution in a single go.
     *
     * This function combines the overhead for dispatching jobs, reducing 
     * the overall cost to a minimum.
     *
     * \param job_array Pointer to array of jobs to dispatch.
     * \param count Number of jobs in array.
     *
     * \return Value indicating the success of this function.
     */
    result dispatch_batch(job_handle* job_array, size_t count);

    /**
     * \brief Waits until all jobs are complete and the schedulers workers are idle.
     *
     * This call is blocking.
     *
     * \param in_timeout If provided, this function will wait a maximum of this time. If
     *                   the function returns due to a timeout the result provided will be
     *                   result::timeout.
     *
     * \return Value indicating the success of this function.
     */
    result wait_until_idle(timeout wait_timeout = timeout::infinite);

    /**
     * \brief Returns true if all jobs are complete and the scheduler and it's workers are idle.
     *
     * \return True if idle.
     */
    bool is_idle() const;

    /**
     * \brief Puts the job or thread to sleep for the given amount of time.
     *
     * If called from a job this is non-blocking, and will queue the job
     * for execution after the time period has elapsed. If called
     * from any other place, it will block.
     *
     * \param duration The length of time before this function returns.
     *
     * \return Value indicating the success of this function.
     */
    static result sleep(timeout duration = timeout::infinite);

    /**
     * \brief Returns the number of logical cores on the system.
     *
     * This is provided to given a rough idea of the amount of worker 
     * threads that should be spawned by the scheduler to provide maximum
     * utilization of system resources.
     *
     * \return Number of logical cores on the system.
     */
    static size_t get_logical_core_count();

    /**
     * \brief Gets the context of the worker management fiber running individual jobs.
     *
     * Generally this should not need to be called by user-code. Rather the wrappers that
     * interface with the context (profile_scope, job_handle, etc) should be preferred.
     *
     * \return Context of the current threads worker fiber, or nullptr if not called on a worker thread.
     */
    static internal::job_context* get_worker_job_context();

    /**
     * \brief Gets the context of the job currently running on calling thread.
     *
     * Generally this should not need to be called by user-code. Rather the wrappers that
     * interface with the context (profile_scope, job_handle, etc) should be preferred. 
     *
     * \return Context of the currently running job, or nullptr if no job is running on this thread.
     */
    static internal::job_context* get_active_job_context();

    /**
     * \brief Gets if the active scheduler has profiling enabled.
     *
     * Used to early-out of various bits of profiling code.
     *
     * \return True if profiling is active.
     */
    static bool is_profiling_active();

    /**
     * \brief Gets the definition of the job currently running on calling thread.
     *
     * Generally this should not need to be called by user-code. Rather the wrappers that
     * interface with the context (profile_scope, job_handle, etc) should be preferred.
     * Its public on the off chance that user-code wants finer-grain control over things
     * like profiling scope.
     *
     * \return Definition of the currently running job, or nullptr if no job is running on this thread.
     */
    static internal::job_definition* get_active_job_definition();

private:

    /** Internal representation of a thread pool. */
    struct thread_pool
    {
        /** Job priorities this pool can execute. */
        priority job_priorities = priority::all;

        /** Number of threads in this pool. */
        size_t thread_count = 0;

        /** Pool of threads. */
        internal::fixed_pool<internal::thread> pool;
    };

    /** Internal representation of a fiber pool. */
    struct fiber_pool
    {
        /** Size of stack for each fiber in this pool. */
        size_t stack_size = 0;

        /** Number of fibers in this pool. */
        size_t fiber_count = 0;

        /** Pool of fibers. */
        internal::fixed_pool<internal::fiber> pool;
    };

    /** Internal representation of a queue of pending tasks */
    struct job_queue
    {
        /** Queue of all job indices that are currently pending execution. */
        internal::atomic_queue<size_t> pending_job_indicies;
    };

protected:

    friend class job_handle;
    friend class event_handle;
    friend class counter_handle;
    friend class internal::job_context;
    friend class internal::callback_scheduler;
    friend class internal::profile_scope_internal;

    /**
     * \brief Gets a job definition by its pool index.
     *
     * \param index Index into the job pool of definition to retrieve.
     *
     * \return Job definition at the given index.
     */
    JOBS_FORCE_INLINE internal::job_definition& get_job_definition(size_t index)
    {
        return *m_job_pool.get_index(index);
    }

    /**
     * \brief Free's a job based on its pool index. 
     *
     * The job will bre recycled later.
     *
     * \param index Index of job to free.
     */
    void free_job(size_t index);

    /**
     * \brief Increases the reference count of the job based on it's pool index.
     *
     * \param index Index of counter to increment ref count of.
     */
    JOBS_FORCE_INLINE void increase_job_ref_count(size_t index);

    /**
     * \brief Decreases the reference count of the job based on it's pool index.
     *
     * If the reference count goes to 0 the job will be automatically freed.
     *
     * \param index Index of counter to decrement ref count of.
     */
    JOBS_FORCE_INLINE void decrease_job_ref_count(size_t index);

    /**
     * \brief Dispatches a job for execution given it's pool index.
     *
     * \param index Index of job to dispatch.
     *
     * \return Value indicating the success of this function.
     */
    result dispatch_job(size_t index);

    /**
     * \brief Requeues a job that has previously been picked up for execution.
     *
     * This is primarily used when requeueing a job following it being readied
     * by a condition it is waiting on being fulfilled (counter getting to value, job completing, etc).
     *
     * \param index Index of job to requeue.
     *
     * \return Value indicating the success of this function.
     */
    result requeue_job(size_t index);

    /**
     * \brief Requeues a quantity of jobs that have previously been picked up for execution.
     *
     * This works the same as @requeue_job except it combines a lot of the overhead to reduce
     * the overall execution time.
     *
     * \param job_array Array of job handles to requeue.
     * \param count Number of job handles in job_array.
     * \param job_queues Bitmaks of queues that jobs should be enqueued in (only if their priority mask also contains the queue).
     *
     * \return Value indicating the success of this function.
     */
    result requeue_job_batch(job_handle* job_array, size_t count, size_t job_queues);

    /**
     * \brief Gets the next available job from the highest priority queue available.
     *
     * \param job_index Reference to store retrieved job index in.
     * \param priorities Priority queues to look for jobs in.
     * \param can_block If true the function will block until a job is available. Otherwise
     *                  the return value can be used to determine if a job was found.
     *
     * \return True if a job was retrieved.
     */
    bool get_next_job(size_t& job_index, priority priorities, bool can_block);

    /**
     * \brief Gets the next available job a given queue.
     *
     * \param job_index Reference to store retrieved job index in.
     * \param queue Queue to retrieve job from.
     * \param queue_mask Priority mask of queue.
     *
     * \return True if a job was retrieved.
     */
    bool get_next_job_from_queue(size_t& job_index, job_queue& queue, size_t queue_mask);

    /**
     * \brief Completes the given job index.
     *
     * When called this will perform any completion processing required by the job, this includes
     * waking up successors, incrementing counters and so on.
     *
     * \param job_index Job index to complete.
     */
    void complete_job(size_t job_index);

    /**
     * \brief Waits for a job to complete.
     *
     * If called from a job this is non-blocking, and will queue the job
     * for execution after the given job completes. If called
     * from any other place, it will block.
     *
     * \param job_handle Job to wait for completion of.
     * \param in_timeout If provided, this function will wait a maximum of this time. If
     *                   the function returns due to a timeout the result provided will be
     *                   result::timeout.
     *
     * \return Value indicating the success of this function.
     */
    result wait_for_job(job_handle job_handle, timeout wait_timeout = timeout::infinite);

    /**
     * \brief Writes a log message to sink provided by set_debug_output.
     *
     * \param level Verbosity level of log message.
     * \param group Semantic group this message belongs to.
     * \param message Printf style format for this log.
     * \param ... Arguments for printf formatting of message. 
     */
    void write_log(debug_log_verbosity level, debug_log_group group, const char* message, ...);

    /**
     * \brief Clears all the dependencies assigned to the given job index.
     *
     * \param job_index Job index to clear dependencies for.
     */
    void clear_job_dependencies(size_t job_index);

    /**
     * \brief Creates a given dependency between two jobs.
     *
     * \param successor Index of job that should complete last.
     * \param predecessor Index of job that should complete first.
     *
     * \return Value indicating the success of this function.
     */
    result add_job_dependency(size_t successor, size_t predecessor);

    /**
     * \brief Attempts to allocate a fiber out of the available fiber pools with the required stack size.
     *
     * Allocation will always be attempted from pools with the smallest stack size first.
     *
     * \param required_stack_size Minimum stack size required for allocated fiber.
     * \param fiber_index Reference to store index of allocated fiber.
     * \param fiber_pool_index Reference to store pool index of allocated fiber.
     *
     * \return Value indicating the success of this function.
     */
    result allocate_fiber(size_t required_stack_size, size_t& fiber_index, size_t& fiber_pool_index);

    /**
     * \brief Frees a fiber allocated with @allocate_fiber, so it can be recycled later.
     *
     * \param fiber_index Index of allocated fiber.
     * \param fiber_pool_index Pool index of allocated fiber.
     *
     * \return Value indicating the success of this function.
     */
    result free_fiber(size_t fiber_index, size_t fiber_pool_index);

    /**
     * \brief Leaves the given job execution context in preperation for entering another.
     *
     * Leaving a context performs various bits of cleanup worker, such as poping profiler
     * markers off the threads stack, in preperation for a new context being entered.
     *
     * \param old_context Context to leave.
     */
    void leave_context(internal::job_context& old_context);

    /**
     * \brief Enters the given job execution context.
     *
     * Entering a context performs various bits of cleanup worker, such as pushing
     * profiler markers onto the thread stack and resuming from where the context fiber left of.
     *
     * \param new_context Context to enter.
     */
    void enter_context(internal::job_context& new_context);

    /**
     * \brief Switches from current job execution context to a new one.
     *
     * This is a helper function that essentially just calls leave_context on the current context
     * and enter_context on the new context provided.
     *
     * \param new_context Context to switch to.
     */
    void switch_context(internal::job_context& new_context);

    /**
     * \brief Returns execution from the current fiber to the worker thread.
     *
     * \param new_context Context of worker thread to return to.
     * \param supress_requeue If true the worker will not requeue the job when it's switched to.
     */
    void return_to_worker(internal::job_context& new_context, bool supress_requeue);

    /**
     * \brief Allocates a profile scope from the pool.
     *
     * \param output Reference to storage location of allocated scope.
     *
     * \return Value indicating the success of this function.
     */
    result alloc_scope(internal::profile_scope_definition*& output);

    /**
     * \brief Frees a profile scope previously allocated with @alloc_scope.
     *
     * \param scope Scope to free.
     *
     * \return Value indicating the success of this function.
     */
    result free_scope(internal::profile_scope_definition* scope);

    /**
     * \brief Gets a counter definition by its pool index.
     *
     * \param index Index into the counter pool of definition to retrieve.
     *
     * \return Counter definition at the given index.
     */
    JOBS_FORCE_INLINE internal::counter_definition& get_counter_definition(size_t index)
    {
        return *m_counter_pool.get_index(index);
    }

    /**
     * \brief Free's a counter based on its pool index.
     *
     * The counter will be recycled later.
     *
     * \param index Index of counter to free.
     */
    void free_counter(size_t index);

    /**
     * \brief Increases the reference count of the counter based on it's pool index.
     *
     * \param index Index of counter to increment ref count of.
     */
    void increase_counter_ref_count(size_t index);

    /**
     * \brief Decreases the reference count of the counter based on it's pool index.
     *
     * If the reference count goes to 0 the counter will be automatically freed.
     *
     * \param index Index of counter to decrement ref count of.
     */
    void decrease_counter_ref_count(size_t index);

    /**
     * \brief Notifies any workers that a given number of jobs are available for processing.
     *
     * \param job_count Number of jobs that are newly available.
     */
    void notify_job_available(size_t job_count = 1);

    /**
     * \brief Notifies any waiting workers that a job has completed.
     */
    void notify_job_complete();

    /**
     * \brief Blocks until a new job has been singled as being available by @notify_job_available.
     */
    void wait_for_job_available();

private:

    /** Default memory allocation function */
    static void* default_alloc(size_t size, size_t alignment);

    /** Default memory deallocation function */
    static void default_free(void* ptr);

    /** Entry point for all worker threads. */
    void worker_entry_point(size_t pool_index, size_t worker_index, const internal::thread& this_thread, const thread_pool& thread_pool);

    /** Entry point for all worker job fibers. */
    void worker_fiber_entry_point(size_t pool_index, size_t worker_index);

    /**
     * Executes the next job in the work queue.
     *
     * \param job_priorities Bitmask of priorities that can be executed.
     * \param can_block True if the function can block until a job is available to execute.
     *
     * \return True if a job was executed.
     */
    bool execute_next_job(priority job_priorities, bool can_block);

    /** 
     * \brief Executes the job assinged to the fiber we are running within. 
     * 
     * Warning: This is set to force no-inlne to ensure thread_local variables read inside
     * it are not cached on the stack between invocation, which can be problematic
     * with fibers which may return/re-enter at unknown points, which can
     * cause the compiler to cache variables for incorrect lifetimes. This is not
     * needed on windows as we use the /GT flag to enable fiber-safe optimizations.
     */
    JOBS_FORCE_NO_INLINE void execute_fiber_job();

private:  

    /** Maximum number of threads pools that can be added. */
    const static size_t max_thread_pools = 16;

    /** Maximum number of fiber pools that can be added. */
    const static size_t max_fiber_pools = 16;

    /** Maximum size of each log message. */
    static const int max_log_size = 1024;

    /** Maximum number of jobs this scheduler can handle concurrently. */
    size_t m_max_jobs = 100;

    /** Maximum number of dependencies jobs can have. */
    size_t m_max_dependencies = 100;

    /** Maximum number of profile scopes we can have. */
    size_t m_max_profile_scopes = 1000;

    /** Maximum number of counters we can have. */
    size_t m_max_counters = 100;

    /** Maximum number of callbacks we can have. */
    size_t m_max_callbacks = 100;

private:

    /** User-defined memory allocation functions. */
    memory_functions m_raw_memory_functions;

    /** Trampolined memory functions that also log allocations. */
    memory_functions m_memory_functions;

    /** User-defined profiling functions. */
    profile_functions m_profile_functions;

    /** Number of thread pools that have been added. */
    size_t m_thread_pool_count = 0;

    /** Array of thread pools that have been added. */
    thread_pool m_thread_pools[max_thread_pools];

    /** Total number of worker threads. */
    size_t m_worker_count = 0;

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

    /** True if the platform is fiber-aware for profiling purposes. Means we don't have to push/pop profile stack when we switch fiber context. */
    bool m_platform_fiber_aware = false;

    /** Pool of jobs that can be allocated. */
    internal::fixed_pool<internal::job_definition> m_job_pool;

    /** Function to pass all debug output */
    debug_output_function m_debug_output_function = nullptr;

    /** Max output verbosity of log messages. */
    debug_log_verbosity m_debug_output_max_verbosity;

    /** Buffer used for creating log message formats. */
    char m_log_format_buffer[max_log_size];

    /** Buffer used for creating log messages. */
    char m_log_buffer[max_log_size];

    /** Mutex for writing to the log */
    std::mutex m_log_mutex;

    /** Total memory alloacted */
    std::atomic<size_t> m_total_memory_allocated{ 0 };

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
    std::atomic<size_t> m_active_job_count{ 0 };

    /** Number of jobs waiting in queues to be executed. */
    std::atomic<size_t> m_available_jobs{ 0 };

    /** Pool of dependencies to be allocated. */
    internal::fixed_pool<internal::job_dependency> m_job_dependency_pool;

    /** Pool of events that can be allocated. */
    internal::fixed_pool<internal::counter_definition> m_counter_pool;

    /** Instance responsable for queueing and calling latent callbacks. */
    internal::callback_scheduler m_callback_scheduler;

    /** Profiling values */
    internal::fixed_pool<internal::profile_scope_definition> m_profile_scope_pool;

#if defined(JOBS_PLATFORM_PS4)

    /** If true, we loaded and are responsible for destryong the fiber module. */
    bool m_owns_scesysmodule_fiber = false;

#endif

    class worker_thread_state;

    /** Array of worker threads indexed by m_worker_job_index. */
    worker_thread_state* m_worker_thread_states = nullptr;

    /** Scheduler that owns the current worker thread */
    static thread_local scheduler* m_worker_thread_scheduler;

    /** Index of the current worker thread. */
    static thread_local worker_thread_state* m_worker_thread_state;

    /** Determines if profiling is active or not. */
    static bool m_profiling_active;
};

}; /* namespace jobs */

#endif /* __JOBS_SCHEDULER_H__ */