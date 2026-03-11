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
    const c8 *title;
    i32 width;
    i32 height;
    PlatformWindowFlags flags;
} PlatformWindowDesc;

PlatformWindow PlatformWindow_Create (const PlatformWindowDesc *desc);
void PlatformWindow_Destroy (PlatformWindow window);
b32 PlatformWindow_IsValid (PlatformWindow window);
void PlatformWindow_Show (PlatformWindow window);
IVec2 PlatformWindow_GetClientSize (PlatformWindow window);

#endif // PLATFORM_WINDOW_H
