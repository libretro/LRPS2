/**
*  Name:        wx/features.h
*  Purpose:     test macros for the features which might be available in some
*               wxWidgets ports but not others
*  Author:      Vadim Zeitlin
*  Modified by: Ryan Norton (Converted to C)
*  Created:     18.03.02
*  Copyright:   (c) 2002 Vadim Zeitlin <vadim@wxwidgets.org>
*  Licence:     wxWindows licence
*/

/* THIS IS A C FILE, DON'T USE C++ FEATURES (IN PARTICULAR COMMENTS) IN IT */

#ifndef _WX_FEATURES_H_
#define _WX_FEATURES_H_

/* This is defined when the compiler provides some type of extended locale
   functions.  Otherwise, we implement them ourselves to only support the
   'C' locale */
#if defined(HAVE_LOCALE_T) || (wxCHECK_VISUALC_VERSION(8))
    #define wxHAS_XLOCALE_SUPPORT
#else
    #undef wxHAS_XLOCALE_SUPPORT
#endif

#endif /*  _WX_FEATURES_H_ */
