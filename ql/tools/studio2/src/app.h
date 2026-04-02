/*
 * app.h — QL Studio 2 application state
 */
#pragma once

#include <SDL.h>
#include <stdbool.h>
#include "sprite.h"

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

    /* Clipboard */
    Sprite clipboard;
    int has_clipboard;

    /* Project */
    char project_path[512];
    int modified;

    /* UI state */
    bool name_focus;       /* is name input focused */
    char name_buf[64];     /* name input buffer */

    /* Console */
    char console_lines[64][256];
    int console_count;
} App;

void app_init(App *app, SDL_Window *window, SDL_Renderer *renderer);
void app_cleanup(App *app);
void app_draw(App *app);

/* File I/O */
void app_new_project(App *app);
void app_load_project(App *app, const char *path);
void app_save_project(App *app, const char *path);
void app_export_c(App *app, const char *directory);

/* Console */
void app_log(App *app, const char *fmt, ...);
