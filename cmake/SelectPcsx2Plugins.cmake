#-------------------------------------------------------------------------------
#                              Dependency message print
#-------------------------------------------------------------------------------
set(msg_dep_common_libs "check these libraries -> wxWidgets (>=3.0), aio")
set(msg_dep_pcsx2       "check these libraries -> wxWidgets (>=3.0), gtk2, zlib (>=1.2.4), pcsx2 common libs")
set(msg_dep_gsdx        "check these libraries -> opengl, png (>=1.2), zlib (>=1.2.4), X11, liblzma")
set(msg_dep_spu2x       "")
set(msg_dep_dev         "check these libraries -> gtk2, pcap, libxml2")

macro(print_dep str dep)
    if (PACKAGE_MODE)
        message(FATAL_ERROR "${str}:${dep}")
    else()
        message(STATUS "${str}:${dep}")
    endif()
endmacro(print_dep)

#-------------------------------------------------------------------------------
#								Pcsx2 core & common libs
#-------------------------------------------------------------------------------
# Check for additional dependencies.
# If all dependencies are available, including OS, build it
#-------------------------------------------------------------------------------
set(GTKn_FOUND FALSE)

#---------------------------------------
#			Common libs
# requires: -wx
#---------------------------------------
if(wxWidgets_FOUND)
    set(common_libs TRUE)
elseif(NOT EXISTS "${CMAKE_SOURCE_DIR}/common/src")
    set(common_libs FALSE)
else()
    set(common_libs FALSE)
    print_dep("Skip build of common libraries: missing dependencies" "${msg_dep_common_libs}")
endif()

#---------------------------------------
#			Pcsx2 core
# requires: -wx
#           -zlib
#           -common_libs
#           -aio
#---------------------------------------
# Common dependancy
if(wxWidgets_FOUND AND ZLIB_FOUND AND common_libs)
    set(pcsx2_core TRUE)
elseif(NOT EXISTS "${CMAKE_SOURCE_DIR}/pcsx2")
    set(pcsx2_core FALSE)
else()
    set(pcsx2_core FALSE)
    print_dep("Skip build of pcsx2 core: missing dependencies" "${msg_dep_pcsx2}")
endif()


#-------------------------------------------------------------------------------
#								Plugins
#-------------------------------------------------------------------------------
# Check all plugins for additional dependencies.
# If all dependencies of a plugin are available, including OS, the plugin will
# be build.
#-------------------------------------------------------------------------------

#---------------------------------------
#			GSdx
#---------------------------------------
# requires: -OpenGL
#           -PNG
#           -X11
#           -zlib
#---------------------------------------
set(GSdx TRUE)
#---------------------------------------

#---------------------------------------
#			USBnull
#---------------------------------------
set(USBnull TRUE)
#---------------------------------------
#-------------------------------------------------------------------------------
