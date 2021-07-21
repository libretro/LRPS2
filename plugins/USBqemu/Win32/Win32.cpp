#include <stdio.h>
#include <windows.h>
#include <windowsx.h>

#include "../USB.h"
#include "resource.h"

HINSTANCE hInst;

BOOL CALLBACK ConfigureDlgProc(HWND hW, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    
	switch(uMsg) {
		case WM_INITDIALOG:
			LoadConfig();
            if (conf.Log)        CheckDlgButton(hW, IDC_LOGGING, TRUE);
			return TRUE;

		case WM_COMMAND:
			switch(LOWORD(wParam)) {
				case IDCANCEL:
					EndDialog(hW, TRUE);
					return TRUE;
				case IDOK:
					if (IsDlgButtonChecked(hW, IDC_LOGGING))
						 conf.Log = 1;
					else conf.Log = 0;
					SaveConfig();
					EndDialog(hW, FALSE);
					return TRUE;
			}
	}
	return FALSE;
}


BOOL CALLBACK AboutDlgProc(HWND hW, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch(uMsg) {
		case WM_INITDIALOG:
			return TRUE;

		case WM_COMMAND:
			switch(LOWORD(wParam)) {
				case IDOK:
					EndDialog(hW, FALSE);
					return TRUE;
			}
	}
	return FALSE;
}

void CALLBACK USBconfigure() {
    DialogBox(hInst,
              MAKEINTRESOURCE(IDD_CONFIG),
              GetActiveWindow(),  
              (DLGPROC)ConfigureDlgProc); 
}

void CALLBACK USBabout() {
    DialogBox(hInst,
              MAKEINTRESOURCE(IDD_ABOUT),
              GetActiveWindow(),  
              (DLGPROC)AboutDlgProc);
}
#ifndef BUILTIN_USB_PLUGIN
BOOL APIENTRY DllMain(HANDLE hModule,                  // DLL INIT
                      DWORD  dwReason, 
                      LPVOID lpReserved) {
	hInst = (HINSTANCE)hModule;
	return TRUE;                                          // very quick :)
}
#endif
