/////////////////////////////////////////////////////////////////////////////
// Name:        src/unix/appunix.cpp
// Purpose:     wxAppConsole with wxMainLoop implementation
// Author:      Lukasz Michalski
// Created:     28/01/2005
// Copyright:   (c) Lukasz Michalski
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#ifndef WX_PRECOMP
    #include "wx/app.h"
#endif

#include "wx/evtloop.h"
#include "wx/scopedptr.h"
#include "wx/unix/private/wakeuppipe.h"
#include "wx/private/fdiodispatcher.h"
#include "wx/private/fdioeventloopsourcehandler.h"

#include <signal.h>
#include <unistd.h>

#ifndef SA_RESTART
    // don't use for systems which don't define it (at least VMS and QNX)
    #define SA_RESTART 0
#endif

// ----------------------------------------------------------------------------
// Helper class calling CheckSignal() on wake up
// ----------------------------------------------------------------------------

namespace
{

class SignalsWakeUpPipe : public wxWakeUpPipe
{
public:
    // Ctor automatically registers this pipe with the event loop.
    SignalsWakeUpPipe()
    {
        m_source = wxEventLoopBase::AddSourceForFD
                                    (
                                        GetReadFd(),
                                        this,
                                        wxEVENT_SOURCE_INPUT
                                    );
    }

    virtual void OnReadWaiting()
    {
        // The base class wxWakeUpPipe::OnReadWaiting() needs to be called in order
        // to read the data out of the wake up pipe and clear it for next time.
        wxWakeUpPipe::OnReadWaiting();

        if ( wxTheApp )
            wxTheApp->CheckSignal();
    }

    virtual ~SignalsWakeUpPipe()
    {
        delete m_source;
    }

private:
    wxEventLoopSource* m_source;
};

} // anonymous namespace

wxAppConsole::wxAppConsole()
{
    m_signalWakeUpPipe = NULL;
}

wxAppConsole::~wxAppConsole()
{
    delete m_signalWakeUpPipe;
}

// use unusual names for arg[cv] to avoid clashes with wxApp members with the
// same names
bool wxAppConsole::Initialize(int& argc_, wxChar** argv_)
{
    if ( !wxAppConsoleBase::Initialize(argc_, argv_) )
        return false;

    sigemptyset(&m_signalsCaught);

    return true;
}

void wxAppConsole::CheckSignal()
{
    for ( SignalHandlerHash::iterator it = m_signalHandlerHash.begin();
          it != m_signalHandlerHash.end();
          ++it )
    {
        int sig = it->first;
        if ( sigismember(&m_signalsCaught, sig) )
        {
            sigdelset(&m_signalsCaught, sig);
            (it->second)(sig);
        }
    }
}

// the type of the signal handlers we use is "void(*)(int)" while the real
// signal handlers are extern "C" and so have incompatible type and at least
// Sun CC warns about it, so use explicit casts to suppress these warnings as
// they should be harmless
extern "C"
{
    typedef void (*SignalHandler_t)(int);
}
