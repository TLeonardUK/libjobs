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

project(libjobs C CXX)

include(${CMAKE_SOURCE_DIR}/cmake/Common.cmake)

# Build options.
option(BUILD_DOC "Build documentation" OFF)
option(BUILD_EXAMPLES "Build examples" ON)

# Version
set(VERSION_MAJOR 1 CACHE INTERNAL "")
set(VERSION_MINOR 0 CACHE INTERNAL "")
set(VERSION_PATCH 0 CACHE INTERNAL "")

# Add examples
if (BUILD_EXAMPLES)
	add_subdirectory(docs/examples/1_simple)
	add_subdirectory(docs/examples/2_debug_output)
	add_subdirectory(docs/examples/3_dependencies)
	add_subdirectory(docs/examples/4_latent_actions)
	add_subdirectory(docs/examples/5_profile_events)
	add_subdirectory(docs/examples/6_user_allocation)
	add_subdirectory(docs/examples/7_game_loop)
endif(BUILD_EXAMPLES)

# Output folders
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib/${TARGET_SYSTEM}_${TARGET_ARCHITECTURE})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib/${TARGET_SYSTEM}_${TARGET_ARCHITECTURE})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib/${TARGET_SYSTEM}_${TARGET_ARCHITECTURE})

include_directories(
	${CMAKE_SOURCE_DIR}/inc 
	${CMAKE_SOURCE_DIR}/third_party
)

# Main library 
add_library(${PROJECT_NAME} STATIC
	"src/jobs_callback_scheduler.cpp"
	"src/jobs_counter.cpp"
	"src/jobs_scheduler.cpp"
	"src/jobs_thread.cpp"
	"src/jobs_fiber.cpp"
	"src/jobs_job.cpp"
	"src/jobs_enums.cpp"
	"src/jobs_event.cpp"
	"src/jobs_utils.cpp"
)

include(${libjobs_SOURCE_DIR}/cmake/CommonLibrary.cmake)

# Generate documentation if available.
if (BUILD_DOC)
	if (NOT CMAKE_BUILD_TYPE MATCHES "Debug")
		find_package(Doxygen)
		if (DOXYGEN_FOUND)

			set(DOXYGEN_IN ${CMAKE_SOURCE_DIR}/Doxyfile.in)
			set(DOXYGEN_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)

			configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)
			message("Doxygen build started")

			add_custom_target(doc_doxygen ALL
				COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
				WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
				COMMENT "Generating API documentation with Doxygen"
				VERBATIM)

		else (DOXYGEN_FOUND)
		  message("Doxygen need to be installed to generate the doxygen documentation")
		endif (DOXYGEN_FOUND)
	endif()
endif(BUILD_DOC)