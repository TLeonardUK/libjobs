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

#include "jobs_job.h"

#include <cassert>

namespace jobs {

job_definition::job_definition()
{
	reset();
}	

void job_definition::reset()
{
	ref_count = 0;
	work = nullptr;
	stack_size = 0;
	job_priority = priority::medium;
	status = job_status::initialized;
	has_successors = false;
	queues_contained_in = 0;

	// Should have been cleaned up by scheduler at this point ...
	assert(first_dependency == nullptr);
	assert(has_fiber == false);
}

job_handle::job_handle(scheduler* scheduler, size_t index)
	: m_scheduler(scheduler)
	, m_index(index)
{
	increase_ref();
}

job_handle::job_handle()
	: m_scheduler(nullptr)
	, m_index(0)
{
}

job_handle::job_handle(const job_handle& other)
{
	m_scheduler = other.m_scheduler;
	m_index = other.m_index;

	increase_ref();
}

job_handle::~job_handle()
{
	decrease_ref();
}

job_handle& job_handle::operator=(const job_handle& other)
{
	m_scheduler = other.m_scheduler;
	m_index = other.m_index;

	increase_ref();

	return *this;
}

void job_handle::increase_ref()
{
	m_scheduler->increase_job_ref_count(m_index);
}

void job_handle::decrease_ref()
{
	m_scheduler->decrease_job_ref_count(m_index);
}

result job_handle::set_work(const job_entry_point& job_work)
{
	if (!is_valid())
	{
		return result::invalid_handle;
	}
	if (!is_mutable())
	{
		return result::not_mutable;
	}

	job_definition& definition = m_scheduler->get_job_definition(m_index);
	definition.work = job_work;

	return result::success;
}

result job_handle::set_stack_size(size_t stack_size)
{
	if (!is_valid())
	{
		return result::invalid_handle;
	}
	if (!is_mutable())
	{
		return result::not_mutable;
	}

	job_definition& definition = m_scheduler->get_job_definition(m_index);
	definition.stack_size = stack_size;

	return result::success;
}

result job_handle::set_priority(priority job_priority)
{
	if (!is_valid())
	{
		return result::invalid_handle;
	}
	if (!is_mutable())
	{
		return result::not_mutable;
	}

	job_definition& definition = m_scheduler->get_job_definition(m_index);
	definition.job_priority = job_priority;

	return result::success;
}

result job_handle::clear_dependencies()
{
	if (!is_valid())
	{
		return result::invalid_handle;
	}
	if (!is_mutable())
	{
		return result::not_mutable;
	}

	m_scheduler->clear_job_dependencies(m_index);

	return result::success;
}

result job_handle::add_predecessor(job_handle other)
{
	if (!is_valid())
	{
		return result::invalid_handle;
	}
	if (!is_mutable())
	{
		return result::not_mutable;
	}

	return m_scheduler->add_job_dependency(m_index, other.m_index);
}

result job_handle::add_successor(job_handle other)
{
	if (!is_valid())
	{
		return result::invalid_handle;
	}
	if (!is_mutable())
	{
		return result::not_mutable;
	}

	return m_scheduler->add_job_dependency(other.m_index, m_index);
}

bool job_handle::is_pending()
{
	if (!is_valid())
	{
		return false;
	}

	job_definition& definition = m_scheduler->get_job_definition(m_index);
	return (definition.status == job_status::pending);
}

bool job_handle::is_running()
{
	if (!is_valid())
	{
		return false;
	}

	job_definition& definition = m_scheduler->get_job_definition(m_index);
	return (definition.status == job_status::running);
}

bool job_handle::is_complete()
{
	if (!is_valid())
	{
		return false;
	}

	job_definition& definition = m_scheduler->get_job_definition(m_index);
	return (definition.status == job_status::completed);
}

bool job_handle::is_mutable()
{
	if (!is_valid())
	{
		return false;
	}

	job_definition& definition = m_scheduler->get_job_definition(m_index);

	return definition.status == job_status::initialized || 
		   definition.status == job_status::completed;
}

bool job_handle::is_valid()
{
	return (m_scheduler != nullptr);
}

result job_handle::wait(timeout in_timeout)// , priority assist_on_tasks)
{
	if (!is_valid())
	{
		return result::invalid_handle;
	}

	return m_scheduler->wait_for_job(*this, in_timeout);// , assist_on_tasks);
}

result job_handle::dispatch()
{
	if (!is_valid())
	{
		return result::invalid_handle;
	}
	if (!is_mutable())
	{
		return result::not_mutable;
	}

	return m_scheduler->dispatch_job(m_index);
}

bool job_handle::operator==(const job_handle& rhs) const
{
	return (m_scheduler == rhs.m_scheduler && m_index == rhs.m_index);
}

bool job_handle::operator!=(const job_handle& rhs) const
{
	return !(*this == rhs);
}

}; /* namespace Jobs */