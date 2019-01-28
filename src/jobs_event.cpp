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

namespace jobs {
namespace internal {

event_definition::event_definition()
{
	reset();
}	

void event_definition::reset()
{
	ref_count = 0;	
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
	// @todo
	return result::success;
}

result event_handle::signal()
{
	// @todo
	return result::success;
}

result event_handle::reset()
{
	// @todo
	return result::success;
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