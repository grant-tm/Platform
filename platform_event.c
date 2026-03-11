#include "platform_event.h"

PlatformEventBuffer PlatformEventBuffer_Create (PlatformEvent *events, usize capacity)
{
    PlatformEventBuffer result;

    if (capacity > 0)
    {
        ASSERT(events != NULL);
    }

    result.events = events;
    result.count = 0;
    result.capacity = capacity;
    return result;
}

void PlatformEventBuffer_Reset (PlatformEventBuffer *buffer)
{
    ASSERT(buffer != NULL);

    buffer->count = 0;
}
