/////////////////////////////////////////////////////////////////////////////
// Name:        wx/gdicmn.h
// Purpose:     Common GDI classes, types and declarations
// Author:      Julian Smart
// Modified by:
// Created:     01/02/97
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GDICMNH__
#define _WX_GDICMNH__

// ---------------------------------------------------------------------------
// headers
// ---------------------------------------------------------------------------

#include "wx/defs.h"
#include "wx/list.h"
#include "wx/string.h"
#include "wx/fontenc.h"
#include "wx/hashmap.h"
#include "wx/math.h"

// ---------------------------------------------------------------------------
// forward declarations
// ---------------------------------------------------------------------------

class WXDLLIMPEXP_FWD_CORE wxFont;
class WXDLLIMPEXP_FWD_BASE wxString;
class WXDLLIMPEXP_FWD_CORE wxPoint;

// ---------------------------------------------------------------------------
// wxSize
// ---------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxSize
{
public:
    // members are public for compatibility, don't use them directly.
    int x, y;

    // constructors
    wxSize() : x(0), y(0) { }
    wxSize(int xx, int yy) : x(xx), y(yy) { }

    // no copy ctor or assignment operator - the defaults are ok

    wxSize& operator+=(const wxSize& sz) { x += sz.x; y += sz.y; return *this; }
    wxSize& operator-=(const wxSize& sz) { x -= sz.x; y -= sz.y; return *this; }
    wxSize& operator/=(int i) { x /= i; y /= i; return *this; }
    wxSize& operator*=(int i) { x *= i; y *= i; return *this; }
    wxSize& operator/=(unsigned int i) { x /= i; y /= i; return *this; }
    wxSize& operator*=(unsigned int i) { x *= i; y *= i; return *this; }
    wxSize& operator/=(long i) { x /= i; y /= i; return *this; }
    wxSize& operator*=(long i) { x *= i; y *= i; return *this; }
    wxSize& operator/=(unsigned long i) { x /= i; y /= i; return *this; }
    wxSize& operator*=(unsigned long i) { x *= i; y *= i; return *this; }
    wxSize& operator/=(double i) { x = int(x/i); y = int(y/i); return *this; }
    wxSize& operator*=(double i) { x = int(x*i); y = int(y*i); return *this; }

    void IncTo(const wxSize& sz)
        { if ( sz.x > x ) x = sz.x; if ( sz.y > y ) y = sz.y; }
    void DecTo(const wxSize& sz)
        { if ( sz.x < x ) x = sz.x; if ( sz.y < y ) y = sz.y; }
    void DecToIfSpecified(const wxSize& sz)
    {
        if ( sz.x != wxDefaultCoord && sz.x < x )
            x = sz.x;
        if ( sz.y != wxDefaultCoord && sz.y < y )
            y = sz.y;
    }

    wxSize& Scale(float xscale, float yscale)
        { x = (int)(x*xscale); y = (int)(y*yscale); return *this; }

    // accessors
    void Set(int xx, int yy) { x = xx; y = yy; }
    void SetWidth(int w) { x = w; }
    void SetHeight(int h) { y = h; }

    int GetWidth() const { return x; }
    int GetHeight() const { return y; }

    bool IsFullySpecified() const { return x != wxDefaultCoord && y != wxDefaultCoord; }

    // combine this size with the other one replacing the default (i.e. equal
    // to wxDefaultCoord) components of this object with those of the other
    void SetDefaults(const wxSize& size)
    {
        if ( x == wxDefaultCoord )
            x = size.x;
        if ( y == wxDefaultCoord )
            y = size.y;
    }

    // compatibility
    int GetX() const { return x; }
    int GetY() const { return y; }
};

inline bool operator==(const wxSize& s1, const wxSize& s2)
{
    return s1.x == s2.x && s1.y == s2.y;
}

inline bool operator!=(const wxSize& s1, const wxSize& s2)
{
    return s1.x != s2.x || s1.y != s2.y;
}

inline wxSize operator+(const wxSize& s1, const wxSize& s2)
{
    return wxSize(s1.x + s2.x, s1.y + s2.y);
}

inline wxSize operator-(const wxSize& s1, const wxSize& s2)
{
    return wxSize(s1.x - s2.x, s1.y - s2.y);
}

inline wxSize operator/(const wxSize& s, int i)
{
    return wxSize(s.x / i, s.y / i);
}

inline wxSize operator*(const wxSize& s, int i)
{
    return wxSize(s.x * i, s.y * i);
}

inline wxSize operator*(int i, const wxSize& s)
{
    return wxSize(s.x * i, s.y * i);
}

inline wxSize operator/(const wxSize& s, unsigned int i)
{
    return wxSize(s.x / i, s.y / i);
}

inline wxSize operator*(const wxSize& s, unsigned int i)
{
    return wxSize(s.x * i, s.y * i);
}

inline wxSize operator*(unsigned int i, const wxSize& s)
{
    return wxSize(s.x * i, s.y * i);
}

inline wxSize operator/(const wxSize& s, long i)
{
    return wxSize(s.x / i, s.y / i);
}

inline wxSize operator*(const wxSize& s, long i)
{
    return wxSize(int(s.x * i), int(s.y * i));
}

inline wxSize operator*(long i, const wxSize& s)
{
    return wxSize(int(s.x * i), int(s.y * i));
}

inline wxSize operator/(const wxSize& s, unsigned long i)
{
    return wxSize(int(s.x / i), int(s.y / i));
}

inline wxSize operator*(const wxSize& s, unsigned long i)
{
    return wxSize(int(s.x * i), int(s.y * i));
}

inline wxSize operator*(unsigned long i, const wxSize& s)
{
    return wxSize(int(s.x * i), int(s.y * i));
}

inline wxSize operator*(const wxSize& s, double i)
{
    return wxSize(int(s.x * i), int(s.y * i));
}

inline wxSize operator*(double i, const wxSize& s)
{
    return wxSize(int(s.x * i), int(s.y * i));
}



// ---------------------------------------------------------------------------
// Point classes: with real or integer coordinates
// ---------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxRealPoint
{
public:
    double x;
    double y;

    wxRealPoint() : x(0.0), y(0.0) { }
    wxRealPoint(double xx, double yy) : x(xx), y(yy) { }
    wxRealPoint(const wxPoint& pt);

    // no copy ctor or assignment operator - the defaults are ok

    //assignment operators
    wxRealPoint& operator+=(const wxRealPoint& p) { x += p.x; y += p.y; return *this; }
    wxRealPoint& operator-=(const wxRealPoint& p) { x -= p.x; y -= p.y; return *this; }

    wxRealPoint& operator+=(const wxSize& s) { x += s.GetWidth(); y += s.GetHeight(); return *this; }
    wxRealPoint& operator-=(const wxSize& s) { x -= s.GetWidth(); y -= s.GetHeight(); return *this; }
};


inline bool operator==(const wxRealPoint& p1, const wxRealPoint& p2)
{
    return wxIsSameDouble(p1.x, p2.x) && wxIsSameDouble(p1.y, p2.y);
}

inline bool operator!=(const wxRealPoint& p1, const wxRealPoint& p2)
{
    return !(p1 == p2);
}

inline wxRealPoint operator+(const wxRealPoint& p1, const wxRealPoint& p2)
{
    return wxRealPoint(p1.x + p2.x, p1.y + p2.y);
}


inline wxRealPoint operator-(const wxRealPoint& p1, const wxRealPoint& p2)
{
    return wxRealPoint(p1.x - p2.x, p1.y - p2.y);
}


inline wxRealPoint operator/(const wxRealPoint& s, int i)
{
    return wxRealPoint(s.x / i, s.y / i);
}

inline wxRealPoint operator*(const wxRealPoint& s, int i)
{
    return wxRealPoint(s.x * i, s.y * i);
}

inline wxRealPoint operator*(int i, const wxRealPoint& s)
{
    return wxRealPoint(s.x * i, s.y * i);
}

inline wxRealPoint operator/(const wxRealPoint& s, unsigned int i)
{
    return wxRealPoint(s.x / i, s.y / i);
}

inline wxRealPoint operator*(const wxRealPoint& s, unsigned int i)
{
    return wxRealPoint(s.x * i, s.y * i);
}

inline wxRealPoint operator*(unsigned int i, const wxRealPoint& s)
{
    return wxRealPoint(s.x * i, s.y * i);
}

inline wxRealPoint operator/(const wxRealPoint& s, long i)
{
    return wxRealPoint(s.x / i, s.y / i);
}

inline wxRealPoint operator*(const wxRealPoint& s, long i)
{
    return wxRealPoint(s.x * i, s.y * i);
}

inline wxRealPoint operator*(long i, const wxRealPoint& s)
{
    return wxRealPoint(s.x * i, s.y * i);
}

inline wxRealPoint operator/(const wxRealPoint& s, unsigned long i)
{
    return wxRealPoint(s.x / i, s.y / i);
}

inline wxRealPoint operator*(const wxRealPoint& s, unsigned long i)
{
    return wxRealPoint(s.x * i, s.y * i);
}

inline wxRealPoint operator*(unsigned long i, const wxRealPoint& s)
{
    return wxRealPoint(s.x * i, s.y * i);
}

inline wxRealPoint operator*(const wxRealPoint& s, double i)
{
    return wxRealPoint(int(s.x * i), int(s.y * i));
}

inline wxRealPoint operator*(double i, const wxRealPoint& s)
{
    return wxRealPoint(int(s.x * i), int(s.y * i));
}


// ----------------------------------------------------------------------------
// wxPoint: 2D point with integer coordinates
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxPoint
{
public:
    int x, y;

    wxPoint() : x(0), y(0) { }
    wxPoint(int xx, int yy) : x(xx), y(yy) { }
    wxPoint(const wxRealPoint& pt) : x(int(pt.x)), y(int(pt.y)) { }

    // no copy ctor or assignment operator - the defaults are ok

    //assignment operators
    wxPoint& operator+=(const wxPoint& p) { x += p.x; y += p.y; return *this; }
    wxPoint& operator-=(const wxPoint& p) { x -= p.x; y -= p.y; return *this; }

    wxPoint& operator+=(const wxSize& s) { x += s.GetWidth(); y += s.GetHeight(); return *this; }
    wxPoint& operator-=(const wxSize& s) { x -= s.GetWidth(); y -= s.GetHeight(); return *this; }

    // check if both components are set/initialized
    bool IsFullySpecified() const { return x != wxDefaultCoord && y != wxDefaultCoord; }

    // fill in the unset components with the values from the other point
    void SetDefaults(const wxPoint& pt)
    {
        if ( x == wxDefaultCoord )
            x = pt.x;
        if ( y == wxDefaultCoord )
            y = pt.y;
    }
};


// comparison
inline bool operator==(const wxPoint& p1, const wxPoint& p2)
{
    return p1.x == p2.x && p1.y == p2.y;
}

inline bool operator!=(const wxPoint& p1, const wxPoint& p2)
{
    return !(p1 == p2);
}


// arithmetic operations (component wise)
inline wxPoint operator+(const wxPoint& p1, const wxPoint& p2)
{
    return wxPoint(p1.x + p2.x, p1.y + p2.y);
}

inline wxPoint operator-(const wxPoint& p1, const wxPoint& p2)
{
    return wxPoint(p1.x - p2.x, p1.y - p2.y);
}

inline wxPoint operator+(const wxPoint& p, const wxSize& s)
{
    return wxPoint(p.x + s.x, p.y + s.y);
}

inline wxPoint operator-(const wxPoint& p, const wxSize& s)
{
    return wxPoint(p.x - s.x, p.y - s.y);
}

inline wxPoint operator+(const wxSize& s, const wxPoint& p)
{
    return wxPoint(p.x + s.x, p.y + s.y);
}

inline wxPoint operator-(const wxSize& s, const wxPoint& p)
{
    return wxPoint(s.x - p.x, s.y - p.y);
}

inline wxPoint operator-(const wxPoint& p)
{
    return wxPoint(-p.x, -p.y);
}

inline wxPoint operator/(const wxPoint& s, int i)
{
    return wxPoint(s.x / i, s.y / i);
}

inline wxPoint operator*(const wxPoint& s, int i)
{
    return wxPoint(s.x * i, s.y * i);
}

inline wxPoint operator*(int i, const wxPoint& s)
{
    return wxPoint(s.x * i, s.y * i);
}

inline wxPoint operator/(const wxPoint& s, unsigned int i)
{
    return wxPoint(s.x / i, s.y / i);
}

inline wxPoint operator*(const wxPoint& s, unsigned int i)
{
    return wxPoint(s.x * i, s.y * i);
}

inline wxPoint operator*(unsigned int i, const wxPoint& s)
{
    return wxPoint(s.x * i, s.y * i);
}

inline wxPoint operator/(const wxPoint& s, long i)
{
    return wxPoint(s.x / i, s.y / i);
}

inline wxPoint operator*(const wxPoint& s, long i)
{
    return wxPoint(int(s.x * i), int(s.y * i));
}

inline wxPoint operator*(long i, const wxPoint& s)
{
    return wxPoint(int(s.x * i), int(s.y * i));
}

inline wxPoint operator/(const wxPoint& s, unsigned long i)
{
    return wxPoint(s.x / i, s.y / i);
}

inline wxPoint operator*(const wxPoint& s, unsigned long i)
{
    return wxPoint(int(s.x * i), int(s.y * i));
}

inline wxPoint operator*(unsigned long i, const wxPoint& s)
{
    return wxPoint(int(s.x * i), int(s.y * i));
}

inline wxPoint operator*(const wxPoint& s, double i)
{
    return wxPoint(int(s.x * i), int(s.y * i));
}

inline wxPoint operator*(double i, const wxPoint& s)
{
    return wxPoint(int(s.x * i), int(s.y * i));
}

WX_DECLARE_LIST_WITH_DECL(wxPoint, wxPointList, class WXDLLIMPEXP_CORE);

// ---------------------------------------------------------------------------
// wxRect
// ---------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxRect
{
public:
    wxRect()
        : x(0), y(0), width(0), height(0)
        { }
    wxRect(int xx, int yy, int ww, int hh)
        : x(xx), y(yy), width(ww), height(hh)
        { }
    wxRect(const wxPoint& topLeft, const wxPoint& bottomRight);
    wxRect(const wxPoint& pt, const wxSize& size)
        : x(pt.x), y(pt.y), width(size.x), height(size.y)
        { }
    wxRect(const wxSize& size)
        : x(0), y(0), width(size.x), height(size.y)
        { }

    // default copy ctor and assignment operators ok

    int GetX() const { return x; }
    void SetX(int xx) { x = xx; }

    int GetY() const { return y; }
    void SetY(int yy) { y = yy; }

    int GetWidth() const { return width; }
    void SetWidth(int w) { width = w; }

    int GetHeight() const { return height; }
    void SetHeight(int h) { height = h; }

    wxPoint GetPosition() const { return wxPoint(x, y); }
    void SetPosition( const wxPoint &p ) { x = p.x; y = p.y; }

    wxSize GetSize() const { return wxSize(width, height); }
    void SetSize( const wxSize &s ) { width = s.GetWidth(); height = s.GetHeight(); }

    bool IsEmpty() const { return (width <= 0) || (height <= 0); }

    int GetLeft()   const { return x; }
    int GetTop()    const { return y; }
    int GetBottom() const { return y + height - 1; }
    int GetRight()  const { return x + width - 1; }

    void SetLeft(int left) { x = left; }
    void SetRight(int right) { width = right - x + 1; }
    void SetTop(int top) { y = top; }
    void SetBottom(int bottom) { height = bottom - y + 1; }

    wxPoint GetTopLeft() const { return GetPosition(); }
    wxPoint GetLeftTop() const { return GetTopLeft(); }
    void SetTopLeft(const wxPoint &p) { SetPosition(p); }
    void SetLeftTop(const wxPoint &p) { SetTopLeft(p); }

    wxPoint GetBottomRight() const { return wxPoint(GetRight(), GetBottom()); }
    wxPoint GetRightBottom() const { return GetBottomRight(); }
    void SetBottomRight(const wxPoint &p) { SetRight(p.x); SetBottom(p.y); }
    void SetRightBottom(const wxPoint &p) { SetBottomRight(p); }

    wxPoint GetTopRight() const { return wxPoint(GetRight(), GetTop()); }
    wxPoint GetRightTop() const { return GetTopRight(); }
    void SetTopRight(const wxPoint &p) { SetRight(p.x); SetTop(p.y); }
    void SetRightTop(const wxPoint &p) { SetTopRight(p); }

    wxPoint GetBottomLeft() const { return wxPoint(GetLeft(), GetBottom()); }
    wxPoint GetLeftBottom() const { return GetBottomLeft(); }
    void SetBottomLeft(const wxPoint &p) { SetLeft(p.x); SetBottom(p.y); }
    void SetLeftBottom(const wxPoint &p) { SetBottomLeft(p); }

    // operations with rect
    wxRect& Inflate(wxCoord dx, wxCoord dy);
    wxRect& Inflate(const wxSize& d) { return Inflate(d.x, d.y); }
    wxRect& Inflate(wxCoord d) { return Inflate(d, d); }
    wxRect Inflate(wxCoord dx, wxCoord dy) const
    {
        wxRect r = *this;
        r.Inflate(dx, dy);
        return r;
    }

    wxRect& Deflate(wxCoord dx, wxCoord dy) { return Inflate(-dx, -dy); }
    wxRect& Deflate(const wxSize& d) { return Inflate(-d.x, -d.y); }
    wxRect& Deflate(wxCoord d) { return Inflate(-d); }
    wxRect Deflate(wxCoord dx, wxCoord dy) const
    {
        wxRect r = *this;
        r.Deflate(dx, dy);
        return r;
    }

    void Offset(wxCoord dx, wxCoord dy) { x += dx; y += dy; }
    void Offset(const wxPoint& pt) { Offset(pt.x, pt.y); }

    wxRect& Intersect(const wxRect& rect);
    wxRect Intersect(const wxRect& rect) const
    {
        wxRect r = *this;
        r.Intersect(rect);
        return r;
    }

    wxRect& Union(const wxRect& rect);
    wxRect Union(const wxRect& rect) const
    {
        wxRect r = *this;
        r.Union(rect);
        return r;
    }

    // return true if the point is (not strcitly) inside the rect
    bool Contains(int x, int y) const;
    bool Contains(const wxPoint& pt) const { return Contains(pt.x, pt.y); }
    // return true if the rectangle 'rect' is (not strictly) inside this rect
    bool Contains(const wxRect& rect) const;

    // like Union() but don't ignore empty rectangles
    wxRect& operator+=(const wxRect& rect);

    // intersections of two rectrangles not testing for empty rectangles
    wxRect& operator*=(const wxRect& rect);

    // centre this rectangle in the given (usually, but not necessarily,
    // larger) one
    wxRect CentreIn(const wxRect& r, int dir = wxBOTH) const
    {
        return wxRect(dir & wxHORIZONTAL ? r.x + (r.width - width)/2 : x,
                      dir & wxVERTICAL ? r.y + (r.height - height)/2 : y,
                      width, height);
    }

    wxRect CenterIn(const wxRect& r, int dir = wxBOTH) const
    {
        return CentreIn(r, dir);
    }

public:
    int x, y, width, height;
};


// compare rectangles
inline bool operator==(const wxRect& r1, const wxRect& r2)
{
    return (r1.x == r2.x) && (r1.y == r2.y) &&
           (r1.width == r2.width) && (r1.height == r2.height);
}

inline bool operator!=(const wxRect& r1, const wxRect& r2)
{
    return !(r1 == r2);
}

// like Union() but don't treat empty rectangles specially
WXDLLIMPEXP_CORE wxRect operator+(const wxRect& r1, const wxRect& r2);

// intersections of two rectangles
WXDLLIMPEXP_CORE wxRect operator*(const wxRect& r1, const wxRect& r2);

#endif
    // _WX_GDICMNH__
