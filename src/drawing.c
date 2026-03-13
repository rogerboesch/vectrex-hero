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
    moveto_d(SPRITE_OY(shape), SPRITE_OX(shape));
    draw_vlc(SPRITE_VLC(shape));
}

void draw_cave(void) {
    const int8_t *p = cur_cave_lines;
    uint8_t n, i;
    uint8_t cave_int = (dyn_exploding || (game_state == STATE_LEVEL_COMPLETE && (level_msg_timer & 4))) ? INTENSITY_BRIGHT : INTENSITY_DIM;

    while ((n = (uint8_t)*p++) != 0) {
        zero_beam();
        intensity_a(cave_int);
        set_scale(0x7F);
        moveto_d(p[0], p[1]);
        p += 2;
        for (i = 0; i < n; i++) {
            draw_line_d(p[0], p[1]);
            p += 2;
        }
    }

    for (i = 0; i < cur_wall_count; i++) {
        if (walls_destroyed & (1 << i)) continue;
        zero_beam();
        intensity_a(dyn_exploding ? INTENSITY_BRIGHT : INTENSITY_HI);
        set_scale(0x7F);
        moveto_d(wall_y(i) + wall_h(i), wall_x(i));
        draw_line_d(0, wall_w(i));
        draw_line_d(-wall_h(i) * 2, 0);
        draw_line_d(0, -wall_w(i));
        draw_line_d(wall_h(i) * 2, 0);
    }
}

void draw_enemies(void) {
    uint8_t i;
    int8_t *frame;
    for (i = 0; i < enemy_count; i++) {
        if (!enemies[i].alive) continue;
        if (enemies[i].type == ENEMY_SPIDER) {
            // Draw thread line from anchor to current position
            zero_beam();
            intensity_a(INTENSITY_DIM);
            set_scale(0x7F);
            moveto_d(enemies[i].home_y, enemies[i].x);
            draw_line_d(enemies[i].y - enemies[i].home_y, 0);
            // Draw spider sprite
            frame = (enemies[i].anim & 8) ? spider_f0 : spider_f1;
        } else {
            frame = (enemies[i].anim & 8) ? bat_f0 : bat_f1;
        }
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
        moveto_d(dyn_y, dyn_x);
        set_scale(0x30);
        moveto_d(SPRITE_OY(dynamite), SPRITE_OX(dynamite));
        draw_vlc(SPRITE_VLC(dynamite));
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

static const int8_t lava_sin[] = {0, 3, 4, 3, 0, -3, -4, -3};

void draw_lava(void) {
    uint8_t i, phase;
    int8_t step, dy;
    if (!cur_has_lava) return;
    step = (int8_t)(((int)cur_cave_right - (int)cur_cave_left) / 8);
    phase = (anim_tick >> 1) & 7;
    // Upper wave
    zero_beam();
    intensity_a(INTENSITY_HI);
    set_scale(0x7F);
    moveto_d(cur_cave_floor + lava_sin[phase], cur_cave_left);
    for (i = 0; i < 8; i++) {
        dy = lava_sin[(phase + i + 1) & 7] - lava_sin[(phase + i) & 7];
        draw_line_d(dy, step);
    }
    // Lower wave (offset phase, 5 units below)
    zero_beam();
    intensity_a(INTENSITY_NORMAL);
    set_scale(0x7F);
    moveto_d(cur_cave_floor - 5 + lava_sin[(phase + 4) & 7], cur_cave_left);
    for (i = 0; i < 8; i++) {
        dy = lava_sin[(phase + 4 + i + 1) & 7] - lava_sin[(phase + 4 + i) & 7];
        draw_line_d(dy, step);
    }
}

void draw_miner(void) {
    if (!cur_has_miner) return;
    draw_sprite(cur_miner_y, cur_miner_x, miner);
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
