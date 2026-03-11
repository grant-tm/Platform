#include "platform_internal.h"

static b32 Platform_PathToCString (String path, c8 *buffer, usize buffer_size)
{
    ASSERT(buffer != NULL);
    ASSERT(buffer_size > 0);

    if ((path.count + 1) > buffer_size)
    {
        return false;
    }

    if (path.count > 0)
    {
        Memory_Copy(buffer, path.data, path.count);
    }

    buffer[path.count] = 0;
    return true;
}

b32 Platform_FileExists (String path)
{
    WIN32_FILE_ATTRIBUTE_DATA file_data;
    c8 path_buffer[MAX_PATH];

    if (!Platform_PathToCString(path, path_buffer, ARRAY_COUNT(path_buffer)))
    {
        return false;
    }

    if (!GetFileAttributesExA(path_buffer, GetFileExInfoStandard, &file_data))
    {
        return false;
    }

    return (file_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

b32 Platform_GetFileSize (String path, u64 *size)
{
    WIN32_FILE_ATTRIBUTE_DATA file_data;
    ULARGE_INTEGER file_size;
    c8 path_buffer[MAX_PATH];

    ASSERT(size != NULL);

    if (!Platform_PathToCString(path, path_buffer, ARRAY_COUNT(path_buffer)))
    {
        return false;
    }

    if (!GetFileAttributesExA(path_buffer, GetFileExInfoStandard, &file_data))
    {
        return false;
    }

    if ((file_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
    {
        return false;
    }

    file_size.LowPart = file_data.nFileSizeLow;
    file_size.HighPart = file_data.nFileSizeHigh;
    *size = file_size.QuadPart;
    return true;
}

PlatformFileRead Platform_ReadEntireFile (MemoryArena *arena, String path)
{
    PlatformFileRead result;
    HANDLE file_handle;
    LARGE_INTEGER file_size;
    DWORD bytes_read;
    c8 path_buffer[MAX_PATH];

    ASSERT(arena != NULL);

    result.bytes = ByteSlice_Create(NULL, 0);
    result.success = false;

    if (!Platform_PathToCString(path, path_buffer, ARRAY_COUNT(path_buffer)))
    {
        return result;
    }

    file_handle = CreateFileA(path_buffer, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file_handle == INVALID_HANDLE_VALUE)
    {
        return result;
    }

    if (!GetFileSizeEx(file_handle, &file_size))
    {
        CloseHandle(file_handle);
        return result;
    }

    if ((file_size.QuadPart < 0) || ((u64) file_size.QuadPart > 0xFFFFFFFFu))
    {
        CloseHandle(file_handle);
        return result;
    }

    result.bytes = ByteSlice_Create(MemoryArena_PushArray(arena, byte, (usize) file_size.QuadPart), (usize) file_size.QuadPart);
    if ((file_size.QuadPart > 0) && (result.bytes.data == NULL))
    {
        CloseHandle(file_handle);
        result.bytes.count = 0;
        return result;
    }

    if (!ReadFile(file_handle, result.bytes.data, (DWORD) result.bytes.count, &bytes_read, NULL))
    {
        CloseHandle(file_handle);
        result.bytes = ByteSlice_Create(NULL, 0);
        return result;
    }

    CloseHandle(file_handle);

    if (bytes_read != result.bytes.count)
    {
        result.bytes = ByteSlice_Create(NULL, 0);
        return result;
    }

    result.success = true;
    return result;
}

b32 Platform_WriteEntireFile (String path, ByteSlice bytes)
{
    HANDLE file_handle;
    DWORD bytes_written;
    c8 path_buffer[MAX_PATH];

    if (!Platform_PathToCString(path, path_buffer, ARRAY_COUNT(path_buffer)))
    {
        return false;
    }

    file_handle = CreateFileA(path_buffer, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file_handle == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    if (!WriteFile(file_handle, bytes.data, (DWORD) bytes.count, &bytes_written, NULL))
    {
        CloseHandle(file_handle);
        return false;
    }

    CloseHandle(file_handle);
    return bytes_written == bytes.count;
}
