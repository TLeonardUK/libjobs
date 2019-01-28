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
 *  \file jobs_utils.h
 *
 *  Include header for general library utilities.
 */

#ifndef __JOBS_UTILS_H__
#define __JOBS_UTILS_H__

#include <stdint.h>
#include <chrono>

namespace jobs {
namespace internal {

/**
 *  \brief Utility class used to time the duration between two points in code.
 */
struct stopwatch
{
public:

	/** @todo */
	void start();

	/** @todo */
	void stop();

	/** @todo */
	uint64_t get_elapsed_ms();

private:

	/** @todo */
	std::chrono::high_resolution_clock::time_point m_start_time;

	/** @todo */
	std::chrono::high_resolution_clock::time_point m_end_time;

	/** @todo */
	bool m_has_end = false;

};

}; /* namespace internal */

/**
 *  \brief Represents a period of time used as a timeout for a blocking function.
 */
struct timeout
{
public:

	/** Duration of this timeout in milliseconds. */
	uint64_t duration;

	/**
	 *  \brief Default constructor
	 */
	timeout()
		: duration(0)
	{
	}

	/**
	 *  \brief Constructor
	 *
	 *  \param inDuration Duration in milliseconds of this timeout.
	 */
	timeout(uint64_t inDuration)
		: duration(inDuration)
	{
	}

	/**
	 *  \brief Returns true if this timeout is infinite.
	 *
	 *  \return true if an infinite timeout.
	 */
	bool is_infinite()
	{
		return duration == infinite.duration;
	}

	/** Represents an infinite, non-ending timeout. */
	static const timeout infinite;

};

}; /* namespace jobs */

#endif /* __JOBS_ENUM_H__ */