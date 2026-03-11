#include "platform_internal.h"

Nanoseconds Platform_QueryTimestamp (void)
{
    LARGE_INTEGER counter;
    f64 seconds;

    ASSERT(platform_state.is_initialized);
    ASSERT(platform_state.performance_frequency.QuadPart > 0);

    QueryPerformanceCounter(&counter);

    seconds = (f64) counter.QuadPart / (f64) platform_state.performance_frequency.QuadPart;
    return Nanoseconds_FromSecondsF64(seconds);
}

Nanoseconds Platform_GetMonotonicTime (void)
{
    return Platform_QueryTimestamp();
}

void Platform_Sleep (Milliseconds duration)
{
    ASSERT(duration >= 0);

    Sleep((DWORD) duration);
}
