### TODO
# Hardcode GAMEINDEX_DIR, if default is fine for everybody

### Select the build type
# Use Release/Devel/Debug      : -DCMAKE_BUILD_TYPE=Release|Devel|Debug
# Enable/disable the stripping : -DCMAKE_BUILD_STRIP=TRUE|FALSE
# generation .po based on src  : -DCMAKE_BUILD_PO=TRUE|FALSE

### GCC optimization options
# control C flags             : -DUSER_CMAKE_C_FLAGS="cflags"
# control C++ flags           : -DUSER_CMAKE_CXX_FLAGS="cxxflags"
# control link flags          : -DUSER_CMAKE_LD_FLAGS="ldflags"

### Packaging options
# Plugin installation path    : -DPLUGIN_DIR="/usr/lib/pcsx2"
# GL Shader installation path : -DGLSL_SHADER_DIR="/usr/share/games/pcsx2"
# Game DB installation path   : -DGAMEINDEX_DIR="/usr/share/games/pcsx2"
#-------------------------------------------------------------------------------

#-------------------------------------------------------------------------------
# Misc option
#-------------------------------------------------------------------------------
option(DISABLE_BUILD_DATE "Disable including the binary compile date")
option(ENABLE_TESTS "Enables building the unit tests" OFF)
option(LIBRETRO "Enables building the libretro core" ON)
set(DISABLE_BUILD_DATE ON)

if(DISABLE_BUILD_DATE OR openSUSE)
    message(STATUS "Disabling the inclusion of the binary compile date.")
    add_definitions(-DDISABLE_BUILD_DATE)
endif()

#-------------------------------------------------------------------------------
# Graphical option
#-------------------------------------------------------------------------------
option(REBUILD_SHADER "Rebuild GLSL/CG shader (developer option)")
option(BUILD_REPLAY_LOADERS "Build GS replayer to ease testing (developer option)")

#-------------------------------------------------------------------------------
# Path and lib option
#-------------------------------------------------------------------------------
option(PACKAGE_MODE "Use this option to ease packaging of PCSX2 (developer/distribution option)")
option(DISABLE_CHEATS_ZIP "Disable including the cheats_ws.zip file")
option(DISABLE_PCSX2_WRAPPER "Disable including the PCSX2-linux.sh file")
option(XDG_STD "Use XDG standard path instead of the standard PCSX2 path")
option(EXTRA_PLUGINS "Build various 'extra' plugins")
option(PORTAUDIO_API "Build portaudio support on spu2x" ON)
option(SDL2_API "Use SDL2 on spu2x and onepad (wxWidget mustn't be built with SDL1.2 support" ON)

if(PACKAGE_MODE)
    if(NOT DEFINED PLUGIN_DIR)
        set(PLUGIN_DIR "${CMAKE_INSTALL_PREFIX}/lib/games/PCSX2")
    endif()

    if(NOT DEFINED GAMEINDEX_DIR)
        set(GAMEINDEX_DIR "${CMAKE_INSTALL_PREFIX}/share/games/PCSX2")
    endif()

    if(NOT DEFINED BIN_DIR)
        set(BIN_DIR "${CMAKE_INSTALL_PREFIX}/bin")
    endif()

    if(NOT DEFINED DOC_DIR)
        set(DOC_DIR "${CMAKE_INSTALL_PREFIX}/share/doc/PCSX2")
    endif()

    if(NOT DEFINED MAN_DIR)
        set(MAN_DIR "${CMAKE_INSTALL_PREFIX}/share/man")
    endif()

    # Compile all source codes with those defines
    add_definitions(-DPLUGIN_DIR_COMPILATION=${PLUGIN_DIR} -DGAMEINDEX_DIR_COMPILATION=${GAMEINDEX_DIR} -DDOC_DIR_COMPILATION=${DOC_DIR})
endif()

if(APPLE)
    option(OSX_USE_DEFAULT_SEARCH_PATH "Don't prioritize system library paths" OFF)
    option(SKIP_POSTPROCESS_BUNDLE "Skip postprocessing bundle for redistributability" OFF)
endif()

#-------------------------------------------------------------------------------
# Compiler extra
#-------------------------------------------------------------------------------
option(USE_ASAN "Enable address sanitizer")

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(USE_CLANG TRUE)
    message(STATUS "Building with Clang/LLVM.")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Intel")
    set(USE_ICC TRUE)
    message(STATUS "Building with Intel's ICC.")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(USE_GCC TRUE)
    message(STATUS "Building with GNU GCC")
elseif(NOT MSVC)
    message(FATAL_ERROR "Unknown compiler: ${CMAKE_CXX_COMPILER_ID}")
endif()

#-------------------------------------------------------------------------------
# Select the support of plugin
#-------------------------------------------------------------------------------
option(BUILTIN_GS           "Disable support of GS plugin (developer option)")
option(BUILTIN_PAD          "Disable support of PAD plugin (developer option)")
#option(BUILTIN_SPU2         "Disable support of SPU2 plugin (developer option)")
option(BUILTIN_USB          "Disable support of USB plugin (developer option)")
#option(BUILTIN_FW           "Disable support of FW plugin (developer option)")
option(BUILTIN_DEV9         "Disable support of DEV9 plugin (developer option)")
#option(BUILTIN_CDVD         "Disable support of CDVD plugin (developer option)")

set(BUILTIN_GS    TRUE)
set(BUILTIN_PAD   TRUE)
#set(BUILTIN_SPU2  TRUE)
set(BUILTIN_USB   TRUE)
set(BUILTIN_FW    TRUE)
set(BUILTIN_DEV9  TRUE)
set(BUILTIN_CDVD  TRUE)

set(PLUGIN_SUPPORT "")
if(BUILTIN_GS)
    set(PLUGIN_SUPPORT "${PLUGIN_SUPPORT} -DBUILTIN_GS_PLUGIN")
endif()
if(BUILTIN_PAD)
    set(PLUGIN_SUPPORT "${PLUGIN_SUPPORT} -DBUILTIN_PAD_PLUGIN")
endif()
#if(BUILTIN_SPU2)
#    set(PLUGIN_SUPPORT "${PLUGIN_SUPPORT} -DBUILTIN_SPU2_PLUGIN")
#endif()
if(BUILTIN_USB)
    set(PLUGIN_SUPPORT "${PLUGIN_SUPPORT} -DBUILTIN_USB_PLUGIN")
endif()
#if(BUILTIN_FW)
#    set(PLUGIN_SUPPORT "${PLUGIN_SUPPORT} -DBUILTIN_FW_PLUGIN")
#endif()
if(BUILTIN_DEV9)
    set(PLUGIN_SUPPORT "${PLUGIN_SUPPORT} -DBUILTIN_DEV9_PLUGIN")
endif()
#if(BUILTIN_CDVD)
#    set(PLUGIN_SUPPORT "${PLUGIN_SUPPORT} -DBUILTIN_CDVD_PLUGIN")
#endif()

#-------------------------------------------------------------------------------
# if no build type is set, use Devel as default
# Note without the CMAKE_BUILD_TYPE options the value is still defined to ""
# Ensure that the value set by the User is correct to avoid some bad behavior later
#-------------------------------------------------------------------------------
if(NOT CMAKE_BUILD_TYPE MATCHES "Debug|Devel|Release|Prof")
	set(CMAKE_BUILD_TYPE Devel)
	message(STATUS "BuildType set to ${CMAKE_BUILD_TYPE} by default")
endif()
# AVX2 doesn't play well with gdb
if(CMAKE_BUILD_TYPE MATCHES "Debug")
    SET(DISABLE_ADVANCE_SIMD ON)
endif()

# Initially strip was disabled on release build but it is not stackstrace friendly!
# It only cost several MB so disbable it by default
option(CMAKE_BUILD_STRIP "Srip binaries to save a couple of MB (developer option)")

if(NOT DEFINED CMAKE_BUILD_PO)
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        set(CMAKE_BUILD_PO TRUE)
        message(STATUS "Enable the building of po files by default in ${CMAKE_BUILD_TYPE} build !!!")
    else()
        set(CMAKE_BUILD_PO FALSE)
        message(STATUS "Disable the building of po files by default in ${CMAKE_BUILD_TYPE} build !!!")
    endif()
endif()

#-------------------------------------------------------------------------------
# Select the architecture
#-------------------------------------------------------------------------------
option(DISABLE_ADVANCE_SIMD "Disable advance use of SIMD (SSE2+ & AVX)" OFF)

# Print if we are cross compiling.
if(CMAKE_CROSSCOMPILING)
    message(STATUS "Cross compilation is enabled.")
else()
    message(STATUS "Cross compilation is disabled.")
endif()

# Architecture bitness detection
include(TargetArch)
target_architecture(PCSX2_TARGET_ARCHITECTURES)
if(${PCSX2_TARGET_ARCHITECTURES} MATCHES "x86_64" OR ${PCSX2_TARGET_ARCHITECTURES} MATCHES "i386")
	message(STATUS "Compiling a ${PCSX2_TARGET_ARCHITECTURES} build on a ${CMAKE_HOST_SYSTEM_PROCESSOR} host.")
else()
	message(FATAL_ERROR "Unsupported architecture: ${PCSX2_TARGET_ARCHITECTURES}")
endif()

if(${PCSX2_TARGET_ARCHITECTURES} MATCHES "i386")
    # * -fPIC option was removed for multiple reasons.
    #     - Code only supports the x86 architecture.
    #     - code uses the ebx register so it's not compliant with PIC.
    #     - Impacts the performance too much.
    #     - Only plugins. No package will link to them.
    set(CMAKE_POSITION_INDEPENDENT_CODE OFF)

    if(NOT DEFINED ARCH_FLAG)
        if (DISABLE_ADVANCE_SIMD)
            if (USE_ICC)
                set(ARCH_FLAG "-msse2")
            else()
                set(ARCH_FLAG "-msse -msse2 -mfxsr -march=i686")
            endif()
        else()
            # AVX requires some fix of the ABI (mangling) (default 2)
            # Note: V6 requires GCC 4.7
            #set(ARCH_FLAG "-march=native -fabi-version=6")
            set(ARCH_FLAG "-mfxsr -march=native")
        endif()
    endif()

    add_definitions(-D_ARCH_32=1 -D_M_X86=1 -D_M_X86_32=1)
    set(_ARCH_32 1)
    set(_M_X86 1)
    set(_M_X86_32 1)
elseif(${PCSX2_TARGET_ARCHITECTURES} MATCHES "x86_64")
    # x86_64 requires -fPIC
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)

    if(NOT DEFINED ARCH_FLAG)
        if (DISABLE_ADVANCE_SIMD)
            if (USE_ICC)
                set(ARCH_FLAG "-msse2")
            else()
                set(ARCH_FLAG "-msse -msse2 -mfxsr")
            endif()
        else()
            #set(ARCH_FLAG "-march=native -fabi-version=6")
            set(ARCH_FLAG "-march=native")
        endif()
    endif()
    add_definitions(-D_ARCH_64=1 -D_M_X86=1 -D_M_X86_64=1 -D__M_X86_64=1)
    set(_ARCH_64 1)
    set(_M_X86 1)
    set(_M_X86_64 1)
else()
    # All but i386 requires -fPIC
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)

    message(FATAL_ERROR "Unsupported architecture: ${PCSX2_TARGET_ARCHITECTURES}")
endif()

if(MSVC)
   set(ARCH_FLAG)
   #   add_compile_options(/permissive-)
   #   add_compile_options(/Zc:inline)
   #   add_compile_options(/Zc:throwingNew)
   #   add_compile_options(/volatile:iso)
   #string(APPEND CMAKE_EXE_LINKER_FLAGS " /NXCOMPAT")
#   add_compile_options(/EHsc)
#   add_definitions(-DPCSX2_DEVBUILD -DPCSX2_DEBUG -D_DEBUG)
#   add_definitions(-DNDEBUG -DPCSX2_DEVBUILD -D_DEVEL)
#   add_definitions(-DNDEBUG -DPCSX2_DEVBUILD)
   add_definitions(-DNDEBUG)

   add_compile_options(/EHa)
   add_compile_options(/MP)
   add_compile_options(/Zi)
   add_link_options(/DEBUG)
   add_definitions(-D__WIN32__ -DWIN32 -D_WINDOWS -D_CRT_SECURE_NO_WARNINGS -D_CRT_SECURE_NO_DEPRECATE)
   add_definitions(${PLUGIN_SUPPORT})
   add_definitions(-D_ARCH_64=1 -D_M_X86_64 -D__M_X86_64)
   add_definitions(/wd4063 /wd4100 /wd4267 /wd4244 /wd4244 /wd4312 /wd4334)
else(MSVC)
#-------------------------------------------------------------------------------
# Control GCC flags
#-------------------------------------------------------------------------------
### Cmake set default value for various compilation variable
### Here the list of default value for documentation purpose
# ${CMAKE_SHARED_LIBRARY_CXX_FLAGS} = "-fPIC"
# ${CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS} = "-rdynamic"
#
# ${CMAKE_C_FLAGS} = "-g -O2"
# ${CMAKE_CXX_FLAGS} = "-g -O2"
# Use in debug mode
# ${CMAKE_CXX_FLAGS_DEBUG} = "-g"
# Use in release mode
# ${CMAKE_CXX_FLAGS_RELEASE} = "-O3 -DNDEBUG"

#-------------------------------------------------------------------------------
# Do not use default cmake flags
#-------------------------------------------------------------------------------
set(CMAKE_C_FLAGS_DEBUG "")
set(CMAKE_CXX_FLAGS_DEBUG "")
set(CMAKE_C_FLAGS_DEVEL "")
set(CMAKE_CXX_FLAGS_DEVEL "")
set(CMAKE_C_FLAGS_RELEASE "")
set(CMAKE_CXX_FLAGS_RELEASE "")

#-------------------------------------------------------------------------------
# Remove bad default option
#-------------------------------------------------------------------------------
# Remove -rdynamic option that can some segmentation fault when openining pcsx2 plugins
set(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "")
set(CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS "")
if(${PCSX2_TARGET_ARCHITECTURES} MATCHES "i386")
	# Remove -fPIC option on 32bit architectures.
	# No good reason to use it for plugins, also it impacts performance.
	set(CMAKE_SHARED_LIBRARY_C_FLAGS "")
	set(CMAKE_SHARED_LIBRARY_CXX_FLAGS "")
endif()

#-------------------------------------------------------------------------------
# Set some default compiler flags
#-------------------------------------------------------------------------------
option(USE_LTO "Enable LTO optimization")
option(USE_PGO_GENERATE "Enable PGO optimization (generate profile)")
option(USE_PGO_OPTIMIZE "Enable PGO optimization (use profile)")

# Note1: Builtin strcmp/memcmp was proved to be slower on Mesa than stdlib version.
# Note2: float operation SSE is impacted by the PCSX2 SSE configuration. In particular, flush to zero denormal.
set(COMMON_FLAG "-pipe -fvisibility=hidden -pthread -fno-builtin-strcmp -fno-builtin-memcmp -mfpmath=sse")

# Remove FORTIFY_SOURCE when compiling as debug, because it spams a lot of warnings on clang due to no optimization.
if (CMAKE_BUILD_TYPE MATCHES "Debug")
set(HARDENING_FLAG "-Wformat -Wformat-security")
else()
set(HARDENING_FLAG "-D_FORTIFY_SOURCE=2  -Wformat -Wformat-security")
endif()

# -Wno-attributes: "always_inline function might not be inlinable" <= real spam (thousand of warnings!!!)
# -Wno-missing-field-initializers: standard allow to init only the begin of struct/array in static init. Just a silly warning.
# Note: future GCC (aka GCC 5.1.1) has less false positive so warning could maybe put back
# -Wno-unused-function: warn for function not used in release build
# -Wno-unused-value: lots of warning for this kind of statements "0 && ...". There are used to disable some parts of code in release/dev build.
# -Wno-overloaded-virtual: Gives a fair number of warnings under clang over in the wxwidget gui section of the code.
set(DEFAULT_WARNINGS "-Wall -Wextra -Wno-attributes -Wno-unused-function -Wno-unused-parameter -Wno-missing-field-initializers -Wno-overloaded-virtual -Wno-address")
if (NOT USE_ICC)
    set(DEFAULT_WARNINGS "${DEFAULT_WARNINGS} -Wno-unused-value ")
endif()

# -Wstrict-aliasing=n: to fix one day aliasing issue. n=1/2/3
if (USE_ICC)
    set(AGGRESSIVE_WARNING "-Wstrict-aliasing ")
else()
    set(AGGRESSIVE_WARNING "-Wstrict-aliasing -Wstrict-overflow=1 ")
endif()

if (USE_CLANG)
    # -Wno-deprecated-register: glib issue...
    set(DEFAULT_WARNINGS "${DEFAULT_WARNINGS}  -Wno-deprecated-register -Wno-c++14-extensions")
    set(DBG "-g -fno-omit-frame-pointer")
elseif (USE_ICC)
    set(DBG "-g -fno-omit-frame-pointer")
elseif (USE_GCC)
    set(DBG "-ggdb3 -fno-omit-frame-pointer")
endif()

if (USE_LTO)
    include(ProcessorCount)
    ProcessorCount(ncpu)
    set(LTO_FLAGS "-fuse-linker-plugin -flto=${ncpu}")
    set(DBG "") # not supported with LTO
    set(CMAKE_AR /usr/bin/gcc-ar CACHE STRING "Archiver" FORCE)
    set(CMAKE_RANLIB /usr/bin/gcc-ranlib CACHE STRING "ranlib" FORCE)
    set(CMAKE_NM /usr/bin/gcc-nm CACHE STRING "nm" FORCE)
else()
    set(LTO_FLAGS "")
endif()

if (USE_PGO_GENERATE OR USE_PGO_OPTIMIZE)
    set(PGO_FLAGS "-fprofile-dir=${CMAKE_SOURCE_DIR}/profile")
endif()

if (USE_PGO_GENERATE)
    set(PGO_FLAGS "${PGO_FLAGS} -fprofile-generate")
endif()

if(USE_PGO_OPTIMIZE)
    set(PGO_FLAGS "${PGO_FLAGS} -fprofile-use")
endif()
endif(MSVC)
if(CMAKE_BUILD_TYPE MATCHES "Debug")
    set(DEBUG_FLAG "${DBG} -DPCSX2_DEVBUILD -DPCSX2_DEBUG -D_DEBUG")
elseif(CMAKE_BUILD_TYPE MATCHES "Devel")
    set(DEBUG_FLAG "${DBG} -DNDEBUG -DPCSX2_DEVBUILD -D_DEVEL")
elseif(CMAKE_BUILD_TYPE MATCHES "Release")
    set(DEBUG_FLAG "-DNDEBUG")
elseif(CMAKE_BUILD_TYPE MATCHES "Prof")
    # Keep frame pointer and debug information for profiler tool
    set(DEBUG_FLAG "-g -fno-omit-frame-pointer -DNDEBUG")
endif()

if (USE_ASAN)
    set(ASAN_FLAG "-fsanitize=address ${DBG} -DASAN_WORKAROUND")
else()
    set(ASAN_FLAG "")
endif()

if(NOT DEFINED OPTIMIZATION_FLAG)
    if (CMAKE_BUILD_TYPE STREQUAL Debug)
#        if (USE_GCC)
#            set(OPTIMIZATION_FLAG -Og)
#        else()
            set(OPTIMIZATION_FLAG -O0)
#        endif()
    else()
        set(OPTIMIZATION_FLAG -O2)
    endif()
endif()

if (NOT DEFINED PGO)
    set(PGO "none")
    set(GCOV_LIBRARIES "")
else()
    set(GCOV_LIBRARIES "-lgcov")
endif()

if(USE_CLANG)
    if(TIMETRACE)
        set(COMMON_FLAG "${COMMON_FLAG} -ftime-trace ")
    endif()
endif()

# Note: -DGTK_DISABLE_DEPRECATED can be used to test a build without gtk deprecated feature. It could be useful to port to a newer API
set(DEFAULT_GCC_FLAG "${ARCH_FLAG} ${COMMON_FLAG} ${DEFAULT_WARNINGS} ${AGGRESSIVE_WARNING} ${HARDENING_FLAG} ${DEBUG_FLAG} ${ASAN_FLAG} ${OPTIMIZATION_FLAG} ${LTO_FLAGS} ${PGO_FLAGS} ${PLUGIN_SUPPORT}")
# c++ only flags
if(NOT MSVC)
set(DEFAULT_CPP_FLAG "${DEFAULT_GCC_FLAG} -Wno-invalid-offsetof")
endif()
#-------------------------------------------------------------------------------
# Allow user to set some default flags
# Note: string STRIP must be used to remove trailing and leading spaces.
#       See policy CMP0004
#-------------------------------------------------------------------------------
# TODO: once we completely clean all flags management, this mess could be cleaned ;)
### linker flags
if(DEFINED USER_CMAKE_LD_FLAGS)
    message(STATUS "Pcsx2 is very sensible with gcc flags, so use USER_CMAKE_LD_FLAGS at your own risk !!!")
    string(STRIP "${USER_CMAKE_LD_FLAGS}" USER_CMAKE_LD_FLAGS)
else()
    set(USER_CMAKE_LD_FLAGS "")
endif()

# ask the linker to strip the binary
if(CMAKE_BUILD_STRIP)
    string(STRIP "${USER_CMAKE_LD_FLAGS} -s" USER_CMAKE_LD_FLAGS)
endif()


### c flags
# Note CMAKE_C_FLAGS is also send to the linker.
# By default allow build on amd64 machine
if(DEFINED USER_CMAKE_C_FLAGS)
    message(STATUS "Pcsx2 is very sensible with gcc flags, so use USER_CMAKE_C_FLAGS at your own risk !!!")
    string(STRIP "${USER_CMAKE_C_FLAGS}" CMAKE_C_FLAGS)
endif()
# Use some default machine flags
string(STRIP "${CMAKE_C_FLAGS} ${DEFAULT_GCC_FLAG}" CMAKE_C_FLAGS)


### C++ flags
# Note CMAKE_CXX_FLAGS is also send to the linker.
# By default allow build on amd64 machine
if(DEFINED USER_CMAKE_CXX_FLAGS)
    message(STATUS "Pcsx2 is very sensible with gcc flags, so use USER_CMAKE_CXX_FLAGS at your own risk !!!")
    string(STRIP "${USER_CMAKE_CXX_FLAGS}" CMAKE_CXX_FLAGS)
endif()
# Use some default machine flags
string(STRIP "${CMAKE_CXX_FLAGS} ${DEFAULT_CPP_FLAG}" CMAKE_CXX_FLAGS)

#-------------------------------------------------------------------------------
# MacOS-specific things
#-------------------------------------------------------------------------------

set(CMAKE_OSX_DEPLOYMENT_TARGET 10.9)

# CMake defaults the suffix for modules to .so on macOS but wx tells us that the
# extension is .dylib (so that's what we search for)
if(APPLE)
    set(CMAKE_SHARED_MODULE_SUFFIX ".dylib")
endif()

if(CMAKE_SYSTEM_NAME MATCHES "Darwin")
    if(NOT OSX_USE_DEFAULT_SEARCH_PATH)
        # Hack up the path to prioritize the path to built-in OS libraries to
        # increase the chance of not depending on a bunch of copies of them
        # installed by MacPorts, Fink, Homebrew, etc, and ending up copying
        # them into the bundle.  Since we depend on libraries which are not 
        # part of OS X (wx, etc.), however, don't remove the default path 
        # entirely.  This is still kinda evil, since it defeats the user's 
        # path settings...
        # See http://www.cmake.org/cmake/help/v3.0/command/find_program.html
        list(APPEND CMAKE_PREFIX_PATH "/usr")
    endif()

    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-dead_strip,-dead_strip_dylibs")
    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -Wl,-dead_strip,-dead_strip_dylibs")
endif()
