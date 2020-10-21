set(wxWidgets_FOUND 1)
set(wxWidgets_DIR ${CMAKE_SOURCE_DIR}/3rdparty/wxwidgets3.0)
set(wxWidgets_INCLUDE_DIRS ${wxWidgets_DIR}/include ${wxWidgets_DIR}/include/nogui)
set(wxWidgets_LIBRARIES wxwidgets)
set(wxWidgets_CXX_FLAGS)

include_directories(SYSTEM ${wxWidgets_INCLUDE_DIRS})
add_definitions(-DwxUSE_GUI=0 -D_FILE_OFFSET_BITS=64)
if(UNIX)
   add_definitions(-DwxUSE_UNIX -D__UNIX__)
   if(APPLE)
      add_definitions(-D__DARWIN__)
   else()
       add_definitions(-D__LINUX__)
   endif()
elseif(WIN32)
   add_definitions(-D__WINDOWS__)
   if(MINGW)
      add_definitions(-D__GNUWIN32__)
   endif()
endif()



