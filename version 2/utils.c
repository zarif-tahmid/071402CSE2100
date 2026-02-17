/*
 * utils.c
 * String comparison and path construction helpers.
 * No dependency on global application state.
 */

#include <windows.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

/* -------------------------------------------------------------------------
 * String utilities
 * ---------------------------------------------------------------------- */

int str_equals_icase(const char *a, const char *b)
{
    if (a == NULL || b == NULL) {
        return 0;
    }
    while (*a && *b &&
           (tolower((unsigned char)*a) == tolower((unsigned char)*b))) {
        ++a;
        ++b;
    }
    return (*a == '\0' && *b == '\0');
}

int str_starts_with_icase(const char *text, const char *prefix)
{
    if (prefix == NULL || *prefix == '\0') {
        return 1;
    }
    if (text == NULL) {
        return 0;
    }
    while (*prefix && *text) {
        if (tolower((unsigned char)*text) != tolower((unsigned char)*prefix)) {
            return 0;
        }
        ++text;
        ++prefix;
    }
    return (*prefix == '\0');
}

/* -------------------------------------------------------------------------
 * Path utilities
 * ---------------------------------------------------------------------- */

void path_join(char *out, size_t out_cap, const char *dir, const char *name)
{
    if (dir == NULL || *dir == '\0') {
        strncpy(out, name, out_cap - 1);
        out[out_cap - 1] = '\0';
        return;
    }
    size_t dir_len = strlen(dir);
    int has_sep = (dir[dir_len - 1] == '\\' || dir[dir_len - 1] == '/');
    if (has_sep) {
        snprintf(out, out_cap, "%s%s", dir, name);
    } else {
        snprintf(out, out_cap, "%s\\%s", dir, name);
    }
    out[out_cap - 1] = '\0';
}

void path_make_pattern(char *out, size_t out_cap, const char *dir)
{
    size_t dir_len = strlen(dir);
    int has_sep = (dir_len > 0 &&
                   (dir[dir_len - 1] == '\\' || dir[dir_len - 1] == '/'));
    if (has_sep) {
        snprintf(out, out_cap, "%s*", dir);
    } else {
        snprintf(out, out_cap, "%s\\*", dir);
    }
    out[out_cap - 1] = '\0';
}

/* -------------------------------------------------------------------------
 * Attribute helpers
 * ---------------------------------------------------------------------- */

int is_dot_entry(const char *name)
{
    return (strcmp(name, ".") == 0 || strcmp(name, "..") == 0);
}

int is_skippable_attr(DWORD attrs)
{
    return ((attrs & FILE_ATTRIBUTE_HIDDEN) != 0 ||
            (attrs & FILE_ATTRIBUTE_SYSTEM) != 0);
}
