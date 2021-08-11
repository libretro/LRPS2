# Check that people use the good file
if(NOT TOP_CMAKE_WAS_SOURCED)
    message(FATAL_ERROR "
    You did not 'cmake' the good CMakeLists.txt file. Use the one in the top dir.
    It is advice to delete all wrongly generated cmake stuff => CMakeFiles & CMakeCache.txt")
endif(NOT TOP_CMAKE_WAS_SOURCED)


set(Output Utilities)

set(CommonFlags
    )

set(UtilitiesFinalFlags ${CommonFlags})

# variable with all sources of this library
	set(UtilitiesSources
		VirtualMemory.cpp
		../../include/Utilities/FixedPointTypes.inl
		../../include/Utilities/EventSource.inl
		../../include/Utilities/SafeArray.inl
		Exceptions.cpp
		FastFormatString.cpp
		Mutex.cpp
		PathUtils.cpp
		PrecompiledHeader.cpp
		pxStreams.cpp
		StringHelpers.cpp
		ThreadTools.cpp
      		wxAppWithHelpers.cpp
		)
   if(NOT MSVC)
   set(UtilitiesSources ${UtilitiesSources}
      AlignedMalloc.cpp
      Linux/LnxHostSys.cpp)
   endif()

# variable with all headers of this library
set(UtilitiesHeaders
	../../include/Utilities/Assertions.h
	../../include/Utilities/boost_spsc_queue.hpp
	../../include/Utilities/Dependencies.h
	../../include/Utilities/EventSource.h
	../../include/Utilities/Exceptions.h
	../../include/Utilities/FixedPointTypes.h
	../../include/Utilities/General.h
	../../include/Utilities/MakeUnique.h
	../../include/Utilities/MemcpyFast.h
	../../include/Utilities/MemsetFast.inl
	../../include/Utilities/Path.h
	../../include/Utilities/PageFaultSource.h
	../../include/Utilities/pxForwardDefs.h
	../../include/Utilities/pxStreams.h
	../../include/Utilities/RedtapeWindows.h
	../../include/Utilities/SafeArray.h
	../../include/Utilities/ScopedAlloc.h
	../../include/Utilities/ScopedPtrMT.h
	../../include/Utilities/StringHelpers.h
	../../include/Utilities/Threading.h
	../../include/Utilities/wxAppWithHelpers.h
	PrecompiledHeader.h
	ThreadingInternal.h
)

if(APPLE)
	LIST(APPEND UtilitiesSources
		Darwin/DarwinThreads.cpp
		Darwin/DarwinSemaphore.cpp
	)
elseif(Windows)
	LIST(APPEND UtilitiesSources
		Windows/WinThreads.cpp
		Windows/WinHostSys.cpp
		Windows/WinMisc.cpp
      Semaphore.cpp
	)
# if(x86)
#   LIST(APPEND UtilitiesSources
#		x86/MemcpyFast.cpp
#	)
# endif()

else()
	LIST(APPEND UtilitiesSources
		Linux/LnxThreads.cpp
		Semaphore.cpp
	)
endif()

set(UtilitiesFinalSources
	${UtilitiesSources}
	${UtilitiesHeaders}
)

set(UtilitiesFinalLibs
    ${LIBC_LIBRARIES} # Gold (new linux linker) does not get automatically dependency of dependency
	${wxWidgets_LIBRARIES}
	yaml-cpp
	chdr-static
)

if(MSVC)
   set(UtilitiesFinalLibs ${UtilitiesFinalLibs} pthread)
endif()


add_pcsx2_lib(${Output} "${UtilitiesFinalSources}" "${UtilitiesFinalLibs}" "${UtilitiesFinalFlags}")
add_pcsx2_lib(${Output}_NO_TLS "${UtilitiesFinalSources}" "${UtilitiesFinalLibs}" "${UtilitiesFinalFlags} -DPCSX2_THREAD_LOCAL=0")

#if(COMMAND target_precompile_headers)
#	target_precompile_headers(${Output} PRIVATE PrecompiledHeader.h)
#	target_precompile_headers(${Output}_NO_TLS PRIVATE PrecompiledHeader.h)
#endif()
