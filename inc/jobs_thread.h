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

/**
 *  \file jobs_thread.h
 *
 *  Include header for thread management functionality.
 */

#ifndef __JOBS_THREAD_H__
#define __JOBS_THREAD_H__

#include <thread>

namespace jobs {
    
/**
 *  \brief User-defined memory allocation function.
 *
 *  Function prototype that can be passed into a job scheduler through
 *  set_memory_functions to override the default malloc memory allocator.
 *
 *  \param size Size of block of memory to be allocated.
 *  \return Pointer to block of memory that was allocated, or nullptr on failure.
 */
typedef std::function<void(const thread& this_thread, void* meta_data)> thread_entry_point;

/**
 *  Encapsulates a thread of execution on the base platform.
 */
class thread
{
public:

    thread(const thread_entry_point& entry_point, void* meta_data);

    void start();
    void join();

private:
    std::thread m_thread;

};

}; /* namespace Jobs */

#endif /* __JOBS_THREAD_H__ */