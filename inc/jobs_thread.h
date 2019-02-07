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

#include "jobs_defines.h"
#include "jobs_enums.h"
#include "jobs_memory.h"

#include <thread>
#include <functional>

namespace jobs {
namespace internal {

class thread;
    
/**
 *  \brief Entry point for a thread.
 */
typedef std::function<void()> thread_entry_point;

/**
 *  Encapsulates a thread of execution on the base platform.
 */
class thread
{
public:

    /**
     * Constructor.
     *
     * \param memory_functions Scheduler defined memory functions used to 
     *						   override default memory allocation behaviour.
     */
    thread(const memory_functions& memory_functions);
    
    /** Destructor. */
    ~thread();
    
    /**
     * \brief Initializes this thread of execution and begins running the thread.
     *
     * \param entry_point Function that should be run when the thread starts.
     * \param name Contextual name of this thread to show in debugger.
     * \param core_affinity Mask of which cores this thread can execute on.
     *
     * \return Value indicating the success of this function.
     */
    result init(const thread_entry_point& entry_point, const char* name, size_t core_affinity);

    /**
     * \brief Blocks until thread completes execution.
     *
     * \return Value indicating the success of this function.
     */
    void join();

private:

    /** Memory allocation functions provided by the scheduler. */
    memory_functions m_memory_functions;

    /** Thread of execution we are encapsulating */
    std::thread m_thread;

};

}; /* namespace internal */
}; /* namespace jobs */

#endif /* __JOBS_THREAD_H__ */