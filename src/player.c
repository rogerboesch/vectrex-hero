//
// player.c — Player physics, input, and drawing
//

#include "hero.h"
#include "sprites.h"

void update_player_physics(void) {
    uint8_t i;
    int8_t wt, wb, wl, wr;
    int8_t x1, y1, x2, y2, seg_min, seg_max;
    const int8_t *segs;

    // Apply gravity
    player_vy -= GRAVITY;
    if (player_vy < -MAX_VEL_Y) player_vy = -MAX_VEL_Y;
    if (player_vy > MAX_VEL_Y) player_vy = MAX_VEL_Y;

    // --- Move X, resolve collisions ---
    player_x += player_vx;

    // Horizontal boundaries — per-room bounds (skip if exit exists)
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

    // X-axis wall collisions: only when hitting from the side
    for (i = 0; i < cur_wall_count; i++) {
        if (walls_destroyed & (1 << i)) continue;
        if (player_hits_wall(i)) {
            wt = wall_y(i) + wall_h(i);
            if (player_y < wt) {
                wl = wall_x(i);
                wr = wall_x(i) + wall_w(i);
                if (player_vx > 0) {
                    player_x = wl - PLAYER_HW;
                } else if (player_vx < 0) {
                    player_x = wr + PLAYER_HW;
                }
                player_vx = 0;
            }
        }
    }

    // --- Move Y, resolve collisions ---
    player_y += player_vy;

    player_on_ground = 0;

    // Y-axis wall collisions (ledges, floor, destroyable)
    for (i = 0; i < cur_wall_count; i++) {
        if (walls_destroyed & (1 << i)) continue;
        if (player_hits_wall(i)) {
            wl = wall_x(i);
            wr = wall_x(i) + wall_w(i);
            // Skip if only touching edge (X collision already resolved)
            if (player_x + PLAYER_HW <= wl || player_x - PLAYER_HW >= wr) continue;
            wt = wall_y(i) + wall_h(i);
            wb = wall_y(i) - wall_h(i);
            if (player_vy <= 0) {
                // Falling - land on top
                player_y = wt + PLAYER_HH;
                player_vy = 0;
                player_on_ground = 1;
            } else {
                // Rising - hit underside
                player_y = wb - PLAYER_HH;
                player_vy = 0;
            }
        }
    }

    // Floor collision — per-room bounds (skip if bottom exit exists)
    if (room_exits[current_room * 4 + 3] == NONE &&
        player_y - PLAYER_HH < cur_cave_floor) {
        player_y = cur_cave_floor + PLAYER_HH;
        player_vy = 0;
        player_on_ground = 1;
    }

    // Ceiling collision — per-room bounds (skip if top exit exists)
    if (room_exits[current_room * 4 + 2] == NONE &&
        player_y + PLAYER_HH > cur_cave_top) {
        player_y = cur_cave_top - PLAYER_HH;
        player_vy = 0;
    }

    // Cave segment collision
    segs = cur_cave_segs;
    for (i = 0; i < cur_seg_count; i++) {
        x1 = segs[i * 4];
        y1 = segs[i * 4 + 1];
        x2 = segs[i * 4 + 2];
        y2 = segs[i * 4 + 3];

        if (y1 == y2) {
            // Horizontal segment
            seg_min = x1 < x2 ? x1 : x2;
            seg_max = x1 > x2 ? x1 : x2;
            if (player_x + PLAYER_HW > seg_min &&
                player_x - PLAYER_HW < seg_max) {
                if (player_y > y1 &&
                    player_y - PLAYER_HH < y1) {
                    player_y = y1 + PLAYER_HH;
                    player_vy = 0;
                    player_on_ground = 1;
                } else if (player_y < y1 &&
                           player_y + PLAYER_HH > y1) {
                    player_y = y1 - PLAYER_HH;
                    player_vy = 0;
                }
            }
        } else if (x1 == x2) {
            // Vertical segment
            seg_min = y1 < y2 ? y1 : y2;
            seg_max = y1 > y2 ? y1 : y2;
            if (player_y + PLAYER_HH > seg_min &&
                player_y - PLAYER_HH < seg_max) {
                if (player_x < x1 &&
                    player_x + PLAYER_HW > x1) {
                    player_x = x1 - PLAYER_HW;
                    player_vx = 0;
                } else if (player_x > x1 &&
                           player_x - PLAYER_HW < x1) {
                    player_x = x1 + PLAYER_HW;
                    player_vx = 0;
                }
            }
        }
    }
}

void handle_input(void) {
    controller_check_joysticks();

    player_thrusting = 0;
    player_vx = 0;

    if (controller_joystick_1_left()) {
        player_vx = player_on_ground ? -WALK_SPEED : -MAX_VEL_X;
        player_facing = -1;
    }
    if (controller_joystick_1_right()) {
        player_vx = player_on_ground ? WALK_SPEED : MAX_VEL_X;
        player_facing = 1;
    }

    if (controller_joystick_1_up()) {
        if (player_fuel > 0) {
            player_vy += THRUST;
            if (player_vy > MAX_VEL_Y) player_vy = MAX_VEL_Y;
            player_fuel -= FUEL_DRAIN;
            player_thrusting = 1;
            player_on_ground = 0;
        }
    }

    if (controller_joystick_1_down() && player_on_ground) {
        place_dynamite();
    }

    if (controller_button_1_1_pressed()) {
        fire_laser();
    }
}

void draw_player(void) {
    // Draw body from VLC sprite data
    zero_beam();
    set_scale(0x7F);
    moveto_d(player_y, player_x);
    set_scale(0x6A);
    draw_vlc(player);

    // Propeller rod (always visible)
    // VLC is a closed shape so beam is back at player center
    moveto_d(5, -4);
    draw_line_d(6, 0);

    if (!player_on_ground || player_thrusting) {
        // Spinning blades
        if (anim_tick & 4) {
            moveto_d(0, -4);
            draw_line_d(0, 8);
            moveto_d(-14, -1);
        } else {
            moveto_d(2, -3);
            draw_line_d(-4, 6);
            moveto_d(-12, 0);
        }
        draw_line_d(-4, 0);
        moveto_d(4, 3);
        draw_line_d(-4, 0);
    } else {
        // Static blades (horizontal)
        moveto_d(0, -4);
        draw_line_d(0, 8);
        moveto_d(-14, -1);
        // Legs with feet
        draw_line_d(-4, 0);
        draw_line_d(0, -2);
        moveto_d(4, 5);
        draw_line_d(-4, 0);
        draw_line_d(0, 2);
    }
}
