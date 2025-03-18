#pragma once
#include <windows.h>
#include <tchar.h>
#include <gdiplus.h>
#include <shellapi.h>
#include "Registry.h"
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "Msimg32.lib")


// Define GET_X_LPARAM and GET_Y_LPARAM if not available.
#ifndef GET_X_LPARAM
#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#endif
#ifndef GET_Y_LPARAM
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))
#endif


// Define custom command IDs
#define ID_SYNC_OSK_Y_POSITION   (WM_USER + 1)


#define WM_TRAYICON				 (WM_APP + 1)
#define IDM_TRAY_AUTOSTART		 1003
#define IDM_TRAY_EXIT			 1002
#define IDM_TRAY_SEPARATOR		 1001
#ifdef _DEBUG
#define IMAGE_PATH               _T("C:\\TabTap.png")
#else
#define IMAGE_PATH               _T("TabTap.png")
#endif // _DEBUG


HWND g_hWndMain{}; // Handle to the main application window
TCHAR g_mainExecPath[MAX_PATH];       // Buffer to store the path to the main application executable
const TCHAR g_mainName[]           = _T("TabTap"); // Application name
const TCHAR g_mainWndClassName[]   = _T("TabTapMainClass"); // Class name for the main application window
const TCHAR g_mainRegKey[]         = _T("SOFTWARE\\Empurple\\TabTap"); // Registry subkey for application settings
const TCHAR g_autoStartRegKey[]    = _T("Software\\Microsoft\\Windows\\CurrentVersion\\Run"); // Autorun registry subkey

const SIZE g_mainWndSizeCollapsed{ 7, 95 }; // Size of the Main Window in collapsed state
const SIZE g_mainWndSizeExpanded{ 29, 95 }; // Size of the Main Window in expanded state

HWND g_hWndOsk{}; // Handle to the on-screen keyboard window
TCHAR g_oskExecPathEX[MAX_PATH]; // Buffer to store the full path to the on-screen keyboard
const TCHAR g_oskExecPath[]        = _T("%WINDIR%\\System32\\osk.exe"); // Default path to the on-screen keyboard executable
const TCHAR g_oskRegKey[]          = _T("SOFTWARE\\Microsoft\\Osk"); // Registry subkey for on-screen keyboard settings
const TCHAR g_oskWndClassName[]    = _T("OSKMainClass"); // Class name for the on-screen keyboard window

ULONG_PTR g_gdiplusToken{}; // Token for GDI+ initialization
Gdiplus::Image* g_pApplicationImage{}; // Pointer to the application's image resource
NOTIFYICONDATA g_notifyIconData{}; // Data for the system tray icon
HMENU g_hTrayContextMenu{}; // Handle to the context menu for the system tray icon
HMODULE hDll{};
PROCESS_INFORMATION processInfo{};
STARTUPINFO startupInfo{};

bool isForAllUsers{}; // Indicates whether registry operations should use HKEY_LOCAL_MACHINE (true) or HKEY_CURRENT_USER (false)
RECT g_mainWindowRect{}; // Stores main window position


// Cleans up global resources used by the window.
void Cleanup()
{
	// Destroy context menu.
	if (g_hTrayContextMenu) {
		DestroyMenu(g_hTrayContextMenu);
		g_hTrayContextMenu = NULL;
	}

	// Remove the tray icon from the notification area.
	Shell_NotifyIcon(NIM_DELETE, &g_notifyIconData); // Returns true/false

	// Free any allocated image resources.
	if (g_pApplicationImage) {
		delete g_pApplicationImage;
		g_pApplicationImage = NULL;
	}

	// Shutdown GDI+ if it was initialized.
	if (g_gdiplusToken) {
		Gdiplus::GdiplusShutdown(g_gdiplusToken);
		g_gdiplusToken = NULL;
	}
}

// Performs additional cleanup and resource release upon error exit.
void ExitError()
{
	// Clean up common resources.
	Cleanup();

	// If a DLL was loaded, free it.
	if (hDll) {
		FreeLibrary(hDll);
		hDll = NULL;
	}

	// Close the thread handle if it is valid.
	if (processInfo.hThread) {
		CloseHandle(processInfo.hThread);
		processInfo.hThread = NULL;
	}
	// Close the process handle if it is valid.
	if (processInfo.hProcess) {
		CloseHandle(processInfo.hProcess);
		processInfo.hProcess = NULL;
	}
	// Close OSK if it is open.
	if (g_hWndOsk = FindWindow(g_oskWndClassName, NULL)) {
		PostMessage(g_hWndOsk, WM_CLOSE, 0, (LPARAM)TRUE);
	}
}

DWORD ErrorHandler(const TCHAR* message, DWORD errorCode = 0)
{
	TCHAR buffer[256];
	if (!errorCode) {
		errorCode = GetLastError();
	}
	wsprintf(buffer, _T("%s (%lu)"),message, errorCode);
	MessageBox(NULL, buffer, _T("Error"), MB_OK | MB_ICONERROR);
	ExitError();
	return errorCode;
}

void DrawImageOnLayeredWindow(HWND hwnd, bool isWindowExpanded)
{
	SIZE sizeWnd = isWindowExpanded ? g_mainWndSizeExpanded : g_mainWndSizeCollapsed;

	HDC hdcScreen = GetDC(NULL);
	HDC hdcMem = CreateCompatibleDC(hdcScreen);

	// Create a top-down 32-bit DIB section.
	BITMAPINFO bmi{};
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = sizeWnd.cx;
	bmi.bmiHeader.biHeight = -sizeWnd.cy; // Negative height for top-down bitmap.
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 32;
	bmi.bmiHeader.biCompression = BI_RGB;

	void* pvBits{};
	HBITMAP hBitmap = CreateDIBSection(hdcMem, &bmi, DIB_RGB_COLORS, &pvBits, NULL, 0);
	if (!hBitmap)  // Check if hBitmap is NULL
	{
		DeleteDC(hdcMem);
		ReleaseDC(NULL, hdcScreen);
		return; // Exit the function safely
	}

	HBITMAP hOldBmp = (HBITMAP)SelectObject(hdcMem, hBitmap);

	// Create a GDI+ Graphics object on the memory DC.
	Gdiplus::Graphics graphics(hdcMem);
	graphics.SetCompositingMode(Gdiplus::CompositingModeSourceOver);
	graphics.SetCompositingQuality(Gdiplus::CompositingQualityHighQuality);
	graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
	graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);

	// Clear with full transparency.
	graphics.Clear(Gdiplus::Color(0, 0, 0, 0));

	// Determine cropping parameters:
	int srcLeft = isWindowExpanded ? 0 : g_pApplicationImage->GetWidth() - g_mainWndSizeCollapsed.cx;
	int srcWidth = isWindowExpanded ? g_mainWndSizeExpanded.cx : g_mainWndSizeCollapsed.cx;

	graphics.DrawImage(g_pApplicationImage, 0, 0, srcLeft, 0, srcWidth, g_pApplicationImage->GetHeight(), Gdiplus::UnitPixel);

	// Prepare the blend function for per-pixel alpha.
	BLENDFUNCTION blendFunc = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };

	POINT ptSrc{};
	POINT ptDst;
	RECT rect;
	GetWindowRect(hwnd, &rect);
	ptDst.x = rect.left;
	ptDst.y = rect.top;

	UpdateLayeredWindow(hwnd, hdcScreen, &ptDst, &sizeWnd, hdcMem, &ptSrc, 0, &blendFunc, ULW_ALPHA);

	SelectObject(hdcMem, hOldBmp);  // Restore original bitmap
	DeleteObject(hBitmap);
	DeleteDC(hdcMem);
	ReleaseDC(NULL, hdcScreen);
}

void CreateTrayPopupMenu()
{
	g_hTrayContextMenu = CreatePopupMenu();
	// Check state
	TCHAR autostartData[MAX_PATH];
	bool autostartResult = ReadRegistry(isForAllUsers, g_autoStartRegKey, g_mainName, autostartData);

	AppendMenu(g_hTrayContextMenu, MF_SEPARATOR, IDM_TRAY_SEPARATOR, NULL);
	AppendMenu(g_hTrayContextMenu, MF_STRING | (autostartResult ? MF_CHECKED : 0), IDM_TRAY_AUTOSTART, _T("Autostart"));
	AppendMenu(g_hTrayContextMenu, MF_SEPARATOR, IDM_TRAY_SEPARATOR, NULL);
	AppendMenu(g_hTrayContextMenu, MF_STRING, IDM_TRAY_EXIT, _T("Exit"));
	AppendMenu(g_hTrayContextMenu, MF_SEPARATOR, IDM_TRAY_SEPARATOR, NULL);
}

void UpdateOSKPosition()
{
	DWORD oskWindowHeight;
	LONG oskNewTop;
	if (g_hWndMain) {
		GetWindowRect(g_hWndMain, &g_mainWindowRect);
	}
	ReadRegistry(isForAllUsers, g_oskRegKey, _T("WindowHeight"), &oskWindowHeight);
	oskNewTop = g_mainWindowRect.top - ((LONG)oskWindowHeight - g_mainWndSizeCollapsed.cy) / 2;
	oskNewTop = max(0L, min(GetSystemMetrics(SM_CYSCREEN) - (int)oskWindowHeight, oskNewTop)); // Clamp position within screen bounds
	WriteRegistry(isForAllUsers, g_oskRegKey, _T("WindowTop"), &oskNewTop, REG_DWORD);
}


LRESULT CALLBACK WindowProc(
	HWND hWnd,
	UINT uMsg,
	WPARAM wParam,
	LPARAM lParam)
{
	static bool isMouseTracking{};
	static bool isDragging{};
	static bool isWindowExpanded{}; // Tracks whether the window is expanded or collapsed
	static POINT dragStartPoint{};

	switch (uMsg)
	{
	case WM_TIMER:
	{
		SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
		break;
	}
	case WM_SETCURSOR:
	{
		SetCursor(LoadCursor(NULL, IDC_ARROW));
		break;
	}
	case WM_RBUTTONDOWN:
	{
		SetCapture(hWnd);
		isDragging = true;
		dragStartPoint = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
		ClientToScreen(hWnd, &dragStartPoint);
		GetWindowRect(hWnd, &g_mainWindowRect);
		break;
	}
	case WM_MOUSEACTIVATE:
	{
		return MA_NOACTIVATE; // Prevent window activation on mouse click
	}
	case WM_MOUSEMOVE:
	{
		if (isDragging)
		{
			POINT cursorPosition = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
			ClientToScreen(hWnd, &cursorPosition);
			int newWindowTop = g_mainWindowRect.top + cursorPosition.y - dragStartPoint.y;
			int screenHeight = GetSystemMetrics(SM_CYSCREEN);
			newWindowTop = max(0, min(newWindowTop, screenHeight - g_mainWndSizeExpanded.cy));
			int currentWidth = isWindowExpanded ? g_mainWndSizeExpanded.cx : g_mainWndSizeCollapsed.cx;
			SetWindowPos(hWnd, NULL, 0, newWindowTop, currentWidth, g_mainWndSizeExpanded.cy, SWP_NOZORDER | SWP_NOACTIVATE);
			DrawImageOnLayeredWindow(hWnd, isWindowExpanded);
		}
		else if (!isWindowExpanded)
		{
			GetWindowRect(hWnd, &g_mainWindowRect);
			SetWindowPos(hWnd, NULL, 0, g_mainWindowRect.top, g_mainWndSizeExpanded.cx, g_mainWndSizeCollapsed.cy, SWP_NOMOVE | SWP_NOZORDER);
			isWindowExpanded = true;
			DrawImageOnLayeredWindow(hWnd, isWindowExpanded);
		}
		if (!isMouseTracking)
		{
			TRACKMOUSEEVENT trackMouseEvent = { sizeof(TRACKMOUSEEVENT), TME_LEAVE, hWnd, 0 };
			TrackMouseEvent(&trackMouseEvent);
			isMouseTracking = true;
		}
		break;
	}
	case WM_RBUTTONUP:
	{
		isDragging = false;
		ReleaseCapture();
		break;
	}
	case WM_MBUTTONDOWN:
	{
		break;
	}
	case WM_MBUTTONUP:
	{
		SendMessage(hWnd, WM_LBUTTONUP, wParam, lParam);
		break;
	}
	case WM_LBUTTONDOWN:
	{
		break;
	}
	case WM_LBUTTONUP:
	{
		if (IsIconic(g_hWndOsk)) {
			UpdateOSKPosition();
			ShowWindowAsync(g_hWndOsk, SW_RESTORE); // Restore OSK
		}
		else if (IsWindowVisible(g_hWndOsk)) {
			ShowWindowAsync(g_hWndOsk, SW_HIDE); // Hide OSK
		}
		else {
			UpdateOSKPosition();
			ShowWindowAsync(g_hWndOsk, SW_SHOWNA); // Show OSK
		}
		break;
	}
	case WM_MOUSELEAVE:
	{
		isMouseTracking = false;
		if (!isDragging && isWindowExpanded) {
			GetWindowRect(hWnd, &g_mainWindowRect);
			SetWindowPos(hWnd, HWND_TOPMOST, 0, g_mainWindowRect.top, g_mainWndSizeCollapsed.cx, g_mainWndSizeCollapsed.cy, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
			isWindowExpanded = false;
			DrawImageOnLayeredWindow(hWnd, isWindowExpanded);
		}
		break;
	}
	case WM_TRAYICON:
	{
		if (lParam == WM_LBUTTONUP) {
			SendMessage(hWnd, WM_LBUTTONUP, 0, 0);
		}
		else if (lParam == WM_RBUTTONUP) {
			POINT cursorPosition;
			GetCursorPos(&cursorPosition);
			SetForegroundWindow(hWnd);
			TrackPopupMenu(g_hTrayContextMenu, TPM_LEFTBUTTON, cursorPosition.x, cursorPosition.y, 0, hWnd, NULL);
		}
		break;
	}
	case WM_COMMAND:
	{
		if (LOWORD(wParam) == IDM_TRAY_AUTOSTART) {
			UINT menuItemState = GetMenuState(g_hTrayContextMenu, IDM_TRAY_AUTOSTART, MF_BYCOMMAND);
			if ((menuItemState & MF_CHECKED) == MF_CHECKED) {
				RemoveRegistry(isForAllUsers, g_autoStartRegKey, g_mainName);
			}
			else {
				WriteRegistry(isForAllUsers, g_autoStartRegKey, g_mainName, g_mainExecPath, REG_SZ);
			}
			CheckMenuItem(g_hTrayContextMenu, IDM_TRAY_AUTOSTART, MF_BYCOMMAND | ((menuItemState & MF_CHECKED) ? MF_UNCHECKED : MF_CHECKED));
		}
		else if (LOWORD(wParam) == IDM_TRAY_EXIT) {
			SendMessage(hWnd, WM_CLOSE, 0, 0);
		}
		break;
	}
	case WM_CREATE:
	{
		CreateTrayPopupMenu();
		SetTimer(hWnd, 1, 3000, NULL); // 3000ms interval
		break;
	}
	case WM_DESTROY:
	{
		PostMessage(g_hWndOsk, WM_CLOSE, 0, (LPARAM)TRUE);
		GetWindowRect(g_hWndMain, &g_mainWindowRect);

		PostQuitMessage(0);
		return 0;
	}
	case ID_SYNC_OSK_Y_POSITION:
	{
		static bool is_process{};
		// Centers the main window vertically relative to the OSK window
		if (!is_process) {
			RECT oskWndRect;
			GetWindowRect(g_hWndOsk, &oskWndRect);
			GetWindowRect(hWnd, &g_mainWindowRect);
			lParam = MAKELPARAM(
				1,	// Local message marker
				oskWndRect.top + (oskWndRect.bottom - oskWndRect.top - g_mainWndSizeCollapsed.cy) / 2 // Target CY
			);
			is_process = true;
		}
		if (LOWORD(lParam)) {
			if (g_mainWindowRect.top != HIWORD(lParam)) {
				SetWindowPos(hWnd, NULL,
					g_mainWindowRect.left,
					g_mainWindowRect.top < HIWORD(lParam) ? ++g_mainWindowRect.top : --g_mainWindowRect.top,
					0, 0,
					SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE
				);
				PostMessage(
					hWnd,
					ID_SYNC_OSK_Y_POSITION,
					wParam, lParam
				);
			}
			else {
				is_process = false;
			}
		}
		break;
	}

	}
	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}


// Setup System Tray Icon
BOOL SetupTrayIcon(HWND hWnd)
{
	ZeroMemory(&g_notifyIconData, sizeof(g_notifyIconData));
	g_notifyIconData.cbSize = sizeof(NOTIFYICONDATA);
	g_notifyIconData.hWnd = hWnd;
	g_notifyIconData.uID = 1;  // Unique ID for tray icon
	g_notifyIconData.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
	g_notifyIconData.uCallbackMessage = WM_TRAYICON;  // Custom message for mouse events
	g_notifyIconData.hIcon = ExtractIcon(NULL, g_oskExecPathEX, 0); // First icon in the EXE
	lstrcpy(g_notifyIconData.szTip, g_mainName);

	return Shell_NotifyIcon(NIM_ADD, &g_notifyIconData);
}

// Load Image Resource
BOOL LoadImageResource()
{
	g_pApplicationImage = new Gdiplus::Image(IMAGE_PATH);
	if (g_pApplicationImage->GetLastStatus() != Gdiplus::Ok)
	{
		delete g_pApplicationImage;
		g_pApplicationImage = NULL;
		return FALSE;
	}
	return TRUE;
}

// Create Layered Window
HWND CreateLayeredWindow(HINSTANCE hInstance)
{
	// Retrieve and adjusts the main window vertical position based on registry-stored values
	// Ensure it remains on-screen and match on-screen keyboard's vertical center
	DWORD position{};
	DWORD oskWindowHeight{};
	int screenHeight = GetSystemMetrics(SM_CYSCREEN);
	bool result = ReadRegistry(isForAllUsers, g_oskRegKey, _T("WindowTop"), &position);
	position = result ? min((LONG)position, screenHeight - g_mainWndSizeCollapsed.cy) : 0;
	result = ReadRegistry(isForAllUsers, g_oskRegKey, _T("WindowHeight"), &oskWindowHeight);
	position += (oskWindowHeight - g_mainWndSizeCollapsed.cy) / 2;

	return CreateWindowEx(
		WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
		g_mainWndClassName,
		g_mainName,
		WS_POPUP,
		0, position, g_mainWndSizeCollapsed.cx, g_mainWndSizeCollapsed.cy,
		(HWND)NULL, (HMENU)NULL, hInstance, (LPVOID)NULL
	);
}

// Register Window Class
ATOM RegisterWindowClass(HINSTANCE hInstance)
{
	WNDCLASS wc{};
	wc.lpfnWndProc = WindowProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = g_mainWndClassName;
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
	return RegisterClass(&wc);
}

// Initialize GDI+
BOOL InitializeGDIPlus()
{
	Gdiplus::GdiplusStartupInput gdiInput;
	Gdiplus::Status gdiStatus = GdiplusStartup(&g_gdiplusToken, &gdiInput, NULL);
	return gdiStatus == Gdiplus::Ok;
}

/*
BOOL IsTaskbarWindow(HWND hWnd) {
	TCHAR szClassName[256];
	GetClassName(hWnd, szClassName, _countof(szClassName));


	HANDLE hFile = CreateFile(
		L"C:\\hooklog.txt",
		FILE_APPEND_DATA,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL,
		OPEN_ALWAYS,
		FILE_ATTRIBUTE_NORMAL,
		NULL
	);

	DWORD dwBytesToWrite = lstrlen(szClassName) * sizeof(WCHAR);
	DWORD dwBytesWritten = 0;

	WriteFile(hFile, szClassName, dwBytesToWrite, &dwBytesWritten, NULL);
	WriteFile(hFile, L"\r\n", 4, &dwBytesWritten, NULL);
	CloseHandle(hFile);




	// Check against known taskbar class names
	return (_tcscmp(szClassName, _T("Shell_TrayWnd")) == 0) ||
		(_tcscmp(szClassName, _T("Shell_SecondaryTrayWnd")) == 0) ||
		(_tcscmp(szClassName, _T("ApplicationManager_DesktopShellWindow")) == 0);
}

HWND GetTrueForegroundWindow() {
	// Get the thread ID of the foreground window
	HWND hForeground = GetForegroundWindow();
	DWORD dwForegroundThreadId = GetWindowThreadProcessId(hForeground, NULL);

	// Get GUI thread info for the foreground window's thread
	GUITHREADINFO guiInfo = { sizeof(GUITHREADINFO) };
	if (GetGUIThreadInfo(dwForegroundThreadId, &guiInfo)) {
		return guiInfo.hwndActive; // True active window
	}

	// Fallback to GetForegroundWindow()
	return hForeground;
}
*/

// Main Entry Point
int WINAPI WinMain(
	_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPSTR lpCmdLine,
	_In_ int nCmdShow)
{
	// Check program is already running.
	if (g_hWndMain = FindWindow(g_mainWndClassName, NULL)) {
		PostMessage(g_hWndMain, WM_LBUTTONUP, 0, 0);
		return ERROR_ALREADY_EXISTS;
	}
	// Close OSK if it is open.
	if (g_hWndOsk = FindWindow(g_oskWndClassName, NULL)) {
		SendMessage(g_hWndOsk, WM_CLOSE, 0, (LPARAM)TRUE);
	}


	if (!ExpandEnvironmentStrings(g_oskExecPath, g_oskExecPathEX, MAX_PATH)) {
		return ErrorHandler(_T("The environment variable expansion failed."));
	}

	DWORD result;
	if (!(result = GetModuleFileName(NULL, g_mainExecPath + 1, MAX_PATH))) {
		return ErrorHandler(_T("The system cannot find the path specified."));
	}
	else { // Format executable path with quotes for registry compatibility
		*g_mainExecPath = *(g_mainExecPath + result + 1) = _T('"');
	}


// CREATE WINDOW BEGIN
	if (!InitializeGDIPlus()) {
		return ErrorHandler(_T("Failed to initialize GDI."), -1);
	}
	if (!RegisterWindowClass(hInstance)) {
		return ErrorHandler(_T("Failed to register window class."));
	}
	if (!(g_hWndMain = CreateLayeredWindow(hInstance))) {
		return ErrorHandler(_T("Failed to create window."));
	}
	if (!LoadImageResource()) {
		return ErrorHandler(_T("Failed to load PNG image."), -1);
	}
	if (!SetupTrayIcon(g_hWndMain)) {
		return ErrorHandler(_T("Failed to setup tray icon."));
	}
	ShowWindow(g_hWndMain, nCmdShow);
	DrawImageOnLayeredWindow(g_hWndMain, false);
// CREATE WINDOW END


// HOOK INITIALIZATION BEGIN
	if (!(hDll = LoadLibrary(_T("CBTHook.dll")))) {
		return ErrorHandler(_T("The specified module could not be found."), ERROR_MOD_NOT_FOUND);
	}

	// Function pointer types for the exported functions.
	typedef BOOL(*UninstallHookFunc)();
	typedef BOOL(*InstallHookFunc)(DWORD);

	UninstallHookFunc UninstallHook = (UninstallHookFunc)GetProcAddress(hDll, "UninstallHook");
	InstallHookFunc InstallHook = (InstallHookFunc)GetProcAddress(hDll, "InstallHook");

	if (!UninstallHook or !InstallHook) {
		return ErrorHandler(_T("The specified procedure could not be found."), ERROR_PROC_NOT_FOUND);
	}
// HOOK INITIALIZATION END


// OSK STARTUP PROCESS BEGIN
	startupInfo.cb = sizeof(startupInfo);
	startupInfo.dwFlags = STARTF_USESHOWWINDOW;
	//startupInfo.wShowWindow = SW_SHOWNA;
	startupInfo.wShowWindow = SW_HIDE;

	if (!CreateProcess(
		g_oskExecPathEX,
		NULL,
		NULL,
		NULL,
		FALSE,
		0, // flags: CREATE_SUSPENDED
		NULL,
		NULL,
		&startupInfo,
		&processInfo
	))
	{
		return ErrorHandler(_T("Failed to create process."));
	}

	result = WaitForInputIdle(processInfo.hProcess, 3000);
	if (result == WAIT_TIMEOUT) {
		return ErrorHandler(_T("The wait time-out interval elapsed."), WAIT_TIMEOUT);
	}
	if (result == WAIT_FAILED) {
		return ErrorHandler(_T("Failed to wait for the Process."), WAIT_FAILED);
	}
// OSK STARTUP PROCESS END


// INJECT BEGIN
	// Create a manual-reset event that starts unsignaled.
	HANDLE hEvent{};
	if (!(hEvent = CreateEvent(NULL, TRUE, FALSE, _T("OSKLoadEvent")))) {
		return ErrorHandler(_T("Failed to Create Event."));
	}

	// Inject hook.
	if (!InstallHook(processInfo.dwThreadId)) {
		return ErrorHandler(_T("Failed to Install the Windows Hook procedure."), ERROR_HOOK_NOT_INSTALLED);
	}

	// Wait for the event to be signaled.
	result = WaitForSingleObject(hEvent, 3000);
	if (result == WAIT_TIMEOUT) {
		CloseHandle(hEvent);
		return ErrorHandler(_T("The wait time-out interval elapsed."), WAIT_TIMEOUT);
	}
	if (result == WAIT_FAILED) {
		CloseHandle(hEvent);
		return ErrorHandler(_T("Failed to wait for the Process."), WAIT_FAILED);
	}
	// Hook has signaled that is fully loaded.
	CloseHandle(hEvent);

	// Unload hook.
	if (!UninstallHook()) {
		return ErrorHandler(_T("Failed to remove the Windows hook."), ERROR_HOOK_NOT_INSTALLED);
	}

	// Unload library.
	FreeLibrary(hDll);
// INJECT END


	// Store OSK handle globally
	if (!(g_hWndOsk = FindWindow(g_oskWndClassName, NULL))) {
		return ErrorHandler(_T("Failed to Find Window."), -1);
	}

	MSG msg;
	while (GetMessage(&msg, nullptr, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	Cleanup();

	result = WaitForSingleObject(processInfo.hProcess, 3000);
	if (result == WAIT_TIMEOUT) {
		return ErrorHandler(_T("The wait time-out interval elapsed."), WAIT_TIMEOUT);
	}
	if (result == WAIT_FAILED) {
		return ErrorHandler(_T("Failed to wait for the Process."), WAIT_FAILED);
	}

	UpdateOSKPosition(); // NOTE: Last main rect update was in WM_DESTROY

	CloseHandle(processInfo.hThread);
	CloseHandle(processInfo.hProcess);

	return 0;
}




