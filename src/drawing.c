//
// drawing.c — All draw functions, HUD, screens
//

#include "hero.h"
#include "sprites.h"

void draw_sprite(int8_t y, int8_t x, int8_t *shape) {
    zero_beam();
    set_scale(0x7F);
    moveto_d(y, x);
    set_scale(0x60);
    draw_vlc(shape);
}

void draw_cave(void) {
    uint8_t i;
    uint8_t cave_int = dyn_exploding ? INTENSITY_BRIGHT : INTENSITY_DIM;

    // Left wall path: top-left -> ledge -> shaft left -> floor
    zero_beam();
    intensity_a(cave_int);
    set_scale(0x7F);
    moveto_d(CAVE_TOP, CAVE_LEFT);
    draw_line_d(LEDGE_Y - CAVE_TOP, 0);
    draw_line_d(0, SHAFT_LEFT - CAVE_LEFT);
    draw_line_d(-40, 0);
    draw_line_d(CAVE_FLOOR - LEDGE_Y + 40, 0);
    draw_line_d(0, SHAFT_RIGHT - SHAFT_LEFT);

    // Right wall path: ceiling -> right wall -> ledge -> shaft right
    zero_beam();
    intensity_a(cave_int);
    moveto_d(CAVE_TOP, SHAFT_LEFT);
    draw_line_d(0, CAVE_RIGHT - SHAFT_LEFT);
    draw_line_d(LEDGE_Y - CAVE_TOP, 0);
    draw_line_d(0, SHAFT_RIGHT - CAVE_RIGHT);
    draw_line_d(-40, 0);
    draw_line_d(CAVE_FLOOR - LEDGE_Y + 40, 0);

    // Draw any destroyable walls that still exist
    for (i = 0; i < cur_wall_count; i++) {
        if (walls_destroyed & (1 << i)) continue;
        if (current_level == 1 && i == 3) {
            zero_beam();
            intensity_a(dyn_exploding ? INTENSITY_BRIGHT : INTENSITY_HI);
            moveto_d(wall_y(i) + wall_h(i), wall_x(i));
            draw_line_d(0, wall_w(i));
            draw_line_d(-wall_h(i) * 2, 0);
            draw_line_d(0, -wall_w(i));
            draw_line_d(wall_h(i) * 2, 0);
        }
    }
}

void draw_enemies(void) {
    uint8_t i;
    int8_t *frame;
    for (i = 0; i < enemy_count; i++) {
        if (!enemies[i].alive) continue;
        frame = (enemies[i].anim & 8) ? bat_frame1 : bat_frame2;
        draw_sprite(enemies[i].y, enemies[i].x, frame);
    }
}

void draw_laser_beam(void) {
    int8_t seg, remaining, len;
    uint8_t on;
    if (!laser_active) return;
    zero_beam();
    set_scale(0x7F);
    moveto_d(laser_y, laser_x);
    // Flickering segmented beam
    remaining = LASER_LENGTH;
    on = anim_tick & 1;
    while (remaining > 0) {
        seg = remaining > 8 ? 8 : remaining;
        len = laser_dir * seg;
        if (on) {
            intensity_a(INTENSITY_HI);
            draw_line_d(0, len);
        } else {
            intensity_a(0);
            draw_line_d(0, len);
        }
        on = !on;
        remaining -= seg;
    }
    intensity_a(INTENSITY_NORMAL);
}

void draw_dynamite_and_explosion(void) {
    uint8_t r;
    if (!dyn_active) return;

    if (!dyn_exploding) {
        zero_beam();
        set_scale(0x7F);
        moveto_d(dyn_y, dyn_x - 2);
        set_scale(0x30);
        draw_line_d(0, 4);
        draw_line_d(-6, 0);
        draw_line_d(0, -4);
        draw_line_d(6, 0);
        if (dyn_timer & 2) {
            intensity_a(INTENSITY_HI);
            draw_line_d(3, 0);
            intensity_a(INTENSITY_NORMAL);
        }
    } else {
        r = (EXPLOSION_TIME - dyn_expl_timer) * 3;
        if (r > EXPLOSION_RADIUS) r = EXPLOSION_RADIUS;

        // Flash: bright burst that fades over time
        {
            uint8_t age = EXPLOSION_TIME - dyn_expl_timer;
            uint8_t inten = INTENSITY_BRIGHT - age * 3;
            if (inten < INTENSITY_NORMAL) inten = INTENSITY_NORMAL;

            // First few frames: draw extra starburst rays
            if (age < 4) {
                zero_beam();
                intensity_a(INTENSITY_BRIGHT);
                set_scale(0x7F);
                moveto_d(dyn_y, dyn_x - r);
                draw_line_d(0, r + r);
                zero_beam();
                moveto_d(dyn_y + r, dyn_x);
                draw_line_d(-r - r, 0);
            }

            // X pattern with fading intensity
            zero_beam();
            intensity_a(inten);
            set_scale(0x7F);
            moveto_d(dyn_y + r, dyn_x - r);
            draw_line_d(-r, r);
            draw_line_d(-r, r);
            zero_beam();
            moveto_d(dyn_y + r, dyn_x + r);
            draw_line_d(-r, -r);
            draw_line_d(-r, -r);
        }
        intensity_a(INTENSITY_NORMAL);
    }
}

void draw_miner(void) {
    draw_sprite(cur_miner_y, cur_miner_x, miner_shape);
}

void draw_hud(void) {
    int8_t f, l;
    f = (int8_t)(player_fuel >> 1);
    l = (int8_t)player_lives;
    zero_beam();
    set_scale(0x7F);
    sprintf(str_buf, "%d ", score);
    print_str_c(127, -125, str_buf);
    zero_beam();
    sprintf(str_buf, "%d ", f);
    print_str_c(127, -20, str_buf);
    zero_beam();
    sprintf(str_buf, "%d ", l);
    print_str_c(127, 100, str_buf);
}

void draw_title_screen(void) {
    zero_beam();
    set_scale(0x7F);
    intensity_a(INTENSITY_HI);
    print_str_c(40, -60, "HERO");
    intensity_a(INTENSITY_NORMAL);
    zero_beam();
    print_str_c(-10, -80, "FOR VECTREX");
    zero_beam();
    print_str_c(-50, -60, "PRESS BTN");
}

void draw_game_over_screen(void) {
    zero_beam();
    set_scale(0x7F);
    intensity_a(INTENSITY_HI);
    print_str_c(30, -80, "GAME OVER");
    intensity_a(INTENSITY_NORMAL);
    zero_beam();
    sprintf(str_buf, "SCORE %d ", score);
    print_str_c(-20, -80, str_buf);
    zero_beam();
    print_str_c(-60, -60, "PRESS BTN");
}
