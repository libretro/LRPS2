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

if (NOT MSVC)
set(CommonFlags
    -Wno-unknown-pragmas
    -Wno-parentheses
    -Wunused-variable # __dummy variable need to be investigated
    )
endif()

# The next two need to be looked at, but spam really badly in gcc 8.
# Largely class alignment in GSDevice.h and memcpy in GSVector*.h.
if(GCC_VERSION VERSION_EQUAL "8.0" OR GCC_VERSION VERSION_GREATER "8.0")
    set(CommonFlags
	    ${CommonFlags}
        -Wno-class-memaccess
        -Wno-packed-not-aligned
    )
endif()

if ("${PGO}" STREQUAL "generate")
    set(pcsx2FinalFlags -fprofile-generate)
elseif("${PGO}" STREQUAL "use")
    set(pcsx2FinalFlags -fprofile-use)
else()
    set(pcsx2FinalFlags "")
endif()

set(pcsx2FinalFlags ${pcsx2FinalFlags} ${CommonFlags})

set(Output pcsx2_libretro)

# Main pcsx2 source
set(pcsx2Sources
	Cache.cpp
	COP0.cpp
	COP2.cpp
	Counters.cpp
	Dmac.cpp
	GameDatabase.cpp
	Elfheader.cpp
	FW.cpp
	FiFo.cpp
	FPU.cpp
	Gif.cpp
	Gif_Unit.cpp
	GS.cpp
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
	Mdec.cpp
	Memory.cpp
	MMI.cpp
	MTGS.cpp
	MTVU.cpp
	MultipartFileReader.cpp
	FlatFileReader.cpp
	Patch.cpp
	Patch_Memory.cpp
	Pcsx2Config.cpp
	R3000A.cpp
	R3000AInterpreter.cpp
	R3000AOpcodeTables.cpp
	R5900.cpp
	R5900OpcodeImpl.cpp
	R5900OpcodeTables.cpp
	SaveState.cpp
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
	VUmicroMem.cpp
	VUops.cpp
	MemoryPatchDatabase.cpp)

# Main pcsx2 header
set(pcsx2Headers
	AsyncFileReader.h
	Cache.h
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
	Hw.h
	IopBios.h
	IopCounters.h
	IopDma.h
	IopGte.h
	IopHw.h
	IopMem.h
	IopSio2.h
	Mdec.h
	MTVU.h
	Memory.h
	MemoryTypes.h
	Patch.h
	PathDefs.h
	Plugins.h
	R3000A.h
	R5900Exceptions.h
	R5900.h
	R5900OpcodeTables.h
	SaveState.h
	Sif.h
	Sio.h
	sio_internal.h
	SPR.h
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
	CDVD/DriveUtility.cpp
	CDVD/IOCtlSrc.cpp
	CDVD/CDVDaccess.cpp
	CDVD/CDVD.cpp
	CDVD/CDVDdiscReader.cpp
	CDVD/CDVDisoReader.cpp
	CDVD/CDVDdiscThread.cpp
	CDVD/InputIsoFile.cpp
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
   SPU2/defs.h
   SPU2/Global.h
   SPU2/Mixer.h
   SPU2/spu2.h
   SPU2/regs.h
   SPU2/SndOut.h
)

# GS sources
set(pcsx2GSSources
    GS/GS.cpp
    GS/GSBlock.cpp
    GS/GSClut.cpp
    GS/GSCodeBuffer.cpp
    GS/GSCrc.cpp
    GS/GSDrawingContext.cpp
    GS/GSLocalMemory.cpp
    GS/GSState.cpp
    GS/GSTables.cpp
    GS/GSUtil.cpp
    GS/GSVector.cpp
    GS/Renderers/Common/GSDevice.cpp
    GS/Renderers/Common/GSDirtyRect.cpp
    GS/Renderers/Common/GSRenderer.cpp
    GS/Renderers/Common/GSTexture.cpp
    GS/Renderers/Common/GSVertexTrace.cpp
    GS/Renderers/HW/GSHwHack.cpp
    GS/Renderers/HW/GSRendererHW.cpp
    GS/Renderers/HW/GSTextureCache.cpp
    GS/Renderers/SW/GSDrawScanline.cpp
    GS/Renderers/SW/GSDrawScanlineCodeGenerator.cpp
    GS/Renderers/SW/GSDrawScanlineCodeGenerator.x64.cpp
    GS/Renderers/SW/GSDrawScanlineCodeGenerator.x64.avx.cpp
    GS/Renderers/SW/GSDrawScanlineCodeGenerator.x64.avx2.cpp
    GS/Renderers/SW/GSDrawScanlineCodeGenerator.x86.cpp
    GS/Renderers/SW/GSDrawScanlineCodeGenerator.x86.avx.cpp
    GS/Renderers/SW/GSDrawScanlineCodeGenerator.x86.avx2.cpp
    GS/Renderers/SW/GSRasterizer.cpp
    GS/Renderers/SW/GSRendererSW.cpp
    GS/Renderers/SW/GSSetupPrimCodeGenerator.cpp
    GS/Renderers/SW/GSSetupPrimCodeGenerator.x64.cpp
    GS/Renderers/SW/GSSetupPrimCodeGenerator.x64.avx.cpp
    GS/Renderers/SW/GSSetupPrimCodeGenerator.x64.avx2.cpp
    GS/Renderers/SW/GSSetupPrimCodeGenerator.x86.cpp
    GS/Renderers/SW/GSSetupPrimCodeGenerator.x86.avx.cpp
    GS/Renderers/SW/GSSetupPrimCodeGenerator.x86.avx2.cpp
    GS/Renderers/SW/GSTextureCacheSW.cpp
    GS/Renderers/SW/GSTextureSW.cpp
    GS/Renderers/SW/GSVertexSW.cpp
    GS/Renderers/OpenGL/GLLoader.cpp
    GS/Renderers/OpenGL/GLState.cpp
    GS/Renderers/OpenGL/GSDeviceOGL.cpp
    GS/Renderers/OpenGL/GSRendererOGL.cpp
    GS/Renderers/OpenGL/GSShaderOGL.cpp
    GS/Renderers/OpenGL/GSTextureCacheOGL.cpp
    GS/Renderers/OpenGL/GSTextureOGL.cpp
    )

# GS headers 
set(pcsx2GSHeaders
    GS/GSAlignedClass.h
    GS/GSBlock.h
    GS/GSClut.h
    GS/GSCodeBuffer.h
    GS/GSCrc.h
    GS/GSDrawingContext.h
    GS/GSDrawingEnvironment.h
    GS/GS.h
    GS/GSFuncs.h
    GS/GSLocalMemory.h
    GS/GSState.h
    GS/GSTables.h
    GS/GSUtil.h
    GS/GSVector.h
    GS/stdafx.h
    GS/Renderers/Common/GSDevice.h
    GS/Renderers/Common/GSDirtyRect.h
    GS/Renderers/Common/GSFastList.h
    GS/Renderers/Common/GSFunctionMap.h
    GS/Renderers/Common/GSRenderer.h
    GS/Renderers/Common/GSTexture.h
    GS/Renderers/Common/GSVertex.h
    GS/Renderers/Common/GSVertexTrace.h
    GS/Renderers/HW/GSRendererHW.h
    GS/Renderers/HW/GSTextureCache.h
    GS/Renderers/HW/GSVertexHW.h
    GS/Renderers/SW/GSDrawScanlineCodeGenerator.h
    GS/Renderers/SW/GSDrawScanline.h
    GS/Renderers/SW/GSRasterizer.h
    GS/Renderers/SW/GSRendererSW.h
    GS/Renderers/SW/GSScanlineEnvironment.h
    GS/Renderers/SW/GSSetupPrimCodeGenerator.h
    GS/Renderers/SW/GSTextureCacheSW.h
    GS/Renderers/SW/GSTextureSW.h
    GS/Renderers/SW/GSVertexSW.h
    GS/Renderers/OpenGL/GLLoader.h
    GS/Renderers/OpenGL/GLState.h
    GS/Renderers/OpenGL/GSDeviceOGL.h
    GS/Renderers/OpenGL/GSRendererOGL.h
    GS/Renderers/OpenGL/GSShaderOGL.h
    GS/Renderers/OpenGL/GSTextureCacheOGL.h
    GS/Renderers/OpenGL/GSTextureOGL.h
    GS/Renderers/OpenGL/GSUniformBufferOGL.h
    GS/Renderers/OpenGL/GSVertexArrayOGL.h
    GS/xbyak/xbyak.h
    GS/xbyak/xbyak_mnemonic.h
    GS/xbyak/xbyak_util.h
    )

if(Windows)
    LIST(APPEND pcsx2GSSources
	GS/Renderers/DX11/GSDevice11.cpp
	GS/Renderers/DX11/GSRendererDX11.cpp
	GS/Renderers/DX11/GSTexture11.cpp
	GS/Renderers/DX11/GSTextureCache11.cpp
	GS/Renderers/DX11/GSTextureFX11.cpp
        )

    LIST(APPEND pcsx2GSHeaders
	GS/Renderers/DX11/GSDevice11.h
	GS/Renderers/DX11/GSRendererDX11.h
	GS/Renderers/DX11/GSTexture11.h
	GS/Renderers/DX11/GSTextureCache11.h
        )
endif()

set(GS_LIBRARIES
    ${OPENGL_LIBRARIES}
    ${LIBC_LIBRARIES}
)

if(MSVC)
	set(GS_LIBRARIES ${GS_LIBRARIES} d3dcompiler d3d11 dxgi dxguid winmm strmiids opengl32 comsuppw comctl32)
	add_definitions(/wd4456 /wd4458 /wd4996 /wd4995 /wd4324 /wd4100 /wd4101 /wd4201 /wd4556 /wd4127 /wd4512)
endif()

# PAD sources
set(pcsx2PADSources
    PAD/PAD.cpp
    )

# PAD headers
set(pcsx2DEV9Headers
     DEV9/DEV9.h
     )

# DEV9 sources
set(pcsx2DEV9Sources
    DEV9/DEV9.cpp
    )

# DEV9 headers
set(pcsx2DEV9Headers
     PAD/PAD.h
     )

# USB sources
set(pcsx2USBSources
    USB/USB.cpp
    )

# DEV9 headers
set(pcsx2USBHeaders
     USB/USB.h
     )

# DebugTools sources
set(pcsx2DebugToolsSources
	DebugTools/DebugInterface.cpp
	DebugTools/MIPSAnalyst.cpp
	DebugTools/SymbolMap.cpp
	)

# DebugTools headers
set(pcsx2DebugToolsHeaders
	DebugTools/DebugInterface.h
	DebugTools/MIPSAnalyst.h
	DebugTools/SymbolMap.h
	)

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
		gui/SysCoreThread.cpp
		gui/SysThreadBase.cpp
		gui/SysFakeThread.cpp
		gui/ThreadTools.cpp
		gui/wxAppWithHelpers.cpp
		)
	# gui headers
	set(pcsx2GuiHeaders
		gui/AppConfig.h
		gui/AppCoreThread.h
		gui/AppEventListeners.h
		gui/AppGameDatabase.h
		gui/App.h
		gui/MemoryCardFile.h
		gui/PersistentThread.h
		gui/SysThreads.h
		gui/wxAppWithHelpers.h
		)

set(db_res_src "${CMAKE_SOURCE_DIR}/resources")
set(db_res_bin "${CMAKE_BINARY_DIR}/resources")

file(MAKE_DIRECTORY ${db_res_bin})
file(REMOVE ${db_res_bin}/cheats_ws.h)
file(REMOVE ${db_res_bin}/cheats_nointerlacing.h)
file(REMOVE ${db_res_bin}/cheats_60fps.h)
file(REMOVE ${db_res_bin}/GameIndex.h)

set(db_resources
		${db_res_bin}/cheats_ws.h
		${db_res_bin}/cheats_nointerlacing.h
		${db_res_bin}/cheats_60fps.h
		${db_res_bin}/GameIndex.h
		)

find_program(XXD xxd)
if (XXD)
	message("-- Found xxd. GameIndex.h, cheats_ws.h, cheats_60fps.h and cheats_nointerlacing.h will be generated.")
	
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
		OUTPUT ${db_res_bin}/cheats_60fps.h
		COMMAND ${XXD} -i cheats_60fps.zip cheats_60fps.h
		COMMAND cp cheats_60fps.h ${db_res_bin}/cheats_60fps.h
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
	Warning: if changes were made to GameIndex.yaml, cheats_ws.zip, cheats_60fps.zip or cheats_nointerlacing.zip, this build script will
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
		OUTPUT ${db_res_bin}/cheats_60fps.h
		COMMAND ${CMAKE_COMMAND} -E copy cheats_60fps.h ${db_res_bin}/cheats_60fps.h
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
	)

# IPU headers
set(pcsx2IPUHeaders
	IPU/IPUdma.h
	IPU/IPU_Fifo.h
	IPU/IPU.h
	IPU/mpeg2lib/Mpeg.h
	)

# ps2 sources
set(pcsx2ps2Sources
	ps2/BiosTools.cpp
	ps2/pgif.cpp
	ps2/Iop/IopHwRead.cpp
	ps2/Iop/IopHwWrite.cpp
	ps2/Iop/PsxBios.cpp)

# ps2 headers
set(pcsx2ps2Headers
	ps2/BiosTools.h
	ps2/pgif.h
	ps2/HwInternal.h
	ps2/Iop/IopHw_Internal.h)

# System headers
set(pcsx2SystemHeaders
	System/RecTypes.h
	)

# Windows sources
set(pcsx2WindowsSources
	windows/WinCompressNTFS.cpp
	)

# x86 sources
set(pcsx2x86Sources
	x86/BaseblockEx.cpp
	x86/iCOP0.cpp
	x86/iCore.cpp
	x86/iFPU.cpp
	x86/iFPUd.cpp
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
	x86/microVU_Tables.inl
	x86/microVU_Upper.inl
	x86/newVif.h
	x86/newVif_HashBucket.h
	x86/newVif_UnpackSSE.h
	)

# common Sources
	set(Common
		${db_resources}
		${pcsx2Sources}
		${pcsx2Headers}
		${pcsx2CDVDSources}
		${pcsx2CDVDHeaders}
		${pcsx2PADSources}
		${pcsx2PADHeaders}
		${pcsx2DEV9Sources}
		${pcsx2DEV9Headers}
		${pcsx2USBSources}
		${pcsx2USBHeaders}
		${pcsx2SPU2Sources}
		${pcsx2SPU2Headers}
		${pcsx2GSSources}
		${pcsx2GSHeaders}
		${pcsx2DebugToolsSources}
		${pcsx2GuiSources}
		${pcsx2GuiHeaders}
		${pcsx2IPUSources}
		${pcsx2IPUHeaders}
		${pcsx2ps2Sources}
		${pcsx2ps2Headers}
		${pcsx2SystemHeaders}
		${pcsx2x86Sources}
		${pcsx2x86Headers})

# platform sources
# Linux
if(Linux)
	set(Platform_Libs
		${LIBUDEV_LIBRARIES}
		)
endif()

# Windows
if(Windows)
	set(Platform
		${pcsx2WindowsSources}
		)
endif()

set(pcsx2FinalSources
	${Common}
	${Platform}
)

set(pcsx2FinalLibs
    Utilities
    x86emitter
    ${wxWidgets_LIBRARIES}
    ${GS_LIBRARIES}
    ${ZLIB_LIBRARIES}
    ${AIO_LIBRARIES}
    ${GCOV_LIBRARIES}
    ${Platform_Libs}
)

set(pcsx2FinalLibs ${pcsx2FinalLibs})
if(MSVC)
  set(pcsx2FinalSources ${pcsx2FinalSources})
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

   add_library(${Output} SHARED
     ${CMAKE_SOURCE_DIR}/libretro/main.cpp
     ${pcsx2FinalSources}
    "../libretro/language_injector.cpp" "../libretro/retro_messager.cpp"  )
   include_directories(. ${CMAKE_SOURCE_DIR}/libretro)
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
target_compile_features(${Output} PRIVATE cxx_std_17)
