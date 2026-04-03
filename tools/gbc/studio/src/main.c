/*
 * GBC Studio — Tile/sprite editor for Game Boy Color
 */
#include <SDL.h>
#include "ui.h"
#include "app.h"
#include "gbc_emu.h"
#include "native_macos.h"
#include "style.h"

int main(int argc, char *argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) { SDL_Log("SDL_Init: %s", SDL_GetError()); return 1; }

    SDL_Window *window = SDL_CreateWindow("GBC Studio", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                           1280, 800, SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!window) { SDL_Log("Window: %s", SDL_GetError()); return 1; }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) { SDL_Log("Renderer: %s", SDL_GetError()); return 1; }

    if (!ui_init(renderer, STYLE_FONT_PATH, STYLE_FONT_SIZE, STYLE_MONO_FONT_PATH, STYLE_MONO_FONT_SIZE)) {
        SDL_Log("ui_init failed"); return 1;
    }

    native_menu_init();

    App app;
    app_init(&app, window, renderer);
    if (argc > 1) app_load_project(&app, argv[1]);

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
