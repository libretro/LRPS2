/////////////////////////////////////////////////////////////////////////////
// Name:        wx/translation.h
// Purpose:     Internationalization and localisation for wxWidgets
// Author:      Vadim Zeitlin, Vaclav Slavik,
//              Michael N. Filippov <michael@idisys.iae.nsk.su>
// Created:     2010-04-23
// Copyright:   (c) 1998 Vadim Zeitlin <zeitlin@dptmaths.ens-cachan.fr>
//              (c) 2010 Vaclav Slavik <vslavik@fastmail.fm>
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_TRANSLATION_H_
#define _WX_TRANSLATION_H_

#include "wx/defs.h"
#include "wx/string.h"

// the macros should still be defined - otherwise compilation would fail

#if !defined(WXINTL_NO_GETTEXT_MACRO)
    #if !defined(_)
        #define _(s)                 (s)
    #endif
    #define wxPLURAL(sing, plur, n)  ((n) == 1 ? (sing) : (plur))
#endif

#define wxTRANSLATE(str) str

// NB: we use a template here in order to avoid using
//     wxLocale::GetUntranslatedString() above, which would be required if
//     we returned const wxString&; this way, the compiler should be able to
//     optimize wxGetTranslation() away

// define this one just in case it occurs somewhere (instead of preferred
// wxTRANSLATE) too
#if !defined(WXINTL_NO_GETTEXT_MACRO)
    #if !defined(gettext_noop)
        #define gettext_noop(str) (str)
    #endif
    #if !defined(N_)
        #define N_(s)             (s)
    #endif
#endif

#endif // _WX_TRANSLATION_H_
