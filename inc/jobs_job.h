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
 *  \file jobs_job.h
 *
 *  Include header for individual job management functionality.
 */

#ifndef __JOBS_JOB_H__
#define __JOBS_JOB_H__

#include <atomic>

#include "jobs_defines.h"
#include "jobs_utils.h"
#include "jobs_enums.h"
#include "jobs_scheduler.h"
#include "jobs_fiber.h"
#include "jobs_event.h"
#include "jobs_counter.h"

namespace jobs {

class scheduler;

/**
 *  \brief Entry point for a jobs workload.
 */
typedef std::function<void()> job_entry_point;

namespace internal {
    
class job_definition;
class job_dependency;
class job_context;
class profile_scope_definition;

/**
 * Holds the execution context of a job, this provides various functionality to 
 * manipulate the execution of a job, such as waiting for events, creating profile scopes, etc.
 */
class job_context
{
protected:

    friend class jobs::scheduler;
    friend class jobs::event_handle;
    friend class jobs::counter_handle;

    /** True if this job context has been assigned a fiber. */
    bool has_fiber = false;

    /** If the fiber assigned to this context is held in @raw_fiber or indirectly through @fiber_index / @fiber_pool_index. */
    bool is_fiber_raw;

    /** Index of the fiber (inside its pool) assigned to this context. */
    size_t fiber_index;

    /** Index of the fiber pool the fiber assigned to this context is contained in. */
    size_t fiber_pool_index;

    /** Raw fiber assigned to this context, rather than a pooled fiber. */
    fiber raw_fiber;

    /** Bitmask of all queues the job being run is contained in. */
    size_t queues_contained_in;

    /** Depth of profile marker stack. */
    size_t profile_scope_depth;

    /** Head of profile marker stack linked list. */
    profile_scope_definition* profile_stack_head = nullptr;

    /** Tail of profile marker stack linked list. */
    profile_scope_definition* profile_stack_tail = nullptr;

    /** Scheduler that owns this context. */
    jobs::scheduler* scheduler = nullptr;

    /** Definition of job being run by this context. */
    job_definition* job_def = nullptr;
    
public:

    /** Constructor. */
    job_context();

    /** Resets all data in this context ready for it to be recycled. */
    void reset();

    /**
     * \brief Pushes another profile marker onto the stack.
     *
     * \param type Semantic type of code area this profile marker is marking.
     * \param unformatted If true, printf style formatting on arguments will be skipped.
     * \param tag Descriptive tag of maker. If unformatted is false this is used as a printf format form the varidic arguments.
     *
     * \return Value indicating the success of this function.
     */
    result enter_scope(profile_scope_type type, bool unformatted, const char* tag, ...);

    /**
     * \brief Pops the top profile marker off the stack.
     *
     * \return Value indicating the success of this function.
     */
    result leave_scope();

};

/**
 *  \brief Current status of a job.
 */
enum class job_status
{
    initialized,        /**< Job is initialized and ready for dispatch */
    pending,            /**< Job is pending execution */
    running,            /**< Job is running on a worker */
    sleeping,           /**< Job is sleeping. */
    waiting_on_counter, /**< Job is waiting for a counter. */
    waiting_on_job,     /**< Job is waiting explicitly (eg. job.wait rather than a dependency) for a job to complete. */
    completed,          /**< Job has completed running */
};

}; /* nemspace internal */

/**
 * \brief Represents an instance of a job that has been created by the scheduler.
 *
 * Job data is owned by the scheduler, be careful accessing handles if
 * the scheduler has been destroyed.
 */
class job_handle
{
protected:

    friend class scheduler;

    /**
     * \brief Constructor
     *
     * \param scheduler Scheduler that owns this job.
     * \param index Index into the scheduler's job pool where this jobs data is held.
     */
    job_handle(scheduler* scheduler, size_t index);

    /** Increases the reference count of this job. */
    void increase_ref();

    /** Decreases the reference count of this job. When it reaches zero, it will be disposed of and recycled. */
    void decrease_ref();

public:

    /** Constructor */
    job_handle();

    /**
     * \brief Copy constructor
     *
     * \param other Object to copy.
     */
    job_handle(const job_handle& other);

    /** Destructor */
    ~job_handle();

    /**
     * \brief Sets the function to call when this job is executed.
     *
     * \param job_work Function to call on execution.
     *
     * \return Value indicating the success of this function.
     */
    result set_work(const job_entry_point& job_work);

    /**
     * \brief Sets the descriptive name of this job.
     *
     * \param tag Name of this job.
     *
     * \return Value indicating the success of this function.
     */
    result set_tag(const char* tag);

    /**
     * \brief Sets the minimum stack size required for this job to run.
     *
     * The scheduler will always attempt to allocate fibers to jobs, from a 
     * fiber pool with a smaller stack-size first. If all pools are empty
     * or no pool exists that meets the minimum stack-size, the job will 
     * never complete.
     *
     * \param stack_size Minimum stack size required for this job to run.
     *
     * \return Value indicating the success of this function.
     */
    result set_stack_size(size_t stack_size);

    /**
     * \brief Sets the priority of this job.
     *
     * Queues are created for each priority, if you want a job to exist in multiple
     * queues then priorities can be combined together into a bitmask (the job will only be executed
     * once though). Worker thread pools can also be assigned to only work on jobs with
     * specific priorities. Combining both of these can give fine grain control over when and where
     * individual jobs are executed.
     *
     * \param job_priority Priority to assiociated with this job.
     *
     * \return Value indicating the success of this function.
     */
    result set_priority(priority job_priority);

    /**
     * \brief Sets a counter that will be incremented when the job completes.
     *
     * \param counter Handle of counter to increment when complete.
     *
     * \return Value indicating the success of this function.
     */
    result set_completion_counter(const counter_handle& counter);

    /**
     * \brief Clears the internal dependency list for this job.
     *
     * \return Value indicating the success of this function.
     */
    result clear_dependencies();

    /**
     * \brief Adds a predecessor job that will always execute before this
     *        job executes.
     *        
     * Care should be taken not to create circular dependencies which
     * will cause jobs to never complete.
     *
     * \param other Handle of job to add as dependency.
     *
     * \return Value indicating the success of this function.
     */
    result add_predecessor(job_handle other);

    /**
     * \brief Adds a successor job that will always execute after this
     *        job executes.
     *
     * Care should be taken not to create circular dependencies which
     * will cause jobs to never complete.
     *
     * \param other Handle of job to add as dependency.
     *
     * \return Value indicating the success of this function.
     */
    result add_successor(job_handle other);

    /**
     * \brief Determines if this job is pending execution.
     *
     * Jobs pending execution are ones that have been queued, but not yet picked up by a worker thread.
     * Jobs can also return to this state if they are requeued for any reason (such as no fibers being available).
     *
     * \return True if job is pending.
     */
    bool is_pending();

    /**
     * \brief Determines if this job is currently executing.
     *
     * A job is only in an executing state when it's fiber is running, waiting jobs are not
     * considered to be running.
     *
     * \return True if job is running.
     */
    bool is_running();

    /**
     * \brief Determines if this job has completed.
     *
     * \return True if job is completed.
     */
    bool is_complete();

    /**
     * \brief Determines if this job can be modified.
     *
     * Jobs are only mutable before they have been dispatched, or after they have completed. At no other
     * point should they be modified.
     *
     * \return True if job is mutable.
     */
    bool is_mutable();

    /**
     * \brief Determines if this handle points to a valid job.
     *
     * \return True if handle is valid.
     */
    bool is_valid();

    /**
     * \brief Waits for this job to complete.
     *
     * If called from a job this is non-blocking, and will queue the job
     * for execution after the given job completes. If called
     * from any other place, it will block.
     *
     * \param in_timeout If provided, this function will wait a maximum of this time. If
     *                   the function returns due to a timeout the result provided will be
     *                   result::timeout.
     *
     * \return Value indicating the success of this function.
     */
    result wait(timeout in_timeout = timeout::infinite);

    /**
     * \brief Dispatches this job, causing it to be queued for execution. 
     *
     * If dispatching a large number of jobs in one go, consider using the
     * more performant @jobs::scheduler::dispatch_batch.
     *
     * \return Value indicating the success of this function.
     */
    result dispatch();

    /**
     * \brief Assignment operator
     *
     * \param other Object to assign.
     *
     * \return Reference to this object.
     */
    job_handle& operator=(const job_handle& other);

    /**
     * \brief Equality operator
     *
     * \param rhs Object to compare against.
     *
     * \return True if objects are equal.
     */
    bool operator==(const job_handle& rhs) const;

    /**
     * \brief Inequality operator
     *
     * \param rhs Object to compare against.
     *
     * \return True if objects are inequal.
     */
    bool operator!=(const job_handle& rhs) const;

private:

    /** Pointer to the owning scheduler of this handle. */
    scheduler* m_scheduler = nullptr;

    /** Index into the scheduler's job definition pool where this jobs data is held. */
    size_t m_index = 0;

};

namespace internal {

/**
 * Encapsulates all the settings required to dispatch and run an instance of a job. This
 * is used for internal storage, and shouldn't ever need to be touched by outside code.
 */
class job_definition
{
public:
    scheduler;

    /**
     * \brief Constructor
     *
     * \param index Index into the scheduler's pool where this jobs data is held.
     */
    job_definition(size_t index);

    /** Resets data so this definition can be recycled. */
    void reset();

public:    

    /** Index into the scheduler's  pool where this jobs data is held. */
    size_t index;

    /** Number of handles that reference this job. Used to track and recycle jobs when no longer used. */
    std::atomic<size_t> ref_count;

    /** Function executed to perform jobs workload. */
    job_entry_point work;

    /** Minimum stack-size fiber must have to execute job. */
    size_t stack_size;

    /** Bitmask of all priorities assigned to job. This determines the work queues it gets placed in. */
    priority job_priority;

    /** Handle to counter which will be incremented on completino. */
    counter_handle completion_counter;

    /** Current execution status of the job. */
    std::atomic<job_status> status;

    /** Handle to event we are currently waiting for. */
    event_handle wait_event;

    /** Handle to counter we are currently waiting for. */
    counter_handle wait_counter;

    /** Value we are waiting for @wait_counter to reach to ready.  */
    size_t wait_counter_value;

    /** If true, the value we are waiting for, will be removed from the counter once its reached. */
    bool wait_counter_remove_value;

    /** 
     * If true the job is not automatically requeued when the counter reaches the wanted value. This is used primarily
     * for fake-jobs which are created purely to wait on, but don't actually need to execute any code. 
     */
    bool wait_counter_do_not_requeue;

    /** Linked list link for this job within the wait-list in the counter we are waiting on. */
    multiple_writer_single_reader_list<internal::job_definition*>::link wait_counter_list_link;

    /** Handle of job we are current waiting for. */
    job_handle wait_job;

    /** Linked list link for this job within the wait-list in the job we are waiting on. */
    multiple_writer_single_reader_list<internal::job_definition*>::link wait_list_link;

    /** Linked list holding all jobs which are currently waiting for us to complete */
    multiple_writer_single_reader_list<internal::job_definition*> wait_list;

    // Note: dependencies are only safe to modify in two situations:
    //            - when job is not running and is mutable
    //            - when job is running and is being modified by the fiber executing it (when not queued).

    /** Head of single linked list holding all predecessor job dependencies. */
    job_dependency* first_predecessor = nullptr;

    /** Head of single linked list holding all successor job dependencies. */
    job_dependency* first_successor = nullptr;

    /** Atomic counter counting down how many pending predecessors need to finish executing before we can run. */
    std::atomic<size_t> pending_predecessors;

    /** Execution context for this job. */
    job_context context;

    /** Maximum size of a descriptive tag that can be assigned to a job. */
    static const size_t max_tag_length = 64;

    /** Descriptive tag that can be assigned to this job. */
    char tag[max_tag_length] = 0;

};

/**
 * Holds an individual dependency of a job, allocated from a pool by the scheduler
 * and joined together as a linked list.
 */
class job_dependency
{
public:

    /**
     * \brief Constructor
     *
     * \param index Index into the scheduler's pool where this dependencies data is held.
     */
    job_dependency(size_t in_pool_index)
        : pool_index(in_pool_index)
    {
    }

    /** Resets data so this can be recycled. */
    void reset()
    {
        // pool_index should not be reset, it should be persistent.
        job = job_handle();
        next = nullptr;
    }

    /** Index into the scheduler's pool where this dependencies data is held. */
    size_t pool_index;

    /** Handle of job this dependency is about. */
    job_handle job;

    /** Next dependency in a job's linked list. */
    job_dependency* next = nullptr;

};

/**
 * Represents an individual scope in a fibers profiling hierarchy.
 * This is stored together as a single linked list.
 */
class profile_scope_definition
{
public:

    /** Semantic type of code this profile marker is enclosing. */
    profile_scope_type type;

    /** Maximum length of a descriptive tag that can be assigned to this marker. */
    static const size_t max_tag_length = 64;

    /** Descriptive tag */
    char tag[max_tag_length];

    /** Next scope in job's stack. */
    profile_scope_definition* next;

    /** Previous scope in job's stack. */
    profile_scope_definition* prev;

};

/**
 * Simple RAII type that enters a profile scope on construction and exits it
 * on destruction.
 */
class profile_scope_internal
{
public:

    /**
     * \brief Constructor
     *
     * \param type Semantic type of code this profile marker is enclosing.
     * \param tag Descriptive tag.
     * \param scheduler Scheduler that owns this profile scope. If nullptr, static value will be used.
     */
    profile_scope_internal(jobs::profile_scope_type type, const char* tag, jobs::scheduler* scheduler = nullptr);

    /** Destructor */
    ~profile_scope_internal();

private:

    /** Job context this scope is inside of. */
    jobs::internal::job_context* m_context = nullptr;

    /** Scheduler that owns this profile scope. */
    jobs::scheduler* m_scheduler = nullptr;

};

}; /* namespace internal */

}; /* namespace jobs */

#endif /* __JOBS_JOB_H__ */