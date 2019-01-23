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

#include "jobs_enums.h"
#include "jobs_scheduler.h"

namespace jobs {
	
/**
 *  \brief Entry point for a jobs workload.
 */
typedef std::function<void()> job_entry_point;

/**
 * Encapsulates all the settings required to dispatch and run an instance of a job.
 */
class job_definition
{
public:

};

/**
 * \brief Represents an instance of a job that has been created by the scheduler.
 *
 * Job data is owned by the scheduler, be careful accessing handles if 
 * the scheduler has been destroyed.
 */
class job_handle
{
public:

	// @todo: these are just stubs that will update the definition held by the scheduler.

	job_handle();

	job_handle(const job_handle& other);

	~job_handle();

	job_handle& operator=(const job_handle& other);

	/** @todo */
	void set_work(const job_entry_point& job_work);

	/** @todo */
	void set_stack_size(size_t stack_size);

	/** @todo */
	void set_priority(priority job_priority);

	/** @todo */
	void clear_dependencies();

	/** @todo */
	void add_predecessor(job_handle other);

	/** @todo */
	//void add_predecessor(event* other);

	/** @todo */
	void add_successor(job_handle other);

	/** @todo */
	//void add_successor(event* other);

	/** @todo */
	bool is_pending();

	/** @todo */
	bool is_running();

	/** @todo */
	bool is_complete();

	/** @todo */
	bool is_valid();

	/** @todo */
	bool wait(timeout = timeout::infinite);

	/** @todo */
	void dispatch();

private:

	/** Pointer to the owning scheduler of this handle. */
	scheduler* m_scheduler;

	/** @todo */
	size_t m_index;

};

#if 0
/**
 *  Encapsulates a single job of work that can be executed by a scheduler.
 */
class job
{
protected:

	friend class scheduler;

	// We only permit the scheduler to instantiate jobs, everywhere else
	// the jobs must be passed around by reference.
	job() = delete;

	/**
	 * \brief Constructor.
	 *
	 * \param pool_index Index of the job inside the schedulers internal job pool.
	 */
	job(size_t pool_index);

public:

	/** */
	void set_work(const job_entry_point& job_work);

	/** */
	void set_stack_size(size_t stack_size);

	/** */
	void set_priority(priority job_priority);

	/** */
	void add_dependency(job* other);

private:

	/** */
	size_t m_pool_index;

};
#endif

}; /* namespace Jobs */

#endif /* __JOBS_JOB_H__ */