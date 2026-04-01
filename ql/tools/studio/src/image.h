/*
 * image.h — 256x256 QL Mode 8 image data
 */
#pragma once

#include <stdint.h>
#include <string>
#include <vector>
#include <algorithm>

#define IMAGE_SIZE 256
#define MODE8_BYTES_PER_IMAGE 32768

struct QLImage {
    std::string name;
    std::vector<std::vector<uint8_t>> pixels; // [row][col] = color 0-7

    QLImage(const char *name_ = "image")
        : name(name_) {
        pixels.resize(IMAGE_SIZE, std::vector<uint8_t>(IMAGE_SIZE, 0));
    }

    void clear() {
        for (auto &row : pixels)
            std::fill(row.begin(), row.end(), (uint8_t)0);
    }

    void flip_h() {
        for (auto &row : pixels)
            std::reverse(row.begin(), row.end());
    }

    void flip_v() {
        std::reverse(pixels.begin(), pixels.end());
    }

    void move_up() {
        auto first = pixels[0];
        pixels.erase(pixels.begin());
        pixels.push_back(first);
    }

    void move_down() {
        auto last = pixels.back();
        pixels.pop_back();
        pixels.insert(pixels.begin(), last);
    }

    void move_left() {
        for (auto &row : pixels) {
            auto first = row[0];
            row.erase(row.begin());
            row.push_back(first);
        }
    }

    void move_right() {
        for (auto &row : pixels) {
            auto last = row.back();
            row.pop_back();
            row.insert(row.begin(), last);
        }
    }

    // Convert to Mode 8 byte pairs (matches Python _image_to_mode8_bytes)
    std::vector<uint8_t> to_mode8() const {
        static const int col_remap[] = {0, 1, 2, 3, 4, 6, 5, 7};
        std::vector<uint8_t> data;
        data.reserve(MODE8_BYTES_PER_IMAGE);
        for (int y = 0; y < IMAGE_SIZE; y++) {
            for (int x = 0; x < IMAGE_SIZE; x += 4) {
                uint8_t even_byte = 0, odd_byte = 0;
                for (int bit = 0; bit < 4; bit++) {
                    int c = col_remap[pixels[y][x + bit] & 7];
                    int shift = 7 - (bit * 2);
                    even_byte |= (((c >> 2) & 1) << shift);
                    odd_byte  |= ((c & 1) << shift);
                    odd_byte  |= (((c >> 1) & 1) << (shift - 1));
                }
                data.push_back(even_byte);
                data.push_back(odd_byte);
            }
        }
        return data;
    }
};
