//
// enemies.c — Enemy AI, laser, dynamite, miner rescue (GBC port)
//

#include "game.h"

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
    int8_t ehw, ehh;
    if (!laser_active) return;

    laser_timer--;
    if (laser_timer == 0) { laser_active = 0; return; }

    lx_end = laser_x + (laser_dir * LASER_LENGTH);
    if (laser_dir > 0) { lx_min = laser_x; lx_max = lx_end; }
    else { lx_min = lx_end; lx_max = laser_x; }

    for (i = 0; i < enemy_count; i++) {
        if (!enemies[i].alive) continue;
        if (enemies[i].type == ENEMY_SPIDER) { ehw = SPIDER_HW; ehh = SPIDER_HH; }
        else if (enemies[i].type == ENEMY_SNAKE) { ehw = SNAKE_HW; ehh = SNAKE_HH; }
        else { ehw = BAT_HW; ehh = BAT_HH; }
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
    dyn_y = player_y - PLAYER_HH + 4;
    dyn_timer = DYNAMITE_FUSE;
}

void update_dynamite(void) {
    uint8_t i;
    int8_t wcx, wcy;
    int8_t ehw, ehh;

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
            for (i = 0; i < cur_wall_count; i++) {
                if (walls_destroyed & (1 << i)) continue;
                wcx = wall_x(i) + (wall_w(i) / 2);
                wcy = wall_y(i);
                if (box_overlap(dyn_x, dyn_y, EXPLOSION_RADIUS, EXPLOSION_RADIUS,
                                wcx, wcy, wall_w(i) / 2, wall_h(i))) {
                    walls_destroyed |= (uint8_t)(1 << i);
                    score += 75;
                    render_destroy_wall(i);
                }
            }

            for (i = 0; i < enemy_count; i++) {
                if (!enemies[i].alive) continue;
                if (enemies[i].type == ENEMY_SPIDER) { ehw = SPIDER_HW; ehh = SPIDER_HH; }
                else if (enemies[i].type == ENEMY_SNAKE) { ehw = SNAKE_HW; ehh = SNAKE_HH; }
                else { ehw = BAT_HW; ehh = BAT_HH; }
                if (box_overlap(dyn_x, dyn_y, EXPLOSION_RADIUS, EXPLOSION_RADIUS,
                                enemies[i].x, enemies[i].y, ehw, ehh)) {
                    enemies[i].alive = 0;
                    score += 50;
                }
            }

            if (box_overlap(dyn_x, dyn_y, EXPLOSION_KILL, EXPLOSION_KILL,
                            player_x, player_y, PLAYER_HW, PLAYER_HH)) {
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
    int8_t x1, y1, x2, y2, seg_min, seg_max;
    int8_t ehw, ehh;
    const int8_t *segs;

    for (i = 0; i < enemy_count; i++) {
        if (!enemies[i].alive) continue;

        if (enemies[i].type == ENEMY_SPIDER) {
            enemies[i].y += enemies[i].vx;
            if (enemies[i].home_y - enemies[i].y >= SPIDER_PATROL) {
                enemies[i].y = enemies[i].home_y - SPIDER_PATROL;
                enemies[i].vx = -enemies[i].vx;
            } else if (enemies[i].y > enemies[i].home_y) {
                enemies[i].y = enemies[i].home_y;
                enemies[i].vx = -enemies[i].vx;
            }
            ehw = SPIDER_HW; ehh = SPIDER_HH;

        } else if (enemies[i].type == ENEMY_SNAKE) {
            enemies[i].x += enemies[i].vx;

            if (enemies[i].x > cur_cave_right - SNAKE_HW ||
                enemies[i].x < cur_cave_left + SNAKE_HW)
                enemies[i].vx = -enemies[i].vx;

            segs = cur_cave_segs;
            for (j = 0; j < cur_seg_count; j++) {
                if (segs[j * 4] != segs[j * 4 + 2]) continue;
                x1 = segs[j * 4];
                y1 = segs[j * 4 + 1];
                y2 = segs[j * 4 + 3];
                seg_min = y1 < y2 ? y1 : y2;
                seg_max = y1 > y2 ? y1 : y2;
                if (enemies[i].y + SNAKE_HH > seg_min &&
                    enemies[i].y - SNAKE_HH < seg_max) {
                    if (enemies[i].vx > 0 &&
                        enemies[i].x + SNAKE_HW > x1 && enemies[i].x < x1) {
                        enemies[i].x = x1 - SNAKE_HW;
                        enemies[i].vx = -enemies[i].vx;
                    } else if (enemies[i].vx < 0 &&
                               enemies[i].x - SNAKE_HW < x1 && enemies[i].x > x1) {
                        enemies[i].x = x1 + SNAKE_HW;
                        enemies[i].vx = -enemies[i].vx;
                    }
                }
            }

            for (j = 0; j < cur_wall_count; j++) {
                int8_t wl, wr, wt, wb;
                if (walls_destroyed & (1 << j)) continue;
                wl = wall_x(j); wr = wl + wall_w(j);
                wt = wall_y(j) + wall_h(j); wb = wall_y(j) - wall_h(j);
                if (enemies[i].y + SNAKE_HH > wb && enemies[i].y - SNAKE_HH < wt) {
                    if (enemies[i].vx > 0 &&
                        enemies[i].x + SNAKE_HW > wl && enemies[i].x < wl) {
                        enemies[i].x = wl - SNAKE_HW;
                        enemies[i].vx = -enemies[i].vx;
                    } else if (enemies[i].vx < 0 &&
                               enemies[i].x - SNAKE_HW < wr && enemies[i].x > wr) {
                        enemies[i].x = wr + SNAKE_HW;
                        enemies[i].vx = -enemies[i].vx;
                    }
                }
            }

            // Floor-edge detection
            segs = cur_cave_segs;
            for (j = 0; j < cur_seg_count; j++) {
                if (segs[j * 4 + 1] != segs[j * 4 + 3]) continue;
                y1 = segs[j * 4 + 1];
                if (y1 > enemies[i].y - SNAKE_HH) continue;
                if (y1 < enemies[i].y - SNAKE_HH - 5) continue;
                x1 = segs[j * 4]; x2 = segs[j * 4 + 2];
                seg_min = x1 < x2 ? x1 : x2;
                seg_max = x1 > x2 ? x1 : x2;
                if (enemies[i].x >= seg_min && enemies[i].x <= seg_max) {
                    if (enemies[i].vx > 0 && enemies[i].x + SNAKE_HW >= seg_max) {
                        enemies[i].x = seg_max - SNAKE_HW;
                        enemies[i].vx = -enemies[i].vx;
                    } else if (enemies[i].vx < 0 && enemies[i].x - SNAKE_HW <= seg_min) {
                        enemies[i].x = seg_min + SNAKE_HW;
                        enemies[i].vx = -enemies[i].vx;
                    }
                }
            }
            ehw = SNAKE_HW; ehh = SNAKE_HH;

        } else {
            // Bat
            enemies[i].x += enemies[i].vx;
            if (enemies[i].x > cur_cave_right - BAT_HW ||
                enemies[i].x < cur_cave_left + BAT_HW)
                enemies[i].vx = -enemies[i].vx;

            segs = cur_cave_segs;
            for (j = 0; j < cur_seg_count; j++) {
                if (segs[j * 4] != segs[j * 4 + 2]) continue;
                x1 = segs[j * 4];
                y1 = segs[j * 4 + 1]; y2 = segs[j * 4 + 3];
                seg_min = y1 < y2 ? y1 : y2;
                seg_max = y1 > y2 ? y1 : y2;
                if (enemies[i].y + BAT_HH > seg_min &&
                    enemies[i].y - BAT_HH < seg_max) {
                    if (enemies[i].vx > 0 &&
                        enemies[i].x + BAT_HW > x1 && enemies[i].x < x1) {
                        enemies[i].x = x1 - BAT_HW;
                        enemies[i].vx = -enemies[i].vx;
                    } else if (enemies[i].vx < 0 &&
                               enemies[i].x - BAT_HW < x1 && enemies[i].x > x1) {
                        enemies[i].x = x1 + BAT_HW;
                        enemies[i].vx = -enemies[i].vx;
                    }
                }
            }
            ehw = BAT_HW; ehh = BAT_HH;
        }

        enemies[i].anim++;

        if (box_overlap(player_x, player_y, PLAYER_HW, PLAYER_HH,
                        enemies[i].x, enemies[i].y, ehw, ehh)) {
            game_state = STATE_DYING;
            death_timer = DEATH_ANIM_TIME;
        }
    }
}

void check_miner_rescue(void) {
    if (!cur_has_miner) return;
    if (box_overlap(player_x, player_y, PLAYER_HW, PLAYER_HH,
                    cur_miner_x, cur_miner_y, MINER_HW, MINER_HH)) {
        score += 1000;
        score += player_fuel;
        score += player_dynamite * 50;
        game_state = STATE_LEVEL_COMPLETE;
        level_msg_timer = RESCUE_ANIM_TIME;
    }
}
