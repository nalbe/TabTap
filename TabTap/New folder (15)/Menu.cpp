#pragma once
#include <windows.h>
#include <tchar.h>
#include <vector>
#include <string>
#include "Menu.h"

// Structure to hold a menu item.
struct MenuItem {
	std::wstring text;
	bool checked;
	RECT rect; // The rectangle where the item is drawn.
};

// Define GET_X_LPARAM and GET_Y_LPARAM if not available.
#ifndef GET_X_LPARAM
#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#endif
#ifndef GET_Y_LPARAM
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))
#endif

// Global vector of menu items.
static std::vector<MenuItem> g_menuItems;

// Custom menu window procedure.
static LRESULT CALLBACK CustomMenuWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_CREATE:
	{
		// Initialize menu items.
		g_menuItems.push_back({ L"Option 1", false, {} });
		g_menuItems.push_back({ L"Option 2", true, {} });
		g_menuItems.push_back({ L"Option 3", false, {} });
		g_menuItems.push_back({ L"Option 4", false, {} });

		// Set the window size based on the number of items.
		const int itemHeight = 30;
		const int width = 200;
		int height = static_cast<int>(g_menuItems.size()) * itemHeight;
		SetWindowPos(hwnd, NULL, 100, 100, width, height, SWP_NOZORDER);
	}
	break;

	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hwnd, &ps);

		// Fill background with the standard menu color.
		HBRUSH hBrushBack = CreateSolidBrush(GetSysColor(COLOR_MENU));
		FillRect(hdc, &ps.rcPaint, hBrushBack);
		DeleteObject(hBrushBack);

		const int itemHeight = 30;
		const int width = 200;

		// Draw each menu item.
		for (size_t i = 0; i < g_menuItems.size(); i++)
		{
			MenuItem& item = g_menuItems[i];
			// Calculate the rectangle for this item.
			item.rect = { 0, static_cast<int>(i * itemHeight), width, static_cast<int>((i + 1) * itemHeight) };

			// Draw a light background (could add hover effects here).
			HBRUSH hBrushItem = CreateSolidBrush(GetSysColor(COLOR_MENU));
			FillRect(hdc, &item.rect, hBrushItem);
			DeleteObject(hBrushItem);

			// Draw the checkbox area (reserve about 20 pixels for the checkbox).
			RECT checkRect = { item.rect.left + 5, item.rect.top + 5, item.rect.left + 20, item.rect.top + 20 };
			DrawEdge(hdc, &checkRect, EDGE_SUNKEN, BF_RECT);

			if (item.checked)
			{
				// If checked, draw a simple "X" mark.
				HPEN hPen = CreatePen(PS_SOLID, 2, RGB(0, 0, 0));
				HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
				MoveToEx(hdc, checkRect.left + 2, checkRect.top + 2, NULL);
				LineTo(hdc, checkRect.right - 2, checkRect.bottom - 2);
				MoveToEx(hdc, checkRect.right - 2, checkRect.top + 2, NULL);
				LineTo(hdc, checkRect.left + 2, checkRect.bottom - 2);
				SelectObject(hdc, hOldPen);
				DeleteObject(hPen);
			}

			// Draw the text of the menu item.
			RECT textRect = { item.rect.left + 25, item.rect.top, item.rect.right, item.rect.bottom };
			SetBkMode(hdc, TRANSPARENT);
			DrawText(hdc, item.text.c_str(), -1, &textRect, DT_SINGLELINE | DT_VCENTER);
		}

		EndPaint(hwnd, &ps);
	}
	break;

	case WM_LBUTTONDOWN:
	{
		int xPos = GET_X_LPARAM(lParam);
		int yPos = GET_Y_LPARAM(lParam);

		// Check which menu item was clicked.
		for (size_t i = 0; i < g_menuItems.size(); i++)
		{
			MenuItem& item = g_menuItems[i];
			if (PtInRect(&item.rect, { xPos, yPos }))
			{
				// Define the checkbox area inside the menu item.
				RECT checkRect = { item.rect.left + 5, item.rect.top + 5, item.rect.left + 20, item.rect.top + 20 };
				if (PtInRect(&checkRect, { xPos, yPos }))
				{
					// Toggle the checked state.
					item.checked = !item.checked;
					InvalidateRect(hwnd, &item.rect, TRUE);

					// Optionally, send a WM_COMMAND to notify a parent window.
					// For example: MAKEWPARAM(4000 + i, 0) as a unique identifier.
					SendMessage(GetParent(hwnd), WM_COMMAND, MAKEWPARAM(4000 + (UINT)i, 0), 0);
				}
				break;
			}
		}
	}
	break;

	case WM_DESTROY:
		PostQuitMessage(0);
		break;

	default:
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}
	return 0;
}

// WinMain: Create and show the custom menu window.
int WINAPI menuWinMain(
	_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPSTR lpCmdLine,
	_In_ int nShowCmd)
{
	const wchar_t CLASS_NAME[] = L"CustomMenuWindow";

	WNDCLASS wc = {};
	wc.lpfnWndProc = CustomMenuWndProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = CLASS_NAME;
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)(COLOR_MENU + 1);

	if (!RegisterClass(&wc))
	{
		MessageBox(NULL, _T("Failed to register window class"), _T("Error"), MB_ICONERROR);
		return 0;
	}

	// Create the custom menu window as a popup that won't show in the taskbar.
	HWND hwndMenu = CreateWindowEx(
		WS_EX_TOOLWINDOW,
		CLASS_NAME,
		L"Custom Menu",
		WS_POPUP,
		100, 100, 200, 200,
		NULL, NULL, hInstance, NULL
	);

	if (!hwndMenu)
	{
		MessageBox(NULL, _T("Failed to create window"), _T("Error"), MB_ICONERROR);
		return 0;
	}

	ShowWindow(hwndMenu, nShowCmd);
	UpdateWindow(hwndMenu);

	MSG msg;
	while (GetMessage(&msg, nullptr, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return 0;
}
