///////////////////////////////////////////////////////////////////////////////
// Name:        src/common/config.cpp
// Purpose:     implementation of wxConfigBase class
// Author:      Vadim Zeitlin
// Modified by:
// Created:     07.04.98
// Copyright:   (c) 1997 Karsten Ballueder  Ballueder@usa.net
//                       Vadim Zeitlin      <zeitlin@dptmaths.ens-cachan.fr>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#include "wx/wxprec.h"

#ifndef WX_PRECOMP
    #include "wx/app.h"
    #include "wx/utils.h"
    #include "wx/arrstr.h"
    #include "wx/math.h"
#endif //WX_PRECOMP

// ----------------------------------------------------------------------------
// static & global functions
// ----------------------------------------------------------------------------

// understands both Unix and Windows (but only under Windows) environment
// variables expansion: i.e. $var, $(var) and ${var} are always understood
// and in addition under Windows %var% is also.

// don't change the values the enum elements: they must be equal
// to the matching [closing] delimiter.
enum Bracket
{
  Bracket_None,
  Bracket_Normal  = ')',
  Bracket_Curly   = '}',
#ifdef  __WINDOWS__
  Bracket_Windows = '%',    // yeah, Windows people are a bit strange ;-)
#endif
  Bracket_Max
};

wxString wxExpandEnvVars(const wxString& str)
{
  wxString strResult;
  strResult.Alloc(str.length());

  size_t m;
  for ( size_t n = 0; n < str.length(); n++ ) {
    switch ( str[n].GetValue() ) {
#ifdef __WINDOWS__
      case wxT('%'):
#endif // __WINDOWS__
      case wxT('$'):
        {
          Bracket bracket;
          #ifdef __WINDOWS__
            if ( str[n] == wxT('%') )
              bracket = Bracket_Windows;
            else
          #endif // __WINDOWS__
          if ( n == str.length() - 1 ) {
            bracket = Bracket_None;
          }
          else {
            switch ( str[n + 1].GetValue() ) {
              case wxT('('):
                bracket = Bracket_Normal;
                n++;                   // skip the bracket
                break;

              case wxT('{'):
                bracket = Bracket_Curly;
                n++;                   // skip the bracket
                break;

              default:
                bracket = Bracket_None;
            }
          }

          m = n + 1;

          while ( m < str.length() && (wxIsalnum(str[m]) || str[m] == wxT('_')) )
            m++;

          wxString strVarName(str.c_str() + n + 1, m - n - 1);

          // NB: use wxGetEnv instead of wxGetenv as otherwise variables
          //     set through wxSetEnv may not be read correctly!
          bool expanded = false;
          wxString tmp;
          if (wxGetEnv(strVarName, &tmp))
          {
              strResult += tmp;
              expanded = true;
          }
          else
          {
            // variable doesn't exist => don't change anything
            #ifdef  __WINDOWS__
              if ( bracket != Bracket_Windows )
            #endif
                if ( bracket != Bracket_None )
                  strResult << str[n - 1];
            strResult << str[n] << strVarName;
          }

          // check the closing bracket
          if ( bracket != Bracket_None ) {
            if ( m == str.length() || str[m] != (wxChar)bracket ) {
              // under MSW it's common to have '%' characters in the registry
              // and it's annoying to have warnings about them each time, so
              // ignroe them silently if they are not used for env vars
              //
              // under Unix, OTOH, this warning could be useful for the user to
              // understand why isn't the variable expanded as intended
            }
            else {
              // skip closing bracket unless the variables wasn't expanded
              if ( !expanded )
                strResult << (wxChar)bracket;
              m++;
            }
          }

          n = m - 1;  // skip variable name
        }
        break;

      case wxT('\\'):
        // backslash can be used to suppress special meaning of % and $
        if ( n != str.length() - 1 &&
                (str[n + 1] == wxT('%') || str[n + 1] == wxT('$')) ) {
          strResult += str[++n];

          break;
        }
        //else: fall through

      default:
        strResult += str[n];
    }
  }

  return strResult;
}
