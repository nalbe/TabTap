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

//#define WM_UPDATE_WNDTOP		(WM_USER + 1)
#define WM_TRAYICON				(WM_APP + 1)
#define IDM_TRAY_HIDEMINBUTTON	1004
#define IDM_TRAY_AUTOSTART		1003
#define IDM_TRAY_EXIT			1002
#define IDM_TRAY_SEPARATOR		1001


HWND g_hWndMain; // Handle to the main application window
HWND g_hWndOsk; // Handle to the on-screen keyboard window
ULONG_PTR g_gdiplusToken; // Token for GDI+ initialization
Gdiplus::Image* g_pApplicationImage{}; // Pointer to the application's image resource

bool g_isWindowExpanded{}; // Tracks whether the window is expanded or collapsed
NOTIFYICONDATA g_notifyIconData{}; // Data for the system tray icon

SIZE g_wndCollapsedSize{ 7, 95 }; // Size of the window in collapsed state
SIZE g_wndExpandedSize{ 29, 95 }; // Size of the window in expanded state

TCHAR g_wndClassName[] = _T("TabTapMainClass"); // Class name for the main application window
//TCHAR g_appName[] = _T("TabTap"); // Application name
TCHAR g_appPath[MAX_PATH]; // Path to the main application executable
TCHAR g_appRegSubKey[] = _T("SOFTWARE\\Empurple\\TabTap"); // Registry subkey for application settings
TCHAR g_runRegSubKey[] = _T("Software\\Microsoft\\Windows\\CurrentVersion\\Run"); // Autorun registry subkey

TCHAR g_oskAppPathEX[MAX_PATH]; // Buffer to store the full path to the on-screen keyboard
TCHAR g_oskAppPath[] = _T("%WINDIR%\\System32\\osk.exe"); // Default path to the on-screen keyboard executable
TCHAR g_oskRegSubKey[] = _T("SOFTWARE\\Microsoft\\Osk"); // Registry subkey for on-screen keyboard settings
TCHAR g_oskWndClassName[] = _T("OSKMainClass"); // Class name for the on-screen keyboard window

bool isForAllUsers; // Indicates whether the application is installed for all users
PROCESS_INFORMATION g_processInfo; // Information about the launched process
STARTUPINFO g_startupInfo; // Startup information for the launched process
HMENU g_hTrayContextMenu; // Handle to the context menu for the system tray icon


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
	SIZE sizeWnd = g_isWindowExpanded ? g_wndExpandedSize : g_wndCollapsedSize;

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
	int srcLeft = g_isWindowExpanded ? 0 : g_pApplicationImage->GetWidth() - g_wndCollapsedSize.cx;
	int srcWidth = g_isWindowExpanded ? g_wndExpandedSize.cx : g_wndCollapsedSize.cx;

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
	g_hTrayContextMenu = CreatePopupMenu();
	// Check IDM_TRAY_HIDEMINBUTTON state
	DWORD hideMinBtData{};
	bool hideMinBtResult = ReadRegistry(isForAllUsers, g_appRegSubKey, _T("hideMinButton"), &hideMinBtData) and hideMinBtData;
	// Check ID_TRAY_AUTOSTART state
	TCHAR autostartData[MAX_PATH];
	bool autostartResult = ReadRegistry(isForAllUsers, g_runRegSubKey, _T("TabTap"), autostartData);

	AppendMenu(g_hTrayContextMenu, MF_SEPARATOR, IDM_TRAY_SEPARATOR, NULL);
	AppendMenu(g_hTrayContextMenu, MF_STRING | (MF_CHECKED * hideMinBtResult), IDM_TRAY_HIDEMINBUTTON, _T("OSK minimize button"));
	AppendMenu(g_hTrayContextMenu, MF_STRING | (MF_CHECKED * autostartResult), IDM_TRAY_AUTOSTART, _T("Autostart"));
	AppendMenu(g_hTrayContextMenu, MF_SEPARATOR, IDM_TRAY_SEPARATOR, NULL);
	AppendMenu(g_hTrayContextMenu, MF_STRING, IDM_TRAY_EXIT, _T("Exit"));
	AppendMenu(g_hTrayContextMenu, MF_SEPARATOR, IDM_TRAY_SEPARATOR, NULL);
}

void UpdateOSKPosition(HWND hWnd, RECT& mainWindowRect, DWORD& oskWindowHeight)
{
	GetWindowRect(hWnd, &mainWindowRect);
	ReadRegistry(isForAllUsers, g_oskRegSubKey, _T("WindowHeight"), &oskWindowHeight);
	LONG oskNewTop = mainWindowRect.top - (((LONG)oskWindowHeight - g_wndCollapsedSize.cy) >> 1);
	int screenHeight = GetSystemMetrics(SM_CYSCREEN);
	if (screenHeight < oskNewTop + oskWindowHeight) {
		oskNewTop = screenHeight - oskWindowHeight;
	}
	else if (oskNewTop < 0) {
		oskNewTop = 0;
	}
	WriteRegistry(isForAllUsers, g_oskRegSubKey, _T("WindowTop"), &oskNewTop, REG_DWORD);
}

void HideFromTaskbar(bool value)
{
	LONG_PTR style = GetWindowLongPtr(g_hWndOsk, GWL_EXSTYLE);
	style = value ? (style & ~WS_EX_APPWINDOW) : (style | WS_EX_APPWINDOW);
	SetWindowLong(g_hWndOsk, GWL_EXSTYLE, style);
}

void HideMinButton(bool value)
{
	LONG_PTR style = GetWindowLongPtr(g_hWndOsk, GWL_STYLE);
	style = value ? (style & ~WS_MINIMIZEBOX) : (style | WS_MINIMIZEBOX);
	SetWindowLong(g_hWndOsk, GWL_STYLE, style);
}

LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	static bool tracking = false;
	static bool dragging = false;
	static bool isOskLoading = false;
	static POINT dragStart{};
	static RECT mainWindowRect;
	static DWORD oskWindowHeight;

	//	Don't block the loop while waiting for OSK 
	if (isOskLoading) {
		g_hWndOsk = FindWindow(g_oskWndClassName, NULL);
		if (g_hWndOsk) {
			// Setup OSK
			HideFromTaskbar(true);
			HideMinButton(true);
			isOskLoading = false;
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
			newTop = max(0, min(newTop, screenHeight - g_wndExpandedSize.cy));
			int currentWidth = g_isWindowExpanded ? g_wndExpandedSize.cx : g_wndCollapsedSize.cx;
			SetWindowPos(hWnd, NULL, 0, newTop, currentWidth, g_wndExpandedSize.cy, SWP_NOZORDER | SWP_NOACTIVATE);
			DrawImageOnLayeredWindow(hWnd);
		}
		else if (!g_isWindowExpanded)
		{
			GetWindowRect(hWnd, &mainWindowRect);
			SetWindowPos(hWnd, NULL, 0, mainWindowRect.top, g_wndExpandedSize.cx, g_wndCollapsedSize.cy, SWP_NOMOVE | SWP_NOZORDER);
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
				ShowWindowAsync(g_hWndOsk, SW_RESTORE); // Restore OSK
			}
			else if (IsWindowVisible(g_hWndOsk)) {
				ShowWindowAsync(g_hWndOsk, SW_HIDE); // Hide OSK
			}
			else {
				UpdateOSKPosition(hWnd, mainWindowRect, oskWindowHeight);
				ShowWindowAsync(g_hWndOsk, SW_SHOWNA); // Show OSK
			}
		}
		else {
			UpdateOSKPosition(hWnd, mainWindowRect, oskWindowHeight);
			if (!CreateProcess(g_oskAppPathEX, NULL, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &g_startupInfo, &g_processInfo)) {
				MessageBox(NULL, _T("Failed to start osk.exe!"), _T("Error"), MB_OK | MB_ICONERROR); // Run OSK
			}
			CloseHandle(g_processInfo.hThread);
			CloseHandle(g_processInfo.hProcess);
			isOskLoading = true;
		}
		break;
	}
	case WM_MOUSELEAVE:
	{
		tracking = false;
		if (!dragging && g_isWindowExpanded) {
			GetWindowRect(hWnd, &mainWindowRect);
			SetWindowPos(hWnd, NULL, 0, mainWindowRect.top, g_wndCollapsedSize.cx, g_wndCollapsedSize.cy, SWP_NOMOVE | SWP_NOZORDER);
			g_isWindowExpanded = false;
			DrawImageOnLayeredWindow(hWnd);
		}
		break;
	}
	case WM_TRAYICON:
	{
		if (lParam == WM_LBUTTONUP) {
			GetWindowRect(hWnd, &mainWindowRect);
			SendMessage(hWnd, WM_LBUTTONUP, 0, MAKELPARAM(mainWindowRect.left, mainWindowRect.top));
		}
		else if (lParam == WM_RBUTTONUP) {
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
			UINT uState = GetMenuState(g_hTrayContextMenu, IDM_TRAY_HIDEMINBUTTON, MF_BYCOMMAND);
			if ((uState & MF_CHECKED) == MF_CHECKED) {

			}
			else {

			}
			CheckMenuItem(g_hTrayContextMenu, IDM_TRAY_HIDEMINBUTTON, MF_BYCOMMAND | ((uState & MF_CHECKED) ? MF_UNCHECKED : MF_CHECKED));
		}
		else if (LOWORD(wParam) == IDM_TRAY_AUTOSTART) {
			UINT uState = GetMenuState(g_hTrayContextMenu, IDM_TRAY_AUTOSTART, MF_BYCOMMAND);
			if ((uState & MF_CHECKED) == MF_CHECKED) {
				RemoveRegistry(isForAllUsers, g_runRegSubKey, _T("TabTap"));
			}
			else {
				WriteRegistry(isForAllUsers, g_runRegSubKey, _T("TabTap"), g_appPath, REG_SZ);
			}
			CheckMenuItem(g_hTrayContextMenu, IDM_TRAY_AUTOSTART, MF_BYCOMMAND | ((uState & MF_CHECKED) ? MF_UNCHECKED : MF_CHECKED));
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
		CreateTrayPopupMenu(hWnd);
		break;
	}
	case WM_DESTROY:
	{	
		g_hWndOsk = FindWindow(g_oskWndClassName, NULL);
		if(g_hWndOsk) {
			SendMessage(g_hWndOsk, WM_CLOSE, 0, 0); // Close OSK
		}
		UpdateOSKPosition(hWnd, mainWindowRect, oskWindowHeight);
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
	// Read start coords from registry
	DWORD regData{};
	int screenHeight = GetSystemMetrics(SM_CYSCREEN);
	bool result = ReadRegistry(false, g_oskRegSubKey, _T("WindowTop"), &regData);
	regData = result ? min(regData, screenHeight - g_wndCollapsedSize.cy) : 0;

	return CreateWindowEx(
		WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
		className,
		_T("TabTap"),
		WS_POPUP,
		0,
		regData,
		g_wndCollapsedSize.cx,
		g_wndCollapsedSize.cy,
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

	DWORD result = GetModuleFileName(NULL, g_appPath + 1, MAX_PATH);
	if (!result) {
		MessageBox(NULL, _T("Failed to get executable path!"), _T("Error"), MB_OK | MB_ICONERROR);
		wprintf(_T("Cannot get path (%d)\n"), GetLastError());
		return 1;
	}
	else { // Format string with quotes
		*g_appPath = *(g_appPath + result + 1) = _T('"');
	}

	InitializeGDIPlus();

	if (!RegisterWindowClass(hInstance, g_wndClassName)) {
		return 1;
	}

	if (!(g_hWndMain = CreateLayeredWindow(hInstance, g_wndClassName)) || !LoadImageResource()) {
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


/*
void ToolWindow(bool value)
{
	LONG_PTR style = GetWindowLongPtr(g_hWndOsk, GWL_EXSTYLE);
	style = value ? (style | WS_EX_TOOLWINDOW) : (style & ~WS_EX_TOOLWINDOW);
	SetWindowLong(g_hWndOsk, GWL_EXSTYLE, style);
}
*/

/*
void HideSysMenu(bool value)
{
	LONG_PTR style = GetWindowLongPtr(g_hWndOsk, GWL_STYLE);
	style = value ? (style & ~WS_SYSMENU) : (style | WS_SYSMENU);
	SetWindowLong(g_hWndOsk, GWL_STYLE, style);
}
*/

/*
void DisableCloseButton(bool value)
{
	HMENU hMenu;
	if (hMenu = GetSystemMenu(g_hWndOsk, FALSE)) {
		if (value) {
			DeleteMenu(hMenu, SC_CLOSE, MF_BYCOMMAND);
		}
		else {
			AppendMenu(hMenu, MF_STRING, SC_CLOSE, _T("Close"));
		}
	}
}
*/

/*
void DisableMinButton(bool value)
{
	HMENU hMenu;
	if (hMenu = GetSystemMenu(g_hWndOsk, FALSE)) {
		if (value) {
			DeleteMenu(hMenu, SC_MINIMIZE, MF_BYCOMMAND);
		}
		else {
			AppendMenu(hMenu, MF_STRING, SC_MINIMIZE, _T("Minimize"));
		}
	}
}
*/
