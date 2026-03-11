#ifndef PLATFORM_FILE_H
#define PLATFORM_FILE_H

#include "platform_input.h"

typedef struct PlatformFileRead
{
    ByteSlice bytes;
    b32 success;
} PlatformFileRead;

typedef struct PlatformDirectory
{
    c8 path[260];
    usize path_count;
} PlatformDirectory;

typedef enum PlatformDirectoryEntryType
{
    PLATFORM_DIRECTORY_ENTRY_TYPE_NONE = 0,
    PLATFORM_DIRECTORY_ENTRY_TYPE_FILE,
    PLATFORM_DIRECTORY_ENTRY_TYPE_DIRECTORY,
} PlatformDirectoryEntryType;

typedef struct PlatformPathInfo
{
    PlatformDirectoryEntryType type;
    u64 size;
    Nanoseconds creation_time;
    Nanoseconds last_write_time;
} PlatformPathInfo;

typedef struct PlatformDirectoryEntry
{
    String name;
    String path;
    PlatformDirectoryEntryType type;
} PlatformDirectoryEntry;

typedef struct PlatformDirectoryIterator
{
    byte state[1024];
} PlatformDirectoryIterator;

String Platform_JoinPath (MemoryArena *arena, String left, String right);
String Platform_GetParentPath (String path);
String Platform_GetFileName (String path);
String Platform_GetExtension (String path);
String Platform_GetStem (String path);
String Platform_GetWorkingDirectory (MemoryArena *arena);
String Platform_GetExecutablePath (MemoryArena *arena);
String Platform_GetExecutableDirectory (MemoryArena *arena);
String Platform_GetTempDirectory (MemoryArena *arena);

b32 Platform_PathExists (String path);
b32 Platform_GetPathInfo (String path, PlatformPathInfo *info);
b32 Platform_DirectoryExists (String path);
b32 Platform_CreateDirectory (String path);
b32 Platform_CreateDirectoryRecursive (String path);
b32 PlatformDirectory_Open (PlatformDirectory *directory, String path);
String PlatformDirectory_GetPath (const PlatformDirectory *directory);
b32 PlatformDirectory_Enter (PlatformDirectory *directory, String child_name);
b32 PlatformDirectory_Up (PlatformDirectory *directory);
b32 PlatformDirectory_BeginIteration (const PlatformDirectory *directory, PlatformDirectoryIterator *iterator, MemoryArena *arena, PlatformDirectoryEntry *entry);
b32 PlatformDirectory_Next (PlatformDirectoryIterator *iterator, MemoryArena *arena, PlatformDirectoryEntry *entry);
void PlatformDirectory_EndIteration (PlatformDirectoryIterator *iterator);

b32 Platform_FileExists (String path);
b32 Platform_GetFileSize (String path, u64 *size);
b32 Platform_CopyFile (String source_path, String destination_path, b32 overwrite);
b32 Platform_MovePath (String source_path, String destination_path);
b32 Platform_DeleteFile (String path);
b32 Platform_DeleteDirectory (String path);
PlatformFileRead Platform_ReadEntireFile (MemoryArena *arena, String path);
b32 Platform_WriteEntireFile (String path, ByteSlice bytes);

#endif // PLATFORM_FILE_H
