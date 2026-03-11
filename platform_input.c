#include "platform_internal.h"

const PlatformInputState *Platform_GetInputState (void)
{
    return &platform_state.input_state;
}

b32 Platform_KeyIsDown (PlatformKey key)
{
    ASSERT((key >= 0) && (key < PLATFORM_KEY_COUNT));

    if (key == PLATFORM_KEY_NONE)
    {
        return false;
    }

    return platform_state.input_state.keys[key];
}

b32 Platform_MouseButtonIsDown (PlatformMouseButton button)
{
    ASSERT((button >= 0) && (button < PLATFORM_MOUSE_BUTTON_COUNT));

    if (button == PLATFORM_MOUSE_BUTTON_NONE)
    {
        return false;
    }

    return platform_state.input_state.mouse_buttons[button];
}

void Platform_ResetTransientInputState (void)
{
    platform_state.input_state.mouse_delta = IVec2_Create(0, 0);
    platform_state.input_state.mouse_wheel_delta = 0;
}

void Platform_UpdateModifierState (void)
{
    platform_state.input_state.shift_is_down = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    platform_state.input_state.control_is_down = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    platform_state.input_state.alt_is_down = (GetKeyState(VK_MENU) & 0x8000) != 0;

    platform_state.input_state.keys[PLATFORM_KEY_SHIFT] = platform_state.input_state.shift_is_down;
    platform_state.input_state.keys[PLATFORM_KEY_CONTROL] = platform_state.input_state.control_is_down;
    platform_state.input_state.keys[PLATFORM_KEY_ALT] = platform_state.input_state.alt_is_down;
}
