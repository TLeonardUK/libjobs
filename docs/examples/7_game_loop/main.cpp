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

// This example show a way you could implement a game loop using
// a fiber job system to update while implicitly handling 
// dependencies between objects.

// Comments on topics previously discussed in other examples have been removed 
// or simplified, go back to older examples if you are unsure of anything.

#include <jobs.h>
#include <cassert>
#include <vector>
#include <cstdio>

#if defined(USE_PIX)
#include <pix/include/WinPixEventRuntime/pix3.h>
#endif

// Couple of external functions implemented in the framework code, to prevent
// a lot of code duplication between examples.
void framework_enter_scope(jobs::profile_scope_type type, const char* tag);
void framework_leave_scope();

// Holds general information used to process a simulate a frame.
struct frame_info
{
    // Scheduler used for running all our entities.
    jobs::scheduler scheduler;

    // Number of tickables that currently exist.
    std::atomic<size_t> tickable_count{ 0 };

    // Counter used to synchronize tickables with the start of a frame.
    jobs::counter_handle frame_counter;

    // Counter used to synchronize the game loop with all tickables completing
    jobs::counter_handle frame_end_counter;
};

// The tickable is our base class for all things that need to update each frame.
// Each tickable creates a persistent job that manages synchronization with the
// start of a frame, and calling the virtual tick() method in derived classes.
//
// Tickables should be passed to dependent objects as const pointers. When a dependent
// object wants to access it, they should call the sync() method which will return
// a non-const pointer after the tickable has finished it's tick. This implicitly
// manages dependencies, and allows you to always use up-to-date state information without
// having to fall back to typical solutions like double-buffer or frame-delaying state.
// 
// It should be noted that having a persistent job has pros and cons. The main benefit
// is you don't have to waste time rescheduling it each frame, the downside is that
// you cannot recycle fiber context's, so you end up with much larger memory requirements, 
// requiring at least one fiber per tickable at all times.
template <typename super_class>
class tickable
{
public:

    // Initializes the tickable with a given block of frame information.
    void init(frame_info* info, const char* name)
    {
        m_tickable_index = info->tickable_count++;
        m_frame_info = info;

        // Creates a new counter that stores the last frame index this tickable
        // has completed a tick for. This is used by the sync() method to ensure
        // completion of the current frame before returning.
        jobs::result result = info->scheduler.create_counter(m_last_tick_frame);
        assert(result == jobs::result::success);

        // Update last tick frame with current frame index.
        size_t current_frame;
        result = info->frame_counter.get(current_frame);
        assert(result == jobs::result::success);

        result = m_last_tick_frame.set(current_frame);
        assert(result == jobs::result::success);

        // Allocate job to run this objects tick.
        result = info->scheduler.create_job(m_tick_job);
        assert(result == jobs::result::success);

        // Setup our tickables job, which just calls the tick_loop() method.
        m_tick_job.set_tag(name);
        m_tick_job.set_stack_size(16 * 1024);
        m_tick_job.set_priority(jobs::priority::low);
        m_tick_job.set_work([=]() { tick_loop(); });

        // Start up the tick task.
        m_tick_job.dispatch();
    }

    // Will return a non-const pointer after the tickable has finished it's tick. 
    // This allows implicit managagement of dependencies.
    super_class* sync() const
    {
        // Allows us to pass a const tickable everywhere and force
        // any entities that interactive with it to perform a sync()
        // if they won't to perform anything that causes modifications.
        const super_class* super_non_const_ptr = static_cast<const super_class*>(this);
        super_class* super_ptr = const_cast<super_class*>(super_non_const_ptr);

        // Wait until we have finished this tickables tick for this frame.
        size_t current_frame;
        super_ptr->m_frame_info->frame_counter.get(current_frame);
        super_ptr->m_last_tick_frame.wait_for(current_frame);

        return super_ptr;
    }

protected:

    // Implemented in derived classes to implement actual functionality.
    virtual void tick() = 0;

private:

    // Core loop of the tickables job. 
    void tick_loop()
    {
        while (true)
        {
            // Wait for the next frame to start processing.
            {
                jobs::profile_scope scope(jobs::profile_scope_type::user_defined, "wait for frame");

                size_t frame = 0;
                jobs::result result = m_last_tick_frame.get(frame);
                assert(result == jobs::result::success);

                m_frame_info->frame_counter.wait_for(frame + 1);
            }

            // Perform any processing required.
            {
                jobs::profile_scope scope(jobs::profile_scope_type::user_defined, "tick");

                tick();
            }

            // Mark as complete for this frame.
            {
                jobs::profile_scope scope(jobs::profile_scope_type::user_defined, "mark complete");

                m_frame_info->frame_end_counter.add(1);
                m_last_tick_frame.add(1);
            }
        }
    }

protected:

    // A unique index of this tickable, used for debug logging.
    size_t m_tickable_index;

private:

    // General info required to simulate a frame.
    frame_info* m_frame_info = nullptr;

    // Handle to tickables job.
    jobs::job_handle m_tick_job;

    // Counter holding the index of the last frame a full tick was performed for.
    jobs::counter_handle m_last_tick_frame;

};

// Demonstration class of what you could implement as a tickable. In a proper 
// implementation this could be responsible for determining and responding to
// collisions between different entities.
class physics_system : public tickable<physics_system>
{
public:
    void init(frame_info* info)
    {
        tickable::init(info, "physics_system");
    }

protected:
    virtual void tick()
    {
        // Here you would do some collision checking between entities.

        //printf("physics_system[%zi]: ticked\n", m_tickable_index);
    }

};

// Demonstration class of a basic entity. In a proper implementation this would
// represent an object in your game-world, which would actually do something :).
class entity : public tickable<entity>
{
public:
    void init(frame_info* info, const physics_system* physics, std::vector<const entity*> dependencies)
    {
        tickable::init(info, "entity");
        m_physics = physics;
        m_dependencies = dependencies;
    }

protected:
    virtual void tick()
    {
        // Demonstrates get a synchronized physics system which has finished its tick. This ensures
        // that the entity would be able to access a stable physics information this frame without
        // ordering issues.
        //physics_system* physics = m_physics->sync();

        // Sync to our dependent entities if we have any
        {
            jobs::profile_scope(jobs::profile_scope_type::user_defined, "sync block");

            for (auto& entity : m_dependencies)
            {
                jobs::profile_scope(jobs::profile_scope_type::user_defined, "sync");
                //entity->sync();
            }
        }

        // Here you would perform typical entity processing, checking for collision, moving, etc.
        //printf("entity[%zi]: ticked\n", m_tickable_index);
    }

private:

    // Pointer to our physics system which we will sync to access.
    const physics_system* m_physics;

    // Pointer to a random dependent entity which we will sync to access.
    std::vector<const entity*> m_dependencies;

};

void jobsMain()
{
    const size_t entity_count = 100;

    // The frame-info struct contains all our general information used for scheduling
    // jobs and synchronising frames.
    frame_info info;

    // Setup profiling functions so we can examine execution in pix.
    jobs::profile_functions profile_functions;
    profile_functions.enter_scope = framework_enter_scope;
    profile_functions.leave_scope = framework_leave_scope;

    // Setup the job scheduler.
    info.scheduler.add_thread_pool(jobs::scheduler::get_logical_core_count(), jobs::priority::all);
    info.scheduler.set_max_callbacks(entity_count * 2);
    info.scheduler.set_max_counters(entity_count * 2);
    info.scheduler.set_max_dependencies(entity_count * 2);
    info.scheduler.set_max_jobs(entity_count * 2);
    info.scheduler.set_max_profile_scopes(entity_count * 20);
    info.scheduler.add_fiber_pool(entity_count * 2, 16 * 1024);
    info.scheduler.set_profile_functions(profile_functions);
    info.scheduler.set_debug_output([](jobs::debug_log_verbosity level, jobs::debug_log_group group, const char* message)
    {
        printf("%s", message);
    });

    // Initializes the scheduler.
    jobs::result result = info.scheduler.init();
    assert(result == jobs::result::success);

    // Create a counter that increments each frame so tickables can sync to the start each frame.
    result = info.scheduler.create_counter(info.frame_counter);
    assert(result == jobs::result::success);

    // Create a counter to determine when all tickables have finished a frame.
    result = info.scheduler.create_counter(info.frame_end_counter);
    assert(result == jobs::result::success);

    // Create a dummy physics system tickable.
    physics_system physics;
    physics.init(&info);

    // Create a handful of dummy entities.
    entity entities[entity_count];
    std::vector<const entity*> dependencies;
    for (int i = 0; i < entity_count/2; i++)
    {
        entities[i].init(&info, &physics, { });
        dependencies.push_back(&entities[i]);
    }

    // Create a handful of entities which will sync to the first lot.
    for (int i = entity_count/2; i < entity_count; i++)
    {
        entities[i].init(&info, &physics, { }); //&entities[i - (entity_count/2)] });
    }

    // Main loop.
    double frame_duration_sum = 0;
    int frame_count = 0;
    while (true)
    {
        jobs::internal::stopwatch timer;
        timer.start();

        // Log out the current frame index.
        size_t frame_index;
        info.frame_counter.get(frame_index);

        // Incrementing the frame counter kicks off all tickables to run their next simulation step.
        info.frame_counter.add(1);

        // Each tickable adds one to the counter, so this blocks until every tickable has finished for the frame.
        info.frame_end_counter.remove(info.tickable_count);

        timer.stop();
        frame_duration_sum += timer.get_elapsed_us() / 1000.0;

        if (++frame_count == 100)
        { 
            printf("Frame Average (over 100): %.4f ms\n", frame_duration_sum / frame_count);
            frame_duration_sum = 0.0f;
            frame_count = 0;
        }

        // Pause between frames so we can actually read the output :).
        jobs::scheduler::sleep(10);
    }

    // Expected execution:
    //  Notice the order in which different things get ticked, namely how ordering is implicitly dealt
    //  with by using job syncronization.
}