#include "jobs.h"

#include <cassert>
#include <string>

// DEBUG DEBUG DEBUG
#include <Windows.h>
#include <pix/include/WinPixEventRuntime/pix3.h>
// DEBUG DEBUG DEBUG

void* user_alloc(size_t size)
{
    return malloc(size);
}

void user_free(void* ptr)
{
    free(ptr);
}

void debug_output(jobs::debug_log_verbosity level, jobs::debug_log_group group, const char* message)
{
	if (level == jobs::debug_log_verbosity::verbose)
	{
		// Do not care about this ...
		return;
	}
	printf("%s", message);
	OutputDebugStringA(message);
}

/*void job_1_work(jobs::job_context& context)
{
    printf("Job 1!");
    context.wait_for_event(next_frame_event);
}

void job_2_work(jobs::job_context& context)
{
    printf("Job 2!");
    context.wait_for_event(next_frame_event);
//    context.wait_for_job();
}*/

void enter_scope(jobs::scope_type type, const char* tag)
{
	UINT64 color;
	if (type == jobs::scope_type::worker)
	{
		color = PIX_COLOR(255, 0, 0);
	}
	else if (type == jobs::scope_type::fiber)
	{
		color = PIX_COLOR(0, 0, 255);
	}
	else 
	{
		color = PIX_COLOR(0, 255, 0);
	}

	//printf("enter %s\n", tag);
	PIXBeginEvent(color, "%s", tag);
}

void leave_scope()
{
	//printf("leave\n");
	PIXEndEvent();
}

int main()
{
	// Define overidden memory functions.
	jobs::memory_functions memory_functions;
	memory_functions.user_alloc = &user_alloc;
	memory_functions.user_free = &user_free;
	
	// Define profiling functions.
	jobs::profile_functions profile_functions;
	profile_functions.enter_scope = &enter_scope;
	profile_functions.leave_scope = &leave_scope;

	// Create scheduler.
    jobs::scheduler scheduler;
	scheduler.set_debug_output(debug_output);
	scheduler.set_profile_functions(profile_functions);
	scheduler.set_memory_functions(memory_functions);
    scheduler.set_max_jobs(200000);
	scheduler.set_max_dependencies(200000);
	scheduler.set_max_profile_scopes(200000);
	scheduler.set_max_events(1000);
	scheduler.add_thread_pool(1, jobs::priority::slow);
	scheduler.add_thread_pool(10, jobs::priority::all_but_slow);
    scheduler.add_fiber_pool(100, 64 * 1024);
    scheduler.add_fiber_pool(1000, 1 * 1024);
    scheduler.add_fiber_pool(10, 2 * 1024 * 1024);

    jobs::result result = scheduler.init();
    assert(result == jobs::result::success);

	// Job 1
	jobs::job_handle job1;
	result = scheduler.create_job(job1);
	assert(result == jobs::result::success);

	job1.set_tag("Final Job");
	job1.set_work([=](jobs::job_context& context) {
		printf("Final executed\n"); 
		Sleep(1000);
	});
	job1.set_stack_size(5 * 1024);
	job1.set_priority(jobs::priority::low);

	/*
	for (int i = 0; i < 100000; i++)
	{
		// Job 2
		jobs::job_handle job2;
		result = scheduler.create_job(job2);
		assert(result == jobs::result::success);

		job2.set_tag(("Sub-job " + std::to_string(i)).c_str());
		job2.set_work([=](jobs::job_context& context) {
			context.enter_scope(jobs::scope_type::user_defined, "Fake Work");

			volatile float max = 0;
			for (int i = 0; i < 100000; i++)
			{
				max += atan2(i, i / 2);
			}

			context.sleep(100);
			context.wait_for_event();

			context.leave_scope();
		});
		job2.set_stack_size(5 * 1024);
		job2.set_priority(jobs::priority::low);

		job1.add_predecessor(job2);

		job2.dispatch();
	}
	*/

	// for sleep(x)
	//		have additional thread dedicated to firing the sleep event. It always sleeps on a cvar with the timeout being
	//		the time until the next sleep timeout. When a new sleep event is queued a signal is sent to wakeup the thread
	//		to update it's timeout
	
	// for event(x)
	//		we just add a predecessor to the counter, and reduce it once signaled.

	job::event_handle second_stage_event;
	result = scheduler.create_event(second_stage_event);
	assert(result == jobs::result::success);

	for (int i = 0; i < 100000; i++)
	{
		// Job 2
		jobs::job_handle job2;
		result = scheduler.create_job(job2);
		assert(result == jobs::result::success);

		job2.set_tag(("Sub-job " + std::to_string(i)).c_str());
		job2.set_stack_size(5 * 1024);
		job2.set_priority(jobs::priority::low);
		job2.set_work([=](jobs::job_context& context) {

			context.enter_scope(jobs::scope_type::user_defined, "Fake Work");
			volatile float max = 0;
			for (int i = 0; i < 100000; i++)
			{
				max += atan2(i, i / 2);
			}
			context.leave_scope();

			context.wait_for_event(second_stage_event, jobs::timeout::infinite);

			context.enter_scope(jobs::scope_type::user_defined, "Fake Work 2");
			volatile float max = 0;
			for (int i = 0; i < 100000; i++)
			{
				max += atan2(i, i / 2);
			}
			context.leave_scope();

		});

		job1.add_predecessor(job2);
		job2.dispatch();
	}

	// Dispatch and wait
	job1.dispatch();

	// Wait before second stage.
	Sleep(10 * 1000);
	second_stage_event.signal();

	scheduler.wait_until_idle();

	printf("Press any key to exit ...\n");
	getchar();

	return 0;
}
