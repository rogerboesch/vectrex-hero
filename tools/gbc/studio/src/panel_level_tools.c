/*
 * panel_level_tools.c — Level management tools (right panel)
 *
 * Add/remove/copy/prev/next level, clear, move (scroll controls).
 */
#include "app.h"
#include "ui.h"
#include <stdio.h>
#include <string.h>

void draw_level_tools(App *app, int px, int py, int pw, int ph) {
    ui_panel_begin_toolbar(px, py, pw, ph);
    SDL_Rect c = ui_panel_content();
    int y = c.y;
    int bw = 30, gap = 3, bh = ui_line_height() + 4;

    /* Level browser — section bar with nav + management icons */
    TilemapLevel *lvl = (app->cur_level >= 0 && app->cur_level < app->tmap.level_count)
                        ? &app->tmap.levels[app->cur_level] : NULL;
    char label[48];
    snprintf(label, sizeof(label), "Level %d/%d", app->cur_level + 1, app->tmap.level_count);
    {
        SDL_Rect tb = ui_section_bar(c.x - 4, y, c.w + 8, label);
        /* Right-aligned buttons: < > + trash copy */
        int bx = tb.x + tb.w - 5 * (bw + 2);
        SDL_Color white = {255, 255, 255, 255};

        if (ui_button(bx, tb.y, bw, bh, "")) {
            if (app->cur_level > 0) app->cur_level--;
        }
        ui_icon_centered(bx, tb.y, bw, bh, ICON_TRI_LEFT, white);
        bx += bw + 2;
        if (ui_button(bx, tb.y, bw, bh, "")) {
            if (app->cur_level < app->tmap.level_count - 1) app->cur_level++;
        }
        ui_icon_centered(bx, tb.y, bw, bh, ICON_TRI_RIGHT, white);
        bx += bw + 2;
        if (ui_button(bx, tb.y, bw, bh, "")) {
            if (app->tmap.level_count < TMAP_MAX_LEVELS) {
                TilemapLevel *nl = &app->tmap.levels[app->tmap.level_count];
                memset(nl, 0, sizeof(*nl));
                snprintf(nl->name, sizeof(nl->name), "Level %d", app->tmap.level_count + 1);
                nl->width = 20; nl->height = 16;
                app->cur_level = app->tmap.level_count;
                app->tmap.level_count++;
                app->modified = true;
            }
        }
        ui_icon_centered(bx, tb.y, bw, bh, ICON_ADD, white);
        bx += bw + 2;
        if (ui_button(bx, tb.y, bw, bh, "")) {
            if (app->tmap.level_count > 1) {
                for (int i = app->cur_level; i < app->tmap.level_count - 1; i++)
                    app->tmap.levels[i] = app->tmap.levels[i + 1];
                app->tmap.level_count--;
                if (app->cur_level >= app->tmap.level_count) app->cur_level = app->tmap.level_count - 1;
                app->modified = true;
            }
        }
        ui_icon_centered(bx, tb.y, bw, bh, ICON_TRASH, white);
        bx += bw + 2;
        if (ui_button(bx, tb.y, bw, bh, "")) {
            if (app->tmap.level_count < TMAP_MAX_LEVELS && lvl) {
                app->tmap.levels[app->tmap.level_count] = *lvl;
                snprintf(app->tmap.levels[app->tmap.level_count].name, 32, "%s copy", lvl->name);
                app->cur_level = app->tmap.level_count;
                app->tmap.level_count++;
                app->modified = true;
            }
        }
        ui_icon_centered(bx, tb.y, bw, bh, ICON_COPY, white);
        y = tb.y + bh + 8;
    }

    /* Size */
    if (lvl) {
        y = ui_section(c.x - 4, y, c.w + 8, "Size");
        int lw = ui_text_width("Height:");
        if (ui_input_int_ex(c.x, y, c.w, "Width:", lw, &lvl->width, 1, 1, TMAP_MAX_W))
            app->modified = true;
        y += ui_line_height() + 8;
        if (ui_input_int_ex(c.x, y, c.w, "Height:", lw, &lvl->height, 1, 1, TMAP_MAX_H))
            app->modified = true;
        y += ui_line_height() + 12;
    }

    /* Entity type selector (when in sprite layer) */
    if ((app->sel_type == SEL_SPRITE)) {
        y = ui_section(c.x - 4, y, c.w + 8, "Entity Type");
        static const char *ent_names[] = {"Player", "Bat", "Spider", "Snake", "Miner"};
        int ebw = (c.w - 4) / 2;
        for (int i = 0; i < ENT_TYPE_COUNT; i++) {
            int col = i % 2, row = i / 2;
            int ex = c.x + col * (ebw + 4);
            int ey = y + row * (ui_line_height() + 6);
            if (ui_button(ex, ey, ebw, ui_line_height() + 4, ent_names[i]))
                app->cur_sprite = i;
        }
        y += ((ENT_TYPE_COUNT + 1) / 2) * (ui_line_height() + 6) + 4;

        /* Entity info */
        if (lvl) {
            snprintf(label, sizeof(label), "Entities: %d / %d", lvl->entity_count, MAX_ENTITIES);
            ui_text_color(c.x, y, label, ui_theme.text_dim);
            y += ui_line_height() + 4;
        }
        ui_text_small(c.x, y, "Click: place entity");
        y += ui_line_height();
        ui_text_small(c.x, y, "Right-click: delete");
        y += ui_line_height();
        ui_text_small(c.x, y, "Drag: move entity");
        y += ui_line_height() + 8;
    }

    /* Actions */
    y = ui_section(c.x - 4, y, c.w + 8, "Actions");
    if (ui_button(c.x, y, bw, bh, "")) {
        if (lvl) {
            memset(lvl->tiles, 0, sizeof(lvl->tiles));
            lvl->entity_count = 0;
            app->modified = true;
        }
    }
        ui_icon_centered(c.x, y, bw, bh, ICON_CLOSE, ui_theme.text);
    ui_text_color(c.x + bw + 6, y + 3, "Clear", ui_theme.text_dim);
    y += bh + 8;

    /* Scroll info */
    snprintf(label, sizeof(label), "Scroll: %d,%d", app->scroll_x, app->scroll_y);
    ui_text_color(c.x, y, label, ui_theme.text_dim);
    y += ui_line_height() + 4;
    ui_text_small(c.x, y, "WASD/Arrows to scroll");

    ui_panel_end();
}
