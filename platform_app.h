#ifndef PLATFORM_APP_H
#define PLATFORM_APP_H

#include "platform_input.h"

b32 Platform_Initialize (void);
void Platform_Shutdown (void);
b32 Platform_IsInitialized (void);
void Platform_PumpEvents (PlatformEventBuffer *event_buffer);

#endif // PLATFORM_APP_H
