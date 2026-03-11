#ifndef PLATFORM_TIME_H
#define PLATFORM_TIME_H

#include "platform_window.h"

Nanoseconds Platform_GetMonotonicTime (void);
void Platform_Sleep (Milliseconds duration);

#endif // PLATFORM_TIME_H
