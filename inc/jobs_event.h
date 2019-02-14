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
 *  \file jobs_event.h
 *
 *  Include header for event based synchronization functionality.
 */

#ifndef __JOBS_EVENT_H__
#define __JOBS_EVENT_H__

#include "jobs_defines.h"
#include "jobs_enums.h"
#include "jobs_utils.h"
#include "jobs_counter.h"

#include <atomic>

namespace jobs {

/**
 * \brief Represents an instance of an event that has been created by the scheduler.
 *
 * Events data is owned by the scheduler, be careful accessing handles if 
 * the scheduler has been destroyed.
 *
 * Events in this library work in two ways. Manual-reset and auto-reset. 
 *
 * For manual-reset events, all jobs that wait on the event will be allowed to continue 
 * once the event is signaled, and will continue being allowed until the event is manually reset
 * from elsewhere.
 *
 * Auto-reset events work similarly to manual-reset, except after the first jobs is woken up 
 * on signal, the event is automatically (and atomically) reset.
 */
class event_handle
{
protected:

    friend class scheduler;

    /**
     * \brief Constructor
     *
     * \param scheduler Scheduler that owns this counter.
     * \param counter Handle to the counter used internally to implement event functionality.
     * \param auto_reset If event should reset its signaled state after waking up a waiting job.
     */
    event_handle(scheduler* scheduler, const counter_handle& counter, bool auto_reset);

public:

    /** Constructor */
    event_handle();

    /**
     * \brief Copy constructor
     *
     * \param other Object to copy.
     */
    event_handle(const event_handle& other);

    /** Destructor */
    ~event_handle();

    /**
     * \brief Waits for this event to be signaled.
     *
     * If called from a job this is non-blocking, and will queue the job
     * for execution after the event is signaled. If called
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
     * \brief Signals this event.
     *
     * If this event is manual-reset, all waiting jobs causes will be readied, and
     * all future jobs waits will be allowed to continue until the event is reset.
     *
     * If this event is auto-reset, one job will be readied and the signal state will
     * be automatically reset.
     *
     * \return Value indicating the success of this function.
     */
    result signal();

    /**
     * \brief Resets the signal state of this event.
     *
     * This function should only be called for manual-reset events, calling it on
     * an auto-reset event performs no operation.
     *
     * \return Value indicating the success of this function.
     */
    result reset();

    /**
     * \brief Assignment operator
     *
     * \param other Object to assign.
     *
     * \return Reference to this object.
     */
    event_handle& operator=(const event_handle& other);

    /**
     * \brief Equality operator
     *
     * \param rhs Object to compare against.
     *
     * \return True if objects are equal.
     */
    bool operator==(const event_handle& rhs) const;

    /**
     * \brief Inequality operator
     *
     * \param rhs Object to compare against.
     *
     * \return True if objects are inequal.
     */
    bool operator!=(const event_handle& rhs) const;

private:

    /** Pointer to the owning scheduler of this handle. */
    scheduler* m_scheduler = nullptr;

    /** Handle of the counter used internally to implement event functionality. */
    counter_handle m_counter;

    /** If this event should reset its signaled state after waking up a waiting job.. */
    bool m_auto_reset = false;

};

}; /* namespace jobs */

#endif /* __JOBS_EVENT_H__ */