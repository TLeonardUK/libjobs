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
	job(int pool_index);

public:

	void set_work(const job_entry_point& job_work);
	void set_stack_size(size_t stack_size);
	void set_priority(priority job_priority);
	void add_dependency(job* other);


};

}; /* namespace Jobs */

#endif /* __JOBS_JOB_H__ */