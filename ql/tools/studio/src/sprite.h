/*
 * sprite.h — Sprite data for QL Mode 8
 */
#pragma once

#include <stdint.h>
#include <string>
#include <vector>

// QL Mode 8 colors (RGBA for display)
static const uint32_t QL_COLORS[8] = {
    0x000000FF, // 0: black
    0xFF0000FF, // 1: red
    0x0000FFFF, // 2: blue
    0xFF00FFFF, // 3: magenta
    0x00FF00FF, // 4: green
    0x00FFFFFF, // 5: cyan
    0xFFFF00FF, // 6: yellow
    0xFFFFFFFF, // 7: white
};

static const char *QL_COLOR_NAMES[8] = {
    "transparent", "red", "blue", "magenta",
    "green", "cyan", "yellow", "white"
};

struct Sprite {
    std::string name;
    int width = 10;   // must be even
    int height = 20;
    std::vector<std::vector<uint8_t>> pixels; // [row][col] = color 0-7

    Sprite(const char *name_ = "sprite", int w = 10, int h = 20)
        : name(name_), width(w), height(h) {
        pixels.resize(h, std::vector<uint8_t>(w, 0));
    }

    void resize(int new_w, int new_h) {
        if (new_w & 1) new_w++;
        std::vector<std::vector<uint8_t>> old = pixels;
        pixels.resize(new_h, std::vector<uint8_t>(new_w, 0));
        for (int y = 0; y < new_h && y < (int)old.size(); y++)
            for (int x = 0; x < new_w && x < (int)old[y].size(); x++)
                pixels[y][x] = old[y][x];
        width = new_w;
        height = new_h;
    }

    void flip_h() {
        for (auto &row : pixels)
            std::reverse(row.begin(), row.end());
    }

    void flip_v() {
        std::reverse(pixels.begin(), pixels.end());
    }

    void move_up() {
        if (pixels.size() < 2) return;
        auto first = pixels[0];
        pixels.erase(pixels.begin());
        pixels.push_back(first);
    }

    void move_down() {
        if (pixels.size() < 2) return;
        auto last = pixels.back();
        pixels.pop_back();
        pixels.insert(pixels.begin(), last);
    }

    void move_left() {
        for (auto &row : pixels) {
            if (row.size() < 2) continue;
            auto first = row[0];
            row.erase(row.begin());
            row.push_back(first);
        }
    }

    void move_right() {
        for (auto &row : pixels) {
            if (row.size() < 2) continue;
            auto last = row.back();
            row.pop_back();
            row.insert(row.begin(), last);
        }
    }

    void clear() {
        for (auto &row : pixels)
            std::fill(row.begin(), row.end(), (uint8_t)0);
    }
};
