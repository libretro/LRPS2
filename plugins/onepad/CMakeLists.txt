# Check that people use the good file
if(NOT TOP_CMAKE_WAS_SOURCED)
    message(FATAL_ERROR "
    You did not 'cmake' the good CMakeLists.txt file. Use the one in the top dir.
    It is advice to delete all wrongly generated cmake stuff => CMakeFiles & CMakeCache.txt")
endif()

set(compiled_images "${CMAKE_BINARY_DIR}/plugins/onepad/ImgHeader")
set(image_sources "${CMAKE_SOURCE_DIR}/plugins/onepad/Img")
set(linux_sources "Linux")
set(linux_headers "Linux")
set(wx_sources "wx_dialog")
set(wx_headers "wx_dialog")
if(NOT MSVC)
set(onepadGuiResources
    ${compiled_images}/analog.h
    ${compiled_images}/circle.h
    ${compiled_images}/cross.h
    ${compiled_images}/dp_bottom.h
    ${compiled_images}/dp_left.h
    ${compiled_images}/dp_right.h
    ${compiled_images}/dp_up.h
    ${compiled_images}/dualshock2.h
    ${compiled_images}/joystick_cursor.h
    ${compiled_images}/l1.h
    ${compiled_images}/r1.h
    ${compiled_images}/l2.h
    ${compiled_images}/r2.h
    ${compiled_images}/l3.h
    ${compiled_images}/r3.h
    ${compiled_images}/select.h
    ${compiled_images}/square.h
    ${compiled_images}/start.h
    ${compiled_images}/triangle.h
    ${compiled_images}/arrow_up.h
    ${compiled_images}/arrow_bottom.h
    ${compiled_images}/arrow_left.h
    ${compiled_images}/arrow_right.h
    )
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
if(LIBRETRO)
# onepad sources
set(onepadSources
    controller.cpp
#    GamePad.cpp
#    SDL/joystick.cpp
#    keyboard.cpp
#    KeyStatus.cpp
    onepad.cpp
    state_management.cpp)
   if(NOT MSVC)
   set(onepadSources ${onepadSources}
       resources.cpp)
   endif()
 # onepad headers
 set(onepadHeaders
     bitwise.h
     controller.h
     GamePad.h
     SDL/joystick.h
#     keyboard.h
     KeyStatus.h
     onepad.h
     state_management.h)
     if(NOT MSVC)
     set(onepadHeaders ${onepadHeaders}
        resources.h)
     endif()
else()
# onepad sources
set(onepadSources
    controller.cpp
    GamePad.cpp
    SDL/joystick.cpp
    keyboard.cpp
    KeyStatus.cpp
    onepad.cpp
    resources.cpp
    state_management.cpp)

# onepad headers
set(onepadHeaders
    bitwise.h
    controller.h
    GamePad.h
    SDL/joystick.h
    keyboard.h
    KeyStatus.h
    onepad.h
    resources.h
    state_management.h)

# onepad wx sources
set(onepadwxSources
    ${wx_sources}/dialog.cpp
    ${wx_sources}/opPanel.cpp
    ${wx_sources}/GamepadConfiguration.cpp
    ${wx_headers}/JoystickConfiguration.cpp)

# onepad wx headers
set(onepadwxHeaders
    ${wx_headers}/opPanel.h
    ${wx_headers}/GamepadConfiguration.h
    ${wx_headers}/JoystickConfiguration.h)
# onepad Linux sources
set(onepadLinuxSources
    ${linux_sources}/ini.cpp
    ${linux_sources}/linux.cpp)

# onepad Linux headers
set(onepadLinuxHeaders)

# onepad Windows sources
set(onepadWindowsSources)

# onepad Windows headers
set(onepadWindowsHeaders)
endif()
if(NOT LIBRETRO)
   set(onepadFinalLibs ${SDL2_LIBRARIES})
   add_definitions(-DSDL_BUILD)
endif()

set(onepadFinalLibs
    ${onepadFinalLibs}
    ${wxWidgets_LIBRARIES}
    ${GTK2_LIBRARIES}
    ${X11_LIBRARIES}
)

set(onepadFinalSources
    ${onepadSources}
    ${onepadHeaders}
    ${onepadwxSources}
    ${onepadwxHeaders}
    ${onepadLinuxSources}
    ${onepadLinuxHeaders}
    ${onepadGuiResources}
)

include_directories(
    ${CMAKE_BINARY_DIR}/plugins/onepad/
)

if(NOT GTKn_FOUND)
   include_directories(${GLIB_INCLUDE_DIRS})
endif()


### Generate the resources files
file(MAKE_DIRECTORY ${compiled_images})

if(MSVC)
   #todo
else()
foreach(result_file IN ITEMS
        analog circle cross dp_bottom dp_left dp_right dp_up dualshock2
        joystick_cursor l1 r1 l2 r2 l3 r3 select square start triangle arrow_up
        arrow_bottom arrow_left arrow_right)
    add_custom_command(
        OUTPUT  "${compiled_images}/${result_file}.h"
        COMMAND perl ${CMAKE_SOURCE_DIR}/linux_various/hex2h.pl "${image_sources}/${result_file}.png" "${compiled_images}/${result_file}" )
endforeach()

set(RESOURCE_FILES res/game_controller_db.txt)

   add_custom_glib_res("resources" "onepad-res.xml" "onepad_res" ${RESOURCE_FILES})
endif()

if(BUILTIN_PAD)
    add_pcsx2_lib(${Output} "${onepadFinalSources}" "${onepadFinalLibs}" "${onepadFinalFlags}")
else()
    add_pcsx2_plugin(${Output} "${onepadFinalSources}" "${onepadFinalLibs}" "${onepadFinalFlags}")
endif()

target_compile_features(${Output} PRIVATE cxx_std_17)
