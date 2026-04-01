/*
 * app.h — QL Studio application
 */
#pragma once

#include <SDL.h>
#include <SDL_opengl.h>
#include "../thirdparty/imgui/imgui.h"
#include "sprite.h"
#include "image.h"
#include <vector>
#include <string>

struct WatchEntry {
    uint32_t addr;
    std::string name;
    int type; // 0=byte, 1=word, 2=long
};

class App {
public:
    App();
    ~App();

    void load_project(const char *path);
    void save_project(const char *path = nullptr);
    void export_c(const char *directory = nullptr);

    void draw_menu_bar();
    void draw_sprite_editor();
    void draw_image_editor();
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

    // Images
    std::vector<QLImage> images;
    int current_image = 0;
    int image_zoom = 2;
    GLuint image_texture = 0;
    bool image_texture_dirty = true;

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

    // Emulator state
    Uint32 emu_start_ticks = 0;
    bool soft_bp_armed = false;

    // Watch list
    std::vector<WatchEntry> watches;

    // Editor helpers
    void update_sprite_texture();
    void update_preview_texture(int sprite_idx);
    void draw_palette();
    void draw_sprite_list();
    void draw_sprite_canvas();
    void draw_sprite_preview();
    void draw_sprite_properties();

    // Image editor helpers
    void draw_image_list();
    void draw_image_canvas();
    void draw_image_tools();
    void update_image_texture();
    void write_image_c_files(const std::string &dir);

    // File I/O
    void new_project();
    void open_project();
    void save_project_as();
    void screenshot();

    // C export
    void write_c_files(const std::string &dir);

    // File dialogs (macOS osascript)
    static std::string dialog_open(const char *title);
    static std::string dialog_save(const char *title, const char *default_name);
    static std::string dialog_folder(const char *title);
};
