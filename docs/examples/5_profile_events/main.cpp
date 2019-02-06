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

// This example shows how to emit debug markers for profiling. For this example we
// use PIX as the target profiler. Take a timing capture of this example running
// to see the profile markers.

// Comments on topics previously discussed in other examples have been removed 
// or simplified, go back to older examples if you are unsure of anything.

#include <jobs.h>
#include <cassert>
#include <cstdio>
#include <cmath>

#if defined(JOBS_PLATFORM_WINDOWS) || defined(JOBS_PLATFORM_XBOX_ONE)
#include <pix/include/WinPixEventRuntime/pix3.h>
#elif defined(JOBS_PLATFORM_PS4)
#include <razorcpu.h>
#endif

void debug_output(
    jobs::debug_log_verbosity level, 
    jobs::debug_log_group group, 
    const char* message)
{
    printf("%s", message);
}

// This function is invoked when a job enters a new frame on the profile scope stack. 
// This function should be used for creating a new marker.
// The type of scope provides semantic context to the type of scope being entered, by default the 
// library will enter various scopes to mark high-level scheduling work.
// Any user-defined scopes should use a enum value >= to the profile_scope_type::user_defined value.
// The tag is whatever user-defined message to associate with the scope. This should describe 
// what the scope's work consists of.
void enter_scope(jobs::profile_scope_type type, const char* tag)
{
#if defined(JOBS_PLATFORM_WINDOWS) || defined(JOBS_PLATFORM_XBOX_ONE)

    UINT64 color;

    // We choose different colors dependending on the type of scope we are emitting.
    if (type == jobs::profile_scope_type::worker)
    {
        color = PIX_COLOR(255, 0, 0);
    }
    else if (type == jobs::profile_scope_type::fiber)
    {
        color = PIX_COLOR(0, 0, 255);
    }
    else
    {
        color = PIX_COLOR(0, 255, 0);
    }

    // For this example we emit a pix event. You should replace this with your own profiler
    // API, vtune, razor, whatever.
    PIXBeginEvent(color, "%s", tag);

#elif defined(JOBS_PLATFORM_PS4)
    
    uint32_t color = 0xFF00FF00;
    if (type == jobs::profile_scope_type::worker)
    {
        color = 0xFF0000FF;
    }
    else if (type == jobs::profile_scope_type::fiber)
    {
        color = 0xFFFF0000;
    }
    else
    {
        color = 0xFF00FF00;
    }

    sceRazorCpuPushMarker(tag, color, 0);

#endif
}

// This function is invoked when a previously entered scope is left. 
// This function should be use for terminating the marker at the top of the stack.
void leave_scope()
{
#if defined(JOBS_PLATFORM_WINDOWS) || defined(JOBS_PLATFORM_XBOX_ONE)

    // Leave the pix event at the top of the stack.
    PIXEndEvent();

#elif defined(JOBS_PLATFORM_PS4)
    
    sceRazorCpuPopMarker();

#endif
}

void jobsMain()
{
    jobs::scheduler scheduler;
    scheduler.add_thread_pool(jobs::scheduler::get_logical_core_count(), jobs::priority::all);
    scheduler.add_fiber_pool(100, 16 * 1024);
    scheduler.set_debug_output(debug_output);

    // This struct defines overrides for all the profiling debug functions. The functions
    // will be called at appropriate times to allow the user to emit profile makers. The actual
    // API is profiler agnostic, it's up to the user to implement the markers for their chosen profiler. The
    // functions above show how to do it for PIX.
    //
    // The scheduler will automatically deal with situations such as removing and re-adding markers when
    // fibers and unscheduled and rescheduled. The user functions should only have to do the minimal work
    // required to start/end a profile marker.
    jobs::profile_functions profile_functions;
    profile_functions.enter_scope = &enter_scope;
    profile_functions.leave_scope = &leave_scope;

    // Set the overriden profile functions for the scheduler.
    scheduler.set_profile_functions(profile_functions);

    // Initializes the scheduler.
    jobs::result result = scheduler.init();
    assert(result == jobs::result::success);

    // Dispatch a whole bunch of jobs that do dummy work so they can be seen in a profile.
    for (int i = 0; i < 100; i++)
    {
        // Allocates a new job.
        jobs::job_handle job;
        result = scheduler.create_job(job);
        assert(result == jobs::result::success);

        // Setup job.
        job.set_tag("Job");
        job.set_stack_size(16 * 1024);
        job.set_priority(jobs::priority::low);
        job.set_work([=]() {

            // Create a couple of dummy workloads so we can see 
            // different scopes appear on the profile.
            for (int i = 0; i < 10; i++)
            {
                // You can use jobs::profile_scope to define your own scopes. This struct follows
                // simple RAII rules. When it constructed it enters a profile scope, when it 
                // is destroyed it leaves a profile scope. You can easily nest these to deliminate
                // useful profiling information.
                jobs::profile_scope scope(jobs::profile_scope_type::user_defined, "Dummy Work");

                // Some dummy work so we don't complete instantly. Marked as volatile
                // to prevent removal during optimization.
                volatile double sum = 0;
                for (int i = 0; i < 2000000; i++) sum += atan2(i, i / 2);
            }

        });

        // Dispatch job.
        job.dispatch();
    }

    // Wait for job to complete.
    scheduler.wait_until_idle();

    printf("All jobs completed.\n");
}