/////////////////////////////////////////////////////////////////////////////
// Name:        wx/validate.h
// Purpose:     wxValidator class
// Author:      Julian Smart
// Modified by:
// Created:     29/01/98
// Copyright:   (c) 1998 Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_VALIDATE_H_
#define _WX_VALIDATE_H_

#include "wx/defs.h"

// wxWidgets is compiled without support for wxValidator, but we still
// want to be able to pass wxDefaultValidator to the functions which take
// a wxValidator parameter to avoid using "#if wxUSE_VALIDATORS"
// everywhere
class WXDLLIMPEXP_FWD_CORE wxValidator;
static const wxValidator* const wxDefaultValidatorPtr = NULL;
#define wxDefaultValidator (*wxDefaultValidatorPtr)

// this macro allows to avoid warnings about unused parameters when
// wxUSE_VALIDATORS == 0
#define wxVALIDATOR_PARAM(val)

#endif // _WX_VALIDATE_H_

