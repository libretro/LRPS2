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

#include <wx/wx.h>

#include "Threading.h"
#include "wxGuiTools.h"
#include "pxEvents.h"
#include "AppTrait.h"

using namespace Threading;

class pxSynchronousCommandEvent;

// --------------------------------------------------------------------------------------
//  wxAppWithHelpers
// --------------------------------------------------------------------------------------
class wxAppWithHelpers : public wxApp
{
    typedef wxApp _parent;

    wxDECLARE_DYNAMIC_CLASS(wxAppWithHelpers);

public:
    wxAppWithHelpers();
    virtual ~wxAppWithHelpers() {}

    wxAppTraits *CreateTraits();

    void CleanUp();

    void DeleteObject(BaseDeletableObject &obj);
    void DeleteObject(BaseDeletableObject *obj)
    {
        if (obj == NULL)
            return;
        DeleteObject(*obj);
    }

    void DeleteThread(Threading::pxThread &obj);
    void DeleteThread(Threading::pxThread *obj)
    {
        if (obj == NULL)
            return;
#ifndef __LIBRETRO__
        DeleteThread(*obj);
#endif
    }

    void PostCommand(void *clientData, int evtType, int intParam = 0, long longParam = 0, const wxString &stringParam = wxEmptyString);
    void PostCommand(int evtType, int intParam = 0, long longParam = 0, const wxString &stringParam = wxEmptyString);
    void PostMethod(FnType_Void *method);
    void ProcessMethod(FnType_Void *method);

    bool Rpc_TryInvoke(FnType_Void *method);
    bool Rpc_TryInvokeAsync(FnType_Void *method);

    sptr ProcessCommand(void *clientData, int evtType, int intParam = 0, long longParam = 0, const wxString &stringParam = wxEmptyString);
    sptr ProcessCommand(int evtType, int intParam = 0, long longParam = 0, const wxString &stringParam = wxEmptyString);

    void ProcessAction(pxActionEvent &evt);
    void PostAction(const pxActionEvent &evt);

    bool OnInit();
    //int  OnExit();

    void PostEvent(const wxEvent &evt);
    bool ProcessEvent(wxEvent &evt);
    bool ProcessEvent(wxEvent *evt);

    bool ProcessEvent(pxActionEvent &evt);
    bool ProcessEvent(pxActionEvent *evt);
};

extern wxString pxGetAppName();
