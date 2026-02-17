FILE SEARCH UTILITY - IMPLEMENTATION DOCUMENTATION
====================================================


OVERVIEW
--------
This program is a Windows desktop application written in C using the Win32 API.
It lets the user pick a root folder, type a filename prefix, and search for all
files whose names start with that prefix. If the user holds Shift while clicking
Search, the search stays shallow (root folder only). Otherwise it recurses into
all subdirectories.

The code is split into 5 source files. Each file has one job. They share data
through extern variables and extern function declarations instead of header files.


FILES AT A GLANCE
-----------------
main.c      - starts the program, defines global variables, runs the message loop
utils.c     - string helpers and path helpers, no dependency on anything else
search.c    - walks directories and finds matching files
results.c   - puts matched paths into the list box on screen
gui.c       - creates the window and controls, handles button clicks


HOW THEY CONNECT
----------------
main.c calls two functions from gui.c to set up the window, then runs the loop.

gui.c calls search.c when the user clicks Search.
gui.c calls results.c to clear the list before each search.

search.c calls utils.c for string and path work.
search.c calls results.c to record each file it finds.

results.c reads the global variables g_hList and g_found_path defined in main.c.

utils.c does not call anything else. It is self-contained.


GLOBAL VARIABLES (defined in main.c)
-------------------------------------
These four variables are defined once in main.c. Any file that needs them
declares them with the extern keyword at the top of that file.

    char g_found_path[32768]
        Stores the most recent matched file path. Gets overwritten every time
        a new match is found.

    HWND g_hList
        The handle to the list box control on the window. results.c uses this
        to add matched paths to the visible list.

    HWND g_hEditRoot
        The handle to the text box where the user types or browses for a root
        folder. gui.c reads this when the Search button is clicked.

    HWND g_hEditTerm
        The handle to the text box where the user types the filename prefix.
        gui.c reads this when the Search button is clicked.


====================================================
FILE: utils.c
====================================================

This file contains helper functions that are used by search.c. None of these
functions touch global state. They take inputs and return outputs, nothing more.


FUNCTION: str_equals_icase
--------------------------
Checks if two strings are exactly equal, ignoring upper and lower case.
Returns 1 if they match, 0 if they do not, and 0 if either pointer is NULL.
It walks both strings one character at a time, comparing the lowercase version
of each character. If both strings reach the end at the same time, they match.


FUNCTION: str_starts_with_icase
--------------------------------
Checks if a filename begins with a given prefix, ignoring case.
Returns 1 if it does, 0 if it does not.
An empty prefix always returns 1 because every string starts with nothing.
A NULL text pointer always returns 0.
It only walks as far as the prefix goes, so it does not need to read the
whole filename.
This is the function that decides whether a file is a match during search.


FUNCTION: path_join
--------------------
Combines a directory path and a filename into one full path string.
Writes the result into an output buffer.
If the directory already ends with a backslash, it just appends the filename.
If not, it inserts a backslash between them first.
If the directory is empty or NULL, it just copies the filename directly.
Always puts a null terminator at the end so the string is safe to use.
Example: "C:\Users\Alice" + "notes.txt" becomes "C:\Users\Alice\notes.txt"


FUNCTION: path_make_pattern
-----------------------------
Takes a directory path and produces a wildcard pattern for FindFirstFileA.
Appends \* to the path (or just * if the path already ends with a slash).
Example: "C:\Users\Alice" becomes "C:\Users\Alice\*"
This pattern tells Windows to list all files and folders in that directory.


FUNCTION: is_dot_entry
-----------------------
Returns 1 if the name is "." or "..", otherwise returns 0.
These two names appear in every directory listing on Windows.
"." means the current directory and ".." means the parent directory.
If the search loop tried to recurse into them it would loop forever,
so this function is used to skip them.


FUNCTION: is_skippable_attr
-----------------------------
Takes the file attribute flags from a WIN32_FIND_DATAA struct.
Returns 1 if the FILE_ATTRIBUTE_HIDDEN or FILE_ATTRIBUTE_SYSTEM flag is set.
Returns 0 otherwise.
Hidden and system files are skipped during search to avoid noise and to avoid
permission errors on files the user would not normally access.


====================================================
FILE: search.c
====================================================

This file does the actual work of walking through directories and finding files
that match the search term. It has two public functions that gui.c calls, and
two private (static) functions that do the real work internally.


FUNCTION: search_directory_all  (public)
-----------------------------------------
Called by gui.c when the user clicks Search without holding Shift.
Searches the root folder and every subfolder at unlimited depth.
Calls the internal search_dir_depth function with a depth value of -1,
which means no depth limit.


FUNCTION: search_directory_shallow  (public)
---------------------------------------------
Called by gui.c when the user holds Shift and clicks Search.
Searches only the files directly inside the root folder.
Does not go into any subfolders.
Calls the internal search_dir_depth function with a depth value of 0,
which means stop after the first level.


FUNCTION: search_dir_depth  (static, internal only)
-----------------------------------------------------
This is the core function that does the actual directory traversal.
It is marked static so nothing outside search.c can call it directly.

It takes a directory path, the search term, the found flag pointer, and a
max_depth number that controls how deep to go.

    max_depth = -1 means go as deep as possible, no limit.
    max_depth = 0 means do not go into any subdirectories at all.
    max_depth > 0 means go that many more levels deep, then stop.

Steps it takes:
1. Builds a wildcard pattern from the directory path using path_make_pattern.
2. Calls FindFirstFileA to get the first entry in the directory.
3. If FindFirstFileA fails (directory is empty or inaccessible), returns immediately.
4. Loops using FindNextFileA until there are no more entries.
5. For each entry, skips it if is_dot_entry or is_skippable_attr returns 1.
6. Otherwise passes the entry to process_entry.
7. Closes the find handle with FindClose when done.


FUNCTION: process_entry  (static, internal only)
--------------------------------------------------
Called by search_dir_depth for each directory entry that was not skipped.
It is also marked static so it cannot be accessed from outside search.c.

It first builds the full path of the entry by calling path_join.

Then it checks what kind of entry it is:

If the entry is a directory:
    It skips it if the name starts with a dot (like .git or .svn).
    Otherwise it calls search_dir_depth recursively on that subdirectory,
    passing max_depth minus 1 so the depth decreases with each level.
    If max_depth is -1 (unlimited), it passes -1 again unchanged.

If the entry is a file:
    It calls str_starts_with_icase to check if the filename starts with
    the search term the user typed.
    If it matches, it calls result_add from results.c with the full path,
    and sets the found flag to 1.


====================================================
FILE: results.c
====================================================

This file owns everything related to showing results to the user.
It is the only file that writes to g_hList or g_found_path.
By keeping all of that in one place, search.c does not need to know anything
about how the GUI displays results.


FUNCTION: result_add
---------------------
Called by search.c every time a matching file is found.
Adds the full file path to the list box by sending an LB_ADDSTRING message
to g_hList. Checks that g_hList is not NULL before doing so.
Also copies the path into g_found_path using strncpy with a size limit,
and manually sets the last byte to null to guarantee the string is terminated.


FUNCTION: results_clear
------------------------
Called by gui.c at the start of every new search before any results come in.
Sends an LB_RESETCONTENT message to g_hList to empty the list box completely.
Sets g_found_path[0] to null to clear the stored last path.


FUNCTION: results_show_not_found
----------------------------------
Called by gui.c after a search completes if the found flag is still 0.
Sends an LB_ADDSTRING message to add the text "No match found." to the list box
so the user knows the search ran but found nothing.


====================================================
FILE: gui.c
====================================================

This file handles everything the user sees and interacts with.
It creates the window and all controls, responds to button clicks,
validates input before starting a search, and contains the window procedure.
All helper functions in this file are static, meaning they cannot be called
from outside this file.


FUNCTION: read_edit_text  (static)
------------------------------------
A small wrapper around GetWindowTextA.
Reads the current text from an edit control into a buffer.
Returns 0 early if the buffer pointer is NULL or the capacity is zero or less.
This avoids passing bad arguments to GetWindowTextA.


FUNCTION: validate_root_folder  (static)
-----------------------------------------
Checks that the root folder the user entered is valid before starting a search.
First checks that the string is not empty or NULL.
Then calls GetFileAttributesA on the path.
If that fails or the path is not a directory, shows a MessageBox with an error
and returns 0.
Returns 1 if everything looks good.


FUNCTION: validate_search_term  (static)
-----------------------------------------
Checks that the user actually typed something in the filename box.
If the string is empty or NULL, shows a MessageBox with an error and returns 0.
Returns 1 if the term is not empty.


FUNCTION: create_child_controls  (static)
------------------------------------------
Called once from WM_CREATE when the main window is first created.
Creates all the controls on the window using CreateWindowExA.

Controls it creates:
    A static label saying "Root:" next to the root folder text box.
    An edit control (g_hEditRoot) where the user types or browses a folder path.
    A button labelled "Browse..." that opens the folder picker.
    A static label saying "Filename:" next to the search term text box.
    An edit control (g_hEditTerm) where the user types the filename prefix.
    A button labelled "Search" that starts the search.
    A list box (g_hList) that shows all matched file paths.

After creating the controls, it gets the default GUI font using GetStockObject
and applies it to the edit controls and list box with WM_SETFONT so the text
does not look like the old Windows 3 system font.


FUNCTION: handle_browse  (static)
-----------------------------------
Called when the user clicks the Browse button.
Opens the Windows shell folder picker dialog using SHBrowseForFolderA.
The BIF_USENEWUI and BIF_NEWDIALOGSTYLE flags give it the modern Vista-style
look with a resizable window and address bar.
If the user picks a folder, it converts the result to a plain path string
using SHGetPathFromIDListA and writes it into the root folder text box.
Then it frees the memory returned by the dialog using CoTaskMemFree.
If the user cancels the dialog, pidl comes back as NULL and the function
returns immediately without changing anything.


FUNCTION: handle_search  (static)
-----------------------------------
Called when the user clicks the Search button.
Reads the text from both edit controls into local buffers.
Runs validate_root_folder and validate_search_term on those buffers.
If either validation fails, the function returns without searching.
If both pass, it calls results_clear to empty the previous results.
Then it checks whether the Shift key is currently held down by calling
GetKeyState(VK_SHIFT) and checking the high-order bit of the result.
If Shift is held, it calls search_directory_shallow.
If not, it calls search_directory_all.
After the search returns, if found is still 0 it calls results_show_not_found.


FUNCTION: WndProc  (static)
-----------------------------
The main window message handler. It is static because it is passed directly
into the WNDCLASSA struct inside gui_register_class, so no other file needs
to call it by name.

Messages it handles:

    WM_CREATE
        Calls create_child_controls to build the UI when the window first appears.
        Returns 0 to signal success.

    WM_COMMAND
        Fired when the user clicks a button or interacts with a control.
        Checks the low word of wParam to identify which control sent it.
        ID_BTN_BROWSE calls handle_browse.
        ID_BTN_SEARCH calls handle_search.
        All other command IDs are ignored.
        Returns 0.

    WM_DESTROY
        Called when the user closes the window.
        Calls PostQuitMessage(0) to tell the message loop in main.c to exit.
        Returns 0.

    All other messages are passed to DefWindowProcA for default handling.


FUNCTION: gui_register_class  (public)
----------------------------------------
Called by main.c before creating the window.
Fills out a WNDCLASSA struct with the window procedure, instance handle,
cursor, background brush, and class name, then calls RegisterClassA.
Returns 1 if registration succeeded, 0 if it failed.


FUNCTION: gui_create_main_window  (public)
-------------------------------------------
Called by main.c after the class is registered.
Creates the main window using CreateWindowExA with a fixed size of 530 by 360.
Calls ShowWindow and UpdateWindow to make it appear on screen.
Returns the window handle on success or NULL on failure.


====================================================
FILE: main.c
====================================================

This is the entry point of the program. It keeps things simple by delegating
all real work to the other files.


GLOBAL VARIABLE DEFINITIONS
-----------------------------
main.c is where the four shared global variables are actually defined and
given memory. The other files (results.c and gui.c) declare them with extern
to get access to the same variables without redefining them.


FUNCTION: WinMain
------------------
This is the entry point Windows calls when the program starts.
The hPrev and lpCmd parameters are not used and are cast to void to silence
compiler warnings about unused parameters.

Steps:
1. Calls CoInitialize(NULL) to initialise COM. This is required because the
   folder picker dialog in gui.c uses COM internally.

2. Calls gui_register_class to register the window class. If this fails, shows
   an error message box and exits with return code 1.

3. Calls gui_create_main_window to create and show the main window. If this
   fails, shows an error message box and exits with return code 1.

4. Enters the message loop. The loop calls GetMessageA to wait for the next
   Windows message. When a message arrives, TranslateMessage converts keyboard
   input into character messages, and DispatchMessageA sends the message to
   WndProc in gui.c.

5. The loop exits when PostQuitMessage is called (from WM_DESTROY in gui.c),
   which causes GetMessageA to return 0 or less.

6. Calls CoUninitialize to clean up COM.

7. Returns the wParam of the quit message as the exit code.


====================================================
HOW A SEARCH WORKS END TO END
====================================================

1. User types a folder path into the root box (or uses Browse to pick one).
2. User types a filename prefix into the filename box.
3. User clicks Search (optionally holding Shift for shallow mode).
4. handle_search in gui.c reads both text boxes.
5. validate_root_folder checks the folder exists and is actually a directory.
6. validate_search_term checks the prefix is not empty.
7. results_clear empties the list box and resets g_found_path.
8. Depending on Shift key state, either search_directory_all or
   search_directory_shallow is called in search.c.
9. search_dir_depth builds a wildcard pattern and opens a find handle.
10. For every entry in the directory, process_entry is called.
11. If the entry is a subdirectory (and depth allows), search_dir_depth
    recurses into it.
12. If the entry is a file and its name starts with the prefix,
    result_add is called in results.c.
13. result_add appends the full path to the list box and saves it in g_found_path.
14. After the search finishes, if nothing was found, results_show_not_found
    puts "No match found." in the list box.
15. The user sees the results in the list box.


====================================================
BUILD INSTRUCTIONS
====================================================

To compile all five files together with MinGW on Windows:

    gcc main.c utils.c search.c results.c gui.c -o file_search.exe -lole32 -lshell32 -mwindows

Flags explained:
    -lole32      links the COM library needed for CoInitialize and CoTaskMemFree
    -lshell32    links the shell library needed for SHBrowseForFolderA and SHGetPathFromIDListA
    -mwindows    tells the linker to produce a GUI application with WinMain instead of a console


====================================================
END OF DOCUMENTATION
====================================================
