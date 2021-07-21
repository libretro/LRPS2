/////////////////////////////////////////////////////////////////////////////
// Name:        wx/intl.h
// Purpose:     Internationalization and localisation for wxWidgets
// Author:      Vadim Zeitlin
// Modified by: Michael N. Filippov <michael@idisys.iae.nsk.su>
//              (2003/09/30 - plural forms support)
// Created:     29/01/98
// Copyright:   (c) 1998 Vadim Zeitlin <zeitlin@dptmaths.ens-cachan.fr>
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_INTL_H_
#define _WX_INTL_H_

#include "wx/defs.h"
#include "wx/string.h"
#include "wx/translation.h"

// Make wxLayoutDirection enum available without need for wxUSE_INTL so wxWindow, wxApp
// and other classes are not distrubed by wxUSE_INTL

enum wxLayoutDirection
{
    wxLayout_Default,
    wxLayout_LeftToRight,
    wxLayout_RightToLeft
};

#endif // _WX_INTL_H_
