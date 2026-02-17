/*
 * gui.c
 * All GUI logic: child-control creation, button event handlers,
 * window class registration, main-window creation, and WndProc.
 */

#include <windows.h>
#include <shlobj.h>
#include <objbase.h>
#include <string.h>

/* Buffer sizes */
#define PATH_CAP       32768
#define ROOT_INPUT_CAP  4096
#define TERM_INPUT_CAP   512

/* Control IDs */
#define ID_EDIT_ROOT     2001
#define ID_BTN_BROWSE    2002
#define ID_EDIT_TERM     2003
#define ID_BTN_SEARCH    2004
#define ID_LIST_RESULTS  2005

/* Window class and title */
#define WINDOW_CLASS_NAME  "FileSearchWindow"
#define WINDOW_TITLE       "File Search"

/* Shared state - defined in main.c */
extern HWND g_hList;
extern HWND g_hEditRoot;
extern HWND g_hEditTerm;

/* Functions from search.c */
extern void search_directory_all    (const char *root_dir, const char *term, int *found);
extern void search_directory_shallow(const char *root_dir, const char *term, int *found);

/* Functions from results.c */
extern void results_clear          (void);
extern void results_show_not_found (void);

/* -------------------------------------------------------------------------
 * Internal input validation helpers (static)
 * ---------------------------------------------------------------------- */

static int read_edit_text(HWND hEdit, char *buf, int buf_cap)
{
    if (buf == NULL || buf_cap <= 0) {
        return 0;
    }
    GetWindowTextA(hEdit, buf, buf_cap);
    return 1;
}

static int validate_root_folder(HWND hwnd, const char *root_path)
{
    if (root_path == NULL || root_path[0] == '\0') {
        MessageBoxA(hwnd, "Root folder cannot be empty.",
                    "Input Error", MB_ICONERROR | MB_OK);
        return 0;
    }
    DWORD attrs = GetFileAttributesA(root_path);
    if (attrs == INVALID_FILE_ATTRIBUTES ||
        (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0) {
        MessageBoxA(hwnd, "Root folder not found or is not a directory.",
                    "Input Error", MB_ICONERROR | MB_OK);
        return 0;
    }
    return 1;
}

static int validate_search_term(HWND hwnd, const char *term)
{
    if (term == NULL || term[0] == '\0') {
        MessageBoxA(hwnd, "File name cannot be empty.",
                    "Input Error", MB_ICONERROR | MB_OK);
        return 0;
    }
    return 1;
}

/* -------------------------------------------------------------------------
 * Child-control creation (static)
 * ---------------------------------------------------------------------- */

static void create_child_controls(HWND hwnd)
{
    HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

    /* Row 1 - Root folder */
    CreateWindowExA(0, "STATIC", "Root:",
                    WS_CHILD | WS_VISIBLE,
                    10, 12, 40, 20, hwnd, NULL, NULL, NULL);

    g_hEditRoot = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
                    WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                    55, 10, 360, 22,
                    hwnd, (HMENU)ID_EDIT_ROOT, NULL, NULL);

    CreateWindowExA(0, "BUTTON", "Browse...",
                    WS_CHILD | WS_VISIBLE,
                    420, 10, 80, 22,
                    hwnd, (HMENU)ID_BTN_BROWSE, NULL, NULL);

    /* Row 2 - Search term */
    CreateWindowExA(0, "STATIC", "Filename:",
                    WS_CHILD | WS_VISIBLE,
                    10, 45, 60, 20, hwnd, NULL, NULL, NULL);

    g_hEditTerm = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
                    WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                    75, 43, 325, 22,
                    hwnd, (HMENU)ID_EDIT_TERM, NULL, NULL);

    CreateWindowExA(0, "BUTTON", "Search",
                    WS_CHILD | WS_VISIBLE,
                    405, 43, 95, 22,
                    hwnd, (HMENU)ID_BTN_SEARCH, NULL, NULL);

    /* Row 3 - Results list */
    g_hList = CreateWindowExA(WS_EX_CLIENTEDGE, "LISTBOX", "",
                    WS_CHILD | WS_VISIBLE | LBS_NOTIFY | WS_VSCROLL,
                    10, 80, 495, 220,
                    hwnd, (HMENU)ID_LIST_RESULTS, NULL, NULL);

    /* Apply consistent font */
    SendMessageA(g_hEditRoot, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessageA(g_hEditTerm, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessageA(g_hList,     WM_SETFONT, (WPARAM)hFont, TRUE);
}

/* -------------------------------------------------------------------------
 * Button event handlers (static)
 * ---------------------------------------------------------------------- */

static void handle_browse(HWND hwnd)
{
    BROWSEINFOA bi;
    memset(&bi, 0, sizeof(bi));
    bi.hwndOwner = hwnd;
    bi.lpszTitle = "Select root folder";
    bi.ulFlags   = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE | BIF_USENEWUI;

    LPITEMIDLIST pidl = SHBrowseForFolderA(&bi);
    if (pidl == NULL) {
        return; /* user cancelled */
    }

    char chosen_path[ROOT_INPUT_CAP];
    if (SHGetPathFromIDListA(pidl, chosen_path)) {
        SetWindowTextA(g_hEditRoot, chosen_path);
    }
    CoTaskMemFree(pidl);
}

static void handle_search(HWND hwnd)
{
    char root[ROOT_INPUT_CAP];
    char term[TERM_INPUT_CAP];

    read_edit_text(g_hEditRoot, root, (int)sizeof(root));
    read_edit_text(g_hEditTerm, term, (int)sizeof(term));

    if (!validate_root_folder(hwnd, root)) {
        return;
    }
    if (!validate_search_term(hwnd, term)) {
        return;
    }

    results_clear();

    int found = 0;
    SHORT shift_state = GetKeyState(VK_SHIFT);
    if ((shift_state & 0x8000) != 0) {
        search_directory_shallow(root, term, &found);
    } else {
        search_directory_all(root, term, &found);
    }

    if (!found) {
        results_show_not_found();
    }
}

/* -------------------------------------------------------------------------
 * Window procedure
 * ---------------------------------------------------------------------- */

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg,
                                 WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_CREATE:
        create_child_controls(hwnd);
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case ID_BTN_BROWSE:
            handle_browse(hwnd);
            break;
        case ID_BTN_SEARCH:
            handle_search(hwnd);
            break;
        default:
            break;
        }
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    default:
        break;
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

/* -------------------------------------------------------------------------
 * Public functions - called from main.c
 * ---------------------------------------------------------------------- */

int gui_register_class(HINSTANCE hInst)
{
    WNDCLASSA wc;
    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursorA(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = WINDOW_CLASS_NAME;
    return (RegisterClassA(&wc) != 0);
}

HWND gui_create_main_window(HINSTANCE hInst, int nShow)
{
    HWND hwnd = CreateWindowExA(
        0,
        WINDOW_CLASS_NAME,
        WINDOW_TITLE,
        WS_OVERLAPPED | WS_SYSMENU | WS_CAPTION | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT,
        530, 360,
        NULL, NULL, hInst, NULL);

    if (hwnd != NULL) {
        ShowWindow(hwnd, nShow);
        UpdateWindow(hwnd);
    }
    return hwnd;
}
