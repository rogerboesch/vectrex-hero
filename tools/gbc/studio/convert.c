/*
 * convert.c — CLI tool for GBC level data conversion
 *
 * Mode 1: ./convert hero.json project.json
 *         Convert Vectrex hero.json → GBC tilemap project.json
 *
 * Mode 2: ./convert --c-export project.json level_data.c level_data.h
 *         Export project.json → C source with auto-tiled RLE data for GBDK
 *
 * Build: cc -std=c11 -O2 -I../../shared -I../igb -o convert convert.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "json.h"
#include "project.h"
#include "project_io.h"
#include "src/tile.h"
#include "src/tilemap.h"

/* ── Conversion from hero.json ───────────────────────────── */

#define GRID_W 20
#define GRID_H 16
#define CELL_EMPTY 0
#define CELL_BORDER 1
#define CELL_SOLID 2

static int tc(int gx) { return (int)(((unsigned)((unsigned char)(gx+128))*20)>>8); }
static int tr(int gy) { int v=(int)(((unsigned)(50-gy)*39)>>8); return v<0?0:v>=GRID_H?GRID_H-1:v; }

static void tl(uint8_t g[GRID_H][GRID_W], int x1,int y1,int x2,int y2) {
    int c1=tc(x1),r1=tr(y1),c2=tc(x2),r2=tr(y2);
    int dx=abs(c2-c1),dy=abs(r2-r1),sx=c1<c2?1:-1,sy=r1<r2?1:-1,err=dx-dy;
    for(;;) {
        if(r1>=0&&r1<GRID_H&&c1>=0&&c1<GRID_W) g[r1][c1]=CELL_BORDER;
        if(c1==c2&&r1==r2) break;
        int e2=2*err;
        if(e2>-dy){err-=dy;c1+=sx;}
        if(e2<dx){err+=dx;r1+=sy;}
    }
}

static void ff(uint8_t g[GRID_H][GRID_W],int sr,int sc) {
    if(sr<0||sr>=GRID_H||sc<0||sc>=GRID_W) return;
    g[sr][sc]=CELL_EMPTY;
    int ch=1; while(ch){ch=0;
        for(int r=0;r<GRID_H;r++) for(int c=0;c<GRID_W;c++){
            if(g[r][c]!=CELL_SOLID) continue;
            if((r>0&&g[r-1][c]==CELL_EMPTY)||(r<GRID_H-1&&g[r+1][c]==CELL_EMPTY)||
               (c>0&&g[r][c-1]==CELL_EMPTY)||(c<GRID_W-1&&g[r][c+1]==CELL_EMPTY))
                {g[r][c]=CELL_EMPTY;ch=1;}
        }
    }
}

static void add_entity(TilemapLevel *tm, int tx, int ty, uint8_t type, int8_t vx) {
    if (tm->entity_count >= MAX_ENTITIES) return;
    tm->entities[tm->entity_count++] = (TilemapEntity){tx, ty, type, vx};
}

static void convert_levels(const char *hero_json, TilemapProject *tmap) {
    Project proj;
    project_init(&proj);
    project_parse_json(hero_json, &proj);

    printf("Converting %d levels...\n", proj.level_count);

    if (tmap->tileset.used_count < 3) tmap->tileset.used_count = 3;
    tmap->level_count = proj.level_count;
    int yoff[] = {20, -10, -40};

    for (int li = 0; li < proj.level_count; li++) {
        Level *lvl = &proj.levels[li];

        /* BFS from room 0 using exits to compute grid positions */
        int gx[MAX_ROOMS], gy[MAX_ROOMS];
        int placed[MAX_ROOMS];
        memset(placed, 0, sizeof(placed));
        memset(gx, 0, sizeof(gx)); memset(gy, 0, sizeof(gy));

        if (lvl->room_count > 0) {
            placed[0] = 1;
            int queue[MAX_ROOMS], qh = 0, qt = 0;
            queue[qt++] = 0;
            while (qh < qt) {
                int ri = queue[qh++];
                Room *r = &lvl->rooms[ri];
                struct { int target; int dx; int dy; } exits[] = {
                    {r->exit_right,  1,  0},
                    {r->exit_left,  -1,  0},
                    {r->exit_top,    0, -1},
                    {r->exit_bottom, 0,  1},
                };
                for (int e = 0; e < 4; e++) {
                    if (exits[e].target < 0) continue;
                    int ti = exits[e].target - 1;
                    if (ti < 0 || ti >= lvl->room_count || placed[ti]) continue;
                    placed[ti] = 1;
                    gx[ti] = gx[ri] + exits[e].dx;
                    gy[ti] = gy[ri] + exits[e].dy;
                    queue[qt++] = ti;
                }
            }
            int max_gy_val = 0;
            for (int i = 0; i < lvl->room_count; i++)
                if (placed[i] && gy[i] > max_gy_val) max_gy_val = gy[i];
            int col = 0;
            for (int i = 0; i < lvl->room_count; i++) {
                if (!placed[i]) { placed[i] = 1; gx[i] = col++; gy[i] = max_gy_val + 2; }
            }
        }

        /* Normalize so min is (0,0) */
        int min_gx = 0, min_gy = 0;
        for (int i = 0; i < lvl->room_count; i++) {
            if (gx[i] < min_gx) min_gx = gx[i];
            if (gy[i] < min_gy) min_gy = gy[i];
        }
        int max_gx = 0, max_gy = 0;
        for (int i = 0; i < lvl->room_count; i++) {
            gx[i] -= min_gx; gy[i] -= min_gy;
            if (gx[i] > max_gx) max_gx = gx[i];
            if (gy[i] > max_gy) max_gy = gy[i];
        }

        TilemapLevel *tm = &tmap->levels[li];
        memset(tm, 0, sizeof(*tm));
        strncpy(tm->name, lvl->name, sizeof(tm->name) - 1);
        tm->width = (max_gx + 1) * GRID_W;
        tm->height = (max_gy + 1) * GRID_H;
        if (tm->width > TMAP_MAX_W) tm->width = TMAP_MAX_W;
        if (tm->height > TMAP_MAX_H) tm->height = TMAP_MAX_H;

        /* Fill with solid */
        memset(tm->tiles, 1, sizeof(tm->tiles));

        for (int ri = 0; ri < lvl->room_count; ri++) {
            Room *room = &lvl->rooms[ri];
            int ox = gx[ri] * GRID_W, oy = gy[ri] * GRID_H;

            uint8_t grid[GRID_H][GRID_W];
            memset(grid, CELL_SOLID, sizeof(grid));
            for (int rri = 0; rri < 3; rri++) {
                int rt_idx = room->rows[rri];
                if (rt_idx < 0 || rt_idx >= proj.row_type_count) continue;
                RowType *rt = &proj.row_types[rt_idx];
                for (int pi = 0; pi < rt->cave_line_count; pi++) {
                    Polyline *pl = &rt->cave_lines[pi];
                    for (int j = 1; j < pl->count; j++)
                        tl(grid, pl->points[j-1].x, pl->points[j-1].y+yoff[rri],
                                 pl->points[j].x, pl->points[j].y+yoff[rri]);
                }
            }

            if (room->has_player_start) {
                ff(grid, tr(room->player_start.y), tc(room->player_start.x));
            } else {
                int found = 0;
                for (int r = 1; r < GRID_H - 1 && !found; r++) {
                    for (int c2 = 1; c2 < GRID_W - 1 && !found; c2++) {
                        if (grid[r][c2] == CELL_SOLID &&
                            (grid[r-1][c2] == CELL_BORDER || grid[r+1][c2] == CELL_BORDER ||
                             grid[r][c2-1] == CELL_BORDER || grid[r][c2+1] == CELL_BORDER)) {
                            ff(grid, r, c2);
                            found = 1;
                        }
                    }
                }
            }

            /* Copy tiles to tilemap */
            for (int r = 0; r < GRID_H; r++) {
                for (int c = 0; c < GRID_W; c++) {
                    int tx2 = ox + c, ty2 = oy + r;
                    if (tx2 >= tm->width || ty2 >= tm->height) continue;
                    switch (grid[r][c]) {
                    case CELL_EMPTY:  tm->tiles[ty2][tx2] = 0; break;
                    case CELL_BORDER: tm->tiles[ty2][tx2] = 2; break;
                    default:          tm->tiles[ty2][tx2] = 1; break;
                    }
                }
            }

            /* Extract entities from room data */
            if (room->has_player_start) {
                int etx = ox + tc(room->player_start.x);
                int ety = oy + tr(room->player_start.y);
                add_entity(tm, etx, ety, ENT_PLAYER_START, 0);
            }
            for (int ei = 0; ei < room->enemy_count; ei++) {
                Enemy *en = &room->enemies[ei];
                int etx = ox + tc(en->x);
                int ety = oy + tr(en->y);
                uint8_t etype;
                switch (en->type) {
                case ENEMY_BAT:    etype = ENT_BAT; break;
                case ENEMY_SPIDER: etype = ENT_SPIDER; break;
                case ENEMY_SNAKE:  etype = ENT_SNAKE; break;
                default:           etype = ENT_BAT; break;
                }
                add_entity(tm, etx, ety, etype, (int8_t)en->vx);
            }
            if (room->has_miner) {
                int etx = ox + tc(room->miner.x);
                int ety = oy + tr(room->miner.y);
                add_entity(tm, etx, ety, ENT_MINER, 0);
            }
        }

        printf("  Level %d: %s -> %dx%d tilemap (%d rooms, %d entities)\n",
               li, lvl->name, tm->width, tm->height, lvl->room_count, tm->entity_count);
    }

    printf("Converted %d levels\n", tmap->level_count);
}

/* ── Save project.json ────────────────────────────────────── */

static void save_project(TilemapProject *tmap, const char *path) {
    FILE *f = fopen(path, "w");
    if (!f) { fprintf(stderr, "Cannot write %s\n", path); return; }

    fprintf(f, "{\n");

    fprintf(f, "  \"tileset\": [\n");
    for (int i = 0; i < tmap->tileset.used_count; i++) {
        fprintf(f, "    [");
        for (int j = 0; j < 16; j++)
            fprintf(f, "%d%s", tmap->tileset.entries[i].data[j], j < 15 ? "," : "");
        fprintf(f, "]%s\n", i < tmap->tileset.used_count - 1 ? "," : "");
    }
    fprintf(f, "  ],\n");

    fprintf(f, "  \"bg_palettes\": [\n");
    for (int i = 0; i < MAX_BG_PALS; i++) {
        fprintf(f, "    [");
        for (int c = 0; c < COLORS_PER_PAL; c++) {
            RGB5 rgb = tmap->bg_pals[i].colors[c];
            fprintf(f, "[%d,%d,%d]%s", rgb.r, rgb.g, rgb.b, c < COLORS_PER_PAL - 1 ? "," : "");
        }
        fprintf(f, "]%s\n", i < MAX_BG_PALS - 1 ? "," : "");
    }
    fprintf(f, "  ],\n");
    fprintf(f, "  \"spr_palettes\": [\n");
    for (int i = 0; i < MAX_SPR_PALS; i++) {
        fprintf(f, "    [");
        for (int c = 0; c < COLORS_PER_PAL; c++) {
            RGB5 rgb = tmap->spr_pals[i].colors[c];
            fprintf(f, "[%d,%d,%d]%s", rgb.r, rgb.g, rgb.b, c < COLORS_PER_PAL - 1 ? "," : "");
        }
        fprintf(f, "]%s\n", i < MAX_SPR_PALS - 1 ? "," : "");
    }
    fprintf(f, "  ],\n");

    fprintf(f, "  \"levels\": [\n");
    for (int li = 0; li < tmap->level_count; li++) {
        TilemapLevel *lvl = &tmap->levels[li];
        fprintf(f, "    {\n");
        fprintf(f, "      \"name\": \"%s\",\n", lvl->name);
        fprintf(f, "      \"width\": %d,\n", lvl->width);
        fprintf(f, "      \"height\": %d,\n", lvl->height);
        fprintf(f, "      \"tiles\": [\n");
        for (int y = 0; y < lvl->height; y++) {
            fprintf(f, "        [");
            for (int x = 0; x < lvl->width; x++)
                fprintf(f, "%d%s", lvl->tiles[y][x], x < lvl->width - 1 ? "," : "");
            fprintf(f, "]%s\n", y < lvl->height - 1 ? "," : "");
        }
        fprintf(f, "      ],\n");
        fprintf(f, "      \"entities\": [\n");
        for (int i = 0; i < lvl->entity_count; i++) {
            TilemapEntity *e = &lvl->entities[i];
            fprintf(f, "        {\"x\":%d,\"y\":%d,\"type\":%d,\"vx\":%d}%s\n",
                    e->x, e->y, e->type, e->vx, i < lvl->entity_count - 1 ? "," : "");
        }
        fprintf(f, "      ]\n");
        fprintf(f, "    }%s\n", li < tmap->level_count - 1 ? "," : "");
    }
    fprintf(f, "  ]\n");

    fprintf(f, "}\n");
    fclose(f);
    printf("Saved: %s (%d tiles, %d levels)\n", path, tmap->tileset.used_count, tmap->level_count);
}

/* ── C Export: auto-tile + RLE ────────────────────────────── */

/* Auto-tile: convert raw tile index (0=empty, 1=solid, 2=border) to
   GBDK tile indices matching tiles.h:
   0 = TILE_EMPTY
   1-16 = TILE_WALL + neighbor mask (auto-tiled)
   Border tiles also use the wall auto-tile range since they look the same. */
static void autotile_level(TilemapLevel *lvl, uint8_t out[256][256]) {
    for (int y = 0; y < lvl->height; y++) {
        for (int x = 0; x < lvl->width; x++) {
            uint8_t t = lvl->tiles[y][x];
            if (t == 0) {
                out[y][x] = 0; /* TILE_EMPTY */
            } else {
                /* Compute 4-bit neighbor mask */
                uint8_t mask = 0;
                uint8_t up    = (y > 0)               ? lvl->tiles[y-1][x] : 1;
                uint8_t right = (x < lvl->width - 1)  ? lvl->tiles[y][x+1] : 1;
                uint8_t down  = (y < lvl->height - 1) ? lvl->tiles[y+1][x] : 1;
                uint8_t left  = (x > 0)               ? lvl->tiles[y][x-1] : 1;
                if (up)    mask |= 0x01;
                if (right) mask |= 0x02;
                if (down)  mask |= 0x04;
                if (left)  mask |= 0x08;
                out[y][x] = 1 + mask; /* TILE_WALL(1) + mask(0..15) */
            }
        }
    }
}

static void export_c(TilemapProject *tmap, const char *c_path, const char *h_path) {
    FILE *fc = fopen(c_path, "w");
    FILE *fh = fopen(h_path, "w");
    if (!fc || !fh) {
        fprintf(stderr, "Cannot write output files\n");
        if (fc) fclose(fc); if (fh) fclose(fh);
        return;
    }

    /* Header file */
    fprintf(fh, "// level_data.h — Generated by convert tool\n");
    fprintf(fh, "#ifndef LEVEL_DATA_H\n#define LEVEL_DATA_H\n\n");
    fprintf(fh, "#include <stdint.h>\n\n");
    fprintf(fh, "#define NUM_LEVELS %d\n\n", tmap->level_count);
    fprintf(fh, "typedef struct {\n");
    fprintf(fh, "    uint8_t width, height;\n");
    fprintf(fh, "    uint8_t bank;\n");
    fprintf(fh, "    const uint8_t *rle;\n");
    fprintf(fh, "    const uint16_t *row_offsets;\n");
    fprintf(fh, "    uint8_t entity_count;\n");
    fprintf(fh, "    const uint8_t *entities;\n");
    fprintf(fh, "} LevelInfo;\n\n");
    fprintf(fh, "extern const LevelInfo levels[NUM_LEVELS];\n\n");

    fprintf(fh, "#include <gb/gb.h>\n\n");
    for (int li = 0; li < tmap->level_count; li++) {
        fprintf(fh, "extern const uint8_t level_%d_rle[];\n", li);
        fprintf(fh, "extern const uint16_t level_%d_row_offsets[];\n", li);
        fprintf(fh, "extern const uint8_t level_%d_entities[];\n", li);
    }
    fprintf(fh, "\n#endif\n");
    fclose(fh);

    /* C source file */
    fprintf(fc, "// level_data.c — Generated by convert tool\n");
    fprintf(fc, "// Do not edit — regenerate with: ./convert --c-export project.json level_data.c level_data.h\n\n");
    fprintf(fc, "#include \"level_data.h\"\n");
    fprintf(fc, "#include <gb/gb.h>\n\n");

    /* Decide banking: pack levels into banks starting at bank 1 */
    int cur_bank = 1;
    int bank_used = 0;
    int level_banks[20];
    #define BANK_SIZE 14000  /* leave headroom in 16KB bank for linker overhead */

    /* First pass: compute bank assignments */
    static uint8_t autotiled[256][256];
    static uint8_t rle_bufs[20][65536];
    static uint16_t row_offs_bufs[20][256];
    static int rle_sizes[20];

    for (int li = 0; li < tmap->level_count; li++) {
        TilemapLevel *lvl = &tmap->levels[li];
        autotile_level(lvl, autotiled);
        int rle_pos = 0;
        for (int y = 0; y < lvl->height; y++) {
            row_offs_bufs[li][y] = (uint16_t)rle_pos;
            int x = 0;
            while (x < lvl->width) {
                uint8_t t = autotiled[y][x];
                int run = 1;
                while (x + run < lvl->width && autotiled[y][x + run] == t && run < 255) run++;
                rle_bufs[li][rle_pos++] = (uint8_t)run;
                rle_bufs[li][rle_pos++] = t;
                x += run;
            }
        }
        rle_sizes[li] = rle_pos;

        int level_size = rle_pos + lvl->height * 2 + lvl->entity_count * 4 + 16;
        if (bank_used + level_size > BANK_SIZE && bank_used > 0) {
            cur_bank++;
            bank_used = 0;
        }
        level_banks[li] = cur_bank;
        bank_used += level_size;
    }

    /* Output banked data — all in one bank for simplicity */
    fprintf(fc, "#pragma bank 1\n\n");
    for (int li = 0; li < tmap->level_count; li++) {
        TilemapLevel *lvl = &tmap->levels[li];
        int rle_pos = rle_sizes[li];

        fprintf(fc, "BANKREF(level_%d_rle)\n", li);
        fprintf(fc, "const uint8_t level_%d_rle[] = {\n    ", li);
        for (int i = 0; i < rle_pos; i++) {
            fprintf(fc, "0x%02X%s", rle_bufs[li][i], i < rle_pos - 1 ? "," : "");
            if ((i & 15) == 15) fprintf(fc, "\n    ");
        }
        fprintf(fc, "\n};\n");

        /* Row offset table */
        fprintf(fc, "BANKREF(level_%d_row_offsets)\n", li);
        fprintf(fc, "const uint16_t level_%d_row_offsets[] = {\n    ", li);
        for (int y = 0; y < lvl->height; y++) {
            fprintf(fc, "%d%s", row_offs_bufs[li][y], y < lvl->height - 1 ? "," : "");
            if ((y & 15) == 15) fprintf(fc, "\n    ");
        }
        fprintf(fc, "\n};\n");

        /* Entity data: packed as [tx, ty, type, vx] */
        fprintf(fc, "BANKREF(level_%d_entities)\n", li);
        fprintf(fc, "const uint8_t level_%d_entities[] = {\n    ", li);
        for (int i = 0; i < lvl->entity_count; i++) {
            TilemapEntity *e = &lvl->entities[i];
            fprintf(fc, "%d,%d,%d,%d%s", e->x, e->y, e->type, (uint8_t)e->vx,
                    i < lvl->entity_count - 1 ? ", " : "");
        }
        if (lvl->entity_count == 0) fprintf(fc, "0"); /* avoid empty array */
        fprintf(fc, "\n};\n");

        printf("  Level %d: %d bytes RLE, %d entities, bank %d\n",
               li, rle_pos, lvl->entity_count, level_banks[li]);
    }
    fclose(fc);

    /* Level info table — in a separate non-banked file */
    {
        /* Write levels_meta.c — goes into fixed ROM (no #pragma bank) */
        char meta_path[512];
        strncpy(meta_path, c_path, sizeof(meta_path) - 10);
        char *dot = strrchr(meta_path, '/');
        if (dot) snprintf(dot + 1, sizeof(meta_path) - (dot + 1 - meta_path), "levels_meta.c");
        else strcpy(meta_path, "levels_meta.c");

        FILE *fm = fopen(meta_path, "w");
        if (fm) {
            fprintf(fm, "// levels_meta.c — Generated, fixed ROM\n");
            fprintf(fm, "#include \"level_data.h\"\n\n");
            fprintf(fm, "const LevelInfo levels[NUM_LEVELS] = {\n");
            for (int li = 0; li < tmap->level_count; li++) {
                TilemapLevel *lvl2 = &tmap->levels[li];
                fprintf(fm, "    {%d, %d, 1, level_%d_rle, level_%d_row_offsets, %d, level_%d_entities}%s\n",
                        lvl2->width, lvl2->height, li, li, lvl2->entity_count, li,
                        li < tmap->level_count - 1 ? "," : "");
            }
            fprintf(fm, "};\n");
            fclose(fm);
            printf("  levels_meta.c written\n");
        }
    }

    printf("Exported %d levels to %s + %s\n",
           tmap->level_count, c_path, h_path);
}

/* ── Load project.json (for --c-export mode) ──────────────── */

static TilemapProject *load_project(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) { fprintf(stderr, "Cannot open %s\n", path); return NULL; }
    fseek(f, 0, SEEK_END); long len = ftell(f); fseek(f, 0, SEEK_SET);
    char *json = malloc(len + 1); fread(json, 1, len, f); json[len] = 0; fclose(f);

    TilemapProject *tmap = calloc(1, sizeof(TilemapProject));
    tilemap_project_init(tmap);

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
                tmap->tileset.entries[idx].data[j] = (uint8_t)v;
            }
            if (*p == ']') p++;
            idx++;
        }
        tmap->tileset.used_count = idx;
    }

    /* Parse levels */
    p = strstr(json, "\"levels\"");
    if (p) {
        p = strchr(p, '['); if (p) p++;
        tmap->level_count = 0;
        while (p && *p && tmap->level_count < TMAP_MAX_LEVELS) {
            p = json_skip_ws(p);
            if (*p == ']') break;
            if (*p == ',') { p++; continue; }
            if (*p != '{') break;
            p++;

            TilemapLevel *lvl = &tmap->levels[tmap->level_count];
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
                        int col2 = 0;
                        while (*p && *p != ']' && col2 < TMAP_MAX_W) {
                            p = json_skip_ws(p);
                            if (*p == ',') { p++; continue; }
                            if (*p == ']') break;
                            int v = 0; p = json_parse_int(p, &v);
                            lvl->tiles[row][col2++] = (uint8_t)v;
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
            tmap->level_count++;
        }
    }

    free(json);
    return tmap;
}

/* ── Main ─────────────────────────────────────────────────── */

int main(int argc, char *argv[]) {
    if (argc >= 4 && strcmp(argv[1], "--c-export") == 0) {
        /* Mode 2: project.json → C source */
        TilemapProject *tmap = load_project(argv[2]);
        if (!tmap) return 1;
        export_c(tmap, argv[3], argc > 4 ? argv[4] : "level_data.h");
        free(tmap);
        return 0;
    }

    if (argc < 3) {
        fprintf(stderr, "Usage:\n");
        fprintf(stderr, "  %s <hero.json> <project.json>\n", argv[0]);
        fprintf(stderr, "  %s --c-export <project.json> <level_data.c> [level_data.h]\n", argv[0]);
        return 1;
    }

    /* Mode 1: hero.json → project.json */
    FILE *f = fopen(argv[1], "r");
    if (!f) { fprintf(stderr, "Cannot open %s\n", argv[1]); return 1; }
    fseek(f, 0, SEEK_END); long len = ftell(f); fseek(f, 0, SEEK_SET);
    char *json = malloc(len + 1);
    fread(json, 1, len, f);
    json[len] = 0;
    fclose(f);

    TilemapProject *tmap = calloc(1, sizeof(TilemapProject));
    tilemap_project_init(tmap);

    convert_levels(json, tmap);
    free(json);

    save_project(tmap, argv[2]);
    free(tmap);

    return 0;
}
