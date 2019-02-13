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

 /** @todo */
#if defined(__ORBIS__)
#   define JOBS_PLATFORM_PS4
#elif defined(_DURANGO)
#   define JOBS_PLATFORM_XBOX_ONE
#elif defined(NN_NINTENDO_SDK)
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

#if defined(JOBS_PLATFORM_WINDOWS) || defined(JOBS_PLATFORM_XBOX_ONE)
#	define WIN32_LEAN_AND_MEAN  1
#	define VC_EXTRALEAN 1
#	include <Windows.h>
#endif

#if defined(JOBS_PLATFORM_SWITCH)
#   include <nn/os.h>
#   include <nn/nn_Log.h>
#endif

 /** @todo */
#if defined(NDEBUG)
#   define JOBS_RELEASE_BUILD 
#else
#   define JOBS_DEBUG_BUILD 
#endif

 /** @todo */
#if defined(JOBS_PLATFORM_WINDOWS) ||  defined(JOBS_PLATFORM_XBOX_ONE)
#   define JOBS_FORCE_INLINE __forceinline 
#   define JOBS_FORCE_NO_INLINE __declspec(noinline)
#elif defined(JOBS_PLATFORM_PS4) || \
      defined(JOBS_PLATFORM_SWITCH)
#   define JOBS_FORCE_INLINE __attribute__((always_inline)) 
#   define JOBS_FORCE_NO_INLINE __attribute__((noinline))
#else
#   define JOBS_FORCE_INLINE 
#   define JOBS_FORCE_NO_INLINE 
#endif

 /** @todo */
#if 1// defined(JOBS_PLATFORM_WINDOWS)
#   if defined(JOBS_PLATFORM_SWITCH)
//#       define JOBS_YIELD() __asm__ __volatile__ ("yield")
#       define JOBS_YIELD() nn::os::YieldThread()
#   else
#       define JOBS_YIELD() _mm_pause()
#   endif
#else
    // This provides no benefit on consoles where we have hard affinity.
#   define JOBS_YIELD()
#endif

 /** @todo */
#if defined(JOBS_DEBUG_BUILD)
#   define JOBS_USE_PROFILE_MARKERS 
#endif

/** @todo */
#if 1//defined(JOBS_DEBUG_BUILD)
#   define JOBS_USE_VERBOSE_LOGGING
#endif

 /** @todo */
#define JOBS_PRINTF(...) jobs::internal::debug_print(__VA_ARGS__)

#endif // __JOBS_DEFINES_H__