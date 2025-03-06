#pragma once
#include <windows.h>
#include <tchar.h>
#include <gdiplus.h>
#include <shellapi.h>
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "Msimg32.lib")

// Define GET_X_LPARAM and GET_Y_LPARAM if not available.
#ifndef GET_X_LPARAM
#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#endif
#ifndef GET_Y_LPARAM
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))
#endif

#define WM_TRAYICON (WM_APP + 1)
#define IDM_TRAY_HIDEMINBUTTON	1003
#define IDM_TRAY_AUTOSTART		1002
#define IDM_TRAY_EXIT			1001


HWND g_hWndMain; // Handle to the main application window
HWND g_hWndOsk; // Handle to the on-screen keyboard window
ULONG_PTR g_gdiplusToken; // Token for GDI+ initialization
Gdiplus::Image* g_pApplicationImage{}; // Pointer to the application's image resource

bool g_isWindowExpanded{}; // Tracks whether the window is expanded or collapsed
NOTIFYICONDATA g_notifyIconData{}; // Data for the system tray icon

SIZE g_mainSizeWndCollapsed{ 7, 95 }; // Size of the window in collapsed state
SIZE g_mainSizeWndExpanded{ 29, 95 }; // Size of the window in expanded state

TCHAR g_mainAppPath[MAX_PATH]; // Path to the main application executable
TCHAR g_mainRegSubKey[] = _T("SOFTWARE\\Empurple\\TabTap"); // Registry subkey for application settings
TCHAR g_mainWndClassName[] = _T("TabTap"); // Class name for the main application window

TCHAR g_oskAppPathEX[MAX_PATH]; // Buffer to store the full path to the on-screen keyboard
TCHAR g_oskAppPath[] = _T("%WINDIR%\\System32\\osk.exe"); // Default path to the on-screen keyboard executable
TCHAR g_oskRegSubKey[] = _T("SOFTWARE\\Microsoft\\Osk"); // Registry subkey for on-screen keyboard settings
TCHAR g_oskWndClassName[] = _T("OSKMainClass"); // Class name for the on-screen keyboard window

bool isForAllUsers; // Indicates whether the application is installed for all users
PROCESS_INFORMATION g_processInfo; // Information about the launched process
STARTUPINFO g_startupInfo; // Startup information for the launched process
HMENU g_hTrayContextMenu; // Handle to the context menu for the system tray icon


static bool WriteRegistry(bool forAllUsers, const TCHAR* subKey, const TCHAR* valueName, DWORD value)
{
	HKEY hKey;
	HKEY hRootKey = forAllUsers ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;

	// Open or create the registry key
	if (RegOpenKeyEx(hRootKey, subKey, 0, KEY_SET_VALUE, &hKey) != ERROR_SUCCESS) {
		if (RegCreateKeyEx(hRootKey, subKey, 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, nullptr, &hKey, nullptr) != ERROR_SUCCESS) {
			MessageBox(NULL, _T("Error: Failed to open or create registry key."), _T("Error"), MB_OK | MB_ICONERROR);
			return false;
		}
	}
	// Set the registry value
	if (RegSetValueEx(hKey, valueName, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&value), sizeof(value)) == ERROR_SUCCESS) {
		RegCloseKey(hKey);
		return true; // Successfully wrote the value
	}
	RegCloseKey(hKey);
	MessageBox(NULL, _T("Error: Failed to write registry value."), _T("Error"), MB_OK | MB_ICONERROR);
	return false;
}

static bool ReadRegistry(bool forAllUsers, const TCHAR* subKey, const TCHAR* valueName, DWORD& outValue)
{
	HKEY hKey;
	HKEY hRootKey = forAllUsers ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
	DWORD dataSize = sizeof(DWORD);
	DWORD valueType{ REG_NONE };

	// Open the registry key
	if (RegOpenKeyEx(hRootKey, subKey, 0, KEY_QUERY_VALUE, &hKey) != ERROR_SUCCESS) {
		return false;
	}
	// Read the registry value
	if (RegQueryValueEx(hKey, valueName, NULL, &valueType, reinterpret_cast<LPBYTE>(&outValue), &dataSize) == ERROR_SUCCESS) {
		if (valueType == REG_DWORD) {
			RegCloseKey(hKey);
			return true; // Successfully read a DWORD value
		}
		else {
			MessageBox(NULL, _T("Error: Value is not REG_DWORD."), _T("Error"), MB_OK | MB_ICONERROR);
		}
	}
	RegCloseKey(hKey);
	return false;
}

static bool WriteRegistry(bool forAllUsers, const TCHAR* subKey, const TCHAR* valueName, const TCHAR* value)
{
	HKEY hKey;
	HKEY hRootKey = forAllUsers ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;

	// Open or create the registry key
	if (RegOpenKeyEx(hRootKey, subKey, 0, KEY_WRITE, &hKey) != ERROR_SUCCESS) {
		if (RegCreateKeyEx(hRootKey, subKey, 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, nullptr) != ERROR_SUCCESS) {
			MessageBox(NULL, _T("Error: Failed to open or create registry key."), _T("Error"), MB_OK | MB_ICONERROR);
			return false;
		}
	}
	// Set the value for autorun
	if (RegSetValueEx(hKey, g_mainWndClassName, 0, REG_SZ, (BYTE*)value, (_tcslen(value) + 1) * sizeof(TCHAR)) == ERROR_SUCCESS) {
		RegCloseKey(hKey);
		return true; // Successfully wrote the value
	}
	RegCloseKey(hKey);
	MessageBox(NULL, _T("Error: Failed to write registry value."), _T("Error"), MB_OK | MB_ICONERROR);
	return false;
}

static bool ReadRegistry(bool forAllUsers, const TCHAR* subKey, const TCHAR* valueName, TCHAR* outValue) {
	HKEY hKey;
	HKEY hRootKey = forAllUsers ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
	DWORD dataSize = MAX_PATH * sizeof(TCHAR); // Buffer size in bytes
	DWORD valueType{ REG_NONE };

	// Open the registry key
	if (RegOpenKeyEx(hRootKey, subKey, 0, KEY_READ, &hKey) != ERROR_SUCCESS) {
		return false; // Key doesn't exist or access denied
	}
	if (RegQueryValueEx(hKey, valueName, NULL, &valueType, reinterpret_cast<LPBYTE>(outValue), &dataSize) == ERROR_SUCCESS) {
		if (valueType == REG_SZ) {
			RegCloseKey(hKey);
			return true; // Successfully read a REG_SZ value
		}
		else {
			MessageBox(NULL, _T("Error: Value is not REG_SZ."), _T("Error"), MB_OK | MB_ICONERROR);
		}
	}
	RegCloseKey(hKey);
	return false;
}

void Cleanup()
{
	// Destroy context menu
	if (g_hTrayContextMenu) {
		DestroyMenu(g_hTrayContextMenu);
		g_hTrayContextMenu = NULL;
	}
	// Remove notify icon
	Shell_NotifyIcon(NIM_DELETE, &g_notifyIconData);
	// Free image resources
	delete g_pApplicationImage;
	//
	Gdiplus::GdiplusShutdown(g_gdiplusToken);
}

static void DrawImageOnLayeredWindow(HWND hwnd)
{
	SIZE sizeWnd = g_isWindowExpanded ? g_mainSizeWndExpanded : g_mainSizeWndCollapsed;

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
	int srcLeft = g_isWindowExpanded ? 0 : g_pApplicationImage->GetWidth() - g_mainSizeWndCollapsed.cx;
	int srcWidth = g_isWindowExpanded ? g_mainSizeWndExpanded.cx : g_mainSizeWndCollapsed.cx;

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

static void CreateTrayPopupMenu(HWND hWnd)
{
	POINT pt;
	GetCursorPos(&pt);
	HMENU hMenu = CreatePopupMenu();
	const TCHAR* subKey = isForAllUsers ? _T("Software\\Microsoft\\Windows\\CurrentVersion\\Run") : _T("Software\\Microsoft\\Windows\\CurrentVersion\\Run");


	// Check registry autostart value
	bool result = ReadRegistry(isForAllUsers, g_mainRegSubKey, g_mainWndClassName, NULL);

	/*
	bool result = ReadRegistry(mainKey, _T("Software\\Microsoft\\Windows\\CurrentVersion\\Run"), g_mainWndClassName, value);
	if ()

	 state2 = GetMenuState(hMenu, ID_TRAY_HIDEMINBUTTON, MF_BYCOMMAND);
	if (state2 & MF_CHECKED) {

	}
	UINT state1 = GetMenuState(hMenu, ID_TRAY_AUTOSTART, MF_BYCOMMAND);
	if (state1 & MF_CHECKED) {

	}
*/
	AppendMenu(hMenu, MF_SEPARATOR, 1, NULL);
	AppendMenu(hMenu, MF_STRING | (MF_CHECKED), IDM_TRAY_HIDEMINBUTTON, _T("Hide `minimize` button"));
	AppendMenu(hMenu, MF_STRING | (result & MF_CHECKED), IDM_TRAY_AUTOSTART, _T("Autostart"));
	AppendMenu(hMenu, MF_SEPARATOR, 1, NULL);
	AppendMenu(hMenu, MF_STRING, IDM_TRAY_EXIT, _T("Exit"));
	// Show the menu
	SetForegroundWindow(hWnd);
	TrackPopupMenu(hMenu, TPM_LEFTBUTTON, pt.x, pt.y, 0, hWnd, NULL);

	//int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, hWnd, NULL);
/*
	if (cmd == 1) {
		DestroyWindow(hWnd);  // Close the window
	}
*/
	//DestroyMenu(hMenu);
}

LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	static bool tracking = false;
	static bool dragging = false;
	static bool isOskRunning = false;
	static POINT dragStart{};
	static RECT mainWindowRect;
	static DWORD oskWindowHeight;

	//	Don't block the loop while waiting for OSK 
	if (isOskRunning) {
		g_hWndOsk = FindWindow(g_oskWndClassName, NULL);
		if (g_hWndOsk) {
			//ShowWindowAsync(hWndOSK, SW_SHOWNA);
			EnableMenuItem(GetSystemMenu(g_hWndOsk, FALSE), SC_MINIMIZE, MF_GRAYED);
			// Change EXSTYLE
			LONG style = GetWindowLong(g_hWndOsk, GWL_EXSTYLE);
			style &= ~WS_EX_APPWINDOW; // Hide from taskbar
			SetWindowLong(g_hWndOsk, GWL_EXSTYLE, style); // Apply new style
			// Change STYLE
			//style = GetWindowLong(g_hWndOsk, GWL_STYLE);
			//style &= ~WS_MINIMIZEBOX; // Remove minimize button
			//SetWindowLong(g_hWndOsk, GWL_STYLE, style); // Apply new style

			isOskRunning = false;
		}
	}

	switch (uMsg)
	{
	case WM_SETCURSOR:
	{
		SetCursor(LoadCursor(NULL, IDC_ARROW));
		break;
	}
	case WM_RBUTTONDOWN:
	{
		SetCapture(hWnd);
		dragging = true;
		dragStart = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
		ClientToScreen(hWnd, &dragStart);
		GetWindowRect(hWnd, &mainWindowRect);
		break;
	}
	case WM_MOUSEACTIVATE:
	{
		return MA_NOACTIVATE; // Prevent window activation on mouse click
	}
	case WM_MOUSEMOVE:
	{
		if (dragging)
		{
			POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
			ClientToScreen(hWnd, &pt);
			int newTop = mainWindowRect.top + pt.y - dragStart.y;
			int screenHeight = GetSystemMetrics(SM_CYSCREEN);
			newTop = max(0, min(newTop, screenHeight - g_mainSizeWndExpanded.cy));
			int currentWidth = g_isWindowExpanded ? g_mainSizeWndExpanded.cx : g_mainSizeWndCollapsed.cx;
			SetWindowPos(hWnd, NULL, 0, newTop, currentWidth, g_mainSizeWndExpanded.cy, SWP_NOZORDER | SWP_NOACTIVATE);
			DrawImageOnLayeredWindow(hWnd);
		}
		else if (!g_isWindowExpanded)
		{
			GetWindowRect(hWnd, &mainWindowRect);
			SetWindowPos(hWnd, NULL, 0, mainWindowRect.top, g_mainSizeWndExpanded.cx, g_mainSizeWndCollapsed.cy, SWP_NOMOVE | SWP_NOZORDER);
			g_isWindowExpanded = true;
			DrawImageOnLayeredWindow(hWnd);
		}

		if (!tracking)
		{
			TRACKMOUSEEVENT tme = { sizeof(TRACKMOUSEEVENT), TME_LEAVE, hWnd, 0 };
			TrackMouseEvent(&tme);
			tracking = true;
		}
		break;
	}
	case WM_RBUTTONUP:
	{
		dragging = false;
		ReleaseCapture();
		break;
	}
	case WM_LBUTTONDOWN:
	{
		break;
	}
	case WM_LBUTTONUP:
	{
		g_hWndOsk = FindWindow(g_oskWndClassName, NULL);

		if (g_hWndOsk) {
			if (IsIconic(g_hWndOsk)) {
				// Restore OSK
				ShowWindow(g_hWndOsk, SW_RESTORE);
			}
			else if (IsWindowVisible(g_hWndOsk)) {
				// Hide OSK
				ShowWindow(g_hWndOsk, SW_HIDE);
			}
			else {
				// Update OSK CY
				GetWindowRect(hWnd, &mainWindowRect);
				ReadRegistry(false, g_oskRegSubKey, _T("WindowHeight"), oskWindowHeight);
				WriteRegistry(false, g_oskRegSubKey, _T("WindowTop"), mainWindowRect.top - (oskWindowHeight - g_mainSizeWndCollapsed.cy) / 2);
				// Show OSK
				ShowWindow(g_hWndOsk, SW_SHOWNA);
			}
		}
		else {
			// Update OSK CY
			GetWindowRect(hWnd, &mainWindowRect);
			ReadRegistry(false, g_oskRegSubKey, _T("WindowHeight"), oskWindowHeight);
			WriteRegistry(false, g_oskRegSubKey, _T("WindowTop"), mainWindowRect.top - (oskWindowHeight - g_mainSizeWndCollapsed.cy) / 2);
			// Run OSK
			if (!CreateProcess(g_oskAppPathEX, NULL, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &g_startupInfo, &g_processInfo)) {
				MessageBox(NULL, _T("Failed to start osk.exe!"), _T("Error"), MB_OK | MB_ICONERROR);
			}
			CloseHandle(g_processInfo.hThread);
			CloseHandle(g_processInfo.hProcess);
			isOskRunning = true;
		}
		break;
	}
	case WM_MOUSELEAVE:
	{
		tracking = false;
		if (!dragging && g_isWindowExpanded)
		{
			GetWindowRect(hWnd, &mainWindowRect);
			SetWindowPos(hWnd, NULL, 0, mainWindowRect.top, g_mainSizeWndCollapsed.cx, g_mainSizeWndCollapsed.cy, SWP_NOMOVE | SWP_NOZORDER);
			g_isWindowExpanded = false;
			DrawImageOnLayeredWindow(hWnd);
		}
		break;
	}
	case WM_TRAYICON:
	{
		if (lParam == WM_LBUTTONUP)
		{
			GetWindowRect(hWnd, &mainWindowRect);
			SendMessage(hWnd, WM_LBUTTONUP, 0, MAKELPARAM(mainWindowRect.left, mainWindowRect.top));
		}
		else if (lParam == WM_RBUTTONUP)
		{
			POINT pt;
			GetCursorPos(&pt);
			SetForegroundWindow(hWnd);
			TrackPopupMenu(g_hTrayContextMenu, TPM_LEFTBUTTON, pt.x, pt.y, 0, hWnd, NULL);
		}
		break;
	}
	case WM_COMMAND:
	{
		if (LOWORD(wParam) == IDM_TRAY_HIDEMINBUTTON) {
			SendMessage(hWnd, WM_CLOSE, 0, 0);
		}
		if (LOWORD(wParam) == IDM_TRAY_AUTOSTART) {
			MENUITEMINFO mii = { sizeof(MENUITEMINFO) };
			mii.fMask = MIIM_STATE;
			//GetMenuItemInfo(hWnd, IDM_TRAY_AUTOSTART, FALSE, &mii);

			if (mii.fState & MFS_CHECKED) {
				mii.fState = MFS_UNCHECKED;
			}
			else {
				mii.fState = MFS_CHECKED;
			}
			//SetMenuItemInfo(hMenu, IDM_TRAY_AUTOSTART, FALSE, &mii);
			//BOOL checked = SendMessage((HWND)lParam, BM_GETCHECK, 0, 0);
			//if (checked) MessageBox(NULL, _T("Failed to start osk.exe!"), _T("Error"), MB_OK | MB_ICONERROR);
		}
		else if (LOWORD(wParam) == IDM_TRAY_EXIT) {
			SendMessage(hWnd, WM_CLOSE, 0, 0);
		}
		break;
	}
	case WM_CREATE:
	{
		if (g_hWndOsk = FindWindow(g_oskWndClassName, NULL)) {
			SendMessage(g_hWndOsk, WM_CLOSE, 0, 0);
		}
		CreateTrayPopupMenu(g_hWndMain);
		break;
	}
	case WM_DESTROY:
	{
		// Close OSK
		g_hWndOsk = FindWindow(g_oskWndClassName, NULL);
		if(g_hWndOsk) {
			SendMessage(g_hWndOsk, WM_CLOSE, 0, 0);
		}

		GetWindowRect(hWnd, &mainWindowRect);
		ReadRegistry(false, g_oskRegSubKey, _T("WindowHeight"), oskWindowHeight);
		WriteRegistry(false, g_oskRegSubKey, _T("WindowTop"), mainWindowRect.top - (oskWindowHeight - g_mainSizeWndCollapsed.cy) / 2);

		Cleanup();
		PostQuitMessage(0);
		return 0;
	}
	}
	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

HICON LoadIconFromExe(const TCHAR* exePath, int iconIndex) {
	return ExtractIcon(NULL, exePath, iconIndex);
}

//
void SetupOSK()
{
	ZeroMemory(&g_startupInfo, sizeof(g_startupInfo));
	ZeroMemory(&g_processInfo, sizeof(g_processInfo));
	g_startupInfo.cb = sizeof(g_startupInfo);
	g_startupInfo.dwFlags = STARTF_USESHOWWINDOW;
	g_startupInfo.wShowWindow = SW_SHOWNA;
	//g_startupInfo.wShowWindow = SW_HIDE;	// Cause flickering
}

// Setup System Tray Icon
void SetupTrayIcon(HWND hwnd)
{
	ZeroMemory(&g_notifyIconData, sizeof(g_notifyIconData));
	g_notifyIconData.cbSize = sizeof(NOTIFYICONDATA);
	g_notifyIconData.hWnd = hwnd;
	g_notifyIconData.uID = 1;  // Unique ID for tray icon
	g_notifyIconData.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
	g_notifyIconData.uCallbackMessage = WM_TRAYICON;  // Custom message for mouse events
	g_notifyIconData.hIcon = LoadIconFromExe(g_oskAppPathEX, 0); // First icon in the EXE
	lstrcpy(g_notifyIconData.szTip, _T("TabTap"));
	Shell_NotifyIcon(NIM_ADD, &g_notifyIconData);
}

// Load Image Resource
bool LoadImageResource()
{
	g_pApplicationImage = new Gdiplus::Image(_T("c:\\background.png"));
	if (g_pApplicationImage->GetLastStatus() != Gdiplus::Ok)
	{
		MessageBox(NULL, _T("Failed to load PNG image."), _T("Error"), MB_OK | MB_ICONERROR);
		delete g_pApplicationImage;
		g_pApplicationImage = nullptr;
		return false;
	}
	return true;
}

// Create Layered Window
HWND CreateLayeredWindow(HINSTANCE hInstance, const wchar_t* className)
{
	// Read initial CY value
	DWORD value;
	int screenHeight = GetSystemMetrics(SM_CYSCREEN);

	bool result = ReadRegistry(false, g_oskRegSubKey, _T("WindowTop"), value);
	value = result ? min(value, screenHeight - g_mainSizeWndCollapsed.cy) : 0;

	return CreateWindowEx(
		WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
		className,
		_T("TabTap"),
		WS_POPUP,
		0,
		value,
		g_mainSizeWndCollapsed.cx,
		g_mainSizeWndCollapsed.cy,
		(HWND)NULL, (HMENU)NULL, hInstance, (LPVOID)NULL
	);
}

// Register Window Class
ATOM RegisterWindowClass(HINSTANCE hInstance, const wchar_t* className)
{
	WNDCLASS wc{};
	wc.lpfnWndProc = WindowProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = className;
	wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
	return RegisterClass(&wc);
}

// Initialize GDI+
void InitializeGDIPlus()
{
	Gdiplus::GdiplusStartupInput gdiInput;
	GdiplusStartup(&g_gdiplusToken, &gdiInput, NULL);
}


// Main Entry Point
int WINAPI WinMain(
	_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPSTR lpCmdLine,
	_In_ int nCmdShow)
{
	if (!ExpandEnvironmentStrings(g_oskAppPath, g_oskAppPathEX, MAX_PATH)) {
		MessageBox(NULL, _T("Failed to expand osk.exe path!"), _T("Error"), MB_OK | MB_ICONERROR);
		wprintf(_T("Cannot expand path (%d)\n"), GetLastError());
		return 1;
	}

	if (!GetModuleFileName(NULL, g_mainAppPath, MAX_PATH)) {
		MessageBox(NULL, _T("Failed to get executable path!"), _T("Error"), MB_OK | MB_ICONERROR);
		wprintf(_T("Cannot get path (%d)\n"), GetLastError());
		return 1;
	}

	InitializeGDIPlus();

	if (!RegisterWindowClass(hInstance, g_mainWndClassName)) {
		return 1;
	}

	if (!(g_hWndMain = CreateLayeredWindow(hInstance, g_mainWndClassName)) || !LoadImageResource()) {
		return 1;
	}

	SetupOSK();
	SetupTrayIcon(g_hWndMain);
	ShowWindow(g_hWndMain, nCmdShow);
	DrawImageOnLayeredWindow(g_hWndMain);

	MSG msg;
	while (GetMessage(&msg, nullptr, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return 0;
}





/*
// Define your hook procedure
LRESULT CALLBACK MyCallWndProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	if (nCode >= 0) {
		CWPSTRUCT* pCwp = (CWPSTRUCT*)lParam;
		// Process the message as needed, e.g.:
		// if(pCwp->message == WM_KEYDOWN) { ... }
	}
	return CallNextHookEx(NULL, nCode, wParam, lParam);
}
//
*/

/*
	HHOOK hHook = SetWindowsHookEx(WH_CALLWNDPROC, MyCallWndProc, NULL, GetCurrentThreadId());
	if (hHook == NULL) {
		// Handle error appropriately
		return -1;
	}
*/
