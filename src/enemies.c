//
// enemies.c — Enemy update, laser, dynamite, miner rescue
//

#include "hero.h"
#include "sprites.h"

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
        int8_t ehw, ehh;
        if (!enemies[i].alive) continue;
        if (enemies[i].type == ENEMY_SPIDER) {
            ehw = SPRITE_HW(spider); ehh = SPRITE_HH(spider);
        } else {
            ehw = SPRITE_HW(bat_f0); ehh = SPRITE_HH(bat_f0);
        }
        if (enemies[i].x + ehw >= lx_min &&
            enemies[i].x - ehw <= lx_max &&
            enemies[i].y + ehh >= laser_y - 3 &&
            enemies[i].y - ehh <= laser_y + 3) {
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
    dyn_y = player_y - SPRITE_HH(player) + 4;
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
            // Destroy nearby walls
            for (i = 0; i < cur_wall_count; i++) {
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
                int8_t ehw, ehh;
                if (!enemies[i].alive) continue;
                if (enemies[i].type == ENEMY_SPIDER) {
                    ehw = SPRITE_HW(spider); ehh = SPRITE_HH(spider);
                } else {
                    ehw = SPRITE_HW(bat_f0); ehh = SPRITE_HH(bat_f0);
                }
                if (box_overlap(dyn_x, dyn_y, EXPLOSION_RADIUS, EXPLOSION_RADIUS,
                                enemies[i].x, enemies[i].y, ehw, ehh)) {
                    enemies[i].alive = 0;
                    score += 50;
                }
            }

            // Kill player if too close (smaller radius)
            if (box_overlap(dyn_x, dyn_y, EXPLOSION_KILL, EXPLOSION_KILL,
                            player_x, player_y, SPRITE_HW(player), SPRITE_HH(player))) {
                game_state = STATE_DYING;
                death_timer = DEATH_ANIM_TIME;
            }
        }

        if (dyn_expl_timer == 0) {
            dyn_active = 0;
            dyn_exploding = 0;
        }
    }
}

void update_enemies(void) {
    uint8_t i, j;
    int8_t x1, y1, y2, seg_min, seg_max;
    int8_t ehw, ehh;
    const int8_t *segs;

    for (i = 0; i < enemy_count; i++) {
        if (!enemies[i].alive) continue;

        if (enemies[i].type == ENEMY_SPIDER) {
            // Spider: vertical patrol from home_y downward
            enemies[i].y += enemies[i].vx;
            if (enemies[i].home_y - enemies[i].y >= SPIDER_PATROL) {
                enemies[i].y = enemies[i].home_y - SPIDER_PATROL;
                enemies[i].vx = -enemies[i].vx;
            } else if (enemies[i].y > enemies[i].home_y) {
                enemies[i].y = enemies[i].home_y;
                enemies[i].vx = -enemies[i].vx;
            }
            ehw = SPRITE_HW(spider);
            ehh = SPRITE_HH(spider);
        } else {
            // Bat: horizontal movement
            enemies[i].x += enemies[i].vx;

            // Bounce off room bounds
            if (enemies[i].x > cur_cave_right - SPRITE_HW(bat_f0) ||
                enemies[i].x < cur_cave_left + SPRITE_HW(bat_f0)) {
                enemies[i].vx = -enemies[i].vx;
            }

            // Bounce off vertical cave segments
            segs = cur_cave_segs;
            for (j = 0; j < cur_seg_count; j++) {
                if (segs[j * 4] != segs[j * 4 + 2]) continue;  // not vertical
                x1 = segs[j * 4];
                y1 = segs[j * 4 + 1];
                y2 = segs[j * 4 + 3];
                seg_min = y1 < y2 ? y1 : y2;
                seg_max = y1 > y2 ? y1 : y2;
                if (enemies[i].y + SPRITE_HH(bat_f0) > seg_min &&
                    enemies[i].y - SPRITE_HH(bat_f0) < seg_max) {
                    if (enemies[i].vx > 0 &&
                        enemies[i].x + SPRITE_HW(bat_f0) > x1 &&
                        enemies[i].x < x1) {
                        enemies[i].x = x1 - SPRITE_HW(bat_f0);
                        enemies[i].vx = -enemies[i].vx;
                    } else if (enemies[i].vx < 0 &&
                               enemies[i].x - SPRITE_HW(bat_f0) < x1 &&
                               enemies[i].x > x1) {
                        enemies[i].x = x1 + SPRITE_HW(bat_f0);
                        enemies[i].vx = -enemies[i].vx;
                    }
                }
            }
            ehw = SPRITE_HW(bat_f0);
            ehh = SPRITE_HH(bat_f0);
        }

        enemies[i].anim++;

        // Check collision with player
        if (box_overlap(player_x, player_y, SPRITE_HW(player), SPRITE_HH(player),
                        enemies[i].x, enemies[i].y, ehw, ehh)) {
            game_state = STATE_DYING;
            death_timer = 30;
        }
    }
}

void check_miner_rescue(void) {
    if (!cur_has_miner) return;
    if (box_overlap(player_x, player_y, SPRITE_HW(player), SPRITE_HH(player),
                    cur_miner_x, cur_miner_y, SPRITE_HW(miner), SPRITE_HH(miner))) {
        score += 1000;
        score += player_fuel;
        score += player_dynamite * 50;
        game_state = STATE_LEVEL_COMPLETE;
        level_msg_timer = RESCUE_ANIM_TIME;
    }
}
