/*
 * app.h — QL Studio application
 */
#pragma once

#include <SDL.h>
#include <SDL_opengl.h>
#include "../imgui/imgui.h"
#include "sprite.h"
#include <vector>
#include <string>

class App {
public:
    App();
    ~App();

    void load_project(const char *path);
    void save_project(const char *path = nullptr);
    void export_c(const char *directory = nullptr);

    void draw_menu_bar();
    void draw_sprite_editor();
    void draw_emulator();
    void draw_debug_panels();
    void cleanup();

private:
    // Project
    std::string project_path;
    std::string last_export_dir;
    std::vector<Sprite> sprites;
    int current_sprite = 0;
    int current_color = 7;
    bool modified = false;

    // Clipboard
    Sprite clipboard;
    bool has_clipboard = false;

    // Console
    std::vector<std::string> console_log;
    bool console_scroll_to_bottom = false;
    void log(const char *fmt, ...);

    // Sprite editor state
    GLuint sprite_texture = 0;
    GLuint preview_texture = 0;
    bool animate = false;
    int anim_frame = 0;
    float anim_timer = 0;

    // Editor helpers
    void update_sprite_texture();
    void update_preview_texture(int sprite_idx);
    void draw_palette();
    void draw_sprite_list();
    void draw_sprite_canvas();
    void draw_sprite_preview();
    void draw_sprite_properties();

    // File I/O
    void new_project();
    void open_project();
    void save_project_as();

    // C export
    void write_c_files(const std::string &dir);
};
