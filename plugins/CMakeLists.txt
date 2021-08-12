# Check that people use the good file
if(NOT TOP_CMAKE_WAS_SOURCED)
    message(FATAL_ERROR "
    You did not 'cmake' the good CMakeLists.txt file. Use the one in the top dir.
    It is advice to delete all wrongly generated cmake stuff => CMakeFiles & CMakeCache.txt")
endif()


if(EXISTS "${CMAKE_SOURCE_DIR}/plugins/DEV9" AND dev9null)
	add_subdirectory(DEV9)
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/plugins/dev9ghzdrk" AND dev9ghzdrk)
	add_subdirectory(dev9ghzdrk)
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/plugins/GS" AND GSdx)
	add_subdirectory(GS)
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/plugins/PAD" AND onepad)
   add_subdirectory(PAD)
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/plugins/USB" AND USBnull)
	add_subdirectory(USB)
endif()
