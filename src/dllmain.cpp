#include <windows.h>



//#define LOG

#ifdef LOG

HANDLE hFile = CreateFile(
	L"C:\\hooklog.txt",
	FILE_APPEND_DATA,
	FILE_SHARE_READ | FILE_SHARE_WRITE,
	NULL,
	OPEN_ALWAYS,
	FILE_ATTRIBUTE_NORMAL,
	NULL
);

CRITICAL_SECTION g_csLog;              // Serialize log access

#define ERROR_CODE_PREFIX      L" Error Code is "
#define ERROR_CODE_PREFIX_LEN  (ARRAYSIZE(ERROR_CODE_PREFIX) - 1)  // Length without null
#define NEWLINE                L"\r\n"
#define NEWLINE_LEN            (ARRAYSIZE(NEWLINE) - 1)

#endif // LOG


HINSTANCE g_hInstance = NULL;          // DLL instance
HHOOK g_hHook = NULL;                  // CBT hook handle
WNDPROC g_originalWndProc = NULL;        // Original window procedure for subclassing


LRESULT CALLBACK CBTProc(int, WPARAM, LPARAM);

#ifdef LOG

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

BOOL LogError(LPCWSTR message)
{
	EnterCriticalSection(&g_csLog);
	DWORD lastErrorCode = GetLastError();
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

BOOL LogSuccess(LPCWSTR message)
{
	EnterCriticalSection(&g_csLog);
	BOOL bSuccess{};

	if (!WriteString(message) or
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

#endif // LOG

extern "C" __declspec(dllexport)
BOOL UninstallHook()
{
	if (!g_hHook) {
#ifdef LOG
		LogError(L"No hook to remove.");
#endif // LOG
		return FALSE;
	}
	if (!UnhookWindowsHookEx(g_hHook)) {
#ifdef LOG
		LogError(L"Failed to remove hook.");
#endif // LOG
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
#ifdef LOG
		InitializeCriticalSection(&g_csLog);
#endif // LOG
		break;
	}
	case DLL_PROCESS_DETACH:
	{
		if (g_hHook != NULL) {
			UnhookWindowsHookEx(g_hHook);
			g_hHook = NULL;
		}
#ifdef LOG
		if (hFile != INVALID_HANDLE_VALUE) {
			CloseHandle(hFile);
			hFile = INVALID_HANDLE_VALUE;
		}
		DeleteCriticalSection(&g_csLog);
#endif // LOG
		break;
	}
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	{
		break;
	}
	}
	return TRUE;
}


LRESULT CALLBACK SubclassedWndProc(
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

	}
	return CallWindowProc(g_originalWndProc, hWnd, msg, wParam, lParam);
}


LRESULT CALLBACK CBTProc(
	int nCode,
	WPARAM wParam,
	LPARAM lParam)
{
	if (nCode < 0)
	{
		return CallNextHookEx(g_hHook, nCode, wParam, lParam);
	}
	if (nCode == HCBT_ACTIVATE)
	{
		HWND hWnd = (HWND)wParam;
		TCHAR szClassName[MAX_PATH]{};

		// Retrieve the window class name.
		if (GetClassName(hWnd, szClassName, sizeof(szClassName) / sizeof(TCHAR)))
		{
			// Check if the class name is "OSKMainClass".
			if (wcscmp(szClassName, L"OSKMainClass") == 0)
			{
				// Use a static flag to process only once.
				static bool bProcessed = false;
				if (!bProcessed)
				{
					bProcessed = true;

					// Store the original window procedure.
					g_originalWndProc = (WNDPROC)GetWindowLongPtr(
						hWnd,
						GWLP_WNDPROC
					);

					// Replace original proc.
					SetWindowLongPtr(
						hWnd,
						GWLP_WNDPROC,
						(LONG_PTR)SubclassedWndProc
					);

					// Remove taskbar entry.
					LONG_PTR exstyle = GetWindowLongPtr(hWnd, GWL_EXSTYLE);
					exstyle &= ~WS_EX_APPWINDOW;
					SetWindowLong(hWnd, GWL_EXSTYLE, exstyle);

					// Remove minimize button.
					LONG_PTR style = GetWindowLongPtr(hWnd, GWL_STYLE);
					style &= ~WS_MINIMIZEBOX;
					SetWindowLong(hWnd, GWL_STYLE, style);

					// Apply style changes.
					SetWindowPos(hWnd, nullptr, 0, 0, 0, 0,
						SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);


					// Increment DLL reference count
					HMODULE hMod;
					if (GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCWSTR)SubclassedWndProc, &hMod)) {
						WCHAR dllPath[MAX_PATH];
						GetModuleFileName(hMod, dllPath, MAX_PATH);
						LoadLibrary(dllPath); // Increments ref count
					}

					ShowWindowAsync(hWnd, NULL);

					// Open the named event created by the first app
					HANDLE hEvent = OpenEvent(EVENT_MODIFY_STATE, FALSE, L"OSKLoadEvent");
					if (hEvent != NULL) {
						// Signal the event indicating that this app has finished loading
						SetEvent(hEvent);
						CloseHandle(hEvent);
					}
					else {
						// Handle error if the event doesn't exist
					}
				}
			}
		}
		return 0;
	}

	return CallNextHookEx(g_hHook, nCode, wParam, lParam);
}



