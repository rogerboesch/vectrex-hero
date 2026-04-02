/*
 * app.h — QL Studio 2 application state
 */
#pragma once

#include <SDL.h>
#include <stdbool.h>
#include "sprite.h"
#include "image.h"

/* Which main view is active */
typedef enum {
    VIEW_SPRITES,
    VIEW_IMAGES,
    VIEW_EMULATOR
} ViewMode;

typedef struct {
    /* Window / renderer */
    SDL_Window *window;
    SDL_Renderer *renderer;
    int win_w, win_h;

    /* View mode */
    ViewMode view;

    /* Sprites */
    Sprite sprites[MAX_SPRITES];
    int sprite_count;
    int current_sprite;
    int current_color;

    /* Canvas texture (for sprite pixel editing) */
    SDL_Texture *canvas_tex;
    int canvas_tex_w, canvas_tex_h;

    /* Preview texture */
    SDL_Texture *preview_tex;
    bool animate;
    int anim_frame;
    float anim_timer;

    /* Images */
    QLImage images[MAX_IMAGES];
    int image_count;
    int current_image;
    int image_zoom;          /* 1, 2, or 4 */
    SDL_Texture *image_tex;
    bool image_tex_dirty;
    int image_scroll_x, image_scroll_y;

    /* Clipboard */
    Sprite clipboard;
    int has_clipboard;

    /* Project */
    char project_path[512];
    int modified;

    /* UI state */
    bool name_focus;       /* is name input focused */
    char name_buf[64];     /* name input buffer */

    /* Emulator */
    Uint32 emu_start_ticks;
    bool soft_bp_armed;
    int speed_idx;
    bool bp_enabled;
    bool trap_log_enabled;
    bool emu_wants_keys;

    /* Console */
    char console_lines[64][256];
    int console_count;
} App;

/* ── Core ─────────────────────────────────────────────────── */

void app_init(App *app, SDL_Window *window, SDL_Renderer *renderer);
void app_cleanup(App *app);
void app_draw(App *app);
void app_log(App *app, const char *fmt, ...);

/* ── File I/O ─────────────────────────────────────────────── */

void app_new_project(App *app);
void app_load_project(App *app, const char *path);
void app_save_project(App *app, const char *path);
void app_export_c(App *app, const char *directory);

/* ── Panel draw functions (each in its own .c file) ───────── */

void draw_sprite_list(App *app, int x, int y, int w, int h);
void draw_sprite_canvas(App *app, int x, int y, int w, int h);
void draw_palette(App *app, int x, int y, int w, int *out_y);
void draw_properties(App *app, int x, int y, int w, int *out_y);
void draw_preview(App *app, int x, int y, int w, int *out_y);
void draw_image_list(App *app, int x, int y, int w, int h);
void draw_image_canvas(App *app, int x, int y, int w, int h);
void draw_image_tools(App *app, int x, int y, int w, int h);
void draw_console(App *app, int x, int y, int w, int h);
void draw_emulator(App *app, int x, int y, int w, int h);
void draw_cpu_state(App *app, int x, int y, int w, int h);
void draw_memory(App *app, int x, int y, int w, int h);
