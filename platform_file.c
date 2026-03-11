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

static String Platform_CopyCStringToArena (MemoryArena *arena, const c8 *source, usize count)
{
    c8 *buffer;

    ASSERT(arena != NULL);

    buffer = MemoryArena_PushArray(arena, c8, count);
    if ((count > 0) && (buffer == NULL))
    {
        return String_Create(NULL, 0);
    }

    if (count > 0)
    {
        Memory_Copy(buffer, source, count);
    }

    return String_Create(buffer, count);
}

String Platform_GetWorkingDirectory (MemoryArena *arena)
{
    DWORD path_length;
    c8 path_buffer[MAX_PATH];

    ASSERT(arena != NULL);

    path_length = GetCurrentDirectoryA(ARRAY_COUNT(path_buffer), path_buffer);
    if ((path_length == 0) || (path_length >= ARRAY_COUNT(path_buffer)))
    {
        return String_Create(NULL, 0);
    }

    return Platform_CopyCStringToArena(arena, path_buffer, (usize) path_length);
}

String Platform_GetExecutablePath (MemoryArena *arena)
{
    DWORD path_length;
    c8 path_buffer[MAX_PATH];

    ASSERT(arena != NULL);

    path_length = GetModuleFileNameA(NULL, path_buffer, ARRAY_COUNT(path_buffer));
    if ((path_length == 0) || (path_length >= ARRAY_COUNT(path_buffer)))
    {
        return String_Create(NULL, 0);
    }

    return Platform_CopyCStringToArena(arena, path_buffer, (usize) path_length);
}

String Platform_GetExecutableDirectory (MemoryArena *arena)
{
    String executable_path;
    usize index;

    ASSERT(arena != NULL);

    executable_path = Platform_GetExecutablePath(arena);
    if (String_IsEmpty(executable_path))
    {
        return executable_path;
    }

    for (index = executable_path.count; index > 0; index -= 1)
    {
        c8 character;

        character = executable_path.data[index - 1];
        if ((character == '\\') || (character == '/'))
        {
            return String_Prefix(executable_path, index - 1);
        }
    }

    return String_Create(NULL, 0);
}

String Platform_GetTempDirectory (MemoryArena *arena)
{
    DWORD path_length;
    c8 path_buffer[MAX_PATH];

    ASSERT(arena != NULL);

    path_length = GetTempPathA(ARRAY_COUNT(path_buffer), path_buffer);
    if ((path_length == 0) || (path_length >= ARRAY_COUNT(path_buffer)))
    {
        return String_Create(NULL, 0);
    }

    if ((path_length > 0) && ((path_buffer[path_length - 1] == '\\') || (path_buffer[path_length - 1] == '/')))
    {
        path_length -= 1;
    }

    return Platform_CopyCStringToArena(arena, path_buffer, (usize) path_length);
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
