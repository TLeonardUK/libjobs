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
if( _XBOX_ONE_TOOLCHAIN_ )
	return()
endif()
set(_XBOX_ONE_TOOLCHAIN_ 1)

# Required XBOX SDK.
# @todo: Fix these hard-coded paths.
set(BIN_TOOLS_ROOT_DIR "C:/Program Files (x86)/Microsoft Visual Studio/2017/Professional/VC/Tools/MSVC/14.15.26726")

# @todo do this properly
set(BIN_TOOLS_COMPILER_DIR "${BIN_TOOLS_ROOT_DIR}/bin/Hostx64/x64")
set(BIN_TOOLS_INCLUDE_DIR "${BIN_TOOLS_ROOT_DIR}/include")

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

set(XDK_COMMON_ROOT "${XDK_ROOT}/..")

# Set CMake system root search path
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR})
set(MSVC TRUE CACHE BOOL "Visual Studio Compiler" FORCE INTERNAL)
set(CMAKE_CROSSCOMPILING "TRUE")
set(CMAKE_SYSTEM XboxOne)
set(CMAKE_SYSTEM_NAME XboxOne)
set(CMAKE_SYSTEM_PROCESSOR "AMD64")
set(CMAKE_SYSROOT "${BIN_TOOLS_COMPILER_DIR}")

# Set the compilers to the ones found in XboxOne XDK directory
set(CMAKE_C_COMPILER "${BIN_TOOLS_COMPILER_DIR}/cl.exe")
set(CMAKE_CXX_COMPILER "${BIN_TOOLS_COMPILER_DIR}/cl.exe")
set(CMAKE_ASM_COMPILER "${BIN_TOOLS_COMPILER_DIR}/ml64.exe")
set(CMAKE_LINKER "${BIN_TOOLS_COMPILER_DIR}/link.exe")
set(CMAKE_AR "${BIN_TOOLS_COMPILER_DIR}/lib.exe")

# Force compilers to skip detecting compiler ABI info and compile features
set(CMAKE_C_COMPILER_FORCED True)
set(CMAKE_CXX_COMPILER_FORCED True)
set(CMAKE_ASM_COMPILER_FORCED True)

# Only search the XBoxOne XDK, not the remainder of the host file system
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Various required defines.
add_definitions(-D_UITHREADCTXT_SUPPORT=0)
add_definitions(-D_CRT_USE_WINAPI_PARTITION_APP)
add_definitions(-D__WRL_NO_DEFAULT_LIB__)
add_definitions(-DMONOLITHIC=1)
add_definitions(-D_DURANGO)
add_definitions(-D_XBOX_ONE)
add_definitions(-DWINAPI_FAMILY=WINAPI_FAMILY_TV_TITLE)
add_definitions(-D_TITLE)
add_definitions(-DWIN32_LEAN_AND_MEAN)
add_definitions(-D_UNICODE)
add_definitions(-DUNICODE)
add_definitions(-D_HAS_EXCEPTIONS=0)

# Various flags, these should really be based off the cmake settinsg that should change these, on
# the off chance one of our sub-projects needs to modify them.
SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} /GR")   # RTTI enabled 
SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} /EHsc") # Exception handling enabled
SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} /Zi /FS")	# Debug info stored in pdb
SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} /ZW")		# Windows Runtime Support

# Runtime library/iterator debug level.
if (CMAKE_BUILD_TYPE MATCHES "Debug")
	add_definitions(-D_ITERATOR_DEBUG_LEVEL=2)
	SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} /MDd")
	SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} /Ob0")
	SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} /Od")	
else()
	add_definitions(-D_ITERATOR_DEBUG_LEVEL=0)
	SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} /MD")
	SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} /Ob2")	
	SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} /O2")	
endif()

# And all our include directories.
include_directories(BEFORE SYSTEM ${BIN_TOOLS_INCLUDE_DIR})
include_directories(BEFORE SYSTEM "${XDK_ROOT}/xdk/include/um")
include_directories(BEFORE SYSTEM "${XDK_ROOT}/xdk/include/shared")
include_directories(BEFORE SYSTEM "${XDK_ROOT}/xdk/include/winrt")
include_directories(BEFORE SYSTEM "${XDK_ROOT}/xdk/include/cppwinrt")
include_directories(BEFORE SYSTEM "${XDK_ROOT}/xdk/ucrt/inc")
include_directories(BEFORE SYSTEM "${XDK_ROOT}/xdk/VS2017/vc/include")
include_directories(BEFORE SYSTEM "${XDK_ROOT}/xdk/VS2017/vc/include/platform/amd64")

# Library directories
link_directories("${XDK_ROOT}/xdk/Lib/amd64")
link_directories("${XDK_ROOT}/xdk/ucrt/lib/amd64")
link_directories("${XDK_ROOT}/xdk/VS2017/vc/lib/amd64")
link_directories("${XDK_ROOT}/xdk/VS2017/vc/platform/amd64")
link_directories("${BIN_TOOLS_ROOT_DIR}/lib/x64")
link_directories("${BIN_TOOLS_ROOT_DIR}/atlmfc/lib/x64")

# Using directories
SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} /AI\"${XDK_ROOT}/xdk/VS2017/vc/platform/amd64\"")
SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} /AI\"${XDK_EXTENSION_ROOT}/references/commonconfiguration/neutral\"")

# Forced using WinMD files.
SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} /FU\"${XDK_ROOT}/xdk/VS2017/vc/platform/amd64/platform.winmd\"")
SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} /FU\"${XDK_EXTENSION_ROOT}/references/commonconfiguration/neutral/windows.winmd\"")

# Include all the default libs
SET(DEFAULT_LIBS "COMBASE.LIB KERNELX.LIB UUID.LIB /NODEFAULTLIB:ADVAPI32.LIB /NODEFAULTLIB:ATL.LIB /NODEFAULTLIB:ATLS.LIB /NODEFAULTLIB:ATLSD.LIB /NODEFAULTLIB:ATLSN.LIB /NODEFAULTLIB:ATLSND.LIB /NODEFAULTLIB:COMCTL32.LIB /NODEFAULTLIB:COMSUPP.LIB /NODEFAULTLIB:DBGHELP.LIB /NODEFAULTLIB:GDI32.LIB /NODEFAULTLIB:GDIPLUS.LIB /NODEFAULTLIB:GUARDCFW.LIB /NODEFAULTLIB:KERNEL32.LIB /NODEFAULTLIB:MMC.LIB /NODEFAULTLIB:MSIMG32.LIB /NODEFAULTLIB:MSVCOLE.LIB /NODEFAULTLIB:MSVCOLED.LIB /NODEFAULTLIB:MSWSOCK.LIB /NODEFAULTLIB:NTSTRSAFE.LIB /NODEFAULTLIB:OLE2.LIB /NODEFAULTLIB:OLE2AUTD.LIB /NODEFAULTLIB:OLE2AUTO.LIB /NODEFAULTLIB:OLE2D.LIB /NODEFAULTLIB:OLE2UI.LIB /NODEFAULTLIB:OLE2UID.LIB /NODEFAULTLIB:OLE32.LIB /NODEFAULTLIB:OLEACC.LIB /NODEFAULTLIB:OLEAUT32.LIB /NODEFAULTLIB:OLEDLG.LIB /NODEFAULTLIB:OLEDLGD.LIB /NODEFAULTLIB:OLDNAMES.LIB /NODEFAULTLIB:RUNTIMEOBJECT.LIB /NODEFAULTLIB:SHELL32.LIB /NODEFAULTLIB:SHLWAPI.LIB /NODEFAULTLIB:STRSAFE.LIB /NODEFAULTLIB:URLMON.LIB /NODEFAULTLIB:USER32.LIB /NODEFAULTLIB:USERENV.LIB /NODEFAULTLIB:UUID.LIB /NODEFAULTLIB:WLMOLE.LIB /NODEFAULTLIB:WLMOLED.LIB /NODEFAULTLIB:WS2_32.LIB")

# Compile command formats
set(CMAKE_CXX_COMPILE_OBJECT
    "<CMAKE_CXX_COMPILER> /nologo <DEFINES> <INCLUDES> <FLAGS> /Fo<OBJECT> -c <SOURCE>")
set(CMAKE_CXX_LINK_EXECUTABLE
    "\"${CMAKE_LINKER}\" /nologo <OBJECTS> /out:<TARGET> /implib:<TARGET_IMPLIB> /pdb:<TARGET_PDB> /machine:X64 /subsystem:Windows /TLBID:1 /DYNAMICBASE /NXCOMPAT /MANIFEST:NO <LINK_LIBRARIES> ${DEFAULT_LIBS}")
set(CMAKE_CXX_ARCHIVE_CREATE 
	"<CMAKE_AR> <OBJECTS> /OUT:<TARGET> <LINK_FLAGS> /NOLOGO /MACHINE:X64")
set(CMAKE_CXX_ARCHIVE_APPEND 
	"<CMAKE_AR> <OBJECTS> /OUT:<TARGET> <LINK_FLAGS> /NOLOGO /MACHINE:X64")
set(CMAKE_CXX_ARCHIVE_FINISH "")

	

