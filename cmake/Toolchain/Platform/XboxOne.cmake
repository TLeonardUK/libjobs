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
if( _XBOX_ONE_PLATFORM_ )
	return()
endif()
set(_XBOX_ONE_PLATFORM_ 1)

# Extensions
set(CMAKE_IMPORT_LIBRARY_PREFIX "")
set(CMAKE_SHARED_LIBRARY_PREFIX "")
set(CMAKE_SHARED_MODULE_PREFIX  "")
set(CMAKE_STATIC_LIBRARY_PREFIX "")

set(CMAKE_EXECUTABLE_SUFFIX     ".exe")
set(CMAKE_IMPORT_LIBRARY_SUFFIX ".dll")
set(CMAKE_SHARED_LIBRARY_SUFFIX ".lib")
set(CMAKE_SHARED_MODULE_SUFFIX  ".lib")
set(CMAKE_STATIC_LIBRARY_SUFFIX ".lib")
SET(CMAKE_C_OUTPUT_EXTENSION	".obj")

# General compiler behaviour. Annoyingly this has to be here not in the 
# toolchain file -_-
SET(CMAKE_OUTPUT_CXX_FLAG "-OUT:")
SET(CMAKE_INCLUDE_FLAG_CXX "-I")
SET(CMAKE_INCLUDE_FLAG_CXX_SEP "")
SET(CMAKE_LIBRARY_PATH_FLAG "/LIBPATH:")
SET(CMAKE_LINK_LIBRARY_FLAG "-l")
