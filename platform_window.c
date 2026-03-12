#include "platform_internal.h"

#include <shellapi.h>

static DWORD Platform_GetWindowStyle (PlatformWindowFlags flags)
{
    DWORD style;

    style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    if (Bits_HasAnyU32(flags, PLATFORM_WINDOW_FLAG_RESIZABLE))
    {
        style |= WS_THICKFRAME | WS_MAXIMIZEBOX;
    }

    return style;
}

static void Platform_AdjustWindowRectForClientSize (HWND hwnd, i32 client_width, i32 client_height, i32 *window_width, i32 *window_height)
{
    RECT client_rect;
    DWORD style;
    DWORD extended_style;

    ASSERT(hwnd != NULL);
    ASSERT(window_width != NULL);
    ASSERT(window_height != NULL);
    ASSERT(client_width > 0);
    ASSERT(client_height > 0);

    style = (DWORD) GetWindowLongPtrW(hwnd, GWL_STYLE);
    extended_style = (DWORD) GetWindowLongPtrW(hwnd, GWL_EXSTYLE);

    client_rect.left = 0;
    client_rect.top = 0;
    client_rect.right = client_width;
    client_rect.bottom = client_height;

    AdjustWindowRectEx(&client_rect, style, FALSE, extended_style);

    *window_width = client_rect.right - client_rect.left;
    *window_height = client_rect.bottom - client_rect.top;
}

static void PlatformWindow_SetTitleCString (HWND hwnd, String title)
{
    MemoryArena arena;
    c8 stack_buffer[512];
    c8 *title_buffer;

    ASSERT(hwnd != NULL);

    arena = MemoryArena_Create(stack_buffer, sizeof(stack_buffer));
    title_buffer = MemoryArena_PushArray(&arena, c8, title.count + 1);
    ASSERT(title_buffer != NULL);

    if (title.count > 0)
    {
        Memory_Copy(title_buffer, title.data, title.count);
    }

    title_buffer[title.count] = 0;
    SetWindowTextA(hwnd, title_buffer);
}

PlatformWindow PlatformWindow_Create (const PlatformWindowDesc *desc)
{
    PlatformWindowState *window_state;
    RECT client_rect;
    DWORD style;
    HWND hwnd;

    ASSERT(desc != NULL);
    ASSERT(desc->title.data != NULL);
    ASSERT(desc->title.count > 0);
    ASSERT(desc->width > 0);
    ASSERT(desc->height > 0);
    ASSERT(platform_state.is_initialized);

    window_state = Platform_AllocateWindowState();
    if (window_state == NULL)
    {
        return HANDLE64_INVALID;
    }

    style = Platform_GetWindowStyle(desc->flags);

    client_rect.left = 0;
    client_rect.top = 0;
    client_rect.right = desc->width;
    client_rect.bottom = desc->height;
    AdjustWindowRect(&client_rect, style, FALSE);

    hwnd = CreateWindowExW(
        0,
        PLATFORM_WINDOW_CLASS_NAME,
        L"",
        style,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        client_rect.right - client_rect.left,
        client_rect.bottom - client_rect.top,
        NULL,
        NULL,
        platform_state.instance,
        window_state
    );

    if (hwnd == NULL)
    {
        Platform_ReleaseWindowState(window_state);
        return HANDLE64_INVALID;
    }

    window_state->flags = desc->flags;

    PlatformWindow_SetTitle(Platform_CreateWindowHandle(window_state->handle.index, window_state->handle.generation), desc->title);

    if (Bits_HasAnyU32(desc->flags, PLATFORM_WINDOW_FLAG_ACCEPTS_DROP))
    {
        DragAcceptFiles(hwnd, TRUE);
    }

    if (Bits_HasAnyU32(desc->flags, PLATFORM_WINDOW_FLAG_VISIBLE))
    {
        ShowWindow(hwnd, SW_SHOW);
    }

    return Platform_CreateWindowHandle(window_state->handle.index, window_state->handle.generation);
}

void PlatformWindow_Destroy (PlatformWindow window)
{
    PlatformWindowState *window_state;

    window_state = Platform_GetWindowState(window);
    if (window_state == NULL)
    {
        return;
    }

    DestroyWindow(window_state->hwnd);
}

b32 PlatformWindow_IsValid (PlatformWindow window)
{
    return Platform_GetWindowState(window) != NULL;
}

void PlatformWindow_SetTitle (PlatformWindow window, String title)
{
    PlatformWindowState *window_state;
    ASSERT(title.data != NULL);
    ASSERT(title.count > 0);

    window_state = Platform_GetWindowState(window);
    ASSERT(window_state != NULL);

    PlatformWindow_SetTitleCString(window_state->hwnd, title);
}

void PlatformWindow_Show (PlatformWindow window)
{
    PlatformWindowState *window_state;

    window_state = Platform_GetWindowState(window);
    ASSERT(window_state != NULL);

    ShowWindow(window_state->hwnd, SW_SHOW);
}

void PlatformWindow_Hide (PlatformWindow window)
{
    PlatformWindowState *window_state;

    window_state = Platform_GetWindowState(window);
    ASSERT(window_state != NULL);

    ShowWindow(window_state->hwnd, SW_HIDE);
}

void PlatformWindow_Focus (PlatformWindow window)
{
    PlatformWindowState *window_state;

    window_state = Platform_GetWindowState(window);
    ASSERT(window_state != NULL);

    SetForegroundWindow(window_state->hwnd);
    SetFocus(window_state->hwnd);
}

void PlatformWindow_Minimize (PlatformWindow window)
{
    PlatformWindowState *window_state;

    window_state = Platform_GetWindowState(window);
    ASSERT(window_state != NULL);

    ShowWindow(window_state->hwnd, SW_MINIMIZE);
}

void PlatformWindow_Maximize (PlatformWindow window)
{
    PlatformWindowState *window_state;

    window_state = Platform_GetWindowState(window);
    ASSERT(window_state != NULL);

    ShowWindow(window_state->hwnd, SW_MAXIMIZE);
}

void PlatformWindow_Restore (PlatformWindow window)
{
    PlatformWindowState *window_state;

    window_state = Platform_GetWindowState(window);
    ASSERT(window_state != NULL);

    ShowWindow(window_state->hwnd, SW_RESTORE);
}

IVec2 PlatformWindow_GetPosition (PlatformWindow window)
{
    PlatformWindowState *window_state;
    RECT rect;

    window_state = Platform_GetWindowState(window);
    ASSERT(window_state != NULL);

    GetWindowRect(window_state->hwnd, &rect);
    return IVec2_Create(rect.left, rect.top);
}

void PlatformWindow_SetPosition (PlatformWindow window, i32 x, i32 y)
{
    PlatformWindowState *window_state;
    RECT rect;

    window_state = Platform_GetWindowState(window);
    ASSERT(window_state != NULL);

    GetWindowRect(window_state->hwnd, &rect);
    SetWindowPos(window_state->hwnd, NULL, x, y, rect.right - rect.left, rect.bottom - rect.top, SWP_NOZORDER | SWP_NOACTIVATE);
}

IVec2 PlatformWindow_GetWindowSize (PlatformWindow window)
{
    PlatformWindowState *window_state;
    RECT rect;

    window_state = Platform_GetWindowState(window);
    ASSERT(window_state != NULL);

    GetWindowRect(window_state->hwnd, &rect);
    return IVec2_Create(rect.right - rect.left, rect.bottom - rect.top);
}

void PlatformWindow_SetClientSize (PlatformWindow window, i32 width, i32 height)
{
    PlatformWindowState *window_state;
    RECT rect;
    i32 window_width;
    i32 window_height;

    ASSERT(width > 0);
    ASSERT(height > 0);

    window_state = Platform_GetWindowState(window);
    ASSERT(window_state != NULL);

    Platform_AdjustWindowRectForClientSize(window_state->hwnd, width, height, &window_width, &window_height);
    GetWindowRect(window_state->hwnd, &rect);

    SetWindowPos(window_state->hwnd, NULL, rect.left, rect.top, window_width, window_height, SWP_NOZORDER | SWP_NOACTIVATE);
}

IVec2 PlatformWindow_GetClientSize (PlatformWindow window)
{
    PlatformWindowState *window_state;
    RECT rect;

    window_state = Platform_GetWindowState(window);
    ASSERT(window_state != NULL);

    GetClientRect(window_state->hwnd, &rect);
    return IVec2_Create(rect.right - rect.left, rect.bottom - rect.top);
}

b32 PlatformWindow_IsFocused (PlatformWindow window)
{
    PlatformWindowState *window_state;

    window_state = Platform_GetWindowState(window);
    ASSERT(window_state != NULL);

    return GetFocus() == window_state->hwnd;
}
