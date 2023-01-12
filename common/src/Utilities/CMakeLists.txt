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
		FastJmp.cpp
		Mutex.cpp
		PathUtils.cpp
		pxStreams.cpp
		StringHelpers.cpp
		Semaphore.cpp
		AlignedMalloc.cpp
		HostSys.cpp
		)

# variable with all headers of this library
set(UtilitiesHeaders
	../../include/Utilities/boost_spsc_queue.hpp
	../../include/Utilities/Dependencies.h
	../../include/Utilities/EventSource.h
	../../include/Utilities/Exceptions.h
	../../include/Utilities/FastJmp.h
	../../include/Utilities/FixedPointTypes.h
	../../include/Utilities/General.h
	../../include/Utilities/MemcpyFast.h
	../../include/Utilities/MemsetFast.inl
	../../include/Utilities/Path.h
	../../include/Utilities/PageFaultSource.h
	../../include/Utilities/pxForwardDefs.h
	../../include/Utilities/pxStreams.h
	../../include/Utilities/RedtapeWindows.h
	../../include/Utilities/SafeArray.h
	../../include/Utilities/ScopedAlloc.h
	../../include/Utilities/StringHelpers.h
	../../include/Utilities/Threading.h
)

if(MSVC)
   enable_language(ASM_MASM)
set(UtilitiesFinalSources
	${UtilitiesSources}
	${UtilitiesHeaders}
        FastJmp.asm
)
else()
set(UtilitiesFinalSources
	${UtilitiesSources}
	${UtilitiesHeaders}
)
endif()


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
