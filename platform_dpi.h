#ifndef PLATFORM_DPI_H
#define PLATFORM_DPI_H

#include "platform_display.h"

typedef struct PlatformDPI
{
    u32 x;
    u32 y;
} PlatformDPI;

PlatformDPI Platform_GetDisplayDPI (PlatformDisplay display);
PlatformDPI Platform_GetWindowDPI (PlatformWindow window);
f32 Platform_GetDisplayScale (PlatformDisplay display);
f32 Platform_GetWindowScale (PlatformWindow window);

#endif // PLATFORM_DPI_H
