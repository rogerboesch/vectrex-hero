/*
 * app.cpp — QL Studio application implementation
 */

#include "app.h"
#include "../thirdparty/imgui/imgui.h"
#include "emulator.h"
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <fstream>
#include <sstream>

// Simple JSON helpers (minimal, no external lib)
#include "json_io.h"

// ============================================================
// macOS native file dialogs via osascript
// ============================================================

static std::string run_osascript(const char *cmd) {
    FILE *fp = popen(cmd, "r");
    if (!fp) return "";
    char buf[1024];
    std::string result;
    while (fgets(buf, sizeof(buf), fp))
        result += buf;
    pclose(fp);
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r'))
        result.pop_back();
    return result;
}

std::string App::dialog_open(const char *title) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
        "osascript -e 'POSIX path of (choose file with prompt \"%s\")' 2>/dev/null", title);
    return run_osascript(cmd);
}

std::string App::dialog_save(const char *title, const char *default_name) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
        "osascript -e 'POSIX path of (choose file name with prompt \"%s\" default name \"%s\")' 2>/dev/null",
        title, default_name);
    return run_osascript(cmd);
}

std::string App::dialog_folder(const char *title) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
        "osascript -e 'POSIX path of (choose folder with prompt \"%s\")' 2>/dev/null", title);
    return run_osascript(cmd);
}

// ============================================================
// App lifecycle
// ============================================================

App::App() {
    sprites.push_back(Sprite("sprite_0", 10, 20));
    glGenTextures(1, &sprite_texture);
    glGenTextures(1, &preview_texture);
    glGenTextures(1, &image_texture);
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
    if (image_texture) glDeleteTextures(1, &image_texture);
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
            if (ImGui::MenuItem("Screenshot...", "Cmd+P")) screenshot();
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
        if (ImGui::IsKeyPressed(ImGuiKey_P)) screenshot();
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
// Image Editor
// ============================================================

void App::update_image_texture() {
    if (current_image >= (int)images.size()) return;
    QLImage &img = images[current_image];

    uint8_t buf[IMAGE_SIZE * IMAGE_SIZE * 4];
    for (int y = 0; y < IMAGE_SIZE; y++) {
        for (int x = 0; x < IMAGE_SIZE; x++) {
            uint8_t c = img.pixels[y][x];
            uint32_t rgba = QL_COLORS[c];
            int i = (y * IMAGE_SIZE + x) * 4;
            buf[i+0] = (rgba >> 24) & 0xFF;
            buf[i+1] = (rgba >> 16) & 0xFF;
            buf[i+2] = (rgba >> 8) & 0xFF;
            buf[i+3] = 0xFF;
        }
    }

    glBindTexture(GL_TEXTURE_2D, image_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, IMAGE_SIZE, IMAGE_SIZE, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, buf);
    image_texture_dirty = false;
}

void App::draw_image_editor() {
    // Image list panel
    ImGui::Begin("Images");
    draw_image_list();
    ImGui::Separator();
    draw_image_tools();
    ImGui::End();

    // Image canvas
    ImGui::Begin("Image Canvas");
    draw_image_canvas();
    ImGui::End();
}

void App::draw_image_list() {
    if (ImGui::Button("+##img")) {
        char name[32];
        snprintf(name, sizeof(name), "image_%d", (int)images.size());
        images.push_back(QLImage(name));
        current_image = (int)images.size() - 1;
        image_texture_dirty = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("-##img") && images.size() > 0) {
        images.erase(images.begin() + current_image);
        if (current_image >= (int)images.size())
            current_image = std::max(0, (int)images.size() - 1);
        image_texture_dirty = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Dup##img") && current_image < (int)images.size()) {
        QLImage copy = images[current_image];
        copy.name += "_copy";
        images.push_back(copy);
        current_image = (int)images.size() - 1;
        image_texture_dirty = true;
    }

    ImGui::Separator();

    for (int i = 0; i < (int)images.size(); i++) {
        if (ImGui::Selectable(images[i].name.c_str(), current_image == i)) {
            current_image = i;
            image_texture_dirty = true;
        }
    }
}

void App::draw_image_tools() {
    if (current_image >= (int)images.size()) return;
    QLImage &img = images[current_image];

    // Name
    static char img_name_buf[64] = {};
    strncpy(img_name_buf, img.name.c_str(), sizeof(img_name_buf) - 1);
    if (ImGui::InputText("Name##img", img_name_buf, sizeof(img_name_buf),
                         ImGuiInputTextFlags_EnterReturnsTrue)) {
        img.name = img_name_buf;
    }

    // Operations
    ImGui::Text("Operations");
    if (ImGui::Button("Clear##img")) { img.clear(); image_texture_dirty = true; }
    ImGui::SameLine();
    if (ImGui::Button("Flip H##img")) { img.flip_h(); image_texture_dirty = true; }
    ImGui::SameLine();
    if (ImGui::Button("Flip V##img")) { img.flip_v(); image_texture_dirty = true; }

    if (ImGui::Button("U##img")) { img.move_up(); image_texture_dirty = true; }
    ImGui::SameLine();
    if (ImGui::Button("D##img")) { img.move_down(); image_texture_dirty = true; }
    ImGui::SameLine();
    if (ImGui::Button("L##img")) { img.move_left(); image_texture_dirty = true; }
    ImGui::SameLine();
    if (ImGui::Button("R##img")) { img.move_right(); image_texture_dirty = true; }

    // Zoom
    ImGui::Separator();
    ImGui::Text("Zoom");
    if (ImGui::RadioButton("1x", image_zoom == 1)) image_zoom = 1;
    ImGui::SameLine();
    if (ImGui::RadioButton("2x", image_zoom == 2)) image_zoom = 2;
    ImGui::SameLine();
    if (ImGui::RadioButton("4x", image_zoom == 4)) image_zoom = 4;

    // Size info
    ImGui::Text("256x256 (32KB)");

    // Export
    ImGui::Separator();
    if (ImGui::Button("Export Image C Files")) {
        std::string dir = dialog_folder("Export images to directory");
        if (!dir.empty()) {
            write_image_c_files(dir);
            log("Exported %d images to %s", (int)images.size(), dir.c_str());
        }
    }
}

void App::draw_image_canvas() {
    if (current_image >= (int)images.size()) {
        ImGui::TextColored(ImVec4(0.5f,0.5f,0.5f,1), "No images. Add one in the Images panel.");
        return;
    }

    if (image_texture_dirty) update_image_texture();

    float display_size = (float)(IMAGE_SIZE * image_zoom);

    // Scrollable child for large zoom levels
    ImGui::BeginChild("ImageScroll", ImVec2(0, 0), false,
                      ImGuiWindowFlags_HorizontalScrollbar);

    ImVec2 p0 = ImGui::GetCursorScreenPos();
    ImGui::Image((ImTextureID)(intptr_t)image_texture, ImVec2(display_size, display_size));

    // Mouse drawing
    if (ImGui::IsItemHovered()) {
        ImVec2 mouse = ImGui::GetMousePos();
        int px = (int)((mouse.x - p0.x) / image_zoom);
        int py = (int)((mouse.y - p0.y) / image_zoom);
        if (px >= 0 && px < IMAGE_SIZE && py >= 0 && py < IMAGE_SIZE) {
            QLImage &img = images[current_image];
            if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                img.pixels[py][px] = current_color;
                image_texture_dirty = true;
                modified = true;
            }
            if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
                current_color = img.pixels[py][px];
            }
            ImGui::SetTooltip("(%d, %d) color: %d (%s)", px, py,
                             img.pixels[py][px], QL_COLOR_NAMES[img.pixels[py][px]]);
        }
    }

    ImGui::EndChild();
}

void App::write_image_c_files(const std::string &dir) {
    std::string c_path = dir + "/images.c";
    std::string h_path = dir + "/images.h";

    FILE *fc = fopen(c_path.c_str(), "w");
    FILE *fh = fopen(h_path.c_str(), "w");
    if (!fc || !fh) { if (fc) fclose(fc); if (fh) fclose(fh); return; }

    fprintf(fc, "/* images.c — Image data for QL Mode 8 */\n");
    fprintf(fc, "/* Generated by QL Studio */\n\n");
    fprintf(fc, "#include \"images.h\"\n\n");

    fprintf(fh, "/* images.h — Image declarations for QL Mode 8 */\n");
    fprintf(fh, "/* Generated by QL Studio */\n\n");
    fprintf(fh, "#ifndef IMAGES_H\n#define IMAGES_H\n\n");
    fprintf(fh, "#include <stdint.h>\n\n");

    for (auto &img : images) {
        std::string cname = img.name;
        std::vector<uint8_t> mode8 = img.to_mode8();
        fprintf(fc, "const uint8_t img_%s[%d] = {\n", cname.c_str(), MODE8_BYTES_PER_IMAGE);
        for (int i = 0; i < (int)mode8.size(); i += 16) {
            fprintf(fc, "    ");
            int end = std::min(i + 16, (int)mode8.size());
            for (int j = i; j < end; j++) {
                fprintf(fc, "0x%02X", mode8[j]);
                if (j < (int)mode8.size() - 1) fprintf(fc, ", ");
            }
            fprintf(fc, "\n");
        }
        fprintf(fc, "};\n\n");
        fprintf(fh, "extern const uint8_t img_%s[%d];\n", cname.c_str(), MODE8_BYTES_PER_IMAGE);
    }

    fprintf(fh, "\n#endif\n");
    fclose(fc);
    fclose(fh);
}

// ============================================================
// Emulator
// ============================================================

bool emu_wants_keys = false;  // read by main.cpp for keyboard forwarding
static int speed_idx = 1;
static const char *speed_names[] = {"Fast", "Normal", "Slow", "Very Slow"};
static int speed_values[] = {0, 1, 4, 10};
static bool bp_enabled = true;
static bool trap_log_enabled = false;
void App::draw_emulator() {
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.12f, 0.12f, 0.12f, 1.0f));
    ImGui::Begin("Emulator");

    // Toolbar — Build is always available
    if (ImGui::Button("Build")) {
        std::string ql_dir = std::string(SDL_GetBasePath()) + "../../";
        write_c_files(last_export_dir.empty() ? ql_dir : last_export_dir);
        log("Sprites exported to %s", ql_dir.c_str());
        std::string output;
        log("Building...");
        if (g_emu.build(ql_dir, output))
            log("Build OK");
        else
            log("ERR Build FAILED:\n%s", output.c_str());
        if (!output.empty()) log("%s", output.c_str());
    }
    ImGui::SameLine();

    if (!g_emu.is_running()) {
        if (ImGui::Button("Run")) {
            std::string sys_path = std::string(getenv("HOME")) + "/Documents/iQLmac/";
            if (g_emu.start(sys_path.c_str())) {
                log("Emulator started");
                g_emu.set_log_callback([this](const char *msg) { log("%s", msg); });
                emu_start_ticks = SDL_GetTicks();
                soft_bp_armed = false;
            }
        }
    } else {
        if (ImGui::Button(g_emu.is_paused() ? "Resume" : "Pause")) {
            if (g_emu.is_paused()) g_emu.resume();
            else g_emu.pause();
        }
        ImGui::SameLine();
        if (ImGui::Button("Stop")) { g_emu.stop(); emu_wants_keys = false; }
        ImGui::SameLine();

        // Restart with confirmation popup
        if (ImGui::Button("Restart")) ImGui::OpenPopup("Restart?");
        if (ImGui::BeginPopupModal("Restart?", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Restart the emulator?");
            if (ImGui::Button("Yes", ImVec2(80, 0))) {
                g_emu.restart();
                emu_start_ticks = SDL_GetTicks();
                soft_bp_armed = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("No", ImVec2(80, 0))) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
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

        // Status label
        ImGui::SameLine();
        if (g_emu.is_paused())
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Paused");
        else
            ImGui::TextColored(ImVec4(0.3f, 0.8f, 0.3f, 1.0f), "Running");
    }

    // Boot delay for software breakpoints
    if (g_emu.is_running() && !soft_bp_armed && bp_enabled &&
        SDL_GetTicks() - emu_start_ticks > 3000) {
        g_emu.set_soft_bp_enabled(true);
        soft_bp_armed = true;
        log("Software breakpoints enabled (after boot delay)");
    }

    // Display area — always show screen rectangle
    ImVec2 screen_pos;
    float pw = 512, ph = 256, scale = 1.0f;
    {
        ImVec2 avail = ImGui::GetContentRegionAvail();
        float dw = 512, dh = 256;

        if (g_emu.is_running())
            g_emu.update_texture();

        if (g_emu.is_ready()) {
            int sw = g_emu.get_screen_width();
            int sh = g_emu.get_screen_height();
            if (sw > 0 && sh > 0) { dw = (float)sw; dh = (float)sh; }
        }

        scale = std::min(avail.x / dw, (avail.y - 40) / dh);
        if (scale < 0.1f) scale = 1.0f;
        pw = dw * scale;
        ph = dh * scale;
        ImVec2 pos = ImGui::GetCursorScreenPos();
        float ox = (avail.x - pw) * 0.5f;
        screen_pos = ImVec2(pos.x + ox, pos.y);

        GLuint tex = g_emu.get_texture();
        if (g_emu.is_ready() && tex) {
            ImGui::SetCursorScreenPos(screen_pos);
            ImGui::Image((ImTextureID)(intptr_t)tex, ImVec2(pw, ph));

            // Pixel inspector on hover
            if (ImGui::IsItemHovered()) {
                ImVec2 mouse = ImGui::GetMousePos();
                int qx = (int)((mouse.x - screen_pos.x) / scale);
                int qy = (int)((mouse.y - screen_pos.y) / scale);
                // iQL screen is 512x256, Mode 8 pixels are 2px wide
                int ql_x = qx / 2;
                int ql_y = qy;
                if (ql_x >= 0 && ql_x < 256 && ql_y >= 0 && ql_y < 256) {
                    uint32_t pair_addr = 0x20000 + ql_y * 128 + (ql_x / 4) * 2;
                    int pos_in_pair = ql_x & 3;
                    uint8_t even = g_emu.read_byte(pair_addr);
                    uint8_t odd = g_emu.read_byte(pair_addr + 1);
                    int shift = (3 - pos_in_pair) * 2;
                    int g = (even >> (shift + 1)) & 1;
                    int r = (odd >> shift) & 1;
                    int b = (odd >> (shift + 1)) & 1;
                    int color = (g << 2) | (b << 1) | r;
                    const char *cname = (color < 8) ? QL_COLOR_NAMES[color] : "?";
                    ImGui::Text("X:%d Y:%d  Color:%d (%s)  Addr:$%05X  Even:$%02X Odd:$%02X",
                                ql_x, ql_y, color, cname, pair_addr, even, odd);
                }
            }
        } else {
            // Dark red placeholder
            ImDrawList *dl = ImGui::GetWindowDrawList();
            dl->AddRectFilled(screen_pos, ImVec2(screen_pos.x + pw, screen_pos.y + ph),
                              IM_COL32(40, 0, 0, 255));
            const char *msg = g_emu.is_running() ? "Booting..." : "Press Build & Run";
            dl->AddText(ImVec2(screen_pos.x + pw/2 - 50, screen_pos.y + ph/2 - 8),
                       IM_COL32(200, 80, 80, 255), msg);
            ImGui::Dummy(ImVec2(pw, ph));
        }

        // Drain iQL log buffer to console
        {
            std::string il = g_emu.drain_iql_log();
            if (!il.empty()) log("%s", il.c_str());
        }

        // Drain trap log to console
        if (trap_log_enabled) {
            std::string tl = g_emu.drain_trap_log();
            if (!tl.empty()) log("%s", tl.c_str());
        }
    }

    // Auto-route keyboard: emulator gets keys when this window is focused
    // and no ImGui text input is active
    bool emu_focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    bool text_active = ImGui::GetIO().WantTextInput;
    emu_wants_keys = g_emu.is_running() && !g_emu.is_paused() && emu_focused && !text_active;

    ImGui::End();
    ImGui::PopStyleColor();
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
    if (g_emu.is_ready()) {
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
    if (g_emu.is_ready()) {
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

    // Breakpoints panel
    ImGui::Begin("Breakpoints");
    if (g_emu.is_ready()) {
        static char bp_addr_buf[16] = "";
        ImGui::SetNextItemWidth(80);
        ImGui::InputText("Addr##bp", bp_addr_buf, sizeof(bp_addr_buf));
        ImGui::SameLine();
        if (ImGui::Button("Add##bp")) {
            uint32_t a = (uint32_t)strtoul(bp_addr_buf, NULL, 16);
            if (a > 0) {
                g_emu.add_breakpoint(a);
                bp_addr_buf[0] = 0;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear All##bp")) {
            g_emu.clear_breakpoints();
        }

        ImGui::Separator();
        auto bps = g_emu.list_breakpoints();
        static int bp_selected = -1;
        ImGui::BeginChild("BPList", ImVec2(0, 120), true);
        for (int i = 0; i < (int)bps.size(); i++) {
            char label[32];
            snprintf(label, sizeof(label), "$%06X", bps[i]);
            if (ImGui::Selectable(label, bp_selected == i))
                bp_selected = i;
        }
        ImGui::EndChild();

        if (ImGui::Button("Remove##bp") && bp_selected >= 0 && bp_selected < (int)bps.size()) {
            g_emu.remove_breakpoint(bps[bp_selected]);
            bp_selected = -1;
        }
    } else {
        ImGui::TextColored(ImVec4(0.5f,0.5f,0.5f,1), "Emulator not running.");
    }
    ImGui::End();

    // Watch panel
    ImGui::Begin("Watch");
    if (g_emu.is_ready()) {
        static char watch_addr_buf[16] = "";
        static char watch_name_buf[32] = "";
        static int watch_type = 0; // 0=byte, 1=word, 2=long

        ImGui::SetNextItemWidth(80);
        ImGui::InputText("Addr##w", watch_addr_buf, sizeof(watch_addr_buf));
        ImGui::SameLine();
        ImGui::SetNextItemWidth(80);
        ImGui::InputText("Name##w", watch_name_buf, sizeof(watch_name_buf));

        ImGui::RadioButton("Byte", &watch_type, 0);
        ImGui::SameLine();
        ImGui::RadioButton("Word", &watch_type, 1);
        ImGui::SameLine();
        ImGui::RadioButton("Long", &watch_type, 2);
        ImGui::SameLine();
        if (ImGui::Button("Add##w")) {
            uint32_t a = (uint32_t)strtoul(watch_addr_buf, NULL, 16);
            if (a > 0) {
                std::string name = watch_name_buf[0] ? watch_name_buf : "";
                if (name.empty()) {
                    char tmp[16];
                    snprintf(tmp, sizeof(tmp), "$%06X", a);
                    name = tmp;
                }
                watches.push_back({a, name, watch_type});
                watch_addr_buf[0] = 0;
                watch_name_buf[0] = 0;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Remove##w") && !watches.empty()) {
            watches.pop_back();
        }

        ImGui::Separator();
        for (auto &w : watches) {
            ImGui::TextColored(ImVec4(0.3f,0.8f,0.7f,1), "%s:", w.name.c_str());
            ImGui::SameLine();
            if (w.type == 0) {
                uint8_t val = g_emu.read_byte(w.addr);
                ImGui::TextColored(ImVec4(0.8f,0.6f,0.4f,1), "$%02X (%d)", val, val);
            } else if (w.type == 1) {
                uint16_t val = g_emu.read_word(w.addr);
                ImGui::TextColored(ImVec4(0.8f,0.6f,0.4f,1), "$%04X (%d)", val, val);
            } else {
                uint32_t val = g_emu.read_long(w.addr);
                ImGui::TextColored(ImVec4(0.8f,0.6f,0.4f,1), "$%08X", val);
            }
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
    images.clear();
    current_image = 0;
    image_texture_dirty = true;
    project_path.clear();
    modified = false;
}

void App::open_project() {
    std::string path = dialog_open("Open Project");
    if (!path.empty()) load_project(path.c_str());
}

void App::save_project(const char *path) {
    if (path) project_path = path;
    if (project_path.empty()) { save_project_as(); return; }

    // Sort sprites alphabetically before saving
    std::sort(sprites.begin(), sprites.end(),
              [](const Sprite &a, const Sprite &b) { return a.name < b.name; });

    json_save_project(project_path.c_str(), sprites, images);
    modified = false;
    log("Project saved: %s (%d sprites, %d images)",
        project_path.c_str(), (int)sprites.size(), (int)images.size());
}

void App::save_project_as() {
    std::string path = dialog_save("Save Project", "sprites.json");
    if (!path.empty()) save_project(path.c_str());
}

void App::load_project(const char *path) {
    sprites.clear();
    images.clear();
    json_load_project(path, sprites, images);
    project_path = path;
    current_sprite = 0;
    current_image = 0;
    image_texture_dirty = true;
    modified = false;
    log("Loaded: %s (%d sprites, %d images)", path, (int)sprites.size(), (int)images.size());
}

void App::export_c(const char *directory) {
    std::string dir;
    if (directory) {
        dir = directory;
    } else {
        dir = dialog_folder("Export C files to directory");
        if (dir.empty()) return;
    }
    last_export_dir = dir;
    write_c_files(dir);
    log("Exported %d sprites to %s", (int)sprites.size(), dir.c_str());
    if (!images.empty()) {
        write_image_c_files(dir);
        log("Exported %d images to %s", (int)images.size(), dir.c_str());
    }
}

void App::screenshot() {
    std::string path = dialog_save("Save Screenshot", "screenshot.ppm");
    if (path.empty()) return;
    if (!g_emu.is_ready()) { log("ERR No emulator screen to capture"); return; }
    if (g_emu.save_screenshot(path.c_str()))
        log("Screenshot: %s", path.c_str());
    else
        log("ERR Screenshot failed");
}

void App::write_c_files(const std::string &dir) {
    std::string c_path = dir + "/sprites.c";
    std::string h_path = dir + "/sprites.h";

    FILE *fc = fopen(c_path.c_str(), "w");
    FILE *fh = fopen(h_path.c_str(), "w");
    if (!fc || !fh) { if (fc) fclose(fc); if (fh) fclose(fh); return; }

    fprintf(fc, "/* sprites.c — Generated by QL Studio */\n");
    fprintf(fc, "#include \"sprites.h\"\n\n");

    fprintf(fh, "/* sprites.h — Generated by QL Studio */\n");
    fprintf(fh, "#ifndef SPRITES_H\n#define SPRITES_H\n\n");
    fprintf(fh, "#include \"game.h\"\n\n");
    fprintf(fh, "typedef struct { uint8_t w; uint8_t h; const uint8_t *data; } Sprite;\n\n");

    for (auto &spr : sprites) {
        int wb = spr.width / 2;
        (void)wb;
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
