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
 *  \file jobs_callback_scheduler.h
 *
 *  Include header for latent callback management functionality.
 */

#ifndef __JOBS_CALLBACK_SCHEDULER_H__
#define __JOBS_CALLBACK_SCHEDULER_H__

#include "jobs_enums.h"
#include "jobs_thread.h"
#include "jobs_utils.h"

#include <functional>

namespace jobs {

struct memory_functions;
class scheduler;
    
namespace internal {

/**
 *  \brief User-defined function called when the scheduled timeout elapses from a callback_scheduler::schedule call.
 */
typedef std::function<void()> callback_scheduler_function;

/**
 *  Encapsulates data required for a latent callback scheduled through a callback_scheduler.
 */
struct callback_definition
{
    /** @todo */
    bool active = false;

    /** @todo */
    size_t generation = 0;

    /** @todo */
    stopwatch stopwatch;

    /** @todo */
    timeout duration;

    /** @todo */
    callback_scheduler_function callback = nullptr;
};

/**
 *  Responsible for enqueueing callbacks which will be run after a given timeout.
 */
class callback_scheduler
{
public:

    /** Constructor. */
    callback_scheduler();

    /** Destructor. */
    ~callback_scheduler();

    /**
     * \brief Initializes this scheduler.
     *
     * \param scheduler scheduler that owners this callback scheduler.
     * \param max_callbacks maximum number of callbacks that can queued concurrently.
     * \param memory_functions User defined functions for allocating and freeing memory.
     *
     * \return Value indicating the success of this function.
     */
    result init(jobs::scheduler* scheduler, size_t max_callbacks, const jobs::memory_functions& memory_functions);

    /**
     * \brief Schedules a new callback after the given timeout.
     *
     * \param duration how long before callback is invoked.
     * \param handle handle of scheduled callback, can be used to cancel it later.
     * \param callback callback that is run after timeout.
     *
     * \return Value indicating the success of this function.
     */
    result schedule(timeout duration, size_t& handle, const callback_scheduler_function& callback);

    /**
     * \brief Cancels a previously scheduled callback.
     *
     * \param handle handle of callback to cancel.
     *
     * \return Value indicating the success of this function.
     */
    result cancel(size_t handle);

private:

    /** @todo */
    void run_callbacks();

    /** @todo */
    uint64_t get_ms_till_next_callback();

private:

    /** Memory allocation functions provided by the scheduler. */
    jobs::memory_functions m_memory_functions;

    /** Schedule updated mutex */
    std::mutex m_schedule_updated_mutex;

    /** Schedule updated condition variable */
    std::condition_variable m_schedule_updated_cvar;

    /** Thread used for running callbacks. */
    thread* m_callback_thread = nullptr;

    /** Number of callbacks pending. */
    fixed_pool<callback_definition> m_callback_pool;

    /** Shutdown flag. */
    bool m_shutting_down = false;

    /** Owner of this class */
    jobs::scheduler* m_scheduler = nullptr;

};

}; /* namespace internal */
}; /* namespace jobs */

#endif /* __JOBS_FIBER_H__ */