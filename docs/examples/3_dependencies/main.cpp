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

// This example shows how to setup dependencies between different jobs to 
// control their execution order

// Comments on topics previously discussed in other examples have been removed 
// or simplified, go back to older examples if you are unsure of anything.

#include <jobs.h>
#include <cassert>
#include <string>

void debug_output(
    jobs::debug_log_verbosity level, 
    jobs::debug_log_group group, 
    const char* message)
{
    if (level == jobs::debug_log_verbosity::verbose)
    {
        return;
    }

    printf("%s", message);
}

void main()
{
    // Create scheduler.
    jobs::scheduler scheduler;
    scheduler.add_thread_pool(jobs::scheduler::get_logical_core_count(), jobs::priority::all);
    scheduler.add_fiber_pool(10, 16 * 1024);
    scheduler.set_debug_output(debug_output);

    // Set the maximum number of dependencies that can exist between all jobs that exist 
    // at a given time. This has a relatively small memory cost, so its useually quite
    // safe to increase it substantially from the default.
    scheduler.set_max_dependencies(100);

    // Initializes the scheduler.
    jobs::result result = scheduler.init();
    assert(result == jobs::result::success);

    // Allocates a few jobs with different names.
    const int job_count = 3;
    const char* job_names[job_count] = { "Dependent Job 1", "Primary Job", "Dependent Job 2" };
    jobs::job_handle jobs[job_count];
    for (int i = 0; i < job_count; i++)
    {
        jobs::job_handle& job = jobs[i];

        result = scheduler.create_job(job);
        assert(result == jobs::result::success);

        // Setup job.
        job.set_tag(job_names[i]);
        job.set_stack_size(16 * 1024);
        job.set_priority(jobs::priority::low);
        job.set_work([=]() {
            printf("%s executed\n", job_names[i]);
        });
    }

    // Make the first job dependent on the second job. This will
    // ensure the first job always run after the second job has run.
    //
    // If it makes it semantically easier to read you can also use the function
    // add_successor, which works as the inverse, making job[1] run before job[0].
    //
    // Each job can have as many dependencies as required. But the maximum number of dependencies
    // allocated by all jobs at a given time is always limited by the scheduler::set_max_dependencies value.
    jobs[0].add_predecessor(jobs[1]);

    // Third job dependent on the second.
    jobs[2].add_predecessor(jobs[1]);

    // First job dependent on the third.
    jobs[0].add_predecessor(jobs[2]);

    // Resulting execution order from this should be:
    //	job[1] (Primary job)
    //	job[2] (Dependent Job 2)
    //  job[0] (Dependent Job 1)

    // Dispatch all jobs.
    for (int i = 0; i < job_count; i++)
    {
        jobs[i].dispatch();
    }

    // Wait for job to complete.
    scheduler.wait_until_idle();

    printf("All jobs completed.\n");

    return;
}