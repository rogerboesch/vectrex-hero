/*
 * panel_image_canvas.c — 256x256 image canvas with zoom and toolbar
 */

#include "app.h"
#include "ui.h"
#include "sprite.h"  /* for QL_SDL_COLORS */
#include <stdio.h>
#include <string.h>

/* Update the SDL texture from image pixel data */
static void update_image_texture(App *app) {
    if (app->current_image >= app->image_count) return;
    QLImage *img = &app->images[app->current_image];

    /* Create texture if needed */
    if (!app->image_tex) {
        app->image_tex = SDL_CreateTexture(app->renderer, SDL_PIXELFORMAT_RGBA32,
                                            SDL_TEXTUREACCESS_STREAMING,
                                            IMAGE_SIZE, IMAGE_SIZE);
    }

    /* Build RGBA pixel buffer */
    uint32_t pixels[IMAGE_SIZE * IMAGE_SIZE];
    for (int y = 0; y < IMAGE_SIZE; y++) {
        for (int x = 0; x < IMAGE_SIZE; x++) {
            uint8_t c = img->pixels[y][x];
            SDL_Color sc = QL_SDL_COLORS[c];
            pixels[y * IMAGE_SIZE + x] = (sc.a << 24) | (sc.b << 16) | (sc.g << 8) | sc.r;
        }
    }
    SDL_UpdateTexture(app->image_tex, NULL, pixels, IMAGE_SIZE * 4);
    app->image_tex_dirty = false;
}

void draw_image_canvas(App *app, int px, int py, int pw, int ph) {
    SDL_Rect tb = ui_panel_begin_toolbar(px, py, pw, ph);
    SDL_Rect c = ui_panel_content();

    if (app->image_count == 0) {
        ui_text_color(c.x + 20, c.y + 20, "No images. Add one in the Images panel.", ui_theme.text_dim);
        ui_panel_end();
        return;
    }

    QLImage *img = &app->images[app->current_image];

    /* Toolbar buttons */
    int bx = tb.x;
    int bh = tb.h;
    int bw = 50;
    int gap = 4;

    if (ui_button(bx, tb.y, bw, bh, "L"))    { image_move_left(img);  app->image_tex_dirty = true; } bx += bw + gap;
    if (ui_button(bx, tb.y, bw, bh, "R"))    { image_move_right(img); app->image_tex_dirty = true; } bx += bw + gap;
    if (ui_button(bx, tb.y, bw, bh, "U"))    { image_move_up(img);    app->image_tex_dirty = true; } bx += bw + gap;
    if (ui_button(bx, tb.y, bw, bh, "D"))    { image_move_down(img);  app->image_tex_dirty = true; } bx += bw + gap;
    if (ui_button(bx, tb.y, bw, bh, "FlpH")) { image_flip_h(img);    app->image_tex_dirty = true; } bx += bw + gap;
    if (ui_button(bx, tb.y, bw, bh, "FlpV")) { image_flip_v(img);    app->image_tex_dirty = true; } bx += bw + gap;
    if (ui_button(bx, tb.y, bw, bh, "Clear")){ image_clear(img);     app->image_tex_dirty = true; }

    /* Update texture if dirty */
    if (app->image_tex_dirty) update_image_texture(app);

    /* Display size based on zoom */
    int disp = IMAGE_SIZE * app->image_zoom;

    /* Clamp scroll */
    int max_scroll_x = disp > c.w ? disp - c.w : 0;
    int max_scroll_y = disp > c.h ? disp - c.h : 0;
    if (app->image_scroll_x > max_scroll_x) app->image_scroll_x = max_scroll_x;
    if (app->image_scroll_y > max_scroll_y) app->image_scroll_y = max_scroll_y;
    if (app->image_scroll_x < 0) app->image_scroll_x = 0;
    if (app->image_scroll_y < 0) app->image_scroll_y = 0;

    /* Center if image fits */
    int ox = c.x, oy = c.y;
    if (disp <= c.w) ox = c.x + (c.w - disp) / 2;
    else ox = c.x - app->image_scroll_x;
    if (disp <= c.h) oy = c.y + (c.h - disp) / 2;
    else oy = c.y - app->image_scroll_y;

    /* Render image texture */
    if (app->image_tex) {
        SDL_Rect dst = {ox, oy, disp, disp};
        SDL_RenderCopy(app->renderer, app->image_tex, NULL, &dst);
    }

    /* Mouse interaction */
    if (ui_mouse_in_rect(c.x, c.y, c.w, c.h)) {
        int mx, my;
        ui_mouse_pos(&mx, &my);
        int img_x = (mx - ox) / app->image_zoom;
        int img_y = (my - oy) / app->image_zoom;

        if (img_x >= 0 && img_x < IMAGE_SIZE && img_y >= 0 && img_y < IMAGE_SIZE) {
            /* Left click: paint */
            if (ui_mouse_down()) {
                img->pixels[img_y][img_x] = app->current_color;
                app->image_tex_dirty = true;
                app->modified = 1;
            }
            /* Right click: pick color */
            if (ui_mouse_right_clicked()) {
                app->current_color = img->pixels[img_y][img_x];
            }
            /* Tooltip */
            char tip[64];
            snprintf(tip, sizeof(tip), "(%d, %d) %d: %s", img_x, img_y,
                     img->pixels[img_y][img_x], QL_COLOR_NAMES[img->pixels[img_y][img_x]]);
            ui_tooltip(tip);
        }

        /* Scroll with mouse wheel — handled via arrow keys for now */
    }

    ui_panel_end();
}
