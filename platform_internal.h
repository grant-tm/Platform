#ifndef PLATFORM_INTERNAL_H
#define PLATFORM_INTERNAL_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>

#include "platform.h"

#define PLATFORM_MAX_WINDOWS 64
#define PLATFORM_MAX_PENDING_EVENTS 1024
#define PLATFORM_MAX_DROPPED_FILES 64
#define PLATFORM_MAX_DROPPED_PATH_BYTES 16384
#define PLATFORM_WINDOW_CLASS_NAME L"MorganAudioPlatformWindowClass"

void PlatformAudio_Initialize (void);
void PlatformAudio_Shutdown (void);
void PlatformDPI_Initialize (void);

typedef struct PlatformWindowState
{
    b32 is_used;
    HWND hwnd;
    GenerationalHandle64 handle;
    PlatformWindowFlags flags;
} PlatformWindowState;

typedef struct PlatformState
{
    b32 is_initialized;
    HINSTANCE instance;
    LARGE_INTEGER performance_frequency;
    ATOM window_class;
    HCURSOR current_cursor;
    PlatformCursorShape cursor_shape;
    b32 cursor_is_visible;
    b32 cursor_is_confined;
    PlatformInputState input_state;
    PlatformWindowState windows[PLATFORM_MAX_WINDOWS];
    PlatformEvent pending_events[PLATFORM_MAX_PENDING_EVENTS];
    usize pending_event_count;
    String dropped_paths[PLATFORM_MAX_DROPPED_FILES];
    usize dropped_path_count;
    c8 dropped_path_bytes[PLATFORM_MAX_DROPPED_PATH_BYTES];
    usize dropped_path_byte_count;
    PlatformModalTickFunction *modal_tick_callback;
    void *modal_tick_user_data;
    b32 is_in_modal_loop;
    b32 is_in_modal_tick;
    PlatformEvent modal_tick_events[PLATFORM_MAX_PENDING_EVENTS];
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
void Platform_ResetTransientInputState (void);
void Platform_UpdateModifierState (void);
LRESULT CALLBACK Platform_WindowProc (HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param);

#endif // PLATFORM_INTERNAL_H
