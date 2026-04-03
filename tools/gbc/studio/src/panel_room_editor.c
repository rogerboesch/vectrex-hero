/*
 * panel_room_editor.c — GBC room editor with tile-based rendering
 *
 * Rasterizes cave polylines onto a 20x16 tile grid, flood fills,
 * then renders each cell as a colored tile matching GBC palette style.
 */

#include "app.h"
#include "ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GRID_W 20
#define GRID_H 16
#define CELL_EMPTY  0
#define CELL_BORDER 1
#define CELL_SOLID  2

/* GBC palette-matched colors */
static const SDL_Color COL_ROCK    = {130, 100,  65, 255};
static const SDL_Color COL_BORDER  = {195, 160, 130, 255};
static const SDL_Color COL_EMPTY   = { 15,  15,  25, 255};
static const SDL_Color COL_WALL    = {195, 160,  30, 255};
static const SDL_Color COL_WALL_D  = {250, 225,  65, 255};
static const SDL_Color COL_LAVA    = {250,  80,   0, 255};
static const SDL_Color COL_ENEMY   = {250,  65,  30, 255};
static const SDL_Color COL_SPIDER  = {210,  50, 210, 255};
static const SDL_Color COL_SNAKE   = { 50, 200,  50, 255};
static const SDL_Color COL_MINER   = {  0, 200, 200, 255};
static const SDL_Color COL_PLAYER  = { 65, 250,  65, 255};
static const SDL_Color COL_SELECT  = {255, 255,   0, 255};

static int tile_col(int gx) { return (int)(((unsigned)((unsigned char)(gx + 128)) * 20) >> 8); }
static int tile_row(int gy) { int v = (int)(((unsigned)(50 - gy) * 39) >> 8); return v < 0 ? 0 : v >= GRID_H ? GRID_H-1 : v; }

static void trace_line(uint8_t grid[GRID_H][GRID_W], int gx1, int gy1, int gx2, int gy2) {
    int c1 = tile_col(gx1), r1 = tile_row(gy1), c2 = tile_col(gx2), r2 = tile_row(gy2);
    int dx = abs(c2-c1), dy = abs(r2-r1), sx = c1<c2?1:-1, sy = r1<r2?1:-1, err = dx-dy;
    for (;;) {
        if (r1>=0 && r1<GRID_H && c1>=0 && c1<GRID_W) grid[r1][c1] = CELL_BORDER;
        if (c1==c2 && r1==r2) break;
        int e2 = 2*err;
        if (e2 > -dy) { err -= dy; c1 += sx; }
        if (e2 < dx)  { err += dx; r1 += sy; }
    }
}

static void flood_fill(uint8_t grid[GRID_H][GRID_W], int sr, int sc) {
    if (sr<0||sr>=GRID_H||sc<0||sc>=GRID_W) return;
    grid[sr][sc] = CELL_EMPTY;
    int changed = 1;
    while (changed) { changed = 0;
        for (int r=0; r<GRID_H; r++) for (int c=0; c<GRID_W; c++) {
            if (grid[r][c]!=CELL_SOLID) continue;
            if ((r>0&&grid[r-1][c]==CELL_EMPTY)||(r<GRID_H-1&&grid[r+1][c]==CELL_EMPTY)||
                (c>0&&grid[r][c-1]==CELL_EMPTY)||(c<GRID_W-1&&grid[r][c+1]==CELL_EMPTY))
                { grid[r][c]=CELL_EMPTY; changed=1; }
        }
    }
}

static void build_grid(App *app, Room *room, uint8_t grid[GRID_H][GRID_W]) {
    memset(grid, CELL_SOLID, GRID_H * GRID_W);
    int yoff[] = {20, -10, -40};
    for (int ri = 0; ri < 3; ri++) {
        int rt_idx = room->rows[ri];
        if (rt_idx < 0 || rt_idx >= app->level_project.row_type_count) continue;
        RowType *rt = &app->level_project.row_types[rt_idx];
        for (int pi = 0; pi < rt->cave_line_count; pi++) {
            Polyline *pl = &rt->cave_lines[pi];
            for (int j = 1; j < pl->count; j++)
                trace_line(grid, pl->points[j-1].x, pl->points[j-1].y + yoff[ri],
                                 pl->points[j].x,   pl->points[j].y + yoff[ri]);
        }
    }
    if (room->has_player_start)
        flood_fill(grid, tile_row(room->player_start.y), tile_col(room->player_start.x));
}

static int sel_type = -1, sel_idx = -1;

void draw_room_editor(App *app, int px, int py, int pw, int ph) {
    SDL_Rect tb = ui_panel_begin_toolbar(px, py, pw, ph);
    SDL_Rect c = ui_panel_content();

    const char *tnames[] = {"Select","Wall","Bat","Spider","Snake","Miner","Player"};
    char lbl[32]; snprintf(lbl, sizeof(lbl), "Tool: %s", tnames[app->room_tool]);
    ui_text_color(tb.x, tb.y+2, lbl, ui_theme.text_dim);

    if (app->cur_level<0||app->cur_level>=app->level_project.level_count) { ui_panel_end(); return; }
    Level *lvl = &app->level_project.levels[app->cur_level];
    if (app->cur_room<0||app->cur_room>=lvl->room_count) { ui_panel_end(); return; }
    Room *room = &lvl->rooms[app->cur_room];

    int margin=10, cx=c.x+margin, cy=c.y+margin, cw=c.w-margin*2, ch=c.h-margin*2;
    int cell_w=cw/GRID_W, cell_h=ch/GRID_H;
    if (cell_w<2) cell_w=2; if (cell_h<2) cell_h=2;
    int tw=GRID_W*cell_w, th=GRID_H*cell_h;
    int ox=cx+(cw-tw)/2, oy=cy+(ch-th)/2;

    uint8_t grid[GRID_H][GRID_W];
    build_grid(app, room, grid);

    /* Draw tile cells */
    for (int r=0; r<GRID_H; r++) for (int col=0; col<GRID_W; col++) {
        SDL_Color tc = grid[r][col]==CELL_EMPTY ? COL_EMPTY : grid[r][col]==CELL_BORDER ? COL_BORDER : COL_ROCK;
        SDL_Rect tr = {ox+col*cell_w, oy+r*cell_h, cell_w, cell_h};
        SDL_SetRenderDrawColor(app->renderer, tc.r, tc.g, tc.b, 255);
        SDL_RenderFillRect(app->renderer, &tr);
    }

    /* Walls */
    for (int i=0; i<room->wall_count; i++) {
        Wall *w = &room->walls[i];
        int c1=tile_col(w->x), r1=tile_row(w->y), c2=tile_col(w->x+w->w), r2=tile_row(w->y-w->h);
        SDL_Color wc = (sel_type==0&&sel_idx==i)?COL_SELECT : w->destroyable?COL_WALL_D:COL_WALL;
        for (int r=r1; r<=r2&&r<GRID_H; r++) for (int cc=c1; cc<=c2&&cc<GRID_W; cc++) {
            SDL_Rect wr={ox+cc*cell_w,oy+r*cell_h,cell_w,cell_h};
            SDL_SetRenderDrawColor(app->renderer,wc.r,wc.g,wc.b,255);
            SDL_RenderFillRect(app->renderer,&wr);
        }
    }

    /* Lava */
    if (room->has_lava) {
        int lr=tile_row(-40);
        for (int r=lr; r<GRID_H; r++) for (int col=0; col<GRID_W; col++) {
            SDL_Rect lr2={ox+col*cell_w,oy+r*cell_h,cell_w,cell_h};
            SDL_SetRenderDrawColor(app->renderer,COL_LAVA.r,COL_LAVA.g,COL_LAVA.b,200);
            SDL_RenderFillRect(app->renderer,&lr2);
        }
    }

    /* Enemies */
    for (int i=0; i<room->enemy_count; i++) {
        Enemy *e=&room->enemies[i]; int ec=tile_col(e->x), er=tile_row(e->y);
        SDL_Color ec2=(sel_type==1&&sel_idx==i)?COL_SELECT:e->type==ENEMY_SPIDER?COL_SPIDER:e->type==ENEMY_SNAKE?COL_SNAKE:COL_ENEMY;
        if (er>=0&&er<GRID_H&&ec>=0&&ec<GRID_W) {
            SDL_Rect er2={ox+ec*cell_w,oy+er*cell_h,cell_w,cell_h};
            SDL_SetRenderDrawColor(app->renderer,ec2.r,ec2.g,ec2.b,255);
            SDL_RenderFillRect(app->renderer,&er2);
        }
    }

    /* Miner */
    if (room->has_miner) {
        int mc=tile_col(room->miner.x),mr=tile_row(room->miner.y);
        SDL_Color mc2=sel_type==2?COL_SELECT:COL_MINER;
        if (mr>=0&&mr<GRID_H&&mc>=0&&mc<GRID_W) {
            SDL_Rect mr2={ox+mc*cell_w,oy+mr*cell_h,cell_w,cell_h};
            SDL_SetRenderDrawColor(app->renderer,mc2.r,mc2.g,mc2.b,255);
            SDL_RenderFillRect(app->renderer,&mr2);
        }
    }

    /* Player start */
    if (room->has_player_start) {
        int pc=tile_col(room->player_start.x),pr=tile_row(room->player_start.y);
        SDL_Color pc2=sel_type==3?COL_SELECT:COL_PLAYER;
        if (pr>=0&&pr<GRID_H&&pc>=0&&pc<GRID_W) {
            SDL_Rect pr2={ox+pc*cell_w,oy+pr*cell_h,cell_w,cell_h};
            SDL_SetRenderDrawColor(app->renderer,pc2.r,pc2.g,pc2.b,255);
            SDL_RenderFillRect(app->renderer,&pr2);
        }
    }

    /* Grid lines */
    SDL_SetRenderDrawColor(app->renderer,30,30,40,255);
    for (int col=0; col<=GRID_W; col++) SDL_RenderDrawLine(app->renderer,ox+col*cell_w,oy,ox+col*cell_w,oy+th);
    for (int r=0; r<=GRID_H; r++) SDL_RenderDrawLine(app->renderer,ox,oy+r*cell_h,ox+tw,oy+r*cell_h);

    /* Tooltip */
    if (ui_mouse_in_rect(ox,oy,tw,th)) {
        int mx,my; ui_mouse_pos(&mx,&my);
        int gc=(mx-ox)/cell_w, gr=(my-oy)/cell_h;
        int vx=(gc*256/20)-128, vy=50-(gr*100/16);
        char tip[32]; snprintf(tip,sizeof(tip),"(%d,%d) [%d,%d]",vx,vy,gc,gr);
        ui_tooltip(tip);

        if (ui_mouse_clicked() && app->room_tool != TOOL_SELECT) {
            switch (app->room_tool) {
            case TOOL_WALL: if (room->wall_count<MAX_WALLS) { room->walls[room->wall_count++]=(Wall){vy,vx,10,20,false}; app->modified=true; } break;
            case TOOL_ENEMY_BAT: case TOOL_ENEMY_SPIDER: case TOOL_ENEMY_SNAKE:
                if (room->enemy_count<MAX_ENEMIES) { room->enemies[room->enemy_count++]=(Enemy){vx,vy,1,app->room_tool-TOOL_ENEMY_BAT}; app->modified=true; } break;
            case TOOL_MINER: room->miner=(Point){vx,vy}; room->has_miner=true; app->modified=true; break;
            case TOOL_PLAYER_START: room->player_start=(Point){vx,vy}; room->has_player_start=true; app->modified=true; break;
            default: break;
            }
        }
    }

    ui_panel_end();
}
