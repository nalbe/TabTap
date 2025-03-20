#include <windows.h>


// Define GET_X_LPARAM and GET_Y_LPARAM if not available.
#ifndef GET_X_LPARAM
#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#endif
#ifndef GET_Y_LPARAM
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))
#endif


// Define Messages Source code
#define MENU            (0)
#define ACCELERATOR     (1)
#define CONTROL         (2)

// Define custom command IDs
#define IDM_APP_SYNC_Y_POSITION      (WM_APP + 1)
#define IDM_APP_DIRECTUI_CREATED     (WM_APP + 2)
#define IDM_APP_DOCKMODE             (WM_APP + 3)
#define IDM_APP_REGULARMODE          (WM_APP + 4)


// Store globals
HINSTANCE g_hInstance             = NULL; // DLL instance
HHOOK g_hHook                     = NULL; // CBT hook handle
HWND g_hTabTapMainWnd             = NULL; // TabTapMainClass handle
HWND g_hDirectUIWnd               = NULL; // DirectUIHWnd handle
HWND g_hOSKMainWnd                = NULL; // OSKMainClass handle
const TCHAR g_oskExecPath[]       = L"%WINDIR%\\System32\\osk.exe"; // Default path to the on-screen keyboard executable

WNDPROC g_origOSKMainWndProc      = NULL; // Original OSKMainClass window procedure
WNDPROC g_origDirectUIWndProc     = NULL; // Original DirectUIHWND window procedure
HICON g_hOSKIcon                  = NULL; // Use icon provided in osk.exe


// Forward declarations
LRESULT CALLBACK CBTProc(int, WPARAM, LPARAM);


//#define USE_LOG
#ifdef USE_LOG
#define LOG_PATH      L"C:\\hooklog.txt"

HANDLE hFile = CreateFile(
	LOG_PATH,
	FILE_APPEND_DATA,
	FILE_SHARE_READ | FILE_SHARE_WRITE,
	NULL,
	OPEN_ALWAYS,
	FILE_ATTRIBUTE_NORMAL,
	NULL
);

CRITICAL_SECTION g_csLog; // Serialize log access

#define ERROR_CODE_PREFIX      L" Error: "
#define ERROR_CODE_PREFIX_LEN  (ARRAYSIZE(ERROR_CODE_PREFIX) - 1) // Length without null
#define NEWLINE                L"\r\n"
#define NEWLINE_LEN            (ARRAYSIZE(NEWLINE) - 1)


BOOL WriteString(LPCWSTR message, DWORD cchMessage = 0)
{
	DWORD dwBytesToWrite = (cchMessage ? cchMessage : lstrlen(message)) * sizeof(WCHAR);
	DWORD dwBytesWritten{};
	return WriteFile(hFile, message, dwBytesToWrite, &dwBytesWritten, NULL)
		and (dwBytesWritten == dwBytesToWrite);
}

BOOL WriteDWORD(DWORD dwValue)
{
	WCHAR buffer[20];  // Enough for any int
	return WriteString(buffer, wsprintf(buffer, L"%d", dwValue));
}

BOOL LogError(LPCWSTR message, DWORD error = ERROR_SUCCESS)
{
	EnterCriticalSection(&g_csLog);
	BOOL bSuccess{};

	if (!WriteString(message) or
		!WriteString(ERROR_CODE_PREFIX, ERROR_CODE_PREFIX_LEN) or
		!WriteDWORD(error) or
		!WriteString(NEWLINE, NEWLINE_LEN))
	{
		bSuccess = FALSE;
	}
	else {
		bSuccess = FlushFileBuffers(hFile);
	}
	LeaveCriticalSection(&g_csLog);
	return bSuccess;
}

#endif // USE_LOG


extern "C" __declspec(dllexport)
BOOL UninstallHook()
{
	if (!g_hHook) {
#ifdef USE_LOG
		LogError(L"No hook to remove.");
#endif // USE_LOG
		return FALSE;
	}
	if (!UnhookWindowsHookEx(g_hHook)) {
#ifdef USE_LOG
		LogError(L"Failed to remove hook.", GetLastError());
#endif // USE_LOG
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
#ifdef USE_LOG
		LogError(L"Failed to set windows hook.");
#endif // USE_LOG
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
		DisableThreadLibraryCalls(hInstance); // Optional: Improve performance
#ifdef USE_LOG
		InitializeCriticalSection(&g_csLog);
#endif // USE_LOG
		break;
	}
	case DLL_PROCESS_DETACH:
	{
		if (g_hHook != NULL) {
			UnhookWindowsHookEx(g_hHook);
			g_hHook = NULL;
		}
#ifdef USE_LOG
		if (hFile != INVALID_HANDLE_VALUE) {
			CloseHandle(hFile);
			hFile = INVALID_HANDLE_VALUE;
		}
		DeleteCriticalSection(&g_csLog);
#endif // USE_LOG
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
	// Since the keys belong to DirectUIHWND, some capturing should occur here
	switch (msg)
	{

	/*
	case WM_STYLECHANGING: // Has no effect
	case WM_SIZE: // Has no effect
	case WM_RBUTTONDBLCLK: // Has no effect (Require the CS_DBLCLKS)
	*/

	case WM_RBUTTONDOWN:
	{
		// Simulate the WM_RBUTTONDOWN event for the parent window
		PostMessage(g_hOSKMainWnd, WM_MBUTTONDOWN, wParam, lParam);
		return 0;
	}
	case WM_RBUTTONUP:
	{
		// If SetCapture() is called on button down, catch button up in MainWndProc.
		return 0;
	}
	case WM_MBUTTONDOWN:
	{
		// Simulate the same event for the parent window
		PostMessage(g_hOSKMainWnd, WM_MBUTTONDOWN, wParam, lParam);
		return 0;
	}
	default:
		break;
	}
	return CallWindowProc(g_origDirectUIWndProc, hWnd, msg, wParam, lParam);
}


LRESULT CALLBACK OSKMainWndProc(
	HWND hWnd,
	UINT msg,
	WPARAM wParam,
	LPARAM lParam)
{
	static bool g_isDragging{};
	static POINT g_dragStartCursorPos{};
	static RECT g_dragStartWindowRect{};

	switch (msg)
	{

	/*
	case WM_MOUSEACTIVATE: // Has no effect
	case WM_ACTIVATE: // Has no effect
	case WM_SIZING: // Has no effect
	case WM_SIZE: // Affect DirectUI redraw
	case WM_WINDOWPOSCHANGED: // Affect DirectUI redraw
	case WM_GETMINMAXINFO: // Allow resizing the window to any values (currently buggy)
	*/

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
		}
		return 0;
	}
	case WM_MOUSEMOVE:
	{
		// Drag implementation
		if (g_isDragging) {
			POINT ptCursor;
			GetCursorPos(&ptCursor); // Get cursor screen coordinates
			SetWindowPos( // Original position + delta
				g_hOSKMainWnd, NULL,
				g_dragStartWindowRect.left + (ptCursor.x - g_dragStartCursorPos.x),
				g_dragStartWindowRect.top + (ptCursor.y - g_dragStartCursorPos.y),
				0, 0,
				SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE
			);
		}
		break;
	}
	case WM_NCMBUTTONDOWN:
	{
		// Repositioning the First App on middle button down of the 'X'
		if (wParam == HTCLOSE) {
			PostMessage(
				g_hTabTapMainWnd,
				WM_COMMAND,
				MAKEWPARAM(IDM_APP_SYNC_Y_POSITION, CONTROL),
				(LPARAM)hWnd
			);
			return 0;
		}
		// TODO
		if (wParam == HTSYSMENU) {
			//MessageBox(hWnd, L"System menu icon clicked!", L"Detected", MB_OK);
			return 0;
		}
		// In general, it simulates a middle click in the client zone
		SendMessage(hWnd, WM_MBUTTONDOWN, wParam, lParam); // Drag trigger
		break;
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
	case WM_NCLBUTTONDOWN:
	{
		break;
	}
	case WM_NCLBUTTONUP: // It skips the first trigger for some reason
	{
		break;
	}
	case WM_MBUTTONDOWN:
	{
		// Drag begin
		SetCapture(g_hOSKMainWnd); // Redirect all mouse input to this window
		GetCursorPos(&g_dragStartCursorPos);
		GetWindowRect(g_hOSKMainWnd, &g_dragStartWindowRect);
		g_isDragging = true;
		return 0;
	}
	case WM_MBUTTONUP:
	{
		// Drag end
		if (g_isDragging) {
			ReleaseCapture();
			g_isDragging = false;
		}
		return 0;
	}
	case WM_RBUTTONUP:
	{
		PostMessage(hWnd, WM_MBUTTONUP, wParam, lParam); // Drag stop trigger
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
	case WM_CAPTURECHANGED:
	{
		// Fallback: Ensure capture is released if mouse is lost
		if ((HWND)lParam != hWnd) {
			g_isDragging = false;
		}
		break;
	}
	case WM_COMMAND:
	{
		if (HIWORD(wParam) == CONTROL) {
			if (LOWORD(wParam) == IDM_APP_DIRECTUI_CREATED) {
				//Subclass DirectUIHWND here, right after the CREATEWND event
				if (!g_hDirectUIWnd) {
					break;
				}
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
			else if (LOWORD(wParam) == IDM_APP_DOCKMODE) {
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
			else if (LOWORD(wParam) == IDM_APP_REGULARMODE) {
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
		}
		break;
	}
	case WM_DESTROY:
	{
		break;
	}

	default:
		break;
	}
	return CallWindowProc(g_origOSKMainWndProc, hWnd, msg, wParam, lParam);
}




LRESULT CALLBACK CBTProc(
	int nCode,
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

		if (wcscmp(szClassName, L"DirectUIHWND") == 0) {
			// Store handle as global
			g_hDirectUIWnd = hWnd;

			// Notify that proc can be subclassed now
			PostMessage(
				g_hOSKMainWnd,
				WM_COMMAND,
				MAKEWPARAM(IDM_APP_DIRECTUI_CREATED, CONTROL),
				(LPARAM)hWnd
			);


			// Open the named event created by the first app
			HANDLE hEvent = OpenEvent(EVENT_MODIFY_STATE, FALSE, L"OSKLoadEvent");
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

		if (wcscmp(szClassName, L"OSKMainClass") == 0) {
			static bool bProcessed{}; // To process only once
			if (bProcessed) {
				break;
			}
			bProcessed = true;


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
			SetWindowPos(hWnd, nullptr, 0, 0, 0, 0,
				SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);


			// Store the first app handle as global
			g_hTabTapMainWnd = FindWindow(L"TabTapMainClass", NULL);

			// Store the main handle as global
			g_hOSKMainWnd = hWnd;

			// Show after changes
			ShowWindowAsync(hWnd, SW_SHOW); // `SW_SHOWNA` causes flickering

			// Increment DLL reference count
			HMODULE hMod;
			if (GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCWSTR)OSKMainWndProc, &hMod)) {
				WCHAR dllPath[MAX_PATH];
				GetModuleFileName(hMod, dllPath, MAX_PATH);
				LoadLibrary(dllPath);
			}
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
	default:
		break;
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

