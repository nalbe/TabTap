#include <windows.h>

// Define GET_X_LPARAM and GET_Y_LPARAM if not available
#ifndef GET_X_LPARAM
#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#endif

#ifndef GET_Y_LPARAM
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))
#endif

HBITMAP g_hBitmap = NULL;
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
			if (newTop > screenHeight - 100)
				newTop = screenHeight - 100;

			int currentWidth = g_bExpanded ? 60 : 30;
			SetWindowPos(hwnd, NULL, 0, newTop, currentWidth, 100, SWP_NOZORDER | SWP_NOACTIVATE);
		}
		else
		{
			if (!g_bExpanded)
			{
				RECT rect;
				GetWindowRect(hwnd, &rect);
				SetWindowPos(hwnd, NULL, 0, rect.top, 60, 100, SWP_NOMOVE | SWP_NOZORDER);
				g_bExpanded = true;
				InvalidateRect(hwnd, NULL, TRUE);
			}
		}

		if (!tracking)
		{
			TRACKMOUSEEVENT tme = { 0 };
			tme.cbSize = sizeof(TRACKMOUSEEVENT);
			tme.dwFlags = TME_LEAVE;
			tme.hwndTrack = hwnd;
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
			SetWindowPos(hwnd, NULL, 0, rect.top, 30, 100, SWP_NOMOVE | SWP_NOZORDER);
			g_bExpanded = false;
			InvalidateRect(hwnd, NULL, TRUE);
		}
		break;
	}

	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hwnd, &ps);
		if (g_hBitmap)
		{
			HDC hdcMem = CreateCompatibleDC(hdc);
			HBITMAP hOldBmp = (HBITMAP)SelectObject(hdcMem, g_hBitmap);

			BITMAP bitmap;
			GetObject(g_hBitmap, sizeof(bitmap), &bitmap);

			int srcLeft = 0;
			int srcWidth = bitmap.bmWidth;
			if (!g_bExpanded)
			{
				srcWidth = 30;
				srcLeft = bitmap.bmWidth - srcWidth;
			}

			RECT clientRect;
			GetClientRect(hwnd, &clientRect);
			StretchBlt(hdc, 0, 0,
				clientRect.right - clientRect.left,
				clientRect.bottom - clientRect.top,
				hdcMem, srcLeft, 0,
				srcWidth, bitmap.bmHeight,
				SRCCOPY);

			SelectObject(hdcMem, hOldBmp);
			DeleteDC(hdcMem);
		}
		EndPaint(hwnd, &ps);
		return 0;
	}

	case WM_DESTROY:
		if (g_hBitmap)
		{
			DeleteObject(g_hBitmap);
			g_hBitmap = NULL;
		}
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
	const wchar_t CLASS_NAME[] = L"BorderlessWindowClass";

	WNDCLASS wc = {};
	wc.lpfnWndProc = WindowProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = CLASS_NAME;
	RegisterClass(&wc);

	HWND hwnd = CreateWindowEx(
		WS_EX_TOPMOST | WS_EX_LAYERED, // <-- Added WS_EX_LAYERED for transparency
		CLASS_NAME,
		L"Borderless Window",
		WS_POPUP,
		0,
		0,
		30,
		100,
		nullptr,
		nullptr,
		hInstance,
		nullptr);

	if (hwnd == nullptr)
		return 0;

	// Set 40% transparency (opacity 102 out of 255)
	SetLayeredWindowAttributes(hwnd, 0, 102, LWA_ALPHA);

	g_hBitmap = (HBITMAP)LoadImageW(NULL, L"c:\\background.bmp", IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
	if (g_hBitmap == NULL)
	{
		MessageBox(hwnd, L"Failed to load background image.", L"Error", MB_OK | MB_ICONERROR);
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
