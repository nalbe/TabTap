// Implementation-specific headers
#include "UIComponents.h"
#include "Image.h"

// Default headers
#include <algorithm>

// Windows headers
#include <Shlwapi.h>
#include <PathCch.h>

// Library links
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "Pathcch.lib")



// --- GDIPlusData ---

Result GDIPlusData::SetResult(Result res)
{
	result = std::move(res);
	return std::move(res);
}

Result GDIPlusData::InitializeGDIPlus()
{
	if (!result) { return result; }
	if (pToken) {
		return SetResult({ Gdiplus::Status::Aborted,
			_T("GDI+ already initialized") });
	}

	Gdiplus::GdiplusStartupInput gdiInput; // GDI+ startup parameters
	Gdiplus::Status gdiStatus = GdiplusStartup(&pToken, &gdiInput, nullptr);

	if (gdiStatus != Gdiplus::Ok) {
		return SetResult({ gdiStatus,
			_T("GDI+ initialization failed") });
	}

	return {};
}

void GDIPlusData::ShutdownGDIPlus()
{
	// Shutdown GDI+
	if (pToken) { Gdiplus::GdiplusShutdown(pToken); }
	// Ensures GDI+ remains active until all objects are destroyed
}

GDIPlusData::~GDIPlusData()
{
	FreeImageAttributes();
	FreeImageResource();
	ShutdownGDIPlus();
}

GDIPlusData::GDIPlusData()
{
	InitializeGDIPlus();
	if (!result) { return; }

	LoadApplicationImage();
	if (!result) { return; }

	CreateImageAttributes();
	if (!result) { return; }
}

Result GDIPlusData::LoadImageFile(LPCTSTR cszImagePath)
{
	// Attempt to load the image
	delete pImage;
	pImage = new Gdiplus::Image(cszImagePath);

	// Check if allocation failed
	if (!pImage) {
		return SetResult({ ERROR_OUTOFMEMORY,
			_T("Image memory allocation failed") });
	}

	// Check image status
	if (pImage->GetLastStatus() != Gdiplus::Ok) {
		Gdiplus::Status status = pImage->GetLastStatus();
		delete pImage;
		pImage = nullptr;
		return SetResult({ status,
			_T("Image loading failed") });
	}

	return {};
}

Result GDIPlusData::LoadImageByteArray()
{
	// Clean up existing image
	delete pImage;

	// Create memory stream from embedded data
	IStream* stream = SHCreateMemStream(PNG_DATA, PNG_DATA_SIZE);
	if (!stream) {
		return SetResult({ ERROR_INVALID_DATA,
			_T("Failed to create memory stream") });
	}

	// Load image from memory stream
	pImage = new Gdiplus::Bitmap(stream);
	stream->Release();  // Release COM object when done

	// Check allocation
	if (!pImage) {
		return SetResult({ ERROR_OUTOFMEMORY,
			_T("Image memory allocation failed") });
	}

	// Check loading status
	Gdiplus::Status status = pImage->GetLastStatus();
	if (status != Gdiplus::Ok) {
		delete pImage;
		pImage = nullptr;
		return SetResult({ status,
			_T("Failed to load image from memory") });
	}

	return {};
}

Result GDIPlusData::LoadApplicationImage()
{
	if (!result) { return result; }

	TCHAR szBuffer[MAX_PATH]{}; // Buffer for the file path

	// Get current directory
	if (!GetCurrentDirectory(MAX_PATH, szBuffer)) {
		return SetResult({ GetLastError(),
			_T("Failed to get image directory") });
	}

	// Combine paths to form full image path
	if (FAILED(PathCchCombine(szBuffer, MAX_PATH, szBuffer, _T("TabTap.png")))) {
		return SetResult({ GetLastError(),
			_T("Failed to get image path") });
	}

	// Verify file existence
	if (GetFileAttributes(szBuffer) == INVALID_FILE_ATTRIBUTES) {
		return LoadImageByteArray();
	}

	return LoadImageFile(szBuffer);
}

void GDIPlusData::FreeImageResource()
{
	delete pImage; // Free the image resource
}

Gdiplus::Image* GDIPlusData::GetImage() const
{
	return pImage;
}

Result GDIPlusData::CreateImageAttributes()
{
	delete pImageAttributes;
	pImageAttributes = new Gdiplus::ImageAttributes;

	if (!pImageAttributes) {
		return SetResult({ ERROR_OUTOFMEMORY,
			_T("Image Attributes memory allocation failed") });
	}

	Gdiplus::Status status = pImageAttributes->GetLastStatus();
	if (status != Gdiplus::Ok) {
		delete pImageAttributes;
		pImageAttributes = nullptr;
		return SetResult({ status,
			_T("Image Attributes failed") });
	}

	return {};
}

void GDIPlusData::FreeImageAttributes()
{
	delete pImageAttributes; // Free the image attribute resource
	pImageAttributes = nullptr;
}

Gdiplus::ImageAttributes* GDIPlusData::GetImageAttributes() const
{
	return pImageAttributes;
}

Result GDIPlusData::ApplyColorMatrix(Gdiplus::ColorMatrix* pColorMatrix)
{
	if (!pImageAttributes || !pColorMatrix) {
		return SetResult({ ERROR_INVALID_PARAMETER,
			_T("Invalid parameters for color matrix") });
	}

	pImageAttributes->SetColorMatrix(pColorMatrix);

	Gdiplus::Status status = pImageAttributes->GetLastStatus();
	if (status != Gdiplus::Ok) {
		return SetResult({ status,
			_T("Image Attributes color matrix error") });
	}

	return {};
}

Result GDIPlusData::GetResult() const
{
	return result;
}



// --- TrayIconManager ---

Result TrayManager::SetResult(Result res)
{
	result = std::move(res);
	return std::move(res);
}

TrayManager::~TrayManager()
{
	DeleteTrayIcon();
	FreeIconResource();
	DestroyTrayMenu();
	delete pTrayAdapter;
}

TrayManager::TrayManager(ITrayAdapter* pAdapter)
{
	if (!pAdapter) { return; }

	pTrayAdapter = pAdapter;
	SetupTrayIcon();
}

Result TrayManager::SetupTrayIcon()
{
	Result result = pTrayAdapter->SetupTrayIcon(trayData);
	if (!result) { return SetResult(result); }

	if (!trayData.hWnd) {
		return SetResult({ 1001,
			_T("Invalid Handle"), _T("Window handle is null") });
	}

	return {};
}

void TrayManager::DeleteTrayIcon()
{
	Shell_NotifyIcon(NIM_DELETE, &trayData);
}

void TrayManager::GetIconResource(HICON hIcon)
{
	if (trayData.hIcon) { DestroyIcon(trayData.hIcon); }
	trayData.hIcon = hIcon;
}

void TrayManager::FreeIconResource()
{
	if (trayData.hIcon) { DestroyIcon(trayData.hIcon); }
	trayData.hIcon = NULL;
}

Result TrayManager::CreateTrayMenu()
{
	return SetResult(pTrayAdapter->CreateTrayMenu(hTrayMenu));
}

Result TrayManager::DestroyTrayMenu()
{
	if (hTrayMenu) {
		if (!DestroyMenu(hTrayMenu)) {
			return SetResult({ GetLastError(),
				_T("Failed to destroy pop-up menu") });
		}
		hTrayMenu = NULL;
	}

	return {};
}

Result TrayManager::TrackTrayMenu()
{
	if (!hTrayMenu) {
		return SetResult({ 1002,
			_T("Invalid Handle"), _T("Menu handle is null") });
	}

	BOOL bResult{};
	POINT ptCursor{};

	bResult = GetCursorPos(&ptCursor);
	if (!bResult) {
		DWORD dwErr = GetLastError();
		return SetResult({ dwErr,
			_T("Failed to get cursor position") });
	}

	bResult = SetForegroundWindow(trayData.hWnd);
	if (!bResult) {
		DWORD dwErr = GetLastError();
		return SetResult({ dwErr,
			_T("Failed to set foreground window") });
	}

	bResult = TrackPopupMenu(
		hTrayMenu,
		TPM_LEFTBUTTON,
		ptCursor.x, ptCursor.y,
		0, trayData.hWnd, NULL
	);
	if (!bResult) {
		DWORD dwErr = GetLastError();
		return SetResult({ dwErr,
			_T("Failed to track popup menu") });
	}

	return {};
}

Result TrayManager::HandleTrayRightClick()
{
	if (!CreateTrayMenu() or
		!TrackTrayMenu() or
		!DestroyTrayMenu())
	{
		return result;
	}

	return {};
}

Result TrayManager::GetResult() const
{
	return result;
}

void TrayManager::ClearResult()
{
	result = {};
}



// --- EdgeSnapping ---

Result EdgeSnapData::SetResult(Result res)
{
	result = std::move(res);
	return std::move(res);
}

void EdgeSnapData::CreatePreviewWindow()
{
	static LPCTSTR PREVIEW_CLASS = _T("EdgeSnapPreviewClass");
	// Register window class once
	static bool registered{};

	if (!registered) {
		WNDCLASSEX wc = { sizeof(WNDCLASSEX) };
		wc.lpfnWndProc = DefWindowProc;
		wc.hInstance = GetModuleHandle(nullptr);
		wc.lpszClassName = PREVIEW_CLASS;
		RegisterClassEx(&wc);
		registered = true;
	}

	hPreviewWnd = CreateWindowEx(
		WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_NOACTIVATE,
		PREVIEW_CLASS,
		nullptr,
		WS_POPUP,
		0, 0, 0, 0,
		nullptr, nullptr,
		GetModuleHandle(nullptr), nullptr
	);
}

void EdgeSnapData::UpdatePreviewWindow()
{
	// Calculate preview area based on edge
	const RECT& workArea = WorkAreaManager::GetWorkArea();
	RECT previewRect;

	switch (snapEdge)
	{
	case ScreenEdge::Left:
	{
		previewRect = { workArea.left, workArea.top,
						workArea.left + PreviewSize, workArea.bottom };
		break;
	}
	case ScreenEdge::Right:
	{
		previewRect = { workArea.right - PreviewSize, workArea.top,
						workArea.right, workArea.bottom };
		break;
	}
	case ScreenEdge::Top:
	{
		previewRect = { workArea.left, workArea.top,
						workArea.right, workArea.top + PreviewSize };
		break;
	}
	case ScreenEdge::Bottom:
	{
		previewRect = { workArea.left, workArea.bottom - PreviewSize,
						workArea.right, workArea.bottom };
		break;
	}
	default:
	{
		const POINT relativePos = dragTracker.GetRelativePosition();

		previewRect = {
			rcTargetRect.left + relativePos.x,
			rcTargetRect.top + relativePos.y,
			rcTargetRect.right + relativePos.x,
			rcTargetRect.bottom + relativePos.y
		};
		break;
	}
	}

	// Position and show preview window
	SetWindowPos(
		hPreviewWnd, HWND_TOPMOST,
		previewRect.left,
		previewRect.top,
		previewRect.right - previewRect.left,
		previewRect.bottom - previewRect.top,
		SWP_NOZORDER | SWP_NOACTIVATE
	);

	DrawPreview();
	ShowWindow(hPreviewWnd, SW_SHOWNOACTIVATE);
}

Result EdgeSnapData::DrawPreview()
{
	// Declare all GDI resources
	HDC hdcScreen{};
	HDC hdcMem{};
	HBITMAP hBmp{};
	HPEN hPen{};
	HBRUSH hBrush{};
	HGDIOBJ hOldBmp{};
	HGDIOBJ hOldPen{};
	HGDIOBJ hOldBrush{};

	// Setup cleanup lambda to release resources
	auto cleanup = [&]() {
		if (hdcMem) {
			if (hOldPen) SelectObject(hdcMem, hOldPen);
			if (hOldBrush) SelectObject(hdcMem, hOldBrush);
			if (hOldBmp) SelectObject(hdcMem, hOldBmp);
		}
		if (hBmp) DeleteObject(hBmp);
		if (hPen) DeleteObject(hPen);
		if (hBrush) DeleteObject(hBrush);
		if (hdcMem) DeleteDC(hdcMem);
		if (hdcScreen) ReleaseDC(nullptr, hdcScreen);
		};

	// Validate preview window handle
	if (!hPreviewWnd || !IsWindow(hPreviewWnd)) {
		return SetResult({ ERROR_INVALID_HANDLE,
			_T("Window Error"), _T("Preview window handle is invalid") });
	}

	// Retrieve window dimensions
	RECT rect;
	if (!GetWindowRect(hPreviewWnd, &rect)) {
		return SetResult({ GetLastError(),
			_T("Failed to get window dimensions") });
	}

	int width = rect.right - rect.left;
	int height = rect.bottom - rect.top;
	if (width <= 0 || height <= 0) {
		return SetResult({ 1,
			_T("Invalid Size"), _T("Window has non-positive dimensions") });
	}

	// Obtain screen DC and create compatible DC
	hdcScreen = GetDC(nullptr);
	if (!hdcScreen) {
		cleanup();
		return SetResult({ GetLastError(),
			_T("Failed to acquire screen DC") });
	}

	hdcMem = CreateCompatibleDC(hdcScreen);
	if (!hdcMem) {
		cleanup();
		return SetResult({ GetLastError(),
			_T("Failed to create memory DC") });
	}

	// Create DIB section for the bitmap
	BITMAPINFO bmi = { {
		sizeof(BITMAPINFOHEADER),
		width,
		-height, // Top-down
		1,
		32,
		BI_RGB
	} };
	void* pvBits{};
	hBmp = CreateDIBSection(hdcMem, &bmi, DIB_RGB_COLORS, &pvBits, nullptr, 0);
	if (!hBmp) {
		cleanup();
		return SetResult({ GetLastError(),
			_T("Failed to create DIB section") });
	}

	hOldBmp = SelectObject(hdcMem, hBmp);
	memset(pvBits, 0, width * height * 4); // Clear to transparent

	// Create and select drawing tools
	hPen = CreatePen(PS_INSIDEFRAME, PreviewFrameSize, RGB(0, 120, 215));
	if (!hPen) {
		cleanup();
		return SetResult({ GetLastError(),
			_T("Failed to create pen") });
	}
	hOldPen = SelectObject(hdcMem, hPen);
	BLENDFUNCTION blend = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };

	if (IsSnapEdge(ScreenEdge::None)) {
		hOldBrush = SelectObject(hdcMem, GetStockObject(NULL_BRUSH));
	}
	else {
		if (pSnapAdapter->IsValidSnapEdge(GetSnapEdge())) {
			hBrush = CreateSolidBrush(RGB(0, 120, 215));
		}
		else {
			hBrush = CreateSolidBrush(RGB(215, 0, 0));
		}

		if (!hBrush) {
			cleanup();
			return SetResult({ GetLastError(),
				_T("Failed to create brush") });
		}

		hOldBrush = SelectObject(hdcMem, hBrush);
		blend.SourceConstantAlpha = 150;
	}

	// Draw the preview frame or filled rectangle
	if (!Rectangle(hdcMem, 0, 0, width, height)) {
		cleanup();
		return SetResult({ GetLastError(),
			_T("Failed to draw preview rectangle") });
	}

	// Update the layered window with the new bitmap
	POINT ptPos{ rect.left, rect.top };
	SIZE szWnd{ width, height };
	POINT ptSrc{};
	if (!UpdateLayeredWindow(hPreviewWnd, nullptr, &ptPos, &szWnd,
		hdcMem, &ptSrc, 0, &blend, ULW_ALPHA)) {
		cleanup();
		return SetResult({ GetLastError(),
			_T("Failed to update layered window") });
	}

	cleanup();
	return {};
}

EdgeSnapData::~EdgeSnapData()
{
	if (hPreviewWnd) {
		DestroyWindow(hPreviewWnd);
	}
	if (pSnapAdapter) {
		delete pSnapAdapter;
	}
}

EdgeSnapData::EdgeSnapData() {}

ScreenEdge EdgeSnapData::GetSnapEdge() const
{
	return snapEdge;
}

bool EdgeSnapData::IsSnapEdge(const ScreenEdge& edge)
{
	return edge == GetSnapEdge();
}

bool EdgeSnapData::Enable(ISnapAdapter* pAdapter)
{
	if (!pAdapter) { return false; }

	HWND hTargetWnd = pAdapter->GetTargetWindow();
	if (!hTargetWnd) { return false; }

	GetWindowRect(hTargetWnd, &rcTargetRect);
	dragTracker.BeginDrag();
	pSnapAdapter = pAdapter;
	isEnabled = true;

	// Update work area to avoid taskbar during snap
	WorkAreaManager::Refresh();

	return true;
}

void EdgeSnapData::Disable()
{
	const POINT relativePos = dragTracker.GetRelativePosition();
	POINT ptDest = {
		rcTargetRect.left + relativePos.x,
		rcTargetRect.top + relativePos.y
	};

	if (IsSnapEdge(ScreenEdge::None)) {
		pSnapAdapter->OnSnapEdgeNone(ptDest);
	}
	else if (pSnapAdapter->IsValidSnapEdge(GetSnapEdge())) {
		pSnapAdapter->OnSnapSuccess(
			GetSnapEdge(), ptDest
		);
	}
	else {
		pSnapAdapter->OnSnapRejected(
			GetSnapEdge(), ptDest
		);
	}

	if (hPreviewWnd) {
		DestroyWindow(hPreviewWnd);
		hPreviewWnd = nullptr;
	}
	if (pSnapAdapter) {
		delete pSnapAdapter;
		pSnapAdapter = nullptr;
	}
	isEnabled = false;
	isPreviewEnabled = false;
	snapEdge = ScreenEdge::None;
	rcTargetRect = {};
	dragTracker.EndDrag();
}

bool EdgeSnapData::IsEnabled() const
{
	return isEnabled;
}

bool EdgeSnapData::IsPreviewEnabled() const
{
	return isPreviewEnabled;
}

bool EdgeSnapData::OnMouseMove()
{
	if (isEnabled) {
		dragTracker.Update();
		const POINT& cursorPos = dragTracker.GetCursorPosition();

		if (isPreviewEnabled) {
			// Determine snap edge based on cursor position
			const RECT& workArea = WorkAreaManager::GetWorkArea();
			snapEdge = ScreenEdge::None;

			if (cursorPos.x <= workArea.left + SnapMargin) { snapEdge = ScreenEdge::Left; }
			else if (cursorPos.x >= workArea.right - SnapMargin) { snapEdge = ScreenEdge::Right; }
			else if (cursorPos.y <= workArea.top + SnapMargin) { snapEdge = ScreenEdge::Top; }
			else if (cursorPos.y >= workArea.bottom - SnapMargin) { snapEdge = ScreenEdge::Bottom; }
			if (snapEdge == pSnapAdapter->GetSnapEdge()) { snapEdge = ScreenEdge::None; }

			UpdatePreviewWindow();
		}
		// Enable preview only if cursor leaves the window
		else if (!PtInRect(&rcTargetRect, cursorPos)) {
			if (!hPreviewWnd) { CreatePreviewWindow(); }
			isPreviewEnabled = true;
		}

		return true;
	}
	return false;
}



// --- AnimationData ---

AnimationData::~AnimationData() {}

AnimationData::AnimationData() {}

bool AnimationData::Enable(HWND hWnd, const POINT& ptDest)
{
	if (isEnabled) { return false; }

	// Update work area to avoid taskbar
	WorkAreaManager::Refresh();

	RECT targetRect;
	if (!GetWindowRect(hWnd, &targetRect)) { return false; }

	ptCurrentPoint = { targetRect.left, targetRect.top };
	ptTargetPoint = {
		// X-axis clamp
		std::clamp(
			ptDest.x,
			WorkAreaManager::GetWorkArea().left,
			WorkAreaManager::GetWorkArea().right - (targetRect.right - targetRect.left)
		),
			// Y-axis clamp
			std::clamp(
				ptDest.y,
				WorkAreaManager::GetWorkArea().top,
				WorkAreaManager::GetWorkArea().bottom - (targetRect.bottom - targetRect.top)
			)
	};
	hAnimatedWnd = hWnd;
	isEnabled = true;

	return true;
}

void AnimationData::Disable()
{
	isEnabled = false;
	hAnimatedWnd = NULL;
	ptCurrentPoint = {};
	ptTargetPoint = {};
}

bool AnimationData::IsEnabled() const
{
	return isEnabled;
}

bool AnimationData::Update()
{
	if (!isEnabled or !hAnimatedWnd) { return false; }

	// Calculate remaining distance for BOTH axes
	const POINT ptRemaining = {
		ptTargetPoint.x - ptCurrentPoint.x,
		ptTargetPoint.y - ptCurrentPoint.y
	};

	// Check if animation is complete (both axes)
	if (ptRemaining.x == 0 and ptRemaining.y == 0) {
		Disable();
		return false;
	}

	ptCurrentPoint.x += std::clamp<LONG>(ptRemaining.x, -pixelByStep, pixelByStep);
	ptCurrentPoint.y += std::clamp<LONG>(ptRemaining.y, -pixelByStep, pixelByStep);

	// Move the window to the new position
	SetWindowPos(
		hAnimatedWnd, NULL,
		ptCurrentPoint.x, ptCurrentPoint.y,
		0, 0,
		SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE
	);

	// Continue if either axis hasn't reached target
	return true;
}



// --- BlinkData ---

void BlinkData::InitializeColorMatrices()
{
	// Initialize normalMatrix to identity matrix (no color adjustment)
	normalMatrix = { {
		{1.0f, 0.0f, 0.0f, 0.0f, 0.0f},  // Red channel
		{0.0f, 1.0f, 0.0f, 0.0f, 0.0f},  // Green channel
		{0.0f, 0.0f, 1.0f, 0.0f, 0.0f},  // Blue channel
		{0.0f, 0.0f, 0.0f, 1.0f, 0.0f},  // Alpha channel
		{0.0f, 0.0f, 0.0f, 0.0f, 1.0f}   // Translations
	} };

	// Copy normalMatrix to blinkMatrix as a starting point
	blinkMatrix = normalMatrix;

	// Set translation values for blink effect (adds color tint)
	blinkMatrix.m[4][0] = GetRValue(defBlinkColor) / 255.0f * defIntensity; // Red component
	blinkMatrix.m[4][1] = GetGValue(defBlinkColor) / 255.0f * defIntensity; // Green component
	blinkMatrix.m[4][2] = GetBValue(defBlinkColor) / 255.0f * defIntensity; // Blue component
}

BlinkData::~BlinkData()
{
	if (isEnabled) { Disable(); }
}

BlinkData::BlinkData()
{
	InitializeColorMatrices();
}

bool BlinkData::Enable(HWND hWnd, const UINT& timerId)
{
	blinkStartTime = GetTickCount();

	if (isEnabled) { return false; }

	hTargetWnd = hWnd;
	uTimerID = timerId;
	isEnabled = true;
	isBlinkState = true;
	SetTimer(hTargetWnd, timerId, defInterval, NULL);

	return true;
}

void BlinkData::Disable()
{
	KillTimer(hTargetWnd, uTimerID);
	uTimerID = {};
	hTargetWnd = NULL;
	isEnabled = false;
	isBlinkState = false;
}

bool BlinkData::IsEnabled() const
{
	return isEnabled;
}

bool BlinkData::Update()
{
	if (IsElapsed()) {
		Disable();
		return false;
	}

	isBlinkState = !isBlinkState;
	return true;
}

bool BlinkData::IsElapsed()
{
	return GetTickCount() >= (blinkStartTime + defDuration);
}

Gdiplus::ColorMatrix* BlinkData::GetActiveColorMatrix()
{
	return isBlinkState
		? &blinkMatrix
		: &normalMatrix;
}



