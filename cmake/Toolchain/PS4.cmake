#  libjobs - Simple coroutine based job scheduling.
#  Copyright (C) 2019 Tim Leonard <me@timleonard.uk>
#
#  This software is provided 'as-is', without any express or implied
#  warranty.  In no event will the authors be held liable for any damages
#  arising from the use of this software.
#  
#  Permission is granted to anyone to use this software for any purpose,
#  including commercial applications, and to alter it and redistribute it
#  freely, subject to the following restrictions:
#
#  1. The origin of this software must not be misrepresented; you must not
#     claim that you wrote the original software. If you use this software
#     in a product, an acknowledgment in the product documentation would be
#     appreciated but is not required.
#  2. Altered source versions must be plainly marked as such, and must not be
#     misrepresented as being the original software.
#  3. This notice may not be removed or altered from any source distribution.

# This module is shared; use include blocker.
if( _PS4_TOOLCHAIN_ )
	return()
endif()
set(_PS4_TOOLCHAIN_ 1)

# Required XBOX SDK.
set(PS4_TOOLCHAIN_VERSION "6.000")

# Get PS4 SCE environment
if (EXISTS "$ENV{SCE_ROOT_DIR}" AND IS_DIRECTORY "$ENV{SCE_ROOT_DIR}")
	string(REGEX REPLACE "\\\\" "/" PS4_ROOT $ENV{SCE_ROOT_DIR})
	string(REGEX REPLACE "//" "/" PS4_ROOT ${PS4_ROOT})
	set(PS4_SDK "${PS4_ROOT}/ORBIS SDKs/${PS4_TOOLCHAIN_VERSION}")
	set(SCE_VERSION PS4_TOOLCHAIN_VERSION)
endif()

# We are building PS4 platform, fail if PS4 SCE not found
if (NOT PS4_ROOT OR NOT PS4_SDK OR NOT IS_DIRECTORY "${PS4_SDK}")
	message(FATAL_ERROR "PS4 SDK could not be found, please check it is installed.")	
endif()

# Set CMake system root search path
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR})
set(CMAKE_CROSSCOMPILING "TRUE")
set(CMAKE_SYSTEM PS4)
set(CMAKE_SYSTEM_NAME PS4)
set(CMAKE_SYSTEM_PROCESSOR "AMD64")
set(CMAKE_SYSROOT "${PS4_SDK}")
set(MSVC 1)

# Set the compilers to the ones found in XboxOne XDK directory
set(CMAKE_C_COMPILER "${PS4_SDK}/host_tools/bin/orbis-clang.exe")
set(CMAKE_CXX_COMPILER "${PS4_SDK}/host_tools/bin/orbis-clang++.exe")
set(CMAKE_ASM_COMPILER "${PS4_SDK}/host_tools/bin/orbis-as.exe")

# Only search the XBoxOne XDK, not the remainder of the host file system
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
