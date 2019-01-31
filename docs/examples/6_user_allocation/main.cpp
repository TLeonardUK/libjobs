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

// This example shows how to register a set of user allocation overrides
// so memory management can be controlled.

// Comments on topics previously discussed in other examples have been removed 
// or simplified, go back to older examples if you are unsure of anything.

#include <jobs.h>
#include <cassert>

void debug_output(
    jobs::debug_log_verbosity level, 
    jobs::debug_log_group group, 
    const char* message)
{
    printf("%s", message);
}

// User-defined memory allocation function, in this example this is just a trampoline 
// to malloc, but you could use this for directing memory allocations to your
// own allocators.
void* user_alloc(size_t size)
{
    void* ptr = malloc(size);
    printf("Allocated %zi bytes @ 0x%p\n", size, ptr);
    return ptr;
}

// User-defined memory release function, in this example this is just a trampoline 
// to free, but you could use this for directing memory allocations to your
// own allocators.
void user_free(void* ptr)
{
    printf("Freed 0x%p\n", ptr);
    free(ptr);
}

void example()
{
    jobs::scheduler scheduler;
    scheduler.add_thread_pool(jobs::scheduler::get_logical_core_count(), jobs::priority::all);
    scheduler.add_fiber_pool(10, 16 * 1024);
    scheduler.set_debug_output(debug_output);

    // This struct defines various overrides for the default malloc/free functions that are 
    // used by default to allocate all the resources the scheduler uses.
    // Semantically the overriden versions of these functions should behave the same as 
    // the c standard library ones.
    // These functions will only be called during scheduler initialization, the scheduler
    // never allocates memory past that point.
    jobs::memory_functions memory_functions;
    memory_functions.user_alloc = &user_alloc;
    memory_functions.user_free = &user_free;

    // Assign our overriden functions to the scheduler.
    scheduler.set_memory_functions(memory_functions);

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
        printf("Example job executed\n");
    });

    // Dispatch job.
    job_1.dispatch();

    // Wait for job to complete.
    scheduler.wait_until_idle();

    printf("All jobs completed.\n");

    return;
}

void main()
{
    example();

    printf("All resources freed.\n");

    return;
}