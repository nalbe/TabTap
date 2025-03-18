#include <windows.h>


// Define GET_X_LPARAM and GET_Y_LPARAM if not available.
#ifndef GET_X_LPARAM
#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#endif
#ifndef GET_Y_LPARAM
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))
#endif

// Define custom command IDs
#define ID_SYNC_OSK_Y_POSITION       (WM_USER + 1)
#define ID_DIRECTUI_HWND_CREATED     (WM_USER + 2)

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
	DWORD dwBytesWritten = 0;
	return WriteFile(hFile, message, dwBytesToWrite, &dwBytesWritten, NULL)
		and (dwBytesWritten == dwBytesToWrite);
}

BOOL WriteDWORD(DWORD dwValue)
{
	WCHAR buffer[20];  // Enough for any int
	DWORD dwBytesWritten{};
	return WriteString(buffer, wsprintf(buffer, L"%d", dwValue));
}

BOOL LogError(LPCWSTR message, BOOL useLastError = TRUE)
{
	EnterCriticalSection(&g_csLog);
	DWORD lastErrorCode = useLastError ? GetLastError() : 0;
	BOOL bSuccess{};

	if (!WriteString(message) or
		!WriteString(ERROR_CODE_PREFIX, ERROR_CODE_PREFIX_LEN) or
		!WriteDWORD(lastErrorCode) or
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
		LogError(L"Failed to remove hook.");
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

	return (g_hHook != NULL);
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
	switch (msg)
	{
	case WM_RBUTTONUP:
	{
		PostMessage(g_hOSKMainWnd, WM_RBUTTONUP, wParam, lParam);
		break;
	}
	case WM_MBUTTONDOWN:
	{
		PostMessage(g_hOSKMainWnd, WM_MBUTTONDOWN, wParam, lParam);
		break;
	}
	default:
		break;
	}
	return CallWindowProc(g_origDirectUIWndProc, hWnd, msg, wParam, lParam);
}


LRESULT CALLBACK MainWndProc(
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
	case WM_MOUSEACTIVATE:
	{
		return MA_NOACTIVATE; // Block ALL activation attempts (client/non-client)
	}
	case WM_ACTIVATE:
	{
		if (wParam != WA_INACTIVE) {
			PostMessage(hWnd, WM_KILLFOCUS, 0, 0); // Force deactivate if somehow activated
		}
		return 0;
	}
	case WM_MOUSEMOVE:
	{
		if (g_isDragging) {
			POINT ptCursor;
			GetCursorPos(&ptCursor); // Get cursor screen coordinates
			SetWindowPos(            // Original position + delta
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
		if (wParam == HTCLOSE) {
			return 0;
		}
		if (wParam == HTSYSMENU) {
			//MessageBox(hWnd, L"System menu icon clicked!", L"Detected", MB_OK);
			return 0;
		}
		SendMessage(hWnd, WM_MBUTTONDOWN, wParam, lParam); // Move action
		break;
	}
	case WM_NCMBUTTONUP:
	{
		if (wParam == HTCLOSE) {
			PostMessage(g_hTabTapMainWnd, ID_SYNC_OSK_Y_POSITION, 0, 0);
			ShowWindow(hWnd, SW_HIDE);
			return 0;  // Prevent default behavior
		}
		break;
	}
	case WM_NCLBUTTONDOWN:
	{
		break;
	}
	case WM_NCLBUTTONUP: // NOTE: Skips first trigger for some reason
	{
		break;
	}
	case WM_MBUTTONDOWN:
	{
		SetCapture(g_hOSKMainWnd); // Redirect all mouse input to this window
		GetCursorPos(&g_dragStartCursorPos);
		GetWindowRect(g_hOSKMainWnd, &g_dragStartWindowRect);
		g_isDragging = true;
		return 0;
	}
	case WM_MBUTTONUP:
	{
		if (g_isDragging) {
			ReleaseCapture();
			g_isDragging = false;
		}
		return 0;
	}
	case WM_RBUTTONUP:
	{
		return 0; // Message handled
	}
	case WM_CLOSE:
	{
		if ((BOOL)lParam == TRUE) { // Real close event
			break;
		}
		ShowWindow(hWnd, SW_HIDE);
		return 0; // Message handled
	}
	case WM_CAPTURECHANGED:
	{
		if ((HWND)lParam != hWnd) { // Fallback: Ensure capture is released if mouse is lost
			g_isDragging = false;
		}
		break;
	}
	case ID_DIRECTUI_HWND_CREATED:
	{
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
		break;
	}
	case WM_DESTROY:
	{
		DestroyIcon(g_hOSKIcon);
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
		// Retrieve the window class name.
		if (!GetClassName(hWnd, szClassName, sizeof(szClassName) / sizeof(TCHAR))) {
			break;
		}

		if (wcscmp(szClassName, L"DirectUIHWND") == 0) {
			// Store handle as global
			g_hDirectUIWnd = hWnd;
			// Notify that proc can be subclassed now
			PostMessage(g_hOSKMainWnd, ID_DIRECTUI_HWND_CREATED, 0, 0); 

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
		// Retrieve the window class name.
		if (!GetClassName(hWnd, szClassName, sizeof(szClassName) / sizeof(TCHAR))) {
			break;
		}

		if (wcscmp(szClassName, L"OSKMainClass") == 0) {
			static bool bProcessed{}; // To process only once.
			if (bProcessed) {
				break;
			}
			bProcessed = true;


			// Store the original window procedure.
			g_origOSKMainWndProc = (WNDPROC)GetWindowLongPtr(
				hWnd,
				GWLP_WNDPROC
			);

			// Replace original proc.
			SetWindowLongPtr(
				hWnd,
				GWLP_WNDPROC,
				(LONG_PTR)MainWndProc
			);


			// Edit Style.
			LONG_PTR style = GetWindowLongPtr(hWnd, GWL_STYLE);
			style &= ~WS_MINIMIZEBOX;
			style &= ~WS_TABSTOP;        // NOTE: nothing happen
			style &= ~WS_POPUP;	         // NOTE: nothing happen
			SetWindowLongPtr(hWnd, GWL_STYLE, style);

			// Edit ExStyle.
			LONG_PTR exstyle = GetWindowLongPtr(hWnd, GWL_EXSTYLE);
			exstyle &= ~WS_EX_APPWINDOW;
			exstyle |= WS_EX_NOACTIVATE; // NOTE: nothing happen
			//exstyle |= WS_EX_TOOLWINDOW;
			//exstyle |= WS_EX_LAYERED;
			//exstyle |= WS_EX_TOPMOST;
			SetWindowLongPtr(hWnd, GWL_EXSTYLE, exstyle);

			// Edit System Menu.
			HMENU hSysMenu = GetSystemMenu(hWnd, FALSE);
			DeleteMenu(hSysMenu, SC_RESTORE, MF_BYCOMMAND);
			DeleteMenu(hSysMenu, SC_MINIMIZE, MF_BYCOMMAND);
			DeleteMenu(hSysMenu, SC_MAXIMIZE, MF_BYCOMMAND);

			// Apply style changes.
			SetWindowPos(hWnd, nullptr, 0, 0, 0, 0,
				SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);


			// Store the first app handle as global
			g_hTabTapMainWnd = FindWindow(L"TabTapMainClass", NULL);

			// Store the main handle as global
			g_hOSKMainWnd = hWnd;

			// Show after changes
			ShowWindowAsync(hWnd, SW_SHOW); // SW_SHOWNA cause flickering

			// Increment DLL reference count
			HMODULE hMod;
			if (GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCWSTR)MainWndProc, &hMod)) {
				WCHAR dllPath[MAX_PATH];
				GetModuleFileName(hMod, dllPath, MAX_PATH);
				LoadLibrary(dllPath); // Increments ref count
			}

			// Load Icon from exe
			ExtractIconEx(g_oskExecPath, 0, &g_hOSKIcon, NULL, 1);
		}
		return 0;
	}

	default:
		break;
	}

	return CallNextHookEx(g_hHook, nCode, wParam, lParam);
}



