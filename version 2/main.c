/*
 * main.c
 * Application entry point.
 * - Defines all shared global variables used across the other .c files.
 * - Initialises COM (needed for the folder-picker in gui.c).
 * - Registers the window class and creates the main window via gui.c.
 * - Runs the Win32 message loop.
 */

#include <windows.h>
#include <objbase.h>

#define PATH_CAP 32768

/* -------------------------------------------------------------------------
 * Global variable definitions
 * These are declared as "extern" inside results.c and gui.c so they can
 * share the same variables without header files.
 * ---------------------------------------------------------------------- */
char g_found_path[PATH_CAP] = { 0 };
HWND g_hList                = NULL;
HWND g_hEditRoot            = NULL;
HWND g_hEditTerm            = NULL;

/* Functions from gui.c */
extern int  gui_register_class    (HINSTANCE hInst);
extern HWND gui_create_main_window(HINSTANCE hInst, int nShow);

/* -------------------------------------------------------------------------
 * Entry point
 * ---------------------------------------------------------------------- */

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev,
                   LPSTR lpCmd, int nShow)
{
    (void)hPrev;
    (void)lpCmd;

    CoInitialize(NULL);

    if (!gui_register_class(hInst)) {
        MessageBoxA(NULL, "Failed to register window class.",
                    "Startup Error", MB_ICONERROR | MB_OK);
        CoUninitialize();
        return 1;
    }

    HWND hwnd = gui_create_main_window(hInst, nShow);
    if (hwnd == NULL) {
        MessageBoxA(NULL, "Failed to create main window.",
                    "Startup Error", MB_ICONERROR | MB_OK);
        CoUninitialize();
        return 1;
    }

    MSG msg;
    while (GetMessageA(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    CoUninitialize();
    return (int)msg.wParam;
}
