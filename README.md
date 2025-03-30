# Windows10 OSK Wrapper

Inspired by the Windows 7 TabTip On-Screen Keyboard, this project is a lightweight wrapper around the default Windows 10 On-Screen Keyboard (`osk.exe`). It enhances usability with additional features while leaving the original executable unchanged.

## Features

- **No modifications to osk.exe:**  
  Operates as a transparent wrapper without altering the original on-screen keyboard executable.

- **Hide on-screen keyboard:**  
  Easily hide the OSK when it overlaps a sensitive window by simply middle-clicking on the tab.

- **System tray integration:**  
  Moves the OSK from the taskbar to the system tray.

- **Lightweight implementation:**  
  Built exclusively using WinAPI, ensuring minimal resource usage.

- **Customizable tab image:**  
  Uses a PNG image with alpha transparency for the tab. This image can be replaced with any preferred image.

- **Autostart capability:**  
  Optionally configure the wrapper to launch automatically with Windows.

- **Forced Dock Mode:**  
  It's like dock mode but with drag ability. The size can be adjusted before switching to it. The right button is for dragging, and the middle button is for sensitive dragging..

## Limitations

**To use the Windows 10 OSK Wrapper tool, you first need to fully turn off User Account Control (UAC) on your system.**  
*Note: Disabling UAC lowers security protections.*
```Save as .reg and apply
Windows Registry Editor Version 5.00

[HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows\CurrentVersion\Policies\System]
"EnableLUA"=dword:00000000
"ConsentPromptBehaviorAdmin"=dword:00000000
"PromptOnSecureDesktop"=dword:00000000
```

## Installation

*No installation needed.*

## Usage

*Run and use.*

## Contributing

Contributions, feedback, and suggestions are welcome. Feel free to submit issues or pull requests to help improve the project.

## License

*MIT License.*
