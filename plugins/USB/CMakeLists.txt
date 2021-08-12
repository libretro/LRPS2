# Check that people use the good file
if(NOT TOP_CMAKE_WAS_SOURCED)
    message(FATAL_ERROR "
    You did not 'cmake' the good CMakeLists.txt file. Use the one in the top dir.
    It is advice to delete all wrongly generated cmake stuff => CMakeFiles & CMakeCache.txt")
endif()


# plugin name
set(Output USB)
if(NOT MSVC)
set(USBnullFinalFlags
-fvisibility=hidden
-Wall
-Wno-parentheses
	)
endif()
# USBnull sources
set(USBnullSources
	USB.cpp)

# USBnull headers
set(USBnullHeaders
	USB.h)

# USBnull Linux sources
set(USBnullLinuxSources
)

# USBnull Linux headers
set(USBnullLinuxHeaders
)

# USBnull Windows sources
set(USBnullWindowsSources
)

# USBnull Windows headers
set(USBnullWindowsHeaders
)

set(USBnullFinalSources
	${USBnullSources}
	${USBnullHeaders}
	${USBnullLinuxSources}
	${USBnullLinuxHeaders}
)

set(USBnullFinalLibs
	${wxWidgets_LIBRARIES}
)

add_pcsx2_lib(${Output} "${USBnullFinalSources}" "${USBnullFinalLibs}" "${USBnullFinalFlags}")

target_compile_features(${Output} PRIVATE cxx_std_17)
