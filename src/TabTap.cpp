// Implementation-specific headers
//#include "resource.h"
#include "Registry.h"
#include "TabTap.h"

// Windows headers
#include <windows.h>
#include <tchar.h>
#include <shellapi.h>
#include <Shlwapi.h>
#include <PathCch.h>
#include <gdiplus.h>

// Library links
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "Msimg32.lib")
#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "Pathcch.lib")


// Define GET_X_LPARAM and GET_Y_LPARAM if not available.
#ifndef GET_X_LPARAM
#define GET_X_LPARAM(lp) ((INT)(WORD)LOWORD(lp))
#endif
#ifndef GET_Y_LPARAM
#define GET_Y_LPARAM(lp) ((INT)(WORD)HIWORD(lp))
#endif // GET_X_LPARAM

// Define min Windows version (Windows 7)
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif // _WIN32_WINNT

// Define end-of-line marker
#ifndef EOL_
#define EOL_ "\r\n"
#endif // EOL_


namespace Config
{
	LPCTSTR MAIN_NAME          = _T("TabTap"); // Application name
	LPCTSTR MAIN_CLASS_NAME    = _T("TabTapMainClass"); // Class name for the main application window
	LPCTSTR g_mainRegKey       = _T("SOFTWARE\\Empurple\\TabTap"); // Registry subkey for application settings
	LPCTSTR g_autoStartRegKey  = _T("Software\\Microsoft\\Windows\\CurrentVersion\\Run"); // Autorun registry subkey

	LPCTSTR OSK_CLASS_NAME     = _T("OSKMainClass"); // Class name for the on-screen keyboard window
	LPCTSTR OSK_APP_PATH       = _T("%WINDIR%\\System32\\osk.exe"); // Default path to the on-screen keyboard executable
	LPCTSTR g_oskRegKey        = _T("SOFTWARE\\Microsoft\\Osk"); // Registry subkey for on-screen keyboard settings
}

namespace
{
	HWND g_hOskWnd{};  // Handle to the On-Screen Keyboard window
	HWND g_hMainWnd{}; // Handle to the Main application window
	const SIZE g_mainWndSizeCollapsed{ 7, 95 }; // Size of the Main window in collapsed state
	const SIZE g_mainWndSizeExpanded{ 28, 95 }; // Size of the Main window in expanded state

	BOOL isForAllUsers{}; // Indicates whether registry operations should use HKEY_LOCAL_MACHINE (true) or HKEY_CURRENT_USER (false)
	RECT g_mainWindowRect{}; // Stores the Main window position
}

// Forward declarations
LRESULT CALLBACK WindowProc(HWND, UINT, WPARAM, LPARAM);


// Drag control structure
typedef struct
{
	BOOL isDragging;         // Indicates if drag operation is active
	POINT startPosition;     // Initial cursor position when drag begins
	POINT currentPosition;   // Current cursor position during drag
} DragData, * PDragInfo;

// Animation control structure
typedef struct {
	BYTE moveStep;
	POINT targetPoint;
	BOOL isAnimating;
} AnimationData, *PAnimInfo;




template <typename ...Args>
void ShowErrorMessage(LPCTSTR szTitle, LPCTSTR szFmtMessage, Args ...args)
{
	TCHAR szBuffer[256];
	_stprintf_s(szBuffer, szFmtMessage, args...);
	DWORD dwFlags = MB_OK | MB_ICONERROR;

	MessageBox(NULL, szBuffer, szTitle, dwFlags);
}

// Renders an image onto a layered window
void DrawImageOnLayeredWindow(HWND hWnd, BOOL isWindowExpanded, Gdiplus::Image* pApplicationImage)
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

	PVOID pvBits{};
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
	INT srcLeft = isWindowExpanded ? 0 : pApplicationImage->GetWidth() - g_mainWndSizeCollapsed.cx;
	INT srcWidth = isWindowExpanded ? g_mainWndSizeExpanded.cx : g_mainWndSizeCollapsed.cx;

	graphics.DrawImage(pApplicationImage, 0, 0, srcLeft, 0, srcWidth, pApplicationImage->GetHeight(), Gdiplus::UnitPixel);

	// Prepare the blend function for per-pixel remaining.
	BLENDFUNCTION blendFunc = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };

	POINT ptSrc{};
	POINT ptDst;
	RECT rect;
	GetWindowRect(hWnd, &rect);
	ptDst.x = rect.left;
	ptDst.y = rect.top;

	UpdateLayeredWindow(hWnd, hdcScreen, &ptDst, &sizeWnd, hdcMem, &ptSrc, 0, &blendFunc, ULW_ALPHA);

	SelectObject(hdcMem, hOldBmp);  // Restore original bitmap
	DeleteObject(hBitmap);
	DeleteDC(hdcMem);
	ReleaseDC(NULL, hdcScreen);
}

// Creates and populates a system tray popup menu
BOOL CreateTrayPopupMenu(HMENU* pMenu)
{
	*pMenu = CreatePopupMenu();
	if (!*pMenu) { return FALSE; }

	// Check if autostart is enabled in the registry.
	BOOL isAutostartEnabled = ReadRegistry(
		isForAllUsers, Config::g_autoStartRegKey, Config::MAIN_NAME, NULL
	);
	// Check if `Dock` mode is enabled in the registry.
	DWORD dwResult;
	ReadRegistry(
		isForAllUsers, Config::g_oskRegKey, _T("Dock"), &dwResult
	);
	BOOL isDockMode = (dwResult == 1);

	AppendMenu(*pMenu, MF_SEPARATOR, IDM_TRAY_SEPARATOR, NULL);
	AppendMenu(*pMenu, MF_STRING | (isAutostartEnabled ? MF_CHECKED : 0), IDM_TRAY_AUTOSTART, _T("Autostart"));
	AppendMenu(*pMenu, MF_SEPARATOR, IDM_TRAY_SEPARATOR, NULL);
	AppendMenu(*pMenu, MF_STRING | (isDockMode ? MF_CHECKED : 0), IDM_TRAY_DOCKMODE, _T("Forced Dock mode"));
	AppendMenu(*pMenu, MF_SEPARATOR, IDM_TRAY_SEPARATOR, NULL);
	AppendMenu(*pMenu, MF_STRING, IDM_TRAY_EXIT, _T("Exit"));
	AppendMenu(*pMenu, MF_SEPARATOR, IDM_TRAY_SEPARATOR, NULL);

	return *pMenu != NULL;
}

// Updates in registry the vertical position of an on-screen keyboard
void UpdateOSKPosition()
{
	DWORD oskWindowHeight;
	LONG oskNewTop;
	if (g_hMainWnd) {
		GetWindowRect(g_hMainWnd, &g_mainWindowRect);
	}

	ReadRegistry(
		isForAllUsers, Config::g_oskRegKey, _T("WindowHeight"), &oskWindowHeight
	);
	oskNewTop = g_mainWindowRect.top - ((LONG)oskWindowHeight - g_mainWndSizeCollapsed.cy) / 2;

	 // Clamp to primary monitor bounds (avoid offscreen positioning)
	oskNewTop = max(0L, min(GetSystemMetrics(SM_CYSCREEN) - (INT)oskWindowHeight, oskNewTop));
	WriteRegistry(
		isForAllUsers, Config::g_oskRegKey, _T("WindowTop"), &oskNewTop, REG_DWORD
	);
}

// Toggles the autostart feature by updating the registry
void ToggleMenuAutostart(HMENU hMenu)
{
	UINT menuItemState = GetMenuState(hMenu, IDM_TRAY_AUTOSTART, MF_BYCOMMAND);

	if ((menuItemState & MF_CHECKED) == MF_CHECKED) {
		RemoveRegistry(
			isForAllUsers, Config::g_autoStartRegKey, Config::MAIN_NAME
		);
	}
	else {
		TCHAR mainExecPath[MAX_PATH + 2]; // Buffer to store the path to the main application executable
		DWORD result = GetModuleFileName(NULL, mainExecPath + 1, MAX_PATH);
		if (!result) {
			return;
		}
		// Format executable path with quotes for registry compatibility
		*mainExecPath = *(mainExecPath + result + 1) = _T('"');
		BOOL bResult = WriteRegistry(
			isForAllUsers, Config::g_autoStartRegKey, Config::MAIN_NAME, mainExecPath, REG_SZ
		);
		if (!bResult) { return; }
	}

	CheckMenuItem(hMenu, IDM_TRAY_AUTOSTART, MF_BYCOMMAND | ((menuItemState & MF_CHECKED) ? MF_UNCHECKED : MF_CHECKED));
}

// Toggles the dock mode feature by updating the registry
void ToggleMenuDockMode(HMENU hMenu)
{
	UINT menuItemState = GetMenuState(hMenu, IDM_TRAY_DOCKMODE, MF_BYCOMMAND);

	if ((menuItemState & MF_CHECKED) == MF_CHECKED) {
		DWORD newRegVal = 0;
		BOOL bResult = WriteRegistry(
			isForAllUsers, Config::g_oskRegKey, _T("Dock"), &newRegVal, REG_DWORD
		);
		if (!bResult) { return; }

		PostMessage(
			g_hOskWnd,
			WM_APP_CUSTOM_MESSAGE,
			MAKEWPARAM(ID_APP_REGULARMODE, 0),
			(LPARAM)g_hMainWnd
		);
	}
	else {
		DWORD newRegVal = 1;
		BOOL bResult = WriteRegistry(
			isForAllUsers, Config::g_oskRegKey, _T("Dock"), &newRegVal, REG_DWORD
		);
		if (!bResult) { return; }

		PostMessage(
			g_hOskWnd,
			WM_APP_CUSTOM_MESSAGE,
			MAKEWPARAM(ID_APP_DOCKMODE, 0),
			(LPARAM)g_hMainWnd
		);
	}

	CheckMenuItem(hMenu, IDM_TRAY_DOCKMODE, MF_BYCOMMAND | ((menuItemState & MF_CHECKED) ? MF_UNCHECKED : MF_CHECKED));
}

// Load the icon
BOOL InitializeIcon(HICON* pIcon)
{
	TCHAR szBuffer[MAX_PATH];
	// Expand path to on-screen keyboard
	if (!ExpandEnvironmentStrings(Config::OSK_APP_PATH, szBuffer, MAX_PATH)) {
		return FALSE;
	}

	*pIcon = ExtractIcon(NULL, szBuffer, 0); // First icon in the EXE
	return (BOOL)*pIcon;
}

// Setup System Tray Icon
BOOL SetupTrayIcon(NOTIFYICONDATA* pNotifyIconData, HWND hWnd, HICON hIcon)
{
	_tcscpy_s(pNotifyIconData->szTip, Config::MAIN_NAME);  // Sets the tooltip text (paired with NIF_TIP)
	pNotifyIconData->cbSize = sizeof(NOTIFYICONDATA);      // Specifies the size of the structure, required by Shell_NotifyIcon
	pNotifyIconData->hWnd = hWnd;                          // Associates the tray icon with a window to receive callback messages
	pNotifyIconData->uID = IDI_NOTIFY_ICON;                // A unique identifier for the tray icon
	pNotifyIconData->uCallbackMessage = WM_APP_TRAYICON;   // Defines the custom message
	pNotifyIconData->hIcon = hIcon;                        // Specifies the icon to display in the tray (paired with NIF_ICON)
	pNotifyIconData->uFlags =
		NIF_ICON |      // The tray icon
		NIF_MESSAGE |   // Event handling
		NIF_TIP |       // Tooltip text
		NIF_SHOWTIP;    // Ensures the tooltip appears when hovering (Windows 10/11 enhancement)

	// Required for Windows 10/11
	pNotifyIconData->dwState = NIS_SHAREDICON;             // Indicates the icon is shared and shouldn’t be deleted when the notification is removed
	pNotifyIconData->dwStateMask = NIS_SHAREDICON;         // Specifies which state bits to modify

	// First add the icon to the tray
	if (!Shell_NotifyIcon(NIM_ADD, pNotifyIconData)) {     // Adds the tray icon to the system tray
		return FALSE;
	}
	// Set version AFTER adding
	pNotifyIconData->uVersion = NOTIFYICON_VERSION;        // Sets the tray icon version to enable modern features (e.g., balloon tips, improved behavior on Windows 10/11)

	return Shell_NotifyIcon(NIM_SETVERSION, pNotifyIconData);
}

// Load Image Resource
Gdiplus::Image* LoadImageResource()
{
	TCHAR szBuffer[MAX_PATH];
	// Get current working directory
	if (!GetCurrentDirectory(MAX_PATH, szBuffer)) { return NULL; }
	// Combine path with "TabTap.png" inplace
	HRESULT hResult = PathCchCombine(szBuffer, MAX_PATH, szBuffer, _T("TabTap.png"));
	if (!SUCCEEDED(hResult)) { return NULL; }

	Gdiplus::Image* pApplicationImage = new Gdiplus::Image(szBuffer);
	if (pApplicationImage->GetLastStatus() != Gdiplus::Ok) {
		delete pApplicationImage;
		return NULL;
	}

	if (pApplicationImage->GetWidth() < (UINT)g_mainWndSizeExpanded.cx ||
		pApplicationImage->GetHeight() < (UINT)g_mainWndSizeExpanded.cy) {
		delete pApplicationImage;
		return NULL;
	}
	return pApplicationImage;
}

// Create Layered Window
HWND CreateLayeredWindow(HINSTANCE hInstance)
{
	DWORD position{};
	DWORD oskWindowHeight{};
	INT screenHeight = GetSystemMetrics(SM_CYSCREEN);
	BOOL bResult{};

	// Retrieve and adjusts the main window vertical position based on registry-stored values
	bResult = ReadRegistry(
		isForAllUsers, Config::g_oskRegKey, _T("WindowTop"), &position
	);
	position = bResult ? min((LONG)position, screenHeight - g_mainWndSizeCollapsed.cy) : 0;

	bResult = ReadRegistry(
		isForAllUsers, Config::g_oskRegKey, _T("WindowHeight"), &oskWindowHeight
	);
	// Ensure it remains on-screen and match on-screen keyboard's vertical center
	position += (oskWindowHeight - g_mainWndSizeCollapsed.cy) / 2;

	return CreateWindowEx(
		WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
		Config::MAIN_CLASS_NAME,
		Config::MAIN_NAME,
		WS_POPUP,
		0, position, g_mainWndSizeCollapsed.cx, g_mainWndSizeCollapsed.cy,
		(HWND)NULL, (HMENU)NULL, hInstance, (LPVOID)NULL
	);
}

// Register Window Class
ATOM RegisterWindowClass(HINSTANCE hInstance, WNDCLASSEX* wcex)
{
	wcex->cbSize = sizeof(WNDCLASSEX);
	wcex->lpfnWndProc = WindowProc;                  // Window procedure
	wcex->hInstance = hInstance;                     // App instance
	wcex->lpszClassName = Config::MAIN_CLASS_NAME;   // Class name
	wcex->hCursor = LoadCursor(NULL, IDC_ARROW);     // Cursor
	wcex->hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);            // Background
	//wcex.hIconSm = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_MAIN_ICON));  // Icon
	wcex->style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;

	return RegisterClassEx(wcex);
}

// Initialize GDI+
BOOL InitializeGDIPlus(PULONG_PTR pGdiplusToken)
{
	Gdiplus::GdiplusStartupInput gdiInput;
	Gdiplus::Status gdiStatus = GdiplusStartup(pGdiplusToken, &gdiInput, NULL);

	return gdiStatus == Gdiplus::Ok;
}

// Creates a process to launch the On-Screen Keyboard (OSK) application
BOOL CreateOSKProcess(STARTUPINFO* pStartupInfo, PROCESS_INFORMATION* pProcessInfo)
{
	TCHAR szBuffer[MAX_PATH];
	pStartupInfo->cb = sizeof(STARTUPINFO);
	pStartupInfo->dwFlags = STARTF_USESHOWWINDOW;
	pStartupInfo->wShowWindow = SW_HIDE;

	// Expand path to on-screen keyboard
	if (!ExpandEnvironmentStrings(Config::OSK_APP_PATH, szBuffer, MAX_PATH)) {
		return FALSE;
	}

	// Create OSK Process
	return CreateProcess(
		szBuffer,
		NULL, NULL, NULL,
		FALSE,
		NULL, NULL, NULL,
		pStartupInfo, pProcessInfo);
}

// Loads a dynamic-link library
BOOL LoadHookDll(HMODULE* hDll)
{
	TCHAR szBuffer[MAX_PATH];
	if (!GetCurrentDirectory(MAX_PATH, szBuffer)) { return FALSE; }

	HRESULT hResult = PathCchCombine(szBuffer, MAX_PATH, szBuffer, _T("CBTHook.dll"));
	if (!SUCCEEDED(hResult)) { return FALSE; }

	*hDll = LoadLibrary(szBuffer);
	if (!*hDll) { return FALSE; }

	return TRUE;
}


LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg,
	WPARAM wParam, LPARAM lParam)
{
	static BOOL isMouseTracking{};
	static BOOL isWindowExpanded{};              // Tracks whether the window is expanded or collapsed
	static ULONG_PTR gdiplusToken{};             // Token for GDI+ initialization
	static Gdiplus::Image* pApplicationImage{};  // Pointer to the application's image resource
	static NOTIFYICONDATA notifyIconData{};      // Data for the system tray icon
	static HICON hIcon;                          // Handle to the system tray icon
	static HMENU hTrayContextMenu{};             // Handle to the context menu for the system tray icon
	static DragData dragData{};
	static AnimationData animData{ 1 };


	switch (uMsg)
	{

	case WM_TIMER:
	{
		if (wParam == IDT_KEEP_ON_TOP) {
			SetWindowPos(
				hWnd,
				HWND_TOPMOST,
				0, 0, 0, 0,
				SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOMOVE
			);
			return 0;
		}
		break;
	}

	case WM_SETCURSOR:
	{
		SetCursor(LoadCursor(NULL, IDC_ARROW));
		break;
	}

	case WM_MOUSEACTIVATE:
	{
		return MA_NOACTIVATE; // Prevent window activation on mouse click
	}

	case WM_MOUSEMOVE:
	{
		if (dragData.isDragging) {
			GetCursorPos(&dragData.currentPosition); // Get cursor screen coordinates
			INT newWindowTop = g_mainWindowRect.top + (dragData.currentPosition.y - dragData.startPosition.y);
			INT screenHeight = GetSystemMetrics(SM_CYSCREEN);
			newWindowTop = max(0, min(newWindowTop, screenHeight - g_mainWndSizeExpanded.cy));
			INT currentWidth = isWindowExpanded ? g_mainWndSizeExpanded.cx : g_mainWndSizeCollapsed.cx;
			SetWindowPos(
				hWnd, NULL,
				0, newWindowTop,
				currentWidth, g_mainWndSizeExpanded.cy,
				SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE
			);
			DrawImageOnLayeredWindow(hWnd, isWindowExpanded, pApplicationImage);
		}
		else if (!isWindowExpanded) {
			GetWindowRect(hWnd, &g_mainWindowRect);
			SetWindowPos(
				hWnd, NULL,
				0, g_mainWindowRect.top,
				g_mainWndSizeExpanded.cx, g_mainWndSizeCollapsed.cy,
				SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE
			);
			isWindowExpanded = true;
			DrawImageOnLayeredWindow(hWnd, isWindowExpanded, pApplicationImage);
		}

		if (!isMouseTracking) {
			TRACKMOUSEEVENT trackMouseEvent = { sizeof(TRACKMOUSEEVENT), TME_LEAVE, hWnd, 0 };
			TrackMouseEvent(&trackMouseEvent);
			isMouseTracking = true;
		}
		break;
	}

	case WM_RBUTTONDOWN:
	{
		SetCapture(hWnd);
		dragData.isDragging = true;
		GetCursorPos(&dragData.startPosition);
		GetWindowRect(hWnd, &g_mainWindowRect);
		break;
	}

	case WM_RBUTTONUP:
	{
		dragData.isDragging = false;
		ReleaseCapture();
		break;
	}

	case WM_RBUTTONDBLCLK:
	{
		if (IsWindowVisible(g_hOskWnd)) {
			// Collapse the main window when the cursor moves away
			RECT oskWindowRect;
			POINT cursorPosition;
			GetWindowRect(g_hOskWnd, &oskWindowRect);
			GetCursorPos(&cursorPosition);
			LONG targetPosition = oskWindowRect.top + 
				(oskWindowRect.bottom - oskWindowRect.top - g_mainWndSizeCollapsed.cy) / 2;

			if (cursorPosition.y < targetPosition
				or cursorPosition.y > targetPosition + g_mainWndSizeCollapsed.cy)
			{
				SendMessage(hWnd, WM_MOUSELEAVE, 0, 0);
			}

			// Start the animation loop
			PostMessage(
				hWnd,
				WM_APP_CUSTOM_MESSAGE,
				MAKEWPARAM(ID_APP_SYNC_Y_POSITION, 0),
				(LPARAM)hWnd
			);
		}
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
		if (wParam & MK_RBUTTON) {
			PostMessage(
				g_hOskWnd,
				WM_APP_CUSTOM_MESSAGE,
				MAKEWPARAM(ID_APP_FADE, 0),
				(LPARAM)hWnd
			);
		}
		break;
	}

	case WM_LBUTTONUP:
	{
		if (IsIconic(g_hOskWnd)) {
			UpdateOSKPosition();
			ShowWindowAsync(g_hOskWnd, SW_RESTORE); // Restore OSK
		}
		else if (IsWindowVisible(g_hOskWnd)) {
			SendMessage(g_hOskWnd, WM_CLOSE, 0, 0);
			//ShowWindowAsync(g_hWndOsk, SW_HIDE); // Hide OSK
		}
		else {
			UpdateOSKPosition();
			ShowWindowAsync(g_hOskWnd, SW_SHOWNA); // Show OSK
		}
		break;
	}

	case WM_MOUSELEAVE:
	{
		isMouseTracking = false;
		if (!dragData.isDragging && isWindowExpanded) {
			GetWindowRect(hWnd, &g_mainWindowRect);
			SetWindowPos(hWnd, HWND_TOPMOST,
				0, g_mainWindowRect.top,
				g_mainWndSizeCollapsed.cx, g_mainWndSizeCollapsed.cy,
				SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE
			);
			isWindowExpanded = false;
			DrawImageOnLayeredWindow(hWnd, isWindowExpanded, pApplicationImage);
		}
		break;
	}

	case WM_APP_TRAYICON:
	{
		if (lParam == WM_LBUTTONUP) {
			SendMessage(hWnd, WM_LBUTTONUP, 0, 0);
		}
		else if (lParam == WM_RBUTTONUP) {
			POINT cursorPosition;
			GetCursorPos(&cursorPosition);
			SetForegroundWindow(hWnd);
			TrackPopupMenu(hTrayContextMenu, TPM_LEFTBUTTON, cursorPosition.x, cursorPosition.y, 0, hWnd, NULL);
		}
		break;
	}

	case WM_COMMAND:
	{
		WORD wNotificationCode = HIWORD(wParam);
		WORD wCommandId = LOWORD(wParam);

		if (wNotificationCode == 0) {  // Menu/accelerator
			if (wCommandId == IDM_TRAY_DOCKMODE) {
				ToggleMenuDockMode(hTrayContextMenu);
			}
			else if (wCommandId == IDM_TRAY_AUTOSTART) {
				ToggleMenuAutostart(hTrayContextMenu);
			}
			else if (wCommandId == IDM_TRAY_EXIT) {
				SendMessage(hWnd, WM_CLOSE, 0, 0);
			}
		}

		else if (wNotificationCode == 1) {}  // Accelerator (rarely used explicitly)

		else {}  // Control notification

		break;
	}

	case WM_APP_CUSTOM_MESSAGE:
	{
		WORD wCommandId = LOWORD(wParam);

		if (wCommandId == ID_APP_SYNC_Y_POSITION) {
			RECT rect;
			GetWindowRect(g_hOskWnd, &rect);
			GetWindowRect(hWnd, &g_mainWindowRect);
			animData.targetPoint.y = rect.top + (rect.bottom - rect.top - g_mainWndSizeCollapsed.cy) / 2;
			if (!animData.isAnimating) {
				PostMessage(hWnd, WM_APP_CUSTOM_MESSAGE, MAKEWPARAM(ID_APP_ANIMATION, 0), 0);
				animData.isAnimating = true;
			}
			return 0;
		}

		if (wCommandId == ID_APP_ANIMATION) {
			LONG remaining = animData.targetPoint.y - g_mainWindowRect.top;

			if (!remaining) {
				animData.isAnimating = false;
				return 1;
			}
			LONG delta = min(abs(remaining), animData.moveStep) * (remaining < 0 ? -1 : 1); // Keep sign
			SetWindowPos(hWnd, NULL,
				g_mainWindowRect.left,
				g_mainWindowRect.top += delta,
				0, 0,
				SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE
			);
			PostMessage(hWnd, WM_APP_CUSTOM_MESSAGE, MAKEWPARAM(ID_APP_ANIMATION, 0), 0);
			return 0;
		}

		return 1;
	}

	case WM_CREATE:
	{
		// Init GDI+
		if (!InitializeGDIPlus(&gdiplusToken)) {
			ShowErrorMessage(_T("GDI+ Error"), _T("Failed to initialize GDI." EOL_ "%ul"),
				GetLastError()
			);
			return -1;
		}

		// Load app image
		pApplicationImage = LoadImageResource();
		if (!pApplicationImage) {
			ShowErrorMessage(_T("GDI+ Error"), _T("Failed to load PNG image." EOL_ "%ul"),
				GetLastError()
			);
			return -1;
		}

		if (!InitializeIcon(&hIcon)) {
			ShowErrorMessage(_T("Tray Icon Error"), _T("Failed to initialize icon." EOL_ "%ul"),
				GetLastError()
			);
			return -1;
		}

		// Set up tray icon
		if (!SetupTrayIcon(&notifyIconData, hWnd, hIcon)) {
			ShowErrorMessage(_T("Tray Icon Error"), _T("Failed to setup tray icon." EOL_ "%ul"),
				GetLastError()
			);
			return -1;
		}

		// Create tray popup menu
		if (!CreateTrayPopupMenu(&hTrayContextMenu)) {
			ShowErrorMessage(_T("Tray Icon Error"), _T("Failed to setup tray icon." EOL_ "%ul"),
				GetLastError()
			);
			return -1;
		}

		// Keep window always on top
		SetTimer(hWnd, IDT_KEEP_ON_TOP, 5000, NULL); // 5 sec interval

		// Explicitly show the window
		ShowWindow(hWnd, SW_SHOW);
		DrawImageOnLayeredWindow(hWnd, FALSE, pApplicationImage);
		return 0;
	}

	case WM_DESTROY:
	{
		// Close osk
		PostMessage(g_hOskWnd, WM_CLOSE, 0, (LPARAM)TRUE);
		// needed for save proper osk coordinates on exit
		GetWindowRect(hWnd, &g_mainWindowRect);
		//
		KillTimer(hWnd, IDT_KEEP_ON_TOP);

		// Destroy context menu.
		if (hTrayContextMenu) {
			DestroyMenu(hTrayContextMenu);
		}

		// Remove system tray icon
		Shell_NotifyIcon(NIM_DELETE, &notifyIconData);

		// Release tray icon resources
		if (hIcon) {
			DestroyIcon(hIcon);
		}

		// Free any allocated image resources.
		if (pApplicationImage) {
			delete pApplicationImage;
		}

		// Shutdown GDI+
		if (gdiplusToken) {
			Gdiplus::GdiplusShutdown(gdiplusToken);
		}

		PostQuitMessage(0);
		return 0;
	}


	}
	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}


// Main Entry Point
INT WINAPI WinMain(
	_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPSTR lpCmdLine,
	_In_ INT nCmdShow)
{
	// Check if program is already running
	if (g_hMainWnd = FindWindow(Config::MAIN_CLASS_NAME, NULL)) {
		PostMessage(g_hMainWnd, WM_LBUTTONUP, 0, 0);
		return ERROR_ALREADY_EXISTS;
	}

	// Close OSK if open
	if (g_hOskWnd = FindWindow(Config::OSK_CLASS_NAME, NULL)) {
		SendMessage(g_hOskWnd, WM_CLOSE, 0, (LPARAM)TRUE);  // lParam forces custom close
	}


	HMODULE hDll{};

	if (!LoadHookDll(&hDll)) {
		ShowErrorMessage(_T("DLL Error"),
			_T("Failed to load the DLL." EOL_ "%ul"), GetLastError());
		return -1;
	}

	// Function pointer types for the exported functions
	typedef BOOL(*UninstallHookFunc)();
	typedef BOOL(*InstallHookFunc)(DWORD);

	UninstallHookFunc UninstallHook = (UninstallHookFunc)GetProcAddress(hDll, "UninstallHook");
	InstallHookFunc InstallHook = (InstallHookFunc)GetProcAddress(hDll, "InstallHook");

	if (!UninstallHook or !InstallHook) {
		ShowErrorMessage(_T("DLL Error"),
			_T("The specified procedure could not be found." EOL_ "%ul"), ERROR_PROC_NOT_FOUND);
		FreeLibrary(hDll);
		return -1;
	}

	STARTUPINFO startupInfo{};
	PROCESS_INFORMATION processInfo{};
	WNDCLASSEX wcex{};
	HANDLE hEvent{};
	DWORD waitResult{};

	// Create OSK Process
	if (!CreateOSKProcess(&startupInfo, &processInfo)) {
		ShowErrorMessage(_T("Window Creation Failed"),
			_T("Unable to create OSK window. Check UAC is disabled." EOL_ "%ul"), GetLastError());
		goto CLEANUP;
	}
	if (!processInfo.hProcess) { goto CLEANUP; } // warnings C6387

	waitResult = WaitForInputIdle(processInfo.hProcess, 3000);
	if (waitResult == WAIT_TIMEOUT) {
		ShowErrorMessage(_T("Event Error"),
			_T("The wait time-out interval elapsed." EOL_ "%ul"), WAIT_TIMEOUT);
		goto CLEANUP;
	}
	if (waitResult == WAIT_FAILED) {
		ShowErrorMessage(_T("Event Error"),
			_T("Failed to wait for the Process." EOL_ "%ul"), WAIT_FAILED);
		goto CLEANUP;
	}


	// Hook Inject
	// ==============================

	// Create a manual-reset event that starts unsignaled
	if (!(hEvent = CreateEvent(NULL, TRUE, FALSE, _T("OSKLoadEvent")))) {
		ShowErrorMessage(_T("Event Error"),
			_T("Failed to create event." EOL_ "%ul"), GetLastError());
		goto CLEANUP;
	}

	// Inject hook
	if (!InstallHook(processInfo.dwThreadId)) {
		ShowErrorMessage(_T("Hook Error"),
			_T("Failed to install the Windows hook procedure." EOL_ "%ul"), ERROR_HOOK_NOT_INSTALLED);
		goto CLEANUP;
	}

	// Wait for the event to be signaled
	waitResult = WaitForSingleObject(hEvent, 3000);
	if (waitResult == WAIT_TIMEOUT) {
		ShowErrorMessage(_T("Event Error"),
			_T("The wait time-out interval elapsed." EOL_ "%ul"), WAIT_TIMEOUT);
		goto CLEANUP;
	}
	if (waitResult == WAIT_FAILED) {
		ShowErrorMessage(_T("Event Error"),
			_T("Failed to wait for the process." EOL_ "%ul"), WAIT_FAILED);
		goto CLEANUP;
	}

	// CBTHook.dll confirmed successful injection; Safe to close the event
	if (!CloseHandle(hEvent)) {
		ShowErrorMessage(_T("Event Error"),
			_T("Failed to close event." EOL_ "%ul"), GetLastError());
		hEvent = {};
		goto CLEANUP;
	}
	hEvent = {};

	// Unload hook
	if (!UninstallHook()) {
		ShowErrorMessage(_T("Hook Error"),
			_T("Failed to remove the Windows hook." EOL_ "%ul"), ERROR_HOOK_NOT_INSTALLED);
		goto CLEANUP;
	}

	// Store OSK handle globally
	if (!(g_hOskWnd = FindWindow(Config::OSK_CLASS_NAME, NULL))) {
		ShowErrorMessage(_T("Find Window Error"),
			_T("Failed to find the window." EOL_ "%ul"), -1);
		goto CLEANUP;
	}

	// Unload library
	FreeLibrary(hDll);
	hDll = {};


	// Register Main class and Create Window
	// ==============================
	if (!RegisterWindowClass(hInstance, &wcex)) {
		ShowErrorMessage(_T("Window Class Registration Failed"),
			_T("Unable to register window class." EOL_ "%ul"), GetLastError());
		goto CLEANUP;
	}

	g_hMainWnd = CreateLayeredWindow(hInstance);
	if (!g_hMainWnd) {
		ShowErrorMessage(_T("Window Creation Failed"),
			_T("Unable to create main window." EOL_ "%ul"), GetLastError());
		goto CLEANUP;
	}

	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}


	// Exit
	// ==============================
	waitResult = WaitForSingleObject(processInfo.hProcess, 3000);
	if (waitResult == WAIT_TIMEOUT) {
		ShowErrorMessage(_T("Process Wait Error"),
			_T("The wait time-out interval elapsed." EOL_ "%ul"), WAIT_TIMEOUT);
		goto CLEANUP;
	}
	if (waitResult == WAIT_FAILED) {
		ShowErrorMessage(_T("Process Wait Error"),
			_T("Failed to wait for the process." EOL_ "%ul"), WAIT_FAILED);
		goto CLEANUP;
	}

	UpdateOSKPosition(); // NOTE: Last time coords was updated in WM_DESTROY


CLEANUP:
	WNDCLASSEX existingWc{ sizeof(WNDCLASSEX) };
	BOOL isRegistered = GetClassInfoEx(wcex.hInstance, Config::MAIN_CLASS_NAME, &existingWc);
	if (isRegistered) { UnregisterClass(Config::MAIN_CLASS_NAME, GetModuleHandle(NULL)); }
	if (hEvent) { CloseHandle(hEvent); }
	if (hDll) { FreeLibrary(hDll); }
	if (processInfo.hProcess) { CloseHandle(processInfo.hProcess); }
	if (processInfo.hThread) { CloseHandle(processInfo.hThread); }

	return 0;
}




