#include "globals.h"

// MEMORY ADDRESSES
DWORD* ENGINE_DEVICE_COUNT = (DWORD*)0x00A3A670;
void* ENGINE_DEVICE_TABLE = (void*)0x00A384F0;
PushEvent_t PushEvent = (PushEvent_t)0x006efa00;
StringAssign_t Real_StringAssign = (StringAssign_t)0x0070BB70;

// GLOBAL POINTERS
INPUT_init_t Original_INPUT_init = nullptr;
CControl_Update_t Original_CControl_Update = nullptr;
ApplyRumble_t Original_ApplyRumble = nullptr;
ControlIcons_t Original_ControlIcons = nullptr;
FilterIcon_t Original_FilterIcon = nullptr;
static LoadConfig_t Original_LoadConfig = nullptr;

// GLOBAL STATE DEFINITIONS
int g_iconMode = 0;
SDL_Gamepad* activePads[4] = { nullptr };
SDL_Joystick* activeJoys[4] = { nullptr };
bool s_sdlInitialized = false;
int firstPadDeviceIndex = -1;

static uintptr_t g_pendingBx = 0;

// HELPERS
static const char* GetXboxIcon(const char* name)
{
    static const char* ps[] = {
        "bx", "bsquare", "btriang", "bcircle",
        "bl1", "br1", "bl2", "br2", "bstart", "bselect"
    };

    static const char* xb[] = {
        "xbbx", "xbbsquare", "xbbtriang", "xbbcircle",
        "xbbl1", "xbbr1", "xbbl2", "xbbr2", "xbbstart", "xbbselect"
    };

    for (int i = 0; i < 10; ++i)
        if (_stricmp(name, ps[i]) == 0)
            return xb[i];

    return nullptr;
}

inline void SetAxisFloat(int* pControl, int offsetIndex, float value) {
    pControl[offsetIndex] = *(int*)&value;
}

// DIRECTINPUT7 PROXY
#pragma comment(linker, "/EXPORT:DirectInputCreateA=_DirectInputCreateA@16")
typedef HRESULT(__stdcall* DirectInputCreateA_t)(HINSTANCE, DWORD, LPVOID*, LPUNKNOWN);
DirectInputCreateA_t Real_DirectInputCreateA = nullptr;

extern "C" HRESULT __stdcall DirectInputCreateA(HINSTANCE hinst, DWORD dwVersion, LPVOID* ppDI, LPUNKNOWN punkOuter) {
    if (!Real_DirectInputCreateA) {
        char syspath[MAX_PATH];
        GetSystemDirectoryA(syspath, MAX_PATH);
        strcat_s(syspath, MAX_PATH, "\\dinput.dll");
        HMODULE hMod = LoadLibraryA(syspath);
        if (hMod) {
            Real_DirectInputCreateA = (DirectInputCreateA_t)GetProcAddress(hMod, "DirectInputCreateA");
        }
    }
    if (Real_DirectInputCreateA) {
        return Real_DirectInputCreateA(hinst, dwVersion, ppDI, punkOuter);
    }
    return 0x80004005;
}

// GAMEPAD/KEYBOARD HOOKS
int __fastcall Hook_INPUT_init(void* ecx, void* edx) {
    int result = Original_INPUT_init(ecx, edx);

    DWORD oldProtect;
    VirtualProtect(ENGINE_DEVICE_TABLE, 32 * 0x10C, PAGE_READWRITE, &oldProtect);

    // Append virtual slots for our gamepads to ensure they exist
    for (int i = 1; i <= 4; i++) {
        DWORD* padRecord = (DWORD*)((uintptr_t)ENGINE_DEVICE_TABLE + (i * 0x10C));
        if (padRecord[0] == 0 && padRecord[2] == 0) {
            padRecord[2] = 4; // Mark as Gamepad
        }
    }

    if (*ENGINE_DEVICE_COUNT < 5) {
        *ENGINE_DEVICE_COUNT = 5;
    }

    VirtualProtect(ENGINE_DEVICE_TABLE, 32 * 0x10C, oldProtect, &oldProtect);
    return result;
}

// SAFE INPUT INJECTION LOOP
void __fastcall Hook_CControl_Update(int* pControl, void* edx) {
    if (!pControl) return;

    int deviceType = pControl[0x2570 / 4];
    int deviceIndex = pControl[0x25C0 / 4];

    if (deviceType != 5 && deviceType != 1 && deviceType != 8) {
        Original_CControl_Update(pControl, edx);
        return;
    }

    if (!s_sdlInitialized) {
        // Init both Gamepad (XInput) and raw Joystick (DInput) subsystems
        SDL_Init(SDL_INIT_GAMEPAD | SDL_INIT_JOYSTICK | SDL_INIT_EVENTS);
        s_sdlInitialized = true;
    }

    SDL_PumpEvents();
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_GAMEPAD_ADDED) {
            for (int i = 0; i < 4; i++) {
                if (!activePads[i] && !activeJoys[i]) {
                    activePads[i] = SDL_OpenGamepad(event.gdevice.which);
                    break;
                }
            }
        }
        else if (event.type == SDL_EVENT_JOYSTICK_ADDED) {
            // Only open as raw joystick if SDL says it isn't a known Gamepad
            if (!SDL_IsGamepad(event.jdevice.which)) {
                for (int i = 0; i < 4; i++) {
                    if (!activePads[i] && !activeJoys[i]) {
                        activeJoys[i] = SDL_OpenJoystick(event.jdevice.which);
                        break;
                    }
                }
            }
        }
        else if (event.type == SDL_EVENT_GAMEPAD_REMOVED) {
            for (int i = 0; i < 4; i++) {
                if (activePads[i] && SDL_GetGamepadID(activePads[i]) == event.gdevice.which) {
                    SDL_CloseGamepad(activePads[i]);
                    activePads[i] = nullptr;
                    break;
                }
            }
        }
        else if (event.type == SDL_EVENT_JOYSTICK_REMOVED) {
            for (int i = 0; i < 4; i++) {
                if (activeJoys[i] && SDL_GetJoystickID(activeJoys[i]) == event.jdevice.which) {
                    SDL_CloseJoystick(activeJoys[i]);
                    activeJoys[i] = nullptr;
                    break;
                }
            }
        }
    }

    // GAMEPAD INJECTION
    if (deviceType == 5 || deviceType == 1) {

        if (firstPadDeviceIndex == -1) firstPadDeviceIndex = deviceIndex;
        int padIndex = deviceIndex - firstPadDeviceIndex;

        if (padIndex >= 0 && padIndex < 4) {

            // XINPUT / KNOWN CONTROLLER (GAMEPAD)
            if (activePads[padIndex]) {
                SDL_Gamepad* pad = activePads[padIndex];

                // CALCULATE LEFT STICK FIRST
                Sint16 rawLX = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTX);
                Sint16 rawLY = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTY);
                float lX = (abs(rawLX) > 4900) ? (rawLX / 32767.0f) : 0.0f;
                float lY = (abs(rawLY) > 4900) ? (-rawLY / 32767.0f) : 0.0f;

                SetAxisFloat(pControl, 2, lX);
                SetAxisFloat(pControl, 3, lY);

                // LEFT STICK & D-PAD LINKED
                bool dpadUp = SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_UP) || (lY > 0.5f);
                bool dpadDown = SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_DOWN) || (lY < -0.5f);
                bool dpadLeft = SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_LEFT) || (lX < -0.5f);
                bool dpadRight = SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_RIGHT) || (lX > 0.5f);

                PushEvent(pControl, dpadUp ? 1 : 0, 6, 0x10);
                PushEvent(pControl, dpadDown ? 1 : 0, 7, 0x20);
                PushEvent(pControl, dpadLeft ? 1 : 0, 8, 0x40);
                PushEvent(pControl, dpadRight ? 1 : 0, 9, 0x80);

                // BUTTONS
                PushEvent(pControl, SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_NORTH) ? 1 : 0, 2, 0x4000);
                PushEvent(pControl, SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_WEST) ? 1 : 0, 1, 0x2);
                PushEvent(pControl, SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_EAST) ? 1 : 0, 0, 0x8000);
                PushEvent(pControl, SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_SOUTH) ? 1 : 0, 3, 0x1);
                PushEvent(pControl, SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER) ? 1 : 0, 10, 0x100);
                PushEvent(pControl, SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER) ? 1 : 0, 13, 0x800);
                PushEvent(pControl, (SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) > 16000) ? 1 : 0, 11, 0x200);
                PushEvent(pControl, (SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) > 16000) ? 1 : 0, 14, 0x1000);
                PushEvent(pControl, SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_START) ? 1 : 0, 4, 4);
                PushEvent(pControl, SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_BACK) ? 1 : 0, 15, 0x2000);

                // RIGHT STICK
                Sint16 rawRX = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHTX);
                Sint16 rawRY = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHTY);
                float rX = (abs(rawRX) > 4900) ? (rawRX / 32767.0f) : 0.0f;
                float rY = (abs(rawRY) > 4900) ? (-rawRY / 32767.0f) : 0.0f;

                SetAxisFloat(pControl, 4, rX);
                SetAxisFloat(pControl, 5, rY);
                PushEvent(pControl, (rX < -0.5f) ? 1 : 0, 16, 0x10000);
                PushEvent(pControl, (rX > 0.5f) ? 1 : 0, 17, 0x20000);
                PushEvent(pControl, (rY > 0.5f) ? 1 : 0, 18, 0x40000);
                PushEvent(pControl, (rY < -0.5f) ? 1 : 0, 19, 0x80000);
            }


            // DINPUT / RAW HARDWARE (JOYSTICK)
            else if (activeJoys[padIndex]) {
                SDL_Joystick* joy = activeJoys[padIndex];

                // Raw Axes (Standard layout: 0=LX, 1=LY, 2=RX/Z, 3=RY/RZ)
                Sint16 rawLX = SDL_GetJoystickAxis(joy, 0);
                Sint16 rawLY = SDL_GetJoystickAxis(joy, 1);
                float lX = (abs(rawLX) > 4900) ? (rawLX / 32767.0f) : 0.0f;
                float lY = (abs(rawLY) > 4900) ? (-rawLY / 32767.0f) : 0.0f;

                SetAxisFloat(pControl, 2, lX);
                SetAxisFloat(pControl, 3, lY);

                // D-Pad is read via "Hats" on generic DirectInput controllers
                Uint8 hat = SDL_GetJoystickHat(joy, 0);
                bool dpadUp = (hat & SDL_HAT_UP) || (lY > 0.5f);
                bool dpadDown = (hat & SDL_HAT_DOWN) || (lY < -0.5f);
                bool dpadLeft = (hat & SDL_HAT_LEFT) || (lX < -0.5f);
                bool dpadRight = (hat & SDL_HAT_RIGHT) || (lX > 0.5f);

                PushEvent(pControl, dpadUp ? 1 : 0, 6, 0x10);
                PushEvent(pControl, dpadDown ? 1 : 0, 7, 0x20);
                PushEvent(pControl, dpadLeft ? 1 : 0, 8, 0x40);
                PushEvent(pControl, dpadRight ? 1 : 0, 9, 0x80);

                // Raw Buttons mapping (Standard generic/PS2-style layout)
                PushEvent(pControl, SDL_GetJoystickButton(joy, 0) ? 1 : 0, 2, 0x4000); // Triangle
                PushEvent(pControl, SDL_GetJoystickButton(joy, 3) ? 1 : 0, 1, 0x2);    // Square
                PushEvent(pControl, SDL_GetJoystickButton(joy, 1) ? 1 : 0, 0, 0x8000); // Circle
                PushEvent(pControl, SDL_GetJoystickButton(joy, 2) ? 1 : 0, 3, 0x1);    // Cross

                PushEvent(pControl, SDL_GetJoystickButton(joy, 4) ? 1 : 0, 10, 0x100); // L1
                PushEvent(pControl, SDL_GetJoystickButton(joy, 5) ? 1 : 0, 13, 0x800); // R1
                PushEvent(pControl, SDL_GetJoystickButton(joy, 6) ? 1 : 0, 11, 0x200); // L2
                PushEvent(pControl, SDL_GetJoystickButton(joy, 7) ? 1 : 0, 14, 0x1000); // R2

                PushEvent(pControl, SDL_GetJoystickButton(joy, 9) ? 1 : 0, 4, 4);      // Start
                PushEvent(pControl, SDL_GetJoystickButton(joy, 8) ? 1 : 0, 15, 0x2000); // Select

                // Right Stick Raw Axes
                Sint16 rawRX = SDL_GetJoystickAxis(joy, 2); // Usually Z-Axis
                Sint16 rawRY = SDL_GetJoystickAxis(joy, 3); // Usually RZ-Axis
                float rX = (abs(rawRX) > 4900) ? (rawRX / 32767.0f) : 0.0f;
                float rY = (abs(rawRY) > 4900) ? (-rawRY / 32767.0f) : 0.0f;

                SetAxisFloat(pControl, 4, rX);
                SetAxisFloat(pControl, 5, rY);
                PushEvent(pControl, (rX < -0.5f) ? 1 : 0, 16, 0x10000);
                PushEvent(pControl, (rX > 0.5f) ? 1 : 0, 17, 0x20000);
                PushEvent(pControl, (rY > 0.5f) ? 1 : 0, 18, 0x40000);
                PushEvent(pControl, (rY < -0.5f) ? 1 : 0, 19, 0x80000);
            }
        }
    }

    //  KEYBOARD INJECTION
    else if (deviceType == 8) {
        PushEvent(pControl, (GetAsyncKeyState('S') & 0x8000) ? 1 : 0, 0, 0x1); // S 
        PushEvent(pControl, (GetAsyncKeyState('W') & 0x8000) ? 1 : 0, 1, 0x4000); // W
        PushEvent(pControl, (GetAsyncKeyState('A') & 0x8000) ? 1 : 0, 2, 0x2);   // A
        PushEvent(pControl, (GetAsyncKeyState('D') & 0x8000) ? 1 : 0, 3, 0x8000); // D

        // Others
        PushEvent(pControl, (GetAsyncKeyState(VK_ESCAPE) & 0x8000) ? 1 : 0, 4, 4); // Esc
        PushEvent(pControl, (GetAsyncKeyState(VK_RETURN) & 0x8000) ? 1 : 0, 5, 8); // Enter/Return
        PushEvent(pControl, (GetAsyncKeyState(VK_SHIFT) & 0x8000) ? 1 : 0, 10, 0x100); // Shift
        PushEvent(pControl, (GetAsyncKeyState('Q') & 0x8000) ? 1 : 0, 11, 0x200); // Q
        PushEvent(pControl, (GetAsyncKeyState('E') & 0x8000) ? 1 : 0, 13, 0x800); // E
        PushEvent(pControl, (GetAsyncKeyState('X') & 0x8000) ? 1 : 0, 15, 0x2000); // X

        // Arrow Keys
        bool upPressed = (GetAsyncKeyState(VK_UP) & 0x8000) != 0;
        bool downPressed = (GetAsyncKeyState(VK_DOWN) & 0x8000) != 0;
        bool leftPressed = (GetAsyncKeyState(VK_LEFT) & 0x8000) != 0;
        bool rightPressed = (GetAsyncKeyState(VK_RIGHT) & 0x8000) != 0;

        PushEvent(pControl, upPressed ? 1 : 0, 6, 0x10);
        PushEvent(pControl, downPressed ? 1 : 0, 7, 0x20);
        PushEvent(pControl, leftPressed ? 1 : 0, 8, 0x40);
        PushEvent(pControl, rightPressed ? 1 : 0, 9, 0x80);

        // Analog axes for bowling marker (keyboard)
        float kbAxisX = 0.0f;
        float kbAxisY = 0.0f;

        if (leftPressed)  kbAxisX -= 1.0f;
        if (rightPressed) kbAxisX += 1.0f;
        if (upPressed)    kbAxisY += 1.0f;
        if (downPressed)  kbAxisY -= 1.0f;

        SetAxisFloat(pControl, 2, kbAxisX);
        SetAxisFloat(pControl, 3, kbAxisY);
    }
}

/* Rumble Support  Not working yet
void __cdecl Hook_ApplyRumble(int deviceIndex, int intensity) {
    if (deviceIndex >= 0 && deviceIndex < 4 && activePads[deviceIndex]) {
        Uint16 rumblePower = (Uint16)(((float)intensity / 10000.0f) * 65535.0f);
        SDL_RumbleGamepad(activePads[deviceIndex], rumblePower, rumblePower, 250);
    }
} */

// --- PS2/XBOX UI RESTORATION HOOKS ---
void __cdecl Hook_ControlIcons(int bitmask, char* outString) {
    Original_ControlIcons(bitmask, outString);

    if (outString && strncmp(outString, "bdgb", 4) == 0) {
        if (g_iconMode == 0) {
            // Keyboard Mode -> changes "bdgb" prefix to "bdkb"
            outString[2] = 'k';
        }
        else if (g_iconMode == 2) {
            // Xbox Mode -> swaps button asset names
            switch (bitmask) {
            case 0x1:    strcpy(outString, "xbbx"); break;        // A
            case 0x2:    strcpy(outString, "xbbsquare"); break;  // X
            case 0x4000: strcpy(outString, "xbbtriang"); break;  // Y
            case 0x8000: strcpy(outString, "xbbcircle"); break;  // B
            case 0x100:  strcpy(outString, "xbbl1"); break;      // LB
            case 0x800:  strcpy(outString, "xbbr1"); break;      // RB
            case 0x200:  strcpy(outString, "xbbl2"); break;      // LT
            case 0x1000: strcpy(outString, "xbbr2"); break;      // RT
            case 0x4:    strcpy(outString, "xbbstart"); break;   // Start
            case 0x2000: strcpy(outString, "xbbselect"); break;  // Back
            }
        }
    }
}

void __cdecl Hook_FilterIcon(void* param_1, char* param_2)
{
    // Capture the UI element pointer
    uintptr_t current_esi;
    __asm { mov current_esi, esi }

    if (!param_2) {
        if (Original_FilterIcon) Original_FilterIcon(param_1, param_2);
        return;
    }

    // Select icon cleanup: If the label right after a 'bx' is $0, wipe the icon
    if (g_pendingBx) {
        if (strcmp(param_2, "$0") == 0 && *(DWORD*)g_pendingBx == 0x007936D8) {
            int** pScreen = ((int** (*)())0x00487930)();
            short screen = (pScreen && *pScreen) ? *(short*)((char*)*pScreen + 4) : -1;

            void* dev = ((void* (*)())0x004ae220)();
            int devCheck = ((int(__fastcall*)(void*, void*))0x004ae490)(dev, nullptr);

            if (screen != 0xa8d && (devCheck == 0 || screen != 1)) {
                Real_StringAssign((void*)(g_pendingBx + 0x70), "trans");
            }
        }
        g_pendingBx = 0; // Always reset immediately
    }

    // Mapping & Tracking
    const char* targetTex = nullptr;

    if (_stricmp(param_2, "bx") == 0) {
        targetTex = "xbbx";
        // Only queue the pointer if it's readable and matches the expected vtable
        g_pendingBx = (!IsBadReadPtr((void*)current_esi, 4) && *(DWORD*)current_esi == 0x007936D8) ? current_esi : 0;
    }
    else if (_stricmp(param_2, "bsquare") == 0) targetTex = "xbbsquare";
    else if (_stricmp(param_2, "btriang") == 0) targetTex = "xbbtriang";
    else if (_stricmp(param_2, "bcircle") == 0) targetTex = "xbbcircle";
    else if (_stricmp(param_2, "bl1") == 0) targetTex = "xbbl1";
    else if (_stricmp(param_2, "br1") == 0) targetTex = "xbbr1";
    else if (_stricmp(param_2, "bl2") == 0) targetTex = "xbbl2";
    else if (_stricmp(param_2, "br2") == 0) targetTex = "xbbr2";
    else if (_stricmp(param_2, "bstart") == 0) targetTex = "xbbstart";
    else if (_stricmp(param_2, "bselect") == 0) targetTex = "xbbselect";

    if (targetTex) {
        if (g_iconMode == 2) Real_StringAssign(param_1, targetTex); // Xbox
        else if (g_iconMode == 0) Real_StringAssign(param_1, "trans"); // Default behaviour ; PC
        else Real_StringAssign(param_1, param_2); // PlayStation
        return;
    }

    if (Original_FilterIcon) Original_FilterIcon(param_1, param_2);
}

void LoadIconMode(HMODULE hModule)
{
    char iniPath[MAX_PATH];
    GetModuleFileNameA(hModule, iniPath, MAX_PATH);

    char* slash = strrchr(iniPath, '\\');
    if (slash)
        strcpy(slash + 1, "Cricket07InputEx.ini");

    g_iconMode = GetPrivateProfileIntA(
        "Controller", "IconMode", 0, iniPath);

    if (g_iconMode < 0 || g_iconMode > 2)
        g_iconMode = 0;
}

static DWORD __fastcall Hook_LoadConfig(void* pThis, void* edx, char* file, char* devName)
{
    DWORD f_dual = Original_LoadConfig(pThis, edx, file, devName);

    // 3 = generic template (no dual sticks)
    if (devName && strncmp(devName, "___", 3) != 0 && *(int*)((char*)pThis + 0x25d0) == 3) {
        DWORD f_dual = Original_LoadConfig(pThis, edx, file, (char*)"Logitech Dual Action USB");
        if (f_dual && *(int*)((char*)pThis + 0x25d0) != 3)
            return f_dual;   // dual-stick preset applied
    }
    return f_dual;
}

// ENTRY POINT
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        LoadIconMode(hModule);
        MH_Initialize();

        MH_CreateHook((LPVOID)0x0076cb30, &Hook_INPUT_init, (LPVOID*)&Original_INPUT_init);
        MH_CreateHook((LPVOID)0x006fa2f0, &Hook_CControl_Update, (LPVOID*)&Original_CControl_Update);
        MH_CreateHook((LPVOID)0x006fc1f0, &Hook_LoadConfig, (LPVOID*)&Original_LoadConfig);
        // MH_CreateHook((LPVOID)0x0076d560, &Hook_ApplyRumble, (LPVOID*)&Original_ApplyRumble);

        MH_CreateHook((LPVOID)0x004c8a20, &Hook_FilterIcon, (LPVOID*)&Original_FilterIcon);
        MH_CreateHook((LPVOID)0x004b1cf0, &Hook_ControlIcons, (LPVOID*)&Original_ControlIcons);

        MH_EnableHook(MH_ALL_HOOKS);
    }
    else if (ul_reason_for_call == DLL_PROCESS_DETACH) {
        if (s_sdlInitialized) SDL_Quit();
        MH_DisableHook(MH_ALL_HOOKS);
        MH_Uninitialize();
    }
    return TRUE;
}
