//
// enemies.c — Enemy AI, laser, dynamite, miner rescue (tilemap version)
//

#include "game.h"
#include "tiles.h"

// =========================================================================
// Laser
// =========================================================================

void fire_laser(void) {
    if (laser_active) return;
    laser_active = 1;
    laser_px = player_px;
    laser_py = player_py;
    laser_dir = player_facing;
    laser_timer = LASER_LIFETIME;
}

void update_laser(void) {
    uint8_t i;
    int16_t lx_end, lx_min, lx_max;
    int8_t ehw, ehh;
    if (!laser_active) return;

    laser_timer--;
    if (laser_timer == 0) { laser_active = 0; return; }

    lx_end = laser_px + (int16_t)(laser_dir * LASER_LENGTH);
    if (laser_dir > 0) { lx_min = laser_px; lx_max = lx_end; }
    else { lx_min = lx_end; lx_max = laser_px; }

    // Check laser vs tile collision (stop at walls)
    {
        int16_t check_x;
        int8_t step = (laser_dir > 0) ? 8 : -8;
        for (check_x = laser_px; ; check_x += step) {
            if (laser_dir > 0 && check_x > lx_max) break;
            if (laser_dir < 0 && check_x < lx_min) break;
            if (tile_solid((uint8_t)(check_x >> 3), (uint8_t)(laser_py >> 3))) {
                // Hit a wall — shorten laser
                if (laser_dir > 0) lx_max = check_x;
                else lx_min = check_x;
                break;
            }
        }
    }

    for (i = 0; i < active_enemy_count; i++) {
        if (!active_enemies[i].alive) continue;
        ActiveEnemy *ae = &active_enemies[i];
        if (ae->type == ENEMY_SPIDER) { ehw = SPIDER_HW; ehh = SPIDER_HH; }
        else if (ae->type == ENEMY_SNAKE) { ehw = SNAKE_HW; ehh = SNAKE_HH; }
        else { ehw = BAT_HW; ehh = BAT_HH; }
        if (ae->px + ehw >= lx_min &&
            ae->px - ehw <= lx_max &&
            ae->py + ehh >= laser_py - 3 &&
            ae->py - ehh <= laser_py + 3) {
            ae->alive = 0;
            level_entities[ae->ent_idx].alive = 0;
            score += 50;
        }
    }
}

// =========================================================================
// Dynamite
// =========================================================================

void place_dynamite(void) {
    if (dyn_active || dyn_exploding) return;
    if (player_dynamite == 0) return;
    player_dynamite--;
    dyn_active = 1;
    dyn_px = player_px;
    dyn_py = player_py + PLAYER_HH - 4;
    dyn_timer = DYNAMITE_FUSE;
}

void update_dynamite(void) {
    uint8_t i;
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
            // Destroy only destroyable wall tiles in explosion radius
            {
                uint8_t ctx = (uint8_t)(dyn_px >> 3);
                uint8_t cty = (uint8_t)(dyn_py >> 3);
                int8_t dx2;
                for (dx2 = -1; dx2 <= 1; dx2++) {
                    uint8_t ttx = ctx + dx2;
                    if (ttx >= level_w) continue;
                    /* Scan full column up and down for destroyable tiles */
                    uint8_t tty;
                    for (tty = cty; tty < level_h; tty++) {
                        uint8_t t = tile_at(ttx, tty);
                        if (t < TILE_DWALL_FIRST || t > TILE_DWALL_LAST) break;
                        decode_cache[tty & (DECODE_ROWS - 1)][ttx] = 0;
                        render_clear_tile(ttx, tty);
                        score += 75;
                    }
                    if (cty > 0) {
                        for (tty = cty - 1; tty < level_h; tty--) {
                            uint8_t t = tile_at(ttx, tty);
                            if (t < TILE_DWALL_FIRST || t > TILE_DWALL_LAST) break;
                            decode_cache[tty & (DECODE_ROWS - 1)][ttx] = 0;
                            render_clear_tile(ttx, tty);
                            score += 75;
                            if (tty == 0) break;
                        }
                    }
                }
            }

            // Kill enemies in radius
            for (i = 0; i < active_enemy_count; i++) {
                if (!active_enemies[i].alive) continue;
                ActiveEnemy *ae = &active_enemies[i];
                if (ae->type == ENEMY_SPIDER) { ehw = SPIDER_HW; ehh = SPIDER_HH; }
                else if (ae->type == ENEMY_SNAKE) { ehw = SNAKE_HW; ehh = SNAKE_HH; }
                else { ehw = BAT_HW; ehh = BAT_HH; }
                if (box_overlap(dyn_px, dyn_py, EXPLOSION_RADIUS, EXPLOSION_RADIUS,
                                ae->px, ae->py, ehw, ehh)) {
                    ae->alive = 0;
                    level_entities[ae->ent_idx].alive = 0;
                    score += 50;
                }
            }

            // Player damage
            if (box_overlap(dyn_px, dyn_py, EXPLOSION_KILL, EXPLOSION_KILL,
                            player_px, player_py, PLAYER_HW, PLAYER_HH)) {
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

// =========================================================================
// Active enemy management
// =========================================================================

#define ACTIVATE_MARGIN 80  // pixels beyond viewport

void activate_nearby_enemies(void) {
    uint8_t i;
    int16_t ax0 = cam_x - ACTIVATE_MARGIN;
    int16_t ax1 = cam_x + SCREEN_W + ACTIVATE_MARGIN;
    int16_t ay0 = cam_y - ACTIVATE_MARGIN;
    int16_t ay1 = cam_y + PLAY_H + ACTIVATE_MARGIN;

    // Deactivate far enemies
    for (i = 0; i < active_enemy_count; ) {
        ActiveEnemy *ae = &active_enemies[i];
        if (ae->px < ax0 - 64 || ae->px > ax1 + 64 ||
            ae->py < ay0 - 64 || ae->py > ay1 + 64) {
            // Remove from active list
            active_enemies[i] = active_enemies[active_enemy_count - 1];
            active_enemy_count--;
        } else {
            i++;
        }
    }

    // Activate nearby entities
    for (i = 0; i < level_entity_count && active_enemy_count < MAX_ACTIVE_ENEMIES; i++) {
        LevelEntity *le = &level_entities[i];
        if (!le->alive) continue;
        if (le->type == 0 || le->type == 4) continue; // skip player_start and miner

        int16_t epx = (int16_t)le->tx * 8 + 4;
        int16_t epy = (int16_t)le->ty * 8 + 4;

        if (epx < ax0 || epx > ax1 || epy < ay0 || epy > ay1) continue;

        // Check if already active
        uint8_t already = 0;
        uint8_t j;
        for (j = 0; j < active_enemy_count; j++) {
            if (active_enemies[j].ent_idx == i) { already = 1; break; }
        }
        if (already) continue;

        // Spawn active enemy
        ActiveEnemy *ae = &active_enemies[active_enemy_count++];
        ae->px = epx;
        ae->py = epy;
        ae->home_py = epy;
        ae->vx = le->vx;
        if (ae->vx == 0) ae->vx = 1; // default velocity
        ae->alive = 1;
        ae->anim = 0;
        ae->type = le->type;
        ae->ent_idx = i;
    }
}

// =========================================================================
// Enemy AI update
// =========================================================================

void update_active_enemies(void) {
    uint8_t i;
    int8_t ehw, ehh;

    for (i = 0; i < active_enemy_count; i++) {
        ActiveEnemy *ae = &active_enemies[i];
        if (!ae->alive) continue;

        if (ae->type == ENEMY_SPIDER) {
            // Vertical patrol
            ae->py += ae->vx;
            if (ae->home_py - ae->py >= SPIDER_PATROL) {
                ae->py = ae->home_py - SPIDER_PATROL;
                ae->vx = -ae->vx;
            } else if (ae->py > ae->home_py) {
                ae->py = ae->home_py;
                ae->vx = -ae->vx;
            }
            // Tile collision (vertical)
            if (tile_solid((uint8_t)(ae->px >> 3), (uint8_t)((ae->py - SPIDER_HH) >> 3)) ||
                tile_solid((uint8_t)(ae->px >> 3), (uint8_t)((ae->py + SPIDER_HH) >> 3))) {
                ae->vx = -ae->vx;
                ae->py += ae->vx;
            }
            ehw = SPIDER_HW; ehh = SPIDER_HH;

        } else if (ae->type == ENEMY_SNAKE) {
            ae->px += ae->vx;
            // Tile collision ahead
            if (ae->vx > 0) {
                if (tile_solid((uint8_t)((ae->px + SNAKE_HW) >> 3), (uint8_t)(ae->py >> 3)))
                    ae->vx = -ae->vx;
            } else {
                if (tile_solid((uint8_t)((ae->px - SNAKE_HW) >> 3), (uint8_t)(ae->py >> 3)))
                    ae->vx = -ae->vx;
            }
            // Floor check: if no ground below, turn around
            if (!tile_solid((uint8_t)(ae->px >> 3), (uint8_t)((ae->py + SNAKE_HH + 1) >> 3))) {
                ae->vx = -ae->vx;
                ae->px += ae->vx;
            }
            ehw = SNAKE_HW; ehh = SNAKE_HH;

        } else {
            // Bat: horizontal patrol
            ae->px += ae->vx;
            // Tile collision ahead
            if (ae->vx > 0) {
                if (tile_solid((uint8_t)((ae->px + BAT_HW) >> 3), (uint8_t)(ae->py >> 3)))
                    ae->vx = -ae->vx;
            } else {
                if (tile_solid((uint8_t)((ae->px - BAT_HW) >> 3), (uint8_t)(ae->py >> 3)))
                    ae->vx = -ae->vx;
            }
            ehw = BAT_HW; ehh = BAT_HH;
        }

        ae->anim++;

        // Enemy-player collision
        if (box_overlap(player_px, player_py, PLAYER_HW, PLAYER_HH,
                        ae->px, ae->py, ehw, ehh)) {
            game_state = STATE_DYING;
            death_timer = DEATH_ANIM_TIME;
        }
    }
}

// =========================================================================
// Miner rescue
// =========================================================================

void check_miner_rescue(void) {
    if (!miner_active) return;
    if (box_overlap(player_px, player_py, PLAYER_HW, PLAYER_HH,
                    miner_px, miner_py, MINER_HW, MINER_HH)) {
        score += 1000;
        score += player_fuel;
        score += player_dynamite * 50;
        game_state = STATE_LEVEL_COMPLETE;
        level_msg_timer = RESCUE_ANIM_TIME;
    }
}
