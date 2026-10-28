#pragma once
#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <SDL3/SDL.h>
#include <MinHook.h>
#include <cmath>
#include <stdio.h>

// MEMORY ADDRESSES
extern DWORD* ENGINE_DEVICE_COUNT;
extern void* ENGINE_DEVICE_TABLE;

// TYPEDEFS
typedef void(__thiscall* PushEvent_t)(void* pControl, int state, int logicalAction, int bitmask);
typedef int(__fastcall* INPUT_init_t)(void* ecx, void* edx);
typedef void(__fastcall* CControl_Update_t)(int* pControl, void* edx);
typedef DWORD(__fastcall* LoadConfig_t)(void* pThis, void* edx, char* file, char* devName);
typedef void(__cdecl* ApplyRumble_t)(int deviceIndex, int intensity);
typedef void(__cdecl* ControlIcons_t)(int bitmask, char* outString);
typedef void(__cdecl* FilterIcon_t)(void* param_1, char* param_2);
typedef void(__thiscall* StringAssign_t)(void* stringObject, const char* value);

// GLOBAL POINTERS
extern PushEvent_t PushEvent;
extern StringAssign_t Real_StringAssign;

extern INPUT_init_t Original_INPUT_init;
extern CControl_Update_t Original_CControl_Update;
extern ApplyRumble_t Original_ApplyRumble;
extern ControlIcons_t Original_ControlIcons;
extern FilterIcon_t Original_FilterIcon;

// GLOBAL STATE
extern int g_iconMode;
extern SDL_Gamepad* activePads[4];
extern SDL_Joystick* activeJoys[4];
extern bool s_sdlInitialized;
extern int firstPadDeviceIndex;
