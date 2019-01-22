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
    jobs::scheduler scheduler;    
    //scheduler.set_memory_functions(&user_alloc, &user_free);
    scheduler.set_max_jobs(1024);
    scheduler.add_thread_pool(8, jobs::priority::all);
    scheduler.add_fiber_pool(200, 64 * 1024);
    scheduler.add_fiber_pool(1000, 1 * 1024);
    scheduler.add_fiber_pool(10, 2 * 1024 * 1024);

    jobs::result result = scheduler.init();
    assert(result == jobs::result::success);


    /*
    scheduler.set_max_jobs(1024);
    scheduler.add_thread_pool(2, jobs::priority::high);
    scheduler.add_thread_pool(6, jobs::priority::any);
    scheduler.add_fiber_pool(128, 128 * 1024);
    scheduler.add_fiber_pool(16, 2 * 1024 * 1024);

    if (scheduler.init() == jobs::result::success)
    {
        jobs::job job = scheduler.create_job();
        job.set_stack_size(5 * 1024);
        job.set_priority(jobs::priority::low);
        job.add_dependency(first_job);

        scheduler.queue_job(job);
        // fire_and_forget_job
        
        job.get_status();
        job.wait();
    }
    else
    {
        printf("Failed to create scheduler.\n");
    }
    */

	return 0;
}
