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
bool g_bExpanded = false;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	static bool tracking = false;
	static bool dragging = false;
	static POINT dragStart = { 0, 0 };
	static RECT windowRect = { 0, 0, 0, 0 };

	switch (uMsg)
	{
	case WM_NCHITTEST:
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

			int currentWidth = g_bExpanded ? 28 : 8;
			SetWindowPos(hwnd, NULL, 0, newTop, currentWidth, 95, SWP_NOZORDER | SWP_NOACTIVATE);
		}
		else
		{
			if (!g_bExpanded)
			{
				RECT rect;
				GetWindowRect(hwnd, &rect);
				SetWindowPos(hwnd, NULL, 0, rect.top, 28, 95, SWP_NOMOVE | SWP_NOZORDER);
				g_bExpanded = true;
				InvalidateRect(hwnd, NULL, TRUE);
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
			InvalidateRect(hwnd, NULL, TRUE);
		}
		break;
	}

	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hwnd, &ps);

		if (g_pImage)
		{
			HDC hdcMem = CreateCompatibleDC(hdc);
			HBITMAP hBitmap = CreateCompatibleBitmap(hdc, 28, 95);
			SelectObject(hdcMem, hBitmap);

			Graphics graphics(hdcMem);
			graphics.SetCompositingMode(CompositingModeSourceOver);

			// **Pixel-Perfect Rendering**
			graphics.SetSmoothingMode(SmoothingModeNone); // No smoothing
			graphics.SetInterpolationMode(InterpolationModeNearestNeighbor); // No blurring

			int srcLeft = 0;
			int srcWidth = g_pImage->GetWidth();
			if (!g_bExpanded)
			{
				srcWidth = 8;  // Show rightmost 8px when collapsed
				srcLeft = g_pImage->GetWidth() - srcWidth;
			}
			else
			{
				srcWidth = 28; // Show 28px when expanded
				srcLeft = 0;
			}

			graphics.Clear(Color(0, 0, 0, 0)); // Transparent background
			graphics.DrawImage(g_pImage, 0, 0, srcLeft, 0, srcWidth, g_pImage->GetHeight(), UnitPixel);

			// Apply transparency using AlphaBlend
			BLENDFUNCTION blendFunc = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
			AlphaBlend(hdc, 0, 0, srcWidth, 95, hdcMem, 0, 0, srcWidth, 95, blendFunc);

			DeleteObject(hBitmap);
			DeleteDC(hdcMem);
		}

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

int WINAPI WinMain(
	_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPSTR lpCmdLine,
	_In_ int nShowCmd)
{
	GdiplusStartupInput gdiInput;
	GdiplusStartup(&gdiToken, &gdiInput, NULL);

	const wchar_t CLASS_NAME[] = L"BorderlessWindowClass";

	WNDCLASS wc = {};
	wc.lpfnWndProc = WindowProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = CLASS_NAME;
	wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH); // Ensure transparency
	RegisterClass(&wc);

	HWND hwnd = CreateWindowEx(
		WS_EX_TOPMOST | WS_EX_LAYERED, // Make the window layered for transparency
		CLASS_NAME,
		L"Borderless Window",
		WS_POPUP,
		0, 0, 8, 95,
		nullptr, nullptr, hInstance, nullptr);

	if (hwnd == nullptr)
		return 0;

	// Set transparency for the window
	SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 0, LWA_COLORKEY);

	g_pImage = new Image(L"c:\\background.png");
	if (g_pImage->GetLastStatus() != Ok)
	{
		MessageBox(hwnd, L"Failed to load PNG image.", L"Error", MB_OK | MB_ICONERROR);
		delete g_pImage;
		g_pImage = nullptr;
	}

	ShowWindow(hwnd, nShowCmd);

	MSG msg = {};
	while (GetMessage(&msg, nullptr, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return 0;
}
