#include "platform_internal.h"

typedef struct PlatformThreadStartData
{
    PlatformThreadProc *proc;
    void *user_data;
} PlatformThreadStartData;

typedef struct PlatformThreadNameInfo
{
    DWORD type;
    LPCSTR name;
    DWORD thread_id;
    DWORD flags;
} PlatformThreadNameInfo;

typedef char PlatformMutex_FitsSRWLOCK[
    (sizeof(SRWLOCK) <= sizeof(((PlatformMutex *) 0)->state)) ? 1 : -1];
typedef char PlatformConditionVariable_FitsWindowsType[
    (sizeof(CONDITION_VARIABLE) <= sizeof(((PlatformConditionVariable *) 0)->state)) ? 1 : -1];

static SRWLOCK *Platform_GetSRWLOCK (PlatformMutex *mutex)
{
    ASSERT(mutex != NULL);
    return (SRWLOCK *) mutex->state;
}

static CONDITION_VARIABLE *Platform_GetConditionVariable (PlatformConditionVariable *condition)
{
    ASSERT(condition != NULL);
    return (CONDITION_VARIABLE *) condition->state;
}

static DWORD WINAPI Platform_ThreadProcTrampoline (LPVOID parameter)
{
    PlatformThreadStartData *start_data;
    PlatformThreadProc *proc;
    void *user_data;
    i32 result;

    start_data = (PlatformThreadStartData *) parameter;
    ASSERT(start_data != NULL);

    proc = start_data->proc;
    user_data = start_data->user_data;
    HeapFree(GetProcessHeap(), 0, start_data);

    ASSERT(proc != NULL);
    result = proc(user_data);
    return (DWORD) result;
}

static void
Platform_SetWindowsThreadName (HANDLE handle, String name)
{
    HRESULT (WINAPI *set_thread_description) (HANDLE thread, PCWSTR name_wide);
    HMODULE kernel32_module;
    wchar_t wide_name[256];
    i32 wide_count;

    if (String_IsEmpty(name))
    {
        return;
    }

    kernel32_module = GetModuleHandleW(L"kernel32.dll");
    if (kernel32_module == NULL)
    {
        return;
    }

    set_thread_description = (HRESULT (WINAPI *) (HANDLE, PCWSTR))
        GetProcAddress(kernel32_module, "SetThreadDescription");
    if (set_thread_description == NULL)
    {
        return;
    }

    wide_count = MultiByteToWideChar(CP_UTF8, 0, name.data, (i32) name.count, wide_name, ARRAY_COUNT(wide_name) - 1);
    if (wide_count <= 0)
    {
        return;
    }

    wide_name[wide_count] = 0;
    set_thread_description(handle, wide_name);
}

PlatformThread PlatformThread_Create (PlatformThreadProc *proc, void *user_data)
{
    PlatformThread thread;
    PlatformThreadStartData *start_data;
    HANDLE handle;
    DWORD thread_id;

    ASSERT(proc != NULL);

    thread.handle = NULL;
    thread.id = PLATFORM_THREAD_ID_INVALID;

    start_data = (PlatformThreadStartData *) HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*start_data));
    if (start_data == NULL)
    {
        return thread;
    }

    start_data->proc = proc;
    start_data->user_data = user_data;

    handle = CreateThread(NULL, 0, Platform_ThreadProcTrampoline, start_data, 0, &thread_id);
    if (handle == NULL)
    {
        HeapFree(GetProcessHeap(), 0, start_data);
        return thread;
    }

    thread.handle = handle;
    thread.id = (PlatformThreadID) thread_id;
    return thread;
}

void PlatformThread_Join (PlatformThread thread, i32 *result)
{
    DWORD exit_code;
    HANDLE handle;

    ASSERT(PlatformThread_IsValid(thread));

    handle = (HANDLE) thread.handle;
    WaitForSingleObject(handle, INFINITE);
    if (result != NULL)
    {
        ASSERT(GetExitCodeThread(handle, &exit_code) != 0);
        *result = (i32) exit_code;
    }

    CloseHandle(handle);
}

void PlatformThread_Detach (PlatformThread thread)
{
    ASSERT(PlatformThread_IsValid(thread));
    CloseHandle((HANDLE) thread.handle);
}

b32 PlatformThread_IsValid (PlatformThread thread)
{
    return (thread.handle != NULL) && (thread.id != PLATFORM_THREAD_ID_INVALID);
}

PlatformThreadID PlatformThread_GetCurrentID (void)
{
    return (PlatformThreadID) GetCurrentThreadId();
}

void PlatformThread_SetName (PlatformThread thread, String name)
{
    ASSERT(PlatformThread_IsValid(thread));
    Platform_SetWindowsThreadName((HANDLE) thread.handle, name);
}

void PlatformThread_SetCurrentName (String name)
{
    Platform_SetWindowsThreadName(GetCurrentThread(), name);
}

void PlatformThread_Yield (void)
{
    SwitchToThread();
}

void PlatformMutex_Init (PlatformMutex *mutex)
{
    ASSERT(mutex != NULL);
    InitializeSRWLock(Platform_GetSRWLOCK(mutex));
}

void PlatformMutex_Destroy (PlatformMutex *mutex)
{
    ASSERT(mutex != NULL);
    Memory_Zero(mutex, sizeof(*mutex));
}

void PlatformMutex_Lock (PlatformMutex *mutex)
{
    AcquireSRWLockExclusive(Platform_GetSRWLOCK(mutex));
}

void PlatformMutex_Unlock (PlatformMutex *mutex)
{
    ReleaseSRWLockExclusive(Platform_GetSRWLOCK(mutex));
}

void PlatformConditionVariable_Init (PlatformConditionVariable *condition)
{
    ASSERT(condition != NULL);
    InitializeConditionVariable(Platform_GetConditionVariable(condition));
}

void PlatformConditionVariable_Destroy (PlatformConditionVariable *condition)
{
    ASSERT(condition != NULL);
    Memory_Zero(condition, sizeof(*condition));
}

void PlatformConditionVariable_WakeOne (PlatformConditionVariable *condition)
{
    WakeConditionVariable(Platform_GetConditionVariable(condition));
}

void PlatformConditionVariable_WakeAll (PlatformConditionVariable *condition)
{
    WakeAllConditionVariable(Platform_GetConditionVariable(condition));
}

void PlatformConditionVariable_Wait (PlatformConditionVariable *condition, PlatformMutex *mutex)
{
    ASSERT(SleepConditionVariableSRW(
        Platform_GetConditionVariable(condition),
        Platform_GetSRWLOCK(mutex),
        INFINITE,
        0) != 0);
}

PlatformSemaphore PlatformSemaphore_Create (i32 initial_count, i32 max_count)
{
    PlatformSemaphore semaphore;

    semaphore.handle = CreateSemaphoreW(NULL, initial_count, max_count, NULL);
    return semaphore;
}

void PlatformSemaphore_Destroy (PlatformSemaphore semaphore)
{
    if (PlatformSemaphore_IsValid(semaphore))
    {
        CloseHandle((HANDLE) semaphore.handle);
    }
}

b32 PlatformSemaphore_IsValid (PlatformSemaphore semaphore)
{
    return semaphore.handle != NULL;
}

void PlatformSemaphore_Signal (PlatformSemaphore semaphore, i32 count)
{
    ASSERT(PlatformSemaphore_IsValid(semaphore));
    ASSERT(ReleaseSemaphore((HANDLE) semaphore.handle, count, NULL) != 0);
}

void PlatformSemaphore_Wait (PlatformSemaphore semaphore)
{
    ASSERT(PlatformSemaphore_IsValid(semaphore));
    WaitForSingleObject((HANDLE) semaphore.handle, INFINITE);
}

b32 PlatformSemaphore_TryWait (PlatformSemaphore semaphore, Milliseconds timeout_milliseconds)
{
    DWORD timeout;
    DWORD result;

    ASSERT(PlatformSemaphore_IsValid(semaphore));
    ASSERT(timeout_milliseconds >= 0);

    timeout = (DWORD) timeout_milliseconds;
    result = WaitForSingleObject((HANDLE) semaphore.handle, timeout);
    return result == WAIT_OBJECT_0;
}

PlatformTLSKey PlatformTLSKey_Create (void)
{
    PlatformTLSKey key;

    key.slot = TlsAlloc();
    return key;
}

void PlatformTLSKey_Destroy (PlatformTLSKey key)
{
    if (PlatformTLSKey_IsValid(key))
    {
        TlsFree(key.slot);
    }
}

b32 PlatformTLSKey_IsValid (PlatformTLSKey key)
{
    return key.slot != PLATFORM_TLS_KEY_INVALID.slot;
}

void PlatformTLS_SetValue (PlatformTLSKey key, void *value)
{
    ASSERT(PlatformTLSKey_IsValid(key));
    ASSERT(TlsSetValue(key.slot, value) != 0);
}

void *PlatformTLS_GetValue (PlatformTLSKey key)
{
    ASSERT(PlatformTLSKey_IsValid(key));
    return TlsGetValue(key.slot);
}
