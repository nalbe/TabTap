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

using namespace Gdiplus;

#define WM_TRAYICON (WM_APP + 1)

ULONG_PTR gdiToken;
Image* g_pImage = nullptr;
bool g_bExpanded = false;  // false = collapsed (8px width), true = expanded (28px width)
NOTIFYICONDATA nid = { 0 };

// UpdateWindowLayer creates a 32-bit DIB section, draws the PNG (cropped as required) with alpha,
// and then updates the layered window via UpdateLayeredWindow.
void UpdateWindowLayer(HWND hwnd)
{
	// Determine desired dimensions.
	int windowWidth = g_bExpanded ? 29 : 7;
	int windowHeight = 95;

	HDC hdcScreen = GetDC(NULL);
	HDC hdcMem = CreateCompatibleDC(hdcScreen);

	// Create a top-down 32-bit DIB section.
	BITMAPINFO bmi = { 0 };
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = windowWidth;
	bmi.bmiHeader.biHeight = -windowHeight; // Negative height for top-down bitmap.
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 32;
	bmi.bmiHeader.biCompression = BI_RGB;

	void* pvBits = nullptr;
	HBITMAP hBitmap = CreateDIBSection(hdcMem, &bmi, DIB_RGB_COLORS, &pvBits, NULL, 0);
	HBITMAP hOldBmp = (HBITMAP)SelectObject(hdcMem, hBitmap);

	// Create a GDI+ Graphics object on the memory DC.
	Graphics graphics(hdcMem);
	graphics.SetCompositingMode(CompositingModeSourceOver);
	graphics.SetCompositingQuality(CompositingQualityHighQuality);
	graphics.SetInterpolationMode(InterpolationModeHighQualityBicubic);
	graphics.SetSmoothingMode(SmoothingModeAntiAlias);

	// Clear with full transparency.
	graphics.Clear(Color(0, 0, 0, 0));

	// Determine cropping parameters:
	// When expanded: draw full 28px (from the left side of the image).
	// When collapsed: draw the rightmost 8px.
	int srcLeft = g_bExpanded ? 0 : g_pImage->GetWidth() - 7;;
	int srcWidth = g_bExpanded ? 29 : 7;

	graphics.DrawImage(g_pImage, 0, 0, srcLeft, 0, srcWidth, g_pImage->GetHeight(), UnitPixel);

	// Prepare the blend function for per-pixel alpha.
	BLENDFUNCTION blendFunc = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };

	POINT ptSrc{};
	SIZE sizeWnd = { windowWidth, windowHeight };
	POINT ptDst;
	RECT rect;
	GetWindowRect(hwnd, &rect);
	ptDst.x = rect.left;
	ptDst.y = rect.top;

	UpdateLayeredWindow(hwnd, hdcScreen, &ptDst, &sizeWnd, hdcMem, &ptSrc, 0, &blendFunc, ULW_ALPHA);

	SelectObject(hdcMem, hOldBmp);
	DeleteObject(hBitmap);
	DeleteDC(hdcMem);
	ReleaseDC(NULL, hdcScreen);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	static bool tracking = false;
	static bool dragging = false;
	static POINT dragStart{};
	static RECT windowRect{};

	switch (uMsg)
	{
	case WM_NCHITTEST:
		// Return HTCLIENT so that our window handles dragging itself.
		return HTCLIENT;

	case WM_SETCURSOR:
		SetCursor(LoadCursor(NULL, IDC_ARROW));
		return TRUE;

	case WM_SYSCOMMAND:
		if ((wParam & 0xFFF0) == SC_MOVE)
			return 0;
		break;

	case WM_LBUTTONDOWN:
	{
		SetCapture(hwnd);
		dragging = true;
		POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
		ClientToScreen(hwnd, &pt);
		dragStart = pt;
		GetWindowRect(hwnd, &windowRect);
		break;
	}

	case WM_MOUSEMOVE:
	{
		if (dragging)
		{
			POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
			ClientToScreen(hwnd, &pt);
			int deltaY = pt.y - dragStart.y;
			int newTop = windowRect.top + deltaY;
			int screenHeight = GetSystemMetrics(SM_CYSCREEN);
			if (newTop < 0)
				newTop = 0;
			if (newTop > screenHeight - 95)
				newTop = screenHeight - 95;
			int currentWidth = g_bExpanded ? 29 : 7;
			SetWindowPos(hwnd, NULL, 0, newTop, currentWidth, 95, SWP_NOZORDER | SWP_NOACTIVATE);
			UpdateWindowLayer(hwnd);
		}
		else
		{
			if (!g_bExpanded)
			{
				RECT rect;
				GetWindowRect(hwnd, &rect);
				SetWindowPos(hwnd, NULL, 0, rect.top, 29, 95, SWP_NOMOVE | SWP_NOZORDER);
				g_bExpanded = true;
				UpdateWindowLayer(hwnd);
			}
		}

		if (!tracking)
		{
			TRACKMOUSEEVENT tme = { sizeof(TRACKMOUSEEVENT), TME_LEAVE, hwnd, 0 };
			TrackMouseEvent(&tme);
			tracking = true;
		}
		break;
	}

	case WM_LBUTTONUP:
		if (dragging)
		{
			dragging = false;
			ReleaseCapture();
		}
		break;

	case WM_MOUSELEAVE:
	{
		tracking = false;
		if (!dragging && g_bExpanded)
		{
			RECT rect;
			GetWindowRect(hwnd, &rect);
			SetWindowPos(hwnd, NULL, 0, rect.top, 7, 95, SWP_NOMOVE | SWP_NOZORDER);
			g_bExpanded = false;
			UpdateWindowLayer(hwnd);
		}
		break;
	}

	case WM_TRAYICON:
		// Process tray icon messages. Here, a double-click on the tray icon restores the window.
		if (lParam == WM_LBUTTONDBLCLK)
		{
			ShowWindow(hwnd, SW_SHOWNORMAL);
			UpdateWindowLayer(hwnd);
		}
		break;

	case WM_PAINT:
	{
		// Layered windows are updated via UpdateLayeredWindow.
		PAINTSTRUCT ps;
		BeginPaint(hwnd, &ps);
		EndPaint(hwnd, &ps);
		return 0;
	}

	case WM_DESTROY:
		Shell_NotifyIcon(NIM_DELETE, &nid);
		delete g_pImage;
		GdiplusShutdown(gdiToken);
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nShowCmd)
{
	// Initialize GDI+.
	GdiplusStartupInput gdiInput;
	GdiplusStartup(&gdiToken, &gdiInput, NULL);

	const wchar_t CLASS_NAME[] = L"BorderlessWindowClass";

	WNDCLASS wc = {};
	wc.lpfnWndProc = WindowProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = CLASS_NAME;
	// Use a NULL brush so the background is not painted by default.
	wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
	RegisterClass(&wc);

	// Create a layered window that is always on top and hidden from the taskbar.
	// WS_EX_TOOLWINDOW ensures the window is not shown in the taskbar.
	HWND hwnd = CreateWindowEx(
		WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TOOLWINDOW,
		CLASS_NAME,
		L"Borderless Window",
		WS_POPUP,
		0, 0, 7, 95,
		nullptr, nullptr, hInstance, nullptr);

	if (hwnd == nullptr)
		return 0;

	// Load the PNG image (expected to be at least 28x95 for the expanded state).
	g_pImage = new Image(L"c:\\background.png");
	if (g_pImage->GetLastStatus() != Ok)
	{
		MessageBox(hwnd, L"Failed to load PNG image.", L"Error", MB_OK | MB_ICONERROR);
		delete g_pImage;
		g_pImage = nullptr;
	}

	// Set up the tray icon.
	nid.cbSize = sizeof(NOTIFYICONDATA);
	nid.hWnd = hwnd;
	nid.uID = 1;
	nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
	nid.uCallbackMessage = WM_TRAYICON;
	nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wcscpy_s(nid.szTip, L"Tray Icon Example");
	Shell_NotifyIcon(NIM_ADD, &nid);

	ShowWindow(hwnd, nShowCmd);
	UpdateWindowLayer(hwnd);

	MSG msg = {};
	while (GetMessage(&msg, nullptr, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return 0;
}
