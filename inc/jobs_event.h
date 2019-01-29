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

#include "jobs_enums.h"
#include "jobs_utils.h"

#include <atomic>

namespace jobs {
	
namespace internal {
	
/**
 * Encapsulates all the settings required to manage an event. This is used 
 * for internal storage, and shouldn't ever need to be touched by outside code.
 */
class event_definition
{
public:

	/** @todo */
	event_definition();

	/** @todo */
	void reset();

public:	

	/** @todo */
	std::atomic<size_t> ref_count;

	/** @todo */
	std::atomic<bool> signalled;

	/** @todo */
	bool auto_reset;

};

}; /* namespace internal */

/**
 * \brief Represents an instance of an event that has been created by the scheduler.
 *
 * Events data is owned by the scheduler, be careful accessing handles if 
 * the scheduler has been destroyed.
 *
 * Events in this library work in two ways. Manual-reset and auto-reset. 
 *
 * For manual-reset events, all fibers that wait on the event will be allowed to continue 
 * once the event is signaled, and will continue being allowed until the event is manually reset
 * from elsewhere.
 *
 * Auto-reset events work similarly to manual-reset, except after the first thread is woken up 
 * on signal, the event is automatically (and atomically) reset.
 */
class event_handle
{
protected:

	friend class scheduler;

	/** @todo */
	event_handle(scheduler* scheduler, size_t index);

	/** @todo */
	void increase_ref();

	/** @todo */
	void decrease_ref();

public:

	/** @todo */
	event_handle();

	/** @todo */
	event_handle(const event_handle& other);

	/** @todo */
	~event_handle();

	/** @todo */
	event_handle& operator=(const event_handle& other);

	/** @todo */
	result wait(timeout in_timeout = timeout::infinite);

	/** @todo */
	result signal();

	/** @todo */
	result reset();

	/** @todo */
	bool is_signalled();

	/** @todo */
	bool operator==(const event_handle& rhs) const;

	/** @todo */
	bool operator!=(const event_handle& rhs) const;

private:

	/** @todo */
	bool consume_signal();

	/** Pointer to the owning scheduler of this handle. */
	scheduler* m_scheduler = nullptr;

	/** @todo */
	size_t m_index = 0;

};

}; /* namespace jobs */

#endif /* __JOBS_JOB_H__ */