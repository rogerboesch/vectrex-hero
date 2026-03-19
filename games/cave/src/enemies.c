//
// enemies.c — Cave Diver enemies: fish, jellyfish, eel + sonar + rescue
//

#include "cave.h"
#include "sprites.h"

void activate_sonar(void) {
    if (sonar_active) return;
    sonar_active = 1;
    sonar_timer = SONAR_COOLDOWN;
}

void update_sonar(void) {
    if (!sonar_active) return;
    sonar_timer--;
    if (sonar_timer == 0) {
        sonar_active = 0;
    }
}

void update_enemies(void) {
    uint8_t i;
    for (i = 0; i < enemy_count; i++) {
        if (!enemies[i].alive) continue;

        enemies[i].anim++;

        switch (enemies[i].type) {
        case ENEMY_FISH:
            // Fish swim horizontally
            enemies[i].x += enemies[i].vx;
            if (enemies[i].x > 80 || enemies[i].x < -80) {
                enemies[i].vx = -enemies[i].vx;
            }
            break;

        case ENEMY_JELLYFISH:
            // Jellyfish bob up and down
            if (enemies[i].anim < JELLYFISH_BOB) {
                enemies[i].y++;
            }
            else {
                enemies[i].y--;
            }
            if (enemies[i].anim >= JELLYFISH_BOB * 2) {
                enemies[i].anim = 0;
            }
            break;

        case ENEMY_EEL:
            // Eel patrols near ground
            enemies[i].x += enemies[i].vx;
            if (enemies[i].x > 70 || enemies[i].x < -70) {
                enemies[i].vx = -enemies[i].vx;
            }
            break;
        }

        // Check collision with player
        if (box_overlap(player_x, player_y, 4, 6,
                        enemies[i].x, enemies[i].y, 4, 4)) {
            game_state = STATE_DYING;
            death_timer = DEATH_ANIM_TIME;
        }
    }
}

void check_diver_rescue(void) {
    if (!cur_has_diver) return;
    if (box_overlap(player_x, player_y, 4, 6,
                    cur_diver_x, cur_diver_y, 4, 6)) {
        score += 1000;
        game_state = STATE_RESCUED;
        level_msg_timer = RESCUE_ANIM_TIME;
    }
}
