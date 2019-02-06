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

// This example shows the minimal steps required to setup and 
// run a simple job on a scheduler.

#include <cstdio>
#include <jobs.h>
#include <cassert>

void jobsMain()
{
    jobs::scheduler scheduler;

    // This sets the maximum number of jobs that can be created 
    // and managed by this scheduler at any given time.
    scheduler.set_max_jobs(10);

    // Create a pool of worker threads. We create one worker for each logical core. 
    // The priority for this example is set to all jobs, you can change this to any priority bit-mask. 
    // You can also add extra worker pools for different priorities, if you want to have more fine grained control 
    // over the compute resources allocated to different jobs, this is especially useful if you want to segregate 
    // off long-running jobs (IO/Network/etc) from more time-critical jobs.
    scheduler.add_thread_pool(jobs::scheduler::get_logical_core_count(), jobs::priority::all);

    // Adds a pool of fibers. Fibers contain the execution context of each job that is currently active (running/waiting).
    // Each pool contains a given amount of fibers that have a fixed amount of stack-space. Jobs that begin running will allocate 
    // a fiber from the first pool (ordered by stack space) that has has a large enough stack space quota to fill the jobs requirements.
    // Multiple pools of varying sizes can be created to have fine-grain control over the memory usage.
    // Make sure you allocate enough fibers to run all concurrently active jobs you intend to run.
    scheduler.add_fiber_pool(10, 16 * 1024);

    // Initializes the scheduler. All memory allocation is done at this point. The scheduler will never allocate memory past this point.
    // Once this returns successfully, jobs can begin being scheduled.
    jobs::result result = scheduler.init();
    assert(result == jobs::result::success);

    // Allocates a new job ready to be configured and executed. 
    jobs::job_handle job_1;
    result = scheduler.create_job(job_1);
    assert(result == jobs::result::success);

    // Sets a jobs tag. This is a descriptive name used to describe the workload.
    // It is used for logging and profiling purposes.
    job_1.set_tag("Example Job");

    // Sets the actual work that is executed when the work is run. This is held as
    // and std::function, so lambda's/function-ptr's all work fine.
    job_1.set_work([=]() {
        printf("Example job executed\n");
    });

    // Sets the stack-size the job needs to run. The size of this is heavily dependent on what 
    // workload you intend to execute in the job. Make sure one of your fiber pools has a large enough stack
    // space quota to fulfil this request.
    job_1.set_stack_size(16 * 1024);

    // Sets a priority. Higher priorities get executed before lower priorities. Depending on the configuration of
    // the worker thread pool's, some jobs may only be executed by specific workers.
    job_1.set_priority(jobs::priority::low);

    // Dispatches the job for execution. After this is called the job is immutable, and may
    // not be changed or dispatched again until it is completed.
    job_1.dispatch();

    // Waits for the scheduler to finish running all jobs. You can also wait
    // on individual jobs or events if you need to (show in future examples).
    scheduler.wait_until_idle();

    printf("All jobs completed.\n");
}