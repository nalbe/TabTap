// Implementation-specific headers
#include "CustomIncludes/WinApi/MessageBoxNotifier.h"
#include "CustomIncludes/WinApi/Debouncer.h"
#include "CustomIncludes/WinApi/WindowDragger.h"
#include "CustomIncludes/WinApi/MouseTracker.h"
#include "CustomIncludes/WinApi/DoubleClickHelper.h"

// Windows system headers
#include <windows.h>
#include <windowsx.h>  // For GET_X_LPARAM, GET_Y_LPARAM
#include <tchar.h>

#ifdef _DEBUG
#include "CustomIncludes/WinApi/LogManager.h"
#endif



// Custom window message IDs
#define WM_APP_CUSTOM_MESSAGE       (WM_APP + 2)

// Custom command IDs (LOWORD)
#define ID_APP_ANIMATION            (3000 + 1)
#define ID_APP_SYNC_Y_POSITION      (3000 + 2)
#define ID_APP_DIRECTUI_READY       (3000 + 3)
#define ID_APP_DOCKMODE             (3000 + 4)
#define ID_APP_REGULARMODE          (3000 + 5)
#define ID_APP_FADE                 (3000 + 6)



namespace
{
	// Store globals
	HINSTANCE g_hInstance  = NULL;  // DLL instance
	HHOOK g_hHook          = NULL;  // CBT hook handle
	HWND g_hTabTapMainWnd  = NULL;  // TabTapMainClass handle
	HWND g_hDirectUIWnd    = NULL;  // DirectUIHWnd handle
	HWND g_hOSKMainWnd     = NULL;  // OSKMainClass handle

	LPCTSTR g_oskExecPath  = _T("%WINDIR%\\System32\\osk.exe");  // Default path to the on-screen keyboard executable

	WNDPROC g_origOSKMainWndProc  = NULL;  // Original OSKMainClass window procedure
	WNDPROC g_origDirectUIWndProc = NULL;  // Original DirectUIHWND window procedure
}



// Forward declarations
LRESULT CALLBACK CBTProc(INT, WPARAM, LPARAM);



extern "C" __declspec(dllexport)
BOOL UninstallHook()
{
	if (!g_hHook) {
#ifdef _DEBUG
		LogManager::WriteLog(_T("No hook to remove."));
#endif // _DEBUG
		return FALSE;
	}
	if (!UnhookWindowsHookEx(g_hHook)) {
#ifdef _DEBUG
		LogManager::WriteLog(_T("Failed to remove hook: %lu"), GetLastError());
#endif // _DEBUG
		return FALSE;
	}
	g_hHook = NULL;

	return TRUE;
}

extern "C" __declspec(dllexport)
BOOL InstallHook(DWORD threadId)
{
	g_hHook = SetWindowsHookEx(
		WH_CBT,
		CBTProc,
		g_hInstance,
		threadId
	);

	if (!g_hHook) {
#ifdef _DEBUG
		LogManager::WriteLog(_T("Failed to set windows hook: %lu"), GetLastError());
#endif // _DEBUG
		return FALSE;
	}
	return TRUE;
}


BOOL APIENTRY DllMain(
	HINSTANCE hInstance,
	DWORD dwReason,
	LPVOID lpReserved)
{
	switch (dwReason)
	{
	case DLL_PROCESS_ATTACH:
	{
		g_hInstance = hInstance;
		DisableThreadLibraryCalls(hInstance);  // Optional
		break;
	}
	case DLL_PROCESS_DETACH:
	{
		if (g_hHook != NULL) {
			UnhookWindowsHookEx(g_hHook);
			g_hHook = NULL;
		}
		break;
	}
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	default:
		break;
	}
	return TRUE;
}


LRESULT CALLBACK DirectUIWndProc(
	HWND hWnd,
	UINT msg,
	WPARAM wParam,
	LPARAM lParam)
{
	DoubleClickHelper::ProcessMessage(msg, lParam);

	switch (msg)
	{

	case WM_MOUSEWHEEL:
	{
		// Forward event to the parent window
		PostMessage(g_hOSKMainWnd, WM_MOUSEWHEEL, wParam, lParam);
		return 0;
	}

	case WM_RBUTTONDOWN:
	{
		// Forward event to the parent window
		PostMessage(g_hOSKMainWnd, WM_RBUTTONDOWN, wParam, lParam);
		return 0;
	}

	case WM_RBUTTONUP:
	{
		// Forward event to the parent window
		PostMessage(g_hOSKMainWnd, WM_RBUTTONUP, wParam, lParam);
		return 0;
	}

	case WM_RBUTTONDBLCLK:
	{
		// Forward event to the parent window
		PostMessage(g_hOSKMainWnd, WM_RBUTTONDBLCLK, wParam, lParam);
		return 0;
	}

	case WM_MBUTTONDOWN:
	{
		// Forward event to the parent window
		PostMessage(g_hOSKMainWnd, WM_MBUTTONDOWN, wParam, lParam);
		return 0;
	}

	case WM_MBUTTONUP:
	{
		// Forward event to the parent window
		PostMessage(g_hOSKMainWnd, WM_MBUTTONUP, wParam, lParam);
		return 0;
	}

	case WM_MBUTTONDBLCLK:
	{
		// Forward event to the parent window
		PostMessage(g_hOSKMainWnd, WM_MBUTTONDBLCLK, wParam, lParam);
		return 0;
	}

	case WM_MOUSEMOVE:
	{
		// Forward event to the parent window
		PostMessage(g_hOSKMainWnd, WM_MOUSEMOVE, wParam, lParam);
		break;
	}

	default: break;
	}

	return CallWindowProc(g_origDirectUIWndProc, hWnd, msg, wParam, lParam);
}


LRESULT CALLBACK OSKMainWndProc(
	HWND hWnd,
	UINT msg,
	WPARAM wParam,
	LPARAM lParam)
{
	static WindowDragger windowDragger{};
	static MouseTracker mouseTracker{ hWnd };
	static Debouncer mouseWheelDebounce{ 100 };


	switch (msg)
	{
	case WM_MOUSEMOVE:
	{
		mouseTracker.OnMouseMove();

		windowDragger.OnMouseMove(
			[&hWnd](const POINT& pt, nullptr_t) {
				return SetWindowPos(
					hWnd,
					NULL,  // No change in Z-order relative to other windows
					pt.x, pt.y,
					0, 0,  // No change in size
					SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE
				) != 0;
			},
			nullptr
		);

		return 0;
	}

	case WM_MOUSELEAVE:
	{
		mouseTracker.OnMouseLeave();
		return 0;
	}

	case WM_MOUSEWHEEL:
	{
		if (!mouseWheelDebounce.ShouldProcess()) { return 0; }

		int delta = GET_WHEEL_DELTA_WPARAM(wParam);
		bool forward = delta > 0;

		// Get installed keyboard layouts
		int layoutCount = GetKeyboardLayoutList(0, nullptr);
		if (layoutCount <= 1) {
			return 0;
		}  // No need to switch if <=1 layout

		HKL* layouts = new HKL[layoutCount];
		GetKeyboardLayoutList(layoutCount, layouts);

		// Find current layout position
		HWND hForegroundWnd = GetForegroundWindow();
		DWORD threadProcessId = GetWindowThreadProcessId(hForegroundWnd, nullptr);
		HKL current = GetKeyboardLayout(threadProcessId);
		int currentIndex = -1;
		for (int i{}; i < layoutCount; ++i) {
			if (layouts[i] == current) {
				currentIndex = i;
				break;
			}
		}

		if (currentIndex == -1) {
			delete[] layouts;
			return 0;
		}

		// Calculate new index with boundary checks
		int newIndex = forward ? currentIndex + 1 : currentIndex - 1;
		if (newIndex < 0 or newIndex >= layoutCount) {
			delete[] layouts;
			return 0;  // Stop at boundaries
		}

		// System-wide activation
		HKL newLayout = layouts[newIndex];
		ActivateKeyboardLayout(newLayout, KLF_ACTIVATE);

		// Update taskbar and all applications
		PostMessage(HWND_BROADCAST, WM_INPUTLANGCHANGE, 0, (LPARAM)newLayout);
		PostMessage(hForegroundWnd, WM_INPUTLANGCHANGEREQUEST, INPUTLANGCHANGE_SYSCHARSET, (LPARAM)newLayout);

		delete[] layouts;
		return 0;
	}

	case WM_STYLECHANGING:
	{
		if (wParam == GWL_STYLE) {
			// Forced frame for `Dock` mode
			//((STYLESTRUCT*)lParam)->styleNew |= WS_THICKFRAME;
		}
		break;
	}

	case WM_WINDOWPOSCHANGING:
	{
		// Prevent changing the size and position when enabling `Dock` mode
		if (((PWINDOWPOS)lParam)->cx == GetSystemMetrics(SM_CXSCREEN)) {
			((PWINDOWPOS)lParam)->flags = (NULL
				| SWP_NOSIZE
				| SWP_NOMOVE
				| SWP_NOACTIVATE
				| SWP_NOSENDCHANGING
				);
			return 0;
		}
		break;
	}

	case WM_NCMBUTTONDOWN:
	{
		// Repositioning the First App on middle button down of the 'X'
		if (wParam == HTCLOSE) {
			PostMessage(
				g_hTabTapMainWnd,
				WM_APP_CUSTOM_MESSAGE,
				MAKEWPARAM(ID_APP_SYNC_Y_POSITION, 0),
				(LPARAM)hWnd
			);
			return 0;
		}

		if (wParam == HTSYSMENU) {
			// TODO
			return 0;
		}

		// In general, it simulates a middle click in the client zone
		PostMessage(hWnd, WM_MBUTTONDOWN, wParam, lParam); // Drag trigger
		return 0;
	}

	case WM_NCMBUTTONUP:
	{
		// Hiding the OSK on middle button up of the 'X'
		if (wParam == HTCLOSE) {
			ShowWindow(hWnd, SW_HIDE);
			return 0;  // Prevent default behavior
		}
		break;
	}

	case WM_MBUTTONDOWN:
	{
		// Drag begin
		if (!windowDragger.Enable(hWnd)) {
			MessageBoxNotifier{
				{ _T("Drag Error") },
				{ _T("Failed to start drag operation. %s"),
					ErrorMessageConverter(GetLastError()) }
			}.ShowError(hWnd);
			return 1;
		}
		return 0;
	}

	case WM_MBUTTONUP:
	{
		// Drag end
		if (!windowDragger.Disable()) {
			MessageBoxNotifier{
				{ _T("Drag Error") },
				{ _T("Failed to finish drag operation. %s"),
					ErrorMessageConverter(GetLastError()) }
			}.ShowError(hWnd);
			return 1;
		}
		return 0;
	}

	case WM_MBUTTONDBLCLK:
	{
		ShowWindow(hWnd, SW_HIDE);
		return 0;
	}

	case WM_RBUTTONDOWN:
	{
		// Act as middle button
		PostMessage(hWnd, WM_MBUTTONDOWN, wParam, lParam);
		return 0;
	}

	case WM_RBUTTONUP:
	{
		// Act as middle button
		PostMessage(hWnd, WM_MBUTTONUP, wParam, lParam);
		return 0;
	}

	case WM_RBUTTONDBLCLK:
	{
		// Act as middle button
		PostMessage(hWnd, WM_MBUTTONDBLCLK, wParam, lParam);
		return 0;
	}

	case WM_CLOSE:
	{
		// Change the 'X' button’s behavior from 'Close' to 'Hide' and use lParam to indicate a real 'Close' event
		if ((BOOL)lParam == TRUE) { // Abuse lParam for custom behavior
			break;
		}
		ShowWindow(hWnd, SW_HIDE);
		return 0; // Message handled
	}

	case WM_APP_CUSTOM_MESSAGE:
	{
		WORD wCommandId = LOWORD(wParam);

		if (wCommandId == ID_APP_DIRECTUI_READY) {
			if (!g_hDirectUIWnd) { break; }

			// Store the original window procedure.
			g_origDirectUIWndProc = reinterpret_cast<WNDPROC>(
				GetWindowLongPtr(g_hDirectUIWnd, GWLP_WNDPROC)
			);

			// Subclass DirectUIHWND
			SetWindowLongPtr(
				g_hDirectUIWnd,
				GWLP_WNDPROC,
				(LONG_PTR)DirectUIWndProc
			);
			return 0;
		}

		if (wCommandId == ID_APP_DOCKMODE) {
			ShowWindow(hWnd, SW_HIDE);
			LONG_PTR style = (NULL
				//| WS_VISIBLE
				| WS_CLIPSIBLINGS
				| WS_CLIPCHILDREN
				| WS_SYSMENU
				//| WS_MINIMIZEBOX
				);
			SetWindowLongPtr(hWnd, GWL_STYLE, style);

			LONG_PTR exstyle = (NULL
				| WS_EX_NOACTIVATE
				| WS_EX_LAYERED
				//| WS_EX_APPWINDOW
				| WS_EX_WINDOWEDGE
				| WS_EX_TOOLWINDOW
				| WS_EX_TOPMOST
				);
			SetWindowLongPtr(hWnd, GWL_EXSTYLE, exstyle);

			ShowWindow(hWnd, SW_SHOW);
			return 0;
		}

		if (wCommandId == ID_APP_REGULARMODE) {
			ShowWindow(hWnd, SW_HIDE);

			LONG_PTR style = (NULL
				//| WS_VISIBLE
				| WS_CLIPSIBLINGS
				| WS_CLIPCHILDREN
				| WS_BORDER
				| WS_DLGFRAME
				| WS_SYSMENU
				| WS_THICKFRAME
				//| WS_MINIMIZEBOX
				);
			SetWindowLongPtr(hWnd, GWL_STYLE, style);

			LONG_PTR exstyle = (NULL
				| WS_EX_NOACTIVATE
				| WS_EX_LAYERED
				//| WS_EX_APPWINDOW
				| WS_EX_TOPMOST
				);
			SetWindowLongPtr(hWnd, GWL_EXSTYLE, exstyle);

			ShowWindow(hWnd, SW_SHOW);
			return 0;
		}

		if (wCommandId == ID_APP_FADE) {
			static BYTE opaque = 0xff;
			static const BYTE MaxOpaque = 0xff;
			static const BYTE MinOpaque = 0x20;

			bool isIncrease = HIWORD(wParam);

			if (isIncrease and opaque < MaxOpaque) {
				opaque += 0x10;
			}
			else if (!isIncrease and opaque > MinOpaque) {
				opaque -= 0x10;
			}

			SetLayeredWindowAttributes(
				hWnd, 0,
				opaque,
				LWA_ALPHA
			);

			return 0;
		}
		return 1;
	}

	case WM_DESTROY:
	{
		break;
	}

	default: break;
	}

	return CallWindowProc(g_origOSKMainWndProc, hWnd, msg, wParam, lParam);
}


LRESULT CALLBACK CBTProc(
	INT nCode,
	WPARAM wParam,
	LPARAM lParam)
{
	if (nCode < 0) {
		return CallNextHookEx(g_hHook, nCode, wParam, lParam);
	}

	switch (nCode)
	{

	case HCBT_CREATEWND:
	{
		HWND hWnd = (HWND)wParam;
		TCHAR szClassName[MAX_PATH]{};
		// Retrieve the window class name
		if (!GetClassName(hWnd, szClassName, sizeof(szClassName) / sizeof(TCHAR))) {
			break;
		}

		if (wcscmp(szClassName, _T("DirectUIHWND")) == 0) {
			// Store handle as global
			g_hDirectUIWnd = hWnd;

			// Notify that proc can be subclassed now
			PostMessage(
				g_hOSKMainWnd,
				WM_APP_CUSTOM_MESSAGE,
				MAKEWPARAM(ID_APP_DIRECTUI_READY, 0),
				(LPARAM)hWnd
			);


			// Open the named event created by the first app
			HANDLE hEvent = OpenEvent(EVENT_MODIFY_STATE, FALSE, _T("OSKLoadEvent"));
			if (hEvent != NULL) {
				// Signal the event indicating that this app has finished loading
				SetEvent(hEvent);
				CloseHandle(hEvent);
			}
		}
		return 0;
	}

	case HCBT_ACTIVATE:
	{
		HWND hWnd = (HWND)wParam;
		TCHAR szClassName[MAX_PATH]{};
		// Retrieve the window class name
		if (!GetClassName(hWnd, szClassName, sizeof(szClassName) / sizeof(TCHAR))) {
			break;
		}

		if (_tcscmp(szClassName, _T("OSKMainClass")) == 0) {
			static BOOL bProcessed{}; // To process only once
			if (bProcessed) {
				break;
			}
			bProcessed = TRUE;


			// Store the original window procedure
			g_origOSKMainWndProc = (WNDPROC)GetWindowLongPtr(
				hWnd,
				GWLP_WNDPROC
			);
			// Replace original procedure
			SetWindowLongPtr(
				hWnd,
				GWLP_WNDPROC,
				(LONG_PTR)OSKMainWndProc
			);

			// Remove `Minimize` box
			LONG_PTR style = GetWindowLongPtr(hWnd, GWL_STYLE);
			style &= ~WS_MINIMIZEBOX;
			SetWindowLongPtr(hWnd, GWL_STYLE, style);

			// Remove from Taskbar
			LONG_PTR exstyle = GetWindowLongPtr(hWnd, GWL_EXSTYLE);
			exstyle &= ~WS_EX_APPWINDOW;
			SetWindowLongPtr(hWnd, GWL_EXSTYLE, exstyle);

			// Edit `System Menu`
			HMENU hSysMenu = GetSystemMenu(hWnd, FALSE);
			DeleteMenu(hSysMenu, SC_RESTORE, MF_BYCOMMAND);
			DeleteMenu(hSysMenu, SC_MINIMIZE, MF_BYCOMMAND);
			DeleteMenu(hSysMenu, SC_MAXIMIZE, MF_BYCOMMAND);

			// Apply style changes
			SetWindowPos(hWnd, NULL, 0, 0, 0, 0,
				SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);


			// Store the first app handle as global
			g_hTabTapMainWnd = FindWindow(_T("TabTapMainClass"), NULL);

			// Store the main handle as global
			g_hOSKMainWnd = hWnd;

			// Show after changes
			ShowWindowAsync(hWnd, SW_SHOW); // `SW_SHOWNA` causes flickering

			// Increment DLL reference count
			HMODULE hMod;
			if (GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCWSTR)OSKMainWndProc, &hMod)) {
				TCHAR pathBuffer[MAX_PATH];
				GetModuleFileName(hMod, pathBuffer, MAX_PATH);
				LoadLibrary(pathBuffer);
			}

#ifdef _DEBUG
			// Set log path
			LogManager::SetDirectory(_T("C:\\"));  // Picked OSK path by default
#endif // _DEBUG

		}
		return 0;
	}

	//Keeping the hook active and dropping focus here doesn’t work either
	/*
	case HCBT_SETFOCUS:
	{
		return 1;
	}
	*/
	default: break;
	}

	return CallNextHookEx(g_hHook, nCode, wParam, lParam);
}







/*
	//=========================
	// Dock
	//=========================

	0x160A0000  GWL_STYLE
	----------
	0x10000000  WS_VISIBLE
	0x04000000  WS_CLIPSIBLINGS
	0x02000000  WS_CLIPCHILDREN
	0x00080000  WS_SYSMENU
	0x00020000  WS_MINIMIZEBOX


	0x080C0188  GWL_EXSTYLE
	----------
	0x08000000  WS_EX_NOACTIVATE
	0x00080000  WS_EX_LAYERED
	0x00040000  WS_EX_APPWINDOW
	0x00000100  WS_EX_WINDOWEDGE
	0x00000080  WS_EX_TOOLWINDOW
	0x00000008  WS_EX_TOPMOST
*/
/*
	//=========================
	// Regular
	//=========================

	0x16CE0000  GWL_STYLE
	----------
	0x10000000  WS_VISIBLE
	0x04000000  WS_CLIPSIBLINGS
	0x02000000  WS_CLIPCHILDREN
	0x00800000  WS_BORDER
	0x00400000  WS_DLGFRAME
	0x00080000  WS_SYSMENU
	0x00040000  WS_THICKFRAME
	0x00020000  WS_MINIMIZEBOX


	0x080C0008  GWL_EXSTYLE
	----------
	0x08000000  WS_EX_NOACTIVATE
	0x00080000  WS_EX_LAYERED
	0x00040000  WS_EX_APPWINDOW
	0x00000008  WS_EX_TOPMOST
*/

