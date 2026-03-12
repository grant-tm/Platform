#include "platform_internal.h"

typedef struct PlatformDisplayEnumState
{
    i32 target_index;
    i32 current_index;
    PlatformDisplay result;
} PlatformDisplayEnumState;

static PlatformDisplay
Platform_CreateDisplayHandle (HMONITOR monitor)
{
    return (PlatformDisplay) (u64) (uintptr_t) monitor;
}

static HMONITOR
Platform_GetDisplayMonitor (PlatformDisplay display)
{
    return (HMONITOR) (uintptr_t) display;
}

static PlatformDisplayRect
Platform_CreateDisplayRect (const RECT *rect)
{
    PlatformDisplayRect result;

    ASSERT(rect != NULL);

    result.x = rect->left;
    result.y = rect->top;
    result.width = rect->right - rect->left;
    result.height = rect->bottom - rect->top;
    return result;
}

static BOOL CALLBACK
Platform_CountDisplaysProc (HMONITOR monitor, HDC device_context, LPRECT rect, LPARAM user_data)
{
    i32 *count;

    (void) monitor;
    (void) device_context;
    (void) rect;

    count = (i32 *) user_data;
    *count += 1;
    return TRUE;
}

static BOOL CALLBACK
Platform_FindDisplayByIndexProc (HMONITOR monitor, HDC device_context, LPRECT rect, LPARAM user_data)
{
    PlatformDisplayEnumState *state;

    (void) device_context;
    (void) rect;

    state = (PlatformDisplayEnumState *) user_data;
    if (state->current_index == state->target_index)
    {
        state->result = Platform_CreateDisplayHandle(monitor);
        return FALSE;
    }

    state->current_index += 1;
    return TRUE;
}

i32
Platform_GetDisplayCount (void)
{
    i32 count;

    count = 0;
    EnumDisplayMonitors(NULL, NULL, Platform_CountDisplaysProc, (LPARAM) &count);
    return count;
}

PlatformDisplay
Platform_GetDisplayByIndex (i32 index)
{
    PlatformDisplayEnumState state;

    if (index < 0)
    {
        return PLATFORM_DISPLAY_INVALID;
    }

    state.target_index = index;
    state.current_index = 0;
    state.result = PLATFORM_DISPLAY_INVALID;
    EnumDisplayMonitors(NULL, NULL, Platform_FindDisplayByIndexProc, (LPARAM) &state);
    return state.result;
}

PlatformDisplay
Platform_GetPrimaryDisplay (void)
{
    POINT origin;
    HMONITOR monitor;

    origin.x = 0;
    origin.y = 0;
    monitor = MonitorFromPoint(origin, MONITOR_DEFAULTTOPRIMARY);
    if (monitor == NULL)
    {
        return PLATFORM_DISPLAY_INVALID;
    }

    return Platform_CreateDisplayHandle(monitor);
}

PlatformDisplay
Platform_GetDisplayForWindow (PlatformWindow window)
{
    PlatformWindowState *window_state;
    HMONITOR monitor;

    window_state = Platform_GetWindowState(window);
    ASSERT(window_state != NULL);

    monitor = MonitorFromWindow(window_state->hwnd, MONITOR_DEFAULTTONEAREST);
    if (monitor == NULL)
    {
        return PLATFORM_DISPLAY_INVALID;
    }

    return Platform_CreateDisplayHandle(monitor);
}

b32
PlatformDisplay_IsValid (PlatformDisplay display)
{
    return Handle64_IsValid(display);
}

b32
Platform_GetDisplayInfo (PlatformDisplay display, MemoryArena *arena, PlatformDisplayInfo *info)
{
    HMONITOR monitor;
    MONITORINFOEXA monitor_info;
    usize name_length;
    c8 *name_copy;

    ASSERT(arena != NULL);
    ASSERT(info != NULL);

    monitor = Platform_GetDisplayMonitor(display);
    if (monitor == NULL)
    {
        return false;
    }

    Memory_Zero(info, sizeof(*info));
    monitor_info.cbSize = sizeof(monitor_info);
    if (GetMonitorInfoA(monitor, (MONITORINFO *) &monitor_info) == 0)
    {
        return false;
    }

    name_length = CString_Length((c8 *) monitor_info.szDevice);
    name_copy = MemoryArena_PushArray(arena, c8, name_length + 1);
    if (name_copy == NULL)
    {
        return false;
    }

    Memory_Copy(name_copy, monitor_info.szDevice, name_length + 1);

    info->name = String_Create(name_copy, name_length);
    info->bounds = Platform_CreateDisplayRect(&monitor_info.rcMonitor);
    info->work_area = Platform_CreateDisplayRect(&monitor_info.rcWork);
    info->is_primary = (monitor_info.dwFlags & MONITORINFOF_PRIMARY) != 0;
    return true;
}
