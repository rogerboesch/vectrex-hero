//
// player.c — Player physics, input, and drawing
//

#include "hero.h"

void update_player_physics(void) {
    uint8_t i;
    int8_t wt, wb, wl, wr;

    // Apply gravity
    player_vy -= GRAVITY;
    if (player_vy < -MAX_VEL_Y) player_vy = -MAX_VEL_Y;
    if (player_vy > MAX_VEL_Y) player_vy = MAX_VEL_Y;

    // --- Move X, resolve collisions ---
    player_x += player_vx;

    // Horizontal boundaries depend on where the player is
    // Shaft zone: player center is between shaft walls AND below ledge top
    if (player_x > SHAFT_LEFT && player_x < SHAFT_RIGHT &&
        player_y < LEDGE_Y) {
        // Inside shaft: constrain to shaft walls
        if (player_x - PLAYER_HW < SHAFT_LEFT) {
            player_x = SHAFT_LEFT + PLAYER_HW;
            player_vx = 0;
        }
        if (player_x + PLAYER_HW > SHAFT_RIGHT) {
            player_x = SHAFT_RIGHT - PLAYER_HW;
            player_vx = 0;
        }
    } else {
        // In ledge area or above: use cave outer walls
        if (player_x < CAVE_LEFT + PLAYER_HW) {
            player_x = CAVE_LEFT + PLAYER_HW;
            player_vx = 0;
        }
        if (player_x > CAVE_RIGHT - PLAYER_HW) {
            player_x = CAVE_RIGHT - PLAYER_HW;
            player_vx = 0;
        }
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

    // Ceiling collision - entire cave
    if (player_y + PLAYER_HH > CAVE_TOP) {
        player_y = CAVE_TOP - PLAYER_HH;
        player_vy = 0;
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
    zero_beam();
    set_scale(0x7F);
    moveto_d(player_y, player_x);
    set_scale(0x6A);

    // Outer silhouette: head + body + backpack as one continuous path
    moveto_d(8, -1);
    draw_line_d(0, 3);
    draw_line_d(-3, 0);
    draw_line_d(0, 1);
    draw_line_d(-8, 0);
    draw_line_d(0, -5);
    draw_line_d(3, 0);
    draw_line_d(0, -3);
    draw_line_d(5, 0);
    draw_line_d(0, 4);
    draw_line_d(3, 0);

    // Propeller rod (always visible)
    moveto_d(-3, -3);
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
