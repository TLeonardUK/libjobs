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

cmake_minimum_required(VERSION 3.8)

project(4_latent_actions C CXX)

include(${libjobs_SOURCE_DIR}/cmake/Common.cmake)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${libjobs_SOURCE_DIR}/bin/${TARGET_SYSTEM}_${TARGET_ARCHITECTURE})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${libjobs_SOURCE_DIR}/bin/${TARGET_SYSTEM}_${TARGET_ARCHITECTURE})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${libjobs_SOURCE_DIR}/bin/${TARGET_SYSTEM}_${TARGET_ARCHITECTURE})

include_directories(
	${libjobs_SOURCE_DIR}/inc 
	${libjobs_SOURCE_DIR}/third_party
)

add_executable(${PROJECT_NAME} 
	../common/example_framework.cpp 
	main.cpp
)

target_link_libraries(${PROJECT_NAME}
	libjobs
)

include(${libjobs_SOURCE_DIR}/cmake/CommonExecutable.cmake)