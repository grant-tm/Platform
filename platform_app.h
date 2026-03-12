#ifndef PLATFORM_APP_H
#define PLATFORM_APP_H

#include "platform_input.h"

typedef void PlatformModalTickFunction (const PlatformEventBuffer *events, void *user_data);

b32 Platform_Initialize (void);
void Platform_Shutdown (void);
b32 Platform_IsInitialized (void);
void Platform_PumpEvents (PlatformEventBuffer *event_buffer);
void Platform_SetModalTickCallback (PlatformModalTickFunction *callback, void *user_data);

#endif // PLATFORM_APP_H
