#include <windows.h>


//#define USE_LOG

#ifdef USE_LOG

HANDLE hFile = CreateFile(
	L"C:\\hooklog.txt",
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

#endif // USE_LOG

// Define custom command IDs
#define ID_SYNC_OSK_Y_POSITION       (WM_USER + 1)
#define ID_DIRECTUI_HWND_CREATED     (WM_USER + 2)


HINSTANCE g_hInstance             = NULL; // DLL instance
HHOOK g_hHook                     = NULL; // CBT hook handle
HWND g_hTabTapWnd                 = NULL; // First app handle
HWND g_hOSKMainWnd                = NULL; // OSKMain handle
HWND g_hDirectUIWnd               = NULL; // DirectUIHWnd handle

WNDPROC g_origMainWndProc         = NULL; // Original OSKMainClass window procedure
WNDPROC g_origDirectUIWndProc     = NULL; // Original DirectUIHWND window procedure


LRESULT CALLBACK CBTProc(int, WPARAM, LPARAM);


#ifdef USE_LOG

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
		// TODO
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
	switch (msg)
	{

	case WM_CLOSE:
	{
		if ((BOOL)lParam == TRUE) { // Real event
			break;
		}
		ShowWindow(hWnd, SW_HIDE);
		return 0;
	}
	case WM_NCMBUTTONUP:
	{
		ShowWindow(hWnd, SW_HIDE);
		return 0;
	}
	case WM_NCMBUTTONDOWN:
	{
		if (g_hTabTapWnd) {
			PostMessage(
				g_hTabTapWnd,
				ID_SYNC_OSK_Y_POSITION,
				0, 0
			);
		}
		return 0;
	}
	case ID_DIRECTUI_HWND_CREATED:
	{
		if (!g_hDirectUIWnd) {
			return 0;
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
	default:
		break;
	}
	return CallWindowProc(g_origMainWndProc, hWnd, msg, wParam, lParam);
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
			g_origMainWndProc = (WNDPROC)GetWindowLongPtr(
				hWnd,
				GWLP_WNDPROC
			);

			// Replace original proc.
			SetWindowLongPtr(
				hWnd,
				GWLP_WNDPROC,
				(LONG_PTR)MainWndProc
			);


			// Remove taskbar entry.
			LONG_PTR exstyle = GetWindowLongPtr(hWnd, GWL_EXSTYLE);
			exstyle &= ~WS_EX_APPWINDOW;
			SetWindowLong(hWnd, GWL_EXSTYLE, exstyle);

			// Remove minimize button.
			LONG_PTR style = GetWindowLongPtr(hWnd, GWL_STYLE);
			style &= ~WS_MINIMIZEBOX;
			SetWindowLong(hWnd, GWL_STYLE, style);

			// Edit the system menu
			HMENU hSysMenu = GetSystemMenu(hWnd, FALSE);
			DeleteMenu(hSysMenu, SC_RESTORE, MF_BYCOMMAND);
			DeleteMenu(hSysMenu, SC_MINIMIZE, MF_BYCOMMAND);
			DeleteMenu(hSysMenu, SC_MAXIMIZE, MF_BYCOMMAND);

			// Apply style changes.
			SetWindowPos(hWnd, nullptr, 0, 0, 0, 0,
				SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);


			// Store the first app handle as global
			g_hTabTapWnd = FindWindow(L"TabTapMainClass", NULL);

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
		}
		return 0;
	}

	default:
		break;
	}

	return CallNextHookEx(g_hHook, nCode, wParam, lParam);
}



