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

#include "Dependencies.h"

#include "General.h"
#include "PersistentThread.h"
#include "wxAppWithHelpers.h"

wxDEFINE_EVENT(pxEvt_InvokeAction, pxActionEvent);

// --------------------------------------------------------------------------------------
//  SynchronousActionState Implementations
// --------------------------------------------------------------------------------------

int SynchronousActionState::WaitForResult()
{
    m_sema.WaitNoCancel();
    return 0;
}

// --------------------------------------------------------------------------------------
//  pxActionEvent Implementations
// --------------------------------------------------------------------------------------

wxIMPLEMENT_DYNAMIC_CLASS(pxActionEvent, wxEvent);

pxActionEvent::pxActionEvent(SynchronousActionState *sema, int msgtype)
    : wxEvent(0, msgtype)
{
    m_state = sema;
}

pxActionEvent::pxActionEvent(SynchronousActionState &sema, int msgtype)
    : wxEvent(0, msgtype)
{
    m_state = &sema;
}

pxActionEvent::pxActionEvent(const pxActionEvent &src)
    : wxEvent(src)
{
    m_state = src.m_state;
}

// --------------------------------------------------------------------------------------
//  pxRpcEvent
// --------------------------------------------------------------------------------------
// Unlike pxPingEvent, the Semaphore belonging to this event is typically posted when the
// invoked method is completed.  If the method can be executed in non-blocking fashion then
// it should leave the semaphore postback NULL.
//
class pxRpcEvent : public pxActionEvent
{
    wxDECLARE_DYNAMIC_CLASS_NO_ASSIGN(pxRpcEvent);

    typedef pxActionEvent _parent;

protected:
    void (*m_Method)();

public:
    virtual ~pxRpcEvent() = default;
    virtual pxRpcEvent *Clone() const { return new pxRpcEvent(*this); }

    explicit pxRpcEvent(void (*method)() = NULL, SynchronousActionState *sema = NULL)
        : pxActionEvent(sema)
    {
        m_Method = method;
    }

    explicit pxRpcEvent(void (*method)(), SynchronousActionState &sema)
        : pxActionEvent(sema)
    {
        m_Method = method;
    }

    pxRpcEvent(const pxRpcEvent &src)
        : pxActionEvent(src)
    {
        m_Method = src.m_Method;
    }
};

wxIMPLEMENT_DYNAMIC_CLASS(pxRpcEvent, pxActionEvent);

// --------------------------------------------------------------------------------------
//  wxAppWithHelpers Implementation
// --------------------------------------------------------------------------------------
//
// TODO : Ping dispatch and IdleEvent dispatch can be unified into a single dispatch, which
// would mean checking only one list of events per idle event, instead of two.  (ie, ping
// events can be appended to the idle event list, instead of into their own custom list).
//
wxIMPLEMENT_DYNAMIC_CLASS(wxAppWithHelpers, wxApp);

// Invokes the specified void method, or posts the method to the main thread if the calling
// thread is not Main.  Action is blocking.  For non-blocking method execution, use
// AppRpc_TryInvokeAsync.
//
// This function works something like setjmp/longjmp, in that the return value indicates if the
// function actually executed the specified method or not.
//
// Returns:
//   false if the method was not invoked (meaning this IS the main thread!)
//   true if the method was invoked.
//

bool wxAppWithHelpers::Rpc_TryInvoke(FnType_Void *method)
{
   if (wxThread::IsMain())
      return false;

   SynchronousActionState sync;
   PostEvent(pxRpcEvent(method, sync));
   sync.WaitForResult();

   return true;
}

void wxAppWithHelpers::PostEvent(const wxEvent &evt)
{
    // Const Cast is OK!
    // Truth is, AddPendingEvent should be a const-qualified parameter, as
    // it makes an immediate clone copy of the event -- but wxWidgets
    // fails again in structured C/C++ design design.  So I'm forcing it as such
    // here. -- air

    _parent::AddPendingEvent(const_cast<wxEvent &>(evt));
}

bool wxAppWithHelpers::ProcessEvent(wxEvent &evt)
{
    // Note: We can't do an automatic blocking post of the message here, because wxWidgets
    // isn't really designed for it (some events return data to the caller via the event
    // struct, and posting the event would require a temporary clone, where changes would
    // be lost).
    return _parent::ProcessEvent(evt);
}

bool wxAppWithHelpers::ProcessEvent(wxEvent *evt)
{
    std::unique_ptr<wxEvent> deleteMe(evt);
    return _parent::ProcessEvent(*deleteMe);
}

bool wxAppWithHelpers::ProcessEvent(pxActionEvent &evt)
{
   if (wxThread::IsMain())
      return _parent::ProcessEvent(evt);

   SynchronousActionState sync;
   evt.SetSyncState(sync);
   AddPendingEvent(evt);
   sync.WaitForResult();
   return true;
}

bool wxAppWithHelpers::ProcessEvent(pxActionEvent *evt)
{
   if (wxThread::IsMain())
   {
      std::unique_ptr<wxEvent> deleteMe(evt);
      return _parent::ProcessEvent(*deleteMe);
   }

   SynchronousActionState sync;
   evt->SetSyncState(sync);
   AddPendingEvent(*evt);
   sync.WaitForResult();
   return true;
}


void wxAppWithHelpers::CleanUp()
{
    // I'm pretty sure the message pump is dead by now, which means we need to run through
    // idle event list by hand and process the pending Deletion messages (all others can be
    // ignored -- it's only deletions we want handled, and really we *could* ignore those too
    // but I like to be tidy. -- air

    //IdleEventDispatcher( "CleanUp" );
    //DeletionDispatcher();
    _parent::CleanUp();
}

void wxAppWithHelpers::PostAction(const pxActionEvent &evt)
{
    PostEvent(evt);
}

bool wxAppWithHelpers::OnInit()
{
    return _parent::OnInit();
}

// In theory we create a Pcsx2App object which inherit from wxAppWithHelpers,
// so Pcsx2App::CreateTraits must be used instead.
//
// However it doesn't work this way because wxAppWithHelpers constructor will
// be called first. This constructor will build some wx objects (here wxTimer)
// that require a trait. In others word, wxAppWithHelpers::CreateTraits will be
// called instead
wxAppTraits *wxAppWithHelpers::CreateTraits()
{
    return new Pcsx2AppTraits;
}

wxAppWithHelpers::wxAppWithHelpers()
{
#ifdef __WXMSW__
    // This variable assignment ensures that MSVC links in the TLS setup stubs even in
    // full optimization builds.  Without it, DLLs that use TLS won't work because the
    // FS segment register won't have been initialized by the main exe, due to tls_insurance
    // being optimized away >_<  --air

    static thread_local int tls_insurance = 0;
    tls_insurance = 1;
#endif
}
