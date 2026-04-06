/*
 * ui.c — Minimal SDL2 UI library implementation
 */

#include "ui.h"
#include <string.h>
#include <stdio.h>

/* ── Theme (dark) ─────────────────────────────────────────── */

UITheme ui_theme = {
    /* Base UI */
    .bg              = { 30,  30,  30, 255},
    .panel_bg        = { 40,  40,  40, 255},
    .panel_title     = { 55,  55,  55, 255},
    .toolbar_bg      = { 48,  48,  52, 255},
    .border          = { 60,  60,  60, 255},
    .text            = {220, 220, 220, 255},
    .text_dim        = {130, 130, 130, 255},
    .text_dark       = { 90,  90,  90, 255},
    /* Buttons */
    .btn_bg          = { 65,  65,  65, 255},
    .btn_hover       = { 80,  80,  80, 255},
    .btn_active      = { 50, 100, 150, 255},
    .btn_text        = {220, 220, 220, 255},
    /* Input */
    .input_bg        = { 25,  25,  25, 255},
    .input_border    = { 80,  80,  80, 255},
    .select_bg       = { 45,  70, 100, 255},
    .swatch_sel      = {255, 255, 255, 255},
    /* Canvas */
    .canvas_grid     = { 50,  50,  50, 255},
    .canvas_byte     = { 80,  80,  80, 255},
    .preview_bg      = {  0,   0,   0, 255},
    /* Console */
    .console_bg      = { 22,  22,  22, 255},
    .console_err     = {240,  80,  80, 255},
    .console_warn    = {255, 200,  50, 255},
    /* Emulator */
    .emu_placeholder = { 40,   0,   0, 255},
    .emu_text        = {200,  80,  80, 255},
    /* Misc */
    .tab_active      = { 80, 140, 200, 255},
    .tooltip_bg      = { 20,  20,  20, 240},
};

/* ── State ────────────────────────────────────────────────── */

static SDL_Renderer *g_renderer = NULL;
static TTF_Font *g_font = NULL;
static TTF_Font *g_font_small = NULL;
static TTF_Font *g_font_mono = NULL;
static TTF_Font *g_font_icon = NULL;

/* Input state */
static int g_mouse_x, g_mouse_y;
static bool g_mouse_left_down;
static bool g_mouse_left_clicked;   /* just pressed this frame */
static bool g_mouse_right_clicked;
static bool g_mouse_left_released;
static int g_mouse_wheel_x, g_mouse_wheel_y;

/* Keyboard */
static SDL_Keycode g_keys_pressed[16];
static int g_keys_count;
static SDL_Keymod g_key_mod;

/* Text input */
static char g_text_input[64];
static bool g_has_text_input;
static SDL_Keycode g_last_key;

/* Panel stack */
static SDL_Rect g_panel_content;
static SDL_Rect g_clip_stack[8];
static int g_clip_depth = 0;

/* Cached metrics */
static int g_line_height = 16;
static int g_char_width = 8;

/* ── Helpers ──────────────────────────────────────────────── */

static void set_color(SDL_Color c) {
    SDL_SetRenderDrawColor(g_renderer, c.r, c.g, c.b, c.a);
}

static void fill_rect(int x, int y, int w, int h, SDL_Color c) {
    SDL_Rect r = {x, y, w, h};
    set_color(c);
    SDL_RenderFillRect(g_renderer, &r);
}

static void draw_rect(int x, int y, int w, int h, SDL_Color c) {
    SDL_Rect r = {x, y, w, h};
    set_color(c);
    SDL_RenderDrawRect(g_renderer, &r);
}

/* Render text, return width. Caller does NOT free texture. */
static int render_text(TTF_Font *font, int x, int y, const char *text, SDL_Color color) {
    if (!text || !text[0]) return 0;
    SDL_Surface *surf = TTF_RenderUTF8_Blended(font, text, color);
    if (!surf) return 0;
    SDL_Texture *tex = SDL_CreateTextureFromSurface(g_renderer, surf);
    SDL_Rect dst = {x, y, surf->w, surf->h};
    SDL_RenderCopy(g_renderer, tex, NULL, &dst);
    int w = surf->w;
    SDL_FreeSurface(surf);
    SDL_DestroyTexture(tex);
    return w;
}

/* ── Init / shutdown ──────────────────────────────────────── */

bool ui_init(SDL_Renderer *renderer, const char *font_path, int font_size,
             const char *mono_font_path, int mono_font_size) {
    g_renderer = renderer;

    if (TTF_Init() < 0) {
        fprintf(stderr, "TTF_Init: %s\n", TTF_GetError());
        return false;
    }

    g_font = TTF_OpenFont(font_path, font_size);
    if (!g_font) {
        fprintf(stderr, "TTF_OpenFont(%s, %d): %s\n", font_path, font_size, TTF_GetError());
        return false;
    }

    g_font_small = TTF_OpenFont(font_path, font_size - 2);
    if (!g_font_small) g_font_small = g_font;

    g_font_mono = TTF_OpenFont(mono_font_path, mono_font_size);
    if (!g_font_mono) g_font_mono = g_font;

    /* Try loading codicon icon font from same directory as the shared headers */
    {
        /* Build path: replace font filename with codicon.ttf using SDL_GetBasePath */
        char icon_path[512];
        const char *base = SDL_GetBasePath();
        snprintf(icon_path, sizeof(icon_path), "%s/../../shared/codicon.ttf", base ? base : ".");
        g_font_icon = TTF_OpenFont(icon_path, font_size);
        /* Not fatal if missing */
    }

    g_line_height = TTF_FontLineSkip(g_font) + 2;
    /* Measure a char for monospace width */
    int adv = 0;
    TTF_GlyphMetrics(g_font, 'M', NULL, NULL, NULL, NULL, &adv);
    g_char_width = adv;

    return true;
}

void ui_shutdown(void) {
    if (g_font_icon) TTF_CloseFont(g_font_icon);
    if (g_font_mono && g_font_mono != g_font) TTF_CloseFont(g_font_mono);
    if (g_font_small && g_font_small != g_font) TTF_CloseFont(g_font_small);
    if (g_font) TTF_CloseFont(g_font);
    g_font = NULL;
    g_font_small = NULL;
    TTF_Quit();
}

/* ── Frame cycle ──────────────────────────────────────────── */

void ui_begin_frame(void) {
    g_mouse_left_clicked = false;
    g_mouse_right_clicked = false;
    g_mouse_left_released = false;
    g_mouse_wheel_x = 0;
    g_mouse_wheel_y = 0;
    g_keys_count = 0;
    g_has_text_input = false;
    g_text_input[0] = 0;
    g_last_key = 0;
    g_key_mod = SDL_GetModState();
}

void ui_process_event(const SDL_Event *event) {
    switch (event->type) {
    case SDL_MOUSEMOTION:
        g_mouse_x = event->motion.x;
        g_mouse_y = event->motion.y;
        break;
    case SDL_MOUSEBUTTONDOWN:
        if (event->button.button == SDL_BUTTON_LEFT) {
            g_mouse_left_down = true;
            g_mouse_left_clicked = true;
        }
        if (event->button.button == SDL_BUTTON_RIGHT) {
            g_mouse_right_clicked = true;
        }
        break;
    case SDL_MOUSEBUTTONUP:
        if (event->button.button == SDL_BUTTON_LEFT) {
            g_mouse_left_down = false;
            g_mouse_left_released = true;
        }
        break;
    case SDL_MOUSEWHEEL:
        g_mouse_wheel_x += event->wheel.x;
        g_mouse_wheel_y += event->wheel.y;
        break;
    case SDL_KEYDOWN:
        if (g_keys_count < 16) {
            g_keys_pressed[g_keys_count++] = event->key.keysym.sym;
        }
        g_last_key = event->key.keysym.sym;
        g_key_mod = event->key.keysym.mod;
        break;
    case SDL_TEXTINPUT:
        g_has_text_input = true;
        strncpy(g_text_input, event->text.text, sizeof(g_text_input) - 1);
        break;
    }
}

/* ── Layout ───────────────────────────────────────────────── */

void ui_panel_begin(const char *title, int x, int y, int w, int h) {
    /* Panel background */
    fill_rect(x, y, w, h, ui_theme.panel_bg);
    draw_rect(x, y, w, h, ui_theme.border);

    /* Title bar */
    int title_h = g_line_height + 4;
    fill_rect(x, y, w, title_h, ui_theme.panel_title);
    render_text(g_font, x + 6, y + 2, title, ui_theme.text);

    /* Content area */
    g_panel_content = (SDL_Rect){x + 4, y + title_h + 10, w - 8, h - title_h - 14};

    /* Set clip to panel */
    if (g_clip_depth < 8) {
        g_clip_stack[g_clip_depth++] = (SDL_Rect){x, y, w, h};
    }
    SDL_RenderSetClipRect(g_renderer, &g_clip_stack[g_clip_depth - 1]);
}

SDL_Rect ui_panel_begin_toolbar(int x, int y, int w, int h) {
    /* Panel background */
    fill_rect(x, y, w, h, ui_theme.panel_bg);
    draw_rect(x, y, w, h, ui_theme.border);

    /* Toolbar area */
    int title_h = g_line_height + 8;
    fill_rect(x, y, w, title_h, ui_theme.toolbar_bg);

    /* Content area */
    g_panel_content = (SDL_Rect){x + 4, y + title_h + 10, w - 8, h - title_h - 14};

    /* Set clip to panel */
    if (g_clip_depth < 8) {
        g_clip_stack[g_clip_depth++] = (SDL_Rect){x, y, w, h};
    }
    SDL_RenderSetClipRect(g_renderer, &g_clip_stack[g_clip_depth - 1]);

    /* Return toolbar rect for caller to place buttons */
    return (SDL_Rect){x + 4, y + 2, w - 8, title_h - 4};
}

void ui_panel_end(void) {
    if (g_clip_depth > 0) g_clip_depth--;
    if (g_clip_depth > 0) {
        SDL_RenderSetClipRect(g_renderer, &g_clip_stack[g_clip_depth - 1]);
    } else {
        SDL_RenderSetClipRect(g_renderer, NULL);
    }
}

SDL_Rect ui_panel_content(void) {
    return g_panel_content;
}

/* ── Text ─────────────────────────────────────────────────── */

int ui_text(int x, int y, const char *text) {
    return render_text(g_font, x, y, text, ui_theme.text);
}

int ui_text_color(int x, int y, const char *text, SDL_Color color) {
    return render_text(g_font, x, y, text, color);
}

int ui_text_small(int x, int y, const char *text) {
    return render_text(g_font_small, x, y, text, ui_theme.text_dim);
}

int ui_text_mono(int x, int y, const char *text) {
    return render_text(g_font_mono, x, y, text, ui_theme.text);
}

int ui_text_mono_color(int x, int y, const char *text, SDL_Color color) {
    return render_text(g_font_mono, x, y, text, color);
}

/* ── Icon (Codicon font) ──────────────────────────────────── */

int ui_icon(int x, int y, uint16_t codepoint) {
    return ui_icon_color(x, y, codepoint, ui_theme.text);
}

int ui_icon_color(int x, int y, uint16_t codepoint, SDL_Color color) {
    if (!g_font_icon) return 0;
    char buf[4];
    buf[0] = (char)(0xE0 | ((codepoint >> 12) & 0x0F));
    buf[1] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
    buf[2] = (char)(0x80 | (codepoint & 0x3F));
    buf[3] = 0;
    return render_text(g_font_icon, x, y, buf, color);
}

void ui_icon_centered(int x, int y, int w, int h, uint16_t codepoint, SDL_Color color) {
    if (!g_font_icon) return;
    char buf[4];
    buf[0] = (char)(0xE0 | ((codepoint >> 12) & 0x0F));
    buf[1] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
    buf[2] = (char)(0x80 | (codepoint & 0x3F));
    buf[3] = 0;
    /* Measure glyph size then center */
    int tw = 0, th = 0;
    TTF_SizeUTF8(g_font_icon, buf, &tw, &th);
    int ix = x + (w - tw) / 2;
    int iy = y + (h - th) / 2;
    render_text(g_font_icon, ix, iy, buf, color);
}

/* ── Button ───────────────────────────────────────────────── */

bool ui_button(int x, int y, int w, int h, const char *label) {
    bool hover = ui_mouse_in_rect(x, y, w, h);
    bool pressed = hover && g_mouse_left_down;
    bool clicked = hover && g_mouse_left_clicked;

    SDL_Color bg = pressed ? ui_theme.btn_active :
                   hover   ? ui_theme.btn_hover  :
                             ui_theme.btn_bg;
    fill_rect(x, y, w, h, bg);
    draw_rect(x, y, w, h, ui_theme.border);

    /* Center text */
    int tw = ui_text_width(label);
    int tx = x + (w - tw) / 2;
    int ty = y + (h - g_line_height) / 2;
    render_text(g_font, tx, ty, label, ui_theme.btn_text);

    return clicked;
}

bool ui_button_auto(int x, int y, const char *label) {
    int tw = ui_text_width(label);
    int w = tw + 16;
    int h = g_line_height + 6;
    return ui_button(x, y, w, h, label);
}

/* ── Selectable ───────────────────────────────────────────── */

bool ui_selectable(int x, int y, int w, const char *label, bool selected) {
    int h = g_line_height + 2;
    bool hover = ui_mouse_in_rect(x, y, w, h);
    bool clicked = hover && g_mouse_left_clicked;

    if (selected) {
        fill_rect(x, y, w, h, ui_theme.select_bg);
    } else if (hover) {
        fill_rect(x, y, w, h, ui_theme.btn_hover);
    }

    render_text(g_font, x + 4, y + 1, label, ui_theme.text);
    return clicked;
}

/* ── Color swatch ─────────────────────────────────────────── */

bool ui_color_swatch(int x, int y, int size, SDL_Color color, bool selected) {
    bool hover = ui_mouse_in_rect(x, y, size, size);
    bool clicked = hover && g_mouse_left_clicked;

    fill_rect(x, y, size, size, color);
    SDL_Color border = selected ? ui_theme.swatch_sel : ui_theme.border;
    draw_rect(x, y, size, size, border);
    if (selected) {
        draw_rect(x + 1, y + 1, size - 2, size - 2, border);
    }

    return clicked;
}

/* ── Input text ───────────────────────────────────────────── */

bool ui_input_text(int x, int y, int w, char *buf, int max_len, bool *has_focus) {
    int h = g_line_height + 6;
    bool hover = ui_mouse_in_rect(x, y, w, h);
    bool enter_pressed = false;

    /* Click to focus */
    if (g_mouse_left_clicked) {
        bool was_focused = *has_focus;
        *has_focus = hover;
        if (hover && !was_focused) {
            SDL_StartTextInput();
        } else if (!hover && was_focused) {
            SDL_StopTextInput();
        }
    }

    /* Background */
    fill_rect(x, y, w, h, ui_theme.input_bg);
    SDL_Color border = *has_focus ? ui_theme.btn_active : ui_theme.input_border;
    draw_rect(x, y, w, h, border);

    /* Handle text input when focused */
    if (*has_focus) {
        /* Append typed characters */
        if (g_has_text_input) {
            int len = (int)strlen(buf);
            int add = (int)strlen(g_text_input);
            if (len + add < max_len) {
                strcat(buf, g_text_input);
            }
        }
        /* Backspace */
        if (g_last_key == SDLK_BACKSPACE) {
            int len = (int)strlen(buf);
            if (len > 0) buf[len - 1] = 0;
        }
        /* Enter */
        if (g_last_key == SDLK_RETURN) {
            enter_pressed = true;
            *has_focus = false;
            SDL_StopTextInput();
        }
        /* Escape */
        if (g_last_key == SDLK_ESCAPE) {
            *has_focus = false;
            SDL_StopTextInput();
        }
    }

    /* Draw text + cursor */
    render_text(g_font, x + 4, y + 3, buf, ui_theme.text);
    if (*has_focus) {
        int tw = ui_text_width(buf);
        int cx = x + 4 + tw;
        int cy = y + 3;
        SDL_Rect cursor = {cx, cy, 2, g_line_height};
        set_color(ui_theme.text);
        SDL_RenderFillRect(g_renderer, &cursor);
    }

    return enter_pressed;
}

/* ── Input int ────────────────────────────────────────────── */

bool ui_input_int(int x, int y, int w, const char *label, int *value,
                  int step, int min_val, int max_val) {
    return ui_input_int_ex(x, y, w, label, 0, value, step, min_val, max_val);
}

bool ui_input_int_ex(int x, int y, int w, const char *label, int label_w, int *value,
                     int step, int min_val, int max_val) {
    int h = g_line_height + 6;
    bool changed = false;

    /* Label */
    int lw = render_text(g_font, x, y + 3, label, ui_theme.text_dim);
    if (label_w > 0) lw = label_w;
    int bx = x + lw + 6;

    /* - button */
    int bw = h;
    if (ui_button(bx, y, bw, h, "-")) {
        *value -= step;
        if (*value < min_val) *value = min_val;
        changed = true;
    }

    /* Value display */
    char vbuf[16];
    snprintf(vbuf, sizeof(vbuf), "%d", *value);
    int vw = w - lw - 6 - bw * 2;
    fill_rect(bx + bw, y, vw, h, ui_theme.input_bg);
    int tw = ui_text_width(vbuf);
    render_text(g_font, bx + bw + (vw - tw) / 2, y + 3, vbuf, ui_theme.text);

    /* + button */
    if (ui_button(bx + bw + vw, y, bw, h, "+")) {
        *value += step;
        if (*value > max_val) *value = max_val;
        changed = true;
    }

    return changed;
}

/* ── Checkbox ─────────────────────────────────────────────── */

bool ui_checkbox(int x, int y, const char *label, bool *value) {
    int size = g_line_height;
    bool hover = ui_mouse_in_rect(x, y, size + ui_text_width(label) + 8, size);
    bool clicked = hover && g_mouse_left_clicked;

    /* Box */
    fill_rect(x, y, size, size, ui_theme.input_bg);
    draw_rect(x, y, size, size, ui_theme.input_border);

    if (*value) {
        /* Checkmark — filled inner square */
        fill_rect(x + 3, y + 3, size - 6, size - 6, ui_theme.btn_active);
    }

    /* Label */
    render_text(g_font, x + size + 6, y, label, ui_theme.text);

    if (clicked) *value = !(*value);
    return clicked;
}

/* ── Section header ───────────────────────────────────────── */

int ui_section(int x, int y, int w, const char *label) {
    int h = g_line_height + 4;
    fill_rect(x, y, w, h, ui_theme.toolbar_bg);
    render_text(g_font, x + 4, y + 2, label, ui_theme.text_dim);
    return y + h + 10;
}

SDL_Rect ui_section_bar(int x, int y, int w, const char *label) {
    int h = g_line_height + 4;
    fill_rect(x, y, w, h, ui_theme.toolbar_bg);
    SDL_Color white = {255, 255, 255, 255};
    render_text(g_font, x + 8, y + 2, label, white);
    return (SDL_Rect){x, y, w, h};
}

/* ── Tooltip ──────────────────────────────────────────────── */

void ui_tooltip(const char *text) {
    int tw = ui_text_width(text);
    int h = g_line_height + 4;
    int x = g_mouse_x + 12;
    int y = g_mouse_y + 12;

    fill_rect(x, y, tw + 8, h, ui_theme.tooltip_bg);
    draw_rect(x, y, tw + 8, h, ui_theme.border);
    render_text(g_font, x + 4, y + 2, text, ui_theme.text);
}

/* ── Metrics ──────────────────────────────────────────────── */

int ui_line_height(void) { return g_line_height; }

int ui_text_width(const char *text) {
    if (!text || !text[0] || !g_font) return 0;
    int w = 0;
    TTF_SizeUTF8(g_font, text, &w, NULL);
    return w;
}

int ui_char_width(void) { return g_char_width; }

/* ── Input queries ────────────────────────────────────────── */

bool ui_mouse_in_rect(int x, int y, int w, int h) {
    return g_mouse_x >= x && g_mouse_x < x + w &&
           g_mouse_y >= y && g_mouse_y < y + h;
}

bool ui_mouse_clicked(void) { return g_mouse_left_clicked; }
bool ui_mouse_down(void)    { return g_mouse_left_down; }
bool ui_mouse_right_clicked(void) { return g_mouse_right_clicked; }

void ui_mouse_pos(int *x, int *y) {
    *x = g_mouse_x;
    *y = g_mouse_y;
}
void ui_mouse_wheel(int *x, int *y) {
    *x = g_mouse_wheel_x;
    *y = g_mouse_wheel_y;
}

bool ui_key_pressed(SDL_Keycode key) {
    for (int i = 0; i < g_keys_count; i++) {
        if (g_keys_pressed[i] == key) return true;
    }
    return false;
}

bool ui_key_mod_cmd(void)   { return (g_key_mod & KMOD_GUI) != 0; }
bool ui_key_mod_shift(void) { return (g_key_mod & KMOD_SHIFT) != 0; }
