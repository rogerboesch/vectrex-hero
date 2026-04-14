//
// player.c — Player physics and input (tilemap scrolling version)
//

#include "game.h"
#include "tiles.h"

// =========================================================================
// Tile-based collision helpers
// =========================================================================

// Check if player bbox at (px, py) overlaps any solid tile
static u8 collides_x(s16 px, s16 py) {
    u8 left  = (u8)((px - PLAYER_HW) >> 3);
    u8 right = (u8)((px + PLAYER_HW) >> 3);
    u8 top   = (u8)((py - PLAYER_HH) >> 3);
    u8 bot   = (u8)((py + PLAYER_HH) >> 3);
    u8 ty;
    for (ty = top; ty <= bot; ty++) {
        if (tile_solid(left, ty)) return 1;
        if (tile_solid(right, ty)) return 1;
    }
    return 0;
}

static u8 collides_y(s16 px, s16 py) {
    u8 left  = (u8)((px - PLAYER_HW) >> 3);
    u8 right = (u8)((px + PLAYER_HW) >> 3);
    u8 top   = (u8)((py - PLAYER_HH) >> 3);
    u8 bot   = (u8)((py + PLAYER_HH) >> 3);
    u8 tx;
    for (tx = left; tx <= right; tx++) {
        if (tile_solid(tx, top)) return 1;
        if (tile_solid(tx, bot)) return 1;
    }
    return 0;
}

// =========================================================================
// Physics
// =========================================================================

void update_player_physics(void) {
    s16 new_px, new_py;

    // Gravity (y-down: positive vy = falling)
    player_vy += GRAVITY;
    if (player_vy > MAX_VEL_Y) player_vy = MAX_VEL_Y;
    if (player_vy < -MAX_VEL_Y) player_vy = -MAX_VEL_Y;

    // --- Move X ---
    new_px = player_px + player_vx;

    // Clamp to level bounds
    if (new_px < PLAYER_HW) new_px = PLAYER_HW;
    if (new_px > (s16)level_w * 8 - PLAYER_HW - 1) new_px = (s16)level_w * 8 - PLAYER_HW - 1;

    // Check tile collision — snap to tile edge on hit
    if (collides_x(new_px, player_py)) {
        if (player_vx > 0) {
            // Moving right: snap left edge of wall
            u8 wall_tx = (u8)((new_px + PLAYER_HW) >> 3);
            player_px = (s16)wall_tx * 8 - PLAYER_HW - 1;
        }
        else if (player_vx < 0) {
            // Moving left: snap right edge of wall
            u8 wall_tx = (u8)((new_px - PLAYER_HW) >> 3);
            player_px = (s16)(wall_tx + 1) * 8 + PLAYER_HW;
        }
        player_vx = 0;
    }
    else {
        player_px = new_px;
    }

    // --- Move Y ---
    new_py = player_py + player_vy;
    player_on_ground = 0;

    // Clamp to level bounds
    if (new_py < PLAYER_HH) new_py = PLAYER_HH;
    if (new_py > (s16)level_h * 8 - PLAYER_HH - 1) {
        new_py = (s16)level_h * 8 - PLAYER_HH - 1;
        player_vy = 0;
        player_on_ground = 1;
    }

    // Check tile collision
    if (collides_y(player_px, new_py)) {
        if (player_vy > 0) {
            // Falling: land on top of solid tile
            u8 land_ty = (u8)((new_py + PLAYER_HH) >> 3);
            player_py = (s16)land_ty * 8 - PLAYER_HH - 1;
            player_on_ground = 1;
        } else {
            // Rising: hit ceiling
            u8 ceil_ty = (u8)((new_py - PLAYER_HH) >> 3);
            player_py = (s16)(ceil_ty + 1) * 8 + PLAYER_HH;
        }
        player_vy = 0;
    } else {
        player_py = new_py;
    }

    // Extra ground check: is there a solid tile just below feet?
    if (!player_on_ground) {
        if (tile_solid((u8)(player_px >> 3), (u8)((player_py + PLAYER_HH + 1) >> 3)))
            player_on_ground = 1;
    }
}

// =========================================================================
// Input handling
// =========================================================================

void handle_input(void) {
    player_thrusting = 0;
    player_vx = 0;

    if (joy & J_LEFT) {
        player_vx = player_on_ground ? -WALK_SPEED : -MAX_VEL_X;
        player_facing = -1;
    }
    if (joy & J_RIGHT) {
        player_vx = player_on_ground ? WALK_SPEED : MAX_VEL_X;
        player_facing = 1;
    }
    if (joy & J_UP) {
        if (player_fuel > 0) {
            player_vy -= THRUST;
            if (player_vy < -MAX_VEL_Y) player_vy = -MAX_VEL_Y;
            player_fuel -= FUEL_DRAIN;
            player_thrusting = 1;
            player_on_ground = 0;
        }
    }
    if ((joy & J_DOWN) && player_on_ground) {
        place_dynamite();
    }
    if (joy_pressed & J_A) {
        fire_laser();
    }
    if (joy_pressed & J_B) {
        place_dynamite();
    }
}
