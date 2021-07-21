/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2010  PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "PrecompiledHeader.h"
#include "IopCommon.h"

#include <wx/dir.h>
#include <wx/file.h>
#include <memory>

#include "GS.h"
#include "Gif.h"

#include "Utilities/pxStreams.h"

#include "svnrev.h"

SysPluginBindings SysPlugins;

bool SysPluginBindings::McdIsPresent( uint port, uint slot )
{
	return !!Mcd->McdIsPresent( (PS2E_THISPTR) Mcd, port, slot );
}

void SysPluginBindings::McdGetSizeInfo( uint port, uint slot, PS2E_McdSizeInfo& outways )
{
	if( Mcd->McdGetSizeInfo )
		Mcd->McdGetSizeInfo( (PS2E_THISPTR) Mcd, port, slot, &outways );
}

bool SysPluginBindings::McdIsPSX( uint port, uint slot )
{
	return Mcd->McdIsPSX( (PS2E_THISPTR) Mcd, port, slot );
}

void SysPluginBindings::McdRead( uint port, uint slot, u8 *dest, u32 adr, int size )
{
	Mcd->McdRead( (PS2E_THISPTR) Mcd, port, slot, dest, adr, size );
}

void SysPluginBindings::McdSave( uint port, uint slot, const u8 *src, u32 adr, int size )
{
	Mcd->McdSave( (PS2E_THISPTR) Mcd, port, slot, src, adr, size );
}

void SysPluginBindings::McdEraseBlock( uint port, uint slot, u32 adr )
{
	Mcd->McdEraseBlock( (PS2E_THISPTR) Mcd, port, slot, adr );
}

u64 SysPluginBindings::McdGetCRC( uint port, uint slot )
{
	return Mcd->McdGetCRC( (PS2E_THISPTR) Mcd, port, slot );
}

void SysPluginBindings::McdNextFrame( uint port, uint slot ) {
	Mcd->McdNextFrame( (PS2E_THISPTR) Mcd, port, slot );
}

bool SysPluginBindings::McdReIndex( uint port, uint slot, const wxString& filter ) {
	return Mcd->McdReIndex( (PS2E_THISPTR) Mcd, port, slot, filter );
}

// ----------------------------------------------------------------------------
// Yay, order of this array shouldn't be important. :)
//
const PluginInfo tbl_PluginInfo[] =
{
	{ "GS",		PluginId_GS,	PS2E_LT_GS,		PS2E_GS_VERSION		},
	{ "PAD",	PluginId_PAD,	PS2E_LT_PAD,	PS2E_PAD_VERSION	},
	{ "USB",	PluginId_USB,	PS2E_LT_USB,	PS2E_USB_VERSION	},
	{ "DEV9",	PluginId_DEV9,	PS2E_LT_DEV9,	PS2E_DEV9_VERSION	},

	{ NULL },

	// See PluginEnums_t for details on the MemoryCard plugin hack.
	{ "Mcd",	PluginId_Mcd,	0,	0	},
};

typedef void CALLBACK VoidMethod();
typedef void CALLBACK vMeth();		// shorthand for VoidMethod

// ----------------------------------------------------------------------------
struct LegacyApi_CommonMethod
{
	const char*		MethodName;

	// fallback is used if the method is null.  If the method is null and fallback is null
	// also, the plugin is considered incomplete or invalid, and an error is generated.
	VoidMethod*	Fallback;

	// returns the method name as a wxString, converted from UTF8.
	wxString GetMethodName( PluginsEnum_t pid ) const
	{
		return tbl_PluginInfo[pid].GetShortname() + fromUTF8( MethodName );
	}
};

// ----------------------------------------------------------------------------
struct LegacyApi_ReqMethod
{
	const char*		MethodName;
	VoidMethod**	Dest;		// Target function where the binding is saved.

	// fallback is used if the method is null.  If the method is null and fallback is null
	// also, the plugin is considered incomplete or invalid, and an error is generated.
	VoidMethod*	Fallback;

	// returns the method name as a wxString, converted from UTF8.
	wxString GetMethodName( ) const
	{
		return fromUTF8( MethodName );
	}
};

// ----------------------------------------------------------------------------
struct LegacyApi_OptMethod
{
	const char*		MethodName;
	VoidMethod**	Dest;		// Target function where the binding is saved.

	// returns the method name as a wxString, converted from UTF8.
	wxString GetMethodName() const { return fromUTF8( MethodName ); }
};


static s32  CALLBACK fallback_freeze(int mode, freezeData *data)
{
	if( mode == FREEZE_SIZE ) data->size = 0;
	return 0;
}

static void CALLBACK fallback_keyEvent(keyEvent *ev) {}
static void CALLBACK fallback_setSettingsDir(const char* dir) {}
static void CALLBACK fallback_setLogDir(const char* dir) {}
static void CALLBACK fallback_configure() {}
static void CALLBACK fallback_about() {}
static s32  CALLBACK fallback_test() { return 0; }

#ifndef BUILTIN_GS_PLUGIN
_GSvsync           GSvsync;
_GSosdLog          GSosdLog;
_GSosdMonitor      GSosdMonitor;
_GSopen            GSopen;
_GSopen2           GSopen2;
_GSgifTransfer     GSgifTransfer;
_GSgifTransfer1    GSgifTransfer1;
_GSgifTransfer2    GSgifTransfer2;
_GSgifTransfer3    GSgifTransfer3;
_GSgifSoftReset    GSgifSoftReset;
_GSreadFIFO        GSreadFIFO;
_GSreadFIFO2       GSreadFIFO2;
_GSinitReadFIFO    GSinitReadFIFO;
_GSinitReadFIFO2   GSinitReadFIFO2;
_GSchangeSaveState GSchangeSaveState;
_GSgetTitleInfo2   GSgetTitleInfo2;
_GSmakeSnapshot	   GSmakeSnapshot;
_GSmakeSnapshot2   GSmakeSnapshot2;
_GSirqCallback 	   GSirqCallback;
_GSsetBaseMem		GSsetBaseMem;
_GSsetGameCRC		GSsetGameCRC;
_GSsetFrameSkip		GSsetFrameSkip;
_GSsetVsync			GSsetVsync;
_GSsetExclusive		GSsetExclusive;
_GSsetupRecording	GSsetupRecording;
_GSreset			GSreset;
_GSwriteCSR			GSwriteCSR;
#endif

static void CALLBACK GS_makeSnapshot(const char *path) {}
static void CALLBACK GS_setGameCRC(u32 crc, int gameopts) {}
static void CALLBACK GS_irqCallback(void (*callback)()) {}
static void CALLBACK GS_setFrameSkip(int frameskip) {}
static void CALLBACK GS_setVsync(int enabled) {}
static void CALLBACK GS_setExclusive(int isExcl) {}
static void CALLBACK GS_changeSaveState( int, const char* filename ) {}

void CALLBACK GS_getTitleInfo2( char* dest, size_t length )
{
	// Just return a generic "GS" title -- a plugin actually implementing this feature
	// should return a title such as "GSdx" instead.  --air

	dest[0] = 'G';
	dest[1] = 'S';
	dest[2] = 0;
}

#if COPY_GS_PACKET_TO_MTGS == 1
// This legacy passthrough function is needed because the old GS plugins tended to assume that
// a PATH1 transfer that didn't EOP needed an automatic EOP (which was needed to avoid a crash
// in the BIOS when it starts an XGKICK prior to having an EOP written to VU1 memory).  The new
// MTGS wraps data around the end of the MTGS buffer, so it often splits PATH1 data into two
// transfers now.
static void CALLBACK GS_Legacy_gifTransfer( const u32* src, u32 data )
{
	static __aligned16 u128 path1queue[0x400];
	static uint path1size = 0;

	const u128* src128 = (u128*)src;

	if( (src128 + data) >= &RingBuffer.m_Ring[RingBufferSize] )
	{
		// the transfer is most likely wrapped/partial.  We need to queue it into a linear buffer
		// and then send it on its way on the next copy.

		memcpy( path1queue, src128, data*16);
		path1size = data;
	}
	else
	{
		if (path1size != 0)
		{
			// Previous transfer check.  *Most* likely this one should be added to it, but to know for
			// sure we need to check to see if src points to the head of RingBuffer.  If its pointing
			// to like Ringbuffer[1] instead it means the last transfer finished and this transfer is
			// a new one.

			if (src128 == RingBuffer.m_Ring)
			{
				pxAssert( (data+path1size) <= 0x400 );
				memcpy( &path1queue[path1size], src128, data*16);
				path1size += data;
			}
			GSgifTransfer1( (u32*)path1queue, 0 );
			path1size = 0;
		}
		else
		{
			GSgifTransfer1( (u32*)src128, 0 );
		}
	}
}
#else
// In this case the MTGS thread will only be using the "GSgifTransfer"
// callback, which falls back to this function if its an old plugin.
// Since GSgifTransfer2 is the least hacky old call-back, and MTGS will
// just be using a single gif path, we'll just solely use path 2...
static void CALLBACK GS_Legacy_gifTransfer(const u32* src, u32 data) {
	GSgifTransfer2((u32*)src, data);
}
#endif

static void CALLBACK GS_Legacy_GSreadFIFO2(u64* pMem, int qwc) {
	while(qwc--) GSreadFIFO(pMem);
}

// PAD
#ifndef BUILTIN_PAD_PLUGIN
_PADinit           PADinit;
_PADopen           PADopen;
_PADstartPoll      PADstartPoll;
_PADpoll           PADpoll;
_PADquery          PADquery;
_PADupdate         PADupdate;
_PADkeyEvent       PADkeyEvent;
_PADsetSlot        PADsetSlot;
_PADqueryMtap      PADqueryMtap;
_PADWriteEvent	   PADWriteEvent;
#endif

static void PAD_update( u32 padslot ) { }

// DEV9
#ifndef BUILTIN_DEV9_PLUGIN
_DEV9open          DEV9open;
_DEV9read8         DEV9read8;
_DEV9read16        DEV9read16;
_DEV9read32        DEV9read32;
_DEV9write8        DEV9write8;
_DEV9write16       DEV9write16;
_DEV9write32       DEV9write32;

_DEV9readDMA8Mem   DEV9readDMA8Mem;
_DEV9writeDMA8Mem  DEV9writeDMA8Mem;

_DEV9irqCallback   DEV9irqCallback;
_DEV9irqHandler    DEV9irqHandler;
_DEV9async         DEV9async;
#endif

// USB
#ifndef BUILTIN_USB_PLUGIN
_USBopen           USBopen;
_USBread8          USBread8;
_USBread16         USBread16;
_USBread32         USBread32;
_USBwrite8         USBwrite8;
_USBwrite16        USBwrite16;
_USBwrite32        USBwrite32;
_USBasync          USBasync;

_USBirqCallback    USBirqCallback;
_USBirqHandler     USBirqHandler;
_USBsetRAM         USBsetRAM;
#endif

DEV9handler dev9Handler;
USBhandler usbHandler;
uptr pDsp[2];

static s32 CALLBACK _hack_PADinit()
{
	return PADinit( 3 );
}

// ----------------------------------------------------------------------------
// Important: Contents of this array must match the order of the contents of the
// LegacyPluginAPI_Common structure defined in Plugins.h.
//
static const LegacyApi_CommonMethod s_MethMessCommon[] =
{
	{	"init",				NULL	},
	{	"close",			NULL	},
	{	"shutdown",			NULL	},

	{	"keyEvent",			(vMeth*)fallback_keyEvent },
	{	"setSettingsDir",	(vMeth*)fallback_setSettingsDir },
	{	"setLogDir",	    (vMeth*)fallback_setLogDir },

	{	"freeze",			(vMeth*)fallback_freeze	},
	{	"test",				(vMeth*)fallback_test },
	{	"configure",		fallback_configure	},
	{	"about",			fallback_about	},

	{ NULL }

};

// ----------------------------------------------------------------------------
//  GS Mess!
// ----------------------------------------------------------------------------
static const LegacyApi_ReqMethod s_MethMessReq_GS[] =
{
	{	"GSopen",			(vMeth**)&GSopen,			NULL	},
	{	"GSvsync",			(vMeth**)&GSvsync,			NULL	},
	{	"GSgifTransfer",	(vMeth**)&GSgifTransfer,	(vMeth*)GS_Legacy_gifTransfer },
	{	"GSgifTransfer2",	(vMeth**)&GSgifTransfer2,	NULL	},
	{	"GSgifTransfer3",	(vMeth**)&GSgifTransfer3,	NULL	},
	{	"GSreadFIFO2",		(vMeth**)&GSreadFIFO2,		(vMeth*)GS_Legacy_GSreadFIFO2 },

	{	"GSmakeSnapshot",	(vMeth**)&GSmakeSnapshot,	(vMeth*)GS_makeSnapshot },
	{	"GSirqCallback",	(vMeth**)&GSirqCallback,	(vMeth*)GS_irqCallback },
	{	"GSsetBaseMem",		(vMeth**)&GSsetBaseMem,		NULL	},
	{	"GSwriteCSR",		(vMeth**)&GSwriteCSR,		NULL	},
	{	"GSsetGameCRC",		(vMeth**)&GSsetGameCRC,		(vMeth*)GS_setGameCRC },

	{	"GSsetFrameSkip",	(vMeth**)&GSsetFrameSkip,	(vMeth*)GS_setFrameSkip	},
	{	"GSsetVsync",		(vMeth**)&GSsetVsync,		(vMeth*)GS_setVsync	},
	{	"GSsetExclusive",	(vMeth**)&GSsetExclusive,	(vMeth*)GS_setExclusive	},
	{	"GSchangeSaveState",(vMeth**)&GSchangeSaveState,(vMeth*)GS_changeSaveState },
	{	"GSgetTitleInfo2",	(vMeth**)&GSgetTitleInfo2,	(vMeth*)GS_getTitleInfo2 },
	{ NULL }
};

static const LegacyApi_OptMethod s_MethMessOpt_GS[] =
{
	{	"GSosdLog",			(vMeth**)&GSosdLog			},
	{	"GSosdMonitor",		(vMeth**)&GSosdMonitor		},
	{	"GSopen2",			(vMeth**)&GSopen2			},
	{	"GSreset",			(vMeth**)&GSreset			},
	{	"GSsetupRecording",	(vMeth**)&GSsetupRecording	},
	{	"GSmakeSnapshot2",	(vMeth**)&GSmakeSnapshot2	},
	{	"GSgifSoftReset",	(vMeth**)&GSgifSoftReset	},
	{	"GSreadFIFO",		(vMeth**)&GSreadFIFO		},
	{	"GSinitReadFIFO",	(vMeth**)&GSinitReadFIFO	},
	{	"GSinitReadFIFO2",	(vMeth**)&GSinitReadFIFO2	},
	{	"GSgifTransfer1",	(vMeth**)&GSgifTransfer1	},
	{ NULL }
};

// ----------------------------------------------------------------------------
//  PAD Mess!
// ----------------------------------------------------------------------------
static s32 CALLBACK PAD_queryMtap( u8 slot ) { return 0; }
static s32 CALLBACK PAD_setSlot(u8 port, u8 slot) { return 0; }

static const LegacyApi_ReqMethod s_MethMessReq_PAD[] =
{
	{	"PADopen",			(vMeth**)&PADopen,		NULL },
	{	"PADstartPoll",		(vMeth**)&PADstartPoll,	NULL },
	{	"PADpoll",			(vMeth**)&PADpoll,		NULL },
	{	"PADquery",			(vMeth**)&PADquery,		NULL },
	{	"PADkeyEvent",		(vMeth**)&PADkeyEvent,	NULL },

	// fixme - Following functions are new as of some revison post-0.9.6, and
	// are for multitap support only.  They should either be optional or offer
	// NOP fallbacks, to allow older plugins to retain functionality.
	{	"PADsetSlot",		(vMeth**)&PADsetSlot,	(vMeth*)PAD_setSlot },
	{	"PADqueryMtap",		(vMeth**)&PADqueryMtap,	(vMeth*)PAD_queryMtap },
	{ NULL },
};

static const LegacyApi_OptMethod s_MethMessOpt_PAD[] =
{
	{	"PADupdate",		(vMeth**)&PADupdate },
	{   "PADWriteEvent",	(vMeth**)&PADWriteEvent },
	{ NULL },
};

// ----------------------------------------------------------------------------
//  DEV9 Mess!
// ----------------------------------------------------------------------------
static const LegacyApi_ReqMethod s_MethMessReq_DEV9[] =
{
	{	"DEV9open",			(vMeth**)&DEV9open,			NULL },
	{	"DEV9read8",		(vMeth**)&DEV9read8,		NULL },
	{	"DEV9read16",		(vMeth**)&DEV9read16,		NULL },
	{	"DEV9read32",		(vMeth**)&DEV9read32,		NULL },
	{	"DEV9write8",		(vMeth**)&DEV9write8,		NULL },
	{	"DEV9write16",		(vMeth**)&DEV9write16,		NULL },
	{	"DEV9write32",		(vMeth**)&DEV9write32,		NULL },
	{	"DEV9readDMA8Mem",	(vMeth**)&DEV9readDMA8Mem,	NULL },
	{	"DEV9writeDMA8Mem",	(vMeth**)&DEV9writeDMA8Mem,	NULL },
	{	"DEV9irqCallback",	(vMeth**)&DEV9irqCallback,	NULL },
	{	"DEV9irqHandler",	(vMeth**)&DEV9irqHandler,	NULL },

	{ NULL }
};

static const LegacyApi_OptMethod s_MethMessOpt_DEV9[] =
{
	{ "DEV9async", (vMeth**)&DEV9async },
	{ NULL }
};

// ----------------------------------------------------------------------------
//  USB Mess!
// ----------------------------------------------------------------------------
static const LegacyApi_ReqMethod s_MethMessReq_USB[] =
{
	{	"USBopen",			(vMeth**)&USBopen,			NULL },
	{	"USBread8",			(vMeth**)&USBread8,			NULL },
	{	"USBread16",		(vMeth**)&USBread16,		NULL },
	{	"USBread32",		(vMeth**)&USBread32,		NULL },
	{	"USBwrite8",		(vMeth**)&USBwrite8,		NULL },
	{	"USBwrite16",		(vMeth**)&USBwrite16,		NULL },
	{	"USBwrite32",		(vMeth**)&USBwrite32,		NULL },
	{	"USBirqCallback",	(vMeth**)&USBirqCallback,	NULL },
	{	"USBirqHandler",	(vMeth**)&USBirqHandler,	NULL },
	{ NULL }
};

static const LegacyApi_OptMethod s_MethMessOpt_USB[] =
{
	{	"USBasync",		(vMeth**)&USBasync },
	{	"USBsetRAM",	(vMeth**)&USBsetRAM },
	{ NULL }
};

static const LegacyApi_ReqMethod* const s_MethMessReq[] =
{
	s_MethMessReq_GS,
	s_MethMessReq_PAD,
	s_MethMessReq_USB,
	s_MethMessReq_DEV9
};

static const LegacyApi_OptMethod* const s_MethMessOpt[] =
{
	s_MethMessOpt_GS,
	s_MethMessOpt_PAD,
	s_MethMessOpt_USB,
	s_MethMessOpt_DEV9
};

SysCorePlugins *g_plugins = NULL;

// ---------------------------------------------------------------------------------
//       Plugin-related Exception Implementations
// ---------------------------------------------------------------------------------

wxString Exception::SaveStateLoadError::FormatDiagnosticMessage() const
{
	FastFormatUnicode retval;
	retval.Write("Savestate is corrupt or incomplete!\n");
	log_cb(RETRO_LOG_ERROR, "Error: Savestate is corrupt or incomplete!\n");
	_formatDiagMsg(retval);
	return retval;
}

wxString Exception::SaveStateLoadError::FormatDisplayMessage() const
{
	FastFormatUnicode retval;
	retval.Write(_("The savestate cannot be loaded, as it appears to be corrupt or incomplete."));
	retval.Write("\n");
	log_cb(RETRO_LOG_ERROR, "Error: The savestate cannot be loaded, as it appears to be corrupt or incomplete.\n");
	_formatUserMsg(retval);
	return retval;
}

Exception::PluginOpenError::PluginOpenError( PluginsEnum_t pid )
{
	PluginId = pid;
	m_message_diag = L"%s plugin failed to open!";
	m_message_user = _("%s plugin failed to open.  Your computer may have insufficient resources, or incompatible hardware/drivers.");
}

Exception::PluginInitError::PluginInitError( PluginsEnum_t pid )
{
	PluginId = pid;
	m_message_diag = L"%s plugin initialization failed!";
	m_message_user = _("%s plugin failed to initialize.  Your system may have insufficient memory or resources needed.");
}

Exception::PluginLoadError::PluginLoadError( PluginsEnum_t pid )
{
	PluginId = pid;
}

wxString Exception::PluginLoadError::FormatDiagnosticMessage() const
{
	return pxsFmt( m_message_diag, WX_STR(tbl_PluginInfo[PluginId].GetShortname()) ) +
		L"\n\n" + StreamName;
}

wxString Exception::PluginLoadError::FormatDisplayMessage() const
{
	return pxsFmt( m_message_user, WX_STR(tbl_PluginInfo[PluginId].GetShortname()) ) +
		L"\n\n" + StreamName;
}

wxString Exception::PluginError::FormatDiagnosticMessage() const
{
	return pxsFmt( m_message_diag, WX_STR(tbl_PluginInfo[PluginId].GetShortname()) );
}

wxString Exception::PluginError::FormatDisplayMessage() const
{
	return pxsFmt( m_message_user, WX_STR(tbl_PluginInfo[PluginId].GetShortname()) );
}

wxString Exception::FreezePluginFailure::FormatDiagnosticMessage() const
{
	return pxsFmt(
		L"%s plugin returned an error while saving the state.\n\n",
		tbl_PluginInfo[PluginId].shortname
	);
}

wxString Exception::FreezePluginFailure::FormatDisplayMessage() const
{
	// [TODO]
	return m_message_user;
}

wxString Exception::ThawPluginFailure::FormatDiagnosticMessage() const
{
	return pxsFmt(
		L"%s plugin returned an error while loading the state.\n\n",
		tbl_PluginInfo[PluginId].shortname
	);
}

wxString Exception::ThawPluginFailure::FormatDisplayMessage() const
{
	// [TODO]
	return m_message_user;
}

// --------------------------------------------------------------------------------------
//  PCSX2 Callbacks passed to Plugins
// --------------------------------------------------------------------------------------
// This is currently unimplemented, and should be provided by the AppHost (gui) rather
// than the EmuCore.  But as a quickhackfix until the new plugin API is fleshed out, this
// will suit our needs nicely. :)

static BOOL PS2E_CALLBACK pcsx2_GetInt( const char* name, int* dest )
{
	return FALSE;		// not implemented...
}

static BOOL PS2E_CALLBACK pcsx2_GetBoolean( const char* name, BOOL* result )
{
	return FALSE;		// not implemented...
}

static BOOL PS2E_CALLBACK pcsx2_GetString( const char* name, char* dest, int maxlen )
{
	return FALSE;		// not implemented...
}

static char* PS2E_CALLBACK pcsx2_GetStringAlloc( const char* name, void* (PS2E_CALLBACK* allocator)(int size) )
{
	return FALSE;		// not implemented...
}

static void PS2E_CALLBACK pcsx2_OSD_WriteLn( int icon, const char* msg )
{
	log_cb(RETRO_LOG_INFO, msg );
}

// ---------------------------------------------------------------------------------
// DynamicStaticLibrary
// ---------------------------------------------------------------------------------
std::string s_strIniPath = "inis";
std::string s_strLogPath = "logs";

StaticLibrary::StaticLibrary(PluginsEnum_t _pid) : pid(_pid)
{
}

bool StaticLibrary::Load(const wxString& name)
{
	return true;
}

void* StaticLibrary::GetSymbol(const wxString &name)
{
#define RETURN_SYMBOL(s) if (name == #s) return (void*)&s;

#define RETURN_COMMON_SYMBOL(p) \
	RETURN_SYMBOL(p##init) \
	RETURN_SYMBOL(p##close) \
	RETURN_SYMBOL(p##shutdown) \
	RETURN_SYMBOL(p##keyEvent) \
	RETURN_SYMBOL(p##setSettingsDir) \
	RETURN_SYMBOL(p##setLogDir) \
	RETURN_SYMBOL(p##freeze) \
	RETURN_SYMBOL(p##test) \
	RETURN_SYMBOL(p##configure) \
	RETURN_SYMBOL(p##about)

#ifdef BUILTIN_GS_PLUGIN
	RETURN_COMMON_SYMBOL(GS);
#endif
#ifdef BUILTIN_PAD_PLUGIN
	RETURN_COMMON_SYMBOL(PAD);
#endif
#ifdef BUILTIN_DEV9_PLUGIN
	RETURN_COMMON_SYMBOL(DEV9);
#endif
#ifdef BUILTIN_USB_PLUGIN
	RETURN_COMMON_SYMBOL(USB);
#endif


#undef RETURN_COMMON_SYMBOL
#undef RETURN_SYMBOL

	return NULL;
}

bool StaticLibrary::HasSymbol(const wxString &name)
{
	return false;
}

DynamicLibrary::DynamicLibrary() : Lib()
{
}

bool DynamicLibrary::Load(const wxString& name)
{
	return Lib.Load(name);
}

void* DynamicLibrary::GetSymbol(const wxString &name)
{
	return Lib.GetSymbol(name);
}

bool DynamicLibrary::HasSymbol(const wxString &name)
{
	return Lib.HasSymbol(name);
}

// ---------------------------------------------------------------------------------
//  PluginStatus_t Implementations
// ---------------------------------------------------------------------------------
SysCorePlugins::SysCorePlugins() :
	m_mcdPlugin(NULL), m_SettingsFolder(), m_LogFolder(), m_mtx_PluginStatus(), m_mcdOpen(false)
{
}

SysCorePlugins::PluginStatus_t::PluginStatus_t( PluginsEnum_t _pid, const wxString& srcfile )
	: Filename( srcfile )
{
	pid = _pid;

	IsInitialized	= false;
	IsOpened		= false;

	switch (_pid) {
#ifdef BUILTIN_GS_PLUGIN
		case PluginId_GS:
#endif
#ifdef BUILTIN_PAD_PLUGIN
		case PluginId_PAD:
#endif
#ifdef BUILTIN_DEV9_PLUGIN
		case PluginId_DEV9:
#endif
#ifdef BUILTIN_USB_PLUGIN
		case PluginId_USB:
#endif
		case PluginId_Count:
			IsStatic	= true;
			Lib			= new StaticLibrary(_pid);
			break;
		default:
			IsStatic	= false;
			Lib			= new DynamicLibrary();
			break;
	}

	if (IsStatic) {
		BindCommon( pid );

	} else {
		if( Filename.IsEmpty() )
			throw Exception::PluginInitError( pid ).SetDiagMsg( L"Empty plugin filename" );

		if( !wxFile::Exists( Filename ) )
			throw Exception::PluginLoadError( pid ).SetStreamName(srcfile)
				.SetBothMsgs(pxL("The configured %s plugin file was not found"));

		if( !Lib->Load( Filename ) )
			throw Exception::PluginLoadError( pid ).SetStreamName(Filename)
				.SetBothMsgs(pxL("The configured %s plugin file is not a valid dynamic library"));


		// Try to enumerate the new v2.0 plugin interface first.
		// If that fails, fall back on the old style interface.

		//m_libs[i].GetSymbol( L"PS2E_InitAPI" );		// on the TODO list!


		// 2.0 API Failed; Enumerate the Old Stuff! -->

		_PS2EgetLibName		GetLibName		= (_PS2EgetLibName)		Lib->GetSymbol( L"PS2EgetLibName" );
		_PS2EgetLibVersion2	GetLibVersion2	= (_PS2EgetLibVersion2)	Lib->GetSymbol( L"PS2EgetLibVersion2" );

		if( GetLibName == NULL || GetLibVersion2 == NULL )
			throw Exception::PluginLoadError( pid ).SetStreamName(Filename)
				.SetDiagMsg(L"%s plugin init failed: Method binding failure on GetLibName or GetLibVersion2.")
				.SetUserMsg(_( "The configured %s plugin is not a PCSX2 plugin, or is for an older unsupported version of PCSX2."));

		Name = fromUTF8( GetLibName() );
		int version = GetLibVersion2( tbl_PluginInfo[pid].typemask );
		Version.Printf( L"%d.%d.%d", (version>>8)&0xff, version&0xff, (version>>24)&0xff );

		// Bind Required Functions
		// (generate critical error if binding fails)

		BindCommon( pid );
		BindRequired( pid );
		BindOptional( pid );
	}


	// Run Plugin's Functionality Test.
	// A lot of plugins don't bother to implement this function and return 0 (success)
	// regardless, but some do so let's go ahead and check it. I mean, we're supposed to. :)

	int testres = CommonBindings.Test();
	if( testres != 0 )
		throw Exception::PluginLoadError( pid ).SetStreamName(Filename)
			.SetDiagMsg(wxsFormat( L"Plugin Test failure, return code: %d", testres ))
			.SetUserMsg(_("The plugin reports that your hardware or software/drivers are not supported."));
}

void SysCorePlugins::PluginStatus_t::BindCommon( PluginsEnum_t pid )
{
	const LegacyApi_CommonMethod* current = s_MethMessCommon;
	VoidMethod** target = (VoidMethod**)&CommonBindings;

	wxDoNotLogInThisScope please;

	while( current->MethodName != NULL )
	{
		*target = (VoidMethod*)Lib->GetSymbol( current->GetMethodName( pid ) );

		if( *target == NULL )
			*target = current->Fallback;

		if( *target == NULL )
		{
			throw Exception::PluginLoadError( pid ).SetStreamName(Filename)
				.SetDiagMsg(wxsFormat( L"\nMethod binding failure on: %s\n", WX_STR(current->GetMethodName( pid )) ))
				.SetUserMsg(_("Configured plugin is not a PCSX2 plugin, or is for an older unsupported version of PCSX2."));
		}

		target++;
		current++;
	}
}

void SysCorePlugins::PluginStatus_t::BindRequired( PluginsEnum_t pid )
{
	const LegacyApi_ReqMethod* current = s_MethMessReq[pid];

	wxDoNotLogInThisScope please;

	while( current->MethodName != NULL )
	{
		*(current->Dest) = (VoidMethod*)Lib->GetSymbol( current->GetMethodName() );

		if( *(current->Dest) == NULL )
			*(current->Dest) = current->Fallback;

		if( *(current->Dest) == NULL )
		{
			throw Exception::PluginLoadError( pid ).SetStreamName(Filename)
				.SetDiagMsg(wxsFormat( L"\n%s plugin init error; Method binding failed: %s\n", WX_STR(current->GetMethodName()) ))
				.SetUserMsg(_( "Configured %s plugin is not a valid PCSX2 plugin, or is for an older unsupported version of PCSX2."));
		}

		current++;
	}
}

void SysCorePlugins::PluginStatus_t::BindOptional( PluginsEnum_t pid )
{
	const LegacyApi_OptMethod* current = s_MethMessOpt[pid];

	wxDoNotLogInThisScope please;

	while( current->MethodName != NULL )
	{
		*(current->Dest) = (VoidMethod*)Lib->GetSymbol( current->GetMethodName() );
		current++;
	}
}

// =====================================================================================
//  SysCorePlugins Implementations
// =====================================================================================

SysCorePlugins::~SysCorePlugins()
{
	try
	{
		Unload();
	}
	DESTRUCTOR_CATCHALL

	// All library unloading done automatically by wx.
}

void SysCorePlugins::Load( PluginsEnum_t pid, const wxString& srcfile )
{
	ScopedLock lock( m_mtx_PluginStatus );
	pxAssert( (uint)pid < PluginId_Count );

	m_info[pid] = std::make_unique<PluginStatus_t>(pid, srcfile);

	log_cb(RETRO_LOG_INFO, "Bound %4s: %s [%s %s]\n", WX_STR(tbl_PluginInfo[pid].GetShortname()), 
		WX_STR(wxFileName(srcfile).GetFullName()), WX_STR(m_info[pid]->Name), WX_STR(m_info[pid]->Version));
}

void SysCorePlugins::Load( const wxString (&folders)[PluginId_Count] )
{
	if( !NeedsLoad() ) return;

	wxDoNotLogInThisScope please;

	ForPlugins([&] (const PluginInfo * pi) {
		Load( pi->id, folders[pi->id] );
		pxYield( 2 );
	});

	// Hack for PAD's stupid parameter passed on Init
#ifndef BUILTIN_PAD_PLUGIN
	PADinit = (_PADinit)m_info[PluginId_PAD]->CommonBindings.Init;
#endif
	m_info[PluginId_PAD]->CommonBindings.Init = _hack_PADinit;

	log_cb(RETRO_LOG_INFO, "Plugins loaded successfully.\n" );

	// HACK!  Manually bind the Internal MemoryCard plugin for now, until
	// we get things more completed in the new plugin api.

	static const PS2E_EmulatorInfo myself =
	{
		"PCSX2",

		{ 0, PCSX2_VersionHi, PCSX2_VersionLo, SVN_REV },

		(int)x86caps.PhysicalCores,
		(int)x86caps.LogicalCores,
		sizeof(wchar_t),

		{ 0,0,0,0,0,0 },

		pcsx2_GetInt,
		pcsx2_GetBoolean,
		pcsx2_GetString,
		pcsx2_GetStringAlloc,
		pcsx2_OSD_WriteLn,

		NULL, // AddMenuItem
		NULL, // Menu_Create
		NULL, // Menu_Delete
		NULL, // Menu_AddItem

		{ 0 }, // MenuItem

		{ 0,0,0,0,0,0,0,0 },
	};

	m_mcdPlugin = FileMcd_InitAPI( &myself );
	if( m_mcdPlugin == NULL )
	{
		// fixme: use plugin's GetLastError (not implemented yet!)
		throw Exception::PluginLoadError( PluginId_Mcd ).SetDiagMsg(L"Internal Memorycard Plugin failed to load.");
	}

	SendLogFolder();
	SendSettingsFolder();
}

void SysCorePlugins::Unload(PluginsEnum_t pid)
{
	ScopedLock lock( m_mtx_PluginStatus );
	pxAssert( (uint)pid < PluginId_Count );
	m_info[pid] = nullptr;
}

void SysCorePlugins::Unload()
{
	if( NeedsShutdown() )
		log_cb(RETRO_LOG_WARN, "(SysCorePlugins) Warning: Unloading plugins prior to shutdown!\n");

	//Shutdown();

	if( !NeedsUnload() ) return;

	log_cb(RETRO_LOG_DEBUG, "Unloading plugins...\n" );

	for( int i=PluginId_Count-1; i>=0; --i )
		Unload( tbl_PluginInfo[i].id );

	log_cb(RETRO_LOG_DEBUG, "Plugins unloaded successfully.\n" );
}

// Exceptions:
//   FileNotFound - Thrown if one of the configured plugins doesn't exist.
//   NotPcsxPlugin - Thrown if one of the configured plugins is an invalid or unsupported DLL

extern bool renderswitch;
extern void spu2DMA4Irq();
extern void spu2DMA7Irq();
extern void spu2Irq();

bool SysCorePlugins::OpenPlugin_GS()
{
	GetMTGS().Resume();
	return true;
}

bool SysCorePlugins::OpenPlugin_PAD()
{
	return !PADopen( (void*)pDsp );
}

bool SysCorePlugins::OpenPlugin_DEV9()
{
	dev9Handler = NULL;

	if( DEV9open( (void*)pDsp ) ) return false;
	DEV9irqCallback( dev9Irq );
	dev9Handler = DEV9irqHandler();
	return true;
}

bool SysCorePlugins::OpenPlugin_USB()
{
	usbHandler = NULL;

	if( USBopen((void*)pDsp) ) return false;
	USBirqCallback( usbIrq );
	usbHandler = USBirqHandler();
	// iopMem is not initialized yet. Moved elsewhere
	//if( USBsetRAM != NULL )
	//	USBsetRAM(iopMem->Main);
	return true;
}

bool SysCorePlugins::OpenPlugin_Mcd()
{
	ScopedLock lock( m_mtx_PluginStatus );

	// [TODO] Fix up and implement PS2E_SessionInfo here!!  (the currently NULL parameter)
	if( SysPlugins.Mcd )
		SysPlugins.Mcd->Base.EmuOpen( (PS2E_THISPTR) SysPlugins.Mcd, NULL );

	return true;
}

void SysCorePlugins::Open( PluginsEnum_t pid )
{
	pxAssert( (uint)pid < PluginId_Count );
	if( IsOpen(pid) ) return;

	log_cb(RETRO_LOG_INFO, "Opening %s\n", tbl_PluginInfo[pid].shortname );

	// Each Open needs to be called explicitly. >_<

	bool result = true;
	switch( pid )
	{
		case PluginId_GS:	result = OpenPlugin_GS();	break;
		case PluginId_PAD:	result = OpenPlugin_PAD();	break;
		case PluginId_USB:	result = OpenPlugin_USB();	break;
		case PluginId_DEV9:	result = OpenPlugin_DEV9();	break;

		jNO_DEFAULT;
	}
	if( !result )
		throw Exception::PluginOpenError( pid );

	ScopedLock lock( m_mtx_PluginStatus );
	if( m_info[pid] ) m_info[pid]->IsOpened = true;
}

void SysCorePlugins::Open()
{
	Init();

	if( !NeedsOpen() ) return;		// Spam stopper:  returns before writing any logs. >_<

	log_cb(RETRO_LOG_INFO, "Opening plugins...\n");

	SendSettingsFolder();

	ForPlugins([&] (const PluginInfo * pi) {
		Open( pi->id );
		// If GS doesn't support GSopen2, need to wait until call to GSopen
		// returns to populate pDsp.  If it does, can initialize other plugins
		// at same time as GS, as long as GSopen2 does not subclass its window.
#ifdef __linux__
		// On linux, application have also a channel (named display) to communicate with the
		// Xserver. The safe rule is 1 thread, 1 channel. In our case we use the display in
		// several places. Save yourself of multithread headache. Wait few seconds the end of 
		// gsopen -- Gregory
		if (pi->id == PluginId_GS) GetMTGS().WaitForOpen();
#elif !defined(BUILTIN_GS_PLUGIN)
		if (pi->id == PluginId_GS && !GSopen2) GetMTGS().WaitForOpen();
#endif
	});
#ifndef BUILTIN_GS_PLUGIN
	if (GSopen2)
#endif
		GetMTGS().WaitForOpen();

	if( !m_mcdOpen.exchange(true) )
	{
		log_cb(RETRO_LOG_DEBUG, "Opening Memorycards\n");
		OpenPlugin_Mcd();
	}

	log_cb(RETRO_LOG_INFO, "Plugins opened successfully.\n");
}

void SysCorePlugins::_generalclose( PluginsEnum_t pid )
{
	ScopedLock lock( m_mtx_PluginStatus );
	if( m_info[pid] ) m_info[pid]->CommonBindings.Close();
}

void SysCorePlugins::ClosePlugin_GS()
{
	// old-skool: force-close PAD before GS, because the PAD depends on the GS window.

	if( GetMTGS().IsSelf() )
		_generalclose( PluginId_GS );
	else
	{
#ifndef BUILTIN_GS_PLUGIN
		if( !GSopen2 ) Close( PluginId_PAD );
#endif
		GetMTGS().Suspend();
	}
}

void SysCorePlugins::ClosePlugin_PAD()
{
	_generalclose( PluginId_PAD );
}

void SysCorePlugins::ClosePlugin_DEV9()
{
	_generalclose( PluginId_DEV9 );
}

void SysCorePlugins::ClosePlugin_USB()
{
	_generalclose( PluginId_USB );
}

void SysCorePlugins::ClosePlugin_Mcd()
{
	ScopedLock lock( m_mtx_PluginStatus );
	if( SysPlugins.Mcd ) SysPlugins.Mcd->Base.EmuClose( (PS2E_THISPTR) SysPlugins.Mcd );
}

void SysCorePlugins::Close( PluginsEnum_t pid )
{
	pxAssert( (uint)pid < PluginId_Count );

	if( !IsOpen(pid) ) return;
	
	if( !GetMTGS().IsSelf() )		// stop the spam!
		log_cb(RETRO_LOG_INFO, "Closing %s\n", tbl_PluginInfo[pid].shortname );

	switch( pid )
	{
		case PluginId_GS:	ClosePlugin_GS();	break;
		case PluginId_PAD:	ClosePlugin_PAD();	break;
		case PluginId_USB:	ClosePlugin_USB();	break;
		case PluginId_DEV9:	ClosePlugin_DEV9();	break;
		case PluginId_Mcd:	ClosePlugin_Mcd();	break;
		
		jNO_DEFAULT;
	}

	ScopedLock lock( m_mtx_PluginStatus );
	if( m_info[pid] ) m_info[pid]->IsOpened = false;
}

void SysCorePlugins::Close()
{
	if( !NeedsClose() ) return;	// Spam stopper; returns before writing any logs. >_<

	// Close plugins in reverse order of the initialization procedure, which
	// ensures the GS gets closed last.

	log_cb(RETRO_LOG_INFO, "Closing plugins...\n" );

	if( m_mcdOpen.exchange(false) )
	{
		log_cb(RETRO_LOG_DEBUG, "Closing Memorycards\n");
		ClosePlugin_Mcd();
	}

	for( int i=PluginId_Count-1; i>=0; --i )
		Close( tbl_PluginInfo[i].id );
	
	log_cb(RETRO_LOG_INFO, "Plugins closed successfully.\n");
}

void SysCorePlugins::Init( PluginsEnum_t pid )
{
	ScopedLock lock( m_mtx_PluginStatus );

	if( !m_info[pid] || m_info[pid]->IsInitialized ) return;

	log_cb(RETRO_LOG_INFO, "Init %s\n", tbl_PluginInfo[pid].shortname );
	if( 0 != m_info[pid]->CommonBindings.Init() )
		throw Exception::PluginInitError( pid );

	m_info[pid]->IsInitialized = true;
}

void SysCorePlugins::Shutdown( PluginsEnum_t pid )
{
	ScopedLock lock( m_mtx_PluginStatus );

	if( !m_info[pid] || !m_info[pid]->IsInitialized )
		return;
#ifndef NDEBUG
	log_cb(RETRO_LOG_DEBUG, "Shutdown %s\n", tbl_PluginInfo[pid].shortname );
#endif
	m_info[pid]->IsInitialized = false;
	m_info[pid]->CommonBindings.Shutdown();
}

// Initializes all plugins.  Plugin initialization should be done once for every new emulation
// session.  During a session emulation can be paused/resumed using Open/Close, and should be
// terminated using Shutdown().
//
// Returns TRUE if an init was performed for any (or all) plugins.  Returns FALSE if all
// plugins were already in an initialized state (no action taken).
//
// In a purist emulation sense, Init() and Shutdown() should only ever need be called for when
// the PS2's hardware has received a *full* hard reset.  Soft resets typically should rely on
// the PS2's bios/kernel to re-initialize hardware on the fly.
//
bool SysCorePlugins::Init()
{
	if( !NeedsInit() ) return false;

	log_cb(RETRO_LOG_INFO, "Initializing plugins...\n");

	ForPlugins([&] (const PluginInfo * pi) {
		Init( pi->id );
	});

	if( SysPlugins.Mcd == NULL )
	{
		SysPlugins.Mcd = (PS2E_ComponentAPI_Mcd*)m_mcdPlugin->NewComponentInstance( PS2E_TYPE_Mcd );
		if( SysPlugins.Mcd == NULL )
		{
			// fixme: use plugin's GetLastError (not implemented yet!)
			throw Exception::PluginInitError( PluginId_Mcd )
				.SetBothMsgs(pxLt("Internal Memorycard Plugin failed to initialize."));
		}
	}

	log_cb(RETRO_LOG_INFO, "Plugins initialized successfully.\n" );

	return true;
}


// Shuts down all plugins.  Plugins are closed first, if necessary.
// Returns TRUE if a shutdown was performed for any (or all) plugins.  Returns FALSE if all
// plugins were already in shutdown state (no action taken).
//
// In a purist emulation sense, Init() and Shutdown() should only ever need be called for when
// the PS2's hardware has received a *full* hard reset.  Soft resets typically should rely on
// the PS2's bios/kernel to re-initialize hardware on the fly.
//
bool SysCorePlugins::Shutdown()
{
	if( !NeedsShutdown() ) return false;

	pxAssertDev( !NeedsClose(), "Cannot shut down plugins prior to Close()" );
	
	GetMTGS().Cancel();	// cancel it for speedier shutdown!
	
	log_cb(RETRO_LOG_INFO, "Shutting down plugins...\n");

	// Shutdown plugins in reverse order (probably doesn't matter...
	//  ... but what the heck, right?)

	for( int i=PluginId_Count-1; i>=0; --i )
	{
		Shutdown( tbl_PluginInfo[i].id );
	}

	// More memorycard hacks!!

	if( (SysPlugins.Mcd != NULL) && (m_mcdPlugin != NULL) )
	{
		m_mcdPlugin->DeleteComponentInstance( (PS2E_THISPTR)SysPlugins.Mcd );
		SysPlugins.Mcd = NULL;
	}

	log_cb(RETRO_LOG_INFO, "Plugins shutdown successfully.\n");
	
	return true;
}

// For internal use only, unless you're the MTGS.  Then it's for you too!
// Returns false if the plugin returned an error.
bool SysCorePlugins::DoFreeze( PluginsEnum_t pid, int mode, freezeData* data )
{
	if( (pid == PluginId_GS) && !GetMTGS().IsSelf() )
	{
		// GS needs some thread safety love...

		MTGS_FreezeData woot = { data, 0 };
		GetMTGS().Freeze( mode, woot );
		return woot.retval != -1;
	}
	else
	{
		ScopedLock lock( m_mtx_PluginStatus );
		return !m_info[pid] || m_info[pid]->CommonBindings.Freeze( mode, data ) != -1;
	}
}

// Thread Safety:
//   This function should only be called by the Main GUI thread and the GS thread (for GS states only),
//   as it has special handlers to ensure that GS freeze commands are executed appropriately on the
//   GS thread.
//
void SysCorePlugins::Freeze( PluginsEnum_t pid, SaveStateBase& state )
{
	// No locking leeded -- DoFreeze locks as needed, and this avoids MTGS deadlock.
	//ScopedLock lock( m_mtx_PluginStatus );

	freezeData fP = { 0, NULL };
	if( !DoFreeze( pid, FREEZE_SIZE, &fP ) )
		fP.size = 0;

	int fsize = fP.size;
	state.Freeze( fsize );

	log_cb(RETRO_LOG_INFO,
			"%s %s\n", state.IsSaving() ? "Saving" : "Loading",
			tbl_PluginInfo[pid].shortname );

	if( state.IsLoading() && (fsize == 0) )
	{
		// no state data to read, but the plugin expects some state data.
		// Issue a warning to console...
		if( fP.size != 0 )
			log_cb(RETRO_LOG_WARN, "Warning: No data for this plugin was found. Plugin status may be unpredictable.\n" );
		return;

		// Note: Size mismatch check could also be done here on loading, but
		// some plugins may have built-in version support for non-native formats or
		// older versions of a different size... or could give different sizes depending
		// on the status of the plugin when loading, so let's ignore it.
	}

	fP.size = fsize;
	if( fP.size == 0 ) return;

	state.PrepBlock( fP.size );
	fP.data = (s8*)state.GetBlockPtr();

	if( state.IsSaving() )
	{
		if( !DoFreeze(pid, FREEZE_SAVE, &fP) )
			throw Exception::FreezePluginFailure( pid );
	}
	else
	{
		if( !DoFreeze(pid, FREEZE_LOAD, &fP) )
			throw Exception::ThawPluginFailure( pid );
	}

	state.CommitBlock( fP.size );
}

size_t SysCorePlugins::GetFreezeSize( PluginsEnum_t pid )
{
	freezeData fP = { 0, NULL };
	if (!DoFreeze( pid, FREEZE_SIZE, &fP)) return 0;
	return fP.size;
}

void SysCorePlugins::FreezeOut( PluginsEnum_t pid, void* dest )
{
	// No locking needed -- DoFreeze locks as needed, and this avoids MTGS deadlock.
	//ScopedLock lock( m_mtx_PluginStatus );

	freezeData fP = { 0, (s8*)dest };
	if (!DoFreeze( pid, FREEZE_SIZE, &fP)) return;
	if (!fP.size) return;

	log_cb(RETRO_LOG_INFO, "Saving %s\n", tbl_PluginInfo[pid].shortname );

	if (!DoFreeze(pid, FREEZE_SAVE, &fP))
		throw Exception::FreezePluginFailure( pid );
}

void SysCorePlugins::FreezeOut( PluginsEnum_t pid, pxOutputStream& outfp )
{
	// No locking needed -- DoFreeze locks as needed, and this avoids MTGS deadlock.
	//ScopedLock lock( m_mtx_PluginStatus );

	freezeData fP = { 0, NULL };
	if (!DoFreeze( pid, FREEZE_SIZE, &fP)) return;
	if (!fP.size) return;

	log_cb(RETRO_LOG_INFO, "Saving %s\n", tbl_PluginInfo[pid].shortname );

	ScopedAlloc<s8> data( fP.size );
	fP.data = data.GetPtr();

	if (!DoFreeze(pid, FREEZE_SAVE, &fP))
		throw Exception::FreezePluginFailure( pid );

	outfp.Write( fP.data, fP.size );
}

void SysCorePlugins::FreezeIn( PluginsEnum_t pid, pxInputStream& infp )
{
	// No locking needed -- DoFreeze locks as needed, and this avoids MTGS deadlock.
	//ScopedLock lock( m_mtx_PluginStatus );

	freezeData fP = { 0, NULL };
	if (!DoFreeze( pid, FREEZE_SIZE, &fP ))
		fP.size = 0;

	log_cb(RETRO_LOG_INFO, "Loading %s\n", tbl_PluginInfo[pid].shortname );

	if (!infp.IsOk() || !infp.Length())
	{
		// no state data to read, but the plugin expects some state data?
		// Issue a warning to console...
		if( fP.size != 0 )
			log_cb(RETRO_LOG_WARN, "Warning: No data for this plugin was found. Plugin status may be unpredictable.\n" );

		return;

		// Note: Size mismatch check could also be done here on loading, but
		// some plugins may have built-in version support for non-native formats or
		// older versions of a different size... or could give different sizes depending
		// on the status of the plugin when loading, so let's ignore it.
	}

	ScopedAlloc<s8> data( fP.size );
	fP.data = data.GetPtr();

	infp.Read( fP.data, fP.size );
	if (!DoFreeze(pid, FREEZE_LOAD, &fP))
		throw Exception::ThawPluginFailure( pid );
}

bool SysCorePlugins::KeyEvent( const keyEvent& evt )
{
	ScopedLock lock( m_mtx_PluginStatus );

	// [TODO] : The plan here is to give plugins "first chance" handling of keys.
	// Handling order will be fixed (GS, SPU2, PAD, etc), and the first plugin to
	// pick up the key and return "true" (for handled) will cause the loop to break.
	// The current version of PS2E doesn't support it yet, though.

	ForPlugins([&] (const PluginInfo * pi) {
		if( pi->id != PluginId_PAD && m_info[pi->id] )
			m_info[pi->id]->CommonBindings.KeyEvent( const_cast<keyEvent*>(&evt) );
	});

	return false;
}

void SysCorePlugins::SendSettingsFolder()
{
	ScopedLock lock( m_mtx_PluginStatus );
	if( m_SettingsFolder.IsEmpty() ) return;

	ForPlugins([&] (const PluginInfo * pi) {
		if( m_info[pi->id] ) m_info[pi->id]->CommonBindings.SetSettingsDir( m_SettingsFolder.utf8_str() );
	});
}

void SysCorePlugins::SetSettingsFolder( const wxString& folder )
{
	ScopedLock lock( m_mtx_PluginStatus );

	wxString fixedfolder( folder );
	if( !fixedfolder.IsEmpty() && (fixedfolder[fixedfolder.length()-1] != wxFileName::GetPathSeparator() ) )
	{
		fixedfolder += wxFileName::GetPathSeparator();
	}
	
	if( m_SettingsFolder == fixedfolder ) return;
	m_SettingsFolder = fixedfolder;
}

void SysCorePlugins::SendLogFolder()
{
	ScopedLock lock( m_mtx_PluginStatus );
	if( m_LogFolder.IsEmpty() ) return;

	ForPlugins([&] (const PluginInfo * pi) {
		if( m_info[pi->id] ) m_info[pi->id]->CommonBindings.SetLogDir( m_LogFolder.utf8_str() );
	});
}

void SysCorePlugins::SetLogFolder( const wxString& folder )
{
	ScopedLock lock( m_mtx_PluginStatus );

	wxString fixedfolder( folder );
	if( !fixedfolder.IsEmpty() && (fixedfolder[fixedfolder.length()-1] != wxFileName::GetPathSeparator() ) )
	{
		fixedfolder += wxFileName::GetPathSeparator();
	}

	if( m_LogFolder == fixedfolder ) return;
	m_LogFolder = fixedfolder;
}

void SysCorePlugins::Configure( PluginsEnum_t pid )
{
	ScopedLock lock( m_mtx_PluginStatus );
	if( m_info[pid] ) m_info[pid]->CommonBindings.Configure();
}

bool SysCorePlugins::AreLoaded() const
{
	ScopedLock lock( m_mtx_PluginStatus );
	for( int i=0; i<PluginId_Count; ++i )
	{
		if( !m_info[i] ) return false;
	}

	return true;
}

bool SysCorePlugins::AreOpen() const
{
	ScopedLock lock( m_mtx_PluginStatus );

	return IfPlugins([&] (const PluginInfo * pi) {
		return !IsOpen(pi->id);
	});
}

bool SysCorePlugins::AreAnyLoaded() const
{
	ScopedLock lock( m_mtx_PluginStatus );
	for( int i=0; i<PluginId_Count; ++i )
	{
		if( m_info[i] ) return true;
	}

	return false;
}

bool SysCorePlugins::AreAnyInitialized() const
{
	ScopedLock lock( m_mtx_PluginStatus );

	return IfPlugins([&] (const PluginInfo * pi) {
		return IsInitialized(pi->id);
	});
}

bool SysCorePlugins::IsOpen( PluginsEnum_t pid ) const
{
	pxAssert( (uint)pid < PluginId_Count );
	ScopedLock lock( m_mtx_PluginStatus );
	return m_info[pid] && m_info[pid]->IsInitialized && m_info[pid]->IsOpened;
}

bool SysCorePlugins::IsInitialized( PluginsEnum_t pid ) const
{
	pxAssert( (uint)pid < PluginId_Count );
	ScopedLock lock( m_mtx_PluginStatus );
	return m_info[pid] && m_info[pid]->IsInitialized;
}

bool SysCorePlugins::IsLoaded( PluginsEnum_t pid ) const
{
	pxAssert( (uint)pid < PluginId_Count );
	return !!m_info[pid];
}

bool SysCorePlugins::NeedsLoad() const
{
	return IfPlugins([&] (const PluginInfo * pi) {
		return !IsLoaded(pi->id);
	});
}		

bool SysCorePlugins::NeedsUnload() const
{

	return IfPlugins([&] (const PluginInfo * pi) {
		return IsLoaded(pi->id);
	});
}		

bool SysCorePlugins::NeedsInit() const
{
	ScopedLock lock( m_mtx_PluginStatus );

	return IfPlugins([&] (const PluginInfo * pi) {
		return !IsInitialized(pi->id);
	});
}

bool SysCorePlugins::NeedsShutdown() const
{
	ScopedLock lock( m_mtx_PluginStatus );

	return IfPlugins([&] (const PluginInfo * pi) {
		return IsInitialized(pi->id);
	});
}

bool SysCorePlugins::NeedsOpen() const
{
	return IfPlugins([&] (const PluginInfo * pi) {
		return !IsOpen(pi->id);
	});
}

bool SysCorePlugins::NeedsClose() const
{
	return IfPlugins([&] (const PluginInfo * pi) {
		return IsOpen(pi->id);
	});
}

const wxString SysCorePlugins::GetName( PluginsEnum_t pid ) const
{
	ScopedLock lock( m_mtx_PluginStatus );
	pxAssert( (uint)pid < PluginId_Count );
	return m_info[pid] ? m_info[pid]->Name : (wxString)_("Unloaded Plugin");
}

const wxString SysCorePlugins::GetVersion( PluginsEnum_t pid ) const
{
	ScopedLock lock( m_mtx_PluginStatus );
	pxAssert( (uint)pid < PluginId_Count );
	return m_info[pid] ? m_info[pid]->Version : L"0.0";
}
