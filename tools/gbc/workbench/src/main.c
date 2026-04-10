/*
 * GBC Workbench — Tile/sprite editor for Game Boy Color
 */
#include <SDL.h>
#include "ui.h"
#include "app.h"
#include "gbc_emu.h"
#include "native_macos.h"
#include "style.h"

int main(int argc, char *argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) { SDL_Log("SDL_Init: %s", SDL_GetError()); return 1; }

    SDL_Window *window = SDL_CreateWindow("GBC Workbench", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                           1280, 800, SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!window) { SDL_Log("Window: %s", SDL_GetError()); return 1; }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) { SDL_Log("Renderer: %s", SDL_GetError()); return 1; }

    if (!ui_init(renderer, STYLE_FONT_PATH, STYLE_FONT_SIZE, STYLE_MONO_FONT_PATH, STYLE_MONO_FONT_SIZE)) {
        SDL_Log("ui_init failed"); return 1;
    }

    native_menu_init_ex("GBC Workbench");
    {
        char icon_path[512];
        const char *base = SDL_GetBasePath();
        snprintf(icon_path, sizeof(icon_path), "%s/../icon.icns", base ? base : ".");
        native_set_dock_icon(icon_path);
    }

    App app;
    app_init(&app, window, renderer);
    /* Load project file */
    if (argc > 1) {
        app_load_project(&app, argv[1]);
    } else {
        char project_path[512];
        snprintf(project_path, sizeof(project_path), "%s/../../../gbc/game-rescue.json", SDL_GetBasePath());
        app_load_project(&app, project_path);
    }

    int running = 1;
    while (running) {
        ui_begin_frame();
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            bool fwd = false;
            if (gbc_emu_wants_keys()) {
                if (event.type == SDL_KEYDOWN) { gbc_emu_key_down(event.key.keysym.sym); fwd = true; }
                if (event.type == SDL_KEYUP)   { gbc_emu_key_up(event.key.keysym.sym); fwd = true; }
            }
            if (!fwd) ui_process_event(&event);
            if (event.type == SDL_QUIT) running = 0;
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE) running = 0;
        }

        MenuAction action = native_menu_poll();
        switch (action) {
            case MENU_NEW:     app_new_project(&app); break;
            case MENU_OPEN:    app_open_project(&app); break;
            case MENU_SAVE:    app_save_project(&app, NULL); break;
            case MENU_SAVE_AS: app_save_project_as(&app); break;
            case MENU_EXPORT:  app_export_c(&app); break;
            case MENU_QUIT:    running = 0; break;
            case MENU_COPY:
                if (app.sel_type == SEL_TILE && app.cur_tset_tile < app.tmap.tileset.used_count) {
                    app.clip_type = CLIP_TILE;
                    app.clip_tile = app.tmap.tileset.entries[app.cur_tset_tile];
                    app_log_info(&app, "Copied tile %d", app.cur_tset_tile);
                } else if (app.sel_type == SEL_SPRITE && app.cur_sprite < app.sprite_count) {
                    app.clip_type = CLIP_SPRITE;
                    app.clip_sprite = app.sprites[app.cur_sprite];
                    app_log_info(&app, "Copied sprite %d (%s)", app.cur_sprite, app.sprites[app.cur_sprite].name);
                }
                break;
            case MENU_PASTE:
                if (app.clip_type == CLIP_TILE && app.sel_type == SEL_TILE &&
                    app.cur_tset_tile < app.tmap.tileset.used_count) {
                    app.tmap.tileset.entries[app.cur_tset_tile] = app.clip_tile;
                    app.modified = true;
                    app_log_info(&app, "Pasted tile to %d", app.cur_tset_tile);
                } else if (app.clip_type == CLIP_SPRITE && app.sel_type == SEL_SPRITE &&
                           app.cur_sprite < app.sprite_count) {
                    memcpy(app.sprites[app.cur_sprite].data, app.clip_sprite.data, TILE_BYTES);
                    app.sprites[app.cur_sprite].palette = app.clip_sprite.palette;
                    app.modified = true;
                    app_log_info(&app, "Pasted sprite to %d (%s)", app.cur_sprite, app.sprites[app.cur_sprite].name);
                } else {
                    app_log_warn(&app, "Paste failed: clip=%d sel=%d", app.clip_type, app.sel_type);
                }
                break;
            default: break;
        }

        app_draw(&app);
        SDL_RenderPresent(renderer);
    }

    gbc_emu_stop();
    app_cleanup(&app);
    ui_shutdown();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
