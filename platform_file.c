#include "platform_internal.h"

typedef struct PlatformDirectoryIteratorState
{
    HANDLE find_handle;
    WIN32_FIND_DATAA find_data;
    c8 directory_path[260];
    usize directory_path_count;
    b32 is_active;
} PlatformDirectoryIteratorState;

typedef char PlatformDirectoryIteratorState_FitsInPublicState[
    (sizeof(PlatformDirectoryIteratorState) <= sizeof(((PlatformDirectoryIterator *) 0)->state)) ? 1 : -1];

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

static b32 Platform_PathExists (String path)
{
    WIN32_FILE_ATTRIBUTE_DATA file_data;
    c8 path_buffer[MAX_PATH];

    if (!Platform_PathToCString(path, path_buffer, ARRAY_COUNT(path_buffer)))
    {
        return false;
    }

    return GetFileAttributesExA(path_buffer, GetFileExInfoStandard, &file_data) != 0;
}

b32 Platform_DirectoryExists (String path)
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

    return (file_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
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

static String Platform_CopyStringToArena (MemoryArena *arena, String source)
{
    return Platform_CopyCStringToArena(arena, source.data, source.count);
}

static b32 Platform_IsPathSeparator (c8 character)
{
    return (character == '\\') || (character == '/');
}

static PlatformDirectoryIteratorState *Platform_GetDirectoryIteratorState (PlatformDirectoryIterator *iterator)
{
    ASSERT(iterator != NULL);
    return (PlatformDirectoryIteratorState *) iterator->state;
}

static b32 Platform_CopyPathToFixedBuffer (c8 *destination, usize destination_capacity, usize *destination_count, String source)
{
    usize count;

    ASSERT(destination != NULL);
    ASSERT(destination_count != NULL);

    if (String_IsEmpty(source))
    {
        return false;
    }

    count = source.count;
    if ((count >= 2) && Platform_IsPathSeparator(source.data[count - 1]))
    {
        if (!((count == 3) && (source.data[1] == ':')))
        {
            count -= 1;
        }
    }

    if ((count + 1) > destination_capacity)
    {
        return false;
    }

    Memory_Copy(destination, source.data, count);
    destination[count] = 0;
    *destination_count = count;
    return true;
}

static b32 Platform_DirectoryIteratorOutputEntry (const PlatformDirectoryIteratorState *state, MemoryArena *arena, PlatformDirectoryEntry *entry)
{
    String name;
    String directory_path;
    String entry_path;

    ASSERT(state != NULL);
    ASSERT(arena != NULL);
    ASSERT(entry != NULL);

    name = String_FromCString((c8 *) state->find_data.cFileName);
    directory_path = String_Create((c8 *) state->directory_path, state->directory_path_count);
    entry_path = Platform_JoinPath(arena, directory_path, name);
    if (String_IsEmpty(entry_path))
    {
        return false;
    }

    entry->name = Platform_CopyStringToArena(arena, name);
    entry->path = entry_path;
    entry->type = ((state->find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
        ? PLATFORM_DIRECTORY_ENTRY_TYPE_DIRECTORY
        : PLATFORM_DIRECTORY_ENTRY_TYPE_FILE;

    return !String_IsEmpty(entry->name);
}

static b32 Platform_DirectoryIteratorAdvance (PlatformDirectoryIteratorState *state)
{
    ASSERT(state != NULL);

    if (!state->is_active)
    {
        return false;
    }

    for (;;)
    {
        String name;

        name = String_FromCString(state->find_data.cFileName);
        if (!String_EqualsCString(name, ".") && !String_EqualsCString(name, ".."))
        {
            return true;
        }

        if (!FindNextFileA(state->find_handle, &state->find_data))
        {
            return false;
        }
    }
}

static String Platform_GetParentDirectoryView (String path)
{
    usize index;

    if (String_IsEmpty(path))
    {
        return String_Create(NULL, 0);
    }

    for (index = path.count; index > 0; index -= 1)
    {
        if (Platform_IsPathSeparator(path.data[index - 1]))
        {
            if ((index == 3) && (path.count >= 3) && (path.data[1] == ':'))
            {
                return String_Prefix(path, 3);
            }

            return String_Prefix(path, index - 1);
        }
    }

    return String_Create(NULL, 0);
}

String Platform_JoinPath (MemoryArena *arena, String left, String right)
{
    c8 *buffer;
    usize separator_count;
    usize total_count;

    ASSERT(arena != NULL);

    separator_count = 0;
    if (!String_IsEmpty(left) && !String_IsEmpty(right) && !Platform_IsPathSeparator(left.data[left.count - 1]))
    {
        separator_count = 1;
    }

    total_count = left.count + separator_count + right.count;
    buffer = MemoryArena_PushArray(arena, c8, total_count);
    if ((total_count > 0) && (buffer == NULL))
    {
        return String_Create(NULL, 0);
    }

    if (left.count > 0)
    {
        Memory_Copy(buffer, left.data, left.count);
    }

    if (separator_count > 0)
    {
        buffer[left.count] = '\\';
    }

    if (right.count > 0)
    {
        Memory_Copy(buffer + left.count + separator_count, right.data, right.count);
    }

    return String_Create(buffer, total_count);
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

b32 PlatformDirectory_Open (PlatformDirectory *directory, String path)
{
    ASSERT(directory != NULL);
    
    if (!Platform_DirectoryExists(path))
    {
        return false;
    }

    Memory_Zero(directory, sizeof(*directory));
    return Platform_CopyPathToFixedBuffer(directory->path, ARRAY_COUNT(directory->path), &directory->path_count, path);
}

String PlatformDirectory_GetPath (const PlatformDirectory *directory)
{
    ASSERT(directory != NULL);
    return String_Create((c8 *) directory->path, directory->path_count);
}

b32 PlatformDirectory_Enter (PlatformDirectory *directory, String child_name)
{
    usize original_count;
    usize separator_count;
    usize total_count;
    String child_path;

    ASSERT(directory != NULL);
    ASSERT(!String_IsEmpty(child_name));

    original_count = directory->path_count;
    separator_count = 0;
    if ((original_count > 0) && !Platform_IsPathSeparator(directory->path[original_count - 1]))
    {
        separator_count = 1;
    }

    total_count = original_count + separator_count + child_name.count;
    if ((total_count + 1) > ARRAY_COUNT(directory->path))
    {
        return false;
    }

    if (separator_count > 0)
    {
        directory->path[original_count] = '\\';
    }

    Memory_Copy(directory->path + original_count + separator_count, child_name.data, child_name.count);
    directory->path[total_count] = 0;
    directory->path_count = total_count;

    child_path = PlatformDirectory_GetPath(directory);
    if (!Platform_DirectoryExists(child_path))
    {
        directory->path_count = original_count;
        directory->path[original_count] = 0;
        return false;
    }

    return true;
}

b32 PlatformDirectory_Up (PlatformDirectory *directory)
{
    String path;
    String parent;

    ASSERT(directory != NULL);

    path = PlatformDirectory_GetPath(directory);
    parent = Platform_GetParentDirectoryView(path);
    if (String_IsEmpty(parent) || String_Equals(parent, path))
    {
        return false;
    }

    directory->path_count = parent.count;
    directory->path[parent.count] = 0;
    return true;
}

b32 PlatformDirectory_BeginIteration (const PlatformDirectory *directory, PlatformDirectoryIterator *iterator, MemoryArena *arena, PlatformDirectoryEntry *entry)
{
    PlatformDirectoryIteratorState *state;
    String search_pattern;
    c8 search_pattern_buffer[MAX_PATH];

    ASSERT(directory != NULL);
    ASSERT(arena != NULL);
    ASSERT(iterator != NULL);
    ASSERT(entry != NULL);

    state = Platform_GetDirectoryIteratorState(iterator);
    Memory_Zero(state, sizeof(*state));

    if (directory->path_count == 0)
    {
        return false;
    }

    Memory_Copy(state->directory_path, directory->path, directory->path_count);
    state->directory_path[directory->path_count] = 0;
    state->directory_path_count = directory->path_count;

    search_pattern = Platform_JoinPath(arena, PlatformDirectory_GetPath(directory), String_FromLiteral(StringLiteral_Create("*")));
    if (!Platform_PathToCString(search_pattern, search_pattern_buffer, ARRAY_COUNT(search_pattern_buffer)))
    {
        return false;
    }

    state->find_handle = FindFirstFileA(search_pattern_buffer, &state->find_data);
    if (state->find_handle == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    state->is_active = true;
    if (!Platform_DirectoryIteratorAdvance(state))
    {
        PlatformDirectory_EndIteration(iterator);
        return false;
    }

    return Platform_DirectoryIteratorOutputEntry(state, arena, entry);
}

b32 PlatformDirectory_Next (PlatformDirectoryIterator *iterator, MemoryArena *arena, PlatformDirectoryEntry *entry)
{
    PlatformDirectoryIteratorState *state;

    ASSERT(iterator != NULL);
    ASSERT(arena != NULL);
    ASSERT(entry != NULL);

    state = Platform_GetDirectoryIteratorState(iterator);
    if (!state->is_active)
    {
        return false;
    }

    if (!FindNextFileA(state->find_handle, &state->find_data))
    {
        PlatformDirectory_EndIteration(iterator);
        return false;
    }

    if (!Platform_DirectoryIteratorAdvance(state))
    {
        PlatformDirectory_EndIteration(iterator);
        return false;
    }

    return Platform_DirectoryIteratorOutputEntry(state, arena, entry);
}

void PlatformDirectory_EndIteration (PlatformDirectoryIterator *iterator)
{
    PlatformDirectoryIteratorState *state;

    ASSERT(iterator != NULL);

    state = Platform_GetDirectoryIteratorState(iterator);
    if (state->is_active && (state->find_handle != INVALID_HANDLE_VALUE))
    {
        FindClose(state->find_handle);
    }

    Memory_Zero(state, sizeof(*state));
}

b32 Platform_FileExists (String path)
{
    return Platform_PathExists(path) && !Platform_DirectoryExists(path);
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
