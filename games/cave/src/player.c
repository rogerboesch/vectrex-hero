//
// player.c — Cave Diver player: swimming physics, input, drawing
//

#include "cave.h"
#include "sprites.h"

void update_player_physics(void) {
    uint8_t i;
    int8_t wt, wb, wl, wr;

    // Apply gravity (reduced underwater)
    player_vy -= GRAVITY;
    if (player_vy < -MAX_VEL_Y) player_vy = -MAX_VEL_Y;
    if (player_vy > MAX_VEL_Y) player_vy = MAX_VEL_Y;

    // Move X
    player_x += player_vx;

    // Horizontal boundaries
    if (room_exits[current_room * 4 + 0] == NONE &&
        player_x < cur_cave_left + 4) {
        player_x = cur_cave_left + 4;
        player_vx = 0;
    }
    if (room_exits[current_room * 4 + 1] == NONE &&
        player_x > cur_cave_right - 4) {
        player_x = cur_cave_right - 4;
        player_vx = 0;
    }

    // Move Y
    player_y += player_vy;

    // Floor
    if (player_y < cur_cave_floor + 6) {
        player_y = cur_cave_floor + 6;
        player_vy = 0;
        player_on_ground = 1;
    }
    else {
        player_on_ground = 0;
    }

    // Ceiling
    if (player_y > cur_cave_top - 6) {
        player_y = cur_cave_top - 6;
        player_vy = 0;
    }

    // Wall collisions
    for (i = 0; i < cur_wall_count; i++) {
        if (room_walls_destroyed[current_room] & (1 << i)) continue;
        if (player_hits_wall(i)) {
            wt = wall_y(i) + wall_h(i) / 2;
            wb = wall_y(i) - wall_h(i) / 2;
            wl = wall_x(i) - wall_w(i) / 2;
            wr = wall_x(i) + wall_w(i) / 2;

            if (player_vx > 0 && player_x > wl) {
                player_x = wl - 4;
                player_vx = 0;
            }
            else if (player_vx < 0 && player_x < wr) {
                player_x = wr + 4;
                player_vx = 0;
            }
        }
    }

    // Room transitions
    if (player_y < ROOM_BOUND_FLOOR) {
        uint8_t next = room_exits[current_room * 4 + 3];
        if (next != NONE) {
            current_room = next;
            set_room_data();
            load_enemies();
            player_y = ROOM_BOUND_TOP;
        }
    }
    if (player_y > ROOM_BOUND_TOP) {
        uint8_t next = room_exits[current_room * 4 + 2];
        if (next != NONE) {
            current_room = next;
            set_room_data();
            load_enemies();
            player_y = ROOM_BOUND_FLOOR;
        }
    }
    if (player_x < ROOM_BOUND_LEFT) {
        uint8_t next = room_exits[current_room * 4 + 0];
        if (next != NONE) {
            current_room = next;
            set_room_data();
            load_enemies();
            player_x = ROOM_BOUND_RIGHT;
        }
    }
    if (player_x > ROOM_BOUND_RIGHT) {
        uint8_t next = room_exits[current_room * 4 + 1];
        if (next != NONE) {
            current_room = next;
            set_room_data();
            load_enemies();
            player_x = ROOM_BOUND_LEFT;
        }
    }
}

void handle_input(void) {
    controller_check_joysticks();

    player_vx = 0;
    player_swimming = 0;

    // Horizontal movement
    if (controller_joystick_1_left()) {
        player_vx = -WALK_SPEED;
        player_facing = -1;
    }
    else if (controller_joystick_1_right()) {
        player_vx = WALK_SPEED;
        player_facing = 1;
    }

    // Swim up (thrust)
    if (controller_joystick_1_up()) {
        player_vy += SWIM_THRUST;
        if (player_vy > MAX_VEL_Y) player_vy = MAX_VEL_Y;
        player_swimming = 1;
    }

    // Sonar pulse
    if (controller_button_1_1_pressed() && !sonar_active) {
        activate_sonar();
    }
}

void draw_player(void) {
    // Simple placeholder — draw a small shape at player position
    zero_beam();
    intensity_a(INTENSITY_BRIGHT);
    set_scale(0x40);
    moveto_d(player_y, player_x);
    draw_line_d(6, -4);
    draw_line_d(0, 8);
    draw_line_d(-6, -4);
}
