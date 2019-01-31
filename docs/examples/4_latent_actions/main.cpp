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

// This example shows how latent actions (wait/sleep/etc) work when dealing
// with user-space threading (fibers).

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
    printf("%s", message);
}

void main()
{
    // Create scheduler.
    jobs::scheduler scheduler;
    scheduler.set_max_jobs(10);
    scheduler.set_debug_output(debug_output);
    scheduler.add_fiber_pool(10, 16 * 1024);

    // Sets the maximum number of callbacks. This number represents the maximum of wait() functions
    // that can have a non-infinite timeout value registered at a given time.
    scheduler.set_max_callbacks(100);

    // Sets the maximum number of events that can exist at a given time. Events are described below.
    scheduler.set_max_events(10);

    // To make the effects of running latent actions obvious, we will be running this example
    // only on a single thread, no multi-threading will occur.
    scheduler.add_thread_pool(1, jobs::priority::all);

    // Initializes the scheduler.
    jobs::result result = scheduler.init();
    assert(result == jobs::result::success);

    // Create an event. Events work similar to semaphores in classic multi-threading. Fibers can pause 
    // their execution and wait on events until they are signalled elsewhere in code.
    // Events come in two flavours, auto-reset and manual-reset. 
    // Auto-reset events when signalled will will wake up the first waiting fiber and will then go back to a non-signalled state.
    // Manual-reset events will wake up all fibers and allow all future waiting fibers to continue until it's signalled state is manually reset with event_handle.reset().
    // Events can also be waited for outside of a job context, but in this case the event will block thread execution.
    //
    // In this situation, it doesn't matter which flavour we create as we only have a single job waiting on the event.
    jobs::event_handle event_1;
    result = scheduler.create_event(event_1, true);
    assert(result == jobs::result::success);

    // Create an counter. Counters can be throught of as sempahores but with far more control over the signal count.
    // They allow you to add values, remove values (and block if it would go negative), and wait for specific values to be set.
    // They are incredibly useful for synchronizing large amounts of jobs together.
    jobs::counter_handle counter_1;
    result = scheduler.create_counter(counter_1);
    assert(result == jobs::result::success);

    // Allocates a few jobs with different names.
    const int job_count = 5;
    const char* job_names[job_count] = { "Job 1 (Sleeping)", "Job 2 (Signalling Event)", "Job 3 (Waiting For Event Signal)", "Job 4 (Waiting On Sleeping Job)", "Job 5 (Waiting On Counter" };
    jobs::job_handle jobs[job_count];
    for (int i = 0; i < job_count; i++)
    {
        jobs::job_handle& job = jobs[i];

        result = scheduler.create_job(job);
        assert(result == jobs::result::success);

        job.set_tag(job_names[i]);
        job.set_stack_size(16 * 1024);
        job.set_priority(jobs::priority::low);
    }

    // Make the first job go to sleep for a while.
    jobs[0].set_work([&]() {
        printf("%s: starting sleep\n", job_names[0]);
        jobs::scheduler::sleep(8 * 1000);
        printf("%s: finish sleep\n", job_names[0]);

        counter_1.add(1);
    });

    // Make the second job signal an event.
    jobs[1].set_work([&]() {
        jobs::scheduler::sleep(4 * 1000);
        printf("%s: signaling event\n", job_names[1]);
        event_1.signal();

        counter_1.add(1);
    });

    // Make the third job wait on the event signal.
    jobs[2].set_work([&]() {
        printf("%s: waiting on event\n", job_names[2]);

        // If you only want to wait a given amount of time you can provide
        // a non-infinite timeout and check the return value for result::timeout
        // to determine the reason the function returned.
        event_1.wait(jobs::timeout::infinite);

        printf("%s: continuing\n", job_names[2]);

        counter_1.add(1);
    });

    // Make the forth job wait on the first sleeping job.
    jobs[3].set_work([&]() {
        printf("%s: waiting on sleeping job\n", job_names[3]);

        // If you only want to wait a given amount of time you can provide
        // a non-infinite timeout and check the return value for result::timeout
        // to determine the reason the function returned.
        jobs[0].wait(jobs::timeout::infinite);

        printf("%s: continuing\n", job_names[3]);

        counter_1.add(1);
    });

    // Make the fifth job that waits until a counter (incremented by other jobs) gets to a value.
    jobs[4].set_work([&]() {
        printf("%s: waiting on counter\n", job_names[4]);

        // This will wait until the counter is incremented by all the other jobs.
        counter_1.wait_for(job_count - 1, jobs::timeout::infinite);

        printf("%s: continuing\n", job_names[4]);
    });

    // Dispatch all jobs.
    for (int i = 0; i < job_count; i++)
    {
        jobs[i].dispatch();
    }

    // Expected execution:
    //
    // Notice how when this is executed none of the wait or sleep calls block execution
    // even though we are only running on a single-thread. This is one of the most powerful
    // advantages of user-space threading (fibers). Its allows jobs to be paused and 
    // other jobs to continue doing useful work on the processor, until the paused jobs are in 
    // a state where they can do something useful again.
    //
    // This also has the benefit of being able to create job dependencies organically (using job.wait)
    // rather than having to manually specify which jobs are dependent, which can get complex with
    // highly interdependent systems.

    // Wait for job to complete.
    scheduler.wait_until_idle();

    printf("All jobs completed.\n");

    return;
}