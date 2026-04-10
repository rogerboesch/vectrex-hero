/*
 * panel_sprite_tools.c — Sprite/frame browser + tools (left panel for Sprite Editor)
 */

#include "app.h"
#include "ui.h"
#include <stdio.h>
#include <string.h>

void draw_sprite_tools(App *app, int px, int py, int pw, int ph) {
    ui_panel_begin("Sprites", px, py, pw, ph);
    SDL_Rect c = ui_panel_content();
    int y = c.y;
    int bw = 30, gap = 3;

    /* ── Sprite browser ── */
    y = ui_section(c.x - 4, y, c.w + 8, "Sprite");

    VecSprite *spr = (app->cur_sprite >= 0 && app->cur_sprite < app->project.sprite_count)
                     ? &app->project.sprites[app->cur_sprite] : NULL;
    char label[64];
    snprintf(label, sizeof(label), "%s (%d / %d)",
             spr ? spr->name : "—", app->cur_sprite + 1, app->project.sprite_count);
    ui_text(c.x, y, label);
    y += ui_line_height() + 4;

    int bx = c.x;
    if (ui_button(bx, y, bw, ui_line_height() + 4, "<")) {
        if (app->cur_sprite > 0) { app->cur_sprite--; app->cur_frame = 0; }
    }
    bx += bw + gap;
    if (ui_button(bx, y, bw, ui_line_height() + 4, ">")) {
        if (app->cur_sprite < app->project.sprite_count - 1) { app->cur_sprite++; app->cur_frame = 0; }
    }
    bx += bw + gap;
    if (ui_button(bx, y, bw, ui_line_height() + 4, "+")) {
        if (app->project.sprite_count < MAX_SPRITES) {
            VecSprite *ns = &app->project.sprites[app->project.sprite_count];
            memset(ns, 0, sizeof(*ns));
            snprintf(ns->name, sizeof(ns->name), "sprite_%d", app->project.sprite_count);
            ns->frame_count = 1;
            app->cur_sprite = app->project.sprite_count;
            app->cur_frame = 0;
            app->project.sprite_count++;
            app->modified = true;
        }
    }
    bx += bw + gap;
    if (ui_button(bx, y, bw, ui_line_height() + 4, "-")) {
        if (app->project.sprite_count > 1) {
            for (int i = app->cur_sprite; i < app->project.sprite_count - 1; i++)
                app->project.sprites[i] = app->project.sprites[i + 1];
            app->project.sprite_count--;
            if (app->cur_sprite >= app->project.sprite_count)
                app->cur_sprite = app->project.sprite_count - 1;
            app->cur_frame = 0;
            app->modified = true;
        }
    }
    y += ui_line_height() + 12;

    /* ── Frame browser ── */
    y = ui_section(c.x - 4, y, c.w + 8, "Frame");

    spr = (app->cur_sprite >= 0 && app->cur_sprite < app->project.sprite_count)
          ? &app->project.sprites[app->cur_sprite] : NULL;
    snprintf(label, sizeof(label), "Frame %d / %d",
             app->cur_frame + 1, spr ? spr->frame_count : 0);
    ui_text(c.x, y, label);
    y += ui_line_height() + 4;

    bx = c.x;
    if (ui_button(bx, y, bw, ui_line_height() + 4, "<")) {
        if (app->cur_frame > 0) app->cur_frame--;
    }
    bx += bw + gap;
    if (ui_button(bx, y, bw, ui_line_height() + 4, ">")) {
        if (spr && app->cur_frame < spr->frame_count - 1) app->cur_frame++;
    }
    bx += bw + gap;
    if (ui_button(bx, y, bw, ui_line_height() + 4, "+")) {
        if (spr && spr->frame_count < MAX_FRAMES) {
            spr->frames[spr->frame_count].count = 0;
            app->cur_frame = spr->frame_count;
            spr->frame_count++;
            app->modified = true;
        }
    }
    bx += bw + gap;
    if (ui_button(bx, y, bw, ui_line_height() + 4, "-")) {
        if (spr && spr->frame_count > 1) {
            for (int i = app->cur_frame; i < spr->frame_count - 1; i++)
                spr->frames[i] = spr->frames[i + 1];
            spr->frame_count--;
            if (app->cur_frame >= spr->frame_count)
                app->cur_frame = spr->frame_count - 1;
            app->modified = true;
        }
    }
    bx += bw + gap;
    if (ui_button(bx, y, 40, ui_line_height() + 4, "Copy")) {
        if (spr && spr->frame_count < MAX_FRAMES) {
            SpriteFrame *src = &spr->frames[app->cur_frame];
            spr->frames[spr->frame_count] = *src;
            app->cur_frame = spr->frame_count;
            spr->frame_count++;
            app->modified = true;
        }
    }
    y += ui_line_height() + 12;

    /* ── Help ── */
    y = ui_section(c.x - 4, y, c.w + 8, "Help");
    ui_text_small(c.x, y, "D: Draw mode"); y += ui_line_height();
    ui_text_small(c.x, y, "S: Select mode"); y += ui_line_height();
    ui_text_small(c.x, y, "Draw: click to add"); y += ui_line_height();
    ui_text_small(c.x, y, "Right-click: undo"); y += ui_line_height();
    ui_text_small(c.x, y, "Select: drag point"); y += ui_line_height();
    ui_text_small(c.x, y, "Delete: remove point");

    ui_panel_end();
}
