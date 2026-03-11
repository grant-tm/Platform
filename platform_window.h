#ifndef PLATFORM_WINDOW_H
#define PLATFORM_WINDOW_H

#include "platform_event.h"

typedef enum PlatformWindowFlags
{
    PLATFORM_WINDOW_FLAG_NONE = 0,
    PLATFORM_WINDOW_FLAG_VISIBLE = BIT_U32(0),
    PLATFORM_WINDOW_FLAG_RESIZABLE = BIT_U32(1),
} PlatformWindowFlags;

typedef struct PlatformWindowDesc
{
    String title;
    i32 width;
    i32 height;
    PlatformWindowFlags flags;
} PlatformWindowDesc;

PlatformWindow PlatformWindow_Create (const PlatformWindowDesc *desc);
void PlatformWindow_Destroy (PlatformWindow window);
b32 PlatformWindow_IsValid (PlatformWindow window);
void PlatformWindow_SetTitle (PlatformWindow window, String title);
void PlatformWindow_Show (PlatformWindow window);
void PlatformWindow_Hide (PlatformWindow window);
void PlatformWindow_Focus (PlatformWindow window);
void PlatformWindow_Minimize (PlatformWindow window);
void PlatformWindow_Maximize (PlatformWindow window);
void PlatformWindow_Restore (PlatformWindow window);
IVec2 PlatformWindow_GetPosition (PlatformWindow window);
void PlatformWindow_SetPosition (PlatformWindow window, i32 x, i32 y);
IVec2 PlatformWindow_GetWindowSize (PlatformWindow window);
void PlatformWindow_SetClientSize (PlatformWindow window, i32 width, i32 height);
IVec2 PlatformWindow_GetClientSize (PlatformWindow window);
b32 PlatformWindow_IsFocused (PlatformWindow window);

#endif // PLATFORM_WINDOW_H
