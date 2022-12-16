#-------------------------------------------------------------------------------
#                              Dependency message print
#-------------------------------------------------------------------------------
set(msg_dep_common_libs "check these libraries -> wxWidgets (>=3.0), aio")
set(msg_dep_pcsx2       "check these libraries -> wxWidgets (>=3.0), gtk2, zlib (>=1.2.4), pcsx2 common libs")
set(msg_dep_gsdx        "check these libraries -> opengl, png (>=1.2), zlib (>=1.2.4), X11, liblzma")
set(msg_dep_spu2x       "")
set(msg_dep_dev         "check these libraries -> gtk2, pcap, libxml2")

macro(print_dep str dep)
        message(STATUS "${str}:${dep}")
endmacro(print_dep)

if(wxWidgets_FOUND)
    set(common_libs TRUE)
elseif(NOT EXISTS "${CMAKE_SOURCE_DIR}/common/src")
    set(common_libs FALSE)
else()
    set(common_libs FALSE)
    print_dep("Skip build of common libraries: missing dependencies" "${msg_dep_common_libs}")
endif()

set(pcsx2_core TRUE)
set(GSdx TRUE)
set(USBnull TRUE)
