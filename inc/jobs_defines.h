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

/** Defines the platform we are compiling for. */
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

/** Defines the build flavour we are compiling for. */
#if defined(NDEBUG)
#   define JOBS_RELEASE_BUILD 
#else
#   define JOBS_DEBUG_BUILD 
#endif

/** Various compiler specific flags macro's. */
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

/** Yields the thread, primarily used inside busy-waits. */
#if defined(JOBS_PLATFORM_SWITCH)
#   define JOBS_YIELD() nn::os::YieldThread()
#else
#   define JOBS_YIELD() _mm_pause()
#endif

/** If true the jobs library will emit profile markers for various internal operations. */
#if defined(JOBS_DEBUG_BUILD)
#   define JOBS_USE_PROFILE_MARKERS 
#endif

/** If true the jobs library will emit verbose logging messages. */
#if defined(JOBS_DEBUG_BUILD)
#   define JOBS_USE_VERBOSE_LOGGING
#endif

/** Utility function to perform debug-output across all platforms. */
#define JOBS_PRINTF(...) jobs::internal::debug_print(__VA_ARGS__)

/** Macro used to initialize an anonymous RAII profile marker. */
#if defined(JOBS_USE_PROFILE_MARKERS)
#define jobs_profile_scope(...) ::jobs::internal::profile_scope_internal _profile_scope__##__LINE__(__VA_ARGS__);
#else
#define jobs_profile_scope(...)
#endif

#endif // __JOBS_DEFINES_H__