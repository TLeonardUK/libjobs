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

event_handle::event_handle(scheduler* scheduler, const counter_handle& counter, bool auto_reset)
    : m_scheduler(scheduler)
    , m_counter(counter)
    , m_auto_reset(auto_reset)
{
}

event_handle::event_handle()
    : m_scheduler(nullptr)
    , m_auto_reset(false)
{
}

event_handle::event_handle(const event_handle& other)
{
    m_scheduler = other.m_scheduler;
    m_counter = other.m_counter;
    m_auto_reset = other.m_auto_reset;
}

event_handle::~event_handle()
{
}

event_handle& event_handle::operator=(const event_handle& other)
{
    m_scheduler = other.m_scheduler;
    m_counter = other.m_counter;
    m_auto_reset = other.m_auto_reset;

    return *this;
}

result event_handle::wait(timeout in_timeout)
{
    if (m_auto_reset)
    {
        return m_counter.remove(1, in_timeout);
    }
    else
    {
        return m_counter.wait_for(1, in_timeout);
    }
}

result event_handle::signal()
{
    return m_counter.set(1);
}

result event_handle::reset()
{
    return m_counter.set(0);
}

bool event_handle::operator==(const event_handle& rhs) const
{
    return (m_scheduler == rhs.m_scheduler && m_counter == rhs.m_counter && m_auto_reset == rhs.m_auto_reset);
}

bool event_handle::operator!=(const event_handle& rhs) const
{
    return !(*this == rhs);
}

}; /* namespace jobs */