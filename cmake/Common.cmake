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

# Figure out architecture
if("${CMAKE_SIZEOF_VOID_P}" STREQUAL "4")
	set(TARGET_ARCHITECTURE "x86" CACHE INTERNAL "")
else()
	set(TARGET_ARCHITECTURE "x64" CACHE INTERNAL "")
endif()

set(TARGET_SYSTEM "${CMAKE_SYSTEM_NAME}" CACHE INTERNAL "")
string(TOLOWER ${TARGET_SYSTEM} TARGET_SYSTEM)

# Require C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# General settings
set(USE_PIX OFF CACHE INTERNAL "")

# Include toolchain specific config.
if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
	
	# Pix enabled
	add_definitions(-DUSE_PIX)
	add_definitions(-D_CRT_SECURE_NO_WARNINGS)
	set(USE_PIX ON CACHE INTERNAL "")

	# Enable fiber-safe optimizations on msvc build, otherwise it will cache thread_local vars
	# and make various things explode when we switch context's.
	set(CMAKE_C_FLAGS_RELEASE			"${CMAKE_C_FLAGS_RELEASE} /GT")
	set(CMAKE_CXX_FLAGS_RELEASE			"${CMAKE_CXX_FLAGS_RELEASE} /GT")
	set(CMAKE_C_FLAGS_RELWITHDEBINFO	"${CMAKE_C_FLAGS_RELWITHDEBINFO} /GT")
	set(CMAKE_CXX_FLAGS_RELWITHDEBINFO	"${CMAKE_CXX_FLAGS_RELWITHDEBINFO} /GT")
	set(CMAKE_C_FLAGS_DEBUG				"${CMAKE_C_FLAGS_DEBUG} /GT")
	set(CMAKE_CXX_FLAGS_DEBUG			"${CMAKE_CXX_FLAGS_DEBUG} /GT")	

elseif (CMAKE_SYSTEM_NAME STREQUAL "XboxOne")

	# Pix enabled
	add_definitions(-DUSE_PIX)
	add_definitions(-D_CRT_SECURE_NO_WARNINGS)
	set(USE_PIX ON CACHE INTERNAL "")

	# Enable fiber-safe optimizations on msvc build, otherwise it will cache thread_local vars
	# and make various things explode when we switch context's.
	set(CMAKE_C_FLAGS_RELEASE			"${CMAKE_C_FLAGS_RELEASE} /GT")
	set(CMAKE_CXX_FLAGS_RELEASE			"${CMAKE_CXX_FLAGS_RELEASE} /GT")
	set(CMAKE_C_FLAGS_RELWITHDEBINFO	"${CMAKE_C_FLAGS_RELWITHDEBINFO} /GT")
	set(CMAKE_CXX_FLAGS_RELWITHDEBINFO	"${CMAKE_CXX_FLAGS_RELWITHDEBINFO} /GT")
	set(CMAKE_C_FLAGS_DEBUG				"${CMAKE_C_FLAGS_DEBUG} /GT")
	set(CMAKE_CXX_FLAGS_DEBUG			"${CMAKE_CXX_FLAGS_DEBUG} /GT")	

elseif (CMAKE_SYSTEM_NAME STREQUAL "PS4")

elseif (CMAKE_SYSTEM_NAME STREQUAL "Switch")

endif()