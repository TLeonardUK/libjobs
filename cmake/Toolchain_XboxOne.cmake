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

# Based vaguely off
# https://github.com/AutodeskGames/stingray-plugin/blob/master/cmake/Toolchain-XBoxOne.cmake

# This module is shared; use include blocker.
if( _XBOX_ONE_TOOLCHAIN_ )
	return()
endif()
set(_XBOX_ONE_TOOLCHAIN_ 1)

# Required XBOX SDK.
set(XDK_TOOLCHAIN_VERSION "180604")

# @todo do this properly
set(XDK_ROOT_DIR "C:/Program Files (x86)/Microsoft Visual Studio/2017/Professional/VC/Tools/MSVC/14.15.26726")
set(XDK_COMPILER_DIR "${XDK_ROOT_DIR}/bin/Hostx64/x64")
set(XDK_INCLUDE_DIR "${XDK_ROOT_DIR}/include")

# Extract XDK path.
if (EXISTS "$ENV{XboxOneXDKLatest}" AND IS_DIRECTORY "$ENV{XboxOneXDKLatest}")
	string(REGEX REPLACE "\\\\" "/" XDK_ROOT $ENV{XboxOneXDKLatest})
	string(REGEX REPLACE "//" "/" XDK_ROOT ${XDK_ROOT})
else()
	message(FATAL_ERROR "XB1 XDK could not be found, please check it is installed.")	
endif()
if (EXISTS "$ENV{XboxOneExtensionSDKLatest}" AND IS_DIRECTORY "$ENV{XboxOneExtensionSDKLatest}")
	string(REGEX REPLACE "\\\\" "/" XDK_EXTENSION_ROOT $ENV{XboxOneExtensionSDKLatest})
	string(REGEX REPLACE "//" "/" XDK_EXTENSION_ROOT ${XDK_EXTENSION_ROOT})
else()
	message(FATAL_ERROR "XB1 Extension XDK could not be found, please check it is installed.")	
endif()

# Set CMake system root search path
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR})
set(MSVC TRUE CACHE BOOL "Visual Studio Compiler" FORCE INTERNAL)
set(CMAKE_CROSSCOMPILING "TRUE")
set(CMAKE_SYSTEM XboxOne)
set(CMAKE_SYSTEM_NAME XboxOne)
set(CMAKE_SYSTEM_PROCESSOR "AMD64")
set(CMAKE_SYSROOT "${XDK_COMPILER_DIR}")

set(CMAKE_C_COMPILER_ID "MSVC")
set(CMAKE_CXX_COMPILER_ID "MSVC")

# Set the compilers to the ones found in XboxOne XDK directory
set(CMAKE_C_COMPILER "${XDK_COMPILER_DIR}/cl.exe")
set(CMAKE_CXX_COMPILER "${XDK_COMPILER_DIR}/cl.exe")
set(CMAKE_ASM_COMPILER "${XDK_COMPILER_DIR}/ml64.exe")

# Force compilers to skip detecting compiler ABI info and compile features
set(CMAKE_C_COMPILER_FORCED True)
set(CMAKE_CXX_COMPILER_FORCED True)
set(CMAKE_ASM_COMPILER_FORCED True)

# Only search the XBoxOne XDK, not the remainder of the host file system
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} /AI\"${XDK_EXTENSION_ROOT}/references/commonconfiguration/neutral\"")

SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} /D WINAPI_FAMILY=WINAPI_FAMILY_TV_TITLE")
SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} /D _UITHREADCTXT_SUPPORT=0")

include_directories(BEFORE SYSTEM ${XDK_INCLUDE_DIR})

set(CMAKE_CXX_COMPILE_OBJECT
    "<CMAKE_CXX_COMPILER>  <DEFINES> <INCLUDES> <FLAGS> /out:<OBJECT> -c <SOURCE>")
set(CMAKE_CXX_LINK_EXECUTABLE
    "<CMAKE_CXX_COMPILER>  <FLAGS> <CMAKE_CXX_LINK_FLAGS> <LINK_FLAGS> <OBJECTS>  /out:<TARGET> <LINK_LIBRARIES>")


