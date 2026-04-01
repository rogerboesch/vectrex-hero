/*
 * json_io.h — Minimal JSON read/write for sprite project files
 *
 * Reads/writes the same format as the Python sprite_editor.py.
 * No external JSON library — hand-rolled for this specific format.
 */
#pragma once

#include "sprite.h"
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

static void json_load_project(const char *path, std::vector<Sprite> &sprites) {
    std::string json = read_file(path);
    const char *p = json.c_str();

    // Find "sprites" array
    p = strstr(p, "\"sprites\"");
    if (!p) return;
    p = strchr(p, '[');
    if (!p) return;
    p++; // skip [

    while (*p) {
        p = skip_ws(p);
        if (*p == ']') break;
        if (*p == ',') { p++; continue; }
        if (*p != '{') break;
        p++; // skip {

        Sprite spr;
        while (*p && *p != '}') {
            p = skip_ws(p);
            if (*p == ',') { p++; continue; }
            std::string key;
            p = parse_string(p, key);
            p = skip_ws(p);
            if (*p == ':') p++;

            if (key == "name") {
                p = parse_string(p, spr.name);
            } else if (key == "width") {
                p = parse_int(p, spr.width);
            } else if (key == "height") {
                p = parse_int(p, spr.height);
            } else if (key == "pixels") {
                p = parse_pixels(p, spr.pixels);
            }
        }
        if (*p == '}') p++;
        sprites.push_back(spr);
    }
}

static void json_save_project(const char *path, const std::vector<Sprite> &sprites) {
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
    fprintf(f, "  ]\n}\n");
    fclose(f);
}
