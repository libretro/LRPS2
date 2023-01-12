/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2010  PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "StringHelpers.h"
#include <memory>

namespace Exception
{
// --------------------------------------------------------------------------------------
//  BaseException
// --------------------------------------------------------------------------------------
// std::exception sucks, and isn't entirely cross-platform reliable in its implementation,
// so I made a replacement.  The internal messages are non-const, which means that a
// catch clause can optionally modify them and then re-throw to a top-level handler.
//
// Note, this class is "abstract" which means you shouldn't use it directly like, ever.
//
// Because exceptions are the (only!) really useful example of multiple inheritance,
// this class has only a trivial constructor, and must be manually initialized using
// InitBaseEx() or by individual member assignments.  This is because C++ multiple inheritence
// is, by design, a lot of fail, especially when class initializers are mixed in.
class BaseException
{
protected:
    wxString m_message_diag; // (untranslated) a "detailed" message of what disastrous thing has occurred!
    wxString m_message_user; // (translated) a "detailed" message of what disastrous thing has occurred!

public:
    virtual ~BaseException() = default;

    wxString &DiagMsg() { return m_message_diag; }
    wxString &UserMsg() { return m_message_user; }

    BaseException &SetDiagMsg(const wxString &msg_diag);
    BaseException &SetUserMsg(const wxString &msg_user);
};

// --------------------------------------------------------------------------------------
//  Ps2Generic Exception
// --------------------------------------------------------------------------------------
// This class is used as a base exception for things tossed by PS2 cpus (EE, IOP, etc).
//
// Implementation note: does not derive from BaseException, so that we can use different
// catch block hierarchies to handle them (if needed).
//
// Translation Note: Currently these exceptions are never translated.  English/diagnostic
// format only. :)
//
class Ps2Generic
{
protected:
public:
    virtual ~Ps2Generic() = default;
};

// Some helper macros for defining the standard constructors of internationalized constructors
// Parameters:
//  classname - Yeah, the name of this class being defined. :)
//
//  defmsg - default message (in english), which will be used for both english and i18n messages.
//     The text string will be passed through the translator, so if it's int he gettext database
//     it will be optionally translated.
//
// (update: web searches indicate it's MSVC specific -- happens in 2008, not sure about 2010).
//
#define DEFINE_EXCEPTION_COPYTORS(classname, parent) \
private:                                             \
    typedef parent _parent;                          \
                                                     \
public:                                              \
    virtual ~classname() = default;                  

#define DEFINE_EXCEPTION_MESSAGES(classname)        \
public:                                             \
    classname &SetDiagMsg(const wxString &msg_diag) \
    {                                               \
        m_message_diag = msg_diag;                  \
        return *this;                               \
    }                                               \
    classname &SetUserMsg(const wxString &msg_user) \
    {                                               \
        m_message_user = msg_user;                  \
        return *this;                               \
    }

#define DEFINE_RUNTIME_EXCEPTION(classname, parent, message) \
    DEFINE_EXCEPTION_COPYTORS(classname, parent)             \
    classname() { SetDiagMsg(message); }                     \
    DEFINE_EXCEPTION_MESSAGES(classname)


// ---------------------------------------------------------------------------------------
//  RuntimeError - Generalized Exceptions with Recoverable Traits!
// ---------------------------------------------------------------------------------------

class RuntimeError : public BaseException
{
    DEFINE_EXCEPTION_COPYTORS(RuntimeError, BaseException)
    DEFINE_EXCEPTION_MESSAGES(RuntimeError)

public:
    RuntimeError() { }
    RuntimeError(const std::runtime_error &ex, const wxString &prefix = wxEmptyString);
    RuntimeError(const std::exception &ex, const wxString &prefix = wxEmptyString);
};

class ParseError : public RuntimeError
{
    DEFINE_RUNTIME_EXCEPTION(ParseError, RuntimeError, "Parse error");
};

// ---------------------------------------------------------------------------------------
// Streaming (file) Exceptions:
//   Stream / BadStream / CannotCreateStream / FileNotFound / AccessDenied / EndOfStream
// ---------------------------------------------------------------------------------------

#define DEFINE_STREAM_EXCEPTION(classname, parent)             \
    DEFINE_RUNTIME_EXCEPTION(classname, parent, wxEmptyString) \
    classname(const wxString &filename)                        \
    {                                                          \
        StreamName = filename;                                 \
    }

// A generic base error class for bad streams -- corrupted data, sudden closures, loss of
// connection, or anything else that would indicate a failure to open a stream or read the
// data after the stream was successfully opened.
//
class BadStream : public RuntimeError
{
    DEFINE_STREAM_EXCEPTION(BadStream, RuntimeError)

public:
    wxString StreamName; // name of the stream (if applicable)
};

// A generic exception for odd-ball stream creation errors.
//
class CannotCreateStream : public BadStream
{
    DEFINE_STREAM_EXCEPTION(CannotCreateStream, BadStream)
};

// Exception thrown when an attempt to open a non-existent file is made.
// (this exception can also mean file permissions are invalid)
//
class FileNotFound : public CannotCreateStream
{
public:
    DEFINE_STREAM_EXCEPTION(FileNotFound, CannotCreateStream)
};

class AccessDenied : public CannotCreateStream
{
public:
    DEFINE_STREAM_EXCEPTION(AccessDenied, CannotCreateStream)
};

// EndOfStream can be used either as an error, or used just as a shortcut for manual
// feof checks.
//
class EndOfStream : public BadStream
{
public:
    DEFINE_STREAM_EXCEPTION(EndOfStream, BadStream)
};
}

using Exception::BaseException;
