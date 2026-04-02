/* panel_level_viewer.c — Stub (coming soon) */
#include "app.h"
#include "ui.h"
void draw_level_viewer(App *app, int x, int y, int w, int h) {
    ui_panel_begin("Level Viewer", x, y, w, h);
    ui_text_color(x + 20, y + 40, "Level viewer — coming soon", ui_theme.text_dim);
    ui_panel_end();
}
