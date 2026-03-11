#ifndef PLATFORM_INTERNAL_H
#define PLATFORM_INTERNAL_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>

#include "platform.h"

#define PLATFORM_MAX_WINDOWS 64
#define PLATFORM_WINDOW_CLASS_NAME L"MorganAudioPlatformWindowClass"

typedef struct PlatformWindowState
{
    b32 is_used;
    HWND hwnd;
    GenerationalHandle64 handle;
} PlatformWindowState;

typedef struct PlatformState
{
    b32 is_initialized;
    HINSTANCE instance;
    LARGE_INTEGER performance_frequency;
    ATOM window_class;
    PlatformWindowState windows[PLATFORM_MAX_WINDOWS];
    PlatformEventBuffer *active_event_buffer;
} PlatformState;

extern PlatformState platform_state;

PlatformWindowState *Platform_GetWindowState (PlatformWindow window);
PlatformWindow Platform_CreateWindowHandle (u32 index, u32 generation);
PlatformWindowState *Platform_AllocateWindowState (void);
void Platform_ReleaseWindowState (PlatformWindowState *window_state);
PlatformKey Platform_KeyFromVirtualKey (WPARAM virtual_key);
PlatformMouseButton Platform_MouseButtonFromMessage (UINT message, WPARAM w_param);
Nanoseconds Platform_QueryTimestamp (void);
void Platform_PushEvent (const PlatformEvent *event);
LRESULT CALLBACK Platform_WindowProc (HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param);

#endif // PLATFORM_INTERNAL_H
