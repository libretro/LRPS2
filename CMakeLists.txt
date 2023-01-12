cmake_policy(SET CMP0048 NEW)

# Project Name
project(Pcsx2)

# Debian-based distributions require at least 2.8.5 due to multiarch.
# Bumping up to 3.0 seems reasonable at this point, and will let us modernize
# things a bit.
#
# Setting it to a range tells it that it supports the features on the newer
# versions as well, avoiding setting policies.
cmake_minimum_required(VERSION 3.3.0...3.17)

# Variable to check that people use the good file
set(TOP_CMAKE_WAS_SOURCED TRUE)

# set module path
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

# include some generic functions to ensure correctness of the env
include(Pcsx2Utils)

check_no_parenthesis_in_path()
detectOperatingSystem()
check_compiler_version("7.0" "7.0")
if(NOT MSVC)
   set(CMAKE_C_COMPILER_LAUNCHER ccache)
   set(CMAKE_CXX_COMPILER_LAUNCHER ccache)
endif()

#-------------------------------------------------------------------------------
# Include specific module
# BuildParameters Must be done before SearchForStuff
include(BuildParameters)
# SearchForStuff be done before SelectPcsx2Plugins
include(SearchForStuff)
include(SelectPcsx2Plugins)

# Must be done after SearchForStuff
get_git_version_info()
write_svnrev_h()
add_definitions(-D__LIBRETRO__)

add_subdirectory(3rdparty/wxwidgets3.0)

# make common
if(common_libs)
    add_subdirectory(common/src/Utilities)
    add_subdirectory(common/src/x86emitter)
endif()

# make pcsx2
add_subdirectory(pcsx2)
