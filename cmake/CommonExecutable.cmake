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

# Suffix appropriate build tags.
if (CMAKE_BUILD_TYPE MATCHES "Debug")
	set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "${PROJECT_NAME}.debug")
else()
	set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "${PROJECT_NAME}.release")
endif()

SET_TARGET_PROPERTIES(${PROJECT_NAME} PROPERTIES PREFIX "")

# If we are building for switch, generate our nso/meta/package files.
if (CMAKE_SYSTEM_NAME STREQUAL "Switch")

	get_target_property(app_output_name ${PROJECT_NAME} OUTPUT_NAME)

	# Generate NSO
	add_custom_command(TARGET ${PROJECT_NAME} 
		POST_BUILD
		COMMAND "${SWITCH_ROOT}/Tools/CommandLineTools/MakeNso/MakeNso.exe" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${app_output_name}.nss" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${app_output_name}.nso"
	)

	# Generate metadata
	add_custom_command(TARGET ${PROJECT_NAME} 
		POST_BUILD
		COMMAND "${SWITCH_ROOT}/Tools/CommandLineTools/MakeMeta/MakeMeta.exe" "--desc" "${SWITCH_ROOT}/Resources/SpecFiles/Application.desc" "--meta" "${CMAKE_SOURCE_DIR}/docs/examples/common/switch/Application.aarch64.lp64.nmeta" "-o" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${app_output_name}.npdm" "-d" "DefaultIs64BitInstruction=True" "-d" "DefaultProcessAddressSpace=AddressSpace64Bit"
	)

	# Generate nspd
	add_custom_command(TARGET ${PROJECT_NAME} 
		POST_BUILD
		COMMAND "${SWITCH_ROOT}/Tools/CommandLineTools/AuthoringTool/AuthoringTool.exe" "createnspd" "-o" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${app_output_name}.nspd" "--meta" "${CMAKE_SOURCE_DIR}/docs/examples/common/switch/Application.aarch64.lp64.nmeta" "--type" "Application" "--program" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${app_output_name}.nspd/program0.ncd/code" "--utf8"
	)

	# Copy things around.
	add_custom_command(TARGET ${PROJECT_NAME} 
		POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${app_output_name}.npdm" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${app_output_name}.nspd/program0.ncd/code/main.npdm"
		COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${app_output_name}.nso" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${app_output_name}.nspd/program0.ncd/code/main"
		COMMAND ${CMAKE_COMMAND} -E copy "${SWITCH_ROOT}/Libraries/NX-NXFP2-a64/Develop/nnSdkEn.nso" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${app_output_name}.nspd/program0.ncd/code/sdk"
		COMMAND ${CMAKE_COMMAND} -E copy "${SWITCH_ROOT}/Libraries/NX-NXFP2-a64/Develop/nnrtld.nso" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${app_output_name}.nspd/program0.ncd/code/rtld"
	)
	
	# Generate nsp
	add_custom_command(TARGET ${PROJECT_NAME} 
		POST_BUILD
		COMMAND "${SWITCH_ROOT}/Tools/CommandLineTools/AuthoringTool/AuthoringTool.exe" "creatensp" "-o" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${app_output_name}.nsp" 
	)

endif()

# If PS4, make sure to link against fiber/system libraries.
if (CMAKE_SYSTEM_NAME STREQUAL "PS4")

	target_link_libraries(${PROJECT_NAME} SceSysmodule_stub_weak SceFiber_stub_weak SceRazorCpu_stub_weak)

endif()


if (USE_PIX)
	target_link_libraries(${PROJECT_NAME} ${CMAKE_SOURCE_DIR}/third_party/pix/bin/WinPixEventRuntime.lib)

	# Copy pix dll to output folder.
	configure_file(${libjobs_SOURCE_DIR}/third_party/pix/bin/WinPixEventRuntime.dll
				   ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/WinPixEventRuntime.dll COPYONLY)
endif()
		