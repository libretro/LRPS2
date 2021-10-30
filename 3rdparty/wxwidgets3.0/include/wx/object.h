/////////////////////////////////////////////////////////////////////////////
// Name:        wx/object.h
// Purpose:     wxObject class, plus run-time type information macros
// Author:      Julian Smart
// Modified by: Ron Lee
// Created:     01/02/97
// Copyright:   (c) 1997 Julian Smart
//              (c) 2001 Ron Lee <ron@debian.org>
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_OBJECTH__
#define _WX_OBJECTH__

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#include "wx/memory.h"

#define wxDECLARE_CLASS_INFO_ITERATORS()                                     \
class WXDLLIMPEXP_BASE const_iterator                                    \
    {                                                                        \
    typedef wxHashTable_Node Node;                                       \
    public:                                                                  \
    typedef const wxClassInfo* value_type;                               \
    typedef const value_type& const_reference;                           \
    typedef const_iterator itor;                                         \
    typedef value_type* ptr_type;                                        \
    \
    Node* m_node;                                                        \
    wxHashTable* m_table;                                                \
    public:                                                                  \
    typedef const_reference reference_type;                              \
    typedef ptr_type pointer_type;                                       \
    \
    const_iterator(Node* node, wxHashTable* table)                       \
    : m_node(node), m_table(table) { }                               \
    const_iterator() : m_node(NULL), m_table(NULL) { }                   \
    value_type operator*() const;                                        \
    itor& operator++();                                                  \
    const itor operator++(int);                                          \
    bool operator!=(const itor& it) const                                \
            { return it.m_node != m_node; }                                  \
            bool operator==(const itor& it) const                                \
            { return it.m_node == m_node; }                                  \
    };                                                                       \
    \
    static const_iterator begin_classinfo();                                 \
    static const_iterator end_classinfo()

// based on the value of wxUSE_EXTENDED_RTTI symbol,
// only one of the RTTI system will be compiled:
// - the "old" one (defined by rtti.h) or
// - the "new" one (defined by xti.h)
#include "wx/rtti.h"

#define wxIMPLEMENT_CLASS(name, basename)                                     \
    wxIMPLEMENT_ABSTRACT_CLASS(name, basename)

#define wxIMPLEMENT_CLASS2(name, basename1, basename2)                        \
    wxIMPLEMENT_ABSTRACT_CLASS2(name, basename1, basename2)

// -----------------------------------
// for pluggable classes
// -----------------------------------

    // NOTE: this should probably be the very first statement
    //       in the class declaration so wxPluginSentinel is
    //       the first member initialised and the last destroyed.

// _DECLARE_DL_SENTINEL(name) wxPluginSentinel m_pluginsentinel;

#if wxUSE_NESTED_CLASSES

#define _DECLARE_DL_SENTINEL(name, exportdecl)  \
class exportdecl name##PluginSentinel {         \
private:                                        \
    static const wxString sm_className;         \
public:                                         \
    name##PluginSentinel();                     \
    ~name##PluginSentinel();                    \
};                                              \
name##PluginSentinel  m_pluginsentinel

#define _IMPLEMENT_DL_SENTINEL(name)                                \
 const wxString name::name##PluginSentinel::sm_className(#name);    \
 name::name##PluginSentinel::name##PluginSentinel() {               \
    wxPluginLibrary *e = (wxPluginLibrary*) wxPluginLibrary::ms_classes.Get(#name);   \
    if( e != 0 ) { e->RefObj(); }                                      \
 }                                                                  \
 name::name##PluginSentinel::~name##PluginSentinel() {            \
    wxPluginLibrary *e = (wxPluginLibrary*) wxPluginLibrary::ms_classes.Get(#name);   \
    if( e != 0 ) { e->UnrefObj(); }                                 \
 }
#else

#define _DECLARE_DL_SENTINEL(name)
#define _IMPLEMENT_DL_SENTINEL(name)

#endif  // wxUSE_NESTED_CLASSES

#define wxDECLARE_PLUGGABLE_CLASS(name) \
 wxDECLARE_DYNAMIC_CLASS(name); _DECLARE_DL_SENTINEL(name, WXDLLIMPEXP_CORE)
#define wxDECLARE_ABSTRACT_PLUGGABLE_CLASS(name)  \
 wxDECLARE_ABSTRACT_CLASS(name); _DECLARE_DL_SENTINEL(name, WXDLLIMPEXP_CORE)

#define wxDECLARE_USER_EXPORTED_PLUGGABLE_CLASS(name, usergoo) \
 wxDECLARE_DYNAMIC_CLASS(name); _DECLARE_DL_SENTINEL(name, usergoo)
#define wxDECLARE_USER_EXPORTED_ABSTRACT_PLUGGABLE_CLASS(name, usergoo)  \
 wxDECLARE_ABSTRACT_CLASS(name); _DECLARE_DL_SENTINEL(name, usergoo)

#define wxIMPLEMENT_PLUGGABLE_CLASS(name, basename) \
 wxIMPLEMENT_DYNAMIC_CLASS(name, basename) _IMPLEMENT_DL_SENTINEL(name)
#define wxIMPLEMENT_PLUGGABLE_CLASS2(name, basename1, basename2)  \
 wxIMPLEMENT_DYNAMIC_CLASS2(name, basename1, basename2) _IMPLEMENT_DL_SENTINEL(name)
#define wxIMPLEMENT_ABSTRACT_PLUGGABLE_CLASS(name, basename) \
 wxIMPLEMENT_ABSTRACT_CLASS(name, basename) _IMPLEMENT_DL_SENTINEL(name)
#define wxIMPLEMENT_ABSTRACT_PLUGGABLE_CLASS2(name, basename1, basename2)  \
 wxIMPLEMENT_ABSTRACT_CLASS2(name, basename1, basename2) _IMPLEMENT_DL_SENTINEL(name)

#define wxIMPLEMENT_USER_EXPORTED_PLUGGABLE_CLASS(name, basename) \
 wxIMPLEMENT_PLUGGABLE_CLASS(name, basename)
#define wxIMPLEMENT_USER_EXPORTED_PLUGGABLE_CLASS2(name, basename1, basename2)  \
 wxIMPLEMENT_PLUGGABLE_CLASS2(name, basename1, basename2)
#define wxIMPLEMENT_USER_EXPORTED_ABSTRACT_PLUGGABLE_CLASS(name, basename) \
 wxIMPLEMENT_ABSTRACT_PLUGGABLE_CLASS(name, basename)
#define wxIMPLEMENT_USER_EXPORTED_ABSTRACT_PLUGGABLE_CLASS2(name, basename1, basename2)  \
 wxIMPLEMENT_ABSTRACT_PLUGGABLE_CLASS2(name, basename1, basename2)

#define wxCLASSINFO(name) (&name::ms_classInfo)

#define wxIS_KIND_OF(obj, className) obj->IsKindOf(&className::ms_classInfo)

// Just seems a bit nicer-looking (pretend it's not a macro)
#define wxIsKindOf(obj, className) obj->IsKindOf(&className::ms_classInfo)

// this cast does some more checks at compile time as it uses static_cast
// internally
//
// note that it still has different semantics from dynamic_cast<> and so can't
// be replaced by it as long as there are any compilers not supporting it
#define wxDynamicCast(obj, className) \
    ((className *) wxCheckDynamicCast( \
        const_cast<wxObject *>(static_cast<const wxObject *>(\
          const_cast<className *>(static_cast<const className *>(obj)))), \
        &className::ms_classInfo))

// The 'this' pointer is always true, so use this version
// to cast the this pointer and avoid compiler warnings.
#define wxDynamicCastThis(className) \
     (IsKindOf(&className::ms_classInfo) ? (className *)(this) : (className *)0)

// FIXME-VC6: dummy argument needed because VC6 doesn't support explicitly
//            choosing the template function to call
template <class T>
inline T *wxCheckCast(const void *ptr, T * = NULL)
{
    return const_cast<T *>(static_cast<const T *>(ptr));
}

#define wxStaticCast(obj, className) wxCheckCast((obj), (className *)NULL)

// ----------------------------------------------------------------------------
// set up memory debugging macros
// ----------------------------------------------------------------------------

/*
    Which new/delete operator variants do we want?

    _WX_WANT_NEW_SIZET_WXCHAR_INT             = void *operator new (size_t size, wxChar *fileName = 0, int lineNum = 0)
    _WX_WANT_DELETE_VOID                      = void operator delete (void * buf)
    _WX_WANT_DELETE_VOID_CONSTCHAR_SIZET      = void operator delete (void *buf, const char *_fname, size_t _line)
    _WX_WANT_DELETE_VOID_WXCHAR_INT           = void operator delete(void *buf, wxChar*, int)
    _WX_WANT_ARRAY_NEW_SIZET_WXCHAR_INT       = void *operator new[] (size_t size, wxChar *fileName , int lineNum = 0)
    _WX_WANT_ARRAY_DELETE_VOID                = void operator delete[] (void *buf)
    _WX_WANT_ARRAY_DELETE_VOID_WXCHAR_INT     = void operator delete[] (void* buf, wxChar*, int )
*/

// ----------------------------------------------------------------------------
// Compatibility macro aliases DECLARE group
// ----------------------------------------------------------------------------
// deprecated variants _not_ requiring a semicolon after them and without wx prefix.
// (note that also some wx-prefixed macro do _not_ require a semicolon because
//  it's not always possible to force the compire to require it)

#define DECLARE_CLASS_INFO_ITERATORS()                              wxDECLARE_CLASS_INFO_ITERATORS();
#define DECLARE_ABSTRACT_CLASS(n)                                   wxDECLARE_ABSTRACT_CLASS(n);
#define DECLARE_DYNAMIC_CLASS_NO_ASSIGN(n)                          wxDECLARE_DYNAMIC_CLASS_NO_ASSIGN(n);
#define DECLARE_DYNAMIC_CLASS_NO_COPY(n)                            wxDECLARE_DYNAMIC_CLASS_NO_COPY(n);
#define DECLARE_DYNAMIC_CLASS(n)                                    wxDECLARE_DYNAMIC_CLASS(n);
#define DECLARE_CLASS(n)                                            wxDECLARE_CLASS(n);

#define DECLARE_PLUGGABLE_CLASS(n)                                  wxDECLARE_PLUGGABLE_CLASS(n);
#define DECLARE_ABSTRACT_PLUGGABLE_CLASS(n)                         wxDECLARE_ABSTRACT_PLUGGABLE_CLASS(n);
#define DECLARE_USER_EXPORTED_PLUGGABLE_CLASS(n,u)                  wxDECLARE_USER_EXPORTED_PLUGGABLE_CLASS(n,u);
#define DECLARE_USER_EXPORTED_ABSTRACT_PLUGGABLE_CLASS(n,u)         wxDECLARE_USER_EXPORTED_ABSTRACT_PLUGGABLE_CLASS(n,u);

// ----------------------------------------------------------------------------
// wxRefCounter: ref counted data "manager"
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_BASE wxRefCounter
{
public:
    wxRefCounter() { m_count = 1; }

    void IncRef() { m_count++; }
    void DecRef();

protected:
    // this object should never be destroyed directly but only as a
    // result of a DecRef() call:
    virtual ~wxRefCounter() { }

private:
    // our refcount:
    int m_count;

    // It doesn't make sense to copy the reference counted objects, a new ref
    // counter should be created for a new object instead and compilation
    // errors in the code using wxRefCounter due to the lack of copy ctor often
    // indicate a problem, e.g. a forgotten copy ctor implementation somewhere.
    wxDECLARE_NO_COPY_CLASS(wxRefCounter);
};

// ----------------------------------------------------------------------------
// wxObjectRefData: ref counted data meant to be stored in wxObject
// ----------------------------------------------------------------------------

typedef wxRefCounter wxObjectRefData;

// ----------------------------------------------------------------------------
// wxObject: the root class of wxWidgets object hierarchy
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_BASE wxObject
{
    wxDECLARE_ABSTRACT_CLASS(wxObject);

public:
    wxObject() { m_refData = NULL; }
    virtual ~wxObject() { UnRef(); }

    wxObject(const wxObject& other)
    {
         m_refData = other.m_refData;
         if (m_refData)
             m_refData->IncRef();
    }

    wxObject& operator=(const wxObject& other)
    {
        if ( this != &other )
        {
            Ref(other);
        }
        return *this;
    }

    bool IsKindOf(const wxClassInfo *info) const;


    // Turn on the correct set of new and delete operators

#ifdef _WX_WANT_NEW_SIZET_WXCHAR_INT
    void *operator new ( size_t size, const wxChar *fileName = NULL, int lineNum = 0 );
#endif

#ifdef _WX_WANT_DELETE_VOID
    void operator delete ( void * buf );
#endif

#ifdef _WX_WANT_DELETE_VOID_CONSTCHAR_SIZET
    void operator delete ( void *buf, const char *_fname, size_t _line );
#endif

#ifdef _WX_WANT_DELETE_VOID_WXCHAR_INT
    void operator delete ( void *buf, const wxChar*, int );
#endif

#ifdef _WX_WANT_ARRAY_NEW_SIZET_WXCHAR_INT
    void *operator new[] ( size_t size, const wxChar *fileName = NULL, int lineNum = 0 );
#endif

#ifdef _WX_WANT_ARRAY_DELETE_VOID
    void operator delete[] ( void *buf );
#endif

#ifdef _WX_WANT_ARRAY_DELETE_VOID_WXCHAR_INT
    void operator delete[] (void* buf, const wxChar*, int );
#endif

    // ref counted data handling methods

    // make a 'clone' of the object
    void Ref(const wxObject& clone);

    // destroy a reference
    void UnRef();

    // check if this object references the same data as the other one
    bool IsSameAs(const wxObject& o) const { return m_refData == o.m_refData; }

protected:
    // both methods must be implemented if AllocExclusive() is used, not pure
    // virtual only because of the backwards compatibility reasons

    wxObjectRefData *m_refData;
};

inline wxObject *wxCheckDynamicCast(wxObject *obj, wxClassInfo *classInfo)
{
    return obj && obj->GetClassInfo()->IsKindOf(classInfo) ? obj : NULL;
}

// ----------------------------------------------------------------------------
// Compatibility macro aliases IMPLEMENT group
// ----------------------------------------------------------------------------

// deprecated variants _not_ requiring a semicolon after them and without wx prefix.
// (note that also some wx-prefixed macro do _not_ require a semicolon because
//  it's not always possible to force the compire to require it)

#define IMPLEMENT_DYNAMIC_CLASS(n,b)                                wxIMPLEMENT_DYNAMIC_CLASS(n,b)
#define IMPLEMENT_DYNAMIC_CLASS2(n,b1,b2)                           wxIMPLEMENT_DYNAMIC_CLASS2(n,b1,b2)
#define IMPLEMENT_ABSTRACT_CLASS(n,b)                               wxIMPLEMENT_ABSTRACT_CLASS(n,b)
#define IMPLEMENT_ABSTRACT_CLASS2(n,b1,b2)                          wxIMPLEMENT_ABSTRACT_CLASS2(n,b1,b2)
#define IMPLEMENT_CLASS(n,b)                                        wxIMPLEMENT_CLASS(n,b)
#define IMPLEMENT_CLASS2(n,b1,b2)                                   wxIMPLEMENT_CLASS2(n,b1,b2)

#define IMPLEMENT_PLUGGABLE_CLASS(n,b)                              wxIMPLEMENT_PLUGGABLE_CLASS(n,b)
#define IMPLEMENT_PLUGGABLE_CLASS2(n,b,b2)                          wxIMPLEMENT_PLUGGABLE_CLASS2(n,b,b2)
#define IMPLEMENT_ABSTRACT_PLUGGABLE_CLASS(n,b)                     wxIMPLEMENT_ABSTRACT_PLUGGABLE_CLASS(n,b)
#define IMPLEMENT_ABSTRACT_PLUGGABLE_CLASS2(n,b,b2)                 wxIMPLEMENT_ABSTRACT_PLUGGABLE_CLASS2(n,b,b2)
#define IMPLEMENT_USER_EXPORTED_PLUGGABLE_CLASS(n,b)                wxIMPLEMENT_USER_EXPORTED_PLUGGABLE_CLASS(n,b)
#define IMPLEMENT_USER_EXPORTED_PLUGGABLE_CLASS2(n,b,b2)            wxIMPLEMENT_USER_EXPORTED_PLUGGABLE_CLASS2(n,b,b2)
#define IMPLEMENT_USER_EXPORTED_ABSTRACT_PLUGGABLE_CLASS(n,b)       wxIMPLEMENT_USER_EXPORTED_ABSTRACT_PLUGGABLE_CLASS(n,b)
#define IMPLEMENT_USER_EXPORTED_ABSTRACT_PLUGGABLE_CLASS2(n,b,b2)   wxIMPLEMENT_USER_EXPORTED_ABSTRACT_PLUGGABLE_CLASS2(n,b,b2)

#define CLASSINFO(n)                                wxCLASSINFO(n)

#endif // _WX_OBJECTH__
