#include "platform_internal.h"

static LPCSTR
Platform_GetCursorResource (PlatformCursorShape shape)
{
    switch (shape)
    {
        case PLATFORM_CURSOR_SHAPE_ARROW: return IDC_ARROW;
        case PLATFORM_CURSOR_SHAPE_IBEAM: return IDC_IBEAM;
        case PLATFORM_CURSOR_SHAPE_CROSSHAIR: return IDC_CROSS;
        case PLATFORM_CURSOR_SHAPE_HAND: return IDC_HAND;
        case PLATFORM_CURSOR_SHAPE_SIZE_ALL: return IDC_SIZEALL;
        case PLATFORM_CURSOR_SHAPE_SIZE_NS: return IDC_SIZENS;
        case PLATFORM_CURSOR_SHAPE_SIZE_WE: return IDC_SIZEWE;
        case PLATFORM_CURSOR_SHAPE_SIZE_NESW: return IDC_SIZENESW;
        case PLATFORM_CURSOR_SHAPE_SIZE_NWSE: return IDC_SIZENWSE;
        case PLATFORM_CURSOR_SHAPE_NOT_ALLOWED: return IDC_NO;
    }

    return IDC_ARROW;
}

static HCURSOR
Platform_LoadCursorShape (PlatformCursorShape shape)
{
    return LoadCursorA(NULL, Platform_GetCursorResource(shape));
}

static void
Platform_ApplyCursor (void)
{
    if (!platform_state.cursor_is_visible)
    {
        SetCursor(NULL);
    }
    else
    {
        SetCursor(platform_state.current_cursor);
    }
}

void
PlatformCursor_SetShape (PlatformCursorShape shape)
{
    platform_state.cursor_shape = shape;
    platform_state.current_cursor = Platform_LoadCursorShape(shape);
    ASSERT(platform_state.current_cursor != NULL);
    Platform_ApplyCursor();
}

PlatformCursorShape
PlatformCursor_GetShape (void)
{
    return platform_state.cursor_shape;
}

void
PlatformCursor_Show (void)
{
    platform_state.cursor_is_visible = true;
    Platform_ApplyCursor();
}

void
PlatformCursor_Hide (void)
{
    platform_state.cursor_is_visible = false;
    Platform_ApplyCursor();
}

b32
PlatformCursor_IsVisible (void)
{
    return platform_state.cursor_is_visible;
}

b32
PlatformCursor_Capture (PlatformWindow window)
{
    PlatformWindowState *window_state;

    window_state = Platform_GetWindowState(window);
    ASSERT(window_state != NULL);

    SetCapture(window_state->hwnd);
    return GetCapture() == window_state->hwnd;
}

void
PlatformCursor_ReleaseCapture (void)
{
    ReleaseCapture();
}

b32
PlatformCursor_IsCaptured (PlatformWindow window)
{
    PlatformWindowState *window_state;

    window_state = Platform_GetWindowState(window);
    ASSERT(window_state != NULL);

    return GetCapture() == window_state->hwnd;
}

b32
PlatformCursor_ConfineToWindow (PlatformWindow window)
{
    PlatformWindowState *window_state;
    RECT rect;
    POINT top_left;
    POINT bottom_right;

    window_state = Platform_GetWindowState(window);
    ASSERT(window_state != NULL);

    if (GetClientRect(window_state->hwnd, &rect) == 0)
    {
        return false;
    }

    top_left.x = rect.left;
    top_left.y = rect.top;
    bottom_right.x = rect.right;
    bottom_right.y = rect.bottom;

    ClientToScreen(window_state->hwnd, &top_left);
    ClientToScreen(window_state->hwnd, &bottom_right);

    rect.left = top_left.x;
    rect.top = top_left.y;
    rect.right = bottom_right.x;
    rect.bottom = bottom_right.y;

    platform_state.cursor_is_confined = (ClipCursor(&rect) != 0);
    return platform_state.cursor_is_confined;
}

void
PlatformCursor_ClearConfine (void)
{
    ClipCursor(NULL);
    platform_state.cursor_is_confined = false;
}

b32
PlatformCursor_IsConfined (void)
{
    return platform_state.cursor_is_confined;
}
