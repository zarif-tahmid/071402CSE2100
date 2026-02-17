/*
 * results.c
 * Handles recording and displaying search results.
 * Owns all interaction with the results list-box and the found-path buffer.
 */

#include <windows.h>
#include <string.h>

#define PATH_CAP 32768

/* Shared state - defined in main.c, used here */
extern char g_found_path[PATH_CAP];
extern HWND g_hList;

/* -------------------------------------------------------------------------
 * Public functions
 * ---------------------------------------------------------------------- */

void result_add(const char *full_path)
{
    if (g_hList != NULL) {
        SendMessageA(g_hList, LB_ADDSTRING, 0, (LPARAM)full_path);
    }
    strncpy(g_found_path, full_path, PATH_CAP - 1);
    g_found_path[PATH_CAP - 1] = '\0';
}

void results_clear(void)
{
    SendMessageA(g_hList, LB_RESETCONTENT, 0, 0);
    g_found_path[0] = '\0';
}

void results_show_not_found(void)
{
    SendMessageA(g_hList, LB_ADDSTRING, 0, (LPARAM)"No match found.");
}
