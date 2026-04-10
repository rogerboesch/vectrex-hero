/*
 * project_io.h — Shared JSON parser for project.json level/room/sprite data
 *
 * Header-only. Both Vectrex Workbench and GBC Workbench include this.
 * Requires: project.h, json.h, <stdio.h>, <stdlib.h>, <string.h>
 */
#pragma once

#include "project.h"
#include "json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Parse row_types, levels, and sprites from a project.json string.
   Returns number of levels parsed. */
static inline int project_parse_json(const char *json, Project *proj) {
    const char *p;

    /* Parse row_types */
    p = strstr(json, "\"row_types\"");
    if (p) {
        p = strchr(p, '['); if (p) p++;
        int idx = 0;
        while (p && *p && idx < MAX_ROW_TYPES) {
            p = json_skip_ws(p);
            if (*p == ']') break;
            if (*p == ',') { p++; continue; }
            if (*p != '{') break;
            p++;
            RowType *rt = &proj->row_types[idx];
            memset(rt, 0, sizeof(*rt));

            for (;;) {
                p = json_skip_ws(p);
                if (!*p || *p == '}') break;
                if (*p == ',') { p++; continue; }
                if (*p != '"') break;
                char key[32] = {};
                p = json_parse_str(p, key, sizeof(key));
                p = json_skip_ws(p); if (*p == ':') p++;

                if (strcmp(key, "name") == 0) {
                    p = json_parse_str(p, rt->name, sizeof(rt->name));
                } else if (strcmp(key, "cave_lines") == 0) {
                    p = json_skip_ws(p);
                    if (*p == '[') {
                        p++;
                        while (*p && *p != ']' && rt->cave_line_count < MAX_POLYLINES) {
                            p = json_skip_ws(p);
                            if (*p == ',') { p++; continue; }
                            if (*p == ']') break;
                            if (*p != '[') break;
                            p++;
                            Polyline *pl = &rt->cave_lines[rt->cave_line_count];
                            pl->count = 0;
                            while (*p && *p != ']' && pl->count < MAX_POINTS) {
                                p = json_skip_ws(p);
                                if (*p == ',') { p++; continue; }
                                if (*p == ']') break;
                                if (*p != '[') break;
                                p++;
                                int x = 0, y = 0;
                                p = json_parse_int(p, &x);
                                p = json_skip_comma(p);
                                p = json_parse_int(p, &y);
                                p = json_skip_ws(p);
                                if (*p == ']') p++;
                                pl->points[pl->count++] = (Point){x, y};
                            }
                            if (*p == ']') p++;
                            rt->cave_line_count++;
                        }
                        if (*p == ']') p++;
                    }
                } else {
                    p = json_skip_value(p);
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
            p = json_skip_ws(p);
            if (*p == ']') break;
            if (*p == ',') { p++; continue; }
            if (*p != '{') break;
            p++;
            Level *lvl = &proj->levels[proj->level_count];
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
                } else if (strcmp(key, "rooms") == 0) {
                    p = json_skip_ws(p);
                    if (*p == '[') {
                        p++;
                        while (*p && *p != ']' && lvl->room_count < MAX_ROOMS) {
                            p = json_skip_ws(p);
                            if (*p == ',') { p++; continue; }
                            if (*p == ']') break;
                            if (*p != '{') break;
                            p++;
                            Room *room = &lvl->rooms[lvl->room_count];
                            memset(room, 0, sizeof(*room));
                            room->exit_left = room->exit_right = room->exit_top = room->exit_bottom = -1;

                            for (;;) {
                                p = json_skip_ws(p);
                                if (!*p || *p == '}') break;
                                if (*p == ',') { p++; continue; }
                                if (*p != '"') break;
                                char rkey[32] = {};
                                p = json_parse_str(p, rkey, sizeof(rkey));
                                p = json_skip_ws(p); if (*p == ':') p++;

                                if (strcmp(rkey, "number") == 0) p = json_parse_int(p, &room->number);
                                else if (strcmp(rkey, "has_lava") == 0) p = json_parse_bool(p, &room->has_lava);
                                else if (strcmp(rkey, "exit_left") == 0) { p = json_skip_ws(p); if (*p == 'n') { room->exit_left = -1; p += 4; } else p = json_parse_int(p, &room->exit_left); }
                                else if (strcmp(rkey, "exit_right") == 0) { p = json_skip_ws(p); if (*p == 'n') { room->exit_right = -1; p += 4; } else p = json_parse_int(p, &room->exit_right); }
                                else if (strcmp(rkey, "exit_top") == 0) { p = json_skip_ws(p); if (*p == 'n') { room->exit_top = -1; p += 4; } else p = json_parse_int(p, &room->exit_top); }
                                else if (strcmp(rkey, "exit_bottom") == 0) { p = json_skip_ws(p); if (*p == 'n') { room->exit_bottom = -1; p += 4; } else p = json_parse_int(p, &room->exit_bottom); }
                                else if (strcmp(rkey, "rows") == 0) {
                                    p = json_skip_ws(p); if (*p == '[') p++;
                                    for (int i = 0; i < 3; i++) { p = json_skip_ws(p); p = json_parse_int(p, &room->rows[i]); p = json_skip_comma(p); }
                                    p = json_skip_ws(p); if (*p == ']') p++;
                                }
                                else if (strcmp(rkey, "walls") == 0) {
                                    p = json_skip_ws(p); if (*p == '[') p++;
                                    while (*p && *p != ']' && room->wall_count < MAX_WALLS) {
                                        p = json_skip_ws(p); if (*p == ',') { p++; continue; } if (*p == ']') break;
                                        if (*p != '{') break; p++;
                                        Wall *w = &room->walls[room->wall_count];
                                        memset(w, 0, sizeof(*w));
                                        for (;;) {
                                            p = json_skip_ws(p); if (!*p || *p == '}') break; if (*p == ',') { p++; continue; } if (*p != '"') break;
                                            char wk[16] = {}; p = json_parse_str(p, wk, sizeof(wk)); p = json_skip_ws(p); if (*p == ':') p++;
                                            if (strcmp(wk, "y") == 0) p = json_parse_int(p, &w->y);
                                            else if (strcmp(wk, "x") == 0) p = json_parse_int(p, &w->x);
                                            else if (strcmp(wk, "h") == 0) p = json_parse_int(p, &w->h);
                                            else if (strcmp(wk, "w") == 0) p = json_parse_int(p, &w->w);
                                            else if (strcmp(wk, "destroyable") == 0) p = json_parse_bool(p, &w->destroyable);
                                            else p = json_skip_value(p);
                                        }
                                        if (*p == '}') p++;
                                        room->wall_count++;
                                    }
                                    if (*p == ']') p++;
                                }
                                else if (strcmp(rkey, "enemies") == 0) {
                                    p = json_skip_ws(p); if (*p == '[') p++;
                                    while (*p && *p != ']' && room->enemy_count < MAX_ENEMIES) {
                                        p = json_skip_ws(p); if (*p == ',') { p++; continue; } if (*p == ']') break;
                                        if (*p != '{') break; p++;
                                        Enemy *e = &room->enemies[room->enemy_count];
                                        memset(e, 0, sizeof(*e));
                                        for (;;) {
                                            p = json_skip_ws(p); if (!*p || *p == '}') break; if (*p == ',') { p++; continue; } if (*p != '"') break;
                                            char ek[16] = {}; p = json_parse_str(p, ek, sizeof(ek)); p = json_skip_ws(p); if (*p == ':') p++;
                                            if (strcmp(ek, "x") == 0) p = json_parse_int(p, &e->x);
                                            else if (strcmp(ek, "y") == 0) p = json_parse_int(p, &e->y);
                                            else if (strcmp(ek, "vx") == 0) p = json_parse_int(p, &e->vx);
                                            else if (strcmp(ek, "type") == 0) {
                                                p = json_skip_ws(p);
                                                if (*p == '"') {
                                                    char ts[16] = {}; p = json_parse_str(p, ts, sizeof(ts));
                                                    if (strcmp(ts, "spider") == 0) e->type = ENEMY_SPIDER;
                                                    else if (strcmp(ts, "snake") == 0) e->type = ENEMY_SNAKE;
                                                    else e->type = ENEMY_BAT;
                                                } else p = json_parse_int(p, &e->type);
                                            }
                                            else p = json_skip_value(p);
                                        }
                                        if (*p == '}') p++;
                                        room->enemy_count++;
                                    }
                                    if (*p == ']') p++;
                                }
                                else if (strcmp(rkey, "miner") == 0) {
                                    p = json_skip_ws(p);
                                    if (*p == 'n') { room->has_miner = false; p += 4; }
                                    else if (*p == '{') {
                                        p++; room->has_miner = true;
                                        for (;;) {
                                            p = json_skip_ws(p); if (!*p || *p == '}') break; if (*p == ',') { p++; continue; } if (*p != '"') break;
                                            char mk[8] = {}; p = json_parse_str(p, mk, sizeof(mk)); p = json_skip_ws(p); if (*p == ':') p++;
                                            if (strcmp(mk, "x") == 0) p = json_parse_int(p, &room->miner.x);
                                            else if (strcmp(mk, "y") == 0) p = json_parse_int(p, &room->miner.y);
                                            else p = json_skip_value(p);
                                        }
                                        if (*p == '}') p++;
                                    }
                                }
                                else if (strcmp(rkey, "player_start") == 0) {
                                    p = json_skip_ws(p);
                                    if (*p == 'n') { room->has_player_start = false; p += 4; }
                                    else if (*p == '{') {
                                        p++; room->has_player_start = true;
                                        for (;;) {
                                            p = json_skip_ws(p); if (!*p || *p == '}') break; if (*p == ',') { p++; continue; } if (*p != '"') break;
                                            char pk[8] = {}; p = json_parse_str(p, pk, sizeof(pk)); p = json_skip_ws(p); if (*p == ':') p++;
                                            if (strcmp(pk, "x") == 0) p = json_parse_int(p, &room->player_start.x);
                                            else if (strcmp(pk, "y") == 0) p = json_parse_int(p, &room->player_start.y);
                                            else p = json_skip_value(p);
                                        }
                                        if (*p == '}') p++;
                                    }
                                }
                                else p = json_skip_value(p);
                            }
                            if (*p == '}') p++;
                            lvl->room_count++;
                        }
                        /* Skip remaining rooms beyond MAX_ROOMS */
                        while (*p && *p != ']') {
                            if (*p == '{') p = json_skip_object(p);
                            else p++;
                        }
                        if (*p == ']') p++;
                    }
                } else p = json_skip_value(p);
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
            p = json_skip_ws(p);
            if (*p == ']') break;
            if (*p == ',') { p++; continue; }
            if (*p != '{') break;
            p++;
            VecSprite *spr = &proj->sprites[proj->sprite_count];
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
                } else if (strcmp(key, "frames") == 0) {
                    p = json_skip_ws(p);
                    if (*p == '[') {
                        p++;
                        while (*p && *p != ']' && spr->frame_count < MAX_FRAMES) {
                            p = json_skip_ws(p);
                            if (*p == ',') { p++; continue; }
                            if (*p == ']') break;
                            if (*p != '{') break;
                            p++;
                            SpriteFrame *frame = &spr->frames[spr->frame_count];
                            frame->count = 0;
                            for (;;) {
                                p = json_skip_ws(p);
                                if (!*p || *p == '}') break;
                                if (*p == ',') { p++; continue; }
                                if (*p != '"') break;
                                char fkey[32] = {};
                                p = json_parse_str(p, fkey, sizeof(fkey));
                                p = json_skip_ws(p); if (*p == ':') p++;
                                if (strcmp(fkey, "points") == 0) {
                                    p = json_skip_ws(p);
                                    if (*p == '[') {
                                        p++;
                                        while (*p && *p != ']' && frame->count < MAX_POINTS) {
                                            p = json_skip_ws(p);
                                            if (*p == ',') { p++; continue; }
                                            if (*p == ']') break;
                                            if (*p != '[') break;
                                            p++;
                                            int fx = 0, fy = 0;
                                            p = json_parse_int(p, &fx);
                                            p = json_skip_comma(p);
                                            p = json_parse_int(p, &fy);
                                            p = json_skip_ws(p);
                                            if (*p == ']') p++;
                                            frame->points[frame->count++] = (Point){fx, fy};
                                        }
                                        if (*p == ']') p++;
                                    }
                                } else p = json_skip_value(p);
                            }
                            if (*p == '}') p++;
                            spr->frame_count++;
                        }
                        if (*p == ']') p++;
                    }
                } else p = json_skip_value(p);
            }
            if (*p == '}') p++;
            proj->sprite_count++;
        }
    }

    return proj->level_count;
}
