/*
 * panel_level_list.c — Level browser, room browser, room properties (left panel)
 *
 * Layout:
 *   Level: "Level 3 / 20"  [<] [>] [+] [-] [Copy]
 *   Room:  "Room 2 / 3"    [<] [>] [+] [-] [Copy]
 *   Room Exits: Left/Right/Top/Bottom fields
 *   Has Lava checkbox
 */

#include "app.h"
#include "ui.h"
#include <stdio.h>
#include <string.h>

static bool exit_l_focus = false, exit_r_focus = false;
static bool exit_t_focus = false, exit_b_focus = false;
static char exit_l_buf[8] = "", exit_r_buf[8] = "";
static char exit_t_buf[8] = "", exit_b_buf[8] = "";
static int prev_level = -1, prev_room = -1;

static void sync_exit_bufs(Room *room) {
    if (room->exit_left >= 0)  snprintf(exit_l_buf, sizeof(exit_l_buf), "%d", room->exit_left);
    else exit_l_buf[0] = 0;
    if (room->exit_right >= 0) snprintf(exit_r_buf, sizeof(exit_r_buf), "%d", room->exit_right);
    else exit_r_buf[0] = 0;
    if (room->exit_top >= 0)   snprintf(exit_t_buf, sizeof(exit_t_buf), "%d", room->exit_top);
    else exit_t_buf[0] = 0;
    if (room->exit_bottom >= 0) snprintf(exit_b_buf, sizeof(exit_b_buf), "%d", room->exit_bottom);
    else exit_b_buf[0] = 0;
}

static int parse_exit(const char *buf) {
    if (!buf[0]) return -1;
    return atoi(buf);
}

void draw_level_list(App *app, int px, int py, int pw, int ph) {
    ui_panel_begin("", px, py, pw, ph);
    SDL_Rect c = ui_panel_content();
    int y = c.y;
    int bw = 30, gap = 3;
    Project *proj = &app->level_project;

    /* ── Level browser ── */
    y = ui_section(c.x - 4, y, c.w + 8, "Level");

    Level *lvl = (app->cur_level >= 0 && app->cur_level < proj->level_count)
                 ? &proj->levels[app->cur_level] : NULL;

    char label[64];
    snprintf(label, sizeof(label), "%s (%d / %d)",
             lvl ? lvl->name : "—", app->cur_level + 1, proj->level_count);
    ui_text(c.x, y, label);
    y += ui_line_height() + 4;

    int bx = c.x;
    if (ui_button(bx, y, bw, ui_line_height() + 4, "<")) {
        if (app->cur_level > 0) { app->cur_level--; app->cur_room = 0; }
    }
    bx += bw + gap;
    if (ui_button(bx, y, bw, ui_line_height() + 4, ">")) {
        if (app->cur_level < proj->level_count - 1) { app->cur_level++; app->cur_room = 0; }
    }
    bx += bw + gap;
    if (ui_button(bx, y, bw, ui_line_height() + 4, "+")) {
        if (proj->level_count < MAX_LEVELS) {
            Level *nl = &proj->levels[proj->level_count];
            memset(nl, 0, sizeof(*nl));
            snprintf(nl->name, sizeof(nl->name), "Level %d", proj->level_count + 1);
            nl->room_count = 1;
            nl->rooms[0].number = 1;
            nl->rooms[0].exit_left = nl->rooms[0].exit_right = -1;
            nl->rooms[0].exit_top = nl->rooms[0].exit_bottom = -1;
            app->cur_level = proj->level_count;
            app->cur_room = 0;
            proj->level_count++;
            app->modified = true;
        }
    }
    bx += bw + gap;
    if (ui_button(bx, y, bw, ui_line_height() + 4, "-")) {
        if (proj->level_count > 1) {
            for (int i = app->cur_level; i < proj->level_count - 1; i++)
                proj->levels[i] = proj->levels[i + 1];
            proj->level_count--;
            if (app->cur_level >= proj->level_count) app->cur_level = proj->level_count - 1;
            app->cur_room = 0;
            app->modified = true;
        }
    }
    bx += bw + gap;
    if (ui_button(bx, y, 40, ui_line_height() + 4, "Copy")) {
        if (proj->level_count < MAX_LEVELS && lvl) {
            proj->levels[proj->level_count] = *lvl;
            snprintf(proj->levels[proj->level_count].name, 64, "%s copy", lvl->name);
            app->cur_level = proj->level_count;
            app->cur_room = 0;
            proj->level_count++;
            app->modified = true;
        }
    }
    y += ui_line_height() + 12;

    /* Refresh lvl pointer after possible changes */
    lvl = (app->cur_level >= 0 && app->cur_level < proj->level_count)
          ? &proj->levels[app->cur_level] : NULL;

    /* ── Room browser ── */
    y = ui_section(c.x - 4, y, c.w + 8, "Room");

    Room *room = NULL;
    if (lvl && app->cur_room >= 0 && app->cur_room < lvl->room_count)
        room = &lvl->rooms[app->cur_room];

    snprintf(label, sizeof(label), "Room %d / %d",
             app->cur_room + 1, lvl ? lvl->room_count : 0);
    ui_text(c.x, y, label);
    y += ui_line_height() + 4;

    bx = c.x;
    if (ui_button(bx, y, bw, ui_line_height() + 4, "<")) {
        if (app->cur_room > 0) app->cur_room--;
    }
    bx += bw + gap;
    if (ui_button(bx, y, bw, ui_line_height() + 4, ">")) {
        if (lvl && app->cur_room < lvl->room_count - 1) app->cur_room++;
    }
    bx += bw + gap;
    if (ui_button(bx, y, bw, ui_line_height() + 4, "+")) {
        if (lvl && lvl->room_count < MAX_ROOMS) {
            Room *nr = &lvl->rooms[lvl->room_count];
            memset(nr, 0, sizeof(*nr));
            nr->number = lvl->room_count + 1;
            nr->exit_left = nr->exit_right = nr->exit_top = nr->exit_bottom = -1;
            app->cur_room = lvl->room_count;
            lvl->room_count++;
            app->modified = true;
        }
    }
    bx += bw + gap;
    if (ui_button(bx, y, bw, ui_line_height() + 4, "-")) {
        if (lvl && lvl->room_count > 1) {
            for (int i = app->cur_room; i < lvl->room_count - 1; i++)
                lvl->rooms[i] = lvl->rooms[i + 1];
            lvl->room_count--;
            if (app->cur_room >= lvl->room_count) app->cur_room = lvl->room_count - 1;
            app->modified = true;
        }
    }
    bx += bw + gap;
    if (ui_button(bx, y, 40, ui_line_height() + 4, "Copy")) {
        if (lvl && lvl->room_count < MAX_ROOMS && room) {
            lvl->rooms[lvl->room_count] = *room;
            lvl->rooms[lvl->room_count].number = lvl->room_count + 1;
            app->cur_room = lvl->room_count;
            lvl->room_count++;
            app->modified = true;
        }
    }
    y += ui_line_height() + 12;

    /* Refresh room pointer */
    room = NULL;
    if (lvl && app->cur_room >= 0 && app->cur_room < lvl->room_count)
        room = &lvl->rooms[app->cur_room];

    /* Sync exit buffers when level/room changes */
    if (app->cur_level != prev_level || app->cur_room != prev_room) {
        if (room) sync_exit_bufs(room);
        prev_level = app->cur_level;
        prev_room = app->cur_room;
    }

    /* ── Room Properties ── */
    if (room) {
        y = ui_section(c.x - 4, y, c.w + 8, "Room Exits");

        int lw = ui_text_width("Bottom:");
        int fx = c.x + lw + 8;
        int fw = c.w - lw - 8;

        ui_text_color(c.x, y + 3, "Left:", ui_theme.text_dim);
        if (!exit_l_focus) { if (room->exit_left >= 0) snprintf(exit_l_buf, 8, "%d", room->exit_left); else exit_l_buf[0] = 0; }
        if (ui_input_text(fx, y, fw, exit_l_buf, sizeof(exit_l_buf), &exit_l_focus))
            { room->exit_left = parse_exit(exit_l_buf); app->modified = true; }
        y += ui_line_height() + 6;

        ui_text_color(c.x, y + 3, "Right:", ui_theme.text_dim);
        if (!exit_r_focus) { if (room->exit_right >= 0) snprintf(exit_r_buf, 8, "%d", room->exit_right); else exit_r_buf[0] = 0; }
        if (ui_input_text(fx, y, fw, exit_r_buf, sizeof(exit_r_buf), &exit_r_focus))
            { room->exit_right = parse_exit(exit_r_buf); app->modified = true; }
        y += ui_line_height() + 6;

        ui_text_color(c.x, y + 3, "Top:", ui_theme.text_dim);
        if (!exit_t_focus) { if (room->exit_top >= 0) snprintf(exit_t_buf, 8, "%d", room->exit_top); else exit_t_buf[0] = 0; }
        if (ui_input_text(fx, y, fw, exit_t_buf, sizeof(exit_t_buf), &exit_t_focus))
            { room->exit_top = parse_exit(exit_t_buf); app->modified = true; }
        y += ui_line_height() + 6;

        ui_text_color(c.x, y + 3, "Bottom:", ui_theme.text_dim);
        if (!exit_b_focus) { if (room->exit_bottom >= 0) snprintf(exit_b_buf, 8, "%d", room->exit_bottom); else exit_b_buf[0] = 0; }
        if (ui_input_text(fx, y, fw, exit_b_buf, sizeof(exit_b_buf), &exit_b_focus))
            { room->exit_bottom = parse_exit(exit_b_buf); app->modified = true; }
        y += ui_line_height() + 10;

        /* Lava */
        if (ui_checkbox(c.x, y, "Has Lava", &room->has_lava))
            app->modified = true;
    }

    ui_panel_end();
}
