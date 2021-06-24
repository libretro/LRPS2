# Check that people use the good file
if(NOT TOP_CMAKE_WAS_SOURCED)
    message(FATAL_ERROR "
    You did not 'cmake' the good CMakeLists.txt file. Use the one in the top dir.
    It is advice to delete all wrongly generated cmake stuff => CMakeFiles & CMakeCache.txt")
endif()

# plugin name
set(Output onepad)
if(NOT MSVC)
set(onepadFinalFlags
    -fvisibility=hidden
    -Wall
    -Wno-parentheses
    )
endif()

# onepad sources
set(onepadSources
    controller.cpp
    onepad.cpp
    state_management.cpp)
 # onepad headers
 set(onepadHeaders
     bitwise.h
     controller.h
     GamePad.h
     SDL/joystick.h
     KeyStatus.h
     onepad.h
     state_management.h)

set(onepadFinalLibs
    ${onepadFinalLibs}
    ${GTK2_LIBRARIES}
    ${X11_LIBRARIES}
)

set(onepadFinalSources
    ${onepadSources}
    ${onepadHeaders}
)

include_directories(
    ${CMAKE_BINARY_DIR}/plugins/onepad/
)

if(NOT GTKn_FOUND)
   include_directories(${GLIB_INCLUDE_DIRS})
endif()


if(BUILTIN_PAD)
    add_pcsx2_lib(${Output} "${onepadFinalSources}" "${onepadFinalLibs}" "${onepadFinalFlags}")
else()
    add_pcsx2_plugin(${Output} "${onepadFinalSources}" "${onepadFinalLibs}" "${onepadFinalFlags}")
endif()

target_compile_features(${Output} PRIVATE cxx_std_17)
