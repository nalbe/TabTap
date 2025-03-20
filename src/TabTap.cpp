#pragma once
#include <windows.h>
#include <tchar.h>
#include <gdiplus.h>
#include <shellapi.h>
#include <Shlwapi.h>  // For PathRemoveFileSpec
#include <PathCch.h> // For PathCchCombine
#include "Registry.h"
#include <Sddl.h> // For security functions
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "Msimg32.lib")
#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "Pathcch.lib")


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

#define IDM_TRAY_DOCKMODE            (WM_APP + 4)
#define IDM_TRAY_AUTOSTART           (WM_APP + 3)
#define IDM_TRAY_EXIT                (WM_APP + 2)
#define IDM_TRAY_SEPARATOR           (WM_APP + 1)

#define WM_TRAYICON                  (WM_APP + 1)
#define WM_MOVE_ANIMATION            (WM_APP + 2)


HWND g_hWndMain{}; // Handle to the Main application window
const TCHAR g_mainName[]           = _T("TabTap"); // Application name
const TCHAR g_mainWndClassName[]   = _T("TabTapMainClass"); // Class name for the main application window
const TCHAR g_mainRegKey[]         = _T("SOFTWARE\\Empurple\\TabTap"); // Registry subkey for application settings
const TCHAR g_autoStartRegKey[]    = _T("Software\\Microsoft\\Windows\\CurrentVersion\\Run"); // Autorun registry subkey

const SIZE g_mainWndSizeCollapsed{ 7, 95 }; // Size of the Main window in collapsed state
const SIZE g_mainWndSizeExpanded{ 28, 95 }; // Size of the Main window in expanded state

HWND g_hWndOsk{}; // Handle to the on-screen keyboard window
const TCHAR g_oskExecPath[]        = _T("%WINDIR%\\System32\\osk.exe"); // Default path to the on-screen keyboard executable
const TCHAR g_oskRegKey[]          = _T("SOFTWARE\\Microsoft\\Osk"); // Registry subkey for on-screen keyboard settings
const TCHAR g_oskWndClassName[]    = _T("OSKMainClass"); // Class name for the on-screen keyboard window

ULONG_PTR g_gdiplusToken{};            // Token for GDI+ initialization
Gdiplus::Image* g_pApplicationImage{}; // Pointer to the application's image resource
NOTIFYICONDATA g_notifyIconData{};     // Data for the system tray icon
HMENU g_hTrayContextMenu{};            // Handle to the context menu for the system tray icon
HMODULE hDll{};
PROCESS_INFORMATION processInfo{};
STARTUPINFO startupInfo{};

bool isForAllUsers{}; // Indicates whether registry operations should use HKEY_LOCAL_MACHINE (true) or HKEY_CURRENT_USER (false)
RECT g_mainWindowRect{}; // Stores the Main window position


DWORD ErrorHandler(const TCHAR* message, DWORD errorCode = ERROR_SUCCESS)
{
	TCHAR buffer[256];
	wsprintf(buffer, _T("%s (%lu)"),message, errorCode);
	MessageBox(NULL, buffer, _T("Error"), MB_OK | MB_ICONERROR);
	return errorCode;
}

// Cleans up global resources used by the window. Called during normal shutdown and error exits.
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
void ForcedCleanup()
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

void ForcedExit(const TCHAR* message, DWORD errorCode = ERROR_SUCCESS)
{
	ErrorHandler(message, errorCode);
	ForcedCleanup();
	exit(errorCode);
}


void DrawImageOnLayeredWindow(HWND hwnd, bool isWindowExpanded)
{
	SIZE sizeWnd = isWindowExpanded ? g_mainWndSizeExpanded : g_mainWndSizeCollapsed;

	HDC hdcScreen = GetDC(NULL);
	HDC hdcMem = CreateCompatibleDC(hdcScreen);

	// Top-down DIB (negative height).
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
	// Check if autostart is enabled in the registry.
	bool autostartResult = ReadRegistry(isForAllUsers, g_autoStartRegKey, g_mainName, NULL);
	// Check if `Dock` mode is enabled in the registry.
	DWORD dwRegVal;
	ReadRegistry(isForAllUsers, g_oskRegKey, _T("Dock"), &dwRegVal);
	bool dockModeResult = (dwRegVal == 1);


	AppendMenu(g_hTrayContextMenu, MF_SEPARATOR, IDM_TRAY_SEPARATOR, NULL);
	AppendMenu(g_hTrayContextMenu, MF_STRING | (autostartResult ? MF_CHECKED : 0), IDM_TRAY_AUTOSTART, _T("Autostart"));
	AppendMenu(g_hTrayContextMenu, MF_SEPARATOR, IDM_TRAY_SEPARATOR, NULL);
	AppendMenu(g_hTrayContextMenu, MF_STRING | (dockModeResult ? MF_CHECKED : 0), IDM_TRAY_DOCKMODE, _T("Forced Dock mode"));
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
	 // Clamp to primary monitor bounds (avoid offscreen positioning)
	oskNewTop = max(0L, min(GetSystemMetrics(SM_CYSCREEN) - (int)oskWindowHeight, oskNewTop));
	WriteRegistry(isForAllUsers, g_oskRegKey, _T("WindowTop"), &oskNewTop, REG_DWORD);
}

void AutostartMenuToggle()
{
	UINT menuItemState = GetMenuState(g_hTrayContextMenu, IDM_TRAY_AUTOSTART, MF_BYCOMMAND);

	if ((menuItemState & MF_CHECKED) == MF_CHECKED) {
		RemoveRegistry(isForAllUsers, g_autoStartRegKey, g_mainName);
	}
	else {
		TCHAR mainExecPath[MAX_PATH + 2]; // Buffer to store the path to the main application executable
		DWORD result = GetModuleFileName(NULL, mainExecPath + 1, MAX_PATH);
		if (!result) {
			return;
		}
		// Format executable path with quotes for registry compatibility
		*mainExecPath = *(mainExecPath + result + 1) = _T('"');
		if (!WriteRegistry(isForAllUsers, g_autoStartRegKey, g_mainName, mainExecPath, REG_SZ)) {
			return;
		}
	}
	CheckMenuItem(g_hTrayContextMenu, IDM_TRAY_AUTOSTART, MF_BYCOMMAND | ((menuItemState & MF_CHECKED) ? MF_UNCHECKED : MF_CHECKED));
}

void DockModeToggle()
{
	UINT menuItemState = GetMenuState(g_hTrayContextMenu, IDM_TRAY_DOCKMODE, MF_BYCOMMAND);

	if ((menuItemState & MF_CHECKED) == MF_CHECKED) {
		DWORD newRegVal = 0;
		if (!WriteRegistry(isForAllUsers, g_oskRegKey, _T("Dock"), &newRegVal, REG_DWORD)) {
			return;
		}
		PostMessage(
			g_hWndOsk,
			WM_COMMAND,
			MAKEWPARAM(IDM_APP_REGULARMODE, CONTROL),
			(LPARAM)g_hWndMain
		);
	}
	else {
		DWORD newRegVal = 1;
		if (!WriteRegistry(isForAllUsers, g_oskRegKey, _T("Dock"), &newRegVal, REG_DWORD)) {
			return;
		}
		PostMessage(
			g_hWndOsk,
			WM_COMMAND,
			MAKEWPARAM(IDM_APP_DOCKMODE, CONTROL),
			(LPARAM)g_hWndMain
		);
	}
	CheckMenuItem(g_hTrayContextMenu, IDM_TRAY_DOCKMODE, MF_BYCOMMAND | ((menuItemState & MF_CHECKED) ? MF_UNCHECKED : MF_CHECKED));
}

LRESULT CALLBACK WindowProc(
	HWND hWnd,
	UINT uMsg,
	WPARAM wParam,
	LPARAM lParam)
{
	static bool isMouseTracking{};
	static bool isDragging{};
	static bool isMoving{};
	static bool isWindowExpanded{}; // Tracks whether the window is expanded or collapsed
	static POINT dragStartPoint{};
	static POINT moveTargePoint{};


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
		if (HIWORD(wParam) == MENU) {
			if (LOWORD(wParam) == IDM_TRAY_DOCKMODE) {
				DockModeToggle();
			}
			else if (LOWORD(wParam) == IDM_TRAY_AUTOSTART) {
				AutostartMenuToggle();
			}
			else if (LOWORD(wParam) == IDM_TRAY_EXIT) {
				SendMessage(hWnd, WM_CLOSE, 0, 0);
			}
		}
		else if (HIWORD(wParam) == CONTROL) {
			if (LOWORD(wParam) == IDM_APP_SYNC_Y_POSITION) {
				RECT rect;
				GetWindowRect(g_hWndOsk, &rect);
				GetWindowRect(hWnd, &g_mainWindowRect);
				moveTargePoint.y = rect.top + (rect.bottom - rect.top - g_mainWndSizeCollapsed.cy) / 2;
				if (!isMoving) {
					PostMessage(hWnd, WM_MOVE_ANIMATION, 0, 0);
					isMoving = true;
				}
			}
		}
		break;
	}
	case WM_CREATE:
	{
		CreateTrayPopupMenu();
		SetTimer(hWnd, 1, 5000, NULL); // 5 sec interval
		break;
	}
	case WM_DESTROY:
	{
		PostMessage(g_hWndOsk, WM_CLOSE, 0, (LPARAM)TRUE);
		GetWindowRect(g_hWndMain, &g_mainWindowRect);

		PostQuitMessage(0);
		return 0;
	}
	case WM_MOVE_ANIMATION:
	{
		if (g_mainWindowRect.top == moveTargePoint.y) {
			isMoving = false;
			break;
		}
		SetWindowPos(hWnd, NULL,
			g_mainWindowRect.left,
			g_mainWindowRect.top < moveTargePoint.y ? ++g_mainWindowRect.top : --g_mainWindowRect.top,
			0, 0,
			SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE
		);
		PostMessage(hWnd, WM_MOVE_ANIMATION, 0, 0);
		break;
	}

	}
	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}


// Setup System Tray Icon
BOOL SetupTrayIcon(HWND hWnd, const TCHAR* iconPath)
{
	ZeroMemory(&g_notifyIconData, sizeof(g_notifyIconData));
	g_notifyIconData.cbSize = sizeof(NOTIFYICONDATA);
	g_notifyIconData.hWnd = hWnd;
	g_notifyIconData.uID = 1;  // Unique ID for tray icon
	g_notifyIconData.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
	g_notifyIconData.uCallbackMessage = WM_TRAYICON;  // Custom message for mouse events
	g_notifyIconData.hIcon = ExtractIcon(NULL, iconPath, 0); // First icon in the EXE
	lstrcpy(g_notifyIconData.szTip, g_mainName);

	return Shell_NotifyIcon(NIM_ADD, &g_notifyIconData);
}

// Load Image Resource
BOOL LoadImageResource(const TCHAR* imagePath)
{
	g_pApplicationImage = new Gdiplus::Image(imagePath);
	if (g_pApplicationImage->GetLastStatus() != Gdiplus::Ok)
	{
		delete g_pApplicationImage;
		g_pApplicationImage = NULL;
		return FALSE;
	}

	if (g_pApplicationImage->GetWidth() < (UINT)g_mainWndSizeExpanded.cx ||
		g_pApplicationImage->GetHeight() < (UINT)g_mainWndSizeExpanded.cy) {
		return FALSE;
	}
	return TRUE;
}

// Create Layered Window
HWND CreateLayeredWindow(HINSTANCE hInstance)
{
	DWORD position{};
	DWORD oskWindowHeight{};
	int screenHeight = GetSystemMetrics(SM_CYSCREEN);
	// Retrieve and adjusts the main window vertical position based on registry-stored values
	bool result = ReadRegistry(isForAllUsers, g_oskRegKey, _T("WindowTop"), &position);
	position = result ? min((LONG)position, screenHeight - g_mainWndSizeCollapsed.cy) : 0;
	result = ReadRegistry(isForAllUsers, g_oskRegKey, _T("WindowHeight"), &oskWindowHeight);
	// Ensure it remains on-screen and match on-screen keyboard's vertical center
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

BOOL IsRunningAsAdmin()
{
	BOOL isAdmin = FALSE;
	PSID adminGroupSid = NULL;

	// Initialize SID for the Administrators group
	SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
	if (!AllocateAndInitializeSid(&ntAuthority, 2,
		SECURITY_BUILTIN_DOMAIN_RID,
		DOMAIN_ALIAS_RID_ADMINS,
		0, 0, 0, 0, 0, 0,
		&adminGroupSid))
	{
		return FALSE; // Failed to allocate SID
	}

	// Check if the token is part of the Administrators group
	if (!CheckTokenMembership(NULL, adminGroupSid, &isAdmin))
	{
		isAdmin = FALSE; // Fallback if check fails
	}

	// Cleanup
	if (adminGroupSid)
	{
		FreeSid(adminGroupSid);
	}

	return isAdmin;
}


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
	// Administrative rights required for CBTHook?	<-- CHECK
	if (!IsRunningAsAdmin())
	{
		ErrorHandler(_T(
			"This application requires administrative privileges to function properly. Please restart the application as an administrator."
		));
	}

	// Close OSK if it is open.
	if (g_hWndOsk = FindWindow(g_oskWndClassName, NULL)) {
		SendMessage(g_hWndOsk, WM_CLOSE, 0, (LPARAM)TRUE); // Abuse lParam for custom behavior
	}

	TCHAR oskExecPathEx[MAX_PATH]; // Buffer to store the full path to the on-screen keyboard
	if (!ExpandEnvironmentStrings(g_oskExecPath, oskExecPathEx, MAX_PATH)) {
		ForcedExit(_T("The environment variable expansion failed."), GetLastError());
	}

	TCHAR workDirBuff[MAX_PATH]; // Buffer to store the path to the current working directory
	TCHAR fullPathBuff[MAX_PATH]; // Buffer to store the full path
	GetModuleFileName(NULL, workDirBuff, MAX_PATH); // Get full EXE path
	PathRemoveFileSpec(workDirBuff); // Remove the filename

	HRESULT hr;
	hr = PathCchCombine(fullPathBuff, MAX_PATH, workDirBuff, _T("TabTap.png"));
	if (!SUCCEEDED(hr)) {
		ForcedExit(_T("Failed to combine full path."), GetLastError());
	}


	// Main Window Create
	// ==============================

	if (!InitializeGDIPlus()) {
		ForcedExit(_T("Failed to initialize GDI."), -1);
	}
	if (!RegisterWindowClass(hInstance)) {
		ForcedExit(_T("Failed to register window class."), GetLastError());
	}
	if (!(g_hWndMain = CreateLayeredWindow(hInstance))) {
		ForcedExit(_T("Failed to create window."), GetLastError());
	}
	if (!LoadImageResource(fullPathBuff)) {
		ForcedExit(_T("Failed to load PNG image."), -1);
	}
	if (!SetupTrayIcon(g_hWndMain, oskExecPathEx)) {
		ForcedExit(_T("Failed to setup tray icon."), GetLastError());
	}
	ShowWindow(g_hWndMain, nCmdShow);
	DrawImageOnLayeredWindow(g_hWndMain, false);


	// CBTHook Initializing
	// ==============================

	hr = PathCchCombine(fullPathBuff, MAX_PATH, workDirBuff, _T("CBTHook.dll"));
	if (!SUCCEEDED(hr)) {
		ForcedExit(_T("Failed to combine full path."), GetLastError());
	}

	if (!(hDll = LoadLibrary(fullPathBuff))) {
		ForcedExit(_T("The specified module could not be found."), ERROR_MOD_NOT_FOUND);
	}

	// Function pointer types for the exported functions.
	typedef BOOL(*UninstallHookFunc)();
	typedef BOOL(*InstallHookFunc)(DWORD);

	UninstallHookFunc UninstallHook = (UninstallHookFunc)GetProcAddress(hDll, "UninstallHook");
	InstallHookFunc InstallHook = (InstallHookFunc)GetProcAddress(hDll, "InstallHook");

	if (!UninstallHook or !InstallHook) {
		ForcedExit(_T("The specified procedure could not be found."), ERROR_PROC_NOT_FOUND);
	}


	// OSK Load
	// ==============================

	startupInfo.cb = sizeof(startupInfo);
	startupInfo.dwFlags = STARTF_USESHOWWINDOW;
	//startupInfo.wShowWindow = SW_SHOWNA;
	startupInfo.wShowWindow = SW_HIDE;

	if (!CreateProcess(
		oskExecPathEx,
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
		ForcedExit(_T("Failed to create process."), GetLastError());
	}

	DWORD result;
	result = WaitForInputIdle(processInfo.hProcess, 3000);
	if (result == WAIT_TIMEOUT) {
		ForcedExit(_T("The wait time-out interval elapsed."), WAIT_TIMEOUT);
	}
	if (result == WAIT_FAILED) {
		ForcedExit(_T("Failed to wait for the Process."), WAIT_FAILED);
	}


	// CBTHook Inject
	// ==============================

	// Create a manual-reset event that starts unsignaled.
	HANDLE hEvent;
	if (!(hEvent = CreateEvent(NULL, TRUE, FALSE, _T("OSKLoadEvent")))) {
		ForcedExit(_T("Failed to Create Event."), GetLastError());
	}

	// Inject hook.
	if (!InstallHook(processInfo.dwThreadId)) {
		ForcedExit(_T("Failed to Install the Windows Hook procedure."), ERROR_HOOK_NOT_INSTALLED);
	}

	// Wait for the event to be signaled.
	result = WaitForSingleObject(hEvent, 3000);
	if (result == WAIT_TIMEOUT) {
		CloseHandle(hEvent);
		ForcedExit(_T("The wait time-out interval elapsed."), WAIT_TIMEOUT);
	}
	if (result == WAIT_FAILED) {
		CloseHandle(hEvent);
		ForcedExit(_T("Failed to wait for the Process."), WAIT_FAILED);
	}

	// CBTHook DLL confirmed successful injection; safe to close the event.
	CloseHandle(hEvent);

	// Unload hook.
	if (!UninstallHook()) {
		ForcedExit(_T("Failed to remove the Windows hook."), ERROR_HOOK_NOT_INSTALLED);
	}

	// Unload library.
	FreeLibrary(hDll);



	// ==============================

	// Store OSK handle globally
	if (!(g_hWndOsk = FindWindow(g_oskWndClassName, NULL))) {
		ForcedExit(_T("Failed to Find Window."), -1);
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
		ForcedExit(_T("The wait time-out interval elapsed."), WAIT_TIMEOUT);
	}
	if (result == WAIT_FAILED) {
		ForcedExit(_T("Failed to wait for the Process."), WAIT_FAILED);
	}

	UpdateOSKPosition(); // NOTE: Last time coords was updated in WM_DESTROY

	if (processInfo.hProcess) {
		CloseHandle(processInfo.hProcess);
		processInfo.hProcess = NULL;
	}
	if (processInfo.hThread) {
		CloseHandle(processInfo.hThread);
		processInfo.hThread = NULL;
	}

	return 0;
}




