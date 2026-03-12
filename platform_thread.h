#ifndef PLATFORM_THREAD_H
#define PLATFORM_THREAD_H

#include "core.h"

typedef i32 PlatformThreadID;

typedef i32 PlatformThreadProc (void *user_data);

typedef struct PlatformThread
{
    void *handle;
    PlatformThreadID id;
} PlatformThread;

typedef struct PlatformMutex
{
    byte state[8];
} PlatformMutex;

typedef struct PlatformConditionVariable
{
    byte state[8];
} PlatformConditionVariable;

typedef struct PlatformSemaphore
{
    void *handle;
} PlatformSemaphore;

typedef struct PlatformTLSKey
{
    u32 slot;
} PlatformTLSKey;

static const PlatformThreadID PLATFORM_THREAD_ID_INVALID = 0;
static const PlatformTLSKey PLATFORM_TLS_KEY_INVALID = {0xFFFFFFFFu};

PlatformThread PlatformThread_Create (PlatformThreadProc *proc, void *user_data);
void PlatformThread_Join (PlatformThread thread, i32 *result);
void PlatformThread_Detach (PlatformThread thread);
b32 PlatformThread_IsValid (PlatformThread thread);
PlatformThreadID PlatformThread_GetCurrentID (void);
void PlatformThread_SetName (PlatformThread thread, String name);
void PlatformThread_SetCurrentName (String name);
void PlatformThread_Yield (void);

void PlatformMutex_Init (PlatformMutex *mutex);
void PlatformMutex_Destroy (PlatformMutex *mutex);
void PlatformMutex_Lock (PlatformMutex *mutex);
void PlatformMutex_Unlock (PlatformMutex *mutex);

void PlatformConditionVariable_Init (PlatformConditionVariable *condition);
void PlatformConditionVariable_Destroy (PlatformConditionVariable *condition);
void PlatformConditionVariable_WakeOne (PlatformConditionVariable *condition);
void PlatformConditionVariable_WakeAll (PlatformConditionVariable *condition);
void PlatformConditionVariable_Wait (PlatformConditionVariable *condition, PlatformMutex *mutex);

PlatformSemaphore PlatformSemaphore_Create (i32 initial_count, i32 max_count);
void PlatformSemaphore_Destroy (PlatformSemaphore semaphore);
b32 PlatformSemaphore_IsValid (PlatformSemaphore semaphore);
void PlatformSemaphore_Signal (PlatformSemaphore semaphore, i32 count);
void PlatformSemaphore_Wait (PlatformSemaphore semaphore);
b32 PlatformSemaphore_TryWait (PlatformSemaphore semaphore, Milliseconds timeout_milliseconds);

PlatformTLSKey PlatformTLSKey_Create (void);
void PlatformTLSKey_Destroy (PlatformTLSKey key);
b32 PlatformTLSKey_IsValid (PlatformTLSKey key);
void PlatformTLS_SetValue (PlatformTLSKey key, void *value);
void *PlatformTLS_GetValue (PlatformTLSKey key);

#endif // PLATFORM_THREAD_H
