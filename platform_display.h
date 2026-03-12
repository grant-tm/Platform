#ifndef PLATFORM_DISPLAY_H
#define PLATFORM_DISPLAY_H

#include "platform_window.h"

typedef Handle64 PlatformDisplay;

typedef struct PlatformDisplayRect
{
    i32 x;
    i32 y;
    i32 width;
    i32 height;
} PlatformDisplayRect;

typedef struct PlatformDisplayInfo
{
    String name;
    PlatformDisplayRect bounds;
    PlatformDisplayRect work_area;
    b32 is_primary;
} PlatformDisplayInfo;

static const PlatformDisplay PLATFORM_DISPLAY_INVALID = HANDLE64_INVALID;

i32 Platform_GetDisplayCount (void);
PlatformDisplay Platform_GetDisplayByIndex (i32 index);
PlatformDisplay Platform_GetPrimaryDisplay (void);
PlatformDisplay Platform_GetDisplayForWindow (PlatformWindow window);
b32 PlatformDisplay_IsValid (PlatformDisplay display);
b32 Platform_GetDisplayInfo (PlatformDisplay display, MemoryArena *arena, PlatformDisplayInfo *info);

#endif // PLATFORM_DISPLAY_H
