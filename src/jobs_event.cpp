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

#include "jobs_event.h"
#include "jobs_scheduler.h"

#include <cassert>

namespace jobs {
namespace internal {

event_definition::event_definition()
{
	reset();
}	

void event_definition::reset()
{
	ref_count = 0;	
	auto_reset = false;
	signalled = false;
}

}; /* namespace internal */

event_handle::event_handle(scheduler* scheduler, size_t index)
	: m_scheduler(scheduler)
	, m_index(index)
{
	increase_ref();
}

event_handle::event_handle()
	: m_scheduler(nullptr)
	, m_index(0)
{
}

event_handle::event_handle(const event_handle& other)
{
	m_scheduler = other.m_scheduler;
	m_index = other.m_index;

	increase_ref();
}

event_handle::~event_handle()
{
	decrease_ref();
}

event_handle& event_handle::operator=(const event_handle& other)
{
	m_scheduler = other.m_scheduler;
	m_index = other.m_index;

	increase_ref();

	return *this;
}

void event_handle::increase_ref()
{
	if (m_scheduler != nullptr)
	{
		m_scheduler->increase_event_ref_count(m_index);
	}
}

void event_handle::decrease_ref()
{
	if (m_scheduler != nullptr)
	{
		m_scheduler->decrease_event_ref_count(m_index);
	}
}

result event_handle::wait(timeout in_timeout)
{
	// Already signalled, just return.
	internal::event_definition& def = m_scheduler->get_event_definition(m_index);
	if (consume_signal())
	{
		return result::success;
	}

	// @todo if we are already signaled and not auto-reset, just return.
	internal::job_context* context = m_scheduler->get_active_job_context();
	internal::job_context* worker_context = m_scheduler->get_worker_job_context();
	
	// If we have a job context, adds this event to its dependencies and put it to sleep.
	if (context != nullptr)
	{
		while (true)
		{
			assert(worker_context != nullptr);

			volatile bool timeout_called = false;

			// Put job to sleep.
			context->job_def->status = internal::job_status::waiting_on_event;
			context->job_def->wait_event = *this;

			// Queue a wakeup.
			size_t schedule_handle;
			result res = m_scheduler->m_callback_scheduler.schedule(in_timeout, schedule_handle, [&]() {

				// Do this atomatically to make sure we don't set it to pending after the scheduler 
				// has already done that due to this event being signalled.
				internal::job_status expected = internal::job_status::waiting_on_event;
				if (context->job_def->status.compare_exchange_strong(expected, internal::job_status::pending))
				{
					timeout_called = true;
					m_scheduler->notify_job_available();
				}

			});

			// Failed to schedule a wakeup? Abort.
			if (res != result::success)
			{
				context->job_def->status = internal::job_status::pending;
				return res;
			}

			// Switch back to worker which will requeue fiber for execution later.			
			m_scheduler->switch_context(*worker_context);

			// Cleanup
			context->job_def->wait_event = event_handle();

			// If we timed out, just escape here.
			if (timeout_called)
			{
				return result::timeout;
			}

			// Cancel callback and clear our job handle.
			m_scheduler->m_callback_scheduler.cancel(schedule_handle);

			//  Not signalled? 
			if (!consume_signal())
			{
				continue;
			}

			return result::success;
		}
	}
	// If we have no job context, we just have to do a blocking wait.
	else
	{
		internal::stopwatch timer;
		timer.start();

		// Just continue waiting until we can consume a signal. We wait
		// on the task-available cvar, as this gets fired on event signal.
		//
		// @todo Might want to change this to an event-specific cvar as 
		// it will currently result in a lot of pointless wakeups.
		while (true)
		{
			std::unique_lock<std::mutex> lock(m_scheduler->m_task_available_mutex);

			if (consume_signal())
			{
				break;
			}

			if (in_timeout.is_infinite())
			{
				m_scheduler->m_task_available_cvar.wait(lock);
			}
			else
			{
				size_t ms_remaining = in_timeout.duration - timer.get_elapsed_ms();
				if (ms_remaining > 0)
				{
					m_scheduler->m_task_available_cvar.wait_for(lock, std::chrono::milliseconds(ms_remaining));
				}
				else
				{
					return result::timeout;
				}
			}
		}
	}

	return result::success;
}

result event_handle::signal()
{
	// Already signalled, just return.
	internal::event_definition& def = m_scheduler->get_event_definition(m_index);
	if (def.signalled)
	{
		return result::success;
	}

	// Mark as signalled.
	def.signalled = true;

	// Wake up worker threads so they can run any jobs waiting on this event handle.
	m_scheduler->notify_job_available();

	return result::success;
}

result event_handle::reset()
{
	// Already reset, just return.
	internal::event_definition& def = m_scheduler->get_event_definition(m_index);
	if (!def.signalled)
	{
		return result::success;
	}

	// Mark as signalled.
	def.signalled = false;

	return result::success;
}

bool event_handle::consume_signal()
{
	internal::event_definition& def = m_scheduler->get_event_definition(m_index);

	if (def.auto_reset)
	{
		bool expected = true;
		if (!def.signalled.compare_exchange_strong(expected, false))
		{
			return false;
		}
	}
	else
	{
		if (!def.signalled)
		{
			return false;
		}
	}

	return true;
}

bool event_handle::is_signalled()
{
	internal::event_definition& def = m_scheduler->get_event_definition(m_index);
	return def.signalled;
}

bool event_handle::operator==(const event_handle& rhs) const
{
	return (m_scheduler == rhs.m_scheduler && m_index == rhs.m_index);
}

bool event_handle::operator!=(const event_handle& rhs) const
{
	return !(*this == rhs);
}

}; /* namespace jobs */