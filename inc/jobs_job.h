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

#include "jobs_enums.h"
#include "jobs_scheduler.h"

namespace jobs {
	
class job_dependency;

/**
 *  \brief Entry point for a jobs workload.
 */
typedef std::function<void()> job_entry_point;

/**
 *  \brief Current status of a job.
 */
enum class job_status
{
	initialized,	/**< Job is initialized and ready for dispatch */
	pending,		/**< Job is pending execution */
	running,		/**< Job is running on a worker */
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

	/** @todo */
	job_dependency* first_dependency = nullptr;

	/** @todo */
	bool has_successors;

};

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
	result wait(timeout in_timeout = timeout::infinite, priority assist_on_tasks = priority::all_but_slow);

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

}; /* namespace Jobs */

#endif /* __JOBS_JOB_H__ */