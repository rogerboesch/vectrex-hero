/*
 * enemies.c — Enemy AI, laser, dynamite, miner rescue
 *
 * Enemy types:
 *   ENEMY_BAT    — flies horizontally, bounces off walls and cave segments
 *   ENEMY_SPIDER — descends on a thread from home_y, patrols vertically
 *   ENEMY_SNAKE  — walks on floors, bounces off walls/segments/floor edges
 *
 * All enemies kill the player on contact (box_overlap collision).
 * Enemies can be killed by laser (50 pts) or dynamite explosion (50 pts).
 *
 * Dynamite lifecycle:
 *   1. place_dynamite() — drops at player's feet, starts fuse countdown
 *   2. Fuse burns for DYNAMITE_FUSE frames
 *   3. Explosion lasts EXPLOSION_TIME frames
 *   4. On first explosion frame: destroy walls, kill enemies, hurt player
 *
 * Laser:
 *   Fires from player sprite edge (based on facing direction) at
 *   half sprite height. Exists for LASER_LIFETIME frames. Hits any
 *   enemy whose bounding box intersects the laser line.
 */

#include "game.h"

/*
 * fire_laser — Create a laser beam from the player's current position.
 * Only one laser can be active at a time.
 * Origin X: player edge (left or right depending on player_facing)
 * Origin Y: player center height (half of PLAYER_HH below player_y)
 */
void fire_laser(void) {
    if (laser_active) return;
    laser_active = 1;
    laser_x = player_x + (player_facing * PLAYER_HW);
    laser_y = player_y - (PLAYER_HH / 2);
    laser_dir = player_facing;
    laser_timer = LASER_LIFETIME;
}

/*
 * update_laser — Count down laser timer, check for enemy hits.
 * The laser is a horizontal line from laser_x to laser_x + dir*LASER_LENGTH.
 * Hit detection uses a thin box (±3 pixels vertically).
 */
void update_laser(void) {
    uint8_t i;
    int8_t lx_end, lx_min, lx_max;
    if (!laser_active) return;

    laser_timer--;
    if (laser_timer == 0) {
        laser_active = 0;
        return;
    }

    /* Compute laser endpoint and min/max for overlap test */
    lx_end = laser_x + (laser_dir * LASER_LENGTH);
    if (laser_dir > 0) { lx_min = laser_x; lx_max = lx_end; }
    else { lx_min = lx_end; lx_max = laser_x; }

    /* Check each enemy for intersection with laser line */
    for (i = 0; i < enemy_count; i++) {
        int8_t ehw, ehh;
        if (!enemies[i].alive) continue;
        if (enemies[i].type == ENEMY_SPIDER) {
            ehw = SPIDER_HW; ehh = SPIDER_HH;
        }
        else if (enemies[i].type == ENEMY_SNAKE) {
            ehw = SNAKE_HW; ehh = SNAKE_HH;
        }
        else {
            ehw = BAT_HW; ehh = BAT_HH;
        }
        /* Laser is a thin horizontal line — check X overlap and
         * Y within ±3 pixels of laser height */
        if (enemies[i].x + ehw >= lx_min &&
            enemies[i].x - ehw <= lx_max &&
            enemies[i].y + ehh >= laser_y - 3 &&
            enemies[i].y - ehh <= laser_y + 3) {
            enemies[i].alive = 0;
            score += 50;
        }
    }
}

/*
 * place_dynamite — Drop a dynamite stick at the player's feet.
 * Only one dynamite can exist at a time (active or exploding).
 * Consumes one dynamite from player_dynamite inventory.
 */
void place_dynamite(void) {
    if (dyn_active || dyn_exploding) return;
    if (player_dynamite == 0) return;
    player_dynamite--;
    dyn_active = 1;
    dyn_x = player_x;
    dyn_y = player_y - PLAYER_HH + 4;  /* slightly above feet */
    dyn_timer = DYNAMITE_FUSE;
}

/*
 * update_dynamite — Handle fuse countdown and explosion effects.
 *
 * Two phases:
 *   1. Fuse burning (dyn_active && !dyn_exploding):
 *      Count down dyn_timer. When it hits 0, start explosion.
 *   2. Exploding (dyn_exploding):
 *      On the first explosion frame (timer == EXPLOSION_TIME-1):
 *        - Destroy nearby walls within EXPLOSION_RADIUS
 *        - Kill nearby enemies within EXPLOSION_RADIUS
 *        - Kill player if within EXPLOSION_KILL radius
 *      Count down to 0, then deactivate.
 */
void update_dynamite(void) {
    uint8_t i;
    int8_t wcx, wcy;

    /* Phase 1: Fuse burning */
    if (dyn_active && !dyn_exploding) {
        dyn_timer--;
        if (dyn_timer == 0) {
            dyn_exploding = 1;
            dyn_expl_timer = EXPLOSION_TIME;
        }
        return;
    }

    /* Phase 2: Explosion */
    if (dyn_exploding) {
        dyn_expl_timer--;

        /* First explosion frame: apply damage */
        if (dyn_expl_timer == EXPLOSION_TIME - 1) {
            /* Destroy nearby destructible walls */
            for (i = 0; i < cur_wall_count; i++) {
                if (walls_destroyed & (1 << i)) continue;
                wcx = wall_x(i) + (wall_w(i) / 2);
                wcy = wall_y(i);
                if (box_overlap(dyn_x, dyn_y, EXPLOSION_RADIUS, EXPLOSION_RADIUS,
                                wcx, wcy, wall_w(i) / 2, wall_h(i))) {
                    walls_destroyed = walls_destroyed | (uint8_t)(1 << i);
                    score += 75;
                    render_destroy_wall(i);  /* clear wall from screen + bg */
                }
            }

            /* Kill nearby enemies */
            for (i = 0; i < enemy_count; i++) {
                int8_t ehw, ehh;
                if (!enemies[i].alive) continue;
                if (enemies[i].type == ENEMY_SPIDER) {
                    ehw = SPIDER_HW; ehh = SPIDER_HH;
                }
                else if (enemies[i].type == ENEMY_SNAKE) {
                    ehw = SNAKE_HW; ehh = SNAKE_HH;
                }
                else {
                    ehw = BAT_HW; ehh = BAT_HH;
                }
                if (box_overlap(dyn_x, dyn_y, EXPLOSION_RADIUS, EXPLOSION_RADIUS,
                                enemies[i].x, enemies[i].y, ehw, ehh)) {
                    enemies[i].alive = 0;
                    score += 50;
                }
            }

            /* Kill player if too close to blast */
            if (box_overlap(dyn_x, dyn_y, EXPLOSION_KILL, EXPLOSION_KILL,
                            player_x, player_y, PLAYER_HW, PLAYER_HH)) {
                game_state = STATE_DYING;
                death_timer = DEATH_ANIM_TIME;
            }
        }

        /* Explosion finished — deactivate */
        if (dyn_expl_timer == 0) {
            dyn_active = 0;
            dyn_exploding = 0;
        }
    }
}

/*
 * update_enemies — Move all alive enemies, check player collision.
 *
 * Each enemy type has different movement:
 *   SPIDER: vertical patrol from home_y downward by SPIDER_PATROL units.
 *           Uses vx as vertical speed (reuses field, moves along Y).
 *   SNAKE:  horizontal movement, bounces off:
 *           - Room boundaries (cur_cave_left/right)
 *           - Vertical cave segments
 *           - Destructible walls
 *           - Floor edges (horizontal segments below snake)
 *   BAT:    horizontal movement, bounces off:
 *           - Room boundaries
 *           - Vertical cave segments
 *
 * All enemies increment anim counter for sprite animation selection.
 * All enemies kill the player on contact via box_overlap.
 */
void update_enemies(void) {
    uint8_t i, j;
    int8_t x1, y1, x2, y2, seg_min, seg_max;
    int8_t ehw, ehh;
    const int8_t *segs;

    for (i = 0; i < enemy_count; i++) {
        if (!enemies[i].alive) continue;

        if (enemies[i].type == ENEMY_SPIDER) {
            /* Spider: vertical patrol on thread.
             * vx is used as vertical velocity (moves along Y axis).
             * Bounces between home_y and home_y - SPIDER_PATROL. */
            enemies[i].y += enemies[i].vx;
            if (enemies[i].home_y - enemies[i].y >= SPIDER_PATROL) {
                enemies[i].y = enemies[i].home_y - SPIDER_PATROL;
                enemies[i].vx = -enemies[i].vx;
            }
            else if (enemies[i].y > enemies[i].home_y) {
                enemies[i].y = enemies[i].home_y;
                enemies[i].vx = -enemies[i].vx;
            }
            ehw = SPIDER_HW;
            ehh = SPIDER_HH;
        }
        else if (enemies[i].type == ENEMY_SNAKE) {
            /* Snake: horizontal movement along floor surfaces */
            enemies[i].x += enemies[i].vx;

            /* Bounce off room boundaries */
            if (enemies[i].x > cur_cave_right - SNAKE_HW ||
                enemies[i].x < cur_cave_left + SNAKE_HW) {
                enemies[i].vx = -enemies[i].vx;
            }

            /* Bounce off vertical cave segments */
            segs = cur_cave_segs;
            for (j = 0; j < cur_seg_count; j++) {
                if (segs[j * 4] != segs[j * 4 + 2]) continue;  /* skip non-vertical */
                x1 = segs[j * 4];
                y1 = segs[j * 4 + 1];
                y2 = segs[j * 4 + 3];
                seg_min = y1 < y2 ? y1 : y2;
                seg_max = y1 > y2 ? y1 : y2;
                if (enemies[i].y + SNAKE_HH > seg_min &&
                    enemies[i].y - SNAKE_HH < seg_max) {
                    if (enemies[i].vx > 0 &&
                        enemies[i].x + SNAKE_HW > x1 &&
                        enemies[i].x < x1) {
                        enemies[i].x = x1 - SNAKE_HW;
                        enemies[i].vx = -enemies[i].vx;
                    }
                    else if (enemies[i].vx < 0 &&
                               enemies[i].x - SNAKE_HW < x1 &&
                               enemies[i].x > x1) {
                        enemies[i].x = x1 + SNAKE_HW;
                        enemies[i].vx = -enemies[i].vx;
                    }
                }
            }

            /* Bounce off destructible walls (if not destroyed) */
            for (j = 0; j < cur_wall_count; j++) {
                int8_t wl, wr, wt, wb;
                if (walls_destroyed & (1 << j)) continue;
                wl = wall_x(j);
                wr = wl + wall_w(j);
                wt = wall_y(j) + wall_h(j);
                wb = wall_y(j) - wall_h(j);
                if (enemies[i].y + SNAKE_HH > wb &&
                    enemies[i].y - SNAKE_HH < wt) {
                    if (enemies[i].vx > 0 &&
                        enemies[i].x + SNAKE_HW > wl &&
                        enemies[i].x < wl) {
                        enemies[i].x = wl - SNAKE_HW;
                        enemies[i].vx = -enemies[i].vx;
                    }
                    else if (enemies[i].vx < 0 &&
                               enemies[i].x - SNAKE_HW < wr &&
                               enemies[i].x > wr) {
                        enemies[i].x = wr + SNAKE_HW;
                        enemies[i].vx = -enemies[i].vx;
                    }
                }
            }

            /* Floor-edge detection: reverse at the end of horizontal
             * cave segments that form the floor the snake walks on.
             * Looks for horizontal segments near the snake's feet. */
            segs = cur_cave_segs;
            for (j = 0; j < cur_seg_count; j++) {
                if (segs[j * 4 + 1] != segs[j * 4 + 3]) continue;  /* skip non-horizontal */
                y1 = segs[j * 4 + 1];
                /* Only consider segments near the snake's feet */
                if (y1 > enemies[i].y - SNAKE_HH) continue;
                if (y1 < enemies[i].y - SNAKE_HH - 5) continue;
                x1 = segs[j * 4];
                x2 = segs[j * 4 + 2];
                seg_min = x1 < x2 ? x1 : x2;
                seg_max = x1 > x2 ? x1 : x2;
                if (enemies[i].x >= seg_min && enemies[i].x <= seg_max) {
                    if (enemies[i].vx > 0 &&
                        enemies[i].x + SNAKE_HW >= seg_max) {
                        enemies[i].x = seg_max - SNAKE_HW;
                        enemies[i].vx = -enemies[i].vx;
                    }
                    else if (enemies[i].vx < 0 &&
                               enemies[i].x - SNAKE_HW <= seg_min) {
                        enemies[i].x = seg_min + SNAKE_HW;
                        enemies[i].vx = -enemies[i].vx;
                    }
                }
            }

            ehw = SNAKE_HW;
            ehh = SNAKE_HH;
        }
        else {
            /* Bat: horizontal flight, bounces off walls and segments */
            enemies[i].x += enemies[i].vx;

            /* Bounce off room boundaries */
            if (enemies[i].x > cur_cave_right - BAT_HW ||
                enemies[i].x < cur_cave_left + BAT_HW) {
                enemies[i].vx = -enemies[i].vx;
            }

            /* Bounce off vertical cave segments */
            segs = cur_cave_segs;
            for (j = 0; j < cur_seg_count; j++) {
                if (segs[j * 4] != segs[j * 4 + 2]) continue;  /* skip non-vertical */
                x1 = segs[j * 4];
                y1 = segs[j * 4 + 1];
                y2 = segs[j * 4 + 3];
                seg_min = y1 < y2 ? y1 : y2;
                seg_max = y1 > y2 ? y1 : y2;
                if (enemies[i].y + BAT_HH > seg_min &&
                    enemies[i].y - BAT_HH < seg_max) {
                    if (enemies[i].vx > 0 &&
                        enemies[i].x + BAT_HW > x1 &&
                        enemies[i].x < x1) {
                        enemies[i].x = x1 - BAT_HW;
                        enemies[i].vx = -enemies[i].vx;
                    }
                    else if (enemies[i].vx < 0 &&
                               enemies[i].x - BAT_HW < x1 &&
                               enemies[i].x > x1) {
                        enemies[i].x = x1 + BAT_HW;
                        enemies[i].vx = -enemies[i].vx;
                    }
                }
            }
            ehw = BAT_HW;
            ehh = BAT_HH;
        }

        enemies[i].anim++;  /* animation frame counter */

        /* Contact with player = instant death */
        if (box_overlap(player_x, player_y, PLAYER_HW, PLAYER_HH,
                        enemies[i].x, enemies[i].y, ehw, ehh)) {
            game_state = STATE_DYING;
            death_timer = DEATH_ANIM_TIME;
        }
    }
}

/*
 * check_miner_rescue — Check if player has reached the trapped miner.
 * Awards 1000 base points + remaining fuel + dynamite*50 as bonus.
 * Transitions to STATE_LEVEL_COMPLETE on rescue.
 */
void check_miner_rescue(void) {
    if (!cur_has_miner) return;
    if (box_overlap(player_x, player_y, PLAYER_HW, PLAYER_HH,
                    cur_miner_x, cur_miner_y, MINER_HW, MINER_HH)) {
        score += 1000;                    /* base rescue bonus */
        score += player_fuel;             /* remaining fuel bonus */
        score += player_dynamite * 50;    /* remaining dynamite bonus */
        game_state = STATE_LEVEL_COMPLETE;
        level_msg_timer = RESCUE_ANIM_TIME;
    }
}
