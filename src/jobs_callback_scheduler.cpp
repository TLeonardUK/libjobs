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

#include "jobs_callback_scheduler.h"
#include "jobs_scheduler.h"

#include <algorithm>

namespace jobs {
namespace internal {

callback_scheduler::callback_scheduler()
{
}

callback_scheduler::~callback_scheduler()
{
	m_shutting_down = true;

	// Notify callback thread that its time to shutdown.
	{
		std::unique_lock<std::mutex> lock(m_schedule_updated_mutex);
		m_schedule_updated_cvar.notify_all();
	}

	if (m_callback_thread != nullptr)
	{
		m_callback_thread->join();
		m_memory_functions.user_free(m_callback_thread);
		m_callback_thread = nullptr;
	}
}

result callback_scheduler::init(jobs::scheduler* scheduler, size_t max_callbacks, const jobs::memory_functions& memory_functions)
{
	m_scheduler = scheduler;
	m_memory_functions = memory_functions;

	// Init callback pool.
	result result = m_callback_pool.init(m_memory_functions, max_callbacks, [this](callback_definition* instance, size_t index)
	{
		new(instance) callback_definition();
		return result::success;
	});
	if (result != result::success)
	{
		return result;
	}

	// Init main thread.
	m_callback_thread = (thread*)m_memory_functions.user_alloc(sizeof(thread));
	new(m_callback_thread) thread(m_memory_functions);

	m_callback_thread->init([&]()
	{
		while (!m_shutting_down)
		{
			std::unique_lock<std::mutex> lock(m_schedule_updated_mutex);

			run_callbacks();

			// No callbacks? Wait until we have one.
			if (m_callback_pool.count() == 0)
			{
				m_schedule_updated_cvar.wait(lock);
			}
			// Wait until next callback is due to run.
			else
			{
				m_schedule_updated_cvar.wait_for(lock, std::chrono::milliseconds(get_ms_till_next_callback()));
			}
		}
	});

	return result::success;
}

void callback_scheduler::run_callbacks()
{
	// @todo: this has unneccessary iteration over free elements, need to maintain an allocated list.
	for (size_t i = 0; i < m_callback_pool.capacity(); i++)
	{
		callback_definition& def = *m_callback_pool.get_index(i);
		if (def.active)
		{
			if (def.stopwatch.get_elapsed_ms() >= def.duration.duration)
			{
				def.callback();

				def.callback = nullptr;
				def.active = false;

				m_callback_pool.free(i);
			}
		}
	}
}

uint64_t callback_scheduler::get_ms_till_next_callback()
{
	uint64_t time_till_next = UINT64_MAX;

	// @todo: this has unneccessary iteration over free elements, need to maintain an allocated list.
	for (size_t i = 0; i < m_callback_pool.capacity(); i++)
	{
		callback_definition& def =* m_callback_pool.get_index(i);
		if (def.active)
		{
			size_t remaining = max(0, def.duration.duration - def.stopwatch.get_elapsed_ms());
			time_till_next = min(time_till_next, remaining);
		}
	}

	return time_till_next;
}

result callback_scheduler::schedule(timeout duration, const callback_scheduler_function& callback)
{
	std::unique_lock<std::mutex> lock(m_schedule_updated_mutex);

	size_t index = 0;
	result res = m_callback_pool.alloc(index);
	if (res != result::success)
	{
		m_scheduler->write_log(debug_log_verbosity::warning, debug_log_group::scheduler, "attempt to create latent callback, but pool is empty. Behaviour may be incorrect. Try increasing scheduler::set_max_callbacks.");
		return res;
	}

	callback_definition* def = m_callback_pool.get_index(index);
	def->active = true;
	def->stopwatch.start();
	def->duration = duration;
	def->callback = callback;

	m_schedule_updated_cvar.notify_all();

	return result::success;
}

}; /* namespace internal */
}; /* namespace jobs */