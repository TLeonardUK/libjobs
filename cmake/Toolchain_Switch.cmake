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
if( _SWITCH_TOOLCHAIN_ )
	return()
endif()
set(_SWITCH_TOOLCHAIN_ 1)

# Get environment
set(SWITCH_ROOT "" CACHE STRING "")
if (EXISTS "$ENV{NINTENDO_SDK_ROOT}" AND IS_DIRECTORY "$ENV{NINTENDO_SDK_ROOT}")
	string(REGEX REPLACE "\\\\" "/" SWITCH_ROOT $ENV{NINTENDO_SDK_ROOT})
endif()

# We are building switch platform, fail if switch sdk not found
if (NOT SWITCH_ROOT OR NOT IS_DIRECTORY "${SWITCH_ROOT}")
	message(FATAL_ERROR "Switch SDK could not be found, please check it is installed.")	
endif()

# Set CMake system root search path
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR})
set(CMAKE_SYSTEM_NAME Switch)
set(CMAKE_SYSROOT "${SWITCH_ROOT}")
set(MSVC 1)

# Set the compilers to the ones found in XboxOne XDK directory
set(CMAKE_C_COMPILER "${SWITCH_ROOT}/Compilers/NX/nx/aarch64/bin/clang.exe")
set(CMAKE_CXX_COMPILER "${SWITCH_ROOT}/Compilers/NX/nx/aarch64/bin/clang++.exe")
set(CMAKE_AR "${SWITCH_ROOT}/Compilers/NX/nx/aarch64/bin/aarch64-nintendo-nx-elf-ar.exe" CACHE FILEPATH "Archiver")
#set(CMAKE_ASM_COMPILER "${SWITCH_ROOT}/host_tools/bin/orbis-as.exe")

# Only search the XBoxOne XDK, not the remainder of the host file system
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

SET(SWITCH_INIT_FLAGS  "" CACHE STRING "")
SET(SWITCH_INIT_FLAGS  "${SWITCH_INIT_FLAGS} -nostartfiles")
SET(SWITCH_INIT_FLAGS  "${SWITCH_INIT_FLAGS} -Wl,--gc-sections")
SET(SWITCH_INIT_FLAGS  "${SWITCH_INIT_FLAGS} -Wl,-init=_init,-fini=_fini")
SET(SWITCH_INIT_FLAGS  "${SWITCH_INIT_FLAGS} -Wl,-pie")
SET(SWITCH_INIT_FLAGS  "${SWITCH_INIT_FLAGS} -Wl,-z,combreloc,-z,relro,--enable-new-dtags")

SET(SWITCH_INIT_LIB_DIRS  "" CACHE STRING "")
SET(SWITCH_INIT_LIB_DIRS  "${SWITCH_INIT_LIB_DIRS} -L${SWITCH_ROOT}/Libraries/NX-NXFP2-a64/Debug")
SET(SWITCH_INIT_LIB_DIRS  "${SWITCH_INIT_LIB_DIRS} -L${SWITCH_ROOT}/Libraries/NX-NXFP2-a64/Develop")

SET(SWITCH_START_OBJECTS  "" CACHE STRING "")
SET(SWITCH_START_OBJECTS  "${SWITCH_START_OBJECTS} ${SWITCH_ROOT}/Libraries/NX-NXFP2-a64/Develop/rocrt.o")
SET(SWITCH_START_OBJECTS  "${SWITCH_START_OBJECTS} ${SWITCH_ROOT}/Libraries/NX-NXFP2-a64/Develop/nnApplication.o")
 
SET(SWITCH_END_OBJECTS  "" CACHE STRING "")
SET(SWITCH_END_OBJECTS  "${SWITCH_END_OBJECTS} ${SWITCH_ROOT}/Libraries/NX-NXFP2-a64/Develop/libnn_init_memory.a")
SET(SWITCH_END_OBJECTS  "${SWITCH_END_OBJECTS} ${SWITCH_ROOT}/Libraries/NX-NXFP2-a64/Develop/libnn_gfx.a")
SET(SWITCH_END_OBJECTS  "${SWITCH_END_OBJECTS} ${SWITCH_ROOT}/Libraries/NX-NXFP2-a64/Develop/libnn_mii_draw.a")

SET(SWITCH_POST_LINK_FLAGS  "" CACHE STRING "")
SET(SWITCH_POST_LINK_FLAGS  "${SWITCH_POST_LINK_FLAGS} -Wl,--build-id=md5")
SET(SWITCH_POST_LINK_FLAGS  "${SWITCH_POST_LINK_FLAGS} -Wl,-T ${SWITCH_ROOT}/Resources/SpecFiles/Application.aarch64.lp64.ldscript")
SET(SWITCH_POST_LINK_FLAGS  "${SWITCH_POST_LINK_FLAGS} -u,malloc")
SET(SWITCH_POST_LINK_FLAGS  "${SWITCH_POST_LINK_FLAGS} -u,calloc")
SET(SWITCH_POST_LINK_FLAGS  "${SWITCH_POST_LINK_FLAGS} -u,realloc")
SET(SWITCH_POST_LINK_FLAGS  "${SWITCH_POST_LINK_FLAGS} -u,aligned_alloc")
SET(SWITCH_POST_LINK_FLAGS  "${SWITCH_POST_LINK_FLAGS} -u,free")
SET(SWITCH_POST_LINK_FLAGS  "${SWITCH_POST_LINK_FLAGS} -Wl,--start-group")
SET(SWITCH_POST_LINK_FLAGS  "${SWITCH_POST_LINK_FLAGS} ${SWITCH_ROOT}/Libraries/NX-NXFP2-a64/Develop/nnSdkEn.nss")
SET(SWITCH_POST_LINK_FLAGS  "${SWITCH_POST_LINK_FLAGS} -Wl,--end-group")
SET(SWITCH_POST_LINK_FLAGS  "${SWITCH_POST_LINK_FLAGS} ${SWITCH_ROOT}/Libraries/NX-NXFP2-a64/Develop/crtend.o")
SET(SWITCH_POST_LINK_FLAGS  "${SWITCH_POST_LINK_FLAGS} -fuse-ld=gold.exe")
SET(SWITCH_POST_LINK_FLAGS  "${SWITCH_POST_LINK_FLAGS} -Wl,--dynamic-list=${SWITCH_ROOT}/Resources/SpecFiles/ExportDynamicSymbol.lst")
SET(SWITCH_POST_LINK_FLAGS  "${SWITCH_POST_LINK_FLAGS} -fdiagnostics-format=msvc")

set(CMAKE_CXX_LINK_EXECUTABLE
    "<CMAKE_CXX_COMPILER> <FLAGS> <CMAKE_CXX_LINK_FLAGS> <LINK_FLAGS> ${SWITCH_INIT_FLAGS} ${SWITCH_INIT_LIB_DIRS} -Wl,--start-group ${SWITCH_START_OBJECTS} <OBJECTS> ${SWITCH_END_OBJECTS} -Wl,--end-group ${SWITCH_POST_LINK_FLAGS} -o <TARGET> <LINK_LIBRARIES>")
