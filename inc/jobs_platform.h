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
 *  \file jobs_platform.h
 *
 *  Include header for platform specific defines.
 */

#ifndef __JOBS_PLATFORM_H__
#define __JOBS_PLATFORM_H__

#if defined(__ORBIS__) 
#	define JOBS_PLATFORM_PS4 1
#elif defined(XBOX) || defined(_XBOX_ONE) || defined(_DURANGO)
#	define JOBS_PLATFORM_XBOX_ONE 1
#elif defined(_WIN32)
#	define JOBS_PLATFORM_WINDOWS 1
#else
#	error Unknown or unimplemented platform
#endif

#ifdef JOBS_PLATFORM_WINDOWS
#	define WIN32_LEAN_AND_MEAN  1
#	define VC_EXTRALEAN 1
#	include <Windows.h>
#endif

#endif /* __JOBS_MEMORY_H__ */