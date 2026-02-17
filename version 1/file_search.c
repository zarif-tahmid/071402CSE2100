#include <windows.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <shlobj.h>
#include <objbase.h>

#define PATH_CAP 32768
#define ROOT_INPUT_CAP 4096
#define TERM_INPUT_CAP 512

static char g_found_path[PATH_CAP];


static HWND g_hList = NULL;
static HWND g_hEditRoot = NULL;
static HWND g_hEditTerm = NULL;
static HWND g_hResult = NULL;

static int equals_case_insensitive(const char *a, const char *b) {
	if (a == NULL || b == NULL) {
		return 0;
	}
	while (*a && *b && (tolower((unsigned char)*a) == tolower((unsigned char)*b))) {
		++a;
		++b;
	}
	return (*a == '\0' && *b == '\0');
}

static int starts_with_case_insensitive(const char *text, const char *prefix) {
    if (prefix == NULL || *prefix == '\0') {
        return 1;
    }
    if (text == NULL) {
        return 0;
    }
    while (*prefix && *text) {
        unsigned char a = (unsigned char)*text;
        unsigned char b = (unsigned char)*prefix;
        if (tolower(a) != tolower(b)) {
            return 0;
        }
        ++text;
        ++prefix;
    }
    return (*prefix == '\0');
}

// (Content scanning removed for simplicity and speed)

static void print_full_path(const char *path) {
	// Append to GUI list if present
	if (g_hList != NULL) {
		SendMessageA(g_hList, LB_ADDSTRING, 0, (LPARAM)path);
	}
	strncpy(g_found_path, path, sizeof(g_found_path) - 1);
	g_found_path[sizeof(g_found_path) - 1] = '\0';
}

static void join_path(char *out, size_t out_cap, const char *dir, const char *name) {
	if (!dir || !*dir) {
		strncpy(out, name, out_cap - 1);
		out[out_cap - 1] = '\0';
		return;
	}
	size_t len = strlen(dir);
	if (len > 0 && (dir[len - 1] == '\\' || dir[len - 1] == '/')) {
		snprintf(out, out_cap, "%s%s", dir, name);
	} else {
		snprintf(out, out_cap, "%s\\%s", dir, name);
	}
	out[out_cap - 1] = '\0';
}


static void search_directory(const char *root_dir, const char *term, int stop_after_first, int *found);

static void search_directory_with_depth(const char *root_dir, const char *term, int stop_after_first, int *found, int max_depth) {
	WIN32_FIND_DATAA fd;
	char pattern[PATH_CAP];
	size_t cap = sizeof(pattern) / sizeof(pattern[0]);
	size_t root_len = strlen(root_dir);
	if (root_len > 0 && (root_dir[root_len - 1] == '\\' || root_dir[root_len - 1] == '/')) {
		snprintf(pattern, cap, "%s*", root_dir);
	} else {
		snprintf(pattern, cap, "%s\\*", root_dir);
	}
	pattern[cap - 1] = '\0';

	HANDLE h = FindFirstFileA(pattern, &fd);
	if (h == INVALID_HANDLE_VALUE) {
		return;
	}
	do {
		if (stop_after_first && *found) {
			break;
		}
		const char *name = fd.cFileName;
		if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
			continue;
		}
		if ((fd.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) != 0 ||
			(fd.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM) != 0) {
			continue; // skip hidden/system
		}
		char full[PATH_CAP];
		join_path(full, sizeof(full) / sizeof(full[0]), root_dir, name);

		if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
			if (name[0] == '.') {
				continue; 
			}
			if (max_depth != 0) {
				search_directory_with_depth(full, term, stop_after_first, found, max_depth > 0 ? max_depth - 1 : max_depth);
				if (stop_after_first && *found) {
					break;
				}
			}
		} else {
			if (starts_with_case_insensitive(name, term)) {
				print_full_path(full);
				*found = 1;
				if (stop_after_first) {
					break;
				}
			}
		}
	} while (FindNextFileA(h, &fd));
	FindClose(h);
}

static void search_directory(const char *root_dir, const char *term, int stop_after_first, int *found) {
	search_directory_with_depth(root_dir, term, stop_after_first, found, -1); // -1 means unlimited depth
}

static void trim_newline(char *s) {
	if (!s) return;
	size_t n = strlen(s);
	while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r')) {
		s[n - 1] = '\0';
		--n;
	}
}

static int is_exit_command(const char *s) {
    if (!s) return 0;
    const char *t = "exit";
    for (size_t i = 0; ; ++i) {
        char a = s[i];
        char b = t[i];
        if (a >= 'A' && a <= 'Z') {
            a = (char)(a + 32);
        }
        if (a != b) {
            return 0;
        }
        if (a == '\0') {
            return 1;
        }
    }
}


// GUI helpers
static int pick_folder(HWND owner, char *out_path, size_t cap) {
    BROWSEINFOA bi; memset(&bi, 0, sizeof(bi));
    bi.hwndOwner = owner;
    bi.lpszTitle = "Select root folder";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE | BIF_USENEWUI;
    LPITEMIDLIST pidl = SHBrowseForFolderA(&bi);
    if (!pidl) return 0;
    char path[PATH_CAP];
    BOOL ok = SHGetPathFromIDListA(pidl, path);
    CoTaskMemFree(pidl);
    if (!ok) return 0;
    strncpy(out_path, path, cap - 1);
    out_path[cap - 1] = '\0';
    return 1;
}

#define ID_EDIT_ROOT 2001
#define ID_BTN_BROWSE 2002
#define ID_EDIT_TERM 2003
#define ID_BTN_SEARCH 2004
#define ID_LIST_RESULTS 2005

static void on_browse(HWND hwnd) {
    char buf[ROOT_INPUT_CAP];
    if (pick_folder(hwnd, buf, sizeof(buf))) {
        SetWindowTextA(g_hEditRoot, buf);
    }
}

static void on_search(HWND hwnd) {
    char root[ROOT_INPUT_CAP];
    char term[TERM_INPUT_CAP];
    GetWindowTextA(g_hEditRoot, root, (int)sizeof(root));
    GetWindowTextA(g_hEditTerm, term, (int)sizeof(term));
    if (root[0] == '\0') {
        MessageBoxA(hwnd, "Root folder cannot be empty.", "Error", MB_ICONERROR);
        return;
    }
    DWORD attrs = GetFileAttributesA(root);
    if (attrs == INVALID_FILE_ATTRIBUTES || (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0) {
        MessageBoxA(hwnd, "Root folder not found or not a directory.", "Error", MB_ICONERROR);
        return;
    }
    if (term[0] == '\0') {
        MessageBoxA(hwnd, "File name cannot be empty.", "Error", MB_ICONERROR);
        return;
    }
    SendMessageA(g_hList, LB_RESETCONTENT, 0, 0);
    g_found_path[0] = '\0';
    int found = 0;
    // If Shift key is held, do shallow search (depth 0 = only root directory)
    SHORT shift = GetKeyState(VK_SHIFT);
    if ((shift & 0x8000) != 0) {
        search_directory_with_depth(root, term, 0 /* all */, &found, 0);
    } else {
        search_directory(root, term, 0 /* all */, &found);
    }
    if (!found) {
        SendMessageA(g_hList, LB_ADDSTRING, 0, (LPARAM)"No match found.");
    }
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        CreateWindowExA(0, "STATIC", "Root:", WS_CHILD | WS_VISIBLE, 10, 12, 40, 20, hwnd, NULL, NULL, NULL);
        g_hEditRoot = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 55, 10, 360, 22, hwnd, (HMENU)ID_EDIT_ROOT, NULL, NULL);
        CreateWindowExA(0, "BUTTON", "Browse...", WS_CHILD | WS_VISIBLE, 420, 10, 80, 22, hwnd, (HMENU)ID_BTN_BROWSE, NULL, NULL);
        CreateWindowExA(0, "STATIC", "Filename:", WS_CHILD | WS_VISIBLE, 10, 45, 60, 20, hwnd, NULL, NULL, NULL);
        g_hEditTerm = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 75, 43, 325, 22, hwnd, (HMENU)ID_EDIT_TERM, NULL, NULL);
        CreateWindowExA(0, "BUTTON", "Search", WS_CHILD | WS_VISIBLE, 405, 43, 95, 22, hwnd, (HMENU)ID_BTN_SEARCH, NULL, NULL);
        g_hList = CreateWindowExA(WS_EX_CLIENTEDGE, "LISTBOX", "", WS_CHILD | WS_VISIBLE | LBS_NOTIFY | WS_VSCROLL, 10, 80, 495, 220, hwnd, (HMENU)ID_LIST_RESULTS, NULL, NULL);
        SendMessage(g_hEditRoot, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessage(g_hEditTerm, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessage(g_hList, WM_SETFONT, (WPARAM)hFont, TRUE);
        break;
    }
    case WM_COMMAND: {
        switch (LOWORD(wParam)) {
        case ID_BTN_BROWSE:
            on_browse(hwnd);
            break;
        case ID_BTN_SEARCH:
            on_search(hwnd);
            break;
        }
        break;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmd, int nShow) {
    (void)hPrev; (void)lpCmd;
    CoInitialize(NULL);
    const char *cls = "FileSearchWindow";
    WNDCLASSA wc; memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = cls;
    RegisterClassA(&wc);
    HWND hwnd = CreateWindowExA(0, cls, "File Search", WS_OVERLAPPED | WS_SYSMENU | WS_CAPTION | WS_MINIMIZEBOX,
                                CW_USEDEFAULT, CW_USEDEFAULT, 530, 360, NULL, NULL, hInst, NULL);
    ShowWindow(hwnd, nShow);
    UpdateWindow(hwnd);
    MSG msg;
    while (GetMessageA(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    CoUninitialize();
    return (int)msg.wParam;
}