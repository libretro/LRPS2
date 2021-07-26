/////////////////////////////////////////////////////////////////////////////
// Name:        src/common/utilscmn.cpp
// Purpose:     Miscellaneous utility functions and classes
// Author:      Julian Smart
// Modified by:
// Created:     29/01/98
// Copyright:   (c) 1998 Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

// See comment about this hack in time.cpp: here we do it for environ external
// variable which can't be easily declared when using MinGW in strict ANSI mode.
#ifdef wxNEEDS_STRICT_ANSI_WORKAROUNDS
    #undef __STRICT_ANSI__
    #include <stdlib.h>
    #define __STRICT_ANSI__
#endif

#ifndef WX_PRECOMP
    #include "wx/app.h"
    #include "wx/string.h"
    #include "wx/utils.h"
    #include "wx/intl.h"
    #include "wx/log.h"
#endif // WX_PRECOMP

#include "wx/apptrait.h"

#include "wx/txtstrm.h"
#include "wx/config.h"
#include "wx/versioninfo.h"

#if defined(__WXWINCE__) && wxUSE_DATETIME
    #include "wx/datetime.h"
#endif

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !wxONLY_WATCOM_EARLIER_THAN(1,4)
    #if !(defined(_MSC_VER) && (_MSC_VER > 800))
        #include <errno.h>
    #endif
#endif

#ifndef __WXWINCE__
    #include <time.h>
#else
    #include "wx/msw/wince/time.h"
#endif

#ifdef __WXMAC__
    #include "wx/osx/private.h"
#endif

#if !defined(__WXWINCE__)
    #include <sys/types.h>
    #include <sys/stat.h>
#endif

#if defined(__WINDOWS__)
    #include "wx/msw/private.h"
#endif

#if wxUSE_BASE

// ============================================================================
// implementation
// ============================================================================

// Array used in DecToHex conversion routine.
static const wxChar hexArray[] = wxT("0123456789ABCDEF");

// Convert 2-digit hex number to decimal
int wxHexToDec(const wxString& str)
{
    char buf[2];
    buf[0] = str.GetChar(0);
    buf[1] = str.GetChar(1);
    return wxHexToDec((const char*) buf);
}

// Convert decimal integer to 2-character hex string
void wxDecToHex(int dec, wxChar *buf)
{
    int firstDigit = (int)(dec/16.0);
    int secondDigit = (int)(dec - (firstDigit*16.0));
    buf[0] = hexArray[firstDigit];
    buf[1] = hexArray[secondDigit];
    buf[2] = 0;
}

// Convert decimal integer to 2 characters
void wxDecToHex(int dec, char* ch1, char* ch2)
{
    int firstDigit = (int)(dec/16.0);
    int secondDigit = (int)(dec - (firstDigit*16.0));
    (*ch1) = (char) hexArray[firstDigit];
    (*ch2) = (char) hexArray[secondDigit];
}

// Convert decimal integer to 2-character hex string
wxString wxDecToHex(int dec)
{
    wxChar buf[3];
    wxDecToHex(dec, buf);
    return wxString(buf);
}

// ----------------------------------------------------------------------------
// misc functions
// ----------------------------------------------------------------------------
bool wxIsPlatformLittleEndian()
{
    // Are we little or big endian? This method is from Harbison & Steele.
    union
    {
        long l;
        char c[sizeof(long)];
    } u;
    u.l = 1;

    return u.c[0] == 1;
}


// ----------------------------------------------------------------------------
// wxPlatform
// ----------------------------------------------------------------------------

/*
 * Class to make it easier to specify platform-dependent values
 */

wxArrayInt*  wxPlatform::sm_customPlatforms = NULL;

void wxPlatform::Copy(const wxPlatform& platform)
{
    m_longValue = platform.m_longValue;
    m_doubleValue = platform.m_doubleValue;
    m_stringValue = platform.m_stringValue;
}

wxPlatform wxPlatform::If(int platform, long value)
{
    if (Is(platform))
        return wxPlatform(value);
    else
        return wxPlatform();
}

wxPlatform wxPlatform::IfNot(int platform, long value)
{
    if (!Is(platform))
        return wxPlatform(value);
    else
        return wxPlatform();
}

wxPlatform& wxPlatform::ElseIf(int platform, long value)
{
    if (Is(platform))
        m_longValue = value;
    return *this;
}

wxPlatform& wxPlatform::ElseIfNot(int platform, long value)
{
    if (!Is(platform))
        m_longValue = value;
    return *this;
}

wxPlatform wxPlatform::If(int platform, double value)
{
    if (Is(platform))
        return wxPlatform(value);
    else
        return wxPlatform();
}

wxPlatform wxPlatform::IfNot(int platform, double value)
{
    if (!Is(platform))
        return wxPlatform(value);
    else
        return wxPlatform();
}

wxPlatform& wxPlatform::ElseIf(int platform, double value)
{
    if (Is(platform))
        m_doubleValue = value;
    return *this;
}

wxPlatform& wxPlatform::ElseIfNot(int platform, double value)
{
    if (!Is(platform))
        m_doubleValue = value;
    return *this;
}

wxPlatform wxPlatform::If(int platform, const wxString& value)
{
    if (Is(platform))
        return wxPlatform(value);
    else
        return wxPlatform();
}

wxPlatform wxPlatform::IfNot(int platform, const wxString& value)
{
    if (!Is(platform))
        return wxPlatform(value);
    else
        return wxPlatform();
}

wxPlatform& wxPlatform::ElseIf(int platform, const wxString& value)
{
    if (Is(platform))
        m_stringValue = value;
    return *this;
}

wxPlatform& wxPlatform::ElseIfNot(int platform, const wxString& value)
{
    if (!Is(platform))
        m_stringValue = value;
    return *this;
}

wxPlatform& wxPlatform::Else(long value)
{
    m_longValue = value;
    return *this;
}

wxPlatform& wxPlatform::Else(double value)
{
    m_doubleValue = value;
    return *this;
}

wxPlatform& wxPlatform::Else(const wxString& value)
{
    m_stringValue = value;
    return *this;
}

void wxPlatform::AddPlatform(int platform)
{
    if (!sm_customPlatforms)
        sm_customPlatforms = new wxArrayInt;
    sm_customPlatforms->Add(platform);
}

void wxPlatform::ClearPlatforms()
{
    wxDELETE(sm_customPlatforms);
}

/// Function for testing current platform

bool wxPlatform::Is(int platform)
{
#ifdef __WINDOWS__
    if (platform == wxOS_WINDOWS)
        return true;
#endif
#ifdef __WXWINCE__
    if (platform == wxOS_WINDOWS_CE)
        return true;
#endif

#if 0

// FIXME: wxWinPocketPC and wxWinSmartPhone are unknown symbols

#if defined(__WXWINCE__) && defined(__POCKETPC__)
    if (platform == wxWinPocketPC)
        return true;
#endif
#if defined(__WXWINCE__) && defined(__SMARTPHONE__)
    if (platform == wxWinSmartPhone)
        return true;
#endif

#endif

#ifdef __WXGTK__
    if (platform == wxPORT_GTK)
        return true;
#endif
#ifdef __WXMAC__
    if (platform == wxPORT_MAC)
        return true;
#endif
#ifdef __WXX11__
    if (platform == wxPORT_X11)
        return true;
#endif
#ifdef __UNIX__
    if (platform == wxOS_UNIX)
        return true;
#endif
#ifdef __OS2__
    if (platform == wxOS_OS2)
        return true;
#endif
#ifdef __WXPM__
    if (platform == wxPORT_PM)
        return true;
#endif
#ifdef __WXCOCOA__
    if (platform == wxPORT_MAC)
        return true;
#endif

    if (sm_customPlatforms && sm_customPlatforms->Index(platform) != wxNOT_FOUND)
        return true;

    return false;
}

// ----------------------------------------------------------------------------
// user id functions
// ----------------------------------------------------------------------------

wxString wxGetUserId()
{
    static const int maxLoginLen = 256; // FIXME arbitrary number

    wxString buf;
    bool ok = wxGetUserId(wxStringBuffer(buf, maxLoginLen), maxLoginLen);

    if ( !ok )
        buf.Empty();

    return buf;
}

wxString wxGetHomeDir()
{
    wxString home;
    wxGetHomeDir(&home);

    return home;
}

// ----------------------------------------------------------------------------
// Environment
// ----------------------------------------------------------------------------

#ifdef __WXOSX__
#if wxOSX_USE_COCOA_OR_CARBON
    #include <crt_externs.h>
#endif
#endif

bool wxGetEnvMap(wxEnvVariableHashMap *map)
{
    wxCHECK_MSG( map, false, wxS("output pointer can't be NULL") );

#if defined(__VISUALC__)
    // This variable only exists to force the CRT to fill the wide char array,
    // it might only have it in narrow char version until now as we use main()
    // (and not _wmain()) as our entry point.
    static wxChar* s_dummyEnvVar = _tgetenv(wxT("TEMP"));

    wxChar **env = _tenviron;
#elif defined(__VMS)
   // Now this routine wil give false for OpenVMS
   // TODO : should we do something with logicals?
    char **env=NULL;
#elif defined(__DARWIN__)
#if wxOSX_USE_COCOA_OR_CARBON
    // Under Mac shared libraries don't have access to the global environ
    // variable so use this Mac-specific function instead as advised by
    // environ(7) under Darwin
    char ***penv = _NSGetEnviron();
    if ( !penv )
        return false;
    char **env = *penv;
#else
    char **env=NULL;
    // todo translate NSProcessInfo environment into map
#endif
#else // non-MSVC non-Mac
    // Not sure if other compilers have _tenviron so use the (more standard)
    // ANSI version only for them.

    // Both POSIX and Single UNIX Specification say that this variable must
    // exist but not that it must be declared anywhere and, indeed, it's not
    // declared in several common systems (some BSDs, Solaris with native CC)
    // so we (re)declare it ourselves to deal with these cases. However we do
    // not do this under MSW where there can be DLL-related complications, i.e.
    // the variable might be DLL-imported or not. Luckily we don't have to
    // worry about this as all MSW compilers do seem to define it in their
    // standard headers anyhow so we can just rely on already having the
    // correct declaration. And if this turns out to be wrong, we can always
    // add a configure test checking whether it is declared later.
#ifndef __WINDOWS__
    extern char **environ;
#endif // !__WINDOWS__

    char **env = environ;
#endif

    if ( env )
    {
        wxString name,
                 value;
        while ( *env )
        {
            const wxString var(*env);

            name = var.BeforeFirst(wxS('='), &value);

            (*map)[name] = value;

            env++;
        }

        return true;
    }

    return false;
}

// ----------------------------------------------------------------------------
// wxQsort, adapted by RR to allow user_data
// ----------------------------------------------------------------------------

/* This file is part of the GNU C Library.
   Written by Douglas C. Schmidt (schmidt@ics.uci.edu).

   Douglas Schmidt kindly gave permission to relicence the
   code under the wxWindows licence:

From: "Douglas C. Schmidt" <schmidt@dre.vanderbilt.edu>
To: Robert Roebling <robert.roebling@uni-ulm.de>
Subject: Re: qsort licence
Date: Mon, 23 Jul 2007 03:44:25 -0500
Sender: schmidt@dre.vanderbilt.edu
Message-Id: <20070723084426.64F511000A8@tango.dre.vanderbilt.edu>

Hi Robert,

> [...] I'm asking if you'd be willing to relicence your code
> under the wxWindows licence. [...]

That's fine with me [...]

Thanks,

     Doug */


/* Byte-wise swap two items of size SIZE. */
#define SWAP(a, b, size)                                                      \
  do                                                                          \
    {                                                                         \
      size_t __size = (size);                                                 \
      char *__a = (a), *__b = (b);                                            \
      do                                                                      \
        {                                                                     \
          char __tmp = *__a;                                                  \
          *__a++ = *__b;                                                      \
          *__b++ = __tmp;                                                     \
        } while (--__size > 0);                                               \
    } while (0)

/* Discontinue quicksort algorithm when partition gets below this size.
   This particular magic number was chosen to work best on a Sun 4/260. */
#define MAX_THRESH 4

/* Stack node declarations used to store unfulfilled partition obligations. */
typedef struct
  {
    char *lo;
    char *hi;
  } stack_node;

/* The next 4 #defines implement a very fast in-line stack abstraction. */
#define STACK_SIZE        (8 * sizeof(unsigned long int))
#define PUSH(low, high)   ((void) ((top->lo = (low)), (top->hi = (high)), ++top))
#define POP(low, high)    ((void) (--top, (low = top->lo), (high = top->hi)))
#define STACK_NOT_EMPTY   (stack < top)


/* Order size using quicksort.  This implementation incorporates
   four optimizations discussed in Sedgewick:

   1. Non-recursive, using an explicit stack of pointer that store the
      next array partition to sort.  To save time, this maximum amount
      of space required to store an array of MAX_INT is allocated on the
      stack.  Assuming a 32-bit integer, this needs only 32 *
      sizeof(stack_node) == 136 bits.  Pretty cheap, actually.

   2. Chose the pivot element using a median-of-three decision tree.
      This reduces the probability of selecting a bad pivot value and
      eliminates certain extraneous comparisons.

   3. Only quicksorts TOTAL_ELEMS / MAX_THRESH partitions, leaving
      insertion sort to order the MAX_THRESH items within each partition.
      This is a big win, since insertion sort is faster for small, mostly
      sorted array segments.

   4. The larger of the two sub-partitions is always pushed onto the
      stack first, with the algorithm then concentrating on the
      smaller partition.  This *guarantees* no more than log (n)
      stack size is needed (actually O(1) in this case)!  */

void wxQsort(void* pbase, size_t total_elems,
             size_t size, wxSortCallback cmp, const void* user_data)
{
  char *base_ptr = (char *) pbase;
  const size_t max_thresh = MAX_THRESH * size;

  if (total_elems == 0)
    /* Avoid lossage with unsigned arithmetic below.  */
    return;

  if (total_elems > MAX_THRESH)
    {
      char *lo = base_ptr;
      char *hi = &lo[size * (total_elems - 1)];
      stack_node stack[STACK_SIZE];
      stack_node *top = stack;

      PUSH (NULL, NULL);

      while (STACK_NOT_EMPTY)
        {
          char *left_ptr;
          char *right_ptr;

          /* Select median value from among LO, MID, and HI. Rearrange
             LO and HI so the three values are sorted. This lowers the
             probability of picking a pathological pivot value and
             skips a comparison for both the LEFT_PTR and RIGHT_PTR. */

          char *mid = lo + size * ((hi - lo) / size >> 1);

          if ((*cmp) ((void *) mid, (void *) lo, user_data) < 0)
            SWAP (mid, lo, size);
          if ((*cmp) ((void *) hi, (void *) mid, user_data) < 0)
            SWAP (mid, hi, size);
          else
            goto jump_over;
          if ((*cmp) ((void *) mid, (void *) lo, user_data) < 0)
            SWAP (mid, lo, size);
        jump_over:;
          left_ptr  = lo + size;
          right_ptr = hi - size;

          /* Here's the famous ``collapse the walls'' section of quicksort.
             Gotta like those tight inner loops!  They are the main reason
             that this algorithm runs much faster than others. */
          do
            {
              while ((*cmp) ((void *) left_ptr, (void *) mid, user_data) < 0)
                left_ptr += size;

              while ((*cmp) ((void *) mid, (void *) right_ptr, user_data) < 0)
                right_ptr -= size;

              if (left_ptr < right_ptr)
                {
                  SWAP (left_ptr, right_ptr, size);
                  if (mid == left_ptr)
                    mid = right_ptr;
                  else if (mid == right_ptr)
                    mid = left_ptr;
                  left_ptr += size;
                  right_ptr -= size;
                }
              else if (left_ptr == right_ptr)
                {
                  left_ptr += size;
                  right_ptr -= size;
                  break;
                }
            }
          while (left_ptr <= right_ptr);

          /* Set up pointers for next iteration.  First determine whether
             left and right partitions are below the threshold size.  If so,
             ignore one or both.  Otherwise, push the larger partition's
             bounds on the stack and continue sorting the smaller one. */

          if ((size_t) (right_ptr - lo) <= max_thresh)
            {
              if ((size_t) (hi - left_ptr) <= max_thresh)
                /* Ignore both small partitions. */
                POP (lo, hi);
              else
                /* Ignore small left partition. */
                lo = left_ptr;
            }
          else if ((size_t) (hi - left_ptr) <= max_thresh)
            /* Ignore small right partition. */
            hi = right_ptr;
          else if ((right_ptr - lo) > (hi - left_ptr))
            {
              /* Push larger left partition indices. */
              PUSH (lo, right_ptr);
              lo = left_ptr;
            }
          else
            {
              /* Push larger right partition indices. */
              PUSH (left_ptr, hi);
              hi = right_ptr;
            }
        }
    }

  /* Once the BASE_PTR array is partially sorted by quicksort the rest
     is completely sorted using insertion sort, since this is efficient
     for partitions below MAX_THRESH size. BASE_PTR points to the beginning
     of the array to sort, and END_PTR points at the very last element in
     the array (*not* one beyond it!). */

  {
    char *const end_ptr = &base_ptr[size * (total_elems - 1)];
    char *tmp_ptr = base_ptr;
    char *thresh = base_ptr + max_thresh;
    if ( thresh > end_ptr )
        thresh = end_ptr;
    char *run_ptr;

    /* Find smallest element in first threshold and place it at the
       array's beginning.  This is the smallest array element,
       and the operation speeds up insertion sort's inner loop. */

    for (run_ptr = tmp_ptr + size; run_ptr <= thresh; run_ptr += size)
      if ((*cmp) ((void *) run_ptr, (void *) tmp_ptr, user_data) < 0)
        tmp_ptr = run_ptr;

    if (tmp_ptr != base_ptr)
      SWAP (tmp_ptr, base_ptr, size);

    /* Insertion sort, running from left-hand-side up to right-hand-side.  */

    run_ptr = base_ptr + size;
    while ((run_ptr += size) <= end_ptr)
      {
        tmp_ptr = run_ptr - size;
        while ((*cmp) ((void *) run_ptr, (void *) tmp_ptr, user_data) < 0)
          tmp_ptr -= size;

        tmp_ptr += size;
        if (tmp_ptr != run_ptr)
          {
            char *trav;

            trav = run_ptr + size;
            while (--trav >= run_ptr)
              {
                char c = *trav;
                char *hi, *lo;

                for (hi = lo = trav; (lo -= size) >= tmp_ptr; hi = lo)
                  *hi = *lo;
                *hi = c;
              }
          }
      }
  }
}

#endif // wxUSE_BASE
