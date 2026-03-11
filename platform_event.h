#ifndef PLATFORM_EVENT_H
#define PLATFORM_EVENT_H

#include "../Core/core.h"

typedef Handle64 PlatformWindow;

typedef enum PlatformEventType
{
    PLATFORM_EVENT_NONE = 0,
    PLATFORM_EVENT_QUIT,
    PLATFORM_EVENT_WINDOW_CLOSE_REQUESTED,
    PLATFORM_EVENT_WINDOW_MOVED,
    PLATFORM_EVENT_WINDOW_RESIZED,
    PLATFORM_EVENT_WINDOW_MINIMIZED,
    PLATFORM_EVENT_WINDOW_MAXIMIZED,
    PLATFORM_EVENT_WINDOW_RESTORED,
    PLATFORM_EVENT_WINDOW_FOCUS_GAINED,
    PLATFORM_EVENT_WINDOW_FOCUS_LOST,
    PLATFORM_EVENT_KEY_DOWN,
    PLATFORM_EVENT_KEY_UP,
    PLATFORM_EVENT_TEXT_INPUT,
    PLATFORM_EVENT_MOUSE_MOVE,
    PLATFORM_EVENT_MOUSE_BUTTON_DOWN,
    PLATFORM_EVENT_MOUSE_BUTTON_UP,
    PLATFORM_EVENT_MOUSE_WHEEL,
} PlatformEventType;

typedef enum PlatformKey
{
    PLATFORM_KEY_NONE = 0,
    PLATFORM_KEY_ESCAPE,
    PLATFORM_KEY_ENTER,
    PLATFORM_KEY_TAB,
    PLATFORM_KEY_BACKSPACE,
    PLATFORM_KEY_SPACE,
    PLATFORM_KEY_LEFT,
    PLATFORM_KEY_RIGHT,
    PLATFORM_KEY_UP,
    PLATFORM_KEY_DOWN,
    PLATFORM_KEY_HOME,
    PLATFORM_KEY_END,
    PLATFORM_KEY_PAGE_UP,
    PLATFORM_KEY_PAGE_DOWN,
    PLATFORM_KEY_INSERT,
    PLATFORM_KEY_DELETE,
    PLATFORM_KEY_SHIFT,
    PLATFORM_KEY_CONTROL,
    PLATFORM_KEY_ALT,
    PLATFORM_KEY_0,
    PLATFORM_KEY_1,
    PLATFORM_KEY_2,
    PLATFORM_KEY_3,
    PLATFORM_KEY_4,
    PLATFORM_KEY_5,
    PLATFORM_KEY_6,
    PLATFORM_KEY_7,
    PLATFORM_KEY_8,
    PLATFORM_KEY_9,
    PLATFORM_KEY_A,
    PLATFORM_KEY_B,
    PLATFORM_KEY_C,
    PLATFORM_KEY_D,
    PLATFORM_KEY_E,
    PLATFORM_KEY_F,
    PLATFORM_KEY_G,
    PLATFORM_KEY_H,
    PLATFORM_KEY_I,
    PLATFORM_KEY_J,
    PLATFORM_KEY_K,
    PLATFORM_KEY_L,
    PLATFORM_KEY_M,
    PLATFORM_KEY_N,
    PLATFORM_KEY_O,
    PLATFORM_KEY_P,
    PLATFORM_KEY_Q,
    PLATFORM_KEY_R,
    PLATFORM_KEY_S,
    PLATFORM_KEY_T,
    PLATFORM_KEY_U,
    PLATFORM_KEY_V,
    PLATFORM_KEY_W,
    PLATFORM_KEY_X,
    PLATFORM_KEY_Y,
    PLATFORM_KEY_Z,
} PlatformKey;

typedef enum PlatformMouseButton
{
    PLATFORM_MOUSE_BUTTON_NONE = 0,
    PLATFORM_MOUSE_BUTTON_LEFT,
    PLATFORM_MOUSE_BUTTON_RIGHT,
    PLATFORM_MOUSE_BUTTON_MIDDLE,
    PLATFORM_MOUSE_BUTTON_X1,
    PLATFORM_MOUSE_BUTTON_X2,
} PlatformMouseButton;

typedef struct PlatformEventWindowResized
{
    i32 width;
    i32 height;
} PlatformEventWindowResized;

typedef struct PlatformEventWindowMoved
{
    i32 x;
    i32 y;
} PlatformEventWindowMoved;

typedef struct PlatformEventKey
{
    PlatformKey key;
    b32 is_repeat;
} PlatformEventKey;

typedef struct PlatformEventTextInput
{
    u32 codepoint;
} PlatformEventTextInput;

typedef struct PlatformEventMouseMove
{
    i32 x;
    i32 y;
} PlatformEventMouseMove;

typedef struct PlatformEventMouseButtonData
{
    PlatformMouseButton button;
    i32 x;
    i32 y;
} PlatformEventMouseButtonData;

typedef struct PlatformEventMouseWheel
{
    i32 delta;
    i32 x;
    i32 y;
} PlatformEventMouseWheel;

typedef struct PlatformEvent
{
    PlatformEventType type;
    PlatformWindow window;
    Nanoseconds timestamp;

    union
    {
        PlatformEventWindowMoved window_moved;
        PlatformEventWindowResized window_resized;
        PlatformEventKey key;
        PlatformEventTextInput text_input;
        PlatformEventMouseMove mouse_move;
        PlatformEventMouseButtonData mouse_button;
        PlatformEventMouseWheel mouse_wheel;
    } data;
} PlatformEvent;

typedef struct PlatformEventBuffer
{
    PlatformEvent *events;
    usize count;
    usize capacity;
} PlatformEventBuffer;

PlatformEventBuffer PlatformEventBuffer_Create (PlatformEvent *events, usize capacity);
void PlatformEventBuffer_Reset (PlatformEventBuffer *buffer);

#endif // PLATFORM_EVENT_H
