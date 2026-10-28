# Cricket07InputEx

A modern input hook for EA Sports Cricket 07 that brings native plug-and-play support for XInput and DirectInput controllers, restoring Century Stick batting controls from PS2. 

Powered by SDL3 and MinHook, this ASI plugin intercepts the game's legacy input engine and forces dual-analog compatibility for 100% of modern controllers without needing manual `controltypes.csv` edits.

## Features
* **Universal Controller Support:** Works seamlessly both in wired/wireless mode with Xbox, PlayStation, and generic controllers using a dual SDL3 Gamepad/Joystick system.
* **Right Stick Fixed:** Controllers will no longer lose their Right Stick mappings.
* **Dynamic UI Prompts:** Customise your in-game button icons (PlayStation, Xbox, or Keyboard) via given INI file.

## Installation
1. Download the latest `Cricket07InputEx.zip` from the **Releases** tab.
2. Extract the files (`Cricket07InputEx.asi` and `Cricket07InputEx.ini`) directly into your Cricket 07 root directory (where `cricket07.exe` is located).
3. *Note: You must have an ASI Loader (like Ultimate ASI Loader) installed in your game directory for the plugin to load.*
4. Launch the game.
