#ifndef PLATFORM_CURSOR_H
#define PLATFORM_CURSOR_H

#include "platform_window.h"

typedef enum PlatformCursorShape
{
    PLATFORM_CURSOR_SHAPE_ARROW = 0,
    PLATFORM_CURSOR_SHAPE_IBEAM,
    PLATFORM_CURSOR_SHAPE_CROSSHAIR,
    PLATFORM_CURSOR_SHAPE_HAND,
    PLATFORM_CURSOR_SHAPE_SIZE_ALL,
    PLATFORM_CURSOR_SHAPE_SIZE_NS,
    PLATFORM_CURSOR_SHAPE_SIZE_WE,
    PLATFORM_CURSOR_SHAPE_SIZE_NESW,
    PLATFORM_CURSOR_SHAPE_SIZE_NWSE,
    PLATFORM_CURSOR_SHAPE_NOT_ALLOWED,
} PlatformCursorShape;

void PlatformCursor_SetShape (PlatformCursorShape shape);
PlatformCursorShape PlatformCursor_GetShape (void);
void PlatformCursor_Show (void);
void PlatformCursor_Hide (void);
b32 PlatformCursor_IsVisible (void);
b32 PlatformCursor_Capture (PlatformWindow window);
void PlatformCursor_ReleaseCapture (void);
b32 PlatformCursor_IsCaptured (PlatformWindow window);
b32 PlatformCursor_ConfineToWindow (PlatformWindow window);
void PlatformCursor_ClearConfine (void);
b32 PlatformCursor_IsConfined (void);

#endif // PLATFORM_CURSOR_H
