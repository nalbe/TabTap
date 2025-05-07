#pragma once

// Implementation-specific headers
#include "CustomIncludes/WinApi/WorkAreaManager.h"
#include "CustomIncludes/WinApi/DragTracker.h"

// Windows headers
#include <windows.h>
#include <tchar.h>
#include <gdiplus.h>



// Represent the result of an operation
struct Result
{
public:
	// Enumerates types of errors for Result struct
	enum class ErrorType
	{
		None,          // No error (success)
		SystemError,   // System error from GetLastError()
		GDIPlusError,  // GDI+ error from Status codes
		CustomError    // User-defined error
	};

public:
	bool success{};                  // True if successful, false if failed
	ErrorType errorType{};           // Specifies the type of error (or None if operation was successful)
	DWORD errorValue;                // Error code (system, GDI+ etc.)
	LPCTSTR message{};               // Stores a custom error message for CustomError
	LPCTSTR header{};                // Error header

	// Default constructor for a successful result
	Result() :
		success(true),
		errorType(ErrorType::None),
		errorValue(0),
		header(_T("Success")),
		message(_T(""))
	{}

	// Constructor for system error with error code and optional message
	Result(DWORD dwErrCode, LPCTSTR cszMsg = _T("")) :
		success(false),
		errorType(ErrorType::SystemError),
		errorValue(dwErrCode),
		header(_T("System Error")),
		message(cszMsg)
	{}

	// Constructor for GDI+ error with status code and optional message
	Result(Gdiplus::Status eStatus, LPCTSTR cszMsg = _T("")) :
		success(false),
		errorType(ErrorType::GDIPlusError),
		errorValue(static_cast<DWORD>(eStatus)),
		header(_T("GDI+ Error")),
		message(cszMsg)
	{}

	// Constructor for custom error with a user-defined message
	Result(DWORD dwErrCode, LPCTSTR cszHeader, LPCTSTR cszMsg) :
		success(false),
		errorType(ErrorType::CustomError),
		errorValue(dwErrCode),
		header(cszHeader),
		message(cszMsg)
	{}

	// Explicit contextual conversion
	explicit operator bool() const noexcept
	{
		return success;
	}

};


// GDI+ Resource Manager
class GDIPlusData
{
private:
	// --- Member Variables ---
	ULONG_PTR pToken{};           // GDI+ initialization token
	Gdiplus::Image* pImage{};     // Loaded image resource
	Gdiplus::ImageAttributes* pImageAttributes{};  // Image effects processor
	Result result{};              // Operation result storage

private:
	// --- Internal Methods ---
	/// Sets the internal result state
	Result SetResult(Result);
	/// Initializes GDI+ subsystem
	Result InitializeGDIPlus();
	/// Shuts down GDI+ subsystem
	void ShutdownGDIPlus();

public:
	// --- Lifecycle Management ---
	~GDIPlusData();
	GDIPlusData();
	GDIPlusData(const GDIPlusData&) = delete;
	GDIPlusData& operator=(const GDIPlusData&) = delete;

	// --- Image Resource Management ---
	/// Loads image from file
	Result LoadImageFile(LPCTSTR);
	/// Loads image from byte array in memory
	Result LoadImageByteArray();
	/// Loads application image using the configured source method
	Result LoadApplicationImage();
	/// Releases loaded image resources
	void FreeImageResource();
	/// Gets pointer to loaded image
	Gdiplus::Image* GetImage() const;

	// --- Image Effects Management ---
	/// Creates image attributes for effect processing
	Result CreateImageAttributes();
	/// Releases image attributes resources
	void FreeImageAttributes();
	/// Gets pointer to image attributes (null if none created)
	Gdiplus::ImageAttributes* GetImageAttributes() const;
	/// Applies color transformation matrix to image
	Result ApplyColorMatrix(Gdiplus::ColorMatrix*);

	// --- Result Management ---
	/// Gets the last operation result
	Result GetResult() const;
};


// Interface for system tray adapter
struct ITrayAdapter
{
	// Sets up the tray icon
	virtual Result SetupTrayIcon(NOTIFYICONDATA&) = 0;
	// Creates the tray menu
	virtual Result CreateTrayMenu(HMENU&) = 0;
};


// System Tray Icon Manager
class TrayManager
{
private:
	// --- Member Variables ---
	ITrayAdapter* pTrayAdapter{};      // Tray adapter interface 
	NOTIFYICONDATA trayData{};         // Data for the system tray icon
	HMENU hTrayMenu{};                 // Context menu handle
	Result result{};                   // Operation result storage

private:
	// --- Internal Methods ---
	/// Sets the internal result state
	Result SetResult(Result);

public:
	// --- Lifecycle Management ---
	~TrayManager();
	explicit TrayManager(ITrayAdapter*);

	// --- Tray Icon Operations ---
	/// Creates and displays the system tray icon
	Result SetupTrayIcon();
	/// Removes the system tray icon
	void DeleteTrayIcon();
	/// Loads the icon resource from executable
	void GetIconResource(HICON);
	/// Releases the loaded icon resource
	void FreeIconResource();

	// --- Menu Operations ---
	/// Creates the tray context menu
	Result CreateTrayMenu();
	/// Destroys the tray context menu
	Result DestroyTrayMenu();
	/// Displays and tracks the tray context menu
	Result TrackTrayMenu();

	// --- Interaction Handling ---
	/// Handles right-click events on the tray icon
	Result HandleTrayRightClick();

	// --- Result Management ---
	/// Gets the last operation result
	Result GetResult() const;
	/// Clears the stored result
	void ClearResult();
};


// Interface for edge-snapping adapters
struct ISnapAdapter
{
	// Returns target window handle for snapping
	virtual HWND GetTargetWindow() const = 0;
	// Gets current snapped edge
	virtual ScreenEdge GetSnapEdge() const = 0;
	// Checks if edge is valid for snapping
	virtual bool IsValidSnapEdge(const ScreenEdge&) const = 0;
	// Handles successful edge snap with edge and position
	virtual void OnSnapSuccess(const ScreenEdge&, const POINT&) = 0;
	// Handles rejected snap attempt
	virtual void OnSnapRejected(const ScreenEdge&, const POINT&) = 0;
	// Updates position when no snap occurs
	virtual void OnSnapEdgeNone(const POINT&) = 0;
	// Default virtual destructor
	virtual ~ISnapAdapter() = default;
};


// ScreenEdge-Snapping Controller
class EdgeSnapData
{
private:
	// --- Configuration Constants ---
	const int SnapMargin{ 50 };        // Distance in pixels from edge to trigger snapping preview
	const int PreviewFrameSize{ 4 };   // Thickness of preview frame
	const int PreviewSize{ 30 };       // Size of snapping preview indicator

	// --- Member Variables ---
	ISnapAdapter* pSnapAdapter{};      // Snap adapter interface
	DragTracker dragTracker{};         // Handles core drag tracking 
	HWND hPreviewWnd{};                // Preview window handle
	RECT rcTargetRect{};               // Preview window coordinates
	bool isEnabled{};                  // Indicates if edge-snapping is active
	bool isPreviewEnabled{};           // Indicates if edge-snapping is triggered
	ScreenEdge snapEdge{ ScreenEdge::None };       // Current edge to snap to
	Result result{};                   // Operation result storage

private:
	// --- Internal Methods ---
	/// Sets the operation result
	Result SetResult(Result);
	/// Creates the visual preview window
	void CreatePreviewWindow();
	/// Updates preview window position and appearance
	void UpdatePreviewWindow();
	/// Renders the preview graphics
	Result DrawPreview();

public:
	// --- Lifecycle Management ---
	~EdgeSnapData();
	EdgeSnapData();
	EdgeSnapData(const EdgeSnapData&) = delete;
	EdgeSnapData& operator=(const EdgeSnapData&) = delete;

	// --- Snapping Control ---
	/// Enables edge-snapping behavior
	bool Enable(ISnapAdapter*);
	/// Disables edge-snapping behavior
	void Disable();
	/// Checks if snapping is active
	bool IsEnabled() const;
	/// Checks if snapping is active
	bool IsPreviewEnabled() const;

	// --- ScreenEdge Detection ---
	/// Gets currently detected snap edge
	ScreenEdge GetSnapEdge() const;
	/// Checks if specific edge is being snapped to
	bool IsSnapEdge(const ScreenEdge&);

	// --- Drag Operations ---
	/// Updates snapping state based on current cursor position
	bool OnMouseMove();
};


// Window Animation Controller
class AnimationData
{
private:
	// --- Animation Configuration ---
	const LONG pixelByStep{ 1 };      // Pixels to move per animation step

	// --- Animation State ---
	HWND hAnimatedWnd{};              // Handle to window being animated
	bool isEnabled{};                 // Indicates if an animation is currently active
	POINT ptCurrentPoint{};           // Current window left-top coordinates
	POINT ptTargetPoint{};            // Destination coordinates (screen space)

public:
	// --- Lifecycle Management ---
	~AnimationData();
	AnimationData();
	AnimationData(const AnimationData&) = delete;
	AnimationData& operator=(const AnimationData&) = delete;

	// --- Animation Control ---
	/// Starts new animation sequence
	bool Enable(HWND, const POINT&);
	/// Stops current animation
	void Disable();
	/// Checks if animation is currently running
	bool IsEnabled() const;
	/// Updates animation state
	bool Update();
};


// Visual Blink Effect Controller
class BlinkData
{
private:
	// --- Configuration Constants ---
	const COLORREF defBlinkColor{ RGB(255, 255, 0) };  // Color used for the blink effect
	const float defIntensity{ 0.9f };                  // Intensity of the blink color tint (0.0 to 1.0)
	const UINT defInterval{ 400 };                     // Blink interval in milliseconds
	const DWORD defDuration{ 3000 };                   // Total duration of the blink effect in milliseconds

	// --- Effect State ---
	HWND hTargetWnd{};                     // Handle to window receiving blink effect
	bool isEnabled{};                      // Indicates if the blink effect is active
	bool isBlinkState{};                   // Current blink phase (on/off)
	DWORD blinkStartTime{};                // Timestamp when blinking started

	// --- Color Transformations ---
	Gdiplus::ColorMatrix normalMatrix{};   // Identity matrix (normal state)
	Gdiplus::ColorMatrix blinkMatrix{};    // Tinted matrix (blink state)

	// ID of timer for blink effect
	UINT uTimerID{};

private:
	// --- Internal Methods ---
	void InitializeColorMatrices();

public:
	// --- Lifecycle Management ---
	~BlinkData();
	BlinkData();
	BlinkData(const BlinkData&) = delete;
	BlinkData& operator=(const BlinkData&) = delete;

	// --- Effect Control ---
	/// Starts blink effect on specified window
	bool Enable(HWND, const UINT&);
	/// Stops blink effect
	void Disable();
	/// Checks if effect is currently active
	bool IsEnabled() const;

	// --- Effect Processing ---
	/// Updates effect state (call periodically)
	bool Update();
	/// Checks if total duration has elapsed
	bool IsElapsed();

	/// Gets current color transformation matrix
	Gdiplus::ColorMatrix* GetActiveColorMatrix();
};



