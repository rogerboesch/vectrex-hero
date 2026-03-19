//
// drawing.c — Cave Diver draw functions, HUD, screens
//

#include "cave.h"
#include "sprites.h"
#include "font.h"

uint8_t int_to_str(int val, uint8_t pos) {
    char tmp[6];
    uint8_t i = 0;

    if (val < 0) {
        str_buf[pos++] = '-';
        val = -val;
    }
    if (val == 0) {
        tmp[i++] = '0';
    }
    else {
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
    moveto_d(y + SPRITE_OY(shape), x + SPRITE_OX(shape));
    draw_vlc(SPRITE_VLC(shape));
}

void draw_cave(void) {
    zero_beam();
    intensity_a(INTENSITY_DIM);
    set_scale(0x7F);
    draw_vlc(cur_cave_lines);
}

void draw_lava(void) {
    // In Cave Diver, "lava" is rendered as water currents
    if (!cur_has_lava) return;
    zero_beam();
    intensity_a(INTENSITY_DIM);
    set_scale(0x7F);
    moveto_d(cur_cave_floor + LAVA_HEIGHT, cur_cave_left);
    draw_line_d(0, cur_cave_right - cur_cave_left);
}

void draw_enemies(void) {
    uint8_t i;
    for (i = 0; i < enemy_count; i++) {
        if (!enemies[i].alive) continue;
        // Placeholder: draw dot at enemy position
        zero_beam();
        intensity_a(INTENSITY_NORMAL);
        set_scale(0x20);
        moveto_d(enemies[i].y, enemies[i].x);
        draw_line_d(3, -3);
        draw_line_d(-3, -3);
        draw_line_d(-3, 3);
        draw_line_d(3, 3);
    }
}

void draw_sonar_pulse(void) {
    if (!sonar_active) return;
    // Expanding circle placeholder
    uint8_t radius = (SONAR_COOLDOWN - sonar_timer) * 2;
    zero_beam();
    intensity_a(INTENSITY_DIM);
    set_scale(0x40);
    moveto_d(player_y + radius, player_x);
    draw_line_d(0, radius);
    draw_line_d(-radius, 0);
    draw_line_d(0, -radius * 2);
    draw_line_d(radius, 0);
}

void draw_diver(void) {
    if (!cur_has_diver) return;
    zero_beam();
    intensity_a(INTENSITY_BRIGHT);
    set_scale(0x30);
    moveto_d(cur_diver_y, cur_diver_x);
    // Small person shape
    draw_line_d(8, 0);
    draw_line_d(-4, 3);
    draw_line_d(-4, -3);
    draw_line_d(4, -3);
    draw_line_d(-4, 3);
}

void draw_hud(void) {
    uint8_t p;
    // Score
    str_buf[0] = 'S';
    str_buf[1] = ' ';
    p = int_to_str(score, 2);
    draw_text(120, -120, str_buf, 0x30, 6);

    // Lives
    str_buf[0] = 'L';
    str_buf[1] = ' ';
    p = int_to_str(player_lives, 2);
    draw_text(120, 60, str_buf, 0x30, 6);
}

void draw_oxygen_bar(void) {
    int8_t bar_w;
    zero_beam();
    intensity_a(INTENSITY_NORMAL);
    set_scale(0x7F);
    moveto_d(120, -60);

    bar_w = (int8_t)(player_oxygen / 3);
    draw_line_d(0, bar_w);
    draw_line_d(-3, 0);
    draw_line_d(0, -bar_w);
    draw_line_d(3, 0);
}

void draw_title_screen(void) {
    draw_text(40, -80, "CAVE DIVER", 0x70, 16);
    draw_text(-20, -58, "PRESS BTN", 0x50, 13);
}

void draw_game_over_screen(void) {
    draw_text(30, -45, "GAME OVER", 0x50, 10);
    uint8_t p = int_to_str(score, 0);
    draw_text(-20, -30, str_buf, 0x50, 10);
    draw_text(-60, -45, "PRESS BTN", 0x50, 10);
}

void draw_level_intro_screen(void) {
    str_buf[0] = 'L';
    str_buf[1] = 'E';
    str_buf[2] = 'V';
    str_buf[3] = 'E';
    str_buf[4] = 'L';
    str_buf[5] = ' ';
    int_to_str(current_level + 1, 6);
    draw_text(20, -48, str_buf, 0xA0, 18);
}

void draw_rescued_screen(void) {
    draw_text(30, -50, "RESCUED", 0xA0, 18);
}

void draw_failed_screen(void) {
    draw_text(30, -45, "FAILED", 0xA0, 18);
}
