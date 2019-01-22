#include "jobs.h"

#include <cassert>

void* user_alloc(size_t size)
{
    void* ptr = malloc(size);
    printf("User Allocated: %zi (%p)\n", size, ptr);
    return ptr;
}

void user_free(void* ptr)
{
    free(ptr);
    printf("User Freed: %p\n", ptr);
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
    scheduler.set_memory_functions(memory_functions);
    scheduler.set_max_jobs(1024);
	scheduler.add_thread_pool(1, jobs::priority::slow);
	scheduler.add_thread_pool(7, jobs::priority::all_but_slow);
    scheduler.add_fiber_pool(100, 64 * 1024);
    scheduler.add_fiber_pool(1000, 1 * 1024);
    scheduler.add_fiber_pool(10, 2 * 1024 * 1024);

    jobs::result result = scheduler.init();
    assert(result == jobs::result::success);


	// Option 1
	jobs::job job;
	job.set_work([=]() { printf("Work Executed!\n"); });
	job.set_stack_size(5 * 1024);
	job.set_priority(jobs::priority::low);
	job.add_dependency(job_handle_2);

	jobs::job_handle handle = scheduler.queue_job(job);
	handle.wait();
	handle.get_status();

	// Option 2
	jobs::job* job = nullptr;

	result = scheduler.create_job(job);
	assert(result == jobs::result::success);

	job->set_stack_size(5 * 1024);
	job->set_priority(jobs::priority::low);
	//job.add_dependency(first_job);

	scheduler.queue_job(job);

	//job.get_status();
	//job.wait();

	printf("Press any key to exit ...\n");
	getchar();

	return 0;
}
