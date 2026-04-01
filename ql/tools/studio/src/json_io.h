/*
 * json_io.h — Minimal JSON read/write for sprite + image project files
 *
 * Reads/writes the same format as the Python qlstudio.py.
 * No external JSON library — hand-rolled for this specific format.
 */
#pragma once

#include "sprite.h"
#include "image.h"
#include <vector>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

// Read entire file to string
static std::string read_file(const char *path) {
    std::ifstream f(path);
    if (!f.is_open()) return "";
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// Skip whitespace
static const char *skip_ws(const char *p) {
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    return p;
}

// Parse JSON string (simple: no escapes)
static const char *parse_string(const char *p, std::string &out) {
    p = skip_ws(p);
    if (*p != '"') return p;
    p++;
    out.clear();
    while (*p && *p != '"') out += *p++;
    if (*p == '"') p++;
    return p;
}

// Parse JSON integer
static const char *parse_int(const char *p, int &out) {
    p = skip_ws(p);
    out = atoi(p);
    if (*p == '-') p++;
    while (*p >= '0' && *p <= '9') p++;
    return p;
}

// Parse pixel array: [[0,1,2,...], [3,4,...], ...]
static const char *parse_pixels(const char *p, std::vector<std::vector<uint8_t>> &pixels) {
    p = skip_ws(p);
    if (*p != '[') return p;
    p++;
    pixels.clear();
    while (*p && *p != ']') {
        p = skip_ws(p);
        if (*p == ',') { p++; continue; }
        if (*p != '[') break;
        p++; // skip [
        std::vector<uint8_t> row;
        while (*p && *p != ']') {
            p = skip_ws(p);
            if (*p == ',') { p++; continue; }
            int val = 0;
            p = parse_int(p, val);
            row.push_back((uint8_t)val);
        }
        if (*p == ']') p++;
        pixels.push_back(row);
    }
    if (*p == ']') p++;
    return p;
}

// Parse a single object (sprite or image) from the array
static const char *parse_object(const char *p, std::string &name,
                                 int &width, int &height,
                                 std::vector<std::vector<uint8_t>> &pixels) {
    p = skip_ws(p);
    if (*p != '{') return p;
    p++; // skip {

    name.clear();
    width = 0;
    height = 0;
    pixels.clear();

    while (*p && *p != '}') {
        p = skip_ws(p);
        if (*p == ',') { p++; continue; }
        std::string key;
        p = parse_string(p, key);
        p = skip_ws(p);
        if (*p == ':') p++;

        if (key == "name") {
            p = parse_string(p, name);
        } else if (key == "width") {
            p = parse_int(p, width);
        } else if (key == "height") {
            p = parse_int(p, height);
        } else if (key == "pixels") {
            p = parse_pixels(p, pixels);
        }
    }
    if (*p == '}') p++;
    return p;
}

static void json_load_project(const char *path, std::vector<Sprite> &sprites,
                                std::vector<QLImage> &images) {
    std::string json = read_file(path);
    const char *p = json.c_str();

    // Find "sprites" array
    const char *sp = strstr(p, "\"sprites\"");
    if (sp) {
        sp = strchr(sp, '[');
        if (sp) {
            sp++; // skip [
            while (*sp) {
                sp = skip_ws(sp);
                if (*sp == ']') break;
                if (*sp == ',') { sp++; continue; }
                if (*sp != '{') break;

                std::string name;
                int width = 0, height = 0;
                std::vector<std::vector<uint8_t>> pixels;
                sp = parse_object(sp, name, width, height, pixels);

                Sprite spr;
                spr.name = name;
                spr.width = width;
                spr.height = height;
                spr.pixels = pixels;
                sprites.push_back(spr);
            }
        }
    }

    // Find "images" array
    const char *ip = strstr(p, "\"images\"");
    if (ip) {
        ip = strchr(ip, '[');
        if (ip) {
            ip++; // skip [
            while (*ip) {
                ip = skip_ws(ip);
                if (*ip == ']') break;
                if (*ip == ',') { ip++; continue; }
                if (*ip != '{') break;

                std::string name;
                int width = 0, height = 0;
                std::vector<std::vector<uint8_t>> pixels;
                ip = parse_object(ip, name, width, height, pixels);

                QLImage img(name.c_str());
                img.pixels = pixels;
                images.push_back(img);
            }
        }
    }
}

static void json_save_project(const char *path, const std::vector<Sprite> &sprites,
                                const std::vector<QLImage> &images) {
    FILE *f = fopen(path, "w");
    if (!f) return;

    fprintf(f, "{\n  \"sprites\": [\n");
    for (int i = 0; i < (int)sprites.size(); i++) {
        const Sprite &s = sprites[i];
        fprintf(f, "    {\n");
        fprintf(f, "      \"name\": \"%s\",\n", s.name.c_str());
        fprintf(f, "      \"width\": %d,\n", s.width);
        fprintf(f, "      \"height\": %d,\n", s.height);
        fprintf(f, "      \"pixels\": [\n");
        for (int y = 0; y < s.height; y++) {
            fprintf(f, "        [");
            for (int x = 0; x < s.width; x++) {
                fprintf(f, "%d", s.pixels[y][x]);
                if (x < s.width - 1) fprintf(f, ", ");
            }
            fprintf(f, "]%s\n", y < s.height - 1 ? "," : "");
        }
        fprintf(f, "      ]\n");
        fprintf(f, "    }%s\n", i < (int)sprites.size() - 1 ? "," : "");
    }
    fprintf(f, "  ]");

    if (!images.empty()) {
        fprintf(f, ",\n  \"images\": [\n");
        for (int i = 0; i < (int)images.size(); i++) {
            const QLImage &img = images[i];
            fprintf(f, "    {\n");
            fprintf(f, "      \"name\": \"%s\",\n", img.name.c_str());
            fprintf(f, "      \"width\": %d,\n", IMAGE_SIZE);
            fprintf(f, "      \"height\": %d,\n", IMAGE_SIZE);
            fprintf(f, "      \"pixels\": [\n");
            for (int y = 0; y < IMAGE_SIZE; y++) {
                fprintf(f, "        [");
                for (int x = 0; x < IMAGE_SIZE; x++) {
                    fprintf(f, "%d", img.pixels[y][x]);
                    if (x < IMAGE_SIZE - 1) fprintf(f, ", ");
                }
                fprintf(f, "]%s\n", y < IMAGE_SIZE - 1 ? "," : "");
            }
            fprintf(f, "      ]\n");
            fprintf(f, "    }%s\n", i < (int)images.size() - 1 ? "," : "");
        }
        fprintf(f, "  ]");
    }

    fprintf(f, "\n}\n");
    fclose(f);
}

