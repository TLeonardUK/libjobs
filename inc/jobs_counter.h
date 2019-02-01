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

#include "jobs_enums.h"
#include "jobs_utils.h"

#include <atomic>
#include <condition_variable>
#include <mutex>

namespace jobs {
    
namespace internal {

class job_definition;
    
/**
 * Encapsulates all the settings required to manage a counter. This is used 
 * for internal storage, and shouldn't ever need to be touched by outside code.
 */
class counter_definition
{
public:

    /** @todo */
    counter_definition();

    /** @todo */
    void reset();

public:	

    /** @todo */
    std::atomic<size_t> ref_count;

    /** @todo */
    std::atomic<size_t> value;

    /** @todo */
    std::condition_variable value_cvar;

    /** @todo */
    std::mutex value_mutex;

    /** @todo */
    internal::job_definition* wait_list_head;

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

    /** @todo */
    counter_handle(scheduler* scheduler, size_t index);

    /** @todo */
    void increase_ref();

    /** @todo */
    void decrease_ref();

public:

    /** @todo */
    counter_handle();

    /** @todo */
    counter_handle(const counter_handle& other);

    /** @todo */
    ~counter_handle();

    /** @todo */
    counter_handle& operator=(const counter_handle& other);

    /** @todo */
    result wait_for(size_t value, timeout in_timeout = timeout::infinite);

    /** @todo */
    result add(size_t value);

    /** @todo */
    result remove(size_t value, timeout in_timeout = timeout::infinite);

    /** @todo */
    result get(size_t& output);

    /** @todo */
    result set(size_t value);

    /** @todo */
    bool operator==(const counter_handle& rhs) const;

    /** @todo */
    bool operator!=(const counter_handle& rhs) const;

private:

    /** @todo */
    void notify_value_changed(size_t new_value, bool lock_required = true);

    /** @todo */
    void add_to_wait_list(internal::job_definition* job_def);

    /** @todo */
    void remove_from_wait_list(internal::job_definition* job_def);

    /** @todo */
    bool try_remove_value(size_t value, bool lock_required = true);

    /** Pointer to the owning scheduler of this handle. */
    scheduler* m_scheduler = nullptr;

    /** @todo */
    size_t m_index = 0;

};

}; /* namespace jobs */

#endif /* __JOBS_COUNTER_H__ */