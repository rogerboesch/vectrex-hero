/*
 * QL Studio — ImGui + SDL2 version
 *
 * Sprite editor, emulator, and debug tools for Sinclair QL game development.
 */

#include <SDL.h>
#include <SDL_opengl.h>
#include "../thirdparty/imgui/imgui.h"
#include "../thirdparty/imgui/imgui_internal.h"
#include "../thirdparty/imgui/imgui_impl_sdl2.h"
#include "../thirdparty/imgui/imgui_impl_opengl3.h"

#include "app.h"
#include "emulator.h"

// SDL keycode → iQL virtual key (from rb_virtual_keys.h)
static int sdl_to_vk(SDL_Keycode k) {
    if (k >= SDLK_a && k <= SDLK_z) return k - SDLK_a;       // RBVK_A=0..RBVK_Z=25
    if (k >= SDLK_0 && k <= SDLK_9) return 26 + (k - SDLK_0); // RBVK_Num0=26..RBVK_Num9=35
    switch (k) {
        case SDLK_SPACE:     return 69;  // RBVK_Space
        case SDLK_TAB:       return 70;  // RBVK_Tab
        case SDLK_RETURN:    return 71;  // RBVK_Return
        case SDLK_BACKSPACE: return 72;  // RBVK_BackSpace
        case SDLK_ESCAPE:    return 73;  // RBVK_Escape
        case SDLK_LEFT:      return 74;  // RBVK_Left
        case SDLK_RIGHT:     return 75;  // RBVK_Right
        case SDLK_UP:        return 76;  // RBVK_Up
        case SDLK_DOWN:      return 77;  // RBVK_Down
        case SDLK_COMMA:     return 47;  // RBVK_Comma
        case SDLK_PERIOD:    return 48;  // RBVK_Period
        case SDLK_SLASH:     return 50;  // RBVK_Slash
        case SDLK_SEMICOLON: return 45;  // RBVK_SemiColon
        case SDLK_QUOTE:     return 46;  // RBVK_Quote
        case SDLK_MINUS:     return 51;  // RBVK_Dash
        case SDLK_EQUALS:    return 52;  // RBVK_Equal
        default: return -1;
    }
}

static bool emu_wants_keys = false;  // set by app when emulator image is hovered

int main(int argc, char *argv[]) {
    // Init SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        SDL_Log("SDL_Init error: %s", SDL_GetError());
        return 1;
    }

    // GL 3.2 Core (macOS requires this)
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    SDL_Window *window = SDL_CreateWindow(
        "QL Studio",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        1280, 800,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI
    );
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1); // vsync

    // Init ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.IniFilename = "qlstudio_imgui.ini";

    // Dark theme
    ImGui::StyleColorsDark();
    ImGuiStyle &style = ImGui::GetStyle();
    style.WindowRounding = 4.0f;
    style.FrameRounding = 2.0f;
    style.GrabRounding = 2.0f;
    style.TabRounding = 4.0f;
    style.FramePadding = ImVec2(6, 4);

    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init("#version 150");

    // Init app
    App app;
    if (argc > 1) {
        app.load_project(argv[1]);
    }

    // Main loop
    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            // Forward keyboard to emulator BEFORE ImGui processes the event
            bool key_forwarded = false;
            if (g_emu.is_running() && !g_emu.is_paused() && emu_wants_keys) {
                if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
                    int vk = sdl_to_vk(event.key.keysym.sym);
                    if (vk >= 0) {
                        bool pressed = (event.type == SDL_KEYDOWN);
                        bool shift = (event.key.keysym.mod & KMOD_SHIFT) != 0;
                        bool ctrl = (event.key.keysym.mod & KMOD_CTRL) != 0;
                        bool alt = (event.key.keysym.mod & KMOD_ALT) != 0;
                        g_emu.send_key(vk, pressed, shift, ctrl, alt);
                        key_forwarded = true;
                    }
                }
            }

            // Let ImGui process the event (skip if we forwarded to emulator)
            if (!key_forwarded)
                ImGui_ImplSDL2_ProcessEvent(&event);

            if (event.type == SDL_QUIT)
                running = false;
            if (event.type == SDL_WINDOWEVENT &&
                event.window.event == SDL_WINDOWEVENT_CLOSE)
                running = false;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        // Full-window dockspace with default layout on first run
        ImGuiID dockspace_id = ImGui::DockSpaceOverViewport();

        static bool first_frame = true;
        if (first_frame) {
            first_frame = false;

            // Check if layout already exists (ini file loaded)
            if (ImGui::DockBuilderGetNode(dockspace_id) == NULL ||
                ImGui::DockBuilderGetNode(dockspace_id)->IsLeafNode()) {
                // Set up default layout:
                //  [Sprites | Canvas            | Tools    ]
                //  [        |                    |          ]
                //  [        | Emulator           | CPU State]
                //  [        | Memory             |          ]
                ImGui::DockBuilderRemoveNode(dockspace_id);
                ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
                ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetMainViewport()->Size);

                ImGuiID left, center, right;
                ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Left, 0.15f, &left, &center);
                ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.22f, &right, &center);

                ImGuiID center_top, center_bottom;
                ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.45f, &center_bottom, &center_top);

                ImGuiID right_top, right_bottom;
                ImGui::DockBuilderSplitNode(right, ImGuiDir_Down, 0.4f, &right_bottom, &right_top);

                ImGui::DockBuilderDockWindow("Sprites", left);
                ImGui::DockBuilderDockWindow("Canvas", center_top);
                ImGui::DockBuilderDockWindow("Emulator", center_bottom);
                ImGui::DockBuilderDockWindow("Console", center_bottom);
                ImGui::DockBuilderDockWindow("Tools", right_top);
                ImGui::DockBuilderDockWindow("CPU State", right_bottom);
                ImGui::DockBuilderDockWindow("Memory", right_bottom);
                ImGui::DockBuilderFinish(dockspace_id);
            }
        }

        // App UI
        app.draw_menu_bar();
        app.draw_sprite_editor();
        app.draw_emulator();
        app.draw_debug_panels();

        // Render
        ImGui::Render();
        int w, h;
        SDL_GetWindowSize(window, &w, &h);
        glViewport(0, 0, w, h);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

    // Cleanup
    app.cleanup();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
