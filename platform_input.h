#ifndef PLATFORM_INPUT_H
#define PLATFORM_INPUT_H

#include "platform_event.h"

typedef struct PlatformInputState
{
    b32 keys[PLATFORM_KEY_COUNT];
    b32 mouse_buttons[PLATFORM_MOUSE_BUTTON_COUNT];
    IVec2 mouse_position;
    IVec2 mouse_delta;
    i32 mouse_wheel_delta;
    b32 shift_is_down;
    b32 control_is_down;
    b32 alt_is_down;
} PlatformInputState;

const PlatformInputState *Platform_GetInputState (void);
b32 Platform_KeyIsDown (PlatformKey key);
b32 Platform_MouseButtonIsDown (PlatformMouseButton button);

#endif // PLATFORM_INPUT_H
