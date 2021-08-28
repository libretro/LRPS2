
add_library(pcsx2_libretro SHARED
  main.cpp
  options.cpp
)

include_directories(${CMAKE_SOURCE_DIR}/pcsx2)
include_directories(${CMAKE_SOURCE_DIR}/pcsx2/gui)
include_directories(${CMAKE_SOURCE_DIR}/libretro)

set(LIBRARY_OUTPUT_PATH "${CMAKE_BINARY_DIR}")

set_target_properties(pcsx2_libretro PROPERTIES PREFIX "")
if(ANDROID)
   target_sources(pcsx2_libretro PRIVATE
   )
   set_target_properties(pcsx2_libretro PROPERTIES SUFFIX "_android.so")
endif()

if(CMAKE_C_COMPILER_ID MATCHES "Clang")
   set(CLANG 1)
endif()

if(NOT MSVC AND NOT CLANG)
   set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--no-undefined")
endif()

if(LIBRETRO_STATIC)
   target_link_libraries(pcsx2_libretro PRIVATE
      pcsx2Core
      Utilities
      x86emitter
      ${wxWidgets_LIBRARIES}
      ${ZLIB_LIBRARIES}
      ${AIO_LIBRARIES}
      ${GCOV_LIBRARIES}
      ${LIBUDEV_LIBRARIES}
      "${pcsx2FinalFlags}"
      "-static-libgcc -static-libstdc++"
   )
else()
   target_link_libraries(pcsx2_libretro PRIVATE
      pcsx2Core
      ${pcsx2FinalLibs}
      "${pcsx2FinalFlags}"
   )
endif()
