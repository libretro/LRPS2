/////////////////////////////////////////////////////////////////////////////
// Name:        src/common/object.cpp
// Purpose:     wxObject implementation
// Author:      Julian Smart
// Modified by: Ron Lee
// Created:     04/01/98
// Copyright:   (c) 1998 Julian Smart
//              (c) 2001 Ron Lee <ron@debian.org>
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifndef WX_PRECOMP
    #include "wx/object.h"
    #include "wx/hash.h"
    #include "wx/memory.h"
    #include "wx/crt.h"
#endif

#include <string.h>

// we must disable optimizations for VC.NET because otherwise its too eager
// linker discards wxClassInfo objects in release build thus breaking many,
// many things
#if defined __VISUALC__ && __VISUALC__ >= 1300
    #pragma optimize("", off)
#endif

wxClassInfo wxObject::ms_classInfo( wxT("wxObject"), 0, 0,
                                        (int) sizeof(wxObject),
                                        (wxObjectConstructorFn) 0 );

// restore optimizations
#if defined __VISUALC__ && __VISUALC__ >= 1300
    #pragma optimize("", on)
#endif

wxClassInfo* wxClassInfo::sm_first = NULL;
wxHashTable* wxClassInfo::sm_classTable = NULL;

// when using XTI, this method is already implemented inline inside
// DECLARE_DYNAMIC_CLASS but otherwise we intentionally make this function
// non-inline because this allows us to have a non-inline virtual function in
// all wx classes and this solves linking problems for HP-UX native toolchain
// and possibly others (we could make dtor non-inline as well but it's more
// useful to keep it inline than this function)

wxClassInfo *wxObject::GetClassInfo() const
{
    return &wxObject::ms_classInfo;
}

// this variable exists only so that we can avoid 'always true/false' warnings
const bool wxFalse = false;

// Is this object a kind of (a subclass of) 'info'?
// E.g. is wxWindow a kind of wxObject?
// Go from this class to superclass, taking into account
// two possible base classes.
bool wxObject::IsKindOf(const wxClassInfo *info) const
{
    const wxClassInfo *thisInfo = GetClassInfo();
    return (thisInfo) ? thisInfo->IsKindOf(info) : false ;
}

#ifdef _WX_WANT_NEW_SIZET_WXCHAR_INT
void *wxObject::operator new ( size_t size, const wxChar *fileName, int lineNum )
{
    return wxDebugAlloc(size, (wxChar*) fileName, lineNum, true);
}
#endif

#ifdef _WX_WANT_DELETE_VOID
void wxObject::operator delete ( void *buf )
{
    wxDebugFree(buf);
}
#endif

#ifdef _WX_WANT_DELETE_VOID_CONSTCHAR_SIZET
void wxObject::operator delete ( void *buf, const char *_fname, size_t _line )
{
    wxDebugFree(buf);
}
#endif

#ifdef _WX_WANT_DELETE_VOID_WXCHAR_INT
void wxObject::operator delete ( void *buf, const wxChar *WXUNUSED(fileName), int WXUNUSED(lineNum) )
{
     wxDebugFree(buf);
}
#endif

#ifdef _WX_WANT_ARRAY_NEW_SIZET_WXCHAR_INT
void *wxObject::operator new[] ( size_t size, const wxChar* fileName, int lineNum )
{
    return wxDebugAlloc(size, (wxChar*) fileName, lineNum, true, true);
}
#endif

#ifdef _WX_WANT_ARRAY_DELETE_VOID
void wxObject::operator delete[] ( void *buf )
{
    wxDebugFree(buf, true);
}
#endif

#ifdef _WX_WANT_ARRAY_DELETE_VOID_WXCHAR_INT
void wxObject::operator delete[] (void * buf, const wxChar*  WXUNUSED(fileName), int WXUNUSED(lineNum) )
{
    wxDebugFree(buf, true);
}
#endif


// ----------------------------------------------------------------------------
// wxClassInfo
// ----------------------------------------------------------------------------

wxClassInfo::~wxClassInfo()
{
    // remove this object from the linked list of all class infos: if we don't
    // do it, loading/unloading a DLL containing static wxClassInfo objects is
    // not going to work
    if ( this == sm_first )
    {
        sm_first = m_next;
    }
    else
    {
        wxClassInfo *info = sm_first;
        while (info)
        {
            if ( info->m_next == this )
            {
                info->m_next = m_next;
                break;
            }

            info = info->m_next;
        }
    }
    Unregister();
}

// Reentrance can occur on some platforms (Solaris for one), as the use of hash
// and string objects can cause other modules to load and register classes
// before the original call returns. This is handled by keeping the hash table
// local when it is first created and only assigning it to the global variable
// when the function is ready to return.
//
// That does make the assumption that after the function has completed the
// first time the problem will no longer happen; all the modules it depends on
// will have been loaded. The assumption is checked using the 'entry' variable
// as a reentrance guard, it checks that once the hash table is global it is
// not accessed multiple times simulateously.

void wxClassInfo::Register()
{
    wxHashTable *classTable;

    // keep the hash local initially, reentrance is possible
    if ( !sm_classTable )
        classTable = new wxHashTable(wxKEY_STRING);
    else
        classTable = sm_classTable;

    classTable->Put(m_className, (wxObject *)this);

    // if we're using a local hash we need to try to make it global
    if ( sm_classTable != classTable )
    {
        if ( !sm_classTable )
        {
            // make the hash global
            sm_classTable = classTable;
        }
        else
        {
            // the gobal hash has already been created by a reentrant call,
            // so delete the local hash and try again
            delete classTable;
            Register();
        }
    }
}

void wxClassInfo::Unregister()
{
    if ( sm_classTable )
    {
        sm_classTable->Delete(m_className);
        if ( sm_classTable->GetCount() == 0 )
        {
            wxDELETE(sm_classTable);
        }
    }
}

// iterator interface
wxClassInfo::const_iterator::value_type
wxClassInfo::const_iterator::operator*() const
{
    return (wxClassInfo*)m_node->GetData();
}

wxClassInfo::const_iterator& wxClassInfo::const_iterator::operator++()
{
    m_node = m_table->Next();
    return *this;
}

const wxClassInfo::const_iterator wxClassInfo::const_iterator::operator++(int)
{
    wxClassInfo::const_iterator tmp = *this;
    m_node = m_table->Next();
    return tmp;
}

wxClassInfo::const_iterator wxClassInfo::begin_classinfo()
{
    sm_classTable->BeginFind();

    return const_iterator(sm_classTable->Next(), sm_classTable);
}

wxClassInfo::const_iterator wxClassInfo::end_classinfo()
{
    return const_iterator(NULL, NULL);
}

// ----------------------------------------------------------------------------
// wxObjectRefData
// ----------------------------------------------------------------------------

void wxRefCounter::DecRef()
{
    if ( --m_count == 0 )
        delete this;
}


// ----------------------------------------------------------------------------
// wxObject
// ----------------------------------------------------------------------------

void wxObject::Ref(const wxObject& clone)
{
    // nothing to be done
    if (m_refData == clone.m_refData)
        return;

    // delete reference to old data
    UnRef();

    // reference new data
    if ( clone.m_refData )
    {
        m_refData = clone.m_refData;
        m_refData->IncRef();
    }
}

void wxObject::UnRef()
{
    if ( m_refData )
    {
        m_refData->DecRef();
        m_refData = NULL;
    }
}
