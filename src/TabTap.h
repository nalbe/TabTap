#pragma once

// Define end-of-line marker
#ifndef EOL_
#define EOL_ "\r\n"
#endif // EOL_


// Custom window message IDs
#define WM_APP_TRAYICON             (WM_APP + 1)  // Custom tray icon notification message
#define WM_APP_CUSTOM_MESSAGE       (WM_APP + 2)  // Custom message


// Icons, menu items, and control identifiers
#define IDI_NOTIFY_ICON				(1000 + 1)
#define IDT_KEEP_ON_TOP             (1000 + 2)
#define IDT_ANIMATION_TIMER         (1000 + 3)
#define IDT_BLINK_TIMER             (1000 + 4)


// Command identifiers for the notification area context menu
#define IDM_TRAY_DOCKMODE           (2000 + 1)
#define IDM_TRAY_AUTOSTART          (2000 + 2)
#define IDM_TRAY_EXIT               (2000 + 3)
#define IDM_TRAY_SEPARATOR          (2000 + 4)


// Custom command IDs (LOWORD)
#define ID_APP_ANIMATION            (3000 + 1)
#define ID_APP_SYNC_Y_POSITION      (3000 + 2)
#define ID_APP_DIRECTUI_READY       (3000 + 3)
#define ID_APP_DOCKMODE             (3000 + 4)
#define ID_APP_REGULARMODE          (3000 + 5)
#define ID_APP_FADE                 (3000 + 6)



