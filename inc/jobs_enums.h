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

#include <stdint.h>
#include <chrono>

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
	out_of_objects,			/**< Could not allocate a free object instance to fulfil request. */  
    maximum_exceeded,       /**< Maximum number of resources that can be registered/added has been exceeded. */  
    already_set,            /**< A value has already been set and cannot be set multiple times. */  
    already_initialized,    /**< Operation could not be performed as the object has already been initialized. */  
    no_thread_pools,        /**< Scheduler attempted to be initialized with no thread pools defined. */  
    no_fiber_pools,         /**< Scheduler attempted to be initialized with no fiber pools defined. */  
	platform_error,			/**< A internal platform function call failed for unspecified/unknown reasons. */
	invalid_handle,			/**< The handle the operation was performed on was invalid, either it points to a disposed object, or was never initialized. */
	already_dispatched,		/**< Job has already been dispatched and cannot be again until complete. */
	not_mutable,			/**< Object is in a state where it is not currently mutable (dispatch/in-progress). */
	timeout,				/**< Operation timed out before completion. */
	not_in_job,				/**< Attempt to execution a function that can only be run under a jobs context. */
};

/**
 *  \brief Priority of a job.
 *
 *  Defines how urgent a job is. The job scheduler will always 
 *  attempt to execute higher priorities first.
 *
 *  Should be ordered most to least priority.
 */
enum class priority
{
	critical	= 1 << 0,			/**< Critical priority jobs */
	high		= 1 << 1,			/**< High priority jobs */
	medium		= 1 << 2,			/**< Medium priority jobs */
	low			= 1 << 3,			/**< Low priority jobs */
	slow        = 1 << 4,			/**< Very slow and long running jobs should be assigned this priority, it allows easy segregation to prevent saturating thread pools. */
    
	count		 = 5,		

    all          = 0xFFFF,			/**< All priorities together. */  
    all_but_slow = 0xFFFF & ~slow , /**< All priorities together except slow. */  
};

/**
 *  \brief Verbosity of a debug output message.
 */
enum class debug_log_verbosity
{
	error,		/**< A potentially critical error has occured. */  
	warning,	/**< A recoverable, but potentially unwanted problem occured. */  
	message,	/**< General logging message, describing progress */  
	verbose,	/**< Very verbose debugging information */  

	count
};

/**
 *  \brief Semantic group a log message belongs to.
 */
enum class debug_log_group
{
	worker,		/**< Regarding the management of worker threads/fibers. */
	scheduler,	/**< Regarding job scheduling */
	memory,		/**< Regarding memroy management */
	job,		/**< Regarding job management */

	count
};

/**
 *  \brief Defines a context-specific type of a profiling scope.
 */
enum class profile_scope_type
{
	worker,			/**< The scope encapsulates scheduling-level work happening outside a fiber. */
	fiber,			/**< The scope encapsulates job work happening inside a fiber. */
	user_defined,	/**< The scope was defined by the user. */
};

namespace internal {

	/** String representation of values in enum debug_log_verbosity. */
	extern const char* debug_log_verbosity_strings[(int)debug_log_verbosity::count];

	/** String representation of values in enum debug_log_group. */
	extern const char* debug_log_group_strings[(int)debug_log_group::count];

}; /* namespace internal */

}; /* namespace jobs */

#endif /* __JOBS_ENUM_H__ */