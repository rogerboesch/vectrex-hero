//
// drawing.c — All draw functions, HUD, screens
//

#include "hero.h"
#include "sprites.h"
#include "font.h"

// Fast int-to-string into str_buf at offset pos.
// Appends trailing space (to clear old digits on screen).
// Returns position after the space.
uint8_t int_to_str(int val, uint8_t pos) {
    char tmp[6];
    uint8_t i = 0;

    if (val < 0) {
        str_buf[pos++] = '-';
        val = -val;
    }
    if (val == 0) {
        tmp[i++] = '0';
    } else {
        while (val > 0) {
            tmp[i++] = (char)('0' + val % 10);
            val = val / 10;
        }
    }
    while (i > 0) {
        str_buf[pos++] = tmp[--i];
    }
    str_buf[pos++] = ' ';
    str_buf[pos] = '\0';
    return pos;
}

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

    intensity_a(cave_int);
    set_scale(0x7F);
    while ((n = (uint8_t)*p++) != 0) {
        zero_beam();
        moveto_d(p[0], p[1]);
        p += 2;
        for (i = 0; i < n; i++) {
            draw_line_d(p[0], p[1]);
            p += 2;
        }
    }

    if (cur_wall_count > 0) {
        intensity_a(dyn_exploding ? INTENSITY_BRIGHT : INTENSITY_HI);
        for (i = 0; i < cur_wall_count; i++) {
            if (walls_destroyed & (1 << i)) continue;
            zero_beam();
            set_scale(0x7F);
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
        if (enemies[i].type == ENEMY_SPIDER) {
            // Draw thread line from anchor to current position
            zero_beam();
            intensity_a(INTENSITY_DIM);
            set_scale(0x7F);
            moveto_d(enemies[i].home_y, enemies[i].x);
            draw_line_d(enemies[i].y - enemies[i].home_y, 0);
            // Draw spider sprite (single frame)
            frame = spider;
        } else {
            frame = (enemies[i].anim & 8) ? bat_f0 : bat_f1;
        }
        draw_sprite(enemies[i].y, enemies[i].x, frame);
    }
}

void draw_laser_beam(void) {
    if (!laser_active) return;
    zero_beam();
    set_scale(0x7F);
    moveto_d(laser_y, laser_x);
    // Per-frame flicker instead of spatial segmentation
    intensity_a((anim_tick & 1) ? INTENSITY_HI : INTENSITY_NORMAL);
    draw_line_d(0, laser_dir * LASER_LENGTH);
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

static const int8_t lava_sin[] = {0, 4, 0, -4};

void draw_lava(void) {
    uint8_t i, phase;
    int8_t step, dy;
    if (!cur_has_lava) return;
    step = (int8_t)(((int)cur_cave_right - (int)cur_cave_left) / 4);
    phase = (anim_tick >> 1) & 3;
    // Upper wave
    zero_beam();
    intensity_a(INTENSITY_HI);
    set_scale(0x7F);
    moveto_d(cur_cave_floor + lava_sin[phase], cur_cave_left);
    for (i = 0; i < 4; i++) {
        dy = lava_sin[(phase + i + 1) & 3] - lava_sin[(phase + i) & 3];
        draw_line_d(dy, step);
    }
    // Lower wave (offset phase, 5 units below)
    zero_beam();
    intensity_a(INTENSITY_NORMAL);
    set_scale(0x7F);
    moveto_d(cur_cave_floor - 5 + lava_sin[(phase + 2) & 3], cur_cave_left);
    for (i = 0; i < 4; i++) {
        dy = lava_sin[(phase + 2 + i + 1) & 3] - lava_sin[(phase + 2 + i) & 3];
        draw_line_d(dy, step);
    }
}

void draw_miner(void) {
    if (!cur_has_miner) return;
    draw_sprite(cur_miner_y, cur_miner_x, miner);
}

void draw_text(int8_t y, int8_t x, const char *str, uint8_t scale, uint8_t advance) {
    const int8_t *g;
    int cx = (int)x;
    uint8_t i;
    for (i = 0; str[i] != '\0'; i++) {
        g = font_glyph(str[i]);
        if (g != 0) {
            zero_beam();
            set_scale(0x7F);
            moveto_d(y, (int8_t)cx);
            set_scale(scale);
            moveto_d(g[0], g[1]);
            draw_vlc((char *)&g[2]);
        }
        cx += advance;
    }
}

void draw_hud(void) {
    uint8_t p;
    int8_t bar_on, bar_off;

    // Dynamite — bar, left
    zero_beam();
    set_scale(0x7F);
    moveto_d(127, -125);

    bar_on = (int8_t)(player_dynamite * 7);
    bar_off = (int8_t)((START_DYNAMITE - player_dynamite) * 7);
    if (bar_on > 0) {
        intensity_a(INTENSITY_BRIGHT);
        draw_line_d(0, bar_on);
    }
    if (bar_off > 0) {
        intensity_a(INTENSITY_DIM);
        draw_line_d(0, bar_off);
    }

    // Score — text, center
    p = int_to_str(score, 0);
    str_buf[p - 1] = '\0';
    draw_text(127, -10, str_buf, 0x50, 10);
    // Lives — bar, right
    zero_beam();
    set_scale(0x7F);
    moveto_d(127, 100);
    bar_on = (int8_t)(player_lives * 7);
    bar_off = (int8_t)((START_LIVES - player_lives) * 7);
    if (bar_on > 0) {
        intensity_a(INTENSITY_BRIGHT);
        draw_line_d(0, bar_on);
    }
    if (bar_off > 0) {
        intensity_a(INTENSITY_DIM);
        draw_line_d(0, bar_off);
    }
    intensity_a(INTENSITY_NORMAL);
}

void draw_fuel_bar(void) {
    int8_t bar_y, bar_len, used_len;
    int max_w, fuel_w;
    int8_t bar_left;

    bar_y = cur_cave_floor - 20;
    max_w = ((int)cur_cave_right - (int)cur_cave_left) / 2;
    bar_left = (int8_t)(-max_w / 2);

    // Bar length proportional to fuel remaining
    fuel_w = ((int)player_fuel * max_w) / START_FUEL;

    // Fill animation during level start timer
    if (level_msg_timer > 0) {
        fuel_w = (fuel_w * (30 - (int)level_msg_timer)) / 30;
    }

    bar_len = (int8_t)fuel_w;
    used_len = (int8_t)(max_w - bar_len);
    if (bar_len <= 0 && used_len <= 0) return;

    zero_beam();
    set_scale(0x7F);
    moveto_d(bar_y, bar_left);

    if (bar_len > 0) {
        intensity_a(INTENSITY_BRIGHT);
        draw_line_d(0, bar_len);
    }
    if (used_len > 0) {
        intensity_a(INTENSITY_DIM);
        draw_line_d(0, used_len);
    }
}

void draw_title_screen(void) {
    zero_beam();
    set_scale(0x7F);
    intensity_a(INTENSITY_HI);
    draw_text(50, -48, "HERO", 0xF0, 24);
    intensity_a(INTENSITY_NORMAL);
    zero_beam();
    draw_text(-20, -70, "FOR VECTREX", 0x70, 13);
    zero_beam();
    draw_text(-55, -58, "PRESS BTN", 0x70, 13);
}

void draw_game_over_screen(void) {
    zero_beam();
    set_scale(0x7F);
    intensity_a(INTENSITY_HI);
    draw_text(30, -45, "GAME OVER", 0x50, 10);
    intensity_a(INTENSITY_NORMAL);
    zero_beam();
    str_buf[0] = 'S'; str_buf[1] = 'C'; str_buf[2] = 'O';
    str_buf[3] = 'R'; str_buf[4] = 'E'; str_buf[5] = ' ';
    int_to_str(score, 6);
    draw_text(-20, -45, str_buf, 0x50, 10);
    zero_beam();
    draw_text(-60, -45, "PRESS BTN", 0x50, 10);
}
