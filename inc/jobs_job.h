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
 *  \file jobs_job.h
 *
 *  Include header for individual job management functionality.
 */

#ifndef __JOBS_JOB_H__
#define __JOBS_JOB_H__

#include <atomic>

#include "jobs_utils.h"
#include "jobs_enums.h"
#include "jobs_scheduler.h"
#include "jobs_fiber.h"

namespace jobs {

class scheduler;

/**
	*  \brief Entry point for a jobs workload.
	*/
typedef std::function<void()> job_entry_point;

namespace internal {
	
class job_definition;
class job_dependency;
class job_context;
class profile_scope_definition;

/**
 * Holds the execution context of a job, this provides various functionality to 
 * manipulate the execution of a job, such as waiting for events, creating profile scopes, etc.
 */
class job_context
{
protected:

	friend class jobs::scheduler;

	/** @todo */
	bool has_fiber = false;

	/** @todo */
	size_t fiber_index;

	/** @todo */
	size_t fiber_pool_index;

	/** @todo */
	size_t queues_contained_in;

	/** @todo */
	bool is_fiber_raw;

	/** @todo */
	fiber raw_fiber;

	/** @todo */
	size_t profile_scope_depth;

	/** @todo */
	profile_scope_definition* profile_stack_head = nullptr;

	/** @todo */
	profile_scope_definition* profile_stack_tail = nullptr;

	/** @todo */
	jobs::scheduler* scheduler = nullptr;

	/** @todo */
	job_definition* job_def = nullptr;

public:

	/** @todo */
	job_context();

	/** @todo */
	void reset();

	/** @todo */
	//result sleep(size_t milliseconds);

	/** @todo */
	//result wait_for(job_event evt);

	/** @todo */
	result enter_scope(profile_scope_type type, const char* tag, ...);

	/** @todo */
	result leave_scope();

};

/**
 *  \brief Current status of a job.
 */
enum class job_status
{
	initialized,	/**< Job is initialized and ready for dispatch */
	pending,		/**< Job is pending execution */
	running,		/**< Job is running on a worker */
	waiting,		/**< Job is waiting for an event. */
	completed,		/**< Job has completed running */
};

/**
 * Encapsulates all the settings required to dispatch and run an instance of a job. This
 * is used for internal storage, and shouldn't ever need to be touched by outside code.
 */
class job_definition
{
public:

	/** @todo */
	job_definition();

	/** @todo */
	void reset();

public:	

	/** @todo */
	std::atomic<size_t> ref_count;

	/** @todo */
	job_entry_point work;

	/** @todo */
	size_t stack_size;

	/** @todo */
	priority job_priority;

	/** @todo */
	job_status status;

	// Note: dependencies are only safe to modify in two situations:
	//			- when job is not running and is mutable
	//			- when job is running and is being modified by the fiber executing it (when not queued).

	/** @todo */
	job_dependency* first_predecessor = nullptr;

	/** @todo */
	job_dependency* first_successor = nullptr;

	/** @todo */
	std::atomic<size_t> pending_predecessors;

	/** @todo */
	job_context context;

	/** @todo */
	static const size_t max_tag_length = 64;

	/** @todo */
	char tag[max_tag_length];

};

}; /* nemspace internal */

/**
 * \brief Represents an instance of a job that has been created by the scheduler.
 *
 * Job data is owned by the scheduler, be careful accessing handles if 
 * the scheduler has been destroyed.
 */
class job_handle
{
protected:

	friend class scheduler;

	/** @todo */
	job_handle(scheduler* scheduler, size_t index);

	/** @todo */
	void increase_ref();

	/** @todo */
	void decrease_ref();

public:

	/** @todo */
	job_handle();

	/** @todo */
	job_handle(const job_handle& other);

	/** @todo */
	~job_handle();

	/** @todo */
	job_handle& operator=(const job_handle& other);

	/** @todo */
	result set_work(const job_entry_point& job_work);

	/** @todo */
	result set_tag(const char* tag);

	/** @todo */
	result set_stack_size(size_t stack_size);

	/** @todo */
	result set_priority(priority job_priority);

	/** @todo */
	result clear_dependencies();

	/** @todo */
	result add_predecessor(job_handle other);

	/** @todo */
	//result add_predecessor(event* other);

	/** @todo */
	result add_successor(job_handle other);

	/** @todo */
	//result add_successor(event* other);

	/** @todo */
	bool is_pending();

	/** @todo */
	bool is_running();

	/** @todo */
	bool is_complete();

	/** @todo */
	bool is_mutable();

	/** @todo */
	bool is_valid();

	/** @todo */
	result wait(timeout in_timeout = timeout::infinite);// , priority assist_on_tasks = priority::all_but_slow);

	/** @todo */
	result dispatch();

	/** @todo */
	bool operator==(const job_handle& rhs) const;

	/** @todo */
	bool operator!=(const job_handle& rhs) const;

private:

	/** Pointer to the owning scheduler of this handle. */
	scheduler* m_scheduler = nullptr;

	/** @todo */
	size_t m_index = 0;

};

namespace internal {

/**
 * Holds an individual dependency of a job, allocated from a pool by the scheduler
 * and joined together as a linked list.
 */
class job_dependency
{
public:

	/** @todo */
	job_dependency(size_t in_pool_index)
		: pool_index(in_pool_index)
	{
	}

	/** @todo */
	void reset()
	{
		// pool_index should not be reset, it should be persistent.
		job = job_handle();
		next = nullptr;
	}

	/** @todo */
	size_t pool_index;

	/** @todo */
	job_handle job;

	/** @todo */
	job_dependency* next = nullptr;

};

/**
 * Represents an individual scope in a fibers profiling hierarchy.
 * This is stored together as a single linked list.
 */
class profile_scope_definition
{
public:

	/** @todo */
	profile_scope_type type;

	/** @todo */
	static const size_t max_tag_length = 64;

	/** @todo */
	char tag[max_tag_length];

	/** @todo */
	profile_scope_definition* next;

	/** @todo */
	profile_scope_definition* prev;

};

}; /* namespace internal */

/**
 * Simple RAII type that enters a profile scope on construction and exits it
 * on destruction.
 */
class profile_scope
{
private:
	internal::job_context* m_context = nullptr;

public:

	/** @todo */
	profile_scope(jobs::profile_scope_type type, const char* tag);

	/** @todo */
	~profile_scope();
};

}; /* namespace jobs */

#endif /* __JOBS_JOB_H__ */