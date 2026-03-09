//
// H.E.R.O. for Vectrex
// A conversion of Activision's H.E.R.O. (1984)
// Built with VectreC toolchain + CMOC
//

#include <vectrex.h>
#include <vectrex/stdlib.h>

#pragma vx_copyright "2026"
#pragma vx_title_pos 0,-80
#pragma vx_title_size -8, 80
#pragma vx_title "g HERO"
#pragma vx_music vx_music_1

// =========================================================================
// Constants
// =========================================================================

#define GRAVITY         1
#define THRUST          3
#define MAX_VEL_Y       2
#define MAX_VEL_X       3
#define WALK_SPEED      2
#define FUEL_DRAIN      1
#define START_FUEL      255
#define START_DYNAMITE  3
#define START_LIVES     3
#define LASER_LENGTH    40
#define LASER_LIFETIME  10
#define DYNAMITE_FUSE   60
#define EXPLOSION_TIME  20
#define EXPLOSION_RADIUS 25
#define EXPLOSION_KILL   10
#define MAX_ENEMIES     3
#define PLAYER_HW       6
#define PLAYER_HH       7
#define BAT_HW          5
#define BAT_HH          4
#define MINER_HW        4
#define MINER_HH        5
#define NONE            255

// Game states
#define STATE_TITLE         0
#define STATE_PLAYING       1
#define STATE_DYING         2
#define STATE_LEVEL_COMPLETE 3
#define STATE_GAME_OVER     4

// =========================================================================
// Cave geometry constants
// =========================================================================

// Outer cave walls
#define CAVE_LEFT   -90
#define CAVE_RIGHT   90
#define CAVE_TOP     105
#define CAVE_FLOOR  -95

// Ledge height (top of ledge surface)
#define LEDGE_Y      65

// Shaft boundaries
#define SHAFT_LEFT  -25
#define SHAFT_RIGHT  25

// =========================================================================
// Vector art data
// =========================================================================

// Player sprite drawn procedurally in draw_player() for detailed shape:
// head, body, backpack, propeller (when flying), legs with feet

// Bat - wings spread
static int8_t bat_frame1[] = {
    5,
    0,   -6,    // left wing out
    4,   3,     // left wing up
    0,   6,     // body across
    4,   3,     // right wing up
    0,   -6     // right wing back
};

// Bat - wings down
static int8_t bat_frame2[] = {
    5,
    0,   -6,    // left wing out
    -3,  3,     // left wing down
    0,   6,     // body across
    -3,  3,     // right wing down
    0,   -6     // right wing back
};

// Miner (rescue target)
static int8_t miner_shape[] = {
    5,
    5,   0,     // head up
    -5,  0,     // back down
    -6,  0,     // body
    0,   -3,    // left leg
    0,   6      // right leg
};

// Dynamite stick
static int8_t dynamite_shape[] = {
    3,
    0,   3,     // top
    -6,  0,     // side
    0,   -3     // bottom
};

// =========================================================================
// Level data
// =========================================================================

// Level 1 layout (matching original H.E.R.O. level 1):
//
//   HERO
//  (start)
//  (-90,120)       (-25,120)========(90,120)  <- ceiling
//      |                              |
//  (-90,80)===(-25,80)    (25,80)==(90,80)    <- ledges
//                  |        |
//                  | shaft  |
//                  |  BAT   |
//                  |        |
//             (-25,-85)==(25,-85)              <- floor
//                  MINER
//
// Hero starts on left ledge, drops into shaft, reaches miner.

// Collision walls for level 1 (horizontal surfaces only)
// Shaft walls handled by code
#define L1_WALL_COUNT 3
static const int8_t l1_walls[] = {
    // y, x, h, w  (center-y, left-x, half-height, width)
    // wall_top = y + h should match the visual line
    LEDGE_Y - 8, CAVE_LEFT, 8, SHAFT_LEFT - CAVE_LEFT,  // left ledge
    LEDGE_Y - 8, SHAFT_RIGHT, 8, CAVE_RIGHT - SHAFT_RIGHT, // right ledge
    CAVE_FLOOR - 8, SHAFT_LEFT, 8, SHAFT_RIGHT - SHAFT_LEFT // floor
};

// Level 1 enemies
#define L1_ENEMY_COUNT 1
static const int8_t l1_enemies[] = {
    // x, y, vx
    0, 0, 1     // bat in middle of shaft
};

// Level 2 layout: same shape but with a destroyable wall across shaft
#define L2_WALL_COUNT 4
static const int8_t l2_walls[] = {
    LEDGE_Y - 8, CAVE_LEFT, 8, SHAFT_LEFT - CAVE_LEFT,    // left ledge
    LEDGE_Y - 8, SHAFT_RIGHT, 8, CAVE_RIGHT - SHAFT_RIGHT,// right ledge
    CAVE_FLOOR - 8, SHAFT_LEFT, 8, SHAFT_RIGHT - SHAFT_LEFT, // floor
    12, SHAFT_LEFT, 8, SHAFT_RIGHT - SHAFT_LEFT               // destroyable wall
};

#define L2_ENEMY_COUNT 2
static const int8_t l2_enemies[] = {
    -10, 0, 1,     // bat below destroyable wall
    0, 50, -1      // bat above in entry area
};

// =========================================================================
// Game state variables
// =========================================================================

int8_t player_x;
int8_t player_y;
int8_t player_vx;
int8_t player_vy;
int8_t player_facing;
uint8_t player_fuel;
uint8_t player_dynamite;
uint8_t player_lives;
uint8_t player_on_ground;
uint8_t player_thrusting;
uint8_t anim_tick;

int score;

uint8_t game_state;
uint8_t current_level;
uint8_t death_timer;
uint8_t level_msg_timer;

// Enemies
struct {
    int8_t x;
    int8_t y;
    int8_t vx;
    uint8_t alive;
    uint8_t anim;
} enemies[MAX_ENEMIES];
uint8_t enemy_count;

// Laser
uint8_t laser_active;
int8_t laser_x;
int8_t laser_y;
int8_t laser_dir;
uint8_t laser_timer;

// Dynamite
uint8_t dyn_active;
int8_t dyn_x;
int8_t dyn_y;
uint8_t dyn_timer;
uint8_t dyn_exploding;
uint8_t dyn_expl_timer;

// Destroyed walls (bitfield)
uint8_t walls_destroyed;

// Current level pointers
const int8_t *cur_walls;
uint8_t cur_wall_count;
const int8_t *cur_enemies_data;
uint8_t cur_enemy_count;
int8_t cur_miner_x;
int8_t cur_miner_y;

char str_buf[16];

// =========================================================================
// Helper functions
// =========================================================================

void draw_sprite(int8_t y, int8_t x, int8_t *shape) {
    zero_beam();
    set_scale(0x7F);
    moveto_d(y, x);
    set_scale(0x60);
    draw_vlc(shape);
}

uint8_t box_overlap(int8_t ax, int8_t ay, int8_t ahw, int8_t ahh,
                    int8_t bx, int8_t by, int8_t bhw, int8_t bhh) {
    if (ax + ahw < bx - bhw) return 0;
    if (ax - ahw > bx + bhw) return 0;
    if (ay + ahh < by - bhh) return 0;
    if (ay - ahh > by + bhh) return 0;
    return 1;
}

// Get wall data from flat array: each wall is 4 bytes (y, x, h, w)
int8_t wall_y(uint8_t i)  { return cur_walls[i * 4]; }
int8_t wall_x(uint8_t i)  { return cur_walls[i * 4 + 1]; }
int8_t wall_h(uint8_t i)  { return cur_walls[i * 4 + 2]; }
int8_t wall_w(uint8_t i)  { return cur_walls[i * 4 + 3]; }

uint8_t player_hits_wall(uint8_t i) {
    int8_t wcx = wall_x(i) + (wall_w(i) / 2);
    int8_t wcy = wall_y(i);
    int8_t whw = wall_w(i) / 2;
    int8_t whh = wall_h(i);
    return box_overlap(player_x, player_y, PLAYER_HW, PLAYER_HH,
                       wcx, wcy, whw, whh);
}

// =========================================================================
// Level management
// =========================================================================

void set_level_data(void) {
    if (current_level == 0) {
        cur_walls = l1_walls;
        cur_wall_count = L1_WALL_COUNT;
        cur_enemies_data = l1_enemies;
        cur_enemy_count = L1_ENEMY_COUNT;
        cur_miner_x = 0;
        cur_miner_y = CAVE_FLOOR + 8;
    } else {
        cur_walls = l2_walls;
        cur_wall_count = L2_WALL_COUNT;
        cur_enemies_data = l2_enemies;
        cur_enemy_count = L2_ENEMY_COUNT;
        cur_miner_x = 0;
        cur_miner_y = CAVE_FLOOR + 8;
    }
}

void load_enemies(void) {
    uint8_t i;
    enemy_count = cur_enemy_count;
    for (i = 0; i < MAX_ENEMIES; i++) {
        if (i < enemy_count) {
            enemies[i].x = cur_enemies_data[i * 3];
            enemies[i].y = cur_enemies_data[i * 3 + 1];
            enemies[i].vx = cur_enemies_data[i * 3 + 2];
            enemies[i].alive = 1;
            enemies[i].anim = 0;
        } else {
            enemies[i].alive = 0;
        }
    }
    walls_destroyed = 0;
}

void init_level(void) {
    set_level_data();
    // Hero starts top-left corner of cave
    player_x = CAVE_LEFT + 15;
    player_y = CAVE_TOP - 2;
    player_vx = 0;
    player_vy = 0;
    player_facing = 1;
    player_on_ground = 0;
    player_thrusting = 0;
    laser_active = 0;
    dyn_active = 0;
    dyn_exploding = 0;
    anim_tick = 0;
    load_enemies();
    level_msg_timer = 30;
}

void start_new_game(void) {
    score = 0;
    player_lives = START_LIVES;
    player_fuel = START_FUEL;
    player_dynamite = START_DYNAMITE;
    current_level = 0;
    init_level();
    game_state = STATE_PLAYING;
}

// =========================================================================
// Player physics
// =========================================================================

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
    // (skip if player center is above wall top = standing on it)
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

// =========================================================================
// Laser
// =========================================================================

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

// =========================================================================
// Dynamite
// =========================================================================

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

// =========================================================================
// Enemies
// =========================================================================

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

// =========================================================================
// Check miner rescue
// =========================================================================

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

// =========================================================================
// Input handling
// =========================================================================

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

// =========================================================================
// Drawing functions
// =========================================================================

// Draw the cave outline as connected polylines (very efficient)
void draw_cave(void) {
    uint8_t i;

    // Left wall path: top-left -> ledge -> shaft left -> floor
    // Only 1 zero_beam call for this entire path!
    zero_beam();
    set_scale(0x7F);
    moveto_d(CAVE_TOP, CAVE_LEFT);
    draw_line_d(LEDGE_Y - CAVE_TOP, 0);         // left wall down to ledge
    draw_line_d(0, SHAFT_LEFT - CAVE_LEFT);      // ledge right to shaft
    draw_line_d(-40, 0);                          // shaft left wall (half 1)
    draw_line_d(CAVE_FLOOR - LEDGE_Y + 40, 0);   // shaft left wall (half 2)
    draw_line_d(0, SHAFT_RIGHT - SHAFT_LEFT);    // floor

    // Right wall path: ceiling -> right wall -> ledge -> shaft right
    // Only 1 zero_beam call!
    zero_beam();
    moveto_d(CAVE_TOP, SHAFT_LEFT);
    draw_line_d(0, CAVE_RIGHT - SHAFT_LEFT);     // ceiling right
    draw_line_d(LEDGE_Y - CAVE_TOP, 0);          // right wall down to ledge
    draw_line_d(0, SHAFT_RIGHT - CAVE_RIGHT);    // ledge left to shaft
    draw_line_d(-40, 0);                          // shaft right wall (half 1)
    draw_line_d(CAVE_FLOOR - LEDGE_Y + 40, 0);   // shaft right wall (half 2)

    // Draw any destroyable walls that still exist
    for (i = 0; i < cur_wall_count; i++) {
        if (walls_destroyed & (1 << i)) continue;
        // Only draw walls that aren't part of permanent geometry
        // (walls at index >= 3 are destroyable in level 2)
        if (current_level == 1 && i == 3) {
            zero_beam();
            moveto_d(wall_y(i) + wall_h(i), wall_x(i));
            draw_line_d(0, wall_w(i));
            draw_line_d(-wall_h(i) * 2, 0);
            draw_line_d(0, -wall_w(i));
            draw_line_d(wall_h(i) * 2, 0);
        }
    }
}

void draw_enemies(void) {
    uint8_t i;
    int8_t *frame;
    for (i = 0; i < enemy_count; i++) {
        if (!enemies[i].alive) continue;
        frame = (enemies[i].anim & 8) ? bat_frame1 : bat_frame2;
        draw_sprite(enemies[i].y, enemies[i].x, frame);
    }
}

void draw_player(void) {
    zero_beam();
    set_scale(0x7F);
    moveto_d(player_y, player_x);
    set_scale(0x6A);
    // beam at local (0,0) = player center

    // Outer silhouette: head + body + backpack as one continuous path
    // Clockwise from head top-left
    moveto_d(8, -1);           // (8, -1)
    draw_line_d(0, 3);         // head top       → (8, 2)
    draw_line_d(-3, 0);        // head right      → (5, 2)
    draw_line_d(0, 1);         // shoulder step   → (5, 3)
    draw_line_d(-8, 0);        // body right      → (-3, 3)
    draw_line_d(0, -5);        // body bottom     → (-3, -2)
    draw_line_d(3, 0);         // up to backpack  → (0, -2)
    draw_line_d(0, -3);        // backpack bottom → (0, -5)
    draw_line_d(5, 0);         // backpack left   → (5, -5)
    draw_line_d(0, 4);         // backpack top    → (5, -1)
    draw_line_d(3, 0);         // head left       → (8, -1)
    // beam at (8, -1)

    // Propeller rod (always visible)
    moveto_d(-3, -3);          // (5, -4)
    draw_line_d(6, 0);         // rod up → (11, -4)

    if (!player_on_ground || player_thrusting) {
        // Spinning blades
        if (anim_tick & 4) {
            moveto_d(0, -4);   // (11, -8)
            draw_line_d(0, 8); // blades → (11, 0)
            moveto_d(-14, -1); // to (-3, -1)
        } else {
            moveto_d(2, -3);   // (13, -7)
            draw_line_d(-4, 6);// blades → (9, -1)
            moveto_d(-12, 0);  // to (-3, -1)
        }
        draw_line_d(-4, 0);    // left leg  → (-7, -1)
        moveto_d(4, 3);        // to (-3, 2)
        draw_line_d(-4, 0);    // right leg → (-7, 2)
    } else {
        // Static blades (horizontal)
        moveto_d(0, -4);       // (11, -8)
        draw_line_d(0, 8);     // blades → (11, 0)
        moveto_d(-14, -1);     // to (-3, -1)
        // Legs with feet
        draw_line_d(-4, 0);    // left leg  → (-7, -1)
        draw_line_d(0, -2);    // left foot → (-7, -3)
        moveto_d(4, 5);        // to (-3, 2)
        draw_line_d(-4, 0);    // right leg → (-7, 2)
        draw_line_d(0, 2);     // right foot → (-7, 4)
    }
}

void draw_laser_beam(void) {
    if (!laser_active) return;
    zero_beam();
    intensity_a(0xFF);
    set_scale(0x7F);
    moveto_d(laser_y, laser_x);
    draw_line_d(0, laser_dir * LASER_LENGTH);
    intensity_a(0x7F);
}

void draw_dynamite_and_explosion(void) {
    uint8_t r;
    if (!dyn_active) return;

    if (!dyn_exploding) {
        zero_beam();
        set_scale(0x7F);
        moveto_d(dyn_y, dyn_x - 2);
        set_scale(0x30);
        draw_line_d(0, 4);
        draw_line_d(-6, 0);
        draw_line_d(0, -4);
        draw_line_d(6, 0);
        if (dyn_timer & 2) {
            // fuse spark
            intensity_a(0xFF);
            draw_line_d(3, 0);
            intensity_a(0x7F);
        }
    } else {
        r = (EXPLOSION_TIME - dyn_expl_timer) * 3;
        if (r > EXPLOSION_RADIUS) r = EXPLOSION_RADIUS;
        zero_beam();
        intensity_a(0xFF);
        set_scale(0x7F);
        moveto_d(dyn_y + r, dyn_x - r);
        draw_line_d(-r, r);
        draw_line_d(-r, r);
        zero_beam();
        moveto_d(dyn_y + r, dyn_x + r);
        draw_line_d(-r, -r);
        draw_line_d(-r, -r);
        intensity_a(0x7F);
    }
}

void draw_miner(void) {
    if (anim_tick & 8) {
        intensity_a(0xFF);
    }
    draw_sprite(cur_miner_y, cur_miner_x, miner_shape);
    intensity_a(0x7F);
}

void draw_hud(void) {
    int8_t f, l;
    f = (int8_t)(player_fuel >> 1);
    l = (int8_t)player_lives;
    zero_beam();
    set_scale(0x7F);
    sprintf(str_buf, "%d ", score);
    print_str_c(127, -125, str_buf);
    zero_beam();
    sprintf(str_buf, "%d ", f);
    print_str_c(127, -20, str_buf);
    zero_beam();
    sprintf(str_buf, "%d ", l);
    print_str_c(127, 100, str_buf);
}

// =========================================================================
// Game state screens
// =========================================================================

void draw_title_screen(void) {
    zero_beam();
    set_scale(0x7F);
    intensity_a(0xFF);
    print_str_c(40, -60, "HERO");
    intensity_a(0x7F);
    zero_beam();
    print_str_c(-10, -80, "FOR VECTREX");
    zero_beam();
    print_str_c(-50, -60, "PRESS BTN");
}

void draw_game_over_screen(void) {
    zero_beam();
    set_scale(0x7F);
    intensity_a(0xFF);
    print_str_c(30, -80, "GAME OVER");
    intensity_a(0x7F);
    zero_beam();
    sprintf(str_buf, "SCORE %d ", score);
    print_str_c(-20, -80, str_buf);
    zero_beam();
    print_str_c(-60, -60, "PRESS BTN");
}

// =========================================================================
// Initialization
// =========================================================================

void vectrex_init(void) {
    set_beam_intensity(0x7F);
    set_scale(0x7F);
    stop_music();
    stop_sound();
    controller_enable_1_x();
    controller_enable_1_y();
}

// =========================================================================
// Main game loop
// =========================================================================

int main(void) {
    vectrex_init();
    game_state = STATE_TITLE;

    while (TRUE) {
        wait_recal();
        intensity_a(0x7F);
        controller_check_buttons();

        if (game_state == STATE_TITLE) {
            draw_title_screen();
            if (controller_button_1_1_pressed() ||
                controller_button_1_2_pressed() ||
                controller_button_1_3_pressed() ||
                controller_button_1_4_pressed()) {
                start_new_game();
            }
        }
        else if (game_state == STATE_PLAYING) {
            anim_tick++;

            handle_input();
            update_player_physics();
            update_laser();
            update_dynamite();
            update_enemies();
            check_miner_rescue();

            // Draw everything (minimal zero_beam calls)
            draw_cave();
            draw_enemies();
            draw_dynamite_and_explosion();
            draw_laser_beam();
            draw_player();
            draw_miner();
            draw_hud();

            if (level_msg_timer > 0) {
                zero_beam();
                sprintf(str_buf, "LEVEL %d", current_level + 1);
                print_str_c(0, -50, str_buf);
                level_msg_timer--;
            }
        }
        else if (game_state == STATE_DYING) {
            death_timer--;
            if (death_timer & 2) {
                draw_player();
            }
            draw_cave();

            if (death_timer == 0) {
                player_lives--;
                if (player_lives == 0) {
                    game_state = STATE_GAME_OVER;
                } else {
                    init_level();
                    game_state = STATE_PLAYING;
                }
            }
        }
        else if (game_state == STATE_LEVEL_COMPLETE) {
            draw_cave();
            zero_beam();
            set_scale(0x7F);
            print_str_c(0, -70, "RESCUED!");

            level_msg_timer--;
            if (level_msg_timer == 0) {
                current_level++;
                if (current_level > 1) {
                    current_level = 0;
                }
                init_level();
                game_state = STATE_PLAYING;
            }
        }
        else if (game_state == STATE_GAME_OVER) {
            draw_game_over_screen();
            if (controller_button_1_1_pressed() ||
                controller_button_1_2_pressed() ||
                controller_button_1_3_pressed() ||
                controller_button_1_4_pressed()) {
                game_state = STATE_TITLE;
            }
        }
    }

    return 0;
}
