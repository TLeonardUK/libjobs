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

// This example shows how to register a debug output function to a scheduler
// to provide useful debugging information.

// Comments on topics previously discussed in other examples have been removed 
// or simplified, go back to older examples if you are unsure of anything.

#include <jobs.h>
#include <cassert>
#include <cstdio>

// This is the function that will be called when the scheduler wants to write out 
// debugging messages. 
//
// The verbosity level determines how important the message is, you 
// likely want to print everything but verbose logs while debugging.
//
// The group defines a semantic area of the library that the message belongs to, such
// as memory allocation/job scheduling/etc.
//
// The message is the actual debug output the scheduler wants to emit.
void debug_output(
    jobs::debug_log_verbosity level, 
    jobs::debug_log_group group, 
    const char* message)
{
    JOBS_PRINTF("%s", message);
}

void jobsMain()
{
    jobs::scheduler scheduler;
    scheduler.add_thread_pool(jobs::scheduler::get_logical_core_count(), jobs::priority::all);
    scheduler.add_fiber_pool(10, 16 * 1024);

    // This function assigns a function to the scheduler that will be called whenever
    // the scheduler wants to write any debug output. You should disable this in release
    // builds to remove the overhead of formatting logging messages.
    // The second parameter allows you to set the maximum verbosity you want to log, generally
    // you never want this higher than message unless your heavily debugging the job system.
    scheduler.set_debug_output(debug_output, jobs::debug_log_verbosity::message);

    // Initializes the scheduler.
    jobs::result result = scheduler.init();
    assert(result == jobs::result::success);

    // Allocates a new job.
    jobs::job_handle job_1;
    result = scheduler.create_job(job_1);
    assert(result == jobs::result::success);

    // Setup job.
    job_1.set_tag("Example Job");
    job_1.set_stack_size(16 * 1024);
    job_1.set_priority(jobs::priority::low);
    job_1.set_work([=]() {
        JOBS_PRINTF("Example job executed\n");
    });

    // Dispatch job.
    job_1.dispatch();

    // Wait for job to complete.
    scheduler.wait_until_idle();

    JOBS_PRINTF("All jobs completed.\n");
}