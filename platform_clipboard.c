#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "platform_clipboard.h"

static b32
PlatformClipboard_Open (void)
{
    return OpenClipboard(NULL) != 0;
}

b32
Platform_HasClipboardText (void)
{
    b32 result;

    result = false;
    if (PlatformClipboard_Open())
    {
        result = IsClipboardFormatAvailable(CF_UNICODETEXT) != 0;
        CloseClipboard();
    }

    return result;
}

String
Platform_GetClipboardText (MemoryArena *arena)
{
    String result;
    HANDLE clipboard_data;
    wchar_t *wide_text;
    i32 utf8_count;
    c8 *utf8_text;

    ASSERT(arena != NULL);

    result = String_Create(NULL, 0);
    if (!PlatformClipboard_Open())
    {
        return result;
    }

    clipboard_data = GetClipboardData(CF_UNICODETEXT);
    if (clipboard_data == NULL)
    {
        CloseClipboard();
        return result;
    }

    wide_text = (wchar_t *)GlobalLock(clipboard_data);
    if (wide_text == NULL)
    {
        CloseClipboard();
        return result;
    }

    utf8_count = WideCharToMultiByte(CP_UTF8, 0, wide_text, -1, NULL, 0, NULL, NULL);
    if (utf8_count > 0)
    {
        utf8_text = MemoryArena_PushArray(arena, c8, (usize)utf8_count);
        if (utf8_text != NULL)
        {
            WideCharToMultiByte(CP_UTF8, 0, wide_text, -1, (char *)utf8_text, utf8_count, NULL, NULL);
            result = String_Create(utf8_text, (usize)(utf8_count - 1));
        }
    }

    GlobalUnlock(clipboard_data);
    CloseClipboard();
    return result;
}

b32
Platform_SetClipboardText (String text)
{
    HGLOBAL clipboard_memory;
    wchar_t *wide_text;
    i32 wide_count;
    SIZE_T byte_count;

    wide_count = MultiByteToWideChar(CP_UTF8, 0, (char *)text.data, (i32)text.count, NULL, 0);
    if (wide_count < 0)
    {
        return false;
    }

    byte_count = (SIZE_T)(wide_count + 1) * sizeof(wchar_t);
    clipboard_memory = GlobalAlloc(GMEM_MOVEABLE, byte_count);
    if (clipboard_memory == NULL)
    {
        return false;
    }

    wide_text = (wchar_t *)GlobalLock(clipboard_memory);
    if (wide_text == NULL)
    {
        GlobalFree(clipboard_memory);
        return false;
    }

    if (wide_count > 0)
    {
        MultiByteToWideChar(CP_UTF8, 0, (char *)text.data, (i32)text.count, wide_text, wide_count);
    }

    wide_text[wide_count] = 0;
    GlobalUnlock(clipboard_memory);

    if (!PlatformClipboard_Open())
    {
        GlobalFree(clipboard_memory);
        return false;
    }

    if (EmptyClipboard() == 0)
    {
        CloseClipboard();
        GlobalFree(clipboard_memory);
        return false;
    }

    if (SetClipboardData(CF_UNICODETEXT, clipboard_memory) == NULL)
    {
        CloseClipboard();
        GlobalFree(clipboard_memory);
        return false;
    }

    CloseClipboard();
    return true;
}
