/* panel_row_editor.c — Stub (coming soon) */
#include "app.h"
#include "ui.h"
void draw_row_editor(App *app, int x, int y, int w, int h) {
    ui_panel_begin("Row Editor", x, y, w, h);
    ui_text_color(x + 20, y + 40, "Row editor — coming soon", ui_theme.text_dim);
    ui_panel_end();
}
