/*
 * file_io.c — JSON project load/save, C code export
 */

#include "app.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void app_new_project(App *app) {
    app->sprite_count = 1;
    sprite_init(&app->sprites[0], "sprite_0", 10, 20);
    app->current_sprite = 0;
    app->project_path[0] = 0;
    app->modified = 0;
}

void app_load_project(App *app, const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) { app_log(app, "ERR Cannot open %s", path); return; }

    /* Read entire file */
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *json = (char *)malloc(len + 1);
    fread(json, 1, len, f);
    json[len] = 0;
    fclose(f);

    /* Simple JSON parse — find sprites array */
    app->sprite_count = 0;
    const char *p = strstr(json, "\"sprites\"");
    if (!p) { free(json); return; }
    p = strchr(p, '[');
    if (!p) { free(json); return; }
    p++;

    while (*p && app->sprite_count < MAX_SPRITES) {
        /* Skip whitespace/commas */
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == ',') p++;
        if (*p == ']') break;
        if (*p != '{') break;
        p++;

        Sprite *spr = &app->sprites[app->sprite_count];
        memset(spr, 0, sizeof(*spr));

        while (*p && *p != '}') {
            while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == ',') p++;
            if (*p == '}') break;
            if (*p != '"') break;
            p++;
            char key[32] = {};
            int ki = 0;
            while (*p && *p != '"' && ki < 31) key[ki++] = *p++;
            if (*p == '"') p++;
            while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == ':') p++;

            if (strcmp(key, "name") == 0) {
                if (*p == '"') p++;
                int ni = 0;
                while (*p && *p != '"' && ni < 63) spr->name[ni++] = *p++;
                if (*p == '"') p++;
            } else if (strcmp(key, "width") == 0) {
                spr->width = atoi(p);
                while ((*p >= '0' && *p <= '9') || *p == '-') p++;
            } else if (strcmp(key, "height") == 0) {
                spr->height = atoi(p);
                while ((*p >= '0' && *p <= '9') || *p == '-') p++;
            } else if (strcmp(key, "pixels") == 0) {
                /* Parse [[0,1,...], ...] */
                if (*p == '[') p++;
                int row = 0;
                while (*p && *p != ']' && row < MAX_SPRITE_H) {
                    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == ',') p++;
                    if (*p == ']') break;
                    if (*p != '[') break;
                    p++;
                    int col = 0;
                    while (*p && *p != ']' && col < MAX_SPRITE_W) {
                        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == ',') p++;
                        if (*p == ']') break;
                        spr->pixels[row][col++] = atoi(p);
                        while ((*p >= '0' && *p <= '9') || *p == '-') p++;
                    }
                    if (*p == ']') p++;
                    row++;
                }
                if (*p == ']') p++;
            }
        }
        if (*p == '}') p++;
        app->sprite_count++;
    }

    free(json);
    strncpy(app->project_path, path, sizeof(app->project_path) - 1);
    app->current_sprite = 0;
    app->modified = 0;
    app_log(app, "Loaded: %s (%d sprites)", path, app->sprite_count);
}

void app_save_project(App *app, const char *path) {
    if (path) strncpy(app->project_path, path, sizeof(app->project_path) - 1);
    if (!app->project_path[0]) return;

    FILE *f = fopen(app->project_path, "w");
    if (!f) return;

    fprintf(f, "{\n  \"sprites\": [\n");
    for (int i = 0; i < app->sprite_count; i++) {
        Sprite *s = &app->sprites[i];
        fprintf(f, "    {\n");
        fprintf(f, "      \"name\": \"%s\",\n", s->name);
        fprintf(f, "      \"width\": %d,\n", s->width);
        fprintf(f, "      \"height\": %d,\n", s->height);
        fprintf(f, "      \"pixels\": [\n");
        for (int y = 0; y < s->height; y++) {
            fprintf(f, "        [");
            for (int x = 0; x < s->width; x++) {
                fprintf(f, "%d", s->pixels[y][x]);
                if (x < s->width - 1) fprintf(f, ", ");
            }
            fprintf(f, "]%s\n", y < s->height - 1 ? "," : "");
        }
        fprintf(f, "      ]\n");
        fprintf(f, "    }%s\n", i < app->sprite_count - 1 ? "," : "");
    }
    fprintf(f, "  ]\n}\n");
    fclose(f);
    app->modified = 0;
    app_log(app, "Saved: %s (%d sprites)", app->project_path, app->sprite_count);
}

void app_export_c(App *app, const char *directory) {
    char c_path[512], h_path[512];
    snprintf(c_path, sizeof(c_path), "%s/sprites.c", directory);
    snprintf(h_path, sizeof(h_path), "%s/sprites.h", directory);

    FILE *fc = fopen(c_path, "w");
    FILE *fh = fopen(h_path, "w");
    if (!fc || !fh) { if (fc) fclose(fc); if (fh) fclose(fh); return; }

    fprintf(fc, "/* sprites.c — Generated by QL Studio 2 */\n");
    fprintf(fc, "#include \"sprites.h\"\n\n");

    fprintf(fh, "/* sprites.h — Generated by QL Studio 2 */\n");
    fprintf(fh, "#ifndef SPRITES_H\n#define SPRITES_H\n\n");
    fprintf(fh, "#include \"game.h\"\n\n");
    fprintf(fh, "typedef struct { uint8_t w; uint8_t h; const uint8_t *data; } Sprite;\n\n");

    for (int i = 0; i < app->sprite_count; i++) {
        Sprite *spr = &app->sprites[i];
        fprintf(fc, "/* %s — %dx%d */\n", spr->name, spr->width, spr->height);
        fprintf(fc, "static const uint8_t spr_%s_data[] = {\n", spr->name);
        for (int y = 0; y < spr->height; y++) {
            fprintf(fc, "    ");
            for (int x = 0; x < spr->width; x += 2) {
                uint8_t hi = spr->pixels[y][x];
                uint8_t lo = (x + 1 < spr->width) ? spr->pixels[y][x + 1] : 0;
                fprintf(fc, "0x%02X, ", (hi << 4) | lo);
            }
            fprintf(fc, "\n");
        }
        fprintf(fc, "};\n");
        fprintf(fc, "const Sprite spr_%s = { %d, %d, spr_%s_data };\n\n",
                spr->name, spr->width, spr->height, spr->name);
        fprintf(fh, "extern const Sprite spr_%s;\n", spr->name);
    }

    fprintf(fh, "\n#endif\n");
    fclose(fc);
    fclose(fh);
    app_log(app, "Exported %d sprites to %s", app->sprite_count, directory);
}
