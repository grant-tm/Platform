#ifndef PLATFORM_FILE_H
#define PLATFORM_FILE_H

#include "platform_input.h"

typedef struct PlatformFileRead
{
    ByteSlice bytes;
    b32 success;
} PlatformFileRead;

b32 Platform_FileExists (String path);
b32 Platform_GetFileSize (String path, u64 *size);
PlatformFileRead Platform_ReadEntireFile (MemoryArena *arena, String path);
b32 Platform_WriteEntireFile (String path, ByteSlice bytes);

#endif // PLATFORM_FILE_H
