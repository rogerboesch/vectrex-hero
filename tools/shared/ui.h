/*
 * ui.h — Minimal SDL2 UI library for QL Workbench
 *
 * Provides: Button, Text, Panel, ColorSwatch, InputText
 * All rendering via SDL_Renderer. Text via SDL_ttf.
 * Fixed-layout panels (no docking/dragging).
 */
#pragma once

#include <SDL.h>
#include <SDL_ttf.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Theme colors ─────────────────────────────────────────── */

typedef struct {
    /* Base UI */
    SDL_Color bg;             /* window background */
    SDL_Color panel_bg;       /* panel background */
    SDL_Color panel_title;    /* panel title bar */
    SDL_Color toolbar_bg;     /* toolbar / section header background */
    SDL_Color border;         /* general borders */
    SDL_Color text;           /* normal text */
    SDL_Color text_dim;       /* dimmed/label text */
    SDL_Color text_dark;      /* very dim text (size info etc.) */
    /* Buttons */
    SDL_Color btn_bg;         /* button background */
    SDL_Color btn_hover;      /* button hover */
    SDL_Color btn_active;     /* button pressed / active tab */
    SDL_Color btn_text;       /* button text */
    /* Input */
    SDL_Color input_bg;       /* input field background */
    SDL_Color input_border;   /* input field border */
    SDL_Color select_bg;      /* selected item */
    SDL_Color swatch_sel;     /* color swatch selection border */
    /* Canvas */
    SDL_Color canvas_grid;    /* pixel grid lines */
    SDL_Color canvas_byte;    /* byte boundary lines */
    SDL_Color preview_bg;     /* preview background */
    /* Console */
    SDL_Color console_bg;     /* console content background */
    SDL_Color console_err;    /* error text */
    SDL_Color console_warn;   /* warning text */
    /* Emulator */
    SDL_Color emu_placeholder;/* emulator placeholder background */
    SDL_Color emu_text;       /* emulator placeholder text */
    /* Misc */
    SDL_Color tab_active;     /* active tab indicator */
    SDL_Color tooltip_bg;     /* tooltip background */
    /* Activity bar */
    SDL_Color activity_bg;    /* activity bar background */
    SDL_Color activity_active;/* active icon indicator */
    /* Status bar */
    SDL_Color accent;         /* accent color (status bar, special buttons) */
    SDL_Color status_text;    /* status bar text */
    /* Scrollbar */
    SDL_Color scrollbar_bg;   /* scrollbar track */
    SDL_Color scrollbar_thumb;/* scrollbar thumb */
    SDL_Color scrollbar_hover;/* scrollbar thumb hover */
} UITheme;

extern UITheme ui_theme;

/* ── Init / shutdown ──────────────────────────────────────── */

/* Initialize the UI system. Call after SDL_Init and creating renderer. */
bool ui_init(SDL_Renderer *renderer, const char *font_path, int font_size,
             const char *mono_font_path, int mono_font_size);

/* Shutdown and free resources. */
void ui_shutdown(void);

/* ── Frame cycle ──────────────────────────────────────────── */

/* Call at start of each frame to reset input state. */
void ui_begin_frame(void);

/* Process an SDL event. Call for every event in the poll loop. */
void ui_process_event(const SDL_Event *event);

/* ── Layout helpers ───────────────────────────────────────── */

/* A panel is a titled, bordered rectangle. Not moveable. */
void ui_panel_begin(const char *title, int x, int y, int w, int h);
void ui_panel_end(void);

/* Get the content area inside the current panel. */
SDL_Rect ui_panel_content(void);

/* Panel with toolbar instead of title. Returns the toolbar rect for placing widgets. */
SDL_Rect ui_panel_begin_toolbar(int x, int y, int w, int h);

/* ── Widgets ──────────────────────────────────────────────── */

/* Draw text at position. Returns width in pixels. */
int ui_text(int x, int y, const char *text);
int ui_text_color(int x, int y, const char *text, SDL_Color color);
int ui_text_small(int x, int y, const char *text);
int ui_text_mono(int x, int y, const char *text);
int ui_text_mono_color(int x, int y, const char *text, SDL_Color color);

/* Icon (Codicon font). Renders a single icon glyph. Returns width. */
int ui_icon(int x, int y, uint16_t codepoint);
int ui_icon_color(int x, int y, uint16_t codepoint, SDL_Color color);
void ui_icon_centered(int x, int y, int w, int h, uint16_t codepoint, SDL_Color color);

/* Codicon codepoints (verified from codicon.ttf) */
#define ICON_ADD        0xEA60
#define ICON_EDIT       0xEA73
#define ICON_CLOSE      0xEA76
#define ICON_NEW_FILE   0xEA7F
#define ICON_TRASH      0xEA81
#define ICON_CHEVRON_L  0xEAB5
#define ICON_CHEVRON_R  0xEAB6
#define ICON_PAUSE      0xEAD1
#define ICON_FILES      0xEAF0
#define ICON_FOLDER     0xEAF7
#define ICON_PLAY       0xEB2C
#define ICON_REFRESH    0xEB37
#define ICON_SAVE       0xEB4B
#define ICON_TRI_LEFT   0xEB6F
#define ICON_TRI_RIGHT  0xEB70
#define ICON_ZOOM_IN    0xEB81
#define ICON_ZOOM_OUT   0xEB82
#define ICON_EXPORT     0xEBAC
#define ICON_COPY       0xEBCC
#define ICON_ERASER     0xEC5D
#define ICON_MIRROR     0xEA69
#define ICON_CLEAR_ALL  0xEABF
#define ICON_LAYERS     0xEA99
#define ICON_SYMBOL_COLOR 0xEB5C
#define ICON_DEBUG      0xEA87
#define ICON_CHEVRON_SEP 0xEAB6  /* > separator for breadcrumbs */
#define ICON_VM_RUNNING 0xEB7B
#define ICON_LAYOUT     0xEBEB
#define ICON_HOME       0xEB06
#define ICON_EYE        0xEA70
#define ICON_GAME       0xEC17
#define ICON_FILE_MEDIA 0xEAEA
#define ICON_PERSON     0xEA67

/* Button. Returns true on click. */
bool ui_button(int x, int y, int w, int h, const char *label);

/* Accent button (status bar color). Returns true on click. */
bool ui_button_accent(int x, int y, int w, int h, const char *label);

/* Small button (auto-sized to text). Returns true on click. */
bool ui_button_auto(int x, int y, const char *label);

/* Selectable list item (full width of panel content). Returns true on click. */
bool ui_selectable(int x, int y, int w, const char *label, bool selected);

/* Color swatch button. Returns true on click. */
bool ui_color_swatch(int x, int y, int size, SDL_Color color, bool selected);

/* Input text field. Returns true when Enter is pressed.
   buf is modified in-place. max_len is buffer size. */
bool ui_input_text(int x, int y, int w, char *buf, int max_len, bool *has_focus);

/* Integer input with +/- buttons. Returns true on change.
   label_w: fixed label width (0 = auto-size from text). */
bool ui_input_int(int x, int y, int w, const char *label, int *value, int step, int min_val, int max_val);
bool ui_input_int_ex(int x, int y, int w, const char *label, int label_w, int *value, int step, int min_val, int max_val);

/* Checkbox. Returns true on toggle. */
bool ui_checkbox(int x, int y, const char *label, bool *value);

/* Section header — full-width toolbar-colored title bar. Returns y after it. */
int ui_section(int x, int y, int w, const char *label);
SDL_Rect ui_section_bar(int x, int y, int w, const char *label);

/* Tooltip — call after the widget it describes. Appears near mouse. */
void ui_tooltip(const char *text);

/* ── Metrics ──────────────────────────────────────────────── */

int ui_line_height(void);       /* line height in pixels */
int ui_text_width(const char *text);  /* width of text in pixels */
int ui_char_width(void);        /* width of a single char (monospace) */

/* ── Input queries ────────────────────────────────────────── */

bool ui_mouse_in_rect(int x, int y, int w, int h);
bool ui_mouse_clicked(void);    /* left button just pressed this frame */
bool ui_mouse_down(void);       /* left button held down */
bool ui_mouse_right_clicked(void); /* right button just pressed */
void ui_mouse_pos(int *x, int *y);
void ui_mouse_wheel(int *x, int *y); /* scroll wheel delta this frame */

/* Was a key pressed this frame? */
bool ui_key_pressed(SDL_Keycode key);

/* Is a modifier held? */
bool ui_key_mod_cmd(void);     /* Cmd on macOS */
bool ui_key_mod_shift(void);

/* ── Activity bar ─────────────────────────────────────────── */

/* Draw a vertical activity bar icon. Returns true on click.
   active: whether this view is currently selected. */
bool ui_activity_icon(int x, int y, int w, int h, uint16_t icon, bool active);

/* ── Status bar ───────────────────────────────────────────── */

/* Draw status bar background. Returns content rect. */
SDL_Rect ui_status_bar(int x, int y, int w, int h);

/* Draw a status bar item. Returns width used. */
int ui_status_item(int x, int y, const char *text);
int ui_status_icon_item(int x, int y, uint16_t icon, const char *text);

/* ── Breadcrumb ───────────────────────────────────────────── */

/* Draw breadcrumb bar background. Returns content rect. */
SDL_Rect ui_breadcrumb_bar(int x, int y, int w, int h);

/* Draw breadcrumb segments. segs is array of strings, count is number. */
void ui_breadcrumb(int x, int y, const char **segs, int count);

/* ── Thin scrollbar (overlay style) ───────────────────────── */

/* Draw a thin vertical scrollbar. Returns new scroll position if dragged. */
int ui_scrollbar_v(int x, int y, int h, int content_h, int visible_h, int scroll_pos);

/* Draw a thin horizontal scrollbar. Returns new scroll position if dragged. */
int ui_scrollbar_h(int x, int y, int w, int content_w, int visible_w, int scroll_pos);

#ifdef __cplusplus
}
#endif
