/*
 * panel_level_tools.c — Level management tools (right panel)
 *
 * Level browser, size, sprites, entity tools.
 */
#include "app.h"
#include "ui.h"
#include <stdio.h>
#include <string.h>

void draw_level_tools(App *app, int px, int py, int pw, int ph) {
    ui_panel_begin_toolbar(px, py, pw, ph);
    SDL_Rect c = ui_panel_content();
    int y = c.y;
    int bw = 30, bh = ui_line_height() + 4;

    /* Level browser — section bar with nav + management icons */
    TilemapLevel *lvl = (app->cur_level >= 0 && app->cur_level < app->tmap.level_count)
                        ? &app->tmap.levels[app->cur_level] : NULL;
    char label[48];
    snprintf(label, sizeof(label), "Level %d/%d", app->cur_level + 1, app->tmap.level_count);
    {
        SDL_Rect tb = ui_section_bar(c.x - 4, y, c.w + 8, label);
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

    /* Entity type selector (always visible) */
    y = ui_section(c.x - 4, y, c.w + 8, "Entity Type");
    {
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

        if (lvl) {
            snprintf(label, sizeof(label), "Entities: %d / %d", lvl->entity_count, MAX_ENTITIES);
            ui_text_color(c.x, y, label, ui_theme.text_dim);
            y += ui_line_height() + 8;
        }
    }

    /* Row shift tools — requires marker row set by right-click */
    y = ui_section(c.x - 4, y, c.w + 8, "Row Shift");
    if (app->marker_row >= 0 && lvl) {
        snprintf(label, sizeof(label), "Marker: row %d", app->marker_row);
        ui_text_color(c.x, y, label, ui_theme.text);
        y += ui_line_height() + 6;

        int sbw = (c.w - 4) / 2;

        /* Shift Down: insert empty row at marker, push everything below down */
        if (ui_button(c.x, y, sbw, bh, "Shift Down")) {
            if (lvl->height > 0) {
                for (int r = lvl->height - 1; r > app->marker_row; r--) {
                    memcpy(lvl->tiles[r], lvl->tiles[r - 1], lvl->width);
                    memcpy(lvl->palettes[r], lvl->palettes[r - 1], lvl->width);
                }
                memset(lvl->tiles[app->marker_row], 0, lvl->width);
                memset(lvl->palettes[app->marker_row], 0, lvl->width);
                app->modified = true;
            }
        }

        /* Shift Up: delete marker row, push everything below up */
        if (ui_button(c.x + sbw + 4, y, sbw, bh, "Shift Up")) {
            if (lvl->height > 0) {
                for (int r = app->marker_row; r < lvl->height - 1; r++) {
                    memcpy(lvl->tiles[r], lvl->tiles[r + 1], lvl->width);
                    memcpy(lvl->palettes[r], lvl->palettes[r + 1], lvl->width);
                }
                memset(lvl->tiles[lvl->height - 1], 0, lvl->width);
                memset(lvl->palettes[lvl->height - 1], 0, lvl->width);
                app->modified = true;
            }
        }
        y += bh + 6;

        /* Clear marker */
        if (ui_button(c.x, y, c.w, bh, "Clear Marker")) {
            app->marker_row = -1;
        }
    }
    else {
        ui_text_color(c.x, y, "Right-click to set marker", ui_theme.text_dim);
    }

    ui_panel_end();
}
