/*
 * file_io.c — JSON project load/save, C header export
 *
 * Reads/writes the same JSON format as the Python level_editor.py.
 */

#include "app.h"
#include "native_macos.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ── JSON helpers ─────────────────────────────────────────── */

static const char *skip_ws(const char *p) {
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    return p;
}

static const char *skip_comma(const char *p) {
    p = skip_ws(p);
    if (*p == ',') p++;
    return skip_ws(p);
}

static const char *parse_str(const char *p, char *out, int max) {
    p = skip_ws(p);
    if (*p == '"') p++;
    int i = 0;
    while (*p && *p != '"' && i < max - 1) out[i++] = *p++;
    out[i] = 0;
    if (*p == '"') p++;
    return p;
}

static const char *parse_int_val(const char *p, int *out) {
    p = skip_ws(p);
    *out = atoi(p);
    if (*p == '-') p++;
    while (*p >= '0' && *p <= '9') p++;
    return p;
}

static const char *parse_bool(const char *p, bool *out) {
    p = skip_ws(p);
    if (strncmp(p, "true", 4) == 0) { *out = true; return p + 4; }
    if (strncmp(p, "false", 5) == 0) { *out = false; return p + 5; }
    *out = false;
    return p;
}

static const char *skip_value(const char *p);

static const char *skip_array(const char *p) {
    if (*p != '[') return p;
    p++;
    int depth = 1;
    while (*p && depth > 0) {
        if (*p == '[' || *p == '{') depth++;
        else if (*p == ']' || *p == '}') depth--;
        else if (*p == '"') { p++; while (*p && *p != '"') p++; }
        p++;
    }
    return p;
}

static const char *skip_object(const char *p) {
    if (*p != '{') return p;
    p++;
    int depth = 1;
    while (*p && depth > 0) {
        if (*p == '{' || *p == '[') depth++;
        else if (*p == '}' || *p == ']') depth--;
        else if (*p == '"') { p++; while (*p && *p != '"') p++; }
        p++;
    }
    return p;
}

static const char *skip_value(const char *p) {
    p = skip_ws(p);
    if (*p == '"') { p++; while (*p && *p != '"') p++; if (*p) p++; return p; }
    if (*p == '[') return skip_array(p);
    if (*p == '{') return skip_object(p);
    while (*p && *p != ',' && *p != '}' && *p != ']') p++;
    return p;
}

/* ── Load ─────────────────────────────────────────────────── */

void app_load_project(App *app, const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) { app_log_err(app, "Cannot open %s", path); return; }

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *json = malloc(len + 1);
    fread(json, 1, len, f);
    json[len] = 0;
    fclose(f);

    project_init(&app->project);
    Project *proj = &app->project;

    /* Parse row_types */
    const char *p = strstr(json, "\"row_types\"");
    if (p) {
        p = strchr(p, '['); if (p) p++;
        int idx = 0;
        while (p && *p && idx < MAX_ROW_TYPES) {
            p = skip_ws(p);
            if (*p == ']') break;
            if (*p == ',') { p++; continue; }
            if (*p != '{') break;
            p++;
            RowType *rt = &proj->row_types[idx];
            memset(rt, 0, sizeof(*rt));

            for (;;) {
                p = skip_ws(p);
                if (!*p || *p == '}') break;
                if (*p == ',') { p++; continue; }
                if (*p != '"') break;
                char key[32] = {};
                p = parse_str(p, key, sizeof(key));
                p = skip_ws(p); if (*p == ':') p++;

                if (strcmp(key, "name") == 0) {
                    p = parse_str(p, rt->name, sizeof(rt->name));
                } else if (strcmp(key, "cave_lines") == 0) {
                    p = skip_ws(p);
                    if (*p == '[') {
                        p++;
                        while (*p && *p != ']' && rt->cave_line_count < MAX_POLYLINES) {
                            p = skip_ws(p);
                            if (*p == ',') { p++; continue; }
                            if (*p == ']') break;
                            if (*p != '[') break;
                            p++; /* [ of polyline */
                            Polyline *pl = &rt->cave_lines[rt->cave_line_count];
                            pl->count = 0;
                            while (*p && *p != ']' && pl->count < MAX_POINTS) {
                                p = skip_ws(p);
                                if (*p == ',') { p++; continue; }
                                if (*p == ']') break;
                                if (*p != '[') break;
                                p++; /* [ of point */
                                int x = 0, y = 0;
                                p = parse_int_val(p, &x);
                                p = skip_comma(p);
                                p = parse_int_val(p, &y);
                                p = skip_ws(p);
                                if (*p == ']') p++;
                                pl->points[pl->count++] = (Point){x, y};
                            }
                            if (*p == ']') p++; /* ] of polyline */
                            rt->cave_line_count++;
                        }
                        if (*p == ']') p++; /* ] of cave_lines */
                    }
                } else {
                    p = skip_value(p);
                }
            }
            if (*p == '}') p++;
            idx++;
        }
        proj->row_type_count = idx > 0 ? idx : 32;
    }

    /* Parse levels */
    p = strstr(json, "\"levels\"");
    if (p) {
        p = strchr(p, '['); if (p) p++;
        proj->level_count = 0;
        while (p && *p && proj->level_count < MAX_LEVELS) {
            p = skip_ws(p);
            if (*p == ']') break;
            if (*p == ',') { p++; continue; }
            if (*p != '{') break;
            p++;
            Level *lvl = &proj->levels[proj->level_count];
            memset(lvl, 0, sizeof(*lvl));

            for (;;) {
                p = skip_ws(p);
                if (!*p || *p == '}') break;
                if (*p == ',') { p++; continue; }
                if (*p != '"') break;
                char key[32] = {};
                p = parse_str(p, key, sizeof(key));
                p = skip_ws(p); if (*p == ':') p++;

                if (strcmp(key, "name") == 0) {
                    p = parse_str(p, lvl->name, sizeof(lvl->name));
                } else if (strcmp(key, "rooms") == 0) {
                    p = skip_ws(p);
                    if (*p == '[') {
                        p++;
                        while (*p && *p != ']' && lvl->room_count < MAX_ROOMS) {
                            p = skip_ws(p);
                            if (*p == ',') { p++; continue; }
                            if (*p == ']') break;
                            if (*p != '{') break;
                            p++;
                            Room *room = &lvl->rooms[lvl->room_count];
                            memset(room, 0, sizeof(*room));
                            room->exit_left = room->exit_right = room->exit_top = room->exit_bottom = -1;

                            for (;;) {
                                p = skip_ws(p);
                                if (!*p || *p == '}') break;
                                if (*p == ',') { p++; continue; }
                                if (*p != '"') break;
                                char rkey[32] = {};
                                p = parse_str(p, rkey, sizeof(rkey));
                                p = skip_ws(p); if (*p == ':') p++;

                                if (strcmp(rkey, "number") == 0) p = parse_int_val(p, &room->number);
                                else if (strcmp(rkey, "has_lava") == 0) p = parse_bool(p, &room->has_lava);
                                else if (strcmp(rkey, "exit_left") == 0) { p = skip_ws(p); if (*p == 'n') { room->exit_left = -1; p += 4; } else p = parse_int_val(p, &room->exit_left); }
                                else if (strcmp(rkey, "exit_right") == 0) { p = skip_ws(p); if (*p == 'n') { room->exit_right = -1; p += 4; } else p = parse_int_val(p, &room->exit_right); }
                                else if (strcmp(rkey, "exit_top") == 0) { p = skip_ws(p); if (*p == 'n') { room->exit_top = -1; p += 4; } else p = parse_int_val(p, &room->exit_top); }
                                else if (strcmp(rkey, "exit_bottom") == 0) { p = skip_ws(p); if (*p == 'n') { room->exit_bottom = -1; p += 4; } else p = parse_int_val(p, &room->exit_bottom); }
                                else if (strcmp(rkey, "rows") == 0) {
                                    p = skip_ws(p); if (*p == '[') p++;
                                    for (int i = 0; i < 3; i++) { p = skip_ws(p); p = parse_int_val(p, &room->rows[i]); p = skip_comma(p); }
                                    p = skip_ws(p); if (*p == ']') p++;
                                }
                                else if (strcmp(rkey, "walls") == 0) {
                                    p = skip_ws(p); if (*p == '[') p++;
                                    while (*p && *p != ']' && room->wall_count < MAX_WALLS) {
                                        p = skip_ws(p); if (*p == ',') { p++; continue; } if (*p == ']') break;
                                        if (*p != '{') break; p++;
                                        Wall *w = &room->walls[room->wall_count];
                                        memset(w, 0, sizeof(*w));
                                        for (;;) {
                                            p = skip_ws(p); if (!*p || *p == '}') break; if (*p == ',') { p++; continue; } if (*p != '"') break;
                                            char wk[16] = {}; p = parse_str(p, wk, sizeof(wk)); p = skip_ws(p); if (*p == ':') p++;
                                            if (strcmp(wk, "y") == 0) p = parse_int_val(p, &w->y);
                                            else if (strcmp(wk, "x") == 0) p = parse_int_val(p, &w->x);
                                            else if (strcmp(wk, "h") == 0) p = parse_int_val(p, &w->h);
                                            else if (strcmp(wk, "w") == 0) p = parse_int_val(p, &w->w);
                                            else if (strcmp(wk, "destroyable") == 0) p = parse_bool(p, &w->destroyable);
                                            else p = skip_value(p);
                                        }
                                        if (*p == '}') p++;
                                        room->wall_count++;
                                    }
                                    if (*p == ']') p++;
                                }
                                else if (strcmp(rkey, "enemies") == 0) {
                                    p = skip_ws(p); if (*p == '[') p++;
                                    while (*p && *p != ']' && room->enemy_count < MAX_ENEMIES) {
                                        p = skip_ws(p); if (*p == ',') { p++; continue; } if (*p == ']') break;
                                        if (*p != '{') break; p++;
                                        Enemy *e = &room->enemies[room->enemy_count];
                                        memset(e, 0, sizeof(*e));
                                        for (;;) {
                                            p = skip_ws(p); if (!*p || *p == '}') break; if (*p == ',') { p++; continue; } if (*p != '"') break;
                                            char ek[16] = {}; p = parse_str(p, ek, sizeof(ek)); p = skip_ws(p); if (*p == ':') p++;
                                            if (strcmp(ek, "x") == 0) p = parse_int_val(p, &e->x);
                                            else if (strcmp(ek, "y") == 0) p = parse_int_val(p, &e->y);
                                            else if (strcmp(ek, "vx") == 0) p = parse_int_val(p, &e->vx);
                                            else if (strcmp(ek, "type") == 0) {
                                                p = skip_ws(p);
                                                if (*p == '"') {
                                                    char tstr[16] = {};
                                                    p = parse_str(p, tstr, sizeof(tstr));
                                                    if (strcmp(tstr, "bat") == 0 || strcmp(tstr, "enemy") == 0) e->type = ENEMY_BAT;
                                                    else if (strcmp(tstr, "spider") == 0) e->type = ENEMY_SPIDER;
                                                    else if (strcmp(tstr, "snake") == 0) e->type = ENEMY_SNAKE;
                                                } else {
                                                    p = parse_int_val(p, &e->type);
                                                }
                                            }
                                            else p = skip_value(p);
                                        }
                                        if (*p == '}') p++;
                                        room->enemy_count++;
                                    }
                                    if (*p == ']') p++;
                                }
                                else if (strcmp(rkey, "miner") == 0) {
                                    p = skip_ws(p);
                                    if (*p == 'n') { room->has_miner = false; p += 4; }
                                    else if (*p == '{') {
                                        p++; room->has_miner = true;
                                        for (;;) {
                                            p = skip_ws(p); if (!*p || *p == '}') break; if (*p == ',') { p++; continue; } if (*p != '"') break;
                                            char mk[8] = {}; p = parse_str(p, mk, sizeof(mk)); p = skip_ws(p); if (*p == ':') p++;
                                            if (strcmp(mk, "x") == 0) p = parse_int_val(p, &room->miner.x);
                                            else if (strcmp(mk, "y") == 0) p = parse_int_val(p, &room->miner.y);
                                            else p = skip_value(p);
                                        }
                                        if (*p == '}') p++;
                                    }
                                }
                                else if (strcmp(rkey, "player_start") == 0) {
                                    p = skip_ws(p);
                                    if (*p == 'n') { room->has_player_start = false; p += 4; }
                                    else if (*p == '{') {
                                        p++; room->has_player_start = true;
                                        for (;;) {
                                            p = skip_ws(p); if (!*p || *p == '}') break; if (*p == ',') { p++; continue; } if (*p != '"') break;
                                            char pk[8] = {}; p = parse_str(p, pk, sizeof(pk)); p = skip_ws(p); if (*p == ':') p++;
                                            if (strcmp(pk, "x") == 0) p = parse_int_val(p, &room->player_start.x);
                                            else if (strcmp(pk, "y") == 0) p = parse_int_val(p, &room->player_start.y);
                                            else p = skip_value(p);
                                        }
                                        if (*p == '}') p++;
                                    }
                                }
                                else p = skip_value(p);
                            }
                            if (*p == '}') p++;
                            lvl->room_count++;
                        }
                        /* Skip any remaining rooms beyond MAX_ROOMS */
                        while (*p && *p != ']') {
                            if (*p == '{') p = skip_object(p);
                            else p++;
                        }
                        if (*p == ']') p++;
                    }
                } else p = skip_value(p);
            }
            if (*p == '}') p++;
            proj->level_count++;
        }
    }

    /* Parse sprites */
    p = strstr(json, "\"sprites\"");
    if (p) {
        p = strchr(p, '['); if (p) p++;
        proj->sprite_count = 0;
        while (p && *p && proj->sprite_count < MAX_SPRITES) {
            p = skip_ws(p);
            if (*p == ']') break;
            if (*p == ',') { p++; continue; }
            if (*p != '{') break;
            p++;
            VecSprite *spr = &proj->sprites[proj->sprite_count];
            memset(spr, 0, sizeof(*spr));

            for (;;) {
                p = skip_ws(p);
                if (!*p || *p == '}') break;
                if (*p == ',') { p++; continue; }
                if (*p != '"') break;
                char key[32] = {};
                p = parse_str(p, key, sizeof(key));
                p = skip_ws(p); if (*p == ':') p++;

                if (strcmp(key, "name") == 0) {
                    p = parse_str(p, spr->name, sizeof(spr->name));
                } else if (strcmp(key, "frames") == 0) {
                    p = skip_ws(p);
                    if (*p == '[') {
                        p++;
                        while (*p && *p != ']' && spr->frame_count < MAX_FRAMES) {
                            p = skip_ws(p);
                            if (*p == ',') { p++; continue; }
                            if (*p == ']') break;
                            if (*p != '{') break;
                            p++;
                            SpriteFrame *frame = &spr->frames[spr->frame_count];
                            frame->count = 0;

                            for (;;) {
                                p = skip_ws(p);
                                if (!*p || *p == '}') break;
                                if (*p == ',') { p++; continue; }
                                if (*p != '"') break;
                                char fkey[32] = {};
                                p = parse_str(p, fkey, sizeof(fkey));
                                p = skip_ws(p); if (*p == ':') p++;

                                if (strcmp(fkey, "points") == 0) {
                                    p = skip_ws(p);
                                    if (*p == '[') {
                                        p++;
                                        while (*p && *p != ']' && frame->count < MAX_POINTS) {
                                            p = skip_ws(p);
                                            if (*p == ',') { p++; continue; }
                                            if (*p == ']') break;
                                            if (*p != '[') break;
                                            p++;
                                            int fx = 0, fy = 0;
                                            p = parse_int_val(p, &fx);
                                            p = skip_comma(p);
                                            p = parse_int_val(p, &fy);
                                            p = skip_ws(p);
                                            if (*p == ']') p++;
                                            frame->points[frame->count++] = (Point){fx, fy};
                                        }
                                        if (*p == ']') p++;
                                    }
                                } else {
                                    p = skip_value(p);
                                }
                            }
                            if (*p == '}') p++;
                            spr->frame_count++;
                        }
                        if (*p == ']') p++;
                    }
                } else {
                    p = skip_value(p);
                }
            }
            if (*p == '}') p++;
            proj->sprite_count++;
        }
    }

    free(json);
    strncpy(app->project_path, path, sizeof(app->project_path) - 1);
    app->cur_level = 0;
    app->cur_room = proj->levels[0].room_count > 0 ? 0 : -1;
    app->modified = false;
    app_log_info(app, "Loaded: %s (%d levels, %d sprites)", path, proj->level_count, proj->sprite_count);
}

/* ── Save / New / Open / Export stubs ─────────────────────── */

void app_new_project(App *app) {
    project_init(&app->project);
    app->project_path[0] = 0;
    app->cur_level = 0;
    app->cur_room = 0;
    app->modified = false;
    app_log_info(app, "New project created");
}

void app_open_project(App *app) {
    char *path = dialog_open_file("Open Project", "json");
    if (path) { app_load_project(app, path); free(path); }
}

void app_save_project(App *app, const char *path) {
    if (path) strncpy(app->project_path, path, sizeof(app->project_path) - 1);
    if (!app->project_path[0]) { app_save_project_as(app); return; }
    /* TODO: full JSON save */
    app_log_info(app, "Save not yet implemented");
}

void app_save_project_as(App *app) {
    char *path = dialog_save_file("Save Project", "hero.json");
    if (path) { app_save_project(app, path); free(path); }
}

void app_export(App *app) {
    char *dir = dialog_open_folder("Export to directory");
    if (dir) {
        /* TODO: export levels.h, sprites.h */
        app_log_info(app, "Export not yet implemented");
        free(dir);
    }
}
