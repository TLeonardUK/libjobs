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
 *  \file jobs_memory.h
 *
 *  Include header for memory functionality.
 */

#ifndef __JOBS_MEMORY_H__
#define __JOBS_MEMORY_H__

#include "jobs_defines.h"
#include "jobs_enums.h"

#include <functional>
#include <mutex>

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
typedef std::function<void*(size_t size, size_t alignment)> memory_alloc_function;

/**
 *  \brief User-defined memory deallocation function.
 *
 *  Function prototype that can be passed into a job scheduler through
 *  set_memory_functions to override the default free memory deallocator.
 *
 *  \param ptr Pointer to memory to be deallocated.
 */
typedef std::function<void(void* ptr)> memory_free_function;

/**
 *  \brief Holds all overrided functions used for managing memory.
 */
struct memory_functions
{
    /** Function to use for allocation of memory. */
    memory_alloc_function user_alloc = nullptr;

    /** Function to use for deallocation of memory. */
    memory_free_function user_free = nullptr;
};

}; /* namespace jobs */

#endif /* __JOBS_MEMORY_H__ */