// Compile as DLL

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

#define OSK_CLASSNAME       L"OSKMainClass"
#define OSK_APPPATH         L"%WINDIR%\\System32\\osk.exe"

HINSTANCE g_hInstance = NULL;          // DLL instance
HHOOK g_hHook = NULL;                  // CBT hook handle
WNDPROC originalWndProc = NULL;        // Original window procedure for subclassing


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
#ifdef LOG
	if (hFile) {
		LogSuccess(L"Hook removed successfully.");
		CloseHandle(hFile);
	}
#endif // LOG
	g_hHook = NULL;

	return TRUE;
}

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

extern "C" __declspec(dllexport)
BOOL CloseOSK()
{
	if (!UninstallHook()) {
		return FALSE;
	}
	HWND hWnd = FindWindow(OSK_CLASSNAME, NULL);
	if (hWnd) {
		SendMessage(hWnd, WM_CLOSE, 0, 0); // Close OSK
		return TRUE;
	}
	return FALSE;
}

extern "C" __declspec(dllexport)
BOOL LaunchOSK()
{
	WCHAR g_oskExecutablePathEX[MAX_PATH];
	if (!ExpandEnvironmentStrings(OSK_APPPATH, g_oskExecutablePathEX, MAX_PATH)) {
#ifdef LOG
		LogError(L"Cannot expand executable path.");
#endif // LOG
		return FALSE;
	}

	PROCESS_INFORMATION processInfo{};
	STARTUPINFO startupInfo{};
	startupInfo.cb = sizeof(startupInfo);
	startupInfo.dwFlags = STARTF_USESHOWWINDOW;
	//startupInfo.wShowWindow = SW_SHOWNA;
	startupInfo.wShowWindow = SW_HIDE;

	if (!CreateProcess(
		g_oskExecutablePathEX,
		NULL,
		NULL,
		NULL,
		FALSE,
		0, // alt CREATE_SUSPENDED
		NULL,
		NULL,
		&startupInfo,
		&processInfo
	))
	{
#ifdef LOG
		LogError(L"Create Process failed.");
#endif // LOG
		return FALSE;
	}

	DWORD result = WaitForInputIdle(processInfo.hProcess, 3000);
	if (result == WAIT_TIMEOUT) {
#ifdef LOG
		LogError(L"The wait of the Process was terminated because the time-out interval elapsed.");
#endif // LOG
		CloseHandle(processInfo.hThread);
		CloseHandle(processInfo.hProcess);
		return FALSE;
	}
	if (result == WAIT_FAILED) {
#ifdef LOG
		LogError(L"Failed to wait for the Process.");
#endif // LOG
		CloseHandle(processInfo.hThread);
		CloseHandle(processInfo.hProcess);
		return FALSE;
	}

	if (!InstallHook(processInfo.dwThreadId)) {
#ifdef LOG
		LogError(L"Failed to Set Windows Hook.");
#endif // LOG
		return FALSE;
	}

	CloseHandle(processInfo.hThread);
	CloseHandle(processInfo.hProcess);
#ifdef LOG
	LogSuccess(L"Hook installed successfully.");
#endif // LOG

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
	HWND hwnd,
	UINT msg,
	WPARAM wParam,
	LPARAM lParam)
{
	switch (msg)
	{
	case WM_CLOSE:
	{
		ShowWindow(hwnd, SW_HIDE);
		return 0;
	}
	case WM_NCMBUTTONUP:
	{
		ShowWindow(hwnd, SW_HIDE);
		return 0;
	}
	}
	return CallWindowProc(originalWndProc, hwnd, msg, wParam, lParam);
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
		WNDPROC currentWndProc = (WNDPROC)GetWindowLongPtr(
			(HWND)wParam,
			GWLP_WNDPROC
		);

		// Check if the window is already subclassed.
		if (currentWndProc != SubclassedWndProc) {
			// Store the original window procedure and subclass.
			originalWndProc = currentWndProc;
			SetWindowLongPtr(
				(HWND)wParam,
				GWLP_WNDPROC,
				(LONG_PTR)SubclassedWndProc
			);

			// Remove taskbar entry.
			LONG_PTR exstyle = GetWindowLongPtr((HWND)wParam, GWL_EXSTYLE);
			exstyle &= ~WS_EX_APPWINDOW;
			SetWindowLong((HWND)wParam, GWL_EXSTYLE, exstyle);

			// Remove minimize button.
			LONG_PTR style = GetWindowLongPtr((HWND)wParam, GWL_STYLE);
			style &= ~WS_MINIMIZEBOX;
			SetWindowLong((HWND)wParam, GWL_STYLE, style);

			// Apply style changes.
			SetWindowPos((HWND)wParam, nullptr, 0, 0, 0, 0,
				SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

			ShowWindowAsync((HWND)wParam, NULL);
		}
		return 0;
	}

	return CallNextHookEx(g_hHook, nCode, wParam, lParam);
}



