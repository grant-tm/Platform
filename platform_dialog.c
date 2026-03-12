#include "platform_internal.h"

#include <commdlg.h>
#include <shlobj.h>

typedef struct PlatformFolderDialogInit
{
    const wchar_t *initial_path;
} PlatformFolderDialogInit;

static wchar_t *
PlatformDialog_StringToWide (MemoryArena *arena, String string)
{
    wchar_t *result;
    i32 wide_count;

    ASSERT(arena != NULL);

    wide_count = MultiByteToWideChar(CP_UTF8, 0, (char *) string.data, (i32) string.count, NULL, 0);
    if (wide_count < 0)
    {
        return NULL;
    }

    result = MemoryArena_PushArray(arena, wchar_t, (usize) wide_count + 1);
    if (result == NULL)
    {
        return NULL;
    }

    if (wide_count > 0)
    {
        MultiByteToWideChar(CP_UTF8, 0, (char *) string.data, (i32) string.count, result, wide_count);
    }

    result[wide_count] = 0;
    return result;
}

static String
PlatformDialog_WideToString (MemoryArena *arena, const wchar_t *wide_string)
{
    String result;
    i32 utf8_count;
    c8 *buffer;

    ASSERT(arena != NULL);
    ASSERT(wide_string != NULL);

    result = String_Create(NULL, 0);
    utf8_count = WideCharToMultiByte(CP_UTF8, 0, wide_string, -1, NULL, 0, NULL, NULL);
    if (utf8_count <= 0)
    {
        return result;
    }

    buffer = MemoryArena_PushArray(arena, c8, (usize) utf8_count);
    if (buffer == NULL)
    {
        return result;
    }

    WideCharToMultiByte(CP_UTF8, 0, wide_string, -1, (char *) buffer, utf8_count, NULL, NULL);
    result = String_Create(buffer, (usize) (utf8_count - 1));
    return result;
}

static HWND
PlatformDialog_GetOwnerWindow (PlatformWindow owner)
{
    PlatformWindowState *window_state;

    if (!Handle64_IsValid(owner))
    {
        return NULL;
    }

    window_state = Platform_GetWindowState(owner);
    if (window_state == NULL)
    {
        return NULL;
    }

    return window_state->hwnd;
}

static wchar_t *
PlatformDialog_BuildFilterString (MemoryArena *arena, const PlatformDialogFilter *filters, usize filter_count)
{
    usize index;
    usize total_count;
    wchar_t *result;
    usize write_index;

    ASSERT(arena != NULL);

    if ((filters == NULL) || (filter_count == 0))
    {
        result = MemoryArena_PushArray(arena, wchar_t, 3);
        if (result == NULL)
        {
            return NULL;
        }

        result[0] = L'*';
        result[1] = 0;
        result[2] = 0;
        return result;
    }

    total_count = 1;
    for (index = 0; index < filter_count; index += 1)
    {
        i32 name_count;
        i32 pattern_count;

        name_count = MultiByteToWideChar(CP_UTF8, 0, (char *) filters[index].name.data, (i32) filters[index].name.count, NULL, 0);
        pattern_count = MultiByteToWideChar(CP_UTF8, 0, (char *) filters[index].pattern.data, (i32) filters[index].pattern.count, NULL, 0);
        if ((name_count < 0) || (pattern_count < 0))
        {
            return NULL;
        }

        total_count += (usize) name_count + 1;
        total_count += (usize) pattern_count + 1;
    }

    result = MemoryArena_PushArray(arena, wchar_t, total_count);
    if (result == NULL)
    {
        return NULL;
    }

    write_index = 0;
    for (index = 0; index < filter_count; index += 1)
    {
        i32 name_count;
        i32 pattern_count;

        name_count = MultiByteToWideChar(CP_UTF8, 0, (char *) filters[index].name.data, (i32) filters[index].name.count, result + write_index, (i32) (total_count - write_index));
        ASSERT(name_count >= 0);
        write_index += (usize) name_count;
        result[write_index] = 0;
        write_index += 1;

        pattern_count = MultiByteToWideChar(CP_UTF8, 0, (char *) filters[index].pattern.data, (i32) filters[index].pattern.count, result + write_index, (i32) (total_count - write_index));
        ASSERT(pattern_count >= 0);
        write_index += (usize) pattern_count;
        result[write_index] = 0;
        write_index += 1;
    }

    result[write_index] = 0;
    return result;
}

static String
PlatformDialog_RunFileDialog (MemoryArena *arena, const PlatformFileDialogDesc *desc, b32 is_save_dialog)
{
    MemoryArena scratch;
    byte scratch_memory[4096];
    OPENFILENAMEW ofn;
    wchar_t file_buffer[1024];
    wchar_t *title_wide;
    wchar_t *initial_path_wide;
    wchar_t *filter_wide;

    ASSERT(arena != NULL);
    ASSERT(desc != NULL);

    MemoryArena_Init(&scratch, scratch_memory, sizeof(scratch_memory));
    title_wide = String_IsEmpty(desc->title) ? NULL : PlatformDialog_StringToWide(&scratch, desc->title);
    initial_path_wide = String_IsEmpty(desc->initial_path) ? NULL : PlatformDialog_StringToWide(&scratch, desc->initial_path);
    filter_wide = PlatformDialog_BuildFilterString(&scratch, desc->filters, desc->filter_count);

    Memory_Zero(&ofn, sizeof(ofn));
    Memory_Zero(file_buffer, sizeof(file_buffer));
    if (initial_path_wide != NULL)
    {
        wcsncpy_s(file_buffer, ARRAY_COUNT(file_buffer), initial_path_wide, _TRUNCATE);
    }

    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = PlatformDialog_GetOwnerWindow(desc->owner);
    ofn.lpstrFile = file_buffer;
    ofn.nMaxFile = ARRAY_COUNT(file_buffer);
    ofn.lpstrTitle = title_wide;
    ofn.lpstrFilter = filter_wide;
    ofn.Flags = OFN_EXPLORER | OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST;
    if (is_save_dialog)
    {
        ofn.Flags |= OFN_OVERWRITEPROMPT;
    }
    else
    {
        ofn.Flags |= OFN_FILEMUSTEXIST;
    }

    if (is_save_dialog)
    {
        if (!GetSaveFileNameW(&ofn))
        {
            return String_Create(NULL, 0);
        }
    }
    else
    {
        if (!GetOpenFileNameW(&ofn))
        {
            return String_Create(NULL, 0);
        }
    }

    return PlatformDialog_WideToString(arena, file_buffer);
}

static int CALLBACK
PlatformDialog_FolderBrowseCallback (HWND hwnd, UINT message, LPARAM l_param, LPARAM data)
{
    PlatformFolderDialogInit *init;

    (void) l_param;

    if (message == BFFM_INITIALIZED)
    {
        init = (PlatformFolderDialogInit *) data;
        if ((init != NULL) && (init->initial_path != NULL))
        {
            SendMessageW(hwnd, BFFM_SETSELECTIONW, TRUE, (LPARAM) init->initial_path);
        }
    }

    return 0;
}

String
Platform_OpenFileDialog (MemoryArena *arena, const PlatformFileDialogDesc *desc)
{
    return PlatformDialog_RunFileDialog(arena, desc, false);
}

String
Platform_SaveFileDialog (MemoryArena *arena, const PlatformFileDialogDesc *desc)
{
    return PlatformDialog_RunFileDialog(arena, desc, true);
}

String
Platform_SelectFolderDialog (MemoryArena *arena, PlatformWindow owner, String title, String initial_path)
{
    MemoryArena scratch;
    byte scratch_memory[2048];
    BROWSEINFOW browse_info;
    PlatformFolderDialogInit init;
    PIDLIST_ABSOLUTE selection;
    wchar_t *title_wide;
    wchar_t *initial_path_wide;
    wchar_t path_buffer[MAX_PATH];

    ASSERT(arena != NULL);

    MemoryArena_Init(&scratch, scratch_memory, sizeof(scratch_memory));
    title_wide = String_IsEmpty(title) ? NULL : PlatformDialog_StringToWide(&scratch, title);
    initial_path_wide = String_IsEmpty(initial_path) ? NULL : PlatformDialog_StringToWide(&scratch, initial_path);

    Memory_Zero(&browse_info, sizeof(browse_info));
    init.initial_path = initial_path_wide;
    browse_info.hwndOwner = PlatformDialog_GetOwnerWindow(owner);
    browse_info.lpszTitle = title_wide;
    browse_info.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE | BIF_USENEWUI;
    browse_info.lpfn = PlatformDialog_FolderBrowseCallback;
    browse_info.lParam = (LPARAM) &init;

    selection = SHBrowseForFolderW(&browse_info);
    if (selection == NULL)
    {
        return String_Create(NULL, 0);
    }

    if (!SHGetPathFromIDListW(selection, path_buffer))
    {
        CoTaskMemFree(selection);
        return String_Create(NULL, 0);
    }

    CoTaskMemFree(selection);
    return PlatformDialog_WideToString(arena, path_buffer);
}
