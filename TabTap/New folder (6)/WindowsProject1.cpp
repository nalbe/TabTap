#include <windows.h>
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "Msimg32.lib")

// Define GET_X_LPARAM and GET_Y_LPARAM if not available
#ifndef GET_X_LPARAM
#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#endif
#ifndef GET_Y_LPARAM
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))
#endif

using namespace Gdiplus;

ULONG_PTR gdiToken;
Image* g_pImage = nullptr;
bool g_bExpanded = false; // false = collapsed (8px width), true = expanded (28px width)

// This helper function updates the layered window with per-pixel alpha.
void UpdateWindowLayer(HWND hwnd)
{
	// Determine desired dimensions.
	int windowWidth = g_bExpanded ? 28 : 8;
	int windowHeight = 95;

	// Get a screen DC.
	HDC hdcScreen = GetDC(NULL);
	// Create a memory DC compatible with the screen.
	HDC hdcMem = CreateCompatibleDC(hdcScreen);

	// Set up a 32-bit bitmap for our window (top-down DIB).
	BITMAPINFO bmi = { 0 };
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = windowWidth;
	bmi.bmiHeader.biHeight = -windowHeight; // negative for top-down DIB.
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

	// Clear with fully transparent color.
	graphics.Clear(Color(0, 0, 0, 0));

	// Determine cropping parameters.
	int srcLeft = 0;
	int srcWidth = 0;
	if (g_bExpanded)
	{
		// When expanded, show the full 28px (from the left side of the image).
		srcWidth = 28;
		srcLeft = 0;
	}
	else
	{
		// When collapsed, show the rightmost 8px.
		srcWidth = 8;
		srcLeft = g_pImage->GetWidth() - 8;
	}

	// Draw the image into our memory DC.
	graphics.DrawImage(g_pImage, 0, 0, srcLeft, 0, srcWidth, g_pImage->GetHeight(), UnitPixel);

	// Set up the blend function for per-pixel alpha.
	BLENDFUNCTION blendFunc = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };

	POINT ptSrc = { 0, 0 };
	SIZE sizeWnd = { windowWidth, windowHeight };
	POINT ptDst;
	// Get the current window position (upper left corner)
	RECT rect;
	GetWindowRect(hwnd, &rect);
	ptDst.x = rect.left;
	ptDst.y = rect.top;

	// Update the layered window.
	UpdateLayeredWindow(hwnd, hdcScreen, &ptDst, &sizeWnd, hdcMem, &ptSrc, 0, &blendFunc, ULW_ALPHA);

	// Clean up.
	SelectObject(hdcMem, hOldBmp);
	DeleteObject(hBitmap);
	DeleteDC(hdcMem);
	ReleaseDC(NULL, hdcScreen);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	static bool tracking = false;
	static bool dragging = false;
	static POINT dragStart = { 0, 0 };
	static RECT windowRect = { 0, 0, 0, 0 };

	switch (uMsg)
	{
	case WM_NCHITTEST:
		// Return HTCLIENT to handle our own dragging.
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

			// Clamp newTop to keep the window within the vertical screen bounds.
			int screenHeight = GetSystemMetrics(SM_CYSCREEN);
			if (newTop < 0)
				newTop = 0;
			if (newTop > screenHeight - 95)
				newTop = screenHeight - 95;

			int currentWidth = g_bExpanded ? 28 : 8;
			SetWindowPos(hwnd, NULL, 0, newTop, currentWidth, 95, SWP_NOZORDER | SWP_NOACTIVATE);
			UpdateWindowLayer(hwnd);
		}
		else
		{
			if (!g_bExpanded)
			{
				// Expand the window on mouseover.
				RECT rect;
				GetWindowRect(hwnd, &rect);
				SetWindowPos(hwnd, NULL, 0, rect.top, 28, 95, SWP_NOMOVE | SWP_NOZORDER);
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
			SetWindowPos(hwnd, NULL, 0, rect.top, 8, 95, SWP_NOMOVE | SWP_NOZORDER);
			g_bExpanded = false;
			UpdateWindowLayer(hwnd);
		}
		break;
	}

	case WM_PAINT:
	{
		// For layered windows, we update the entire window via UpdateLayeredWindow.
		PAINTSTRUCT ps;
		BeginPaint(hwnd, &ps);
		EndPaint(hwnd, &ps);
		return 0;
	}

	case WM_DESTROY:
		delete g_pImage;
		GdiplusShutdown(gdiToken);
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nShowCmd)
{
	// Initialize GDI+
	GdiplusStartupInput gdiInput;
	GdiplusStartup(&gdiToken, &gdiInput, NULL);

	const wchar_t CLASS_NAME[] = L"BorderlessWindowClass";

	WNDCLASS wc = {};
	wc.lpfnWndProc = WindowProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = CLASS_NAME;
	// No background brush is set so that we rely solely on our layered bitmap.
	wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
	RegisterClass(&wc);

	// Create a layered window (WS_EX_LAYERED) for per-pixel alpha.
	HWND hwnd = CreateWindowEx(
		WS_EX_TOPMOST | WS_EX_LAYERED,
		CLASS_NAME,
		L"Borderless Window",
		WS_POPUP,
		0, 0, 8, 95,
		nullptr, nullptr, hInstance, nullptr);

	if (hwnd == nullptr)
		return 0;

	// Note: We no longer call SetLayeredWindowAttributes with a color key.
	// Instead, our UpdateLayeredWindow call in WM_PAINT (and during move/resize) handles per-pixel alpha.

	// Load the PNG image (expected to be 28x95 for full expanded state) using GDI+.
	g_pImage = new Image(L"c:\\background2.png");
	if (g_pImage->GetLastStatus() != Ok)
	{
		MessageBox(hwnd, L"Failed to load PNG image.", L"Error", MB_OK | MB_ICONERROR);
		delete g_pImage;
		g_pImage = nullptr;
	}

	ShowWindow(hwnd, nShowCmd);
	// Initial update of the layered window.
	UpdateWindowLayer(hwnd);

	MSG msg = {};
	while (GetMessage(&msg, nullptr, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return 0;
}
