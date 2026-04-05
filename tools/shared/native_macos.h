/*
 * native_macos.h — Native macOS file dialogs + application menu bar
 *
 * C-callable interface. Implementation in Objective-C (.m file).
 * Returns malloc'd strings (caller frees) or NULL on cancel.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* ── File dialogs ─────────────────────────────────────────── */

/* Returns malloc'd path or NULL on cancel. Caller must free(). */
char *dialog_open_file(const char *title, const char *filter_ext);
char *dialog_save_file(const char *title, const char *default_name);
char *dialog_open_folder(const char *title);

/* ── Menu bar ─────────────────────────────────────────────── */

/* Menu action IDs — polled each frame */
typedef enum {
    MENU_NONE = 0,
    MENU_NEW,
    MENU_OPEN,
    MENU_SAVE,
    MENU_SAVE_AS,
    MENU_EXPORT,
    MENU_QUIT,
    MENU_COPY,
    MENU_PASTE,
    MENU_CLEAR,
} MenuAction;

/* Install the native menu bar. Call once at startup after SDL_Init.
   app_name is shown in the app menu and Quit item. */
void native_menu_init_ex(const char *app_name);
void native_menu_init(void);  /* defaults to "Workbench" */

/* Poll for menu action. Returns MENU_NONE if nothing selected.
   Resets after reading (only returns each action once). */
MenuAction native_menu_poll(void);

#ifdef __cplusplus
}
#endif
