#include "platform_internal.h"

#ifndef USER_DEFAULT_SCREEN_DPI
#define USER_DEFAULT_SCREEN_DPI 96
#endif

#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02E0
#endif

typedef BOOL WINAPI SetProcessDPIAwareProc (void);
typedef HRESULT WINAPI SetProcessDpiAwarenessProc (int value);
typedef BOOL WINAPI SetProcessDpiAwarenessContextProc (HANDLE value);
typedef UINT WINAPI GetDpiForWindowProc (HWND hwnd);
typedef HRESULT WINAPI GetDpiForMonitorProc (HMONITOR monitor, int dpi_type, UINT *dpi_x, UINT *dpi_y);

static HMODULE platform_user32_module = NULL;
static HMODULE platform_shcore_module = NULL;
static SetProcessDPIAwareProc *platform_set_process_dpi_aware = NULL;
static SetProcessDpiAwarenessProc *platform_set_process_dpi_awareness = NULL;
static SetProcessDpiAwarenessContextProc *platform_set_process_dpi_awareness_context = NULL;
static GetDpiForWindowProc *platform_get_dpi_for_window = NULL;
static GetDpiForMonitorProc *platform_get_dpi_for_monitor = NULL;

static void
PlatformDPI_LoadFunctions (void)
{
    if (platform_user32_module == NULL)
    {
        platform_user32_module = LoadLibraryA("user32.dll");
        if (platform_user32_module != NULL)
        {
            platform_set_process_dpi_aware = (SetProcessDPIAwareProc *) GetProcAddress(platform_user32_module, "SetProcessDPIAware");
            platform_set_process_dpi_awareness_context = (SetProcessDpiAwarenessContextProc *) GetProcAddress(platform_user32_module, "SetProcessDpiAwarenessContext");
            platform_get_dpi_for_window = (GetDpiForWindowProc *) GetProcAddress(platform_user32_module, "GetDpiForWindow");
        }
    }

    if (platform_shcore_module == NULL)
    {
        platform_shcore_module = LoadLibraryA("shcore.dll");
        if (platform_shcore_module != NULL)
        {
            platform_set_process_dpi_awareness = (SetProcessDpiAwarenessProc *) GetProcAddress(platform_shcore_module, "SetProcessDpiAwareness");
            platform_get_dpi_for_monitor = (GetDpiForMonitorProc *) GetProcAddress(platform_shcore_module, "GetDpiForMonitor");
        }
    }
}

void
PlatformDPI_Initialize (void)
{
    PlatformDPI_LoadFunctions();

    if (platform_set_process_dpi_awareness_context != NULL)
    {
        if (platform_set_process_dpi_awareness_context((HANDLE) -4))
        {
            return;
        }

        platform_set_process_dpi_awareness_context((HANDLE) -3);
        return;
    }

    if (platform_set_process_dpi_awareness != NULL)
    {
        if (SUCCEEDED(platform_set_process_dpi_awareness(2)))
        {
            return;
        }

        platform_set_process_dpi_awareness(1);
        return;
    }

    if (platform_set_process_dpi_aware != NULL)
    {
        platform_set_process_dpi_aware();
    }
}

static PlatformDPI
PlatformDPI_Create (u32 x, u32 y)
{
    PlatformDPI result;

    result.x = x;
    result.y = y;
    return result;
}

PlatformDPI
Platform_GetDisplayDPI (PlatformDisplay display)
{
    HMONITOR monitor;
    UINT dpi_x;
    UINT dpi_y;

    PlatformDPI_LoadFunctions();

    monitor = (HMONITOR) (uintptr_t) display;
    if (monitor == NULL)
    {
        return PlatformDPI_Create(USER_DEFAULT_SCREEN_DPI, USER_DEFAULT_SCREEN_DPI);
    }

    if (platform_get_dpi_for_monitor != NULL)
    {
        dpi_x = USER_DEFAULT_SCREEN_DPI;
        dpi_y = USER_DEFAULT_SCREEN_DPI;
        if (SUCCEEDED(platform_get_dpi_for_monitor(monitor, 0, &dpi_x, &dpi_y)))
        {
            return PlatformDPI_Create(dpi_x, dpi_y);
        }
    }

    return PlatformDPI_Create(USER_DEFAULT_SCREEN_DPI, USER_DEFAULT_SCREEN_DPI);
}

PlatformDPI
Platform_GetWindowDPI (PlatformWindow window)
{
    PlatformWindowState *window_state;
    UINT dpi;

    PlatformDPI_LoadFunctions();

    window_state = Platform_GetWindowState(window);
    ASSERT(window_state != NULL);

    if (platform_get_dpi_for_window != NULL)
    {
        dpi = platform_get_dpi_for_window(window_state->hwnd);
        if (dpi > 0)
        {
            return PlatformDPI_Create(dpi, dpi);
        }
    }

    return Platform_GetDisplayDPI(Platform_GetDisplayForWindow(window));
}

f32
Platform_GetDisplayScale (PlatformDisplay display)
{
    PlatformDPI dpi;

    dpi = Platform_GetDisplayDPI(display);
    return (f32) dpi.x / (f32) USER_DEFAULT_SCREEN_DPI;
}

f32
Platform_GetWindowScale (PlatformWindow window)
{
    PlatformDPI dpi;

    dpi = Platform_GetWindowDPI(window);
    return (f32) dpi.x / (f32) USER_DEFAULT_SCREEN_DPI;
}
