#ifndef PLATFORM_DIALOG_H
#define PLATFORM_DIALOG_H

#include "platform_window.h"

typedef struct PlatformDialogFilter
{
    String name;
    String pattern;
} PlatformDialogFilter;

typedef struct PlatformFileDialogDesc
{
    PlatformWindow owner;
    String title;
    String initial_path;
    const PlatformDialogFilter *filters;
    usize filter_count;
} PlatformFileDialogDesc;

String Platform_OpenFileDialog (MemoryArena *arena, const PlatformFileDialogDesc *desc);
String Platform_SaveFileDialog (MemoryArena *arena, const PlatformFileDialogDesc *desc);
String Platform_SelectFolderDialog (MemoryArena *arena, PlatformWindow owner, String title, String initial_path);

#endif // PLATFORM_DIALOG_H
