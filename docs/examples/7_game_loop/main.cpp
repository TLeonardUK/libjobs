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
// a fiber job system to handle the updating and dependencies between objects.

// Comments on topics previously discussed in other examples have been removed 
// or simplified, go back to older examples if you are unsure of anything.

#include <jobs.h>
#include <cassert>

void debug_output(
    jobs::debug_log_verbosity level, 
    jobs::debug_log_group group, 
    const char* message)
{
    if (level == jobs::debug_log_verbosity::verbose)
    {
        return;
    }

    printf("%s", message);
}

template <typename base_class>
class tickable
{
public:
    void init(jobs::scheduler& scheduler)
    {
    }

    base_class* sync() const
    {
        own_job_handle.wait();

        // Allows us to pass a const tickable everywhere and force
        // any entities that interactive with it to perform a sync()
        // if they won't to perform anything that causes modifications.
        return const_cast<base_class*>(this);
    }

protected:
    virtual void tick() = 0;

private:
    void tick_loop()
    {
        while (!destroyed)
        {
            frame_start_event.wait();
            tick();
        }
    }

};

class physics_system : public tickable<physics_system>
{
public:
    void init(jobs::scheduler& scheduler)
    {
        tickable::init(scheduler);
    }

    size_t register_aabb(float x, float y, float width, float height)
    {
    }

    size_t unregister_aabb()
    {
    }

    void update_aabb(size_t position)
    {
    }

    bool is_colliding()
    {
    }

protected:
    virtual void tick()
    {
        // Check for collisions between entities.
    }

};

class entity : public tickable<entity>
{
public:
    void init(jobs::scheduler& scheduler, const physics_system* physics)
    {
        tickable::init(scheduler);
    }

protected:
    virtual void tick()
    {
        // move about at random.
    }

private:
    const physics_system* m_physics;

};

void main()
{
    jobs::scheduler scheduler;
    scheduler.add_thread_pool(jobs::scheduler::get_logical_core_count(), jobs::priority::all);
    scheduler.add_fiber_pool(100, 16 * 1024);
    scheduler.set_debug_output(debug_output);

    // Initializes the scheduler.
    jobs::result result = scheduler.init();
    assert(result == jobs::result::success);

    // Create an event thats fired at the start of each frame.
    jobs::event_handle frame_start_event;
    result = scheduler.create_event(frame_start_event, true);
    assert(result == jobs::result::success);

    // Create a physics system which will run in a fiber.
    physics_system physics;
    physics.init(scheduler);

    // Create a handful of entities that will just move around the screen at random.
    entity entities[100];
    for (int i = 0; i < 100; i++)
    {
        entities.init(scheduler, &physics);
    }

    // Main loop.
    while (true)
    {
        // Wait for everything to be waiting for frame-start.
        scheduler.wait_until_idle();

        // Wake everything up.
        frame_start_event.signal();
    }

    printf("All jobs completed.\n");

    return;
}