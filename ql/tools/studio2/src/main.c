/*
 * QL Studio 2 — SDL2-only sprite editor
 *
 * No third-party UI libraries. Custom UI built on SDL2 + SDL_ttf.
 */

#include <SDL.h>
#include "ui.h"
#include "app.h"
#include "style.h"

int main(int argc, char *argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        SDL_Log("SDL_Init: %s", SDL_GetError());
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow(
        "QL Studio 2",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        1280, 800,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI
    );
    if (!window) {
        SDL_Log("SDL_CreateWindow: %s", SDL_GetError());
        return 1;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(
        window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );
    if (!renderer) {
        SDL_Log("SDL_CreateRenderer: %s", SDL_GetError());
        return 1;
    }

    if (!ui_init(renderer, STYLE_FONT_PATH, STYLE_FONT_SIZE)) {
        SDL_Log("ui_init failed");
        return 1;
    }

    App app;
    app_init(&app, window, renderer);

    if (argc > 1) {
        app_load_project(&app, argv[1]);
    }

    int running = 1;
    while (running) {
        ui_begin_frame();

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ui_process_event(&event);

            if (event.type == SDL_QUIT) running = 0;
            if (event.type == SDL_WINDOWEVENT &&
                event.window.event == SDL_WINDOWEVENT_CLOSE) running = 0;
        }

        /* Keyboard shortcuts (Cmd+key) */
        if (ui_key_mod_cmd()) {
            if (ui_key_pressed(SDLK_n)) app_new_project(&app);
            if (ui_key_pressed(SDLK_s)) app_save_project(&app, NULL);
            if (ui_key_pressed(SDLK_q)) running = 0;
        }

        app_draw(&app);

        SDL_RenderPresent(renderer);
    }

    app_cleanup(&app);
    ui_shutdown();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
