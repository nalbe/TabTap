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

#define WM_TRAYICON				(WM_APP + 1)
#define IDM_TRAY_AUTOSTART		1003
#define IDM_TRAY_EXIT			1002
#define IDM_TRAY_SEPARATOR		1001


HWND g_hWndMain{}; // Handle to the main application window
HWND g_hWndOsk{}; // Handle to the on-screen keyboard window
ULONG_PTR g_gdiplusToken; // Token for GDI+ initialization
Gdiplus::Image* g_pApplicationImage{}; // Pointer to the application's image resource

NOTIFYICONDATA g_notifyIconData{}; // Data for the system tray icon

SIZE g_wndCollapsedSize{ 7, 95 }; // Size of the window in collapsed state
SIZE g_wndExpandedSize{ 29, 95 }; // Size of the window in expanded state

TCHAR g_applicationPath[MAX_PATH]; // Buffer to store the path to the main application executable
const TCHAR g_wndClassName[] = _T("TabTapMainClass"); // Class name for the main application window
const TCHAR g_applicationRegKey[] = _T("SOFTWARE\\Empurple\\TabTap"); // Registry subkey for application settings
const TCHAR g_autoStartRegKey[] = _T("Software\\Microsoft\\Windows\\CurrentVersion\\Run"); // Autorun registry subkey

TCHAR g_oskExecutablePathEX[MAX_PATH]; // Buffer to store the full path to the on-screen keyboard
const TCHAR g_oskExecutablePath[] = _T("%WINDIR%\\System32\\osk.exe"); // Default path to the on-screen keyboard executable
const TCHAR g_oskRegKey[] = _T("SOFTWARE\\Microsoft\\Osk"); // Registry subkey for on-screen keyboard settings
const TCHAR g_oskWndClassName[] = _T("OSKMainClass"); // Class name for the on-screen keyboard window

HMENU g_hTrayContextMenu; // Handle to the context menu for the system tray icon
bool isForAllUsers{}; // Indicates whether the application is installed for all users


// Function pointer types for the exported functions.
typedef BOOL(*CloseOSKFunc)();
typedef BOOL(*LaunchOSKFunc)();



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

void DrawImageOnLayeredWindow(HWND hwnd, bool isWindowExpanded)
{
	SIZE sizeWnd = isWindowExpanded ? g_wndExpandedSize : g_wndCollapsedSize;

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
	int srcLeft = isWindowExpanded ? 0 : g_pApplicationImage->GetWidth() - g_wndCollapsedSize.cx;
	int srcWidth = isWindowExpanded ? g_wndExpandedSize.cx : g_wndCollapsedSize.cx;

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
	bool autostartResult = ReadRegistry(isForAllUsers, g_autoStartRegKey, _T("TabTap"), autostartData);

	AppendMenu(g_hTrayContextMenu, MF_SEPARATOR, IDM_TRAY_SEPARATOR, NULL);
	AppendMenu(g_hTrayContextMenu, MF_STRING | (autostartResult ? MF_CHECKED : 0), IDM_TRAY_AUTOSTART, _T("Autostart"));
	AppendMenu(g_hTrayContextMenu, MF_SEPARATOR, IDM_TRAY_SEPARATOR, NULL);
	AppendMenu(g_hTrayContextMenu, MF_STRING, IDM_TRAY_EXIT, _T("Exit"));
	AppendMenu(g_hTrayContextMenu, MF_SEPARATOR, IDM_TRAY_SEPARATOR, NULL);
}

void UpdateOSKPosition()
{
	DWORD oskWindowHeight;
	RECT mainWindowRect;
	LONG oskNewTop;
	ReadRegistry(isForAllUsers, g_oskRegKey, _T("WindowHeight"), &oskWindowHeight);
	GetWindowRect(g_hWndMain, &mainWindowRect);
	oskNewTop = mainWindowRect.top - ((LONG)oskWindowHeight - g_wndCollapsedSize.cy) / 2;
	oskNewTop = max(0L, min(GetSystemMetrics(SM_CYSCREEN) - (int)oskWindowHeight, oskNewTop));
	WriteRegistry(isForAllUsers, g_oskRegKey, _T("WindowTop"), &oskNewTop, REG_DWORD);
}


LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	static bool isMouseTracking{};
	static bool isDragging{};
	static bool isWindowExpanded{}; // Tracks whether the window is expanded or collapsed
	static POINT dragStartPoint{};
	static RECT mainWindowRect;

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
		isDragging = true;
		dragStartPoint = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
		ClientToScreen(hWnd, &dragStartPoint);
		GetWindowRect(hWnd, &mainWindowRect);
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
			int newWindowTop = mainWindowRect.top + cursorPosition.y - dragStartPoint.y;
			int screenHeight = GetSystemMetrics(SM_CYSCREEN);
			newWindowTop = max(0, min(newWindowTop, screenHeight - g_wndExpandedSize.cy));
			int currentWidth = isWindowExpanded ? g_wndExpandedSize.cx : g_wndCollapsedSize.cx;
			SetWindowPos(hWnd, NULL, 0, newWindowTop, currentWidth, g_wndExpandedSize.cy, SWP_NOZORDER | SWP_NOACTIVATE);
			DrawImageOnLayeredWindow(hWnd, isWindowExpanded);
		}
		else if (!isWindowExpanded)
		{
			GetWindowRect(hWnd, &mainWindowRect);
			SetWindowPos(hWnd, NULL, 0, mainWindowRect.top, g_wndExpandedSize.cx, g_wndCollapsedSize.cy, SWP_NOMOVE | SWP_NOZORDER);
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
			GetWindowRect(hWnd, &mainWindowRect);
			SetWindowPos(hWnd, HWND_TOPMOST, 0, mainWindowRect.top, g_wndCollapsedSize.cx, g_wndCollapsedSize.cy, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
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
				RemoveRegistry(isForAllUsers, g_autoStartRegKey, _T("TabTap"));
			}
			else {
				WriteRegistry(isForAllUsers, g_autoStartRegKey, _T("TabTap"), g_applicationPath, REG_SZ);
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
		break;
	}
	case WM_DESTROY:
	{
		SendMessage(g_hWndOsk, WM_CLOSE, 0, 0); // Close OSK
		UpdateOSKPosition();
		Cleanup();
		PostQuitMessage(0);
		return 0;
	}
	}
	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}


HICON LoadIconFromExe(const TCHAR* exePath, int iconIndex)
{
	return ExtractIcon(NULL, exePath, iconIndex);
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
	g_notifyIconData.hIcon = LoadIconFromExe(g_oskExecutablePathEX, 0); // First icon in the EXE
	lstrcpy(g_notifyIconData.szTip, _T("TabTap"));
	Shell_NotifyIcon(NIM_ADD, &g_notifyIconData);
}

// Load Image Resource
bool LoadImageResource()
{
	g_pApplicationImage = new Gdiplus::Image(_T("TabTap.png"));
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
	bool result = ReadRegistry(isForAllUsers, g_oskRegKey, _T("WindowTop"), &regData);
	regData = result ? min((LONG)regData, screenHeight - g_wndCollapsedSize.cy) : 0;

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
	if (g_hWndMain = FindWindow(g_wndClassName, NULL)) {
		SendMessage(g_hWndMain, WM_LBUTTONUP, 0, 0);
		return 0;
	}


	if (!ExpandEnvironmentStrings(g_oskExecutablePath, g_oskExecutablePathEX, MAX_PATH)) {
		MessageBox(NULL, _T("Failed to expand osk.exe path!"), _T("Error"), MB_OK | MB_ICONERROR);
		wprintf(_T("Cannot expand path (%d)\n"), GetLastError());
		return 1;
	}

	DWORD result = GetModuleFileName(NULL, g_applicationPath + 1, MAX_PATH);
	if (!result) {
		MessageBox(NULL, _T("Failed to get executable path!"), _T("Error"), MB_OK | MB_ICONERROR);
		wprintf(_T("Cannot get path (%d)\n"), GetLastError());
		return 1;
	}
	else { // Format string with quotes
		*g_applicationPath = *(g_applicationPath + result + 1) = _T('"');
	}

	InitializeGDIPlus();

	if (!RegisterWindowClass(hInstance, g_wndClassName)) {
		return 1;
	}

	if (!(g_hWndMain = CreateLayeredWindow(hInstance, g_wndClassName)) || !LoadImageResource()) {
		return 1;
	}

	SetupTrayIcon(g_hWndMain);
	ShowWindow(g_hWndMain, nCmdShow);
	DrawImageOnLayeredWindow(g_hWndMain, false);


// Hook load begin
	HMODULE hDll;
	if (!(hDll = LoadLibrary(_T("TabTap.dll")))) {
		wprintf(_T("Failed to load the hook DLL (%d)\n"), GetLastError());
		return 1;
	}

	CloseOSKFunc CloseOSK = (CloseOSKFunc)GetProcAddress(hDll, "CloseOSK");
	LaunchOSKFunc LaunchOSK = (LaunchOSKFunc)GetProcAddress(hDll, "LaunchOSK");

	if (!CloseOSK or !LaunchOSK) {
		wprintf(_T("Failed to locate OSKLauncher function.\n"));
		FreeLibrary(hDll);
		return 1;
	}
	if (!LaunchOSK()) {
		wprintf(_T("OSK loading failed.\n"));
		return 1;
	}
// Hook load end

	//Wait until osk loaded.
	const int sleepTime{ 20 };
	int maxWaitTime{ 3000 };
	int cycleCnt{};
	while (!(g_hWndOsk = FindWindow(g_oskWndClassName, NULL)))
	{
		if (cycleCnt * sleepTime >= maxWaitTime) {
			MessageBox(NULL, _T("Failed to find osk window."), _T("Error"), MB_OK | MB_ICONERROR);
			return 1;
		}
		Sleep(sleepTime);
		++cycleCnt;
	}


	MSG msg;
	while (GetMessage(&msg, nullptr, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}


// Hook unload begin
	if (!CloseOSK()) {
		wprintf(_T("OSK unload failed.\n"));
		return 1;
	}
// Hook unload end

	return 0;
}




