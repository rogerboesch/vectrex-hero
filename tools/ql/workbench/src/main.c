/*
 * QL Workbench — SDL2-only sprite editor + emulator
 *
 * No third-party UI libraries. Custom UI built on SDL2 + SDL_ttf.
 * Native macOS menus and file dialogs.
 */

#include <SDL.h>
#include "ui.h"
#include "app.h"
#include "emulator.h"
#include "native_macos.h"
#include "style.h"

/* iQL virtual key mapping */
#include "rb_virtual_keys.h"

static int sdl_to_vk(SDL_Keycode k) {
    if (k >= SDLK_a && k <= SDLK_z) return RBVK_A + (k - SDLK_a);
    if (k >= SDLK_0 && k <= SDLK_9) return RBVK_Num0 + (k - SDLK_0);
    switch (k) {
        case SDLK_SPACE:        return RBVK_Space;
        case SDLK_TAB:          return RBVK_Tab;
        case SDLK_RETURN:       return RBVK_Return;
        case SDLK_BACKSPACE:    return RBVK_BackSpace;
        case SDLK_ESCAPE:       return RBVK_Escape;
        case SDLK_LEFT:         return RBVK_Left;
        case SDLK_RIGHT:        return RBVK_Right;
        case SDLK_UP:           return RBVK_Up;
        case SDLK_DOWN:         return RBVK_Down;
        case SDLK_COMMA:        return RBVK_Comma;
        case SDLK_PERIOD:       return RBVK_Period;
        case SDLK_SLASH:        return RBVK_Slash;
        case SDLK_BACKSLASH:    return RBVK_BackSlash;
        case SDLK_SEMICOLON:    return RBVK_SemiColon;
        case SDLK_QUOTE:        return RBVK_Quote;
        case SDLK_MINUS:        return RBVK_Dash;
        case SDLK_EQUALS:       return RBVK_Equal;
        case SDLK_LEFTBRACKET:  return RBVK_LSquareBracket;
        case SDLK_RIGHTBRACKET: return RBVK_RSquareBracket;
        case SDLK_BACKQUOTE:    return RBVK_Grave;
        case SDLK_F1:           return RBVK_F1;
        case SDLK_F2:           return RBVK_F2;
        case SDLK_F3:           return RBVK_F3;
        case SDLK_F4:           return RBVK_F4;
        case SDLK_F5:           return RBVK_F5;
        case SDLK_LSHIFT:       return RBVK_LShift;
        case SDLK_RSHIFT:       return RBVK_RShift;
        case SDLK_LCTRL:        return RBVK_LControl;
        case SDLK_RCTRL:        return RBVK_RControl;
        case SDLK_LALT:         return RBVK_LAlt;
        case SDLK_RALT:         return RBVK_RAlt;
        default: return -1;
    }
}

int main(int argc, char *argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        SDL_Log("SDL_Init: %s", SDL_GetError());
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow(
        "QL Workbench",
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

    if (!ui_init(renderer, STYLE_FONT_PATH, STYLE_FONT_SIZE,
                 STYLE_MONO_FONT_PATH, STYLE_MONO_FONT_SIZE)) {
        SDL_Log("ui_init failed");
        return 1;
    }

    native_menu_init_ex("QL Workbench");
    {
        char icon_path[512];
        const char *base = SDL_GetBasePath();
        snprintf(icon_path, sizeof(icon_path), "%s/../icon.icns", base ? base : ".");
        native_set_dock_icon(icon_path);
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
            /* Forward keyboard to emulator BEFORE UI processes it */
            bool key_forwarded = false;
            if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
                int vk = sdl_to_vk(event.key.keysym.sym);
                bool pressed = (event.type == SDL_KEYDOWN);
                if (app.emu_wants_keys && vk >= 0) {
                    bool shift = (event.key.keysym.mod & KMOD_SHIFT) != 0;
                    bool ctrl  = (event.key.keysym.mod & KMOD_CTRL) != 0;
                    bool alt   = (event.key.keysym.mod & KMOD_ALT) != 0;
                    emu_send_key(vk, pressed, shift, ctrl, alt);
                    key_forwarded = true;
                }
            }

            if (!key_forwarded)
                ui_process_event(&event);

            if (event.type == SDL_QUIT) running = 0;
            if (event.type == SDL_WINDOWEVENT &&
                event.window.event == SDL_WINDOWEVENT_CLOSE) running = 0;
        }

        /* Poll native menu actions */
        MenuAction action = native_menu_poll();
        switch (action) {
            case MENU_NEW:     app_new_project(&app); break;
            case MENU_OPEN:    app_open_project(&app); break;
            case MENU_SAVE:    app_save_project(&app, NULL); break;
            case MENU_SAVE_AS: app_save_project_as(&app); break;
            case MENU_EXPORT:  app_export_c_dialog(&app); break;
            case MENU_QUIT:    running = 0; break;
            case MENU_COPY:
                if (app.current_sprite < app.sprite_count) {
                    app.clipboard = app.sprites[app.current_sprite];
                    app.has_clipboard = 1;
                }
                break;
            case MENU_PASTE:
                if (app.current_sprite < app.sprite_count && app.has_clipboard) {
                    memcpy(app.sprites[app.current_sprite].pixels,
                           app.clipboard.pixels, sizeof(app.clipboard.pixels));
                    sprite_resize(&app.sprites[app.current_sprite],
                                  app.clipboard.width, app.clipboard.height);
                }
                break;
            case MENU_CLEAR:
                if (app.current_sprite < app.sprite_count)
                    sprite_clear(&app.sprites[app.current_sprite]);
                break;
            default: break;
        }

        app_draw(&app);

        SDL_RenderPresent(renderer);
    }

    emu_stop();
    app_cleanup(&app);
    ui_shutdown();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
