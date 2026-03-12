#ifndef PLATFORM_CLIPBOARD_H
#define PLATFORM_CLIPBOARD_H

#include "core.h"

b32 Platform_HasClipboardText (void);
String Platform_GetClipboardText (MemoryArena *arena);
b32 Platform_SetClipboardText (String text);

#endif // PLATFORM_CLIPBOARD_H
