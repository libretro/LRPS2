/////////////////////////////////////////////////////////////////////////////
// Name:        wx/unix/app.h
// Purpose:     wxAppConsole implementation for Unix
// Author:      Lukasz Michalski
// Created:     28/01/2005
// Copyright:   (c) Lukasz Michalski
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

//Ensure that sigset_t is being defined
#include <signal.h>
#include "wx/hashmap.h"

class wxFDIODispatcher;
class wxFDIOHandler;
class wxWakeUpPipe;

// wxApp subclass implementing event processing for console applications
class WXDLLIMPEXP_BASE wxAppConsole : public wxAppConsoleBase
{
public:
    wxAppConsole();
    virtual ~wxAppConsole();

    // override base class initialization
    virtual bool Initialize(int& argc, wxChar** argv);


    // Unix-specific: Unix signal handling
    // -----------------------------------

    // type of the function which can be registered as signal handler: notice
    // that it isn't really a signal handler, i.e. it's not subject to the
    // usual signal handlers constraints, because it is called later from
    // CheckSignal() and not when the signal really occurs
    typedef void (*SignalHandler)(int);

    // Check if any Unix signals arrived since the last call and execute
    // handlers for them
    void CheckSignal();

private:
    // signals for which HandleSignal() had been called (reset from
    // CheckSignal())
    sigset_t m_signalsCaught;

    // the signal handlers
    WX_DECLARE_HASH_MAP(int, SignalHandler, wxIntegerHash, wxIntegerEqual, SignalHandlerHash);
    SignalHandlerHash m_signalHandlerHash;

    // pipe used for wake up signal handling: if a signal arrives while we're
    // blocking for input, writing to this pipe triggers a call to our CheckSignal()
    wxWakeUpPipe *m_signalWakeUpPipe;
};
