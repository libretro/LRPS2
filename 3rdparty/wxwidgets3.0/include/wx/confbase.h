///////////////////////////////////////////////////////////////////////////////
// Name:        wx/confbase.h
// Purpose:     declaration of the base class of all config implementations
//              (see also: fileconf.h and msw/regconf.h and iniconf.h)
// Author:      Karsten Ballueder & Vadim Zeitlin
// Modified by:
// Created:     07.04.98 (adapted from appconf.h)
// Copyright:   (c) 1997 Karsten Ballueder   Ballueder@usa.net
//                       Vadim Zeitlin      <zeitlin@dptmaths.ens-cachan.fr>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_CONFBASE_H_
#define _WX_CONFBASE_H_

#include "wx/defs.h"
#include "wx/string.h"
#include "wx/object.h"

class WXDLLIMPEXP_FWD_BASE wxArrayString;

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

/// shall we be case sensitive in parsing variable names?
#ifndef wxCONFIG_CASE_SENSITIVE
  #define  wxCONFIG_CASE_SENSITIVE       0
#endif

/// separates group and entry names (probably shouldn't be changed)
#ifndef wxCONFIG_PATH_SEPARATOR
  #define   wxCONFIG_PATH_SEPARATOR     wxT('/')
#endif

/// introduces immutable entries
// (i.e. the ones which can't be changed from the local config file)
#ifndef wxCONFIG_IMMUTABLE_PREFIX
  #define   wxCONFIG_IMMUTABLE_PREFIX   wxT('!')
#endif

/*
  Replace environment variables ($SOMETHING) with their values. The format is
  $VARNAME or ${VARNAME} where VARNAME contains alphanumeric characters and
  '_' only. '$' must be escaped ('\$') in order to be taken literally.
*/

WXDLLIMPEXP_BASE wxString wxExpandEnvVars(const wxString &sz);

#endif // _WX_CONFBASE_H_

