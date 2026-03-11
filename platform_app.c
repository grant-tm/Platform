#include "platform_internal.h"

PlatformState platform_state = {0};

static b32 Platform_RegisterWindowClass (void)
{
    WNDCLASSEXW window_class = {0};

    window_class.cbSize = sizeof(window_class);
    window_class.style = CS_HREDRAW | CS_VREDRAW;
    window_class.lpfnWndProc = Platform_WindowProc;
    window_class.hInstance = platform_state.instance;
    window_class.hCursor = LoadCursorW(NULL, (LPCWSTR) IDC_ARROW);
    window_class.lpszClassName = PLATFORM_WINDOW_CLASS_NAME;

    platform_state.window_class = RegisterClassExW(&window_class);
    return platform_state.window_class != 0;
}

static i32 Platform_CountLiveWindows (void)
{
    i32 index;
    i32 live_count;

    live_count = 0;
    for (index = 0; index < PLATFORM_MAX_WINDOWS; index += 1)
    {
        if (platform_state.windows[index].is_used)
        {
            live_count += 1;
        }
    }

    return live_count;
}

b32 Platform_Initialize (void)
{
    if (platform_state.is_initialized)
    {
        return true;
    }

    platform_state.instance = GetModuleHandleW(NULL);
    ASSERT(platform_state.instance != NULL);

    ASSERT(QueryPerformanceFrequency(&platform_state.performance_frequency) != 0);

    if (!Platform_RegisterWindowClass())
    {
        return false;
    }

    platform_state.is_initialized = true;
    return true;
}

void Platform_Shutdown (void)
{
    i32 index;

    if (!platform_state.is_initialized)
    {
        return;
    }

    for (index = 0; index < PLATFORM_MAX_WINDOWS; index += 1)
    {
        if (platform_state.windows[index].is_used)
        {
            DestroyWindow(platform_state.windows[index].hwnd);
        }
    }

    if (platform_state.window_class != 0)
    {
        UnregisterClassW(PLATFORM_WINDOW_CLASS_NAME, platform_state.instance);
    }

    Memory_Zero(&platform_state, sizeof(platform_state));
}

b32 Platform_IsInitialized (void)
{
    return platform_state.is_initialized;
}

void Platform_PushEvent (const PlatformEvent *event)
{
    PlatformEventBuffer *buffer;

    buffer = platform_state.active_event_buffer;
    if ((buffer == NULL) || (buffer->events == NULL))
    {
        return;
    }

    if (buffer->count >= buffer->capacity)
    {
        return;
    }

    buffer->events[buffer->count] = *event;
    buffer->count += 1;
}

void Platform_PumpEvents (PlatformEventBuffer *event_buffer)
{
    MSG message;

    ASSERT(platform_state.is_initialized);
    ASSERT(event_buffer != NULL);

    PlatformEventBuffer_Reset(event_buffer);
    platform_state.active_event_buffer = event_buffer;

    while (PeekMessageW(&message, NULL, 0, 0, PM_REMOVE))
    {
        if (message.message == WM_QUIT)
        {
            PlatformEvent event = {0};

            event.type = PLATFORM_EVENT_QUIT;
            event.timestamp = Platform_QueryTimestamp();
            Platform_PushEvent(&event);
            continue;
        }

        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    platform_state.active_event_buffer = NULL;
}

PlatformWindow Platform_CreateWindowHandle (u32 index, u32 generation)
{
    return GenerationalHandle64_Pack(GenerationalHandle64_Create(index, generation));
}

PlatformWindowState *Platform_AllocateWindowState (void)
{
    i32 index;

    for (index = 0; index < PLATFORM_MAX_WINDOWS; index += 1)
    {
        PlatformWindowState *window_state = &platform_state.windows[index];

        if (!window_state->is_used)
        {
            window_state->is_used = true;
            window_state->handle.index = (u32) index + 1;
            window_state->handle.generation += 1;

            if (window_state->handle.generation == 0)
            {
                window_state->handle.generation = 1;
            }

            return window_state;
        }
    }

    return NULL;
}

void Platform_ReleaseWindowState (PlatformWindowState *window_state)
{
    ASSERT(window_state != NULL);

    window_state->is_used = false;
    window_state->hwnd = NULL;
}

PlatformWindowState *Platform_GetWindowState (PlatformWindow window)
{
    GenerationalHandle64 handle;
    u32 slot_index;
    PlatformWindowState *window_state;

    if (!Handle64_IsValid(window))
    {
        return NULL;
    }

    handle = GenerationalHandle64_Unpack(window);
    if (!GenerationalHandle64_IsValid(handle))
    {
        return NULL;
    }

    slot_index = handle.index - 1;
    if (slot_index >= PLATFORM_MAX_WINDOWS)
    {
        return NULL;
    }

    window_state = &platform_state.windows[slot_index];
    if (!window_state->is_used)
    {
        return NULL;
    }

    if (!GenerationalHandle64_Equals(window_state->handle, handle))
    {
        return NULL;
    }

    return window_state;
}

static void Platform_PushWindowEvent (PlatformWindow window, PlatformEventType type)
{
    PlatformEvent event = {0};

    event.type = type;
    event.window = window;
    event.timestamp = Platform_QueryTimestamp();
    Platform_PushEvent(&event);
}

PlatformKey Platform_KeyFromVirtualKey (WPARAM virtual_key)
{
    if ((virtual_key >= '0') && (virtual_key <= '9'))
    {
        return (PlatformKey) (PLATFORM_KEY_0 + (virtual_key - '0'));
    }

    if ((virtual_key >= 'A') && (virtual_key <= 'Z'))
    {
        return (PlatformKey) (PLATFORM_KEY_A + (virtual_key - 'A'));
    }

    switch (virtual_key)
    {
        case VK_ESCAPE: return PLATFORM_KEY_ESCAPE;
        case VK_RETURN: return PLATFORM_KEY_ENTER;
        case VK_TAB: return PLATFORM_KEY_TAB;
        case VK_BACK: return PLATFORM_KEY_BACKSPACE;
        case VK_SPACE: return PLATFORM_KEY_SPACE;
        case VK_LEFT: return PLATFORM_KEY_LEFT;
        case VK_RIGHT: return PLATFORM_KEY_RIGHT;
        case VK_UP: return PLATFORM_KEY_UP;
        case VK_DOWN: return PLATFORM_KEY_DOWN;
        case VK_HOME: return PLATFORM_KEY_HOME;
        case VK_END: return PLATFORM_KEY_END;
        case VK_PRIOR: return PLATFORM_KEY_PAGE_UP;
        case VK_NEXT: return PLATFORM_KEY_PAGE_DOWN;
        case VK_INSERT: return PLATFORM_KEY_INSERT;
        case VK_DELETE: return PLATFORM_KEY_DELETE;
        case VK_SHIFT:
        case VK_LSHIFT:
        case VK_RSHIFT: return PLATFORM_KEY_SHIFT;
        case VK_CONTROL:
        case VK_LCONTROL:
        case VK_RCONTROL: return PLATFORM_KEY_CONTROL;
        case VK_MENU:
        case VK_LMENU:
        case VK_RMENU: return PLATFORM_KEY_ALT;
    }

    return PLATFORM_KEY_NONE;
}

PlatformMouseButton Platform_MouseButtonFromMessage (UINT message, WPARAM w_param)
{
    switch (message)
    {
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP: return PLATFORM_MOUSE_BUTTON_LEFT;
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP: return PLATFORM_MOUSE_BUTTON_RIGHT;
        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP: return PLATFORM_MOUSE_BUTTON_MIDDLE;
        case WM_XBUTTONDOWN:
        case WM_XBUTTONUP:
        {
            return (HIWORD(w_param) == XBUTTON1) ? PLATFORM_MOUSE_BUTTON_X1 : PLATFORM_MOUSE_BUTTON_X2;
        }
    }

    return PLATFORM_MOUSE_BUTTON_NONE;
}

LRESULT CALLBACK Platform_WindowProc (HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param)
{
    PlatformWindowState *window_state;
    PlatformWindow window;

    window_state = (PlatformWindowState *) GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    window = (window_state != NULL) ? Platform_CreateWindowHandle(window_state->handle.index, window_state->handle.generation) : HANDLE64_INVALID;

    switch (message)
    {
        case WM_NCCREATE:
        {
            CREATESTRUCTW *create_struct = (CREATESTRUCTW *) l_param;

            window_state = (PlatformWindowState *) create_struct->lpCreateParams;
            ASSERT(window_state != NULL);

            SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR) window_state);
            window_state->hwnd = hwnd;
            return TRUE;
        }

        case WM_CLOSE:
        {
            Platform_PushWindowEvent(window, PLATFORM_EVENT_WINDOW_CLOSE_REQUESTED);
            return 0;
        }

        case WM_SETFOCUS:
        {
            Platform_PushWindowEvent(window, PLATFORM_EVENT_WINDOW_FOCUS_GAINED);
            return 0;
        }

        case WM_KILLFOCUS:
        {
            Platform_PushWindowEvent(window, PLATFORM_EVENT_WINDOW_FOCUS_LOST);
            return 0;
        }

        case WM_SIZE:
        {
            PlatformEvent event = {0};

            event.type = PLATFORM_EVENT_WINDOW_RESIZED;
            event.window = window;
            event.timestamp = Platform_QueryTimestamp();
            event.data.window_resized.width = (i32) LOWORD(l_param);
            event.data.window_resized.height = (i32) HIWORD(l_param);
            Platform_PushEvent(&event);
            return 0;
        }

        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYUP:
        {
            PlatformEvent event = {0};

            event.type = ((message == WM_KEYDOWN) || (message == WM_SYSKEYDOWN)) ? PLATFORM_EVENT_KEY_DOWN : PLATFORM_EVENT_KEY_UP;
            event.window = window;
            event.timestamp = Platform_QueryTimestamp();
            event.data.key.key = Platform_KeyFromVirtualKey(w_param);
            event.data.key.is_repeat = (l_param & (1 << 30)) != 0;
            Platform_PushEvent(&event);
            return 0;
        }

        case WM_CHAR:
        {
            PlatformEvent event = {0};

            event.type = PLATFORM_EVENT_TEXT_INPUT;
            event.window = window;
            event.timestamp = Platform_QueryTimestamp();
            event.data.text_input.codepoint = (u32) w_param;
            Platform_PushEvent(&event);
            return 0;
        }

        case WM_MOUSEMOVE:
        {
            PlatformEvent event = {0};

            event.type = PLATFORM_EVENT_MOUSE_MOVE;
            event.window = window;
            event.timestamp = Platform_QueryTimestamp();
            event.data.mouse_move.x = GET_X_LPARAM(l_param);
            event.data.mouse_move.y = GET_Y_LPARAM(l_param);
            Platform_PushEvent(&event);
            return 0;
        }

        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_MBUTTONDOWN:
        case WM_XBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_RBUTTONUP:
        case WM_MBUTTONUP:
        case WM_XBUTTONUP:
        {
            PlatformEvent event = {0};

            event.type = ((message == WM_LBUTTONDOWN) || (message == WM_RBUTTONDOWN) || (message == WM_MBUTTONDOWN) || (message == WM_XBUTTONDOWN))
                ? PLATFORM_EVENT_MOUSE_BUTTON_DOWN
                : PLATFORM_EVENT_MOUSE_BUTTON_UP;
            event.window = window;
            event.timestamp = Platform_QueryTimestamp();
            event.data.mouse_button.button = Platform_MouseButtonFromMessage(message, w_param);
            event.data.mouse_button.x = GET_X_LPARAM(l_param);
            event.data.mouse_button.y = GET_Y_LPARAM(l_param);
            Platform_PushEvent(&event);
            return 0;
        }

        case WM_MOUSEWHEEL:
        {
            PlatformEvent event = {0};

            event.type = PLATFORM_EVENT_MOUSE_WHEEL;
            event.window = window;
            event.timestamp = Platform_QueryTimestamp();
            event.data.mouse_wheel.delta = (i32) GET_WHEEL_DELTA_WPARAM(w_param);
            event.data.mouse_wheel.x = GET_X_LPARAM(l_param);
            event.data.mouse_wheel.y = GET_Y_LPARAM(l_param);
            Platform_PushEvent(&event);
            return 0;
        }

        case WM_NCDESTROY:
        {
            if (window_state != NULL)
            {
                Platform_ReleaseWindowState(window_state);

                if (Platform_CountLiveWindows() == 0)
                {
                    PostQuitMessage(0);
                }
            }

            SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
            return DefWindowProcW(hwnd, message, w_param, l_param);
        }
    }

    return DefWindowProcW(hwnd, message, w_param, l_param);
}
