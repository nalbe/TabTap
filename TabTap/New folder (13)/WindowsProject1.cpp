#include <windows.h>
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

HWND hWndMain;
HWND hWndOsk;
ULONG_PTR gdiToken;
Gdiplus::Image* g_pImage = nullptr;
bool g_bExpanded = false;  // false = collapsed (imageS.cxpx width), true = expanded (imageL.cxpx width)
NOTIFYICONDATA nid{};
SIZE sizeCollapsed{ 7, 95 }, sizeExpanded{ 29, 95 };
WCHAR oskPathEX[MAX_PATH];
WCHAR oskPath[] = L"%WINDIR%\\System32\\osk.exe";
HKEY oskKey = HKEY_CURRENT_USER;
WCHAR oskSubKey[] = L"SOFTWARE\\Microsoft\\Osk";
WCHAR oskClassName[] = L"OSKMainClass";
PROCESS_INFORMATION pi;
STARTUPINFO si;


void Cleanup()
{
	Shell_NotifyIcon(NIM_DELETE, &nid);
	delete g_pImage;
	Gdiplus::GdiplusShutdown(gdiToken);
}

bool WriteRegistry(HKEY hRootKey, LPCWSTR subKey, LPCWSTR valueName, DWORD value)
{
	HKEY hKey;

	// Open or create the registry key
	if (RegOpenKeyEx(hRootKey, subKey, 0, KEY_SET_VALUE, &hKey) != ERROR_SUCCESS) {
		if (RegCreateKeyEx(hRootKey, subKey, 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, nullptr, &hKey, nullptr) != ERROR_SUCCESS) {
			MessageBox(NULL, L"Error: Failed to open or create registry key.", L"Error", MB_OK | MB_ICONERROR);
			return false;
		}
	}
	// Set the registry value
	if (RegSetValueEx(hKey, valueName, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&value), sizeof(value)) == ERROR_SUCCESS) {
		RegCloseKey(hKey);
		return true; // Successfully wrote the value
	}
	else {
		MessageBox(NULL, L"Error: Failed to write registry value.", L"Error", MB_OK | MB_ICONERROR);
		RegCloseKey(hKey);
		return false;
	}
}

bool ReadRegistry(HKEY hRootKey, LPCWSTR subKey, LPCWSTR valueName, DWORD& outValue)
{
	HKEY hKey;
	DWORD dataSize = sizeof(DWORD);
	DWORD valueType;

	// Open the registry key
	if (RegOpenKeyEx(hRootKey, subKey, 0, KEY_QUERY_VALUE, &hKey) == ERROR_SUCCESS) {
		// Read the registry value
		if (RegQueryValueEx(hKey, valueName, nullptr, &valueType, reinterpret_cast<LPBYTE>(&outValue), &dataSize) == ERROR_SUCCESS) {
			if (valueType == REG_DWORD) {
				RegCloseKey(hKey);
				return true; // Successfully read a DWORD value
			}
			else {
				MessageBox(NULL, L"Error: Value is not REG_DWORD.", L"Error", MB_OK | MB_ICONERROR);
			}
		}
		else {
			//MessageBox(NULL, L"Error: Failed to read registry value.", L"Error", MB_OK | MB_ICONERROR);
		}
		RegCloseKey(hKey);
	}
	else {
		//MessageBox(NULL, L"Error: Failed to open registry key.", L"Error", MB_OK | MB_ICONERROR);
	}
	return false;
}

void DrawImageOnLayeredWindow(HWND hwnd)
{
	SIZE sizeWnd = g_bExpanded ? sizeExpanded : sizeCollapsed;

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
	int srcLeft = g_bExpanded ? 0 : g_pImage->GetWidth() - sizeCollapsed.cx;
	int srcWidth = g_bExpanded ? sizeExpanded.cx : sizeCollapsed.cx;

	graphics.DrawImage(g_pImage, 0, 0, srcLeft, 0, srcWidth, g_pImage->GetHeight(), Gdiplus::UnitPixel);

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


LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	static bool tracking = false;
	static bool dragging = false;
	static bool isRunOSK = false;
	static POINT dragStart{};
	static RECT windowRect;
	static DWORD WindowHeightOSK;

	//	Don't block the loop while waiting for OSK 
	if (isRunOSK) {
		hWndOsk = FindWindow(oskClassName, NULL);
		if (hWndOsk) {
			//ShowWindowAsync(hWndOSK, SW_SHOWNA);
			//EnableMenuItem(GetSystemMenu(hWndOSK, FALSE), SC_MINIMIZE, MF_GRAYED);
			LONG style = GetWindowLong(hWndOsk, GWL_STYLE); // Get current style
			style &= ~WS_EX_APPWINDOW; // Hide from taskbar
			style &= ~WS_MINIMIZEBOX; // Remove minimize button
			SetWindowLong(hWndOsk, GWL_STYLE, style); // Apply new style
			isRunOSK = false;
		}
	}

	switch (uMsg)
	{
	case WM_NCHITTEST:
	{
		break;
	}
	case WM_SETCURSOR:
	{
		SetCursor(LoadCursor(NULL, IDC_ARROW));
		break;
	}
	case WM_SYSCOMMAND:
	{
		break;
	}
	case WM_RBUTTONDOWN:
	{
		SetCapture(hWnd);
		dragging = true;
		dragStart = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
		ClientToScreen(hWnd, &dragStart);
		GetWindowRect(hWnd, &windowRect);
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
			int newTop = windowRect.top + pt.y - dragStart.y;
			int screenHeight = GetSystemMetrics(SM_CYSCREEN);
			newTop = max(0, min(newTop, screenHeight - sizeExpanded.cy));
			int currentWidth = g_bExpanded ? sizeExpanded.cx : sizeCollapsed.cx;
			SetWindowPos(hWnd, NULL, 0, newTop, currentWidth, sizeExpanded.cy, SWP_NOZORDER | SWP_NOACTIVATE);
			DrawImageOnLayeredWindow(hWnd);
		}
		else if (!g_bExpanded)
		{
			GetWindowRect(hWnd, &windowRect);
			SetWindowPos(hWnd, NULL, 0, windowRect.top, sizeExpanded.cx, sizeCollapsed.cy, SWP_NOMOVE | SWP_NOZORDER);
			g_bExpanded = true;
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
		hWndOsk = FindWindow(oskClassName, NULL);

		if (hWndOsk) {
			if (IsIconic(hWndOsk)) {
				// Restore OSK
				ShowWindow(hWndOsk, SW_RESTORE);
			}
			else if (IsWindowVisible(hWndOsk)) {
				// Hide OSK
				ShowWindow(hWndOsk, SW_HIDE);
			}
			else {
				// Update OSK CY
				GetWindowRect(hWnd, &windowRect);
				ReadRegistry(oskKey, oskSubKey, L"WindowHeight", WindowHeightOSK);
				WriteRegistry(oskKey, oskSubKey, L"WindowTop", windowRect.top - (WindowHeightOSK - sizeCollapsed.cy) / 2);
				// Show OSK
				ShowWindow(hWndOsk, SW_SHOWNA);
			}
		}
		else {
			// Update OSK CY
			GetWindowRect(hWnd, &windowRect);
			ReadRegistry(oskKey, oskSubKey, L"WindowHeight", WindowHeightOSK);
			WriteRegistry(oskKey, oskSubKey, L"WindowTop", windowRect.top - (WindowHeightOSK - sizeCollapsed.cy) / 2);
			// Run OSK
			if (!CreateProcess(oskPathEX, NULL, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
				MessageBox(NULL, L"Failed to start osk.exe!", L"Error", MB_OK | MB_ICONERROR);
			}
			CloseHandle(pi.hThread);
			CloseHandle(pi.hProcess);
			isRunOSK = true;
		}
		break;
	}
	case WM_MOUSELEAVE:
	{
		tracking = false;
		if (!dragging && g_bExpanded)
		{
			GetWindowRect(hWnd, &windowRect);
			SetWindowPos(hWnd, NULL, 0, windowRect.top, sizeCollapsed.cx, sizeCollapsed.cy, SWP_NOMOVE | SWP_NOZORDER);
			g_bExpanded = false;
			DrawImageOnLayeredWindow(hWnd);
		}
		break;
	}
	case WM_TRAYICON:
	{
		if (lParam == WM_LBUTTONDBLCLK)
		{
			ShowWindow(hWnd, SW_SHOWNORMAL);
			DrawImageOnLayeredWindow(hWnd);
		}
		if (lParam == WM_RBUTTONUP)
		{
			DestroyWindow(hWnd);  // Close the window
		}
		break;
	}
	case WM_DESTROY:
	{
		// Close OSK
		hWndOsk = FindWindow(oskClassName, NULL);
		if(hWndOsk) {
			SendMessage(hWndOsk, WM_CLOSE, 0, 0);
		}

		GetWindowRect(hWnd, &windowRect);
		ReadRegistry(oskKey, oskSubKey, L"WindowHeight", WindowHeightOSK);
		WriteRegistry(oskKey, oskSubKey, L"WindowTop", windowRect.top - (WindowHeightOSK - sizeCollapsed.cy) / 2);

		Cleanup();
		PostQuitMessage(0);
		return 0;
	}
	}
	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

//
void SetupProcess()
{
	ZeroMemory(&si, sizeof(si));
	ZeroMemory(&pi, sizeof(pi));
	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESHOWWINDOW;
	si.wShowWindow = SW_SHOWNA;
	//si.wShowWindow = SW_HIDE;	// Cause flickering
}

// Setup System Tray Icon
void SetupTrayIcon(HWND hwnd)
{
	ZeroMemory(&nid, sizeof(nid));
	nid.cbSize = sizeof(NOTIFYICONDATA);
	nid.hWnd = hwnd;
	nid.uID = 1;  // Unique ID for tray icon
	nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
	nid.uCallbackMessage = WM_TRAYICON;  // Custom message for mouse events
	nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	lstrcpy(nid.szTip, L"TabTap");
	Shell_NotifyIcon(NIM_ADD, &nid);
}

// Load Image Resource
bool LoadImageResource()
{
	g_pImage = new Gdiplus::Image(L"c:\\background.png");
	if (g_pImage->GetLastStatus() != Gdiplus::Ok)
	{
		MessageBox(NULL, L"Failed to load PNG image.", L"Error", MB_OK | MB_ICONERROR);
		delete g_pImage;
		g_pImage = nullptr;
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

	bool result = ReadRegistry(oskKey, oskSubKey, L"WindowTop", value);
	value = result ? min(value, screenHeight - sizeCollapsed.cy) : 0;

	return CreateWindowEx(
		WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
		className,
		L"TabTap",
		WS_POPUP,
		0,
		value,
		sizeCollapsed.cx,
		sizeCollapsed.cy,
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
	GdiplusStartup(&gdiToken, &gdiInput, NULL);
}

//
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


// Main Entry Point
int WINAPI WinMain(
	_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPSTR lpCmdLine,
	_In_ int nCmdShow)
{
	ExpandEnvironmentStrings(oskPath, oskPathEX, MAX_PATH);
	SetupProcess();

	hWndOsk = FindWindow(oskClassName, NULL);
	if (hWndOsk) {
		SendMessage(hWndOsk, WM_CLOSE, 0, 0);
	}

/*
	HHOOK hHook = SetWindowsHookEx(WH_CALLWNDPROC, MyCallWndProc, NULL, GetCurrentThreadId());
	if (hHook == NULL) {
		// Handle error appropriately
		return -1;
	}
*/
	InitializeGDIPlus();

	const wchar_t CLASS_NAME[] = L"BorderlessWindowClass";
	if (!RegisterWindowClass(hInstance, CLASS_NAME)) {
		return 0;
	}
	hWndMain = CreateLayeredWindow(hInstance, CLASS_NAME);
	if (!hWndMain || !LoadImageResource()) {
		return 0;
	}

	SetupTrayIcon(hWndMain);
	ShowWindow(hWndMain, nCmdShow);
	DrawImageOnLayeredWindow(hWndMain);

	MSG msg;
	while (GetMessage(&msg, nullptr, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return 0;
}
