//
// enemies.c — Enemy update, laser, dynamite, miner rescue
//

#include "hero.h"

void fire_laser(void) {
    if (laser_active) return;
    laser_active = 1;
    laser_x = player_x;
    laser_y = player_y;
    laser_dir = player_facing;
    laser_timer = LASER_LIFETIME;
}

void update_laser(void) {
    uint8_t i;
    int8_t lx_end, lx_min, lx_max;
    if (!laser_active) return;

    laser_timer--;
    if (laser_timer == 0) {
        laser_active = 0;
        return;
    }

    // Check laser vs enemies
    lx_end = laser_x + (laser_dir * LASER_LENGTH);
    if (laser_dir > 0) { lx_min = laser_x; lx_max = lx_end; }
    else { lx_min = lx_end; lx_max = laser_x; }

    for (i = 0; i < enemy_count; i++) {
        if (!enemies[i].alive) continue;
        if (enemies[i].x + BAT_HW >= lx_min &&
            enemies[i].x - BAT_HW <= lx_max &&
            enemies[i].y + BAT_HH >= laser_y - 3 &&
            enemies[i].y - BAT_HH <= laser_y + 3) {
            enemies[i].alive = 0;
            score += 50;
        }
    }
}

void place_dynamite(void) {
    if (dyn_active || dyn_exploding) return;
    if (player_dynamite == 0) return;
    player_dynamite--;
    dyn_active = 1;
    dyn_x = player_x;
    dyn_y = player_y - PLAYER_HH + 4;
    dyn_timer = DYNAMITE_FUSE;
}

void update_dynamite(void) {
    uint8_t i;
    int8_t wcx, wcy;

    if (dyn_active && !dyn_exploding) {
        dyn_timer--;
        if (dyn_timer == 0) {
            dyn_exploding = 1;
            dyn_expl_timer = EXPLOSION_TIME;
        }
        return;
    }

    if (dyn_exploding) {
        dyn_expl_timer--;

        if (dyn_expl_timer == EXPLOSION_TIME - 1) {
            // Destroy nearby walls (skip first 3 = permanent structure)
            for (i = 3; i < cur_wall_count; i++) {
                if (walls_destroyed & (1 << i)) continue;
                wcx = wall_x(i) + (wall_w(i) / 2);
                wcy = wall_y(i);
                if (box_overlap(dyn_x, dyn_y, EXPLOSION_RADIUS, EXPLOSION_RADIUS,
                                wcx, wcy, wall_w(i) / 2, wall_h(i))) {
                    walls_destroyed = walls_destroyed | (uint8_t)(1 << i);
                    score += 75;
                }
            }

            // Kill nearby enemies
            for (i = 0; i < enemy_count; i++) {
                if (!enemies[i].alive) continue;
                if (box_overlap(dyn_x, dyn_y, EXPLOSION_RADIUS, EXPLOSION_RADIUS,
                                enemies[i].x, enemies[i].y, BAT_HW, BAT_HH)) {
                    enemies[i].alive = 0;
                    score += 50;
                }
            }

            // Kill player if too close (smaller radius)
            if (box_overlap(dyn_x, dyn_y, EXPLOSION_KILL, EXPLOSION_KILL,
                            player_x, player_y, PLAYER_HW, PLAYER_HH)) {
                game_state = STATE_DYING;
                death_timer = 30;
            }
        }

        if (dyn_expl_timer == 0) {
            dyn_active = 0;
            dyn_exploding = 0;
        }
    }
}

void update_enemies(void) {
    uint8_t i;

    for (i = 0; i < enemy_count; i++) {
        if (!enemies[i].alive) continue;

        enemies[i].x += enemies[i].vx;

        // Bounce off shaft walls
        if (enemies[i].x > SHAFT_RIGHT - BAT_HW ||
            enemies[i].x < SHAFT_LEFT + BAT_HW) {
            enemies[i].vx = -enemies[i].vx;
        }

        enemies[i].anim++;

        // Check collision with player
        if (box_overlap(player_x, player_y, PLAYER_HW, PLAYER_HH,
                        enemies[i].x, enemies[i].y, BAT_HW, BAT_HH)) {
            game_state = STATE_DYING;
            death_timer = 30;
        }
    }
}

void check_miner_rescue(void) {
    if (box_overlap(player_x, player_y, PLAYER_HW, PLAYER_HH,
                    cur_miner_x, cur_miner_y, MINER_HW, MINER_HH)) {
        score += 1000;
        score += player_fuel;
        score += player_dynamite * 50;
        game_state = STATE_LEVEL_COMPLETE;
        level_msg_timer = 60;
    }
}
