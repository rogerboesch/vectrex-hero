/*
 * player.c -- Player physics and input handling (Apple II port)
 *
 * This file is a direct port from ql/src/player.c. The physics and
 * collision logic are identical; only the header include differs.
 * All platform-specific constants (GRAVITY, THRUST, etc.) are in game.h,
 * scaled for the Apple II's ~10 FPS target.
 *
 * Physics model:
 *   - Gravity pulls player down each frame (GRAVITY units)
 *   - Thrust (UP key) adds upward velocity, drains fuel
 *   - Horizontal movement is instantaneous (no inertia on ground)
 *   - In air, horizontal speed is MAX_VEL_X; on ground, WALK_SPEED
 *
 * Collision resolution order:
 *   1. Apply X velocity, resolve X collisions (walls, boundaries)
 *   2. Apply Y velocity, resolve Y collisions (walls, floor, ceiling)
 *   3. Check cave segment collisions (H and V segments separately)
 */

#include "game.h"

/*
 * update_player_physics -- Apply gravity, velocity, and resolve all
 * collisions with room boundaries, destructible walls, and cave
 * geometry segments.
 */
void update_player_physics(void) {
    uint8_t i;
    int8_t wt, wb, wl, wr;
    int8_t x1, y1, x2, y2, seg_min, seg_max;
    int8_t prev_x, prev_y;
    const int8_t *segs;

    /* Apply gravity */
    player_vy -= GRAVITY;
    if (player_vy < -MAX_VEL_Y) player_vy = -MAX_VEL_Y;
    if (player_vy > MAX_VEL_Y) player_vy = MAX_VEL_Y;

    /* === X-axis: move horizontally, then resolve collisions === */
    prev_x = player_x;
    player_x += player_vx;

    /* Room boundary clamping (only if no exit in that direction) */
    if (room_exits[current_room * 4 + 0] == NONE &&
        player_x < cur_cave_left + PLAYER_HW) {
        player_x = cur_cave_left + PLAYER_HW;
        player_vx = 0;
    }
    if (room_exits[current_room * 4 + 1] == NONE &&
        player_x > cur_cave_right - PLAYER_HW) {
        player_x = cur_cave_right - PLAYER_HW;
        player_vx = 0;
    }

    /* X-axis collision with destructible walls */
    for (i = 0; i < cur_wall_count; i++) {
        if (walls_destroyed & (1 << i)) continue;
        if (player_hits_wall(i)) {
            wt = wall_y(i) + wall_h(i);
            if (player_y < wt) {
                wl = wall_x(i);
                wr = wall_x(i) + wall_w(i);
                if (player_vx > 0) {
                    player_x = wl - PLAYER_HW;
                }
                else if (player_vx < 0) {
                    player_x = wr + PLAYER_HW;
                }
                player_vx = 0;
            }
        }
    }

    /* X-axis collision with vertical cave segments (swept check) */
    segs = cur_cave_segs;
    for (i = 0; i < cur_seg_count; i++) {
        x1 = segs[i * 4];
        y1 = segs[i * 4 + 1];
        x2 = segs[i * 4 + 2];
        y2 = segs[i * 4 + 3];
        if (x1 == x2) {
            seg_min = y1 < y2 ? y1 : y2;
            seg_max = y1 > y2 ? y1 : y2;
            if (player_y + PLAYER_HH > seg_min &&
                player_y - PLAYER_HH < seg_max) {
                if (player_vx > 0 &&
                    prev_x + PLAYER_HW <= x1 &&
                    player_x + PLAYER_HW > x1) {
                    player_x = x1 - PLAYER_HW;
                    player_vx = 0;
                }
                else if (player_vx < 0 &&
                           prev_x - PLAYER_HW >= x1 &&
                           player_x - PLAYER_HW < x1) {
                    player_x = x1 + PLAYER_HW;
                    player_vx = 0;
                }
            }
        }
    }

    /* === Y-axis: move vertically, then resolve collisions === */
    prev_y = player_y;
    player_y += player_vy;
    player_on_ground = 0;

    /* Y-axis collision with destructible walls */
    for (i = 0; i < cur_wall_count; i++) {
        if (walls_destroyed & (1 << i)) continue;
        if (player_hits_wall(i)) {
            wl = wall_x(i);
            wr = wall_x(i) + wall_w(i);
            if (player_x + PLAYER_HW <= wl || player_x - PLAYER_HW >= wr) continue;
            wt = wall_y(i) + wall_h(i);
            wb = wall_y(i) - wall_h(i);
            if (player_vy <= 0) {
                player_y = wt + PLAYER_HH;
                player_vy = 0;
                player_on_ground = 1;
            }
            else {
                player_y = wb - PLAYER_HH;
                player_vy = 0;
            }
        }
    }

    /* Floor collision (only if no downward exit) */
    if (room_exits[current_room * 4 + 3] == NONE &&
        player_y - PLAYER_HH < cur_cave_floor) {
        player_y = cur_cave_floor + PLAYER_HH;
        player_vy = 0;
        player_on_ground = 1;
    }

    /* Ceiling collision (only if no upward exit) */
    if (room_exits[current_room * 4 + 2] == NONE &&
        player_y + PLAYER_HH > cur_cave_top) {
        player_y = cur_cave_top - PLAYER_HH;
        player_vy = 0;
    }

    /* Y-axis collision with horizontal cave segments (swept check) */
    for (i = 0; i < cur_seg_count; i++) {
        x1 = segs[i * 4];
        y1 = segs[i * 4 + 1];
        x2 = segs[i * 4 + 2];
        y2 = segs[i * 4 + 3];
        if (y1 == y2) {
            seg_min = x1 < x2 ? x1 : x2;
            seg_max = x1 > x2 ? x1 : x2;
            if (player_x + PLAYER_HW > seg_min &&
                player_x - PLAYER_HW < seg_max) {
                if (player_vy <= 0 &&
                    prev_y - PLAYER_HH >= y1 &&
                    player_y - PLAYER_HH < y1) {
                    player_y = y1 + PLAYER_HH;
                    player_vy = 0;
                    player_on_ground = 1;
                }
                else if (player_vy > 0 &&
                           prev_y + PLAYER_HH <= y1 &&
                           player_y + PLAYER_HH > y1) {
                    player_y = y1 - PLAYER_HH;
                    player_vy = 0;
                }
            }
        }
    }
}

/*
 * handle_input -- Translate key states into player actions.
 *
 * Apple II key mapping (set in a2_hw.s):
 *   LEFT/A  -> move left
 *   RIGHT/D -> move right (note: D is also dynamite on QL, remap needed)
 *   UP/W    -> thrust
 *   DOWN/S  -> place dynamite (on ground)
 *   Z       -> place dynamite (alternative, works in air)
 *   SPACE/X -> fire laser
 */
void handle_input(void) {
    player_thrusting = 0;
    player_vx = 0;

    if (key_left) {
        player_vx = player_on_ground ? -WALK_SPEED : -MAX_VEL_X;
        player_facing = -1;
    }
    if (key_right) {
        player_vx = player_on_ground ? WALK_SPEED : MAX_VEL_X;
        player_facing = 1;
    }

    /* Thrust */
    if (key_up) {
        if (player_fuel > 0) {
            player_vy += THRUST;
            if (player_vy > MAX_VEL_Y) player_vy = MAX_VEL_Y;
            player_fuel -= FUEL_DRAIN;
            player_thrusting = 1;
            player_on_ground = 0;
        }
    }

    /* Dynamite placement */
    if (key_down && player_on_ground) {
        place_dynamite();
    }

    if (key_d_pressed) {
        place_dynamite();
    }

    /* Laser */
    if (key_space_pressed) {
        fire_laser();
    }
}
