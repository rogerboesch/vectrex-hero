/*
 * file_io.c — JSON project load/save, C code export
 */

#include "app.h"
#include "native_macos.h"
#include "json.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Aliases for brevity */
#define ws        json_skip_ws
#define comma     json_skip_comma
#define pstr      json_parse_str
#define pint      json_parse_int
#define pbool     json_parse_bool
#define skip_val  json_skip_value

void app_new_project(App *app) {
    app->sprite_count = 1;
    sprite_init(&app->sprites[0], "sprite_0", 10, 20);
    app->current_sprite = 0;
    app->project_path[0] = 0;
    app->modified = 0;
}

void app_load_project(App *app, const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) { app_log_err(app, "Cannot open %s", path); return; }

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *json = (char *)malloc(len + 1);
    fread(json, 1, len, f);
    json[len] = 0;
    fclose(f);

    /* Parse sprites */
    app->sprite_count = 0;
    const char *p = strstr(json, "\"sprites\"");
    if (p) {
        p = strchr(p, '['); if (p) p++;
        while (p && *p && app->sprite_count < MAX_SPRITES) {
            p = ws(p);
            if (*p == ']') break;
            if (*p == ',') { p++; continue; }
            if (*p != '{') break;
            p++;

            Sprite *spr = &app->sprites[app->sprite_count];
            memset(spr, 0, sizeof(*spr));

            for (;;) {
                p = ws(p);
                if (!*p || *p == '}') break;
                if (*p == ',') { p++; continue; }
                if (*p != '"') break;
                char key[32] = {};
                p = pstr(p, key, sizeof(key));
                p = ws(p); if (*p == ':') p++;

                if (strcmp(key, "name") == 0) {
                    p = pstr(p, spr->name, sizeof(spr->name));
                } else if (strcmp(key, "width") == 0) {
                    p = pint(p, &spr->width);
                } else if (strcmp(key, "height") == 0) {
                    p = pint(p, &spr->height);
                } else if (strcmp(key, "pixels") == 0) {
                    p = ws(p); if (*p == '[') p++;
                    int row = 0;
                    while (*p && *p != ']' && row < MAX_SPRITE_H) {
                        p = ws(p);
                        if (*p == ',') { p++; continue; }
                        if (*p == ']') break;
                        if (*p != '[') break;
                        p++;
                        int col = 0;
                        while (*p && *p != ']' && col < MAX_SPRITE_W) {
                            p = ws(p);
                            if (*p == ',') { p++; continue; }
                            if (*p == ']') break;
                            spr->pixels[row][col++] = atoi(p);
                            while ((*p >= '0' && *p <= '9') || *p == '-') p++;
                        }
                        if (*p == ']') p++;
                        row++;
                    }
                    if (*p == ']') p++;
                } else {
                    p = skip_val(p);
                }
            }
            if (*p == '}') p++;
            app->sprite_count++;
        }
    }

    /* Parse images */
    app->image_count = 0;
    p = strstr(json, "\"images\"");
    if (p) {
        p = strchr(p, '['); if (p) p++;
        while (p && *p && app->image_count < MAX_IMAGES) {
            p = ws(p);
            if (*p == ']') break;
            if (*p == ',') { p++; continue; }
            if (*p != '{') break;
            p++;

            QLImage *img = &app->images[app->image_count];
            memset(img, 0, sizeof(*img));

            for (;;) {
                p = ws(p);
                if (!*p || *p == '}') break;
                if (*p == ',') { p++; continue; }
                if (*p != '"') break;
                char key[32] = {};
                p = pstr(p, key, sizeof(key));
                p = ws(p); if (*p == ':') p++;

                if (strcmp(key, "name") == 0) {
                    p = pstr(p, img->name, sizeof(img->name));
                } else if (strcmp(key, "width") == 0 || strcmp(key, "height") == 0) {
                    int dummy; p = pint(p, &dummy);
                } else if (strcmp(key, "pixels") == 0) {
                    p = ws(p); if (*p == '[') p++;
                    int row = 0;
                    while (*p && *p != ']' && row < IMAGE_SIZE) {
                        p = ws(p);
                        if (*p == ',') { p++; continue; }
                        if (*p == ']') break;
                        if (*p != '[') break;
                        p++;
                        int col = 0;
                        while (*p && *p != ']' && col < IMAGE_SIZE) {
                            p = ws(p);
                            if (*p == ',') { p++; continue; }
                            if (*p == ']') break;
                            img->pixels[row][col++] = atoi(p);
                            while ((*p >= '0' && *p <= '9') || *p == '-') p++;
                        }
                        if (*p == ']') p++;
                        row++;
                    }
                    if (*p == ']') p++;
                } else {
                    p = skip_val(p);
                }
            }
            if (*p == '}') p++;
            app->image_count++;
        }
    }

    free(json);
    strncpy(app->project_path, path, sizeof(app->project_path) - 1);
    app->current_sprite = 0;
    app->current_image = 0;
    app->image_tex_dirty = true;
    app->modified = 0;
    app_log_info(app, "Loaded: %s (%d sprites, %d images)", path, app->sprite_count, app->image_count);
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
    fprintf(f, "  ]");

    if (app->image_count > 0) {
        fprintf(f, ",\n  \"images\": [\n");
        for (int i = 0; i < app->image_count; i++) {
            QLImage *img = &app->images[i];
            fprintf(f, "    {\n");
            fprintf(f, "      \"name\": \"%s\",\n", img->name);
            fprintf(f, "      \"width\": %d,\n", IMAGE_SIZE);
            fprintf(f, "      \"height\": %d,\n", IMAGE_SIZE);
            fprintf(f, "      \"pixels\": [\n");
            for (int y = 0; y < IMAGE_SIZE; y++) {
                fprintf(f, "        [");
                for (int x = 0; x < IMAGE_SIZE; x++) {
                    fprintf(f, "%d", img->pixels[y][x]);
                    if (x < IMAGE_SIZE - 1) fprintf(f, ",");
                }
                fprintf(f, "]%s\n", y < IMAGE_SIZE - 1 ? "," : "");
            }
            fprintf(f, "      ]\n");
            fprintf(f, "    }%s\n", i < app->image_count - 1 ? "," : "");
        }
        fprintf(f, "  ]");
    }

    fprintf(f, "\n}\n");
    fclose(f);
    app->modified = 0;
    app_log_info(app, "Saved: %s (%d sprites, %d images)", app->project_path, app->sprite_count, app->image_count);
}

void app_export_c(App *app, const char *directory) {
    char c_path[512], h_path[512];
    snprintf(c_path, sizeof(c_path), "%s/sprites.c", directory);
    snprintf(h_path, sizeof(h_path), "%s/sprites.h", directory);

    FILE *fc = fopen(c_path, "w");
    FILE *fh = fopen(h_path, "w");
    if (!fc || !fh) { if (fc) fclose(fc); if (fh) fclose(fh); return; }

    fprintf(fc, "/* sprites.c — Generated by QL Workbench */\n");
    fprintf(fc, "#include \"sprites.h\"\n\n");
    fprintf(fh, "/* sprites.h — Generated by QL Workbench */\n");
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
    fclose(fc); fclose(fh);
    app_log_info(app, "Exported %d sprites to %s/sprites.c/.h", app->sprite_count, directory);

    if (app->image_count > 0) {
        snprintf(c_path, sizeof(c_path), "%s/images.c", directory);
        snprintf(h_path, sizeof(h_path), "%s/images.h", directory);
        fc = fopen(c_path, "w"); fh = fopen(h_path, "w");
        if (!fc || !fh) { if (fc) fclose(fc); if (fh) fclose(fh); return; }

        fprintf(fc, "/* images.c — Generated by QL Workbench */\n");
        fprintf(fc, "#include \"images.h\"\n\n");
        fprintf(fh, "/* images.h — Generated by QL Workbench */\n");
        fprintf(fh, "#ifndef IMAGES_H\n#define IMAGES_H\n\n");
        fprintf(fh, "#include <stdint.h>\n\n");

        for (int i = 0; i < app->image_count; i++) {
            QLImage *img = &app->images[i];
            uint8_t mode8[MODE8_BYTES_PER_IMAGE];
            image_to_mode8(img, mode8);
            fprintf(fc, "const uint8_t img_%s[%d] = {\n", img->name, MODE8_BYTES_PER_IMAGE);
            for (int j = 0; j < MODE8_BYTES_PER_IMAGE; j += 16) {
                fprintf(fc, "    ");
                int end = j + 16; if (end > MODE8_BYTES_PER_IMAGE) end = MODE8_BYTES_PER_IMAGE;
                for (int k = j; k < end; k++) {
                    fprintf(fc, "0x%02X", mode8[k]);
                    if (k < MODE8_BYTES_PER_IMAGE - 1) fprintf(fc, ", ");
                }
                fprintf(fc, "\n");
            }
            fprintf(fc, "};\n\n");
            fprintf(fh, "extern const uint8_t img_%s[%d];\n", img->name, MODE8_BYTES_PER_IMAGE);
        }

        fprintf(fh, "\n#endif\n");
        fclose(fc); fclose(fh);
        app_log_info(app, "Exported %d images to %s/images.c/.h", app->image_count, directory);
    }
}

void app_open_project(App *app) {
    char *path = dialog_open_file("Open Project", "json");
    if (path) { app_load_project(app, path); free(path); }
}

void app_save_project_as(App *app) {
    char *path = dialog_save_file("Save Project", "sprites.json");
    if (path) { app_save_project(app, path); free(path); }
}

void app_export_c_dialog(App *app) {
    char *dir = dialog_open_folder("Export C files to directory");
    if (dir) { app_export_c(app, dir); free(dir); }
}
