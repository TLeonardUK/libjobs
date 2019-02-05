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

namespace jobs {
namespace internal {

counter_definition::counter_definition()
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
    profile_scope scope(profile_scope_type::fiber, "counter::wait_for");

    internal::counter_definition& def = m_scheduler->get_counter_definition(m_index);

    // Grab the current job context.
    internal::job_context* context = m_scheduler->get_active_job_context();
    internal::job_context* worker_context = m_scheduler->get_worker_job_context();

    // If we have a job context, go to sleep waiting on the value.
    if (context != nullptr)
    {
        while (true)
        {
            assert(worker_context != nullptr);

            volatile bool timeout_called = false;

            // Put job to sleep.
            {
                profile_scope scope(profile_scope_type::fiber, "put to sleep");

                std::shared_lock<std::shared_mutex> lock(def.wait_list.get_mutex());

                // Try and consume value.
                if (def.value == value)
                {
                    break;
                }

                context->job_def->status = internal::job_status::waiting_on_counter;
                context->job_def->wait_counter = *this;
                context->job_def->wait_counter_value = value;
                context->job_def->wait_counter_at_least_value = false;
                add_to_wait_list(context->job_def);
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
                        m_scheduler->notify_job_available();
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

            // Supress requeueing the job, we will do this when the callback returns.
            m_scheduler->m_worker_job_supress_requeue = true;

            // Switch back to worker which will requeue fiber for execution later.			
            m_scheduler->switch_context(*worker_context);

            // Cleanup
            remove_from_wait_list(context->job_def);
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
        }

        return result::success;
    }

    // If no job-context we have to do a blocking wait.
    else
    {
        internal::stopwatch timer;
        timer.start();

        std::unique_lock<std::shared_mutex> lock(def.wait_list.get_mutex());

        printf("Waiting on value %zi\n", value);

        // Keep looping until we manage to decrease the value.
        while (true)
        {
            if (def.value == value)
            {
                break;
            }
            else
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

result counter_handle::add(size_t value)
{
    profile_scope scope(profile_scope_type::fiber, "counter::add");

    internal::counter_definition& def = m_scheduler->get_counter_definition(m_index);

    size_t changed_value = 0;

    {
        profile_scope scope(profile_scope_type::fiber, "lock & increment");

        std::unique_lock<std::shared_mutex> lock(def.wait_list.get_mutex());
        def.value += value;
        changed_value = def.value;

        def.value_cvar.notify_all();
    }

    notify_value_changed(changed_value);

    return result::success;
}

result counter_handle::remove(size_t value, timeout in_timeout)
{
    profile_scope scope(profile_scope_type::fiber, "counter::remove");

    internal::counter_definition& def = m_scheduler->get_counter_definition(m_index);

    // Grab the current job context.
    internal::job_context* context = m_scheduler->get_active_job_context();
    internal::job_context* worker_context = m_scheduler->get_worker_job_context();

    // If we have a job context, go to sleep waiting on the value.
    if (context != nullptr)
    {
        while (true)
        {
            assert(worker_context != nullptr);

            volatile bool timeout_called = false;

            // Put job to sleep.
            {
                profile_scope scope(profile_scope_type::fiber, "put to sleep");

                std::unique_lock<std::shared_mutex> lock(def.wait_list.get_mutex());

                // Try and consume value.
                // this is problematic since we already have a lock above.
                if (try_remove_value(value))
                {
                    break;
                }

                context->job_def->status = internal::job_status::waiting_on_counter;
                context->job_def->wait_counter = *this;
                context->job_def->wait_counter_value = value;
                context->job_def->wait_counter_at_least_value = true;
                add_to_wait_list(context->job_def);
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
                        m_scheduler->notify_job_available();
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

            // Supress requeueing the job, we will do this when the callback returns.
            m_scheduler->m_worker_job_supress_requeue = true;

            // Switch back to worker which will requeue fiber for execution later.			
            m_scheduler->switch_context(*worker_context);

            // Cleanup
            remove_from_wait_list(context->job_def);
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

            // Try and consume value.
            {
                std::unique_lock<std::shared_mutex> lock(def.wait_list.get_mutex());

                if (!try_remove_value(value))
                {
                    continue;
                }
            }

            return result::success;
        }
    }

    // If no job-context we have to do a blocking wait.
    else
    {
        internal::stopwatch timer;
        timer.start();

        size_t changed_value = 0;

       // printf("Waiting on remove %zi\n", value);

        {
            std::unique_lock<std::shared_mutex> lock(def.wait_list.get_mutex());

            // Keep looping until we manage to decrease the value.
            while (true)
            {
                if (def.value >= value)
                {
                   // printf("Got value %zi\n", value);
                    def.value -= value;
                    changed_value = def.value;

                    def.value_cvar.notify_all();

                    break;
                }
                else
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

        notify_value_changed(changed_value);
    }

    return result::success;
}

bool counter_handle::try_remove_value(size_t value)
{
    profile_scope scope(profile_scope_type::fiber, "counter::try_remove_value");

    internal::counter_definition& def = m_scheduler->get_counter_definition(m_index);

    size_t changed_value = 0;

    if (def.value >= value)
    {
        def.value -= value;
        changed_value = def.value;

        def.value_cvar.notify_all();
    }
    else
    {
        return false;
    }

    notify_value_changed(changed_value, false);

    return true;
}

result counter_handle::get(size_t& output)
{
    profile_scope scope(profile_scope_type::fiber, "counter::get");

    internal::counter_definition& def = m_scheduler->get_counter_definition(m_index);

    output = def.value;

    return result::success;
}

result counter_handle::set(size_t value)
{
    profile_scope scope(profile_scope_type::fiber, "counter::set");

    internal::counter_definition& def = m_scheduler->get_counter_definition(m_index);

    {
        std::unique_lock<std::shared_mutex> lock(def.wait_list.get_mutex());
        def.value = value;
        def.value_cvar.notify_all();
    }

    notify_value_changed(value);

    return result::success;
}

void counter_handle::add_to_wait_list(internal::job_definition* job_def)
{
    profile_scope scope(profile_scope_type::fiber, "counter::add_to_wait_list");

    internal::counter_definition& def = m_scheduler->get_counter_definition(m_index);
    
    // Add to list.    
    //printf("added: %p\n", job_def);
    job_def->wait_counter_list_link.value = job_def;
    def.wait_list.add(&job_def->wait_counter_list_link, false);
}

void counter_handle::remove_from_wait_list(internal::job_definition* job_def)
{
    profile_scope scope(profile_scope_type::fiber, "counter::remove_from_wait_list");

    internal::counter_definition& def = m_scheduler->get_counter_definition(m_index);

    //printf("removed: %p\n", job_def);

    def.wait_list.remove(&job_def->wait_counter_list_link);
}

void counter_handle::notify_value_changed(size_t new_value, bool lock_required)
{
    profile_scope scope(profile_scope_type::fiber, "counter::notify_value_changed");

    //printf("notify_value_changed: %zi\n", new_value);

    internal::counter_definition& def = m_scheduler->get_counter_definition(m_index);

    // Go through wait list and mark all jobs as ready to resume if their criteria is met.
    size_t signalled_job_count = 0;

    {
        internal::multiple_writer_single_reader_list<internal::job_definition*>::iterator iter;
        for (def.wait_list.iterate(iter, lock_required); iter; iter++)
        {
            bool signal = false;

            internal::job_definition* job_def = iter.value();

            if (job_def->wait_counter_at_least_value)
            {
                signal = (new_value >= job_def->wait_counter_value);
            }
            else
            {
                signal = (new_value == job_def->wait_counter_value);
            }

            if (signal)
            {
                //printf("woke up: %p\n", job_def);

                internal::job_status expected = internal::job_status::waiting_on_counter;
                if (job_def->status.compare_exchange_strong(expected, internal::job_status::pending))
                {
                    m_scheduler->requeue_job(job_def->index);
                    signalled_job_count++;
                }
            }
        }
    }

    if (signalled_job_count > 0)
    {
        //printf("Signalled %i jobs due to change to %zi.\n", signalled_job_count, new_value);

        // Wake up worker threads.
        m_scheduler->notify_job_available();
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