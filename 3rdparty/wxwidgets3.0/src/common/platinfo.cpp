///////////////////////////////////////////////////////////////////////////////
// Name:        src/common/platinfo.cpp
// Purpose:     implements wxPlatformInfo class
// Author:      Francesco Montorsi
// Modified by:
// Created:     07.07.2006 (based on wxToolkitInfo)
// Copyright:   (c) 2006 Francesco Montorsi
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// for compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#include "wx/platinfo.h"

#ifndef WX_PRECOMP
    #include "wx/app.h"
    #include "wx/utils.h"
#endif //WX_PRECOMP

#include "wx/apptrait.h"

// global object
// VERY IMPORTANT: do not use the default constructor since it would
//                 try to init the wxPlatformInfo instance using
//                 gs_platInfo itself!
static wxPlatformInfo gs_platInfo(wxPORT_UNKNOWN);

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

static const wxChar* const wxOperatingSystemIdNames[] =
{
    wxT("Apple Mac OS"),
    wxT("Apple Mac OS X"),

    wxT("Microsoft Windows 9X"),
    wxT("Microsoft Windows NT"),
    wxT("Microsoft Windows Micro"),
    wxT("Microsoft Windows CE"),

    wxT("Linux"),
    wxT("FreeBSD"),
    wxT("OpenBSD"),
    wxT("NetBSD"),

    wxT("SunOS"),
    wxT("AIX"),
    wxT("HPUX"),

    wxT("Other Unix"),
    wxT("Other Unix"),

    wxT("DOS"),
    wxT("OS/2"),
};

// ----------------------------------------------------------------------------
// wxPlatformInfo
// ----------------------------------------------------------------------------

wxPlatformInfo::wxPlatformInfo()
{
    // just copy platform info for currently running platform
    *this = Get();
}

wxPlatformInfo::wxPlatformInfo(wxPortId pid, int tkMajor, int tkMinor,
                               wxOperatingSystemId id, int osMajor, int osMinor,
                               wxArchitecture arch,
                               wxEndianness endian,
                               bool usingUniversal)
{
    m_tkVersionMajor = tkMajor;
    m_tkVersionMinor = tkMinor;
    m_port = pid;

    m_os = id;
    m_osVersionMajor = osMajor;
    m_osVersionMinor = osMinor;

    m_endian = endian;
    m_arch = arch;
}

bool wxPlatformInfo::operator==(const wxPlatformInfo &t) const
{
    return m_tkVersionMajor == t.m_tkVersionMajor &&
           m_tkVersionMinor == t.m_tkVersionMinor &&
           m_osVersionMajor == t.m_osVersionMajor &&
           m_osVersionMinor == t.m_osVersionMinor &&
           m_os == t.m_os &&
           m_osDesc == t.m_osDesc &&
           m_ldi == t.m_ldi &&
           m_desktopEnv == t.m_desktopEnv &&
           m_port == t.m_port &&
           m_arch == t.m_arch &&
           m_endian == t.m_endian;
}

void wxPlatformInfo::InitForCurrentPlatform()
{
    // autodetect all informations
    const wxAppTraits * const traits = wxTheApp ? wxTheApp->GetTraits() : NULL;
    if ( !traits )
    {
        wxFAIL_MSG( wxT("failed to initialize wxPlatformInfo") );

        m_port = wxPORT_UNKNOWN;
        m_tkVersionMajor =
                m_tkVersionMinor = 0;
    }
    else
    {
        m_port = traits->GetToolkitVersion(&m_tkVersionMajor, &m_tkVersionMinor);
        m_desktopEnv = traits->GetDesktopEnvironment();
    }

    m_os = wxGetOsVersion(&m_osVersionMajor, &m_osVersionMinor);
    m_osDesc = wxGetOsDescription();
    m_endian = wxIsPlatformLittleEndian() ? wxENDIAN_LITTLE : wxENDIAN_BIG;
    m_arch = wxIsPlatform64Bit() ? wxARCH_64 : wxARCH_32;

#ifdef __LINUX__
    m_ldi = wxGetLinuxDistributionInfo();
#endif
    // else: leave m_ldi empty
}

/* static */
const wxPlatformInfo& wxPlatformInfo::Get()
{
    static bool initialized = false;
    if ( !initialized )
    {
        gs_platInfo.InitForCurrentPlatform();
        initialized = true;
    }

    return gs_platInfo;
}

// ----------------------------------------------------------------------------
// wxPlatformInfo - string -> enum conversions
// ----------------------------------------------------------------------------

wxOperatingSystemId wxPlatformInfo::GetOperatingSystemId(const wxString &str)
{
    for ( size_t i = 0; i < WXSIZEOF(wxOperatingSystemIdNames); i++ )
    {
        if ( wxString(wxOperatingSystemIdNames[i]).CmpNoCase(str) == 0 )
            return (wxOperatingSystemId)(1 << i);
    }

    return wxOS_UNKNOWN;
}

wxArchitecture wxPlatformInfo::GetArch(const wxString &arch)
{
    if ( arch.Contains(wxT("32")) )
        return wxARCH_32;

    if ( arch.Contains(wxT("64")) )
        return wxARCH_64;

    return wxARCH_INVALID;
}

wxEndianness wxPlatformInfo::GetEndianness(const wxString& end)
{
    const wxString endl(end.Lower());
    if ( endl.StartsWith(wxT("little")) )
        return wxENDIAN_LITTLE;

    if ( endl.StartsWith(wxT("big")) )
        return wxENDIAN_BIG;

    return wxENDIAN_INVALID;
}

