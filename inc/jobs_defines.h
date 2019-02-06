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
 *  \file jobs_defines.h
 *
 *  Main include header that includes compile defines. Should be included in all files.
 */

#ifndef __JOBS_DEFINES_H__
#define __JOBS_DEFINES_H__

#if defined(__ORBIS__)
#   define JOBS_PLATFORM_PS4
#elif defined(_DURANGO)
#   define JOBS_PLATFORM_XBOXONE
#elif defined(NN_BUILD_TARGET_PLATFORM_NX)
#   define JOBS_PLATFORM_SWITCH
#elif defined(_WIN32)
#   define JOBS_PLATFORM_WINDOWS
#endif

#if !defined(JOBS_PLATFORM_PS4) && \
    !defined(JOBS_PLATFORM_XBOX_ONE) && \
    !defined(JOBS_PLATFORM_SWITCH) && \
    !defined(JOBS_PLATFORM_WINDOWS) 
#	error Unknown or unimplemented platform
#endif

#if defined(JOBS_PLATFORM_WINDOWS)
#	define WIN32_LEAN_AND_MEAN  1
#	define VC_EXTRALEAN 1
#	include <Windows.h>
#endif

#endif // __JOBS_DEFINES_H__