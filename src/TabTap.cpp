// Implementation-specific headers
#include "TabTap.h"
#include "resource.h"
#include "UIComponents.h"
#include "CustomIncludes/WinApi/MessageBoxNotifier.h"
#include "CustomIncludes/WinApi/ThemeManager.h"
#include "CustomIncludes/WinApi/WindowDragger.h"
#include "CustomIncludes/WinApi/MouseTracker.h"
#include "CustomIncludes/WinApi/WorkAreaManager.h"
#include "CustomIncludes/WinApi/RegistryManager.h"

// Default headers
#include <mutex>
#include <algorithm>

// Windows system headers
#include <Windows.h>
#include <windowsx.h>  // For GET_X_LPARAM, GET_Y_LPARAM
#include <PathCch.h>

// Library links
#pragma comment(lib, "Pathcch.lib")



namespace Config
{
	// Application identity
	constexpr LPCTSTR ApplicationName = _T("TabTap");
	constexpr LPCTSTR MainWindowClass = _T("TabTapMainClass");

	// Registry paths
	namespace Registry
	{
		constexpr LPCTSTR ApplicationSettings = _T("Software\\Empurple\\TabTap");
		constexpr LPCTSTR AutoRun             = _T("Software\\Microsoft\\Windows\\CurrentVersion\\Run");
		constexpr LPCTSTR OSKSettings         = _T("Software\\Microsoft\\Osk");
	}

	// On-Screen Keyboard configuration
	namespace OSK
	{
		constexpr LPCTSTR WindowClass = _T("OSKMainClass");
		constexpr LPCTSTR DefaultPath = _T("%WINDIR%\\System32\\osk.exe");
	}
}



// Main Application Window Manager (Singleton)
class MainWindow
{
public:
	// --- Window display states ---
	enum class ExpansionState
	{
		Collapsed,     // Minimized/narrow state
		Expanded       // Full-width state
	};

	// --- Registry management ---
	struct Registry
	{
		// Query the autorun setting
		static DWORD GetAutostartValue(bool*);
		// Explicitly set or clear the value
		static DWORD SetAutostartValue(bool);
		// Flip value
		static DWORD ToggleAutostartValue(bool* = nullptr);
	};

private:
	// --- Size presets ---
	const SIZE collapsedSize = { 7, 95 };
	const SIZE expandedSize = { 28, 95 };

	// --- Runtime MainWindow::ExpansionState Variables ---
	HWND hMainWnd{};          // Main window handle
	RECT rcMainWnd{};         // Current window rectangle
	ExpansionState expansionState{};  // Current expansion state
	ScreenEdge edgeSide{};    // ScreenEdge where window is snapped

private:
	// --- Construction Control ---
	/// Prevent instantiation, copying, and assignment
	~MainWindow() = default;
	MainWindow() = default;
	MainWindow(const MainWindow&) = delete;
	MainWindow& operator=(const MainWindow&) = delete;

public:
	// --- Singleton access ---
	static MainWindow& Instance();

	// --- Window state management ---
	/// Updates the window's stored rectangle (call after position/size changes)
	static void UpdateWndRect();
	/// Returns the current window rectangle
	static const RECT& GetRect();
	/// Returns the window size for a given state (Collapsed/Expanded)
	static const SIZE& GetSize(const MainWindow::ExpansionState&);
	/// Returns the current window size (based on current state)
	static const SIZE& GetSize();

	// --- ScreenEdge (Screen Side) Management ---
	/// Gets which screen edge the window is attached to (Left/Right)
	static ScreenEdge GetSnapEdge();
	/// Move window to specified edge (Left/Right)
	static bool SetSnapEdge(const ScreenEdge&);
	/// Toggles between Left/Right edges
	static bool ToggleSnapEdge();
	/// Checks if window is on specified edge
	static bool IsSnapEdge(const ScreenEdge&);
	/// 
	static bool IsValidSnapEdge(const ScreenEdge&);

	// --- Expansion MainWindow::ExpansionState Management ---
	/// Gets current expansion state (Collapsed/Expanded)
	static MainWindow::ExpansionState GetExpansionState();
	/// Sets window to specified state (with proper sizing)
	static void SetExpansionState(const MainWindow::ExpansionState&);
	/// Toggles between Collapsed/Expanded states
	static void ToggleExpansionState();
	/// Checks if window is in specified state
	static bool IsExpansionState(const MainWindow::ExpansionState&);

	// --- Position Management ---
	/// Constrains a point to stay within work area boundaries
	static POINT ClampPoint(const POINT&);
	/// Moves window to specified point (clamped to work area)
	static bool SetPosition(const POINT&);
	/// Bring window to top layer
	static void EnforceTopmost();

	// --- Window handle management ---
	/// Sets the HWND for the main window
	static void SetHandle(HWND);
	/// Gets the current window handle
	static HWND GetHandle();
};


// On-Screen Keyboard Window Manager (Singleton)
class OSKWindow
{
private:
	// --- Runtime MainWindow::ExpansionState Variables ---
	HWND hOskWnd{};         // Handle to the OSK window
	RECT rcWndRect{};       // Current window rectangle
	SIZE wndSize{};         // Current window dimensions

private:
	// --- Construction Control ---
	/// Prevent instantiation, copying, and assignment
	OSKWindow() = default;
	~OSKWindow() = default;
	OSKWindow(const OSKWindow&) = delete;
	OSKWindow& operator=(const OSKWindow&) = delete;

public:
	// --- Registry management ---
	struct Registry
	{
		// Query the 'Dock' setting
		static DWORD GetDockModeValue(bool*);
		// Explicitly set or clear the value
		static DWORD SetDockModeValue(bool);
		// Flip value
		static DWORD ToggleDockModeValue(bool* = nullptr);
	};

public:
	// --- Singleton access ---
	static OSKWindow& Instance();

	// --- Window Management ---
	/// Updates the stored window rectangle
	static void UpdateWndRect();
	/// Returns the current window rectangle
	static const RECT& GetRect();
	/// Returns the current window dimensions
	static const SIZE& GetSize();

	// --- Window Handle Management  ---
	/// Sets the OSK window handle
	static void SetHandle(HWND hWnd);
	/// Gets the current OSK window handle
	static HWND GetHandle();

	// --- MainWindow::ExpansionState Management  ---
	static DWORD ToggleDockMode();
};


// Drawing Context Manager
class DrawContext
{
private:
	HWND m_hWnd;
	// --- Component Instances ---
	GDIPlusData* pGdiPlus{};          // GDI+ manager instance for image handling
	BlinkData* pBlinker{};            // Blink effect data
	AnimationData* pAnimator{};       // Animation control data
	EdgeSnapData* pSnapper{};         // ScreenEdge-snapping control data
	WindowDragger* pDragger{};        // Drag operation data

	// --- Operation MainWindow::ExpansionState ---
	Result result{};                  // Operation result storage

private:
	// --- Internal Methods ---
	Result SetResult(Result);
	Result InitializeComponents();

public:
	// --- Lifecycle Management ---
	~DrawContext();
	DrawContext(HWND);
	DrawContext(const DrawContext&) = delete;
	DrawContext& operator=(const DrawContext&) = delete;

	// --- Component Access ---
	GDIPlusData* GdiPlus()     const { return pGdiPlus; };
	BlinkData* Blinker()       const { return pBlinker; };
	AnimationData* Animator()  const { return pAnimator; };
	WindowDragger* Dragger()   const { return pDragger; };
	EdgeSnapData* Snapper()    const { return pSnapper; };

	// --- Drawing Operations ---
	Result DrawImageOnLayeredWindow();

	// --- Status Handling ---
	Result GetResult() const;
};



// --- MainWindow ---

MainWindow& MainWindow::Instance()
{
	static MainWindow instance{};
	return instance;
}

void MainWindow::UpdateWndRect()  // Update after changes
{
	if (!Instance().hMainWnd) { return; }

	GetWindowRect(
		Instance().hMainWnd, &Instance().rcMainWnd
	);
}

const RECT& MainWindow::GetRect()
{
	return Instance().rcMainWnd;
}

const SIZE& MainWindow::GetSize(const ExpansionState& state)
{
	return (state == MainWindow::ExpansionState::Collapsed)
		? Instance().collapsedSize
		: Instance().expandedSize;
}

const SIZE& MainWindow::GetSize()
{
	return GetSize(GetExpansionState());
}

ScreenEdge MainWindow::GetSnapEdge()
{
	return Instance().edgeSide;
}

bool MainWindow::SetSnapEdge(const ScreenEdge& edge)
{
	if (!IsValidSnapEdge(edge)) { return false; }

	if (Instance().hMainWnd) {
		const auto [cx, cy] = GetSize();
		const LONG x = (edge == ScreenEdge::Right)
			? GetSystemMetrics(SM_CXSCREEN) - cx
			: 0;

		POINT pt = ClampPoint({
			x, Instance().rcMainWnd.top
			});

		SetWindowPos(
			Instance().hMainWnd, nullptr,
			pt.x, pt.y,
			cx, cy,
			SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE
		);
	}

	Instance().edgeSide = edge;
	UpdateWndRect();

	return true;
}

bool MainWindow::ToggleSnapEdge()
{
	return SetSnapEdge(GetSnapEdge() == ScreenEdge::Left
		? ScreenEdge::Right
		: ScreenEdge::Left
	);
}

bool MainWindow::IsSnapEdge(const ScreenEdge& edge)
{
	return edge == GetSnapEdge();
}

bool MainWindow::IsValidSnapEdge(const ScreenEdge& edge)
{
	if (edge != ScreenEdge::Left and edge != ScreenEdge::Right) {
		return false;
	}
	if (!WorkAreaManager::IsFreeEdge(edge)) {
		return false;
	}

	return true;
}

MainWindow::ExpansionState MainWindow::GetExpansionState()
{
	return Instance().expansionState;
}

void MainWindow::SetExpansionState(const ExpansionState& state)
{
	const auto& size = (state == MainWindow::ExpansionState::Expanded)
		? Instance().expandedSize
		: Instance().collapsedSize;

	const LONG screenWidth = GetSystemMetrics(SM_CXSCREEN);
	const LONG x = (GetSnapEdge() == ScreenEdge::Right)
		? screenWidth - size.cx
		: 0;

	if (Instance().hMainWnd) {
		SetWindowPos(Instance().hMainWnd, nullptr,
			x, Instance().rcMainWnd.top,
			size.cx, size.cy,
			SWP_NOZORDER | SWP_NOACTIVATE
		);
	}
	Instance().expansionState = state;
	UpdateWndRect();
}

void MainWindow::ToggleExpansionState()
{
	SetExpansionState(GetExpansionState() == MainWindow::ExpansionState::Collapsed
		? MainWindow::ExpansionState::Expanded
		: MainWindow::ExpansionState::Collapsed
	);
}

bool MainWindow::IsExpansionState(const MainWindow::ExpansionState& state)
{
	return state == GetExpansionState();
}

POINT MainWindow::ClampPoint(const POINT& point)
{
	return {
		// X-axis constraint
		std::clamp(
			point.x,
			WorkAreaManager::GetWorkArea().left,
			WorkAreaManager::GetWorkArea().right - GetSize().cx
		),
			// Y-axis constraint
			std::clamp(
				point.y,
				WorkAreaManager::GetWorkArea().top,
				WorkAreaManager::GetWorkArea().bottom - GetSize().cy
			)
	};
}

bool MainWindow::SetPosition(const POINT& point)
{
	// Clamp top position
	LONG newTop = ClampPoint({ point }).y;

	// Update window rectangle manually
	Instance().rcMainWnd.top = newTop;
	Instance().rcMainWnd.bottom = newTop + GetSize().cy;

	return SetWindowPos(
		Instance().hMainWnd, NULL,
		GetRect().left, GetRect().top,
		0, 0,
		SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE
	);
}

void MainWindow::EnforceTopmost()
{
	SetWindowPos(
		Instance().hMainWnd, HWND_TOPMOST,
		0, 0, 0, 0,
		SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE
	);
}

void MainWindow::SetHandle(HWND hWnd)
{
	Instance().hMainWnd = hWnd;
}

HWND MainWindow::GetHandle()
{
	return Instance().hMainWnd;
}


// --- MainWindow::Registry ---

DWORD MainWindow::Registry::GetAutostartValue(bool* pRetVal)
{
	// Check if autostart is enabled in the registry
	DWORD dwResult =
		RegistryManager{
			false, Config::Registry::AutoRun
		}.QueryValue(Config::ApplicationName);

	if (dwResult == ERROR_SUCCESS) {
		*pRetVal = true;
	}
	else if (dwResult != ERROR_FILE_NOT_FOUND) {
		return dwResult;
	}
	else { *pRetVal = false; }

	return ERROR_SUCCESS;
}

DWORD MainWindow::Registry::SetAutostartValue(bool enable)
{
	if (enable) {
		TCHAR pathBuffer[MAX_PATH + 2] = { _T('"') };
		DWORD len = GetModuleFileName(nullptr, pathBuffer + 1, MAX_PATH);
		if (!len or len >= MAX_PATH) {
			return ERROR_INSUFFICIENT_BUFFER;
		}
		pathBuffer[len + 1] = _T('"');

		return
			RegistryManager{
				false, Config::Registry::AutoRun
			}.WriteString(Config::ApplicationName, pathBuffer);
	}
	else {
		return
			RegistryManager{
				false, Config::Registry::AutoRun
			}.RemoveValue(Config::ApplicationName);
	}
}

DWORD MainWindow::Registry::ToggleAutostartValue(bool* pRetVal)
{
	bool isAutostartEnabled{};
	DWORD dwResult;

	// Check if autostart is enabled in the registry
	dwResult = GetAutostartValue(&isAutostartEnabled);
	if (dwResult != ERROR_SUCCESS) {
		return dwResult;
	}

	dwResult = SetAutostartValue(!isAutostartEnabled);
	if (pRetVal and dwResult == ERROR_SUCCESS) {
		*pRetVal = !isAutostartEnabled;
	}

	return dwResult;
}



// --- OSKWindow ---

OSKWindow& OSKWindow::Instance()
{
	static OSKWindow instance{};
	return instance;
}

void OSKWindow::UpdateWndRect()
{
	GetWindowRect(
		Instance().hOskWnd, &Instance().rcWndRect
	);

	Instance().wndSize = {
		Instance().rcWndRect.right - Instance().rcWndRect.left,
		Instance().rcWndRect.bottom - Instance().rcWndRect.top
	};
}

const RECT& OSKWindow::GetRect()
{
	return Instance().rcWndRect;
}

const SIZE& OSKWindow::GetSize()
{
	return Instance().wndSize;
}

void OSKWindow::SetHandle(HWND hWnd)
{
	Instance().hOskWnd = hWnd;
}

HWND OSKWindow::GetHandle()
{
	return Instance().hOskWnd;
}

DWORD OSKWindow::ToggleDockMode()
{
	bool isDockMode{};
	DWORD dwResult = Registry::ToggleDockModeValue(&isDockMode);
	if (dwResult != ERROR_SUCCESS) {
		return dwResult;
	}

	UINT msgID = isDockMode ? ID_APP_DOCKMODE : ID_APP_REGULARMODE;
	PostMessage(OSKWindow::GetHandle(), WM_APP_CUSTOM_MESSAGE,
		MAKEWPARAM(msgID, 0), (LPARAM)MainWindow::GetHandle());

	return dwResult;
}


// --- OSKWindow::Registry ---

DWORD OSKWindow::Registry::GetDockModeValue(bool* pRetVal)
{
	// Check if 'Dock' is enabled in the registry
	DWORD dwData;
	DWORD dwResult = 
		RegistryManager{
			false, Config::Registry::OSKSettings
		}.ReadDWORD(_T("Dock"), &dwData);
		
	if (dwResult == ERROR_SUCCESS) {
		*pRetVal = (dwData == 1);
	}

	return dwResult;
}

DWORD OSKWindow::Registry::SetDockModeValue(bool enable)
{
	DWORD newVal = enable ? 1 : 0;

	return
		RegistryManager{
			false, Config::Registry::OSKSettings
		}.WriteDWORD(_T("Dock"), newVal);
}

DWORD OSKWindow::Registry::ToggleDockModeValue(bool* pRetVal)
{
	bool isDockEnabled{};
	DWORD dwResult;

	// Check if 'Dock' is enabled in the registry
	dwResult = GetDockModeValue(&isDockEnabled);
	if (dwResult != ERROR_SUCCESS) {
		return dwResult;
	}

	dwResult = SetDockModeValue(!isDockEnabled);
	if (pRetVal and dwResult == ERROR_SUCCESS) {
		*pRetVal = !isDockEnabled;
	}
	
	return dwResult;
}



// --- DrawContext ---

Result DrawContext::SetResult(Result res)
{
	result = std::move(res);
	return std::move(res);
}

Result DrawContext::InitializeComponents()
{
	pGdiPlus = new GDIPlusData{};
	if (!GdiPlus()->GetResult().success) {
		return SetResult(
			GdiPlus()->GetResult());
	}

	pDragger = new WindowDragger{};
	pSnapper = new EdgeSnapData{};
	pAnimator = new AnimationData{};
	pBlinker = new BlinkData{};

	return {};
}

DrawContext::~DrawContext()
{
	delete Snapper();
	delete Dragger();
	delete Animator();
	delete Blinker();
	delete GdiPlus();
}

DrawContext::DrawContext(HWND hWnd) :
	m_hWnd{ hWnd }
{
	InitializeComponents();
}

Result DrawContext::DrawImageOnLayeredWindow()
{
	HDC hdcScreen{};
	HDC hdcMem{};
	HBITMAP hBitmap{};
	HBITMAP hOldBmp{}; // No cleanup needed
	Gdiplus::Graphics* pGraphics{};

	auto cleanup = [&]() {
		if (pGraphics) {
			delete pGraphics; // Delete GDI+ Graphics object
		}
		if (hdcMem) {
			if (hOldBmp) { // Select original bitmap back if needed
				SelectObject(hdcMem, hOldBmp);
			}
			if (hBitmap) { // Delete the DIB section we created
				DeleteObject(hBitmap);
			}
			DeleteDC(hdcMem); // Delete the memory DC
		}
		if (hdcScreen) {
			ReleaseDC(NULL, hdcScreen); // Release the screen DC
		}
		};

	// --- Get required resources and check for errors ---

	SIZE mainSize = MainWindow::GetSize();
	hdcScreen = GetDC(NULL);
	if (!hdcScreen) {
		// No resources allocated yet, so just return
		return SetResult({ GetLastError(),
			_T("Failed to get Screen DC") });
	}

	hdcMem = CreateCompatibleDC(hdcScreen);
	if (!hdcMem) {
		DWORD lastError = GetLastError();
		cleanup(); // Cleans up hdcScreen
		return SetResult({ lastError,
			_T("Failed to create memory DC") });
	}

	// --- Create DIB Section ---

	BITMAPINFO bmi{};
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = mainSize.cx;
	bmi.bmiHeader.biHeight = -mainSize.cy; // Negative height for top-down bitmap
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 32;
	bmi.bmiHeader.biCompression = BI_RGB;

	PVOID pvBits{};  // Not used directly here, but required by CreateDIBSection
	hBitmap = CreateDIBSection(hdcMem, &bmi, DIB_RGB_COLORS, &pvBits, NULL, 0);
	if (!hBitmap) {
		DWORD lastError = GetLastError();
		cleanup(); // Cleans up hdcMem, hdcScreen
		return SetResult({ lastError,
			_T("Failed to create DIB section") });
	}

	hOldBmp = (HBITMAP)SelectObject(hdcMem, hBitmap);
	if (!hOldBmp || hOldBmp == HGDI_ERROR) {
		DWORD lastError = GetLastError();
		// Note: hBitmap is already selected, cleanup needs to handle it
		hOldBmp = nullptr; // Prevent SelectObject(hdcMem, hOldBmp) in cleanup
		cleanup(); // Cleans up hBitmap, hdcMem, hdcScreen
		return SetResult({ lastError,
			_T("Failed to select bitmap into memory DC") });
	}

	// --- GDI+ Operations ---

	pGraphics = new Gdiplus::Graphics(hdcMem);
	Gdiplus::Status status = pGraphics->GetLastStatus();
	if (status != Gdiplus::Ok) {
		delete pGraphics; // Manually delete graphics before cleanup call
		pGraphics = nullptr; // Prevent double deletion in cleanup
		cleanup(); // Cleans up hOldBmp/hBitmap, hdcMem, hdcScreen
		return SetResult({ status,
			_T("Failed to initialize GDI+ Graphics") });
	}

	// Set rendering quality
	pGraphics->SetCompositingMode(Gdiplus::CompositingModeSourceOver);
	pGraphics->SetCompositingQuality(Gdiplus::CompositingQualityHighQuality);
	pGraphics->SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
	pGraphics->SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);

	// Clear with full transparency
	pGraphics->Clear(Gdiplus::Color(0, 0, 0, 0));

	// --- Prepare for Drawing ---

	if (!GdiPlus() || !GdiPlus()->GetImage() || !Blinker()) {
		cleanup();
		return SetResult({ 0,
			_T("Invalid MainWindow::eState"), _T("Required GDI+ or Blink data is missing") });
	}

	INT srcLeft = (MainWindow::IsExpansionState(MainWindow::ExpansionState::Expanded))
		? 0
		: GdiPlus()->GetImage()->GetWidth() - mainSize.cx;

	// Select attributes based on blink state
	Gdiplus::ImageAttributes* pAttributes = GdiPlus()->GetImageAttributes();
	GdiPlus()->ApplyColorMatrix(Blinker()->GetActiveColorMatrix());

	// Destination rectangle calculation
	Gdiplus::Rect destRect = (MainWindow::IsSnapEdge(ScreenEdge::Right))
		? Gdiplus::Rect(mainSize.cx, 0, -mainSize.cx, mainSize.cy)  // Flipped horizontally
		: Gdiplus::Rect(0, 0, mainSize.cx, mainSize.cy);  // Normal

	// --- Draw the image ---

	status = pGraphics->DrawImage(
		GdiPlus()->GetImage(),
		destRect,
		srcLeft, 0,                // Source X, Y
		mainSize.cx, mainSize.cy,  // Source Width, Height
		Gdiplus::UnitPixel,
		pAttributes
	);

	if (status != Gdiplus::Ok) {
		cleanup(); // Cleans up graphics, hOldBmp/hBitmap, hdcMem, hdcScreen
		return SetResult({ status,
			_T("GDI+ DrawImage failed") });
	}

	// --- Update Layered Window ---

	BLENDFUNCTION blendFunc = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
	POINT ptSrc = { 0, 0 }; // Source point in the memory DC (usually 0,0)
	POINT ptDst = { MainWindow::GetRect().left, MainWindow::GetRect().top }; // Destination point on screen

	HWND hWndMain = MainWindow::GetHandle();
	if (!hWndMain) {
		cleanup();
		return SetResult({ 0,
			_T("Invalid Window"), _T("Main window handle is null") });
	}

	BOOL bSuccess = UpdateLayeredWindow(
		hWndMain,
		hdcScreen,    // Use screen DC for destination context
		&ptDst,       // Screen position to update window to
		&mainSize,    // Size of the window
		hdcMem,       // Source DC with the bitmap content
		&ptSrc,       // Point in the source DC to start copying from
		0,            // Color key (not used with AC_SRC_ALPHA)
		&blendFunc,   // Blending function
		ULW_ALPHA     // Use alpha channel for blending
	);

	if (!bSuccess) {
		DWORD lastError = GetLastError();
		cleanup(); // Cleans up graphics, hOldBmp/hBitmap, hdcMem, hdcScreen
		return SetResult({ lastError,
			_T("Update layered window failed") });
	}

	cleanup();
	return {};
}

Result DrawContext::GetResult() const
{
	return result;
}



// Snap adapter for main window, handles edge-snapping logic
struct MainSnapAdapter : public ISnapAdapter
{
	HWND GetTargetWindow() const override
	{
		return MainWindow::GetHandle();
	}

	ScreenEdge GetSnapEdge() const override
	{
		return MainWindow::GetSnapEdge();
	}

	bool IsValidSnapEdge(const ScreenEdge& edge) const override
	{
		return MainWindow::IsValidSnapEdge(edge);
	}

	void OnSnapSuccess(const ScreenEdge& edge, const POINT& pos) override
	{
		MainWindow::SetSnapEdge(edge);
		MainWindow::SetPosition(pos);
	}

	void OnSnapRejected(const ScreenEdge&, const POINT&) override
	{
		HWND hWnd = MainWindow::GetHandle();
		if (hWnd) {
			DrawContext* pContext = (DrawContext*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
			if (pContext) {
				pContext->Blinker()->Enable(hWnd, IDT_BLINK_TIMER);
			}
		}
	}

	void OnSnapEdgeNone(const POINT& pos) override
	{
		HWND hWnd = MainWindow::GetHandle();
		if (hWnd) {
			DrawContext* pContext = (DrawContext*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
			if (pContext->Snapper()->IsPreviewEnabled()) {
				MainWindow::SetPosition(pos);
			}
		}
	}
};

// System tray setup adapter
struct TraySetupAdapter : public ITrayAdapter
{
	HICON GetIconResource()
	{
		TCHAR szBuffer[MAX_PATH];
		// Expand path to on-screen keyboard
		if (!ExpandEnvironmentStrings(Config::OSK::DefaultPath, szBuffer, MAX_PATH)) {
			return NULL;
		}

		HICON trayIcon = ExtractIcon(NULL, szBuffer, 0);  // First icon in the EXE
		if (!trayIcon) { return NULL; }

		return trayIcon;
	}

	Result SetupTrayIcon(NOTIFYICONDATA& trayData) override
	{
		trayData.hIcon = GetIconResource();                 // Specifies the icon to display in the tray (paired with NIF_ICON)
		if (!trayData.hIcon) {
			return { GetLastError(),
				_T("Failed to load tray icon") };
		}

		_tcscpy_s(trayData.szTip, Config::ApplicationName);  // Sets the tooltip text (paired with NIF_TIP)
		trayData.cbSize = sizeof(NOTIFYICONDATA);            // Specifies the size of the structure, required by Shell_NotifyIcon
		trayData.hWnd = MainWindow::GetHandle();             // Associates the tray icon with a window to receive callback messages
		trayData.uID = IDI_NOTIFY_ICON;                      // A unique identifier for the tray icon
		trayData.uCallbackMessage = WM_APP_TRAYICON;         // Defines the custom message
		trayData.uFlags =
			NIF_ICON |      // The tray icon
			NIF_MESSAGE |   // Event handling
			NIF_TIP |       // Tooltip text
			NIF_SHOWTIP;    // Ensures the tooltip appears when hovering (Windows 10/11 enhancement)

		// Required for Windows 10/11
		trayData.dwState = NIS_SHAREDICON;                   // Indicates the icon is shared and shouldn’t be deleted when the notification is removed
		trayData.dwStateMask = NIS_SHAREDICON;               // Specifies which state bits to modify

		// First add the icon to the tray
		if (!Shell_NotifyIcon(NIM_ADD, &trayData)) {         // Adds the tray icon to the system tray
			return { GetLastError(),
				_T("Failed to initialize tray icon") };
		}
		// Set version AFTER adding
		trayData.uVersion = NOTIFYICON_VERSION;              // Sets the tray icon version

		if (!Shell_NotifyIcon(NIM_SETVERSION, &trayData)) {
			return { GetLastError(),
				_T("Failed to set notify icon version") };
		}

		return {};

	}

	Result CreateTrayMenu(HMENU& hMenu) override
	{
		hMenu = CreatePopupMenu();
		if (!hMenu) {
			return { ERROR_OUTOFMEMORY,
				_T("Tray Menu memory allocation failed") };
		}

		bool isAutostartEnabled{};
		DWORD dwResult;

		// Check if autostart is enabled in the registry
		dwResult = MainWindow::Registry::GetAutostartValue(&isAutostartEnabled);
		if (dwResult != ERROR_SUCCESS) {
			return { dwResult,
			_T("Failed to get Main registry data") };
		}
		
		bool isDockModeEnabled{};
		DWORD dwValue{};

		// Check if `dock` mode is enabled in the registry
		dwResult = OSKWindow::Registry::GetDockModeValue(&isDockModeEnabled);
		if (dwResult != ERROR_SUCCESS) {
			return { dwResult,
				_T("Failed to get OSK registry data") };
		}

		AppendMenu(hMenu, MF_STRING | (isAutostartEnabled ? MF_CHECKED : 0), IDM_TRAY_AUTOSTART, _T("Autostart"));
		AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
		AppendMenu(hMenu, MF_STRING | (isDockModeEnabled ? MF_CHECKED : 0), IDM_TRAY_DOCKMODE, _T("Forced Dock mode"));
		AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
		AppendMenu(hMenu, MF_STRING, IDM_TRAY_EXIT, _T("Exit"));

		return {};
	}
};



// Forward declarations
LRESULT CALLBACK WindowProc(HWND, UINT, WPARAM, LPARAM);


// Synchronizes position with main application window
void SyncOskPositionWithMain()
{
	OSKWindow::UpdateWndRect();

	const RECT& oskRect = OSKWindow::GetRect();
	const SIZE& oskSize = OSKWindow::GetSize();
	const RECT& mainRect = MainWindow::GetRect();
	const SIZE& mainSize = MainWindow::GetSize();


	LONG targetTop = mainRect.top - (oskSize.cy - mainSize.cy) / 2;

	DWORD clampedTop =
		std::clamp(
			targetTop,
			WorkAreaManager::GetWorkArea().top,
			WorkAreaManager::GetWorkArea().bottom - oskSize.cy
		);

	SetWindowPos(
		OSKWindow::GetHandle(), NULL,
		oskRect.left, clampedTop,
		0, 0,
		SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE
	);
}

// Create Window
HWND CreateLayeredWindow(HINSTANCE hInstance)
{
	POINT pt{};
	const auto [cx, cy] = MainWindow::GetSize();

	WorkAreaManager::Refresh();

	// Adjust window X position
	if (WorkAreaManager::IsFreeEdge(ScreenEdge::Left)) {
		MainWindow::SetSnapEdge(ScreenEdge::Left);
		pt.x = WorkAreaManager::GetWorkArea().left;
	}
	else {
		MainWindow::SetSnapEdge(ScreenEdge::Right);
		pt.x = WorkAreaManager::GetWorkArea().right - cx;
	}

	// Adjust position according on-screen keyboard
	OSKWindow::UpdateWndRect();

	LONG oskWndHeight = OSKWindow::GetRect().bottom - OSKWindow::GetRect().top;
	pt.y = OSKWindow::GetRect().top + (oskWndHeight - cy) / 2;

	pt = MainWindow::ClampPoint(pt);

	return
		CreateWindowEx(
			WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
			Config::MainWindowClass,
			Config::ApplicationName,
			WS_POPUP,
			pt.x, pt.y,
			cx, cy,
			NULL, NULL, hInstance, NULL
		);
}

// Register Window Class
ATOM RegisterWindowClass(HINSTANCE hInstance, WNDCLASSEX* wcex)
{
	wcex->cbSize = sizeof(WNDCLASSEX);
	wcex->lpfnWndProc = WindowProc;                  // Window procedure
	wcex->hInstance = hInstance;                     // App instance
	wcex->lpszClassName = Config::MainWindowClass;   // Class name
	wcex->hCursor = LoadCursor(NULL, IDC_ARROW);     // Cursor
	wcex->hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);            // Background
	//wcex.hIconSm = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_MAIN_ICON));  // Icon
	wcex->style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;

	return RegisterClassEx(wcex);
}

// Creates a process to launch the On-Screen Keyboard (OSK) application
BOOL CreateOSKProcess(STARTUPINFO* pStartupInfo, PROCESS_INFORMATION* pProcessInfo)
{
	TCHAR szBuffer[MAX_PATH];
	pStartupInfo->cb = sizeof(STARTUPINFO);
	pStartupInfo->dwFlags = STARTF_USESHOWWINDOW;
	pStartupInfo->wShowWindow = SW_HIDE;

	// Expand path to on-screen keyboard
	if (!ExpandEnvironmentStrings(Config::OSK::DefaultPath, szBuffer, MAX_PATH)) {
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



// Window procedure
LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg,
	WPARAM wParam, LPARAM lParam)
{
	static MouseTracker mouseTracker{ hWnd };
	static DrawContext* pDrawContext{};
	static TrayManager* pTray;


	switch (uMsg)
	{

	case WM_TIMER:
	{
		if (wParam == IDT_KEEP_ON_TOP) {
			MainWindow::EnforceTopmost();
			return 0;
		}

		if (wParam == IDT_BLINK_TIMER) {
			pDrawContext->Blinker()->Update();
			pDrawContext->DrawImageOnLayeredWindow();
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
		auto DragCallback = [&hWnd](const POINT& pt, nullptr_t) {
			return MainWindow::SetPosition(pt);
			};
		
		if (!pDrawContext->Dragger()->OnMouseMove(DragCallback, nullptr) and 
			!pDrawContext->Snapper()->OnMouseMove() and
			MainWindow::IsExpansionState(MainWindow::ExpansionState::Collapsed)
			) {
			MainWindow::ToggleExpansionState();
			pDrawContext->DrawImageOnLayeredWindow();
		}

		mouseTracker.OnMouseMove();

		break;
	}

	case WM_MOUSEWHEEL:
	{
		int delta = GET_WHEEL_DELTA_WPARAM(wParam);
		PostMessage(OSKWindow::GetHandle(), WM_APP_CUSTOM_MESSAGE,
			MAKEWPARAM(ID_APP_FADE, delta > 0),
			0
		);

		break;
	}

	case WM_LBUTTONDOWN:
	{
		// Update work area to avoid taskbar during drag
		WorkAreaManager::Refresh();

		pDrawContext->Snapper()->Enable(new MainSnapAdapter{});
		SetCapture(hWnd);

		return 0;
	}

	case WM_LBUTTONUP:
	{
		if (pDrawContext->Snapper()->IsEnabled()) {
			ReleaseCapture();
			bool isPreview = pDrawContext->Snapper()->IsPreviewEnabled();
			pDrawContext->Snapper()->Disable();

			if (isPreview) {
				pDrawContext->DrawImageOnLayeredWindow();
				return 0;
			}
		}

		HWND hOskWnd = OSKWindow::GetHandle();
		if (IsWindowVisible(hOskWnd)) {
			PostMessage(hOskWnd, WM_CLOSE, 0, 0);
		}
		else {
			SyncOskPositionWithMain();
			ShowWindowAsync(hOskWnd, SW_RESTORE); // Restore OSK
		}
		break;
	}

	case WM_RBUTTONDOWN:
	{
		// Update work area to avoid taskbar during drag
		WorkAreaManager::Refresh();

		// Start drag operation
		pDrawContext->Dragger()->Enable(hWnd);

		break;
	}

	case WM_RBUTTONUP:
	{
		// Stop drag operation
		pDrawContext->Dragger()->Disable();
		ReleaseCapture();

		break;
	}

	case WM_RBUTTONDBLCLK:
	{
		if (IsWindowVisible(OSKWindow::GetHandle())) {
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

	case WM_MOUSELEAVE:
	{
		mouseTracker.OnMouseLeave();

		if (!pDrawContext->Dragger()->IsEnabled() and !pDrawContext->Snapper()->IsEnabled())
		{
			if (MainWindow::IsExpansionState(MainWindow::ExpansionState::Expanded)) {
				MainWindow::SetExpansionState(MainWindow::ExpansionState::Collapsed);

				pDrawContext->DrawImageOnLayeredWindow();
			}
		}
		break;
	}

	case WM_APP_TRAYICON:
	{
		if (lParam == WM_LBUTTONUP) {
			SendMessage(hWnd, WM_LBUTTONUP, 0, 0);
		}
		else if (lParam == WM_RBUTTONUP) {
			pTray->HandleTrayRightClick();
		}

		break;
	}

	case WM_COMMAND:
	{
		WORD wNotificationCode = HIWORD(wParam);
		WORD wCommandId = LOWORD(wParam);

		if (wNotificationCode == 0) {  // Menu/accelerator
			if (wCommandId == IDM_TRAY_DOCKMODE) {
				DWORD dwResult = OSKWindow::ToggleDockMode();
				if (dwResult != ERROR_SUCCESS) {
					MessageBoxNotifier{
						{ _T("Registry Error") },
						{ _T("Failed to get OSK registry data." EOL_ "%lu"), dwResult }
					}.ShowError(hWnd);
					return 1;
				}
			}

			else if (wCommandId == IDM_TRAY_AUTOSTART) {
				DWORD dwResult = MainWindow::Registry::ToggleAutostartValue();
				if (dwResult != ERROR_SUCCESS) {
					MessageBoxNotifier{
						{ _T("Registry Error") },
						{ _T("Failed to get Main registry data." EOL_ "%lu"), dwResult }
					}.ShowError(hWnd);
					return 1;
				}
			}

			else if (wCommandId == IDM_TRAY_EXIT) {
				PostMessage(hWnd, WM_CLOSE, 0, 0);
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
			OSKWindow::UpdateWndRect();

			const RECT& oskRect = OSKWindow::GetRect();
			const SIZE& oskSize = OSKWindow::GetSize();
			const SIZE& mainSize = MainWindow::GetSize();
			const RECT& workArea = WorkAreaManager::GetWorkArea();

			LONG targetY = oskRect.top + (oskSize.cy - mainSize.cy) / 2;
			const LONG clampedY =
				std::clamp(targetY, workArea.top, workArea.bottom - mainSize.cy
				);

			POINT ptCursor;
			GetCursorPos(&ptCursor);

			// Collapse the main window if the cursor moves away
			if (ptCursor.y < clampedY or
				ptCursor.y > clampedY + mainSize.cy)
			{
				SendMessage(hWnd, WM_MOUSELEAVE, 0, 0);
			}

			const POINT ptTarget{
				MainWindow::GetRect().left,  // No changes
				clampedY
			};

			// Start animation with reference to target window position
			pDrawContext->Animator()->Enable(
				MainWindow::GetHandle(), ptTarget);

			if (pDrawContext->Animator()->IsEnabled()) {
				PostMessage(hWnd, WM_APP_CUSTOM_MESSAGE, MAKEWPARAM(ID_APP_ANIMATION, 0), 0);
			}

			return 0;
		}

		if (wCommandId == ID_APP_ANIMATION) {
			bool bContinue = pDrawContext->Animator()->Update();
			if (bContinue) {  // Animation needs to continue
				PostMessage(hWnd, WM_APP_CUSTOM_MESSAGE, MAKEWPARAM(ID_APP_ANIMATION, 0), 0);
			}
			else {
				MainWindow::UpdateWndRect();
			}

			return 0;
		}

		return 1;
	}

	case WM_CREATE:
	{
		// Store the main window handle
		MainWindow::SetHandle(hWnd);

		// Update window position
		MainWindow::UpdateWndRect();

		// Create the drawing context
		pDrawContext = new DrawContext{ hWnd };
		
		Result result = pDrawContext->GetResult();
		if (!result) {
			MessageBoxNotifier{
				{ result.header },
				{ _T("%s" EOL_ "%lu"), result.message, result.errorValue }
			}.ShowError(hWnd);
			return -1;
		}

		// Store the drawing context in window user data
		SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)pDrawContext);

		// Create the tray icon manager
		pTray = new TrayManager(new TraySetupAdapter{});

		result = pTray->GetResult();
		if (!result) {
			MessageBoxNotifier{
				{ result.header },
				{ _T("%s" EOL_ "%lu"), result.message, result.errorValue }
			}.ShowError(hWnd);
			return -1;
		}

		// Keep window always on top
		SetTimer(hWnd, IDT_KEEP_ON_TOP, 5000, NULL); // 5 sec interval

		// Apply system theme
		if (!ThemeManager::FollowSystemTheme(hWnd)) {
			MessageBoxNotifier{
				{ _T("Theme Error") },
				{ _T("Failed to apply system theme." EOL_ "%lu"), GetLastError() }
			}.ShowWarning(hWnd);
		}

		// Explicitly show the window
		ShowWindow(hWnd, SW_SHOW);
		// Draw content on the layered window
		pDrawContext->DrawImageOnLayeredWindow();

		return 0;
	}

	case WM_DESTROY:
	{
		// Move then Close OSK to store position correctly
		ShowWindow(OSKWindow::GetHandle(), SW_HIDE);
		SyncOskPositionWithMain();
		PostMessage(OSKWindow::GetHandle(), WM_CLOSE, 0, (LPARAM)TRUE);

		// Kill timers
		KillTimer(hWnd, IDT_KEEP_ON_TOP);
		KillTimer(hWnd, IDT_BLINK_TIMER);

		// Clean up tray manager object
		delete pTray;

		// Clean up drawing context object
		delete pDrawContext;

		PostQuitMessage(0);
		return 0;
	}

	case WM_SETTINGCHANGE:
	{
		// Check if the setting change is related to the system theme
		if (lParam and CompareStringOrdinal(reinterpret_cast<LPCWCH>(lParam),
			-1, _T("ImmersiveColorSet"), -1, TRUE) == CSTR_EQUAL)
		{
			// Re-apply the correct theme based on current system setting
			ThemeManager::FollowSystemTheme(hWnd);
			break;
		}

		// Update information about the usable screen area
		WorkAreaManager::Refresh();

		// Lambda function to update window position, enable blinker, and redraw
		auto UpdatePosition = [&](ScreenEdge edge) {
			MainWindow::SetSnapEdge(edge);
			pDrawContext->Blinker()->Enable(hWnd, IDT_BLINK_TIMER);
			pDrawContext->DrawImageOnLayeredWindow();
			};

		// Determine if we need to switch sides
		if (MainWindow::IsSnapEdge(ScreenEdge::Left) and
			!WorkAreaManager::IsFreeEdge(ScreenEdge::Left))
		{
			UpdatePosition(ScreenEdge::Right);
		}
		else if (MainWindow::IsSnapEdge(ScreenEdge::Right) and
			!WorkAreaManager::IsFreeEdge(ScreenEdge::Right))
		{
			UpdatePosition(ScreenEdge::Left);
		}
		else {
			const RECT& rect = MainWindow::GetRect();
			auto [cx, cy] = MainWindow::ClampPoint({
				rect.left, rect.top });

			// Update position if clamping changed the top coordinate
			if (rect.top != cy) {
				UpdatePosition(MainWindow::GetSnapEdge());
			}
		}
		break;
	}

	default: break;
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
	HWND hWnd{};

	// Check if program is already running
	if (hWnd = FindWindow(Config::MainWindowClass, NULL)) {
		PostMessage(hWnd, WM_LBUTTONUP, 0, 0);
		return ERROR_ALREADY_EXISTS;
	}

	// Close OSK if open
	if (hWnd = FindWindow(Config::OSK::WindowClass, NULL)) {
		SendMessage(hWnd, WM_CLOSE, 0, (LPARAM)TRUE);  // lParam forces custom close
	}

	HMODULE hDll{};

#ifndef _DEBUG
	if (!LoadHookDll(&hDll)) {
		MessageBoxNotifier{
			{ _T("System Error") },
			{ _T("Failed to load the DLL." EOL_ "%lu"), GetLastError() }
		}.ShowError(hWnd);
		return -1;
	}

	// Function pointer types for the exported functions
	typedef BOOL(*UninstallHookFunc)();
	typedef BOOL(*InstallHookFunc)(DWORD);

	UninstallHookFunc UninstallHook = (UninstallHookFunc)GetProcAddress(hDll, "UninstallHook");
	InstallHookFunc InstallHook = (InstallHookFunc)GetProcAddress(hDll, "InstallHook");

	if (!UninstallHook or !InstallHook) {
		MessageBoxNotifier{
			{ _T("System Error") },
			{ _T("The specified procedure could not be found." EOL_ "%lu"), ERROR_PROC_NOT_FOUND }
		}.ShowError(hWnd);
		FreeLibrary(hDll);
		return -1;
	}
#endif

	STARTUPINFO startupInfo{};
	PROCESS_INFORMATION processInfo{};
	WNDCLASSEX wcex{};
	HANDLE hEvent{};
	DWORD waitResult{};

#ifndef _DEBUG
	// Create OSK Process
	if (!CreateOSKProcess(&startupInfo, &processInfo)) {
		MessageBoxNotifier{
			{ _T("System Error") },
			{ _T("Unable to create OSK window." EOL_ "%lu"), GetLastError() }
		}.ShowError(hWnd);
		goto CLEANUP;
	}
	if (!processInfo.hProcess) { goto CLEANUP; } // warnings C6387

	waitResult = WaitForInputIdle(processInfo.hProcess, 3000);
	if (waitResult == WAIT_TIMEOUT) {
		MessageBoxNotifier{
			{ _T("System Error") },
			{ _T("The wait time-out interval elapsed." EOL_ "%lu"), WAIT_TIMEOUT }
		}.ShowError(hWnd);
		goto CLEANUP;
	}
	if (waitResult == WAIT_FAILED) {
		MessageBoxNotifier{
			{ _T("System Error") },
			{ _T("Failed to wait for the process." EOL_ "%lu"), WAIT_FAILED }
		}.ShowError(hWnd);
		goto CLEANUP;
	}


	// Hook Inject
	// ==============================

	// Create a manual-reset event that starts unsignaled
	if (!(hEvent = CreateEvent(NULL, TRUE, FALSE, _T("OSKLoadEvent")))) {
		MessageBoxNotifier{
			{ _T("System Error") },
			{ _T("Failed to create event." EOL_ "%lu"), GetLastError() }
		}.ShowError(hWnd);
		goto CLEANUP;
	}

	// Inject hook
	if (!InstallHook(processInfo.dwThreadId)) {
		MessageBoxNotifier{
			{ _T("System Error") },
			{ _T("Failed to install the Windows hook procedure." EOL_ "%lu"), GetLastError() }
		}.ShowError(hWnd);
		goto CLEANUP;
	}

	// Wait for the event to be signaled
	waitResult = WaitForSingleObject(hEvent, 3000);
	if (waitResult == WAIT_TIMEOUT) {
		MessageBoxNotifier{
			{ _T("System Error") },
			{ _T("The wait time-out interval elapsed." EOL_ "%lu"), WAIT_TIMEOUT }
		}.ShowError(hWnd);
		goto CLEANUP;
	}
	if (waitResult == WAIT_FAILED) {
		MessageBoxNotifier{
			{ _T("System Error") },
			{ _T("Failed to wait for the process." EOL_ "%lu"), WAIT_FAILED }
		}.ShowError(hWnd);
		goto CLEANUP;
	}

	// CBTHook.dll confirmed successful injection; Safe to close the event
	if (!CloseHandle(hEvent)) {
		MessageBoxNotifier{
			{ _T("System Error") },
			{ _T("Failed to close event." EOL_ "%lu"), GetLastError() }
		}.ShowError(hWnd);
		hEvent = {};
		goto CLEANUP;
	}
	hEvent = {};

	// Unload hook
	if (!UninstallHook()) {
		MessageBoxNotifier{
			{ _T("System Error") },
			{ _T("Failed to remove the Windows hook." EOL_ "%lu"), ERROR_HOOK_NOT_INSTALLED }
		}.ShowError(hWnd);
		goto CLEANUP;
	}

	// Store OSK handle globally
	if (!(hWnd = FindWindow(Config::OSK::WindowClass, NULL))) {
		MessageBoxNotifier{
			{ _T("System Error") },
			{ _T("Failed to find the window." EOL_ "%lu"), GetLastError() }
		}.ShowError(hWnd);
		goto CLEANUP;
	}

	OSKWindow::SetHandle(hWnd);

	// Unload library
	FreeLibrary(hDll);
	hDll = {};
#endif

	if (!ThemeManager::EnableThemeSupport()) {
		MessageBoxNotifier{
			{ _T("Theme Error") },
			{ _T("Failed to enable theme support." EOL_ "%lu"), GetLastError() }
		}.ShowWarning(hWnd);
	}

	if (!RegisterWindowClass(hInstance, &wcex)) {
		MessageBoxNotifier{
			{ _T("System Error") },
			{ _T("Unable to register window class." EOL_ "%lu"), GetLastError() }
		}.ShowError(hWnd);
		goto CLEANUP;
	}

	hWnd = CreateLayeredWindow(hInstance);
	if (!hWnd) {
		MessageBoxNotifier{
			{ _T("System Error") },
			{ _T("Unable to create window." EOL_ "%lu"), GetLastError() }
		}.ShowError(hWnd);
		goto CLEANUP;
	}


	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

#ifndef _DEBUG
	waitResult = WaitForSingleObject(processInfo.hProcess, 3000);
	if (waitResult == WAIT_TIMEOUT) {
		MessageBoxNotifier{
			{ _T("System Error") },
			{ _T("The wait time-out interval elapsed." EOL_ "%lu"), WAIT_TIMEOUT }
		}.ShowError(hWnd);
		goto CLEANUP;
	}
	if (waitResult == WAIT_FAILED) {
		MessageBoxNotifier{
			{ _T("System Error") },
			{ _T("Failed to wait for the process." EOL_ "%lu"), WAIT_FAILED }
		}.ShowError(hWnd);
		goto CLEANUP;
	}
#endif

CLEANUP:
	WNDCLASSEX existingWc{ sizeof(WNDCLASSEX) };
	BOOL isRegistered = GetClassInfoEx(wcex.hInstance, Config::MainWindowClass, &existingWc);
	if (isRegistered) { UnregisterClass(Config::MainWindowClass, GetModuleHandle(NULL)); }
	if (hEvent) { CloseHandle(hEvent); }
	if (hDll) { FreeLibrary(hDll); }
	if (processInfo.hProcess) { CloseHandle(processInfo.hProcess); }
	if (processInfo.hThread) { CloseHandle(processInfo.hThread); }
	ThemeManager::DisableThemeSupport();

	return 0;
}



