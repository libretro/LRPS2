if (openSUSE)
    # openSUSE don't install wx in a standard library system
    # path. Let's bypass the dynamic linker and hardcode the path.
    set(CMAKE_INSTALL_RPATH_USE_LINK_PATH ON)
endif()

# Check that people use the good file
if(NOT TOP_CMAKE_WAS_SOURCED)
    message(FATAL_ERROR "
    You did not 'cmake' the good CMakeLists.txt file. Use the one in the top dir.
    It is advice to delete all wrongly generated cmake stuff => CMakeFiles & CMakeCache.txt")
endif()


set(CommonFlags
    -fno-strict-aliasing
    -Wstrict-aliasing # Allow to track strict aliasing issue.
    -Wno-parentheses
    -Wno-missing-braces
    -Werror=date-time
	)

if ("${PGO}" STREQUAL "generate")
    set(pcsx2FinalFlags -fprofile-generate)
elseif("${PGO}" STREQUAL "use")
    set(pcsx2FinalFlags -fprofile-use)
else()
    set(pcsx2FinalFlags "")
endif()

set(pcsx2FinalFlags ${pcsx2FinalFlags} ${CommonFlags})

if(XDG_STD)
    set(pcsx2FinalFlags ${pcsx2FinalFlags} -DXDG_STD)
endif()

set(Output pcsx2_libretro)

# Main pcsx2 source
set(pcsx2Sources
	Cache.cpp
	COP0.cpp
	COP2.cpp
	Counters.cpp
	GameDatabase.cpp
	Elfheader.cpp
	FW.cpp
	FiFo.cpp
	FPU.cpp
	Gif.cpp
	Gif_Unit.cpp
	GS.cpp
	GSState.cpp
	Hw.cpp
	HwRead.cpp
	HwWrite.cpp
	Interpreter.cpp
	IopBios.cpp
	IopCounters.cpp
	IopDma.cpp
	IopGte.cpp
	IopHw.cpp
	IopIrq.cpp
	IopMem.cpp
	IopSio2.cpp
	IPC.cpp
	Mdec.cpp
	Memory.cpp
	MMI.cpp
	MTGS.cpp
	MTVU.cpp
	MultipartFileReader.cpp
	Patch.cpp
	Patch_Memory.cpp
	Pcsx2Config.cpp
	PrecompiledHeader.cpp
	R3000A.cpp
	R3000AInterpreter.cpp
	R3000AOpcodeTables.cpp
	R5900.cpp
	R5900OpcodeImpl.cpp
	R5900OpcodeTables.cpp
	SaveState.cpp
	ShiftJisToUnicode.cpp
	Sif.cpp
	Sif0.cpp
	Sif1.cpp
	sif2.cpp
	Sio.cpp
	SPR.cpp
	System.cpp
	Vif0_Dma.cpp
	Vif1_Dma.cpp
	Vif1_MFIFO.cpp
	Vif.cpp
	Vif_Codes.cpp
	Vif_Transfer.cpp
	Vif_Unpack.cpp
	vtlb.cpp
	VU0.cpp
	VUmicro.cpp
	VU0micro.cpp
	VU0microInterp.cpp
	VU1micro.cpp
	VU1microInterp.cpp
	VUflags.cpp
	VUmicroMem.cpp
	VUops.cpp
	MemoryPatchDatabase.cpp)

# Main pcsx2 header
set(pcsx2Headers
	AsyncFileReader.h
	Cache.h
	cheatscpp.h
	Common.h
	Config.h
	COP0.h
	Counters.h
	Dmac.h
	GameDatabase.h
	Elfheader.h
	FW.h
	Gif.h
	Gif_Unit.h
	GS.h
	Hardware.h
	Hw.h
	IopBios.h
	IopCommon.h
	IopCounters.h
	IopDma.h
	IopGte.h
	IopHw.h
	IopMem.h
	IopSio2.h
	IPC.h
	Mdec.h
	MTVU.h
	Memory.h
	MemoryTypes.h
	Patch.h
	PathDefs.h
	Plugins.h
	PrecompiledHeader.h
	R3000A.h
	R5900Exceptions.h
	R5900.h
	R5900OpcodeTables.h
	SaveState.h
	Sifcmd.h
	Sif.h
	Sio.h
	sio_internal.h
	SPR.h
	SysForwardDefs.h
	System.h
	Vif_Dma.h
	Vif.h
	Vif_Unpack.h
	vtlb.h
	VUflags.h
	VUmicro.h
	VUops.h
	MemoryPatchDatabase.h)

# CDVD sources
set(pcsx2CDVDSources
	CDVD/BlockdumpFileReader.cpp
	CDVD/CdRom.cpp
	CDVD/CDVDaccess.cpp
	CDVD/CDVD.cpp
	CDVD/CDVDdiscReader.cpp
	CDVD/CDVDisoReader.cpp
	CDVD/CDVDdiscThread.cpp
	CDVD/InputIsoFile.cpp
	CDVD/OutputIsoFile.cpp
	CDVD/ChunksCache.cpp
	CDVD/CompressedFileReader.cpp
	CDVD/ChdFileReader.cpp
	CDVD/CsoFileReader.cpp
	CDVD/GzippedFileReader.cpp
	CDVD/IsoFS/IsoFile.cpp
	CDVD/IsoFS/IsoFSCDVD.cpp
	CDVD/IsoFS/IsoFS.cpp
    )

# CDVD headers
set(pcsx2CDVDHeaders
	CDVD/CdRom.h
	CDVD/CDVDaccess.h
	CDVD/CDVD.h
	CDVD/CDVD_internal.h
	CDVD/CDVDdiscReader.h
	CDVD/CDVDisoReader.h
	CDVD/ChunksCache.h
	CDVD/CompressedFileReader.h
	CDVD/CompressedFileReaderUtils.h
	CDVD/ChdFileReader.h
	CDVD/CsoFileReader.h
	CDVD/GzippedFileReader.h
	CDVD/IsoFileFormats.h
	CDVD/IsoFS/IsoDirectory.h
	CDVD/IsoFS/IsoFileDescriptor.h
	CDVD/IsoFS/IsoFile.h
	CDVD/IsoFS/IsoFSCDVD.h
	CDVD/IsoFS/IsoFS.h
	CDVD/IsoFS/SectorSource.h
	CDVD/zlib_indexed.h
	)

	# SPU2 sources
	set(pcsx2SPU2Sources
      SPU2/ADSR.cpp
      SPU2/DplIIdecoder.cpp
      SPU2/Dma.cpp
      SPU2/Mixer.cpp
      SPU2/spu2.cpp
      SPU2/ReadInput.cpp
      SPU2/RegTable.cpp
      SPU2/Reverb.cpp
      SPU2/spu2freeze.cpp
      SPU2/spu2sys.cpp
		 )

# SPU2 headers
set(pcsx2SPU2Headers
   SPU2/Config.h
   SPU2/defs.h
   SPU2/Dma.h
   SPU2/Global.h
   SPU2/Mixer.h
   SPU2/spu2.h
   SPU2/regs.h
   SPU2/SndOut.h
   SPU2/spdif.h
)


# DebugTools sources
set(pcsx2DebugToolsSources
	DebugTools/DebugInterface.cpp
	DebugTools/DisassemblyManager.cpp
	DebugTools/ExpressionParser.cpp
	DebugTools/MIPSAnalyst.cpp
	DebugTools/MipsAssembler.cpp
	DebugTools/MipsAssemblerTables.cpp
	DebugTools/MipsStackWalk.cpp
	DebugTools/Breakpoints.cpp
	DebugTools/SymbolMap.cpp
	DebugTools/DisR3000A.cpp
	DebugTools/DisR5900asm.cpp
	DebugTools/DisVU0Micro.cpp
	DebugTools/DisVU1Micro.cpp
	DebugTools/BiosDebugData.cpp)

# DebugTools headers
set(pcsx2DebugToolsHeaders
	DebugTools/DebugInterface.h
	DebugTools/DisassemblyManager.h
	DebugTools/ExpressionParser.h
	DebugTools/MIPSAnalyst.h
	DebugTools/MipsAssembler.h
	DebugTools/MipsAssemblerTables.h
	DebugTools/MipsStackWalk.h
	DebugTools/Breakpoints.h
	DebugTools/SymbolMap.h
	DebugTools/Debug.h
	DebugTools/DisASM.h
	DebugTools/DisVUmicro.h
	DebugTools/DisVUops.h
	DebugTools/BiosDebugData.h)

	# gui sources
	set(pcsx2GuiSources
		gui/AppConfig.cpp
		gui/AppCoreThread.cpp
		gui/AppEventSources.cpp
		gui/AppGameDatabase.cpp
		gui/AppInit.cpp
		gui/AppMain.cpp
		gui/AppRes.cpp
		gui/MemoryCardFile.cpp
		)
	# gui headers
	set(pcsx2GuiHeaders
		gui/AppCommon.h
		gui/AppConfig.h
		gui/AppCoreThread.h
		gui/AppEventListeners.h
		gui/AppForwardDefs.h
		gui/AppGameDatabase.h
		gui/App.h
		gui/MemoryCardFile.h
		)

set(db_res_src "${CMAKE_SOURCE_DIR}/resources")
set(db_res_bin "${CMAKE_BINARY_DIR}/resources")

file(MAKE_DIRECTORY ${db_res_bin})
file(REMOVE ${db_res_bin}/cheats_ws.h)
file(REMOVE ${db_res_bin}/cheats_nointerlacing.h)
file(REMOVE ${db_res_bin}/GameIndex.h)

set(db_resources
		${db_res_bin}/cheats_ws.h
		${db_res_bin}/cheats_nointerlacing.h
		${db_res_bin}/GameIndex.h
		)

find_program(XXD xxd)
if (XXD)
	message("-- Found xxd. GameIndex.h, cheats_ws.h and cheats_nointerlacing.h will be generated.")
	
	add_custom_command(
		OUTPUT ${db_res_bin}/cheats_ws.h
		COMMAND ${XXD} -i cheats_ws.zip cheats_ws.h
		COMMAND cp cheats_ws.h ${db_res_bin}/cheats_ws.h
		WORKING_DIRECTORY ${db_res_src}
		VERBATIM
	)
	add_custom_command(
		OUTPUT ${db_res_bin}/cheats_nointerlacing.h
		COMMAND ${XXD} -i cheats_nointerlacing.zip cheats_nointerlacing.h
		COMMAND cp cheats_nointerlacing.h ${db_res_bin}/cheats_nointerlacing.h
		WORKING_DIRECTORY ${db_res_src}
		VERBATIM
	)
	add_custom_command(
		OUTPUT  ${db_res_bin}/GameIndex.h
		COMMAND ${XXD} -i GameIndex.yaml GameIndex.h
		COMMAND cp GameIndex.h  ${db_res_bin}/GameIndex.h
		WORKING_DIRECTORY  ${db_res_src}
		VERBATIM
	)
else()
	message("Could NOT find xxd. Falling back on GameIndex.h, cheats_ws.h, and cheats_nointerlacing.h in ${db_res_src}.
	Warning: if changes were made to GameIndex.yaml, cheats_ws.zip or cheats_nointerlacing.zip, this build script will
	need to be run again with xxd installed; otherwise, you may safely ignore this warning.")
	add_custom_command(
		OUTPUT ${db_res_bin}/cheats_ws.h
		COMMAND ${CMAKE_COMMAND} -E copy cheats_ws.h ${db_res_bin}/cheats_ws.h
		WORKING_DIRECTORY ${db_res_src}
		VERBATIM
	)
	add_custom_command(
		OUTPUT ${db_res_bin}/cheats_nointerlacing.h
		COMMAND ${CMAKE_COMMAND} -E copy cheats_nointerlacing.h ${db_res_bin}/cheats_nointerlacing.h
		WORKING_DIRECTORY ${db_res_src}
		VERBATIM
	)
	add_custom_command(
		OUTPUT  ${db_res_bin}/GameIndex.h
		COMMAND ${CMAKE_COMMAND} -E copy GameIndex.h  ${db_res_bin}/GameIndex.h
		WORKING_DIRECTORY  ${db_res_src}
		VERBATIM
	)

endif()

# IPU sources
set(pcsx2IPUSources
	IPU/IPU.cpp
	IPU/IPU_Fifo.cpp
	IPU/IPUdither.cpp
	IPU/IPUdma.cpp
	IPU/mpeg2lib/Idct.cpp
	IPU/mpeg2lib/Mpeg.cpp
	IPU/yuv2rgb.cpp)

# IPU headers
set(pcsx2IPUHeaders
	IPU/IPUdma.h
	IPU/IPU_Fifo.h
	IPU/IPU.h
	IPU/mpeg2lib/Mpeg.h
	IPU/mpeg2lib/Vlc.h
	IPU/yuv2rgb.h
	)

# Linux sources
set(pcsx2LinuxSources
	CDVD/Linux/DriveUtility.cpp
	CDVD/Linux/IOCtlSrc.cpp
	Linux/LnxFlatFileReader.cpp
   )

set(pcsx2OSXSources
	CDVD/Linux/DriveUtility.cpp
	CDVD/Linux/IOCtlSrc.cpp
	Darwin/DarwinFlatFileReader.cpp
	)

set(pcsx2FreeBSDSources
	CDVD/Linux/DriveUtility.cpp
	CDVD/Linux/IOCtlSrc.cpp
	Darwin/DarwinFlatFileReader.cpp
	)

# Linux headers
set(pcsx2LinuxHeaders
	)

# ps2 sources
set(pcsx2ps2Sources
	ps2/BiosTools.cpp
	ps2/pgif.cpp
	ps2/LegacyDmac.cpp
	ps2/Iop/IopHwRead.cpp
	ps2/Iop/IopHwWrite.cpp
	ps2/Iop/PsxBios.cpp)

# ps2 headers
set(pcsx2ps2Headers
	ps2/BiosTools.h
	ps2/pgif.h
	ps2/HwInternal.h
	ps2/Iop/IopHw_Internal.h)

# System sources
set(pcsx2SystemSources
	System/SysCoreThread.cpp
	System/SysThreadBase.cpp
	System/SysFakeThread.cpp)

# System headers
set(pcsx2SystemHeaders
	System/RecTypes.h
	System/SysThreads.h)

# Utilities sources
set(pcsx2UtilitiesSources
	Utilities/FileUtils.cpp)

# Windows sources
set(pcsx2WindowsSources
	CDVD/Windows/DriveUtility.cpp
	CDVD/Windows/IOCtlSrc.cpp
	windows/FlatFileReaderWindows.cpp
	windows/VCprojects/IopSif.cpp
	windows/WinCompressNTFS.cpp
	)
# Windows headers
set(pcsx2WindowsHeaders
	windows/resource.h
	windows/Win32.h)

# x86 sources
set(pcsx2x86Sources
	x86/BaseblockEx.cpp
	x86/iCOP0.cpp
	x86/iCore.cpp
	x86/iFPU.cpp
	x86/iFPUd.cpp
	x86/iMisc.cpp
	x86/iMMI.cpp
	x86/iR3000A.cpp
	x86/iR3000Atables.cpp
	x86/iR5900Misc.cpp
	x86/ir5900tables.cpp
	x86/ix86-32/iCore-32.cpp
	x86/ix86-32/iR5900-32.cpp
	x86/ix86-32/iR5900Arit.cpp
	x86/ix86-32/iR5900AritImm.cpp
	x86/ix86-32/iR5900Branch.cpp
	x86/ix86-32/iR5900Jump.cpp
	x86/ix86-32/iR5900LoadStore.cpp
	x86/ix86-32/iR5900Move.cpp
	x86/ix86-32/iR5900MultDiv.cpp
	x86/ix86-32/iR5900Shift.cpp
	x86/ix86-32/iR5900Templates.cpp
	x86/ix86-32/recVTLB.cpp
	x86/newVif_Dynarec.cpp
	x86/newVif_Unpack.cpp
	x86/newVif_UnpackSSE.cpp
	)

# x86 headers
set(pcsx2x86Headers
	x86/BaseblockEx.h
	x86/iCOP0.h
	x86/iCore.h
	x86/iFPU.h
	x86/iMMI.h
	x86/iR3000A.h
	x86/iR5900Arit.h
	x86/iR5900AritImm.h
	x86/iR5900Branch.h
	x86/iR5900.h
	x86/iR5900Jump.h
	x86/iR5900LoadStore.h
	x86/iR5900Move.h
	x86/iR5900MultDiv.h
	x86/iR5900Shift.h
	x86/microVU_Alloc.inl
	x86/microVU_Analyze.inl
	x86/microVU_Branch.inl
	x86/microVU_Clamp.inl
	x86/microVU_Compile.inl
	x86/microVU.cpp
	x86/microVU_Execute.inl
	x86/microVU_Flags.inl
	x86/microVU.h
	x86/microVU_IR.h
	x86/microVU_Lower.inl
	x86/microVU_Macro.inl
	x86/microVU_Misc.h
	x86/microVU_Misc.inl
	x86/microVU_Profiler.h
	x86/microVU_Tables.inl
	x86/microVU_Upper.inl
	x86/newVif.h
	x86/newVif_HashBucket.h
	x86/newVif_UnpackSSE.h
	x86/R5900_Profiler.h
	)

# common Sources
	set(Common
		${db_resources}
		${pcsx2Sources}
		${pcsx2Headers}
		${pcsx2CDVDSources}
		${pcsx2CDVDHeaders}
		${pcsx2SPU2Sources}
		${pcsx2SPU2Headers}
		${pcsx2DebugToolsSources}
		${pcsx2GuiSources}
		${pcsx2GuiHeaders}
		${pcsx2IPUSources}
		${pcsx2IPUHeaders}
		${pcsx2ps2Sources}
		${pcsx2ps2Headers}
		${pcsx2SystemSources}
		${pcsx2SystemHeaders}
		${pcsx2UtilitiesSources}
		${pcsx2x86Sources}
		${pcsx2x86Headers})

# platform sources
# Linux
if(Linux)
	set(Platform
		${pcsx2LinuxSources}
		${pcsx2LinuxHeaders}
		)
		
	set(Platform_Libs
		${LIBUDEV_LIBRARIES}
		)
endif()

# Windows
if(Windows)
	set(Platform
		${pcsx2WindowsSources}
		${pcsx2WindowsHeaders})
endif()

# MacOSX
if(APPLE)
	set(Platform
		${pcsx2OSXSources}
		${pcsx2LinuxHeaders})
endif()

if(${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD" OR ${CMAKE_SYSTEM_NAME} MATCHES "NetBSD")
	set(Platform
		${pcsx2FreeBSDSources}
		${pcsx2LinuxHeaders})
endif()

set(pcsx2FinalSources
	${Common}
	${Platform}
)

set(pcsx2FinalLibs
    Utilities
    x86emitter
    ${wxWidgets_LIBRARIES}
    ${GLIB_LIBRARIES}
    ${GLIB_GIO_LIBRARIES}
    ${ZLIB_LIBRARIES}
    ${AIO_LIBRARIES}
    ${GCOV_LIBRARIES}
    ${Platform_Libs}
)

set(pcsx2FinalLibs GS ${pcsx2FinalLibs})
if(MSVC)
  set(pcsx2FinalSources ${pcsx2FinalSources})
endif()
set(pcsx2FinalLibs ${pcsx2FinalLibs} PAD)
set(pcsx2FinalLibs ${pcsx2FinalLibs} USB)
if(ENABLE_DEV9GHZDRK)
    set(pcsx2FinalLibs ${pcsx2FinalLibs} DEV9)
else()
    set(pcsx2FinalLibs ${pcsx2FinalLibs} DEV9)
endif()

# additional include directories
include_directories(
    gui
    x86
    ${db_res_bin}
    ${CMAKE_BINARY_DIR}/pcsx2/gui
)

if(MSVC)
#   include_directories(x86/ix86-32 IPU System)
   add_definitions(-D_M_X86 -D__i386__ -DTIXML_USE_STL -D_SCL_SECURE_NO_WARNINGS)
   add_definitions(-D_ARCH_64=1 -D_M_X86_64 -D__M_X86_64) #64-bit
#   add_definitions(-D_ARCH_32=1 -D_M_X86_32) #32-bit
   set(pcsx2FinalLibs ${pcsx2FinalLibs})
   set(pcsx2FinalLibs ${pcsx2FinalLibs} comctl32 ws2_32 shlwapi winmm rpcrt4)
endif()

#   add_link_options(-fuse-ld=gold)
#   add_link_options(-Wl,--gc-sections,--print-symbol-counts,sym.log)

   add_library(${Output} SHARED
     ${CMAKE_SOURCE_DIR}/libretro/main.cpp
     ${pcsx2FinalSources}
    "../libretro/language_injector.cpp" "../libretro/retro_messager.cpp" "EmuOptionsVM.h" "EmuOptionsUI.h")
   include_directories(. ${CMAKE_SOURCE_DIR}/libretro)
#   set(LIBRARY_OUTPUT_PATH "${CMAKE_BINARY_DIR}")
   set_target_properties(pcsx2_libretro PROPERTIES PREFIX "")

   if(CMAKE_C_COMPILER_ID MATCHES "Clang")
      set(CLANG 1)
   endif()

   if(NOT MSVC AND NOT CLANG)
      set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--no-undefined")
   endif()

   if (MSVC)
      set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /DELAYLOAD:OpenGL32.dll")
	  set(pcsx2FinalLibs ${pcsx2FinalLibs} Delayimp.lib)
   endif()
   
   target_link_libraries(pcsx2_libretro PRIVATE ${pcsx2FinalLibs})
   if(PACKAGE_MODE)
       install(TARGETS ${Output} DESTINATION ${BIN_DIR})
   else(PACKAGE_MODE)
       install(TARGETS ${Output} DESTINATION ${CMAKE_SOURCE_DIR}/bin)
   endif(PACKAGE_MODE)
target_compile_features(${Output} PRIVATE cxx_std_17)

#if(COMMAND target_precompile_headers)
#	message("Using precompiled headers.")
#	target_precompile_headers(${Output} PRIVATE PrecompiledHeader.h)
#endif()

if (APPLE)
	# MacOS defaults to having a maximum protection of the __DATA segment of rw (non-executable)
	# We have a bunch of page-sized arrays in bss that we use for jit
	# Obviously not being able to make those arrays executable would be a problem
	target_link_options(${Output} PRIVATE -Wl,-segprot,__DATA,rwx,rw)

	set_target_properties(${Output} PROPERTIES
		MACOSX_BUNDLE true
		MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/gui/Resources/Info.plist.in"
		OUTPUT_NAME PCSX2
	)

	target_sources(${Output} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/gui/Resources/PCSX2.icns")
	set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/gui/Resources/PCSX2.icns" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)

	# If they say to skip postprocess bundle, leave the target in but make it so they have
	# to manually run it
	if (SKIP_POSTPROCESS_BUNDLE)
		set(postprocessBundleType "")
	else()
		set(postprocessBundleType ALL)
	endif()
	# Use custom_target and not custom_command so plugins can add themselves as dependencies
	add_custom_target(pcsx2-postprocess-bundle ${postprocessBundleType}
		COMMAND ${CMAKE_COMMAND} "-DPCSX2_BUNDLE_PATH=$<TARGET_FILE_DIR:${Output}>/../.."
			-P ${CMAKE_SOURCE_DIR}/cmake/Pcsx2PostprocessBundle.cmake
	)
	add_dependencies(pcsx2-postprocess-bundle ${Output})
endif()

#if(dev9ghzdrk)
#    if(PACKAGE_MODE)
#        install(CODE "execute_process(COMMAND /bin/bash -c \"echo 'Enabling networking capability on Linux...';set -x; [ -f ${BIN_DIR}/${Output} ] && sudo setcap 'CAP_NET_RAW+eip CAP_NET_ADMIN+eip' ${BIN_DIR}/${Output}; set +x\")")
#    else()
#        install(CODE "execute_process(COMMAND /bin/bash -c \"echo 'Enabling networking capability on Linux...';set -x; [ -f ${CMAKE_SOURCE_DIR}/bin/${Output} ] && sudo setcap 'CAP_NET_RAW+eip CAP_NET_ADMIN+eip' ${CMAKE_SOURCE_DIR}/bin/${Output}; set +x\")")
#    endif()
#endif()
