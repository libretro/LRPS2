# Check that people use the good file
if(NOT TOP_CMAKE_WAS_SOURCED)
    message(FATAL_ERROR "
    You did not 'cmake' the good CMakeLists.txt file. Use the one in the top dir.
    It is advice to delete all wrongly generated cmake stuff => CMakeFiles & CMakeCache.txt")
endif()

# plugin name
set(Output PAD)
if(NOT MSVC)
set(onepadFinalFlags
    -fvisibility=hidden
    -Wall
    -Wno-parentheses
    )
endif()

# onepad sources
set(onepadSources
    onepad.cpp
    state_management.cpp)
 # onepad headers
 set(onepadHeaders
     GamePad.h
     KeyStatus.h
     onepad.h
     state_management.h)

set(onepadFinalLibs
    ${onepadFinalLibs}
)

set(onepadFinalSources
    ${onepadSources}
    ${onepadHeaders}
)

include_directories(
    ${CMAKE_BINARY_DIR}/plugins/PAD/
)

add_pcsx2_lib(${Output} "${onepadFinalSources}" "${onepadFinalLibs}" "${onepadFinalFlags}")

target_compile_features(${Output} PRIVATE cxx_std_17)
