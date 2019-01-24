#include "jobs.h"

#include <cassert>

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

int main()
{
	// Defined overidden memory functions.
	jobs::memory_functions memory_functions;
	memory_functions.user_alloc = &user_alloc;
	memory_functions.user_free = &user_free;

	// Create scheduler.
    jobs::scheduler scheduler;    
	scheduler.set_debug_output(debug_output);
    scheduler.set_memory_functions(memory_functions);
    scheduler.set_max_jobs(1024);
	scheduler.add_thread_pool(1, jobs::priority::slow);
	scheduler.add_thread_pool(7, jobs::priority::all_but_slow);
    scheduler.add_fiber_pool(100, 64 * 1024);
    scheduler.add_fiber_pool(1000, 1 * 1024);
    scheduler.add_fiber_pool(10, 2 * 1024 * 1024);

    jobs::result result = scheduler.init();
    assert(result == jobs::result::success);

	// Job 1
	jobs::job_handle job1;
	result = scheduler.create_job(job1);
	assert(result == jobs::result::success);

	job1.set_work([=]() { printf("Final executed\n"); });
	job1.set_stack_size(5 * 1024);
	job1.set_priority(jobs::priority::low);

	for (int i = 0; i < 10; i++)
	{
		// Job 2
		jobs::job_handle job2;
		result = scheduler.create_job(job2);
		assert(result == jobs::result::success);

		job2.set_work([=]() { printf("Sub-job executed\n"); });
		job2.set_stack_size(5 * 1024);
		job2.set_priority(jobs::priority::low);

		job1.add_predecessor(job2);

		job2.dispatch();
	}

	// Dispatch and wait
	job1.dispatch();

	scheduler.wait_until_idle();

	printf("Press any key to exit ...\n");
	getchar();

	return 0;
}
