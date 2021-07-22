///////////////////////////////////////////////////////////////////////////////
// Name:        wx/msw/seh.h
// Purpose:     declarations for SEH (structured exceptions handling) support
// Author:      Vadim Zeitlin
// Created:     2006-04-26
// Copyright:   (c) 2006 Vadim Zeitlin <vadim@wxwindows.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_MSW_SEH_H_
#define _WX_MSW_SEH_H_

#define wxSEH_TRY
#define wxSEH_IGNORE
#define wxSEH_HANDLE(rc)

// the other compilers do nothing as stupid by default so nothing to do for
// them
#define DisableAutomaticSETranslator()

#endif // _WX_MSW_SEH_H_
