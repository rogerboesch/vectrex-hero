/*
 * app.cpp — QL Studio application implementation
 */

#include "app.h"
#include "../imgui/imgui.h"
// Minimal file dialog stubs (replace with native dialogs later)
static const char *simple_file_dialog(const char *title, const char *default_path) {
    // For now, return NULL — use command line args or menu shortcuts
    (void)title; (void)default_path;
    return NULL;
}
#include <cstdio>
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
            if (ImGui::MenuItem("Flip Horizontal", "Cmd+H")) {
                if (current_sprite < (int)sprites.size())
                    sprites[current_sprite].flip_h();
            }
            if (ImGui::MenuItem("Flip Vertical", "Cmd+J")) {
                if (current_sprite < (int)sprites.size())
                    sprites[current_sprite].flip_v();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Clear Sprite")) {
                if (current_sprite < (int)sprites.size())
                    sprites[current_sprite].clear();
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    // Keyboard shortcuts
    ImGuiIO &io = ImGui::GetIO();
    if (io.KeySuper) {
        if (ImGui::IsKeyPressed(ImGuiKey_N)) new_project();
        if (ImGui::IsKeyPressed(ImGuiKey_O)) open_project();
        if (ImGui::IsKeyPressed(ImGuiKey_S)) {
            if (io.KeyShift) save_project_as();
            else save_project();
        }
        if (ImGui::IsKeyPressed(ImGuiKey_E)) export_c();
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
// Emulator + Debug (stubs — to be filled when iQL is integrated)
// ============================================================

void App::draw_emulator() {
    ImGui::Begin("Emulator");
    ImGui::Text("iQL emulator integration coming soon.");
    ImGui::Text("Build & Run, keyboard input, screen display.");
    ImGui::End();
}

void App::draw_debug_panels() {
    ImGui::Begin("CPU State");
    ImGui::Text("Register display coming soon.");
    ImGui::End();

    ImGui::Begin("Memory");
    ImGui::Text("Hex viewer coming soon.");
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
