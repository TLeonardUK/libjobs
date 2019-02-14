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
 *  \file jobs_counter.h
 *
 *  Include header for counter based synchronization functionality.
 */

#ifndef __JOBS_COUNTER_H__
#define __JOBS_COUNTER_H__

#include "jobs_defines.h"
#include "jobs_enums.h"
#include "jobs_utils.h"

#include <atomic>
#include <condition_variable>
#include <mutex>

namespace jobs {
    
class scheduler;

namespace internal {

class job_definition;
    
/**
 * Encapsulates all the settings required to manage a counter. This is used 
 * for internal storage, and shouldn't ever need to be touched by outside code.
 */
class counter_definition
{
public:

    /** Constructor */
    counter_definition();

    /** Resets data so definition can be recycled. */
    void reset();

public:	

    /** Number of handles that reference this counter. Used to track and recycle counters when no longer used. */
    std::atomic<size_t> ref_count;

    /** Value the counter currently holds. */
    std::atomic<size_t> value;

    /** Condition variable that is notified when a value is changed while counter is being waited on.  */
    std::condition_variable_any value_cvar;

    /** Mutex to use in tandem with value_cvar to wait on. */
    std::mutex value_cvar_mutex;

    /** List of all jobs that are waiting on this counter to get to equal a value. */
    multiple_writer_single_reader_list<internal::job_definition*> wait_list;

};

}; /* namespace internal */

/**
 * \brief Represents an instance of an counter that has been created by the scheduler.
 *
 * Events data is owned by the scheduler, be careful accessing handles if 
 * the scheduler has been destroyed.
 *
 * Counters represent an atomic integer value that can be never be negative. Counters can be added
 * to, and removed from. If a removal would result in a negative value, the counter waits until
 * enough has been added that the value can be removed. Counters also support the ability to get/set 
 * values directly as well as wait for specific values to be set.
 *
 * The functionality of counters is straight-forward but incredibly flexible, and can be used in 
 * a multitude of ways to syncronize jobs.
 *
 * Implementation wise this is fairly similar to a semaphore, just with more control over the
 * internal signal count.
 */
class counter_handle
{
protected:

    friend class scheduler;

    /**
     * \brief Constructor
     *
     * \param scheduler Scheduler that owns this counter.
     * \param index Index into the scheduler's counter pool where this counters data is held.
     */
    counter_handle(scheduler* scheduler, size_t index);

    /** Increases the reference count of this counter. */
    void increase_ref();

    /** Decreases the reference count of this counter. When it reaches zero, it will be disposed of. */
    void decrease_ref();

public:

    /** Constructor */
    counter_handle();

    /**
     * \brief Copy constructor
     *
     * \param other Object to copy.
     */
    counter_handle(const counter_handle& other);

    /** Destructor */
    ~counter_handle();

    /**
     * \brief Waits for this counter to reach a specific value. 
     *        
     * If called from a job this is non-blocking, and will queue the job
     * for execution after the counter reaches the given value. If called
     * from any other place, it will block.
     *
     * \param value Value counter needs to reach to continue.
     * \param in_timeout If provided, this function will wait a maximum of this time. If 
     *                   the function returns due to a timeout the result provided will be
     *                   result::timeout.
     *
     * \return Value indicating the success of this function.
     */
    result wait_for(size_t value, timeout in_timeout = timeout::infinite);

    /**
     * \brief Adds a given value to this counter.
     *
     * Causes any waiting jobs to be readied if the resulting value is equal to
     * the value they are waiting for.
     *
     * \param value Value to add to the counter.
     *
     * \return Value indicating the success of this function.
     */
    result add(size_t value);

    /**
     * \brief Removes the given value from this counter.
     *
     * Values of counters can never be negative. If this operation would make
     * the value go negative, the job will be queued for execution (or the thread blocked if called from a non-job)
     * until the counter has a value high enough that this operation would not cause a negative.
     *
     * Causes any waiting jobs to be readied if the resulting value is equal to
     * the value they are waiting for.
     *
     * \param value Value to remove from counter.
     * \param in_timeout If provided, this function will wait a maximum of this time. If
     *                   the function returns due to a timeout the result provided will be
     *                   result::timeout.
     *
     * \return Value indicating the success of this function.
     */
    result remove(size_t value, timeout in_timeout = timeout::infinite);

    /**
     * \brief Gets the current value of this counter.
     *
     * \param output Reference to location to store value in.
     *
     * \return Value indicating the success of this function.
     */
    result get(size_t& output);

    /**
     * \brief Sets the current value of this counter.
     *
     * Causes any waiting jobs to be readied if the resulting value is equal to
     * the value they are waiting for.
     *
     * \param value Value to set counter to.
     *
     * \return Value indicating the success of this function.
     */
    result set(size_t value);

    /**
     * \brief Assignment operator
     *
     * \param other Object to assign.
     *
     * \return Reference to this object.
     */
    counter_handle& operator=(const counter_handle& other);

    /**
     * \brief Equality operator
     *
     * \param rhs Object to compare against.
     *
     * \return True if objects are equal.
     */
    bool operator==(const counter_handle& rhs) const;

    /**
     * \brief Inequality operator
     *
     * \param rhs Object to compare against.
     *
     * \return True if objects are inequal.
     */
    bool operator!=(const counter_handle& rhs) const;

    /**
     * \brief Returns true if this handle points to a valid counter instance.
     *
     * \return True if handle is valid.
     */
    bool is_valid() const;

private:

    /**
     * \brief Attempts to modify the value held by the counter.
     *
     * \param new_value Value that counter should be modified to.
     * \param absolute If true the value should be modified absolutely, otherwise its relative to its current value.
     * \param subtract If true the value should be subtracted not added. 
     * \param lock_required If true a mutex lock needs to be acquired for our wait list. If 
     *                      false its assumed that the lock is held further up the callstack.
     *
     * \return True if value was modified, false if unable to because it would result in a negative number.
     */
    bool modify_value(size_t new_value, bool absolute, bool subtract = false, bool lock_required = true);

    /**
     * \brief Wakes up any jobs that are waiting for the given value.
     *
     * \param new_value Value that jobs should be waiting for to be woken up.
     * \param lock_required If true a mutex lock needs to be acquired for our wait list. If 
     *                      false its assumed that the lock is held further up the callstack.
     *
     * \return True if added to wait list, false if wait criteria was met while
     *         attempting operation and did not need to be added to list.
     */
    void notify_value_changed(size_t new_value, bool lock_required = true);

    /**
     * \brief Adds the given job to our wait list.
     *
     * \param job_def Definition of job to add to our wait list.
     *
     * \return True if added to wait list, false if wait criteria was met while 
     *         attempting operation and did not need to be added to list.
     */
    bool add_to_wait_list(internal::job_definition* job_def);

    /**
     * \brief Removes the given job from our wait list.
     *
     * \param job_def Definition of job to remove from our wait list.
     */
    void remove_from_wait_list(internal::job_definition* job_def);

    /** Pointer to the owning scheduler of this handle. */
    scheduler* m_scheduler = nullptr;

    /** Index into the scheduler's counter pool where this counters data is held. */
    size_t m_index = 0;

};

}; /* namespace jobs */

#endif /* __JOBS_COUNTER_H__ */