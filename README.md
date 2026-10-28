# Cricket 07 Input Extended

A modern input hook for EA Sports Cricket 07 that brings native plug-and-play support for XInput and DirectInput controllers, restoring Century Stick batting controls from PS2. 

Powered by SDL3 and MinHook, this ASI plugin intercepts the game's legacy input engine and forces dual-analog compatibility for 100% of modern controllers without needing manual `controltypes.csv` edits.

## Features
* **Raw Input:** Uses raw input for keyboard
* **Universal Controller Support:** Works seamlessly both in wired/wireless mode with Xbox, PlayStation, and generic controllers using a dual SDL3 Gamepad/Joystick system.
* **Right Stick Fixed:** Controllers will no longer lose their Right Stick mappings.
* **Dynamic UI Prompts:** Customise your in-game button icons (PlayStation, Xbox, or Keyboard) via given INI file.

## Installation
1. Download the latest `Cricket07InputEx.zip` from the **Releases** tab.
2. Extract the files directly into your Cricket 07 root directory (where `cricket07.exe` is located).
    <ul><i>Note: You must have an ASI Loader (like Ultimate ASI Loader) installed in your game directory for the plugin to load.</i></ul>
3. To use new icons, extract the same from the zip file
4. Launch the game.

## Credits
* **[SDL3 (Simple DirectMedia Layer)](https://github.com/libsdl-org/SDL):** Used for robust, modern XInput and DirectInput hardware mapping. 
* **[MinHook](https://github.com/TsudaKageyu/minhook):** Used for API hooking to intercept the game's native engine calls.
