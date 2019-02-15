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

#include "jobs_counter.h"
#include "jobs_scheduler.h"
#include "jobs_utils.h"

#include <cassert>
#include <cstdio>

namespace jobs {
namespace internal {

counter_definition::counter_definition()
#if defined(JOBS_PLATFORM_PS4)
    : value_cvar(nullptr) // -_-. Why do sync-primitives's have to be named by default on ps4.
    , value_cvar_mutex(nullptr)
#endif
{
    reset();
}	

void counter_definition::reset()
{
    ref_count = 0;	
    value = 0;
}

}; /* namespace internal */

counter_handle::counter_handle(scheduler* scheduler, size_t index)
    : m_scheduler(scheduler)
    , m_index(index)
{
    increase_ref();
}

counter_handle::counter_handle()
    : m_scheduler(nullptr)
    , m_index(0)
{
}

counter_handle::counter_handle(const counter_handle& other)
{
    m_scheduler = other.m_scheduler;
    m_index = other.m_index;

    increase_ref();
}

counter_handle::~counter_handle()
{
    decrease_ref();
}

counter_handle& counter_handle::operator=(const counter_handle& other)
{
    m_scheduler = other.m_scheduler;
    m_index = other.m_index;

    increase_ref();

    return *this;
}

bool counter_handle::is_valid() const
{
    return (m_scheduler != nullptr);
}

void counter_handle::increase_ref()
{
    if (m_scheduler != nullptr)
    {
        m_scheduler->increase_counter_ref_count(m_index);
    }
}

void counter_handle::decrease_ref()
{
    if (m_scheduler != nullptr)
    {
        m_scheduler->decrease_counter_ref_count(m_index);
    }
}

result counter_handle::wait_for(size_t value, timeout in_timeout)
{
    jobs_profile_scope(profile_scope_type::fiber, "counter::wait_for", m_scheduler);

    internal::counter_definition& def = m_scheduler->get_counter_definition(m_index);

    // Grab the current job context.
    internal::job_context* context = m_scheduler->get_active_job_context();
    internal::job_context* worker_context = m_scheduler->get_worker_job_context();

    // If we have a job context, go to sleep waiting on the value.
    if (context != nullptr)
    {
        assert(worker_context != nullptr);

        volatile bool timeout_called = false;

        // Put job to sleep.
        {
            context->job_def->status = internal::job_status::waiting_on_counter;
            context->job_def->wait_counter = *this;
            context->job_def->wait_counter_value = value;
            context->job_def->wait_counter_remove_value = false;
            context->job_def->wait_counter_do_not_requeue = false;

            if (!add_to_wait_list(context->job_def))
            {
                return result::success;
            }
        }

        // Queue a wakeup.
        size_t schedule_handle;
        if (!in_timeout.is_infinite())
        {
            result res = m_scheduler->m_callback_scheduler.schedule(in_timeout, schedule_handle, [&]() {

                // Do this atomatically to make sure we don't set it to pending after the scheduler 
                // has already done that due to this event being signalled.
                internal::job_status expected = internal::job_status::waiting_on_counter;
                if (context->job_def->status.compare_exchange_strong(expected, internal::job_status::pending))
                {
                    timeout_called = true;
                    m_scheduler->requeue_job(context->job_def->index);
                }

            });

            // Failed to schedule a wakeup? Abort.
            if (res != result::success)
            {
                remove_from_wait_list(context->job_def);
                context->job_def->status = internal::job_status::pending;
                return res;
            }
        }

        // Supress requeueing the job, and return to worker, we will do this when the callback returns.
        m_scheduler->return_to_worker(*worker_context, true);

        // Cleanup
        context->job_def->wait_counter = counter_handle();

        // If we timed out, just escape here.
        if (timeout_called)
        {
            return result::timeout;
        }

        // Cancel callback and clear our job handle.
        if (!in_timeout.is_infinite())
        {
            m_scheduler->m_callback_scheduler.cancel(schedule_handle);
        }

        return result::success;
    }

    // If no job-context we have to do a blocking wait.
    else
    {
        internal::stopwatch timer;
        timer.start();

        {
            std::unique_lock<std::mutex> lock(def.value_cvar_mutex);

            // Allocate a fake job to be waited on.
            internal::job_definition fake_job(UINT32_MAX);
            fake_job.status = internal::job_status::waiting_on_counter;
            fake_job.wait_counter = *this;
            fake_job.wait_counter_value = value;
            fake_job.wait_counter_remove_value = false;
            fake_job.wait_counter_do_not_requeue = true;

            if (!add_to_wait_list(&fake_job))
            {
                return result::success;
            }

            // Keep looping until we manage to decrease the value.
            while (fake_job.status == internal::job_status::waiting_on_counter)
            {
                if (in_timeout.is_infinite())
                {
                    def.value_cvar.wait(lock);
                }
                else
                {
                    size_t ms_remaining = in_timeout.duration - timer.get_elapsed_ms();
                    if (ms_remaining > 0)
                    {
                        def.value_cvar.wait_for(lock, std::chrono::milliseconds(ms_remaining));
                    }
                    else
                    {
                        return result::timeout;
                    }
                }
            }
        }
    }

    return result::success;
}

result counter_handle::remove(size_t value, timeout in_timeout)
{
    jobs_profile_scope(profile_scope_type::fiber, "counter::remove", m_scheduler);

    internal::counter_definition& def = m_scheduler->get_counter_definition(m_index);

    // Grab the current job context.
    internal::job_context* context = m_scheduler->get_active_job_context();
    internal::job_context* worker_context = m_scheduler->get_worker_job_context();

    // If we have a job context, go to sleep waiting on the value.
    if (context != nullptr)
    {
        assert(worker_context != nullptr);

        volatile bool timeout_called = false;

        // Put job to sleep.
        {
            context->job_def->status = internal::job_status::waiting_on_counter;
            context->job_def->wait_counter = *this;
            context->job_def->wait_counter_value = value;
            context->job_def->wait_counter_remove_value = true;
            context->job_def->wait_counter_do_not_requeue = false;

            if (!add_to_wait_list(context->job_def))
            {
                return result::success;
            }
        }

        // Queue a wakeup.
        size_t schedule_handle;
        if (!in_timeout.is_infinite())
        {
            result res = m_scheduler->m_callback_scheduler.schedule(in_timeout, schedule_handle, [&]() {

                // Do this atomatically to make sure we don't set it to pending after the scheduler 
                // has already done that due to this event being signalled.
                internal::job_status expected = internal::job_status::waiting_on_counter;
                if (context->job_def->status.compare_exchange_strong(expected, internal::job_status::pending))
                {
                    timeout_called = true;
                    m_scheduler->requeue_job(context->job_def->index);
                }

            });

            // Failed to schedule a wakeup? Abort.
            if (res != result::success)
            {
                remove_from_wait_list(context->job_def);
                context->job_def->status = internal::job_status::pending;
                return res;
            }
        }

        // Supress requeueing the job, and return to worker, we will do this when the callback returns.
        m_scheduler->return_to_worker(*worker_context, true);

        // Cleanup
        context->job_def->wait_counter = counter_handle();

        // If we timed out, just escape here.
        if (timeout_called)
        {
            return result::timeout;
        }

        // Cancel callback and clear our job handle.
        if (!in_timeout.is_infinite())
        {
            m_scheduler->m_callback_scheduler.cancel(schedule_handle);
        }

        return result::success;
    }

    // If no job-context we have to do a blocking wait.
    else
    {
        internal::stopwatch timer;
        timer.start();

        size_t changed_value = 0;

        {
            std::unique_lock<std::mutex> lock(def.value_cvar_mutex);

            // Allocate a fake job to be waited on.
            internal::job_definition fake_job(UINT32_MAX);
            fake_job.status = internal::job_status::waiting_on_counter;
            fake_job.wait_counter = *this;
            fake_job.wait_counter_value = value;
            fake_job.wait_counter_remove_value = true;
            fake_job.wait_counter_do_not_requeue = true;

            if (!add_to_wait_list(&fake_job))
            {
                return result::success;
            }

            // Keep looping until we manage to decrease the value.
            while (fake_job.status == internal::job_status::waiting_on_counter)
            {
                if (in_timeout.is_infinite())
                {
                    def.value_cvar.wait(lock);
                }
                else
                {
                    size_t ms_remaining = in_timeout.duration - timer.get_elapsed_ms();
                    if (ms_remaining > 0)
                    {
                        def.value_cvar.wait_for(lock, std::chrono::milliseconds(ms_remaining));
                    }
                    else
                    {
                        return result::timeout;
                    }
                }
            }
        }
    }

    return result::success;
}

result counter_handle::get(size_t& output)
{
    jobs_profile_scope(profile_scope_type::fiber, "counter::get", m_scheduler);

    internal::counter_definition& def = m_scheduler->get_counter_definition(m_index);

    output = def.value;

    return result::success;
}

result counter_handle::add(size_t value)
{
    modify_value(value, false);
    return result::success;
}

result counter_handle::set(size_t value)
{
    modify_value(value, true);
    return result::success;
}

bool counter_handle::modify_value(size_t new_value, bool absolute, bool subtract, bool lock_required)
{
    jobs_profile_scope(profile_scope_type::fiber, "counter::modify_value", m_scheduler);

    internal::counter_definition& def = m_scheduler->get_counter_definition(m_index);

    {
        internal::optional_shared_lock<internal::spinwait_mutex> lock(def.wait_list.get_mutex(), lock_required);
        
        size_t changed_value = 0;
        if (absolute)
        {
            changed_value = (def.value = new_value);
        }
        else
        {
            if (subtract)
            {
                if (def.value >= new_value)
                {
                    changed_value = (def.value -= new_value);
                }
                else
                {
                    return false;
                }
            }
            else
            {
                changed_value = (def.value += new_value);
            }
        }

        notify_value_changed(changed_value, false);
    }

    return true;
}

bool counter_handle::add_to_wait_list(internal::job_definition* job_def)
{
    jobs_profile_scope(profile_scope_type::fiber, "counter::add_to_wait_list", m_scheduler);

    internal::counter_definition& def = m_scheduler->get_counter_definition(m_index);
    
    if (job_def->wait_counter_remove_value)
    {
        internal::optional_lock<internal::spinwait_mutex> lock(def.wait_list.get_mutex());

        // Value high enough to remove? We don't need to wait. 
        if (modify_value(job_def->wait_counter_value, false, true, false))
        {
            job_def->status = internal::job_status::pending;
            job_def->wait_counter = counter_handle();

            return false;
        }

        job_def->wait_counter_list_link.value = job_def;
        def.wait_list.add(&job_def->wait_counter_list_link, false);
    }
    else
    {
        internal::optional_shared_lock<internal::spinwait_mutex> lock(def.wait_list.get_mutex());

        // Is value equal? We don't need to wait.
        if (def.value == job_def->wait_counter_value)
        {
            job_def->status = internal::job_status::pending;
            job_def->wait_counter = counter_handle();

            return false;
        }

        job_def->wait_counter_list_link.value = job_def;
        def.wait_list.add(&job_def->wait_counter_list_link, false);
    }

    return true;
}

void counter_handle::remove_from_wait_list(internal::job_definition* job_def)
{
    jobs_profile_scope(profile_scope_type::fiber, "counter::remove_from_wait_list", m_scheduler);

    internal::counter_definition& def = m_scheduler->get_counter_definition(m_index);

    def.wait_list.remove(&job_def->wait_counter_list_link);
}

void counter_handle::notify_value_changed(size_t new_value, bool lock_required)
{
    jobs_profile_scope(profile_scope_type::fiber, "counter::notify_value_changed", m_scheduler);

    internal::counter_definition& def = m_scheduler->get_counter_definition(m_index);

    // Go through wait list and mark all jobs as ready to resume if their criteria is met.
    size_t signalled_job_count = 0;
    size_t signalled_no_requeue_job_count = 0;

    {
        jobs_profile_scope(profile_scope_type::fiber, "wake up waiting", m_scheduler);

        internal::multiple_writer_single_reader_list<internal::job_definition*>::iterator iter;
        for (def.wait_list.iterate(iter, lock_required); iter; )
        {
            bool signal = false;

            internal::job_definition* job_def = iter.value();

            if (job_def->wait_counter_remove_value)
            {
                signal = modify_value(job_def->wait_counter_value, false, true, false);
            }
            else
            {
                signal = (new_value == job_def->wait_counter_value);
            }

            if (signal)
            {
                internal::job_status expected = internal::job_status::waiting_on_counter;
                if (job_def->status.compare_exchange_strong(expected, internal::job_status::pending))
                {
                    if (!job_def->wait_counter_do_not_requeue)
                    {
                        m_scheduler->requeue_job(job_def->index);
                        signalled_job_count++;
                    }
                    else
                    {
                        signalled_no_requeue_job_count++;
                    }

                    iter.remove();
                    continue;
                }
            }

            iter++;
        }
    }

    if (signalled_job_count > 0)
    {
        jobs_profile_scope(profile_scope_type::fiber, "signal worker threads", m_scheduler);
        m_scheduler->notify_job_available(signalled_job_count);
    }

    if (signalled_no_requeue_job_count > 0)
    {
        jobs_profile_scope(profile_scope_type::fiber, "signal waiting threads", m_scheduler);

        std::unique_lock<std::mutex> lock(def.value_cvar_mutex);
        def.value_cvar.notify_all();
    }
}

bool counter_handle::operator==(const counter_handle& rhs) const
{
    return (m_scheduler == rhs.m_scheduler && m_index == rhs.m_index);
}

bool counter_handle::operator!=(const counter_handle& rhs) const
{
    return !(*this == rhs);
}

}; /* namespace jobs */