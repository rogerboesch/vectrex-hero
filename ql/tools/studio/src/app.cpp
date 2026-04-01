/*
 * app.cpp — QL Studio application implementation
 */

#include "app.h"
#include "../thirdparty/imgui/imgui.h"
// Minimal file dialog stubs (replace with native dialogs later)
static const char *simple_file_dialog(const char *title, const char *default_path) {
    // For now, return NULL — use command line args or menu shortcuts
    (void)title; (void)default_path;
    return NULL;
}
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <fstream>
#include <sstream>

// Simple JSON helpers (minimal, no external lib)
#include "json_io.h"

App::App() {
    sprites.push_back(Sprite("sprite_0", 10, 20));
    glGenTextures(1, &sprite_texture);
    glGenTextures(1, &preview_texture);
}

void App::log(const char *fmt, ...) {
    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    console_log.push_back(buf);
    if ((int)console_log.size() > 500)
        console_log.erase(console_log.begin());
    console_scroll_to_bottom = true;
}

App::~App() {
    if (sprite_texture) glDeleteTextures(1, &sprite_texture);
    if (preview_texture) glDeleteTextures(1, &preview_texture);
}

void App::cleanup() {}

// ============================================================
// Menu bar
// ============================================================

void App::draw_menu_bar() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Project", "Cmd+N")) new_project();
            if (ImGui::MenuItem("Open Project...", "Cmd+O")) open_project();
            if (ImGui::MenuItem("Save Project", "Cmd+S")) save_project();
            if (ImGui::MenuItem("Save Project As...", "Cmd+Shift+S")) save_project_as();
            ImGui::Separator();
            if (ImGui::MenuItem("Export C Files...", "Cmd+E")) export_c();
            ImGui::Separator();
            if (ImGui::MenuItem("Quit", "Cmd+Q")) {
                SDL_Event e; e.type = SDL_QUIT;
                SDL_PushEvent(&e);
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit")) {
            bool has_spr = current_sprite < (int)sprites.size();
            if (ImGui::MenuItem("Copy Sprite", "Cmd+C", false, has_spr)) {
                clipboard = sprites[current_sprite];
                has_clipboard = true;
            }
            if (ImGui::MenuItem("Paste Sprite", "Cmd+V", false, has_spr && has_clipboard)) {
                sprites[current_sprite].pixels = clipboard.pixels;
                sprites[current_sprite].resize(clipboard.width, clipboard.height);
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Flip Horizontal", "Cmd+H", false, has_spr))
                sprites[current_sprite].flip_h();
            if (ImGui::MenuItem("Flip Vertical", "Cmd+J", false, has_spr))
                sprites[current_sprite].flip_v();
            ImGui::Separator();
            if (ImGui::MenuItem("Move Up", "Cmd+Up", false, has_spr))
                sprites[current_sprite].move_up();
            if (ImGui::MenuItem("Move Down", "Cmd+Down", false, has_spr))
                sprites[current_sprite].move_down();
            if (ImGui::MenuItem("Move Left", "Cmd+Left", false, has_spr))
                sprites[current_sprite].move_left();
            if (ImGui::MenuItem("Move Right", "Cmd+Right", false, has_spr))
                sprites[current_sprite].move_right();
            ImGui::Separator();
            if (ImGui::MenuItem("Clear Sprite", "Cmd+Delete", false, has_spr))
                sprites[current_sprite].clear();
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    // Keyboard shortcuts
    ImGuiIO &io = ImGui::GetIO();
    bool has_spr = current_sprite < (int)sprites.size();
    if (io.KeySuper) {
        if (ImGui::IsKeyPressed(ImGuiKey_N)) new_project();
        if (ImGui::IsKeyPressed(ImGuiKey_O)) open_project();
        if (ImGui::IsKeyPressed(ImGuiKey_S)) {
            if (io.KeyShift) save_project_as();
            else save_project();
        }
        if (ImGui::IsKeyPressed(ImGuiKey_E)) export_c();
        if (has_spr) {
            if (ImGui::IsKeyPressed(ImGuiKey_C)) { clipboard = sprites[current_sprite]; has_clipboard = true; }
            if (ImGui::IsKeyPressed(ImGuiKey_V) && has_clipboard) {
                sprites[current_sprite].pixels = clipboard.pixels;
                sprites[current_sprite].resize(clipboard.width, clipboard.height);
            }
            if (ImGui::IsKeyPressed(ImGuiKey_H)) sprites[current_sprite].flip_h();
            if (ImGui::IsKeyPressed(ImGuiKey_J)) sprites[current_sprite].flip_v();
            if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) sprites[current_sprite].move_up();
            if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) sprites[current_sprite].move_down();
            if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) sprites[current_sprite].move_left();
            if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) sprites[current_sprite].move_right();
            if (ImGui::IsKeyPressed(ImGuiKey_Delete)) sprites[current_sprite].clear();
        }
    }
}

// ============================================================
// Sprite Editor
// ============================================================

void App::draw_sprite_editor() {
    // Sprite list panel
    ImGui::Begin("Sprites");
    draw_sprite_list();
    ImGui::End();

    // Main canvas
    ImGui::Begin("Canvas");
    draw_sprite_canvas();
    ImGui::End();

    // Palette + preview
    ImGui::Begin("Tools");
    draw_palette();
    ImGui::Separator();
    draw_sprite_preview();
    ImGui::Separator();
    draw_sprite_properties();
    ImGui::End();
}

void App::draw_sprite_list() {
    if (ImGui::Button("+")) {
        char name[32];
        snprintf(name, sizeof(name), "sprite_%d", (int)sprites.size());
        sprites.push_back(Sprite(name, 10, 20));
        current_sprite = (int)sprites.size() - 1;
    }
    ImGui::SameLine();
    if (ImGui::Button("-") && sprites.size() > 1) {
        sprites.erase(sprites.begin() + current_sprite);
        if (current_sprite >= (int)sprites.size())
            current_sprite = (int)sprites.size() - 1;
    }
    ImGui::SameLine();
    if (ImGui::Button("Dup") && current_sprite < (int)sprites.size()) {
        Sprite copy = sprites[current_sprite];
        copy.name += "_copy";
        sprites.push_back(copy);
        current_sprite = (int)sprites.size() - 1;
    }

    ImGui::Separator();

    for (int i = 0; i < (int)sprites.size(); i++) {
        char label[64];
        snprintf(label, sizeof(label), "%s (%dx%d)",
                 sprites[i].name.c_str(), sprites[i].width, sprites[i].height);
        if (ImGui::Selectable(label, current_sprite == i)) {
            current_sprite = i;
            animate = false;
        }
    }
}

void App::draw_palette() {
    ImGui::Text("Palette");
    for (int i = 0; i < 8; i++) {
        uint32_t c = QL_COLORS[i];
        ImVec4 col(
            ((c >> 24) & 0xFF) / 255.0f,
            ((c >> 16) & 0xFF) / 255.0f,
            ((c >> 8) & 0xFF) / 255.0f,
            1.0f
        );
        char id[16];
        snprintf(id, sizeof(id), "##col%d", i);
        if (ImGui::ColorButton(id, col, ImGuiColorEditFlags_NoTooltip, ImVec2(24, 24))) {
            current_color = i;
        }
        if (current_color == i) {
            ImGui::SameLine();
            ImGui::Text("<");
        }
        ImGui::SameLine();
        ImGui::Text("%d: %s", i, QL_COLOR_NAMES[i]);
    }

    // Number key shortcuts for palette
    for (int i = 0; i < 8; i++) {
        if (ImGui::IsKeyPressed((ImGuiKey)(ImGuiKey_0 + i)))
            current_color = i;
    }
}

void App::draw_sprite_canvas() {
    if (current_sprite >= (int)sprites.size()) return;
    Sprite &spr = sprites[current_sprite];

    ImVec2 avail = ImGui::GetContentRegionAvail();
    float cell = std::min(avail.x / spr.width, avail.y / spr.height);
    cell = std::max(2.0f, std::min(cell, 32.0f));

    float ox = (avail.x - spr.width * cell) * 0.5f;
    float oy = (avail.y - spr.height * cell) * 0.5f;
    ImVec2 p0 = ImGui::GetCursorScreenPos();
    p0.x += ox;
    p0.y += oy;

    ImDrawList *dl = ImGui::GetWindowDrawList();

    // Draw pixels
    for (int y = 0; y < spr.height; y++) {
        for (int x = 0; x < spr.width; x++) {
            uint8_t c = spr.pixels[y][x];
            uint32_t rgba = QL_COLORS[c];
            ImU32 col = IM_COL32((rgba>>24)&0xFF, (rgba>>16)&0xFF, (rgba>>8)&0xFF, 0xFF);
            ImVec2 a(p0.x + x * cell, p0.y + y * cell);
            ImVec2 b(a.x + cell, a.y + cell);
            dl->AddRectFilled(a, b, col);
            dl->AddRect(a, b, IM_COL32(50, 50, 50, 255));
        }
    }

    // Byte boundary lines (every 2 pixels)
    for (int x = 0; x <= spr.width; x += 2) {
        float lx = p0.x + x * cell;
        dl->AddLine(ImVec2(lx, p0.y), ImVec2(lx, p0.y + spr.height * cell),
                    IM_COL32(80, 80, 80, 255));
    }

    // Handle mouse input on canvas
    ImGui::SetCursorScreenPos(ImVec2(p0.x, p0.y));
    ImGui::InvisibleButton("canvas", ImVec2(spr.width * cell, spr.height * cell));

    if (ImGui::IsItemHovered()) {
        ImVec2 mouse = ImGui::GetMousePos();
        int mx = (int)((mouse.x - p0.x) / cell);
        int my = (int)((mouse.y - p0.y) / cell);
        if (mx >= 0 && mx < spr.width && my >= 0 && my < spr.height) {
            // Left click: paint
            if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                spr.pixels[my][mx] = current_color;
                modified = true;
            }
            // Right click: pick color
            if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
                current_color = spr.pixels[my][mx];
            }
            // Tooltip
            ImGui::SetTooltip("(%d, %d) color: %d (%s)", mx, my,
                             spr.pixels[my][mx], QL_COLOR_NAMES[spr.pixels[my][mx]]);
        }
    }
}

void App::draw_sprite_preview() {
    if (current_sprite >= (int)sprites.size()) return;

    ImGui::Checkbox("Animate", &animate);

    int show_idx = current_sprite;
    if (animate) {
        anim_timer += ImGui::GetIO().DeltaTime;
        if (anim_timer > 0.2f) {
            anim_timer = 0;
            anim_frame++;
        }
        // Find animation group
        std::string base = sprites[current_sprite].name;
        auto pos = base.rfind('_');
        if (pos != std::string::npos) {
            std::string suffix = base.substr(pos + 1);
            bool is_num = !suffix.empty() && std::all_of(suffix.begin(), suffix.end(), ::isdigit);
            if (is_num) {
                std::string prefix = base.substr(0, pos + 1);
                std::vector<int> group;
                for (int i = 0; i < (int)sprites.size(); i++) {
                    if (sprites[i].name.find(prefix) == 0) {
                        std::string s = sprites[i].name.substr(prefix.size());
                        if (!s.empty() && std::all_of(s.begin(), s.end(), ::isdigit))
                            group.push_back(i);
                    }
                }
                if (!group.empty())
                    show_idx = group[anim_frame % group.size()];
            }
        }
    }

    Sprite &spr = sprites[show_idx];
    int scale = 3;
    int pw = spr.width * scale;
    int ph = spr.height * scale;

    // Draw preview with ImGui DrawList
    ImVec2 p0 = ImGui::GetCursorScreenPos();
    ImDrawList *dl = ImGui::GetWindowDrawList();

    // Black background
    dl->AddRectFilled(p0, ImVec2(p0.x + pw + 4, p0.y + ph + 4), IM_COL32(0, 0, 0, 255));

    for (int y = 0; y < spr.height; y++) {
        for (int x = 0; x < spr.width; x++) {
            uint8_t c = spr.pixels[y][x];
            if (c == 0) continue;
            uint32_t rgba = QL_COLORS[c];
            ImU32 col = IM_COL32((rgba>>24)&0xFF, (rgba>>16)&0xFF, (rgba>>8)&0xFF, 0xFF);
            ImVec2 a(p0.x + 2 + x * scale, p0.y + 2 + y * scale);
            ImVec2 b(a.x + scale, a.y + scale);
            dl->AddRectFilled(a, b, col);
        }
    }

    ImGui::Dummy(ImVec2((float)(pw + 4), (float)(ph + 4)));

    // Size info
    int bytes = (spr.width / 2) * spr.height;
    ImGui::Text("Size: %d bytes", bytes);
}

void App::draw_sprite_properties() {
    if (current_sprite >= (int)sprites.size()) return;
    Sprite &spr = sprites[current_sprite];

    ImGui::Text("Properties");

    static char name_buf[64] = {};
    strncpy(name_buf, spr.name.c_str(), sizeof(name_buf) - 1);
    if (ImGui::InputText("Name", name_buf, sizeof(name_buf),
                         ImGuiInputTextFlags_EnterReturnsTrue)) {
        spr.name = name_buf;
    }

    int w = spr.width, h = spr.height;
    if (ImGui::InputInt("Width", &w, 2)) {
        if (w < 2) w = 2;
        if (w & 1) w++;
        spr.resize(w, spr.height);
    }
    if (ImGui::InputInt("Height", &h, 1)) {
        if (h < 1) h = 1;
        spr.resize(spr.width, h);
    }
}

// ============================================================
// Emulator
// ============================================================

#include "emulator.h"

// Virtual key codes from iQL (matching rb_virtual_keys.h enum)
enum {
    VK_A=0,VK_B,VK_C,VK_D,VK_E,VK_F,VK_G,VK_H,VK_I,VK_J,VK_K,VK_L,VK_M,
    VK_N,VK_O,VK_P,VK_Q,VK_R,VK_S,VK_T,VK_U,VK_V,VK_W,VK_X,VK_Y,VK_Z,
    VK_0,VK_1,VK_2,VK_3,VK_4,VK_5,VK_6,VK_7,VK_8,VK_9,
    VK_RETURN=52, VK_SPACE=53, VK_ESCAPE=54, VK_TAB=55, VK_BACKSPACE=56,
    VK_UP=57, VK_DOWN=58, VK_LEFT=59, VK_RIGHT=60,
    VK_LSHIFT=61, VK_RSHIFT=62, VK_LCONTROL=63, VK_RCONTROL=64,
    VK_LALT=65, VK_RALT=66,
};

static int sdl_key_to_vk(SDL_Keycode k) {
    if (k >= SDLK_a && k <= SDLK_z) return VK_A + (k - SDLK_a);
    if (k >= SDLK_0 && k <= SDLK_9) return VK_0 + (k - SDLK_0);
    switch (k) {
        case SDLK_RETURN: return VK_RETURN;
        case SDLK_SPACE: return VK_SPACE;
        case SDLK_ESCAPE: return VK_ESCAPE;
        case SDLK_TAB: return VK_TAB;
        case SDLK_BACKSPACE: return VK_BACKSPACE;
        case SDLK_UP: return VK_UP;
        case SDLK_DOWN: return VK_DOWN;
        case SDLK_LEFT: return VK_LEFT;
        case SDLK_RIGHT: return VK_RIGHT;
        case SDLK_LSHIFT: return VK_LSHIFT;
        case SDLK_RSHIFT: return VK_RSHIFT;
        case SDLK_LCTRL: return VK_LCONTROL;
        case SDLK_RCTRL: return VK_RCONTROL;
        case SDLK_LALT: return VK_LALT;
        case SDLK_RALT: return VK_RALT;
        default: return -1;
    }
}

bool emu_wants_keys = false;  // read by main.cpp for keyboard forwarding
static int speed_idx = 1;
static const char *speed_names[] = {"Fast", "Normal", "Slow", "Very Slow"};
static int speed_values[] = {0, 1, 4, 10};
static bool bp_enabled = true;
static bool trap_log_enabled = false;

void App::draw_emulator() {
    ImGui::Begin("Emulator");

    // Toolbar
    if (!g_emu.is_running()) {
        if (ImGui::Button("Build & Run")) {
            // ql/ directory is two levels up from studio/
            std::string ql_dir = std::string(SDL_GetBasePath()) + "../../";

            // Export sprites to ql/ directory
            write_c_files(last_export_dir.empty() ? ql_dir : last_export_dir);
            log("Sprites exported to %s", ql_dir.c_str());
            std::string output;
            log("Building...");
            if (g_emu.build(ql_dir, output)) {
                log("Build OK");
                // Start emulator
                std::string sys_path = std::string(getenv("HOME")) + "/Documents/iQLmac/";
                if (g_emu.start(sys_path.c_str())) {
                    log("Emulator started");
                    g_emu.set_log_callback([this](const char *msg) { log("%s", msg); });
                    // Enable soft BP after delay (QDOS boot)
                    // For now, enable immediately — user can toggle
                }
            } else {
                log("ERR Build FAILED:\n%s", output.c_str());
            }
            if (!output.empty()) log("%s", output.c_str());
        }
    } else {
        if (ImGui::Button(g_emu.is_paused() ? "Resume" : "Pause")) {
            if (g_emu.is_paused()) g_emu.resume();
            else g_emu.pause();
        }
        ImGui::SameLine();
        if (ImGui::Button("Stop")) g_emu.stop();
        ImGui::SameLine();
        if (ImGui::Button(speed_names[speed_idx])) {
            speed_idx = (speed_idx + 1) % 4;
            g_emu.set_speed(speed_values[speed_idx]);
        }
        ImGui::SameLine();
        if (ImGui::Checkbox("BPs", &bp_enabled))
            g_emu.set_soft_bp_enabled(bp_enabled);
        ImGui::SameLine();
        if (ImGui::Checkbox("Traps", &trap_log_enabled))
            g_emu.set_trap_logging(trap_log_enabled);
    }

    // Display emulator screen as texture
    if (g_emu.is_running()) {
        g_emu.update_texture();
        GLuint tex = g_emu.get_texture();
        if (tex) {
            ImVec2 avail = ImGui::GetContentRegionAvail();
            int sw = g_emu.get_screen_width();
            int sh = g_emu.get_screen_height();
            if (sw > 0 && sh > 0) {
                float scale = std::min(avail.x / sw, avail.y / sh);
                float dw = sw * scale, dh = sh * scale;
                ImVec2 pos = ImGui::GetCursorScreenPos();
                float ox = (avail.x - dw) * 0.5f;
                ImGui::SetCursorScreenPos(ImVec2(pos.x + ox, pos.y));
                ImGui::Image((ImTextureID)(intptr_t)tex, ImVec2(dw, dh));

                // Keyboard input when emulator image is hovered
                emu_wants_keys = ImGui::IsItemHovered();
            }
        }

        // Drain trap log to console
        if (trap_log_enabled) {
            std::string tl = g_emu.drain_trap_log();
            if (!tl.empty()) log("%s", tl.c_str());
        }
    }

    ImGui::End();

    // Keyboard forwarding handled in main.cpp via SDL events
}

// ============================================================
// Debug Panels
// ============================================================

static const char *sr_flags(uint16_t sr) {
    static char buf[16];
    snprintf(buf, sizeof(buf), "%c%c%d%c%c%c%c%c",
        sr & 0x8000 ? 'T' : '-', sr & 0x2000 ? 'S' : '-',
        (sr >> 8) & 7,
        sr & 0x10 ? 'X' : '-', sr & 0x08 ? 'N' : '-',
        sr & 0x04 ? 'Z' : '-', sr & 0x02 ? 'V' : '-',
        sr & 0x01 ? 'C' : '-');
    return buf;
}

void App::draw_debug_panels() {
    // CPU State
    ImGui::Begin("CPU State");
    if (g_emu.is_running()) {
        auto cpu = g_emu.get_cpu_state();
        ImGui::TextColored(ImVec4(0.3f,0.8f,0.7f,1), "PC: $%06X  SR: $%04X", cpu.pc, cpu.sr);
        ImGui::Text("Flags: %s", sr_flags(cpu.sr));
        ImGui::Separator();
        for (int i = 0; i < 4; i++)
            ImGui::Text("D%d: %08X  D%d: %08X", i, cpu.d[i], i+4, cpu.d[i+4]);
        ImGui::Separator();
        for (int i = 0; i < 4; i++)
            ImGui::Text("A%d: %08X  A%d: %08X", i, cpu.a[i], i+4, cpu.a[i+4]);
        ImGui::Separator();
        ImGui::Text("USP: %08X  SSP: %08X", cpu.usp, cpu.ssp);

        // Step buttons
        ImGui::Separator();
        if (ImGui::Button("Step 1")) { g_emu.pause(); g_emu.step(1); }
        ImGui::SameLine();
        if (ImGui::Button("Step 10")) { g_emu.pause(); g_emu.step(10); }
        ImGui::SameLine();
        if (ImGui::Button("Step 100")) { g_emu.pause(); g_emu.step(100); }
    } else {
        ImGui::TextColored(ImVec4(0.5f,0.5f,0.5f,1), "Emulator not running.");
    }
    ImGui::End();

    // Memory viewer
    ImGui::Begin("Memory");
    if (g_emu.is_running()) {
        static char addr_buf[16] = "20000";
        static int rows = 16;
        ImGui::SetNextItemWidth(80);
        ImGui::InputText("Addr", addr_buf, sizeof(addr_buf));
        ImGui::SameLine();
        ImGui::SetNextItemWidth(50);
        ImGui::InputInt("Rows", &rows, 4);
        if (rows < 1) rows = 1;
        if (rows > 64) rows = 64;

        uint32_t addr = (uint32_t)strtoul(addr_buf, NULL, 16);
        ImGui::Separator();
        for (int r = 0; r < rows; r++) {
            uint32_t ra = addr + r * 16;
            ImGui::TextColored(ImVec4(0.4f,0.6f,0.9f,1), "%06X:", ra);
            ImGui::SameLine();
            uint8_t data[16];
            g_emu.read_mem(ra, data, 16);
            char hex[64] = {}, ascii[20] = {};
            int hp = 0;
            for (int i = 0; i < 16; i++) {
                hp += snprintf(hex + hp, sizeof(hex) - hp, "%02X ", data[i]);
                ascii[i] = (data[i] >= 32 && data[i] < 127) ? data[i] : '.';
            }
            ascii[16] = 0;
            ImGui::Text("%s %s", hex, ascii);
        }
    } else {
        ImGui::TextColored(ImVec4(0.5f,0.5f,0.5f,1), "Emulator not running.");
    }
    ImGui::End();

    // Console log panel
    ImGui::Begin("Console");
    if (console_log.empty()) {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Console output appears here.");
    } else {
        for (auto &line : console_log) {
            if (line.find("ERR") != std::string::npos)
                ImGui::TextColored(ImVec4(0.95f, 0.3f, 0.3f, 1.0f), "%s", line.c_str());
            else if (line.find("***") != std::string::npos)
                ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "%s", line.c_str());
            else
                ImGui::TextUnformatted(line.c_str());
        }
        if (console_scroll_to_bottom) {
            ImGui::SetScrollHereY(1.0f);
            console_scroll_to_bottom = false;
        }
    }
    ImGui::End();
}

// ============================================================
// File I/O
// ============================================================

void App::new_project() {
    sprites.clear();
    sprites.push_back(Sprite("sprite_0", 10, 20));
    current_sprite = 0;
    project_path.clear();
    modified = false;
}

void App::open_project() {
    const char *path = simple_file_dialog("Open Project", "");
    if (path) load_project(path);
}

void App::save_project(const char *path) {
    if (path) project_path = path;
    if (project_path.empty()) { save_project_as(); return; }

    // Sort sprites alphabetically before saving
    std::sort(sprites.begin(), sprites.end(),
              [](const Sprite &a, const Sprite &b) { return a.name < b.name; });

    json_save_project(project_path.c_str(), sprites);
    modified = false;
    log("Project saved: %s (%d sprites)", project_path.c_str(), (int)sprites.size());
}

void App::save_project_as() {
    const char *path = simple_file_dialog("Save Project", "sprites.json");
    if (path) save_project(path);
}

void App::load_project(const char *path) {
    sprites.clear();
    json_load_project(path, sprites);
    project_path = path;
    current_sprite = 0;
    modified = false;
    log("Loaded: %s (%d sprites)", path, (int)sprites.size());
}

void App::export_c(const char *directory) {
    std::string dir;
    if (directory) {
        dir = directory;
    } else {
        const char *path = simple_file_dialog("Export", last_export_dir.c_str());
        if (!path) return;
        dir = path;
    }
    last_export_dir = dir;
    write_c_files(dir);
    log("Exported %d sprites to %s", (int)sprites.size(), dir.c_str());
}

void App::write_c_files(const std::string &dir) {
    std::string c_path = dir + "/sprites.c";
    std::string h_path = dir + "/sprites.h";

    FILE *fc = fopen(c_path.c_str(), "w");
    FILE *fh = fopen(h_path.c_str(), "w");
    if (!fc || !fh) return;

    fprintf(fc, "/* sprites.c — Generated by QL Studio */\n");
    fprintf(fc, "#include \"sprites.h\"\n\n");

    fprintf(fh, "/* sprites.h — Generated by QL Studio */\n");
    fprintf(fh, "#ifndef SPRITES_H\n#define SPRITES_H\n\n");
    fprintf(fh, "#include \"game.h\"\n\n");
    fprintf(fh, "typedef struct { uint8_t w; uint8_t h; const uint8_t *data; } Sprite;\n\n");

    for (auto &spr : sprites) {
        int wb = spr.width / 2;
        fprintf(fc, "/* %s — %dx%d */\n", spr.name.c_str(), spr.width, spr.height);
        fprintf(fc, "static const uint8_t spr_%s_data[] = {\n", spr.name.c_str());
        for (int y = 0; y < spr.height; y++) {
            fprintf(fc, "    ");
            for (int x = 0; x < spr.width; x += 2) {
                uint8_t hi = spr.pixels[y][x];
                uint8_t lo = (x + 1 < spr.width) ? spr.pixels[y][x + 1] : 0;
                fprintf(fc, "0x%02X, ", (hi << 4) | lo);
            }
            fprintf(fc, "\n");
        }
        fprintf(fc, "};\n");
        fprintf(fc, "const Sprite spr_%s = { %d, %d, spr_%s_data };\n\n",
                spr.name.c_str(), spr.width, spr.height, spr.name.c_str());

        fprintf(fh, "extern const Sprite spr_%s;\n", spr.name.c_str());
    }

    fprintf(fh, "\n#endif\n");
    fclose(fc);
    fclose(fh);
}
