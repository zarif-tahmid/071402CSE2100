/*
 * search.c
 * Recursive directory traversal and filename-prefix matching.
 * Calls result_add() from results.c to record each match.
 */

#include <windows.h>
#include <string.h>

#define PATH_CAP 32768

/* Functions from utils.c */
extern int  str_starts_with_icase(const char *text, const char *prefix);
extern void path_join(char *out, size_t out_cap, const char *dir, const char *name);
extern void path_make_pattern(char *out, size_t out_cap, const char *dir);
extern int  is_dot_entry(const char *name);
extern int  is_skippable_attr(DWORD attrs);

/* Function from results.c */
extern void result_add(const char *full_path);

/* -------------------------------------------------------------------------
 * Internal helpers (static - not visible outside this file)
 * ---------------------------------------------------------------------- */

/* Forward declaration so process_entry can recurse */
static void search_dir_depth(const char *root_dir, const char *term,
                              int stop_after_first, int *found, int max_depth);

static void process_entry(const WIN32_FIND_DATAA *fd, const char *parent_dir,
                           const char *term, int stop_after_first,
                           int *found, int max_depth)
{
    const char *name = fd->cFileName;

    char full_path[PATH_CAP];
    path_join(full_path, sizeof(full_path), parent_dir, name);

    if ((fd->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
        /* Skip hidden dot-directories like ".git" */
        if (name[0] == '.') {
            return;
        }
        if (max_depth != 0) {
            int next_depth = (max_depth > 0) ? max_depth - 1 : max_depth;
            search_dir_depth(full_path, term, stop_after_first, found, next_depth);
        }
    } else {
        if (str_starts_with_icase(name, term)) {
            result_add(full_path);
            *found = 1;
        }
    }
}

static void search_dir_depth(const char *root_dir, const char *term,
                              int stop_after_first, int *found, int max_depth)
{
    char pattern[PATH_CAP];
    path_make_pattern(pattern, sizeof(pattern), root_dir);

    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) {
        return;
    }

    do {
        if (stop_after_first && *found) {
            break;
        }
        if (is_dot_entry(fd.cFileName) || is_skippable_attr(fd.dwFileAttributes)) {
            continue;
        }
        process_entry(&fd, root_dir, term, stop_after_first, found, max_depth);
    } while (FindNextFileA(h, &fd));

    FindClose(h);
}

/* -------------------------------------------------------------------------
 * Public functions
 * ---------------------------------------------------------------------- */

void search_directory_all(const char *root_dir, const char *term, int *found)
{
    search_dir_depth(root_dir, term, 0, found, -1); /* -1 = unlimited depth */
}

void search_directory_shallow(const char *root_dir, const char *term, int *found)
{
    search_dir_depth(root_dir, term, 0, found, 0);  /*  0 = root only      */
}
