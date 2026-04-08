/*
 * file_io.c — JSON load/save, C export, level conversion
 */
#include "app.h"
#include "native_macos.h"
#include "json.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void app_new_project(App *app) {
    app->sprite_count = 0;
    app->cur_sprite = 0;
    tilemap_project_init(&app->tmap);
    app->cur_level = 0;
    app->project_path[0] = 0;
    app->modified = false;
    app_log_info(app, "New project");
}

void app_load_project(App *app, const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) { app_log_err(app, "Cannot open %s", path); return; }
    fseek(f, 0, SEEK_END); long len = ftell(f); fseek(f, 0, SEEK_SET);
    char *json = malloc(len + 1); fread(json, 1, len, f); json[len] = 0; fclose(f);

    tilemap_project_init(&app->tmap);

    /* Parse tileset */
    const char *p = strstr(json, "\"tileset\"");
    if (p) {
        p = strchr(p, '['); if (p) p++;
        int idx = 0;
        while (p && *p && idx < TSET_COUNT) {
            p = json_skip_ws(p);
            if (*p == ']') break;
            if (*p == ',') { p++; continue; }
            if (*p != '[') break;
            p++;
            for (int j = 0; j < 16 && *p && *p != ']'; j++) {
                p = json_skip_ws(p);
                if (*p == ',') { p++; p = json_skip_ws(p); }
                int v = 0; p = json_parse_int(p, &v);
                app->tmap.tileset.entries[idx].data[j] = (uint8_t)v;
            }
            if (*p == ']') p++;
            idx++;
        }
        app->tmap.tileset.used_count = idx;
    }

    /* Parse palettes */
    const char *pal_sec;
    pal_sec = strstr(json, "\"bg_palettes\"");
    if (pal_sec) {
        pal_sec = strchr(pal_sec, '['); if (pal_sec) pal_sec++;
        for (int i = 0; i < MAX_BG_PALS; i++) {
            pal_sec = json_skip_ws(pal_sec);
            if (!*pal_sec || *pal_sec == ']') break;
            if (*pal_sec == ',') { pal_sec++; i--; continue; }
            if (*pal_sec != '[') break;
            pal_sec++;
            for (int c = 0; c < COLORS_PER_PAL; c++) {
                pal_sec = json_skip_ws(pal_sec);
                if (*pal_sec == ',') { pal_sec++; pal_sec = json_skip_ws(pal_sec); }
                if (*pal_sec == '[') {
                    pal_sec++;
                    int r=0,g=0,b=0;
                    pal_sec = json_parse_int(pal_sec, &r);
                    pal_sec = json_skip_ws(pal_sec); if (*pal_sec==',') pal_sec++;
                    pal_sec = json_parse_int(pal_sec, &g);
                    pal_sec = json_skip_ws(pal_sec); if (*pal_sec==',') pal_sec++;
                    pal_sec = json_parse_int(pal_sec, &b);
                    pal_sec = json_skip_ws(pal_sec); if (*pal_sec==']') pal_sec++;
                    app->tmap.bg_pals[i].colors[c] = (RGB5){(uint8_t)r,(uint8_t)g,(uint8_t)b};
                }
            }
            pal_sec = json_skip_ws(pal_sec); if (*pal_sec == ']') pal_sec++;
        }
    }
    pal_sec = strstr(json, "\"spr_palettes\"");
    if (pal_sec) {
        pal_sec = strchr(pal_sec, '['); if (pal_sec) pal_sec++;
        for (int i = 0; i < MAX_SPR_PALS; i++) {
            pal_sec = json_skip_ws(pal_sec);
            if (!*pal_sec || *pal_sec == ']') break;
            if (*pal_sec == ',') { pal_sec++; i--; continue; }
            if (*pal_sec != '[') break;
            pal_sec++;
            for (int c = 0; c < COLORS_PER_PAL; c++) {
                pal_sec = json_skip_ws(pal_sec);
                if (*pal_sec == ',') { pal_sec++; pal_sec = json_skip_ws(pal_sec); }
                if (*pal_sec == '[') {
                    pal_sec++;
                    int r=0,g=0,b=0;
                    pal_sec = json_parse_int(pal_sec, &r);
                    pal_sec = json_skip_ws(pal_sec); if (*pal_sec==',') pal_sec++;
                    pal_sec = json_parse_int(pal_sec, &g);
                    pal_sec = json_skip_ws(pal_sec); if (*pal_sec==',') pal_sec++;
                    pal_sec = json_parse_int(pal_sec, &b);
                    pal_sec = json_skip_ws(pal_sec); if (*pal_sec==']') pal_sec++;
                    app->tmap.spr_pals[i].colors[c] = (RGB5){(uint8_t)r,(uint8_t)g,(uint8_t)b};
                }
            }
            pal_sec = json_skip_ws(pal_sec); if (*pal_sec == ']') pal_sec++;
        }
    }

    /* Parse sprites */
    p = strstr(json, "\"sprites\"");
    if (p) {
        p = strchr(p, '['); if (p) p++;
        app->sprite_count = 0;
        while (p && *p && app->sprite_count < MAX_TILES) {
            p = json_skip_ws(p);
            if (*p == ']') break;
            if (*p == ',') { p++; continue; }
            if (*p != '{') break;
            p++;
            GBCTile *spr = &app->sprites[app->sprite_count];
            memset(spr, 0, sizeof(*spr));
            for (;;) {
                p = json_skip_ws(p);
                if (!*p || *p == '}') break;
                if (*p == ',') { p++; continue; }
                if (*p != '"') break;
                char key[32] = {};
                p = json_parse_str(p, key, sizeof(key));
                p = json_skip_ws(p); if (*p == ':') p++;
                if (strcmp(key, "name") == 0) {
                    p = json_parse_str(p, spr->name, sizeof(spr->name));
                } else if (strcmp(key, "palette") == 0) {
                    int v = 0; p = json_parse_int(p, &v);
                    spr->palette = v;
                } else if (strcmp(key, "data") == 0) {
                    p = json_skip_ws(p); if (*p == '[') p++;
                    for (int j = 0; j < TILE_BYTES && *p && *p != ']'; j++) {
                        p = json_skip_ws(p);
                        if (*p == ',') { p++; p = json_skip_ws(p); }
                        int v = 0; p = json_parse_int(p, &v);
                        spr->data[j] = (uint8_t)v;
                    }
                    if (*p == ']') p++;
                } else {
                    p = json_skip_value(p);
                }
            }
            if (*p == '}') p++;
            app->sprite_count++;
        }
    }

    /* Parse levels */
    p = strstr(json, "\"levels\"");
    if (p) {
        p = strchr(p, '['); if (p) p++;
        app->tmap.level_count = 0;
        while (p && *p && app->tmap.level_count < TMAP_MAX_LEVELS) {
            p = json_skip_ws(p);
            if (*p == ']') break;
            if (*p == ',') { p++; continue; }
            if (*p != '{') break;
            p++;

            TilemapLevel *lvl = &app->tmap.levels[app->tmap.level_count];
            memset(lvl, 0, sizeof(*lvl));

            for (;;) {
                p = json_skip_ws(p);
                if (!*p || *p == '}') break;
                if (*p == ',') { p++; continue; }
                if (*p != '"') break;
                char key[32] = {};
                p = json_parse_str(p, key, sizeof(key));
                p = json_skip_ws(p); if (*p == ':') p++;

                if (strcmp(key, "name") == 0) {
                    p = json_parse_str(p, lvl->name, sizeof(lvl->name));
                } else if (strcmp(key, "width") == 0) {
                    p = json_parse_int(p, &lvl->width);
                } else if (strcmp(key, "height") == 0) {
                    p = json_parse_int(p, &lvl->height);
                } else if (strcmp(key, "tiles") == 0) {
                    p = json_skip_ws(p); if (*p == '[') p++;
                    int row = 0;
                    while (*p && *p != ']' && row < TMAP_MAX_H) {
                        p = json_skip_ws(p);
                        if (*p == ',') { p++; continue; }
                        if (*p == ']') break;
                        if (*p != '[') break;
                        p++;
                        int col = 0;
                        while (*p && *p != ']' && col < TMAP_MAX_W) {
                            p = json_skip_ws(p);
                            if (*p == ',') { p++; continue; }
                            if (*p == ']') break;
                            int v = 0; p = json_parse_int(p, &v);
                            lvl->tiles[row][col++] = (uint8_t)v;
                        }
                        if (*p == ']') p++;
                        row++;
                    }
                    if (*p == ']') p++;
                } else if (strcmp(key, "palettes") == 0) {
                    p = json_skip_ws(p); if (*p == '[') p++;
                    int row = 0;
                    while (*p && *p != ']' && row < TMAP_MAX_H) {
                        p = json_skip_ws(p);
                        if (*p == ',') { p++; continue; }
                        if (*p == ']') break;
                        if (*p != '[') break;
                        p++;
                        int col = 0;
                        while (*p && *p != ']' && col < TMAP_MAX_W) {
                            p = json_skip_ws(p);
                            if (*p == ',') { p++; continue; }
                            if (*p == ']') break;
                            int v = 0; p = json_parse_int(p, &v);
                            lvl->palettes[row][col++] = (uint8_t)v;
                        }
                        if (*p == ']') p++;
                        row++;
                    }
                    if (*p == ']') p++;
                } else if (strcmp(key, "entities") == 0) {
                    p = json_skip_ws(p); if (*p == '[') p++;
                    while (*p && *p != ']' && lvl->entity_count < MAX_ENTITIES) {
                        p = json_skip_ws(p);
                        if (*p == ',') { p++; continue; }
                        if (*p == ']') break;
                        if (*p != '{') break;
                        p++;
                        TilemapEntity *e = &lvl->entities[lvl->entity_count];
                        memset(e, 0, sizeof(*e));
                        for (;;) {
                            p = json_skip_ws(p);
                            if (!*p || *p == '}') break;
                            if (*p == ',') { p++; continue; }
                            if (*p != '"') break;
                            char ek[8] = {};
                            p = json_parse_str(p, ek, sizeof(ek));
                            p = json_skip_ws(p); if (*p == ':') p++;
                            int v = 0; p = json_parse_int(p, &v);
                            if (strcmp(ek, "x") == 0) e->x = v;
                            else if (strcmp(ek, "y") == 0) e->y = v;
                            else if (strcmp(ek, "type") == 0) e->type = (uint8_t)v;
                            else if (strcmp(ek, "vx") == 0) e->vx = (int8_t)v;
                        }
                        if (*p == '}') p++;
                        lvl->entity_count++;
                    }
                    if (*p == ']') p++;
                } else {
                    p = json_skip_value(p);
                }
            }
            if (*p == '}') p++;
            app->tmap.level_count++;
        }
    }

    free(json);
    strncpy(app->project_path, path, sizeof(app->project_path) - 1);
    app->cur_level = 0;
    app->modified = false;
    app_log_info(app, "Loaded: %s (%d tiles, %d levels)", path,
                 app->tmap.tileset.used_count, app->tmap.level_count);
}

void app_save_project(App *app, const char *path) {
    if (path) strncpy(app->project_path, path, sizeof(app->project_path) - 1);
    if (!app->project_path[0]) { app_save_project_as(app); return; }

    FILE *f = fopen(app->project_path, "w");
    if (!f) { app_log_err(app, "Cannot write %s", app->project_path); return; }

    fprintf(f, "{\n");

    /* Tileset */
    fprintf(f, "  \"tileset\": [\n");
    for (int i = 0; i < app->tmap.tileset.used_count; i++) {
        fprintf(f, "    [");
        for (int j = 0; j < 16; j++)
            fprintf(f, "%d%s", app->tmap.tileset.entries[i].data[j], j < 15 ? "," : "");
        fprintf(f, "]%s\n", i < app->tmap.tileset.used_count - 1 ? "," : "");
    }
    fprintf(f, "  ],\n");

    /* Palettes */
    fprintf(f, "  \"bg_palettes\": [\n");
    for (int i = 0; i < MAX_BG_PALS; i++) {
        fprintf(f, "    [");
        for (int c = 0; c < COLORS_PER_PAL; c++) {
            RGB5 rgb = app->tmap.bg_pals[i].colors[c];
            fprintf(f, "[%d,%d,%d]%s", rgb.r, rgb.g, rgb.b, c < COLORS_PER_PAL - 1 ? "," : "");
        }
        fprintf(f, "]%s\n", i < MAX_BG_PALS - 1 ? "," : "");
    }
    fprintf(f, "  ],\n");
    fprintf(f, "  \"spr_palettes\": [\n");
    for (int i = 0; i < MAX_SPR_PALS; i++) {
        fprintf(f, "    [");
        for (int c = 0; c < COLORS_PER_PAL; c++) {
            RGB5 rgb = app->tmap.spr_pals[i].colors[c];
            fprintf(f, "[%d,%d,%d]%s", rgb.r, rgb.g, rgb.b, c < COLORS_PER_PAL - 1 ? "," : "");
        }
        fprintf(f, "]%s\n", i < MAX_SPR_PALS - 1 ? "," : "");
    }
    fprintf(f, "  ],\n");

    /* Sprites */
    fprintf(f, "  \"sprites\": [\n");
    for (int i = 0; i < app->sprite_count; i++) {
        GBCTile *spr = &app->sprites[i];
        fprintf(f, "    {\"name\":\"%s\",\"palette\":%d,\"data\":[", spr->name, spr->palette);
        for (int j = 0; j < TILE_BYTES; j++)
            fprintf(f, "%d%s", spr->data[j], j < TILE_BYTES - 1 ? "," : "");
        fprintf(f, "]}%s\n", i < app->sprite_count - 1 ? "," : "");
    }
    fprintf(f, "  ],\n");

    /* Levels */
    fprintf(f, "  \"levels\": [\n");
    for (int li = 0; li < app->tmap.level_count; li++) {
        TilemapLevel *lvl = &app->tmap.levels[li];
        fprintf(f, "    {\n");
        fprintf(f, "      \"name\": \"%s\",\n", lvl->name);
        fprintf(f, "      \"width\": %d,\n", lvl->width);
        fprintf(f, "      \"height\": %d,\n", lvl->height);
        /* Tiles as rows of comma-separated indices */
        fprintf(f, "      \"tiles\": [\n");
        for (int y = 0; y < lvl->height; y++) {
            fprintf(f, "        [");
            for (int x = 0; x < lvl->width; x++)
                fprintf(f, "%d%s", lvl->tiles[y][x], x < lvl->width - 1 ? "," : "");
            fprintf(f, "]%s\n", y < lvl->height - 1 ? "," : "");
        }
        fprintf(f, "      ],\n");
        /* Per-tile palette attributes */
        fprintf(f, "      \"palettes\": [\n");
        for (int y = 0; y < lvl->height; y++) {
            fprintf(f, "        [");
            for (int x = 0; x < lvl->width; x++)
                fprintf(f, "%d%s", lvl->palettes[y][x], x < lvl->width - 1 ? "," : "");
            fprintf(f, "]%s\n", y < lvl->height - 1 ? "," : "");
        }
        fprintf(f, "      ],\n");
        /* Entities */
        fprintf(f, "      \"entities\": [\n");
        for (int i = 0; i < lvl->entity_count; i++) {
            TilemapEntity *e = &lvl->entities[i];
            fprintf(f, "        {\"x\":%d,\"y\":%d,\"type\":%d,\"vx\":%d}%s\n",
                    e->x, e->y, e->type, e->vx, i < lvl->entity_count - 1 ? "," : "");
        }
        fprintf(f, "      ]\n");
        fprintf(f, "    }%s\n", li < app->tmap.level_count - 1 ? "," : "");
    }
    fprintf(f, "  ]\n");

    fprintf(f, "}\n");
    fclose(f);
    app->modified = false;
    app_log_info(app, "Saved: %s (%d levels)", app->project_path, app->tmap.level_count);
}

void app_open_project(App *app) {
    char *p = dialog_open_file("Open Project", "json");
    if (p) { app_load_project(app, p); free(p); }
}

void app_save_project_as(App *app) {
    char *p = dialog_save_file("Save Project", "tilemap.json");
    if (p) { app_save_project(app, p); free(p); }
}

/* Defined in export.c */
void export_sprites(App *app, const char *dir);
void export_palettes(App *app, const char *dir);
void export_levels(App *app, const char *dir);

void app_export_c(App *app) {
    char *dir = dialog_open_folder("Export to directory");
    if (!dir) return;

    char c_path[512], h_path[512];
    FILE *fc, *fh;

    /* Export tileset to tileset_export.c/h */
    snprintf(c_path, sizeof(c_path), "%s/tileset_export.c", dir);
    snprintf(h_path, sizeof(h_path), "%s/tileset_export.h", dir);
    fc = fopen(c_path, "w");
    fh = fopen(h_path, "w");
    if (!fc || !fh) { if (fc) fclose(fc); if (fh) fclose(fh); free(dir); return; }

    fprintf(fc, "/* tileset_export.c — Generated by GBC Workbench */\n");
    fprintf(fc, "#include \"tileset_export.h\"\n\n");
    fprintf(fh, "/* tileset_export.h — Generated by GBC Workbench */\n");
    fprintf(fh, "#ifndef TILESET_EXPORT_H\n#define TILESET_EXPORT_H\n\n");
    fprintf(fh, "#include <stdint.h>\n\n");
    fprintf(fh, "#define TILESET_COUNT %d\n", app->tmap.tileset.used_count);
    fprintf(fh, "extern const uint8_t tileset_data[%d][16];\n\n", app->tmap.tileset.used_count);
    fprintf(fh, "#endif\n");

    fprintf(fc, "const uint8_t tileset_data[%d][16] = {\n", app->tmap.tileset.used_count);
    for (int i = 0; i < app->tmap.tileset.used_count; i++) {
        fprintf(fc, "    {");
        for (int j = 0; j < 16; j++)
            fprintf(fc, "0x%02X%s", app->tmap.tileset.entries[i].data[j], j < 15 ? "," : "");
        fprintf(fc, "},\n");
    }
    fprintf(fc, "};\n");
    fclose(fc); fclose(fh);
    app_log_info(app, "Exported %d tiles to %s", app->tmap.tileset.used_count, c_path);

    /* Export levels to level_data.c/h + levels_meta.c */
    export_levels(app, dir);

    /* Export sprites */
    export_sprites(app, dir);

    /* Export palettes */
    export_palettes(app, dir);

    free(dir);
}

