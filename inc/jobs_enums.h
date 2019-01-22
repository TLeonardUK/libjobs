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
 *  \file jobs_enums.h
 *
 *  Include header for general library enums.
 */

#ifndef __JOBS_ENUM_H__
#define __JOBS_ENUM_H__

namespace jobs {

/**
 *  \brief Result of an operation.
 *
 *  Result that can be returned from various functions describing 
 *  the specific success or failure of the operation.
 */
enum class result
{
    success,                /**< Request completed successfully. */  
    out_of_memory,          /**< Could not allocate enough memory to fulfil request. */  
    out_of_jobs,            /**< Could not allocate a free job instance to fulfil request. */  
    out_of_fibers,          /**< Could not allocate a free fiber instance to fulfil request. */  
    maximum_exceeded,       /**< Maximum number of resources that can be registered/added has been exceeded. */  
    already_set,            /**< A value has already been set and cannot be set multiple times. */  
    already_initialized,    /**< Operation could not be performed as the object has already been initialized. */  
    no_thread_pools,        /**< Scheduler attempted to be initialized with no thread pools defined. */  
    no_fiber_pools,         /**< Scheduler attempted to be initialized with no fiber pools defined. */  
};

/**
 *  \brief Priority of a job.
 *
 *  Defines how urgent a job is. The job scheduler will always 
 *  attempt to execute higher priorities first.
 */
enum class priority
{
    slow        = 1 << 0,   /**< Very slow and long running jobs should be assigned this priority, it allows easy segregation to prevent saturating thread pools. */  
    low         = 1 << 1,   /**< Low priority jobs */  
    medium      = 1 << 2,   /**< Medium priority jobs */  
    high        = 1 << 3,   /**< High priority jobs */  
    critical    = 1 << 4,   /**< Critical priority jobs */  
    all         = 0xFFFF,   /**< All priorities together. */  
};

}; /* namespace Jobs */

#endif /* __JOBS_ENUM_H__ */