#pragma once
#include <windows.h>
#include <tchar.h>
#include "Settings.h"

// Global variables for control handles
static HWND hWndCheckbox1, hWndCheckbox2, hWndApply, hWndCancel;

// Forward declaration of the window procedure
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

void ShowSettingsWindow(HINSTANCE hInstance)
{
	WNDCLASS wc = { 0 };
	wc.lpfnWndProc = WndProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = _T("SettingsWindow");

	if (!RegisterClass(&wc))
	{
		MessageBox(NULL, _T("Window Registration Failed!"), _T("Error"), MB_ICONERROR);
		return;
	}

	HWND hwnd = CreateWindowEx(
		WS_EX_CLIENTEDGE,
		_T("SettingsWindow"),
		_T("Settings"),
		WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
		CW_USEDEFAULT, CW_USEDEFAULT, 250, 200,
		NULL, NULL, hInstance, NULL
	);

	if (hwnd == NULL)
	{
		MessageBox(NULL, _T("Window Creation Failed!"), _T("Error"), MB_ICONERROR);
		return;
	}

	ShowWindow(hwnd, SW_SHOW);
	UpdateWindow(hwnd);

	MSG msg;
	// Message loop runs until the window is closed.
	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_CREATE:
		// Create first checkbox ("Enable Feature 1")
		hWndCheckbox1 = CreateWindowEx(
			0, _T("BUTTON"), _T("Enable Feature 1"),
			WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
			20, 20, 180, 30,
			hwnd, (HMENU)1, ((LPCREATESTRUCT)lParam)->hInstance, NULL
		);

		// Create second checkbox ("Enable Feature 2")
		hWndCheckbox2 = CreateWindowEx(
			0, _T("BUTTON"), _T("Enable Feature 2"),
			WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
			20, 60, 180, 30,
			hwnd, (HMENU)2, ((LPCREATESTRUCT)lParam)->hInstance, NULL
		);

		// Create "Apply" button
		hWndApply = CreateWindowEx(
			0, _T("BUTTON"), _T("Apply"),
			WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
			50, 110, 80, 30,
			hwnd, (HMENU)3, ((LPCREATESTRUCT)lParam)->hInstance, NULL
		);

		// Create "Cancel" button
		hWndCancel = CreateWindowEx(
			0, _T("BUTTON"), _T("Cancel"),
			WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
			140, 110, 80, 30,
			hwnd, (HMENU)4, ((LPCREATESTRUCT)lParam)->hInstance, NULL
		);
		break;

	case WM_COMMAND:
		if (LOWORD(wParam) == 3) // "Apply" button clicked
		{
			int checked1 = (int)SendMessage(hWndCheckbox1, BM_GETCHECK, 0, 0);
			int checked2 = (int)SendMessage(hWndCheckbox2, BM_GETCHECK, 0, 0);

			MessageBox(hwnd,
				checked1 ? _T("Feature 1 Enabled!") : _T("Feature 1 Disabled!"),
				_T("Settings Applied"), MB_OK);

			MessageBox(hwnd,
				checked2 ? _T("Feature 2 Enabled!") : _T("Feature 2 Disabled!"),
				_T("Settings Applied"), MB_OK);
		}
		else if (LOWORD(wParam) == 4) // "Cancel" button clicked
		{
			DestroyWindow(hwnd);
		}
		break;

	case WM_CLOSE:
		DestroyWindow(hwnd);
		break;

	case WM_DESTROY:
		PostQuitMessage(0);
		break;

	default:
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}
	return 0;
}
