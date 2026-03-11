#include "platform_internal.h"

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

PlatformWindow PlatformWindow_Create (const PlatformWindowDesc *desc)
{
    PlatformWindowState *window_state;
    RECT client_rect;
    DWORD style;
    HWND hwnd;

    ASSERT(desc != NULL);
    ASSERT(desc->title != NULL);
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

    SetWindowTextA(hwnd, desc->title);

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

void PlatformWindow_Show (PlatformWindow window)
{
    PlatformWindowState *window_state;

    window_state = Platform_GetWindowState(window);
    ASSERT(window_state != NULL);

    ShowWindow(window_state->hwnd, SW_SHOW);
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
