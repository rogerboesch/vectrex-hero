/*
 * main.c — Sinclair QL R.E.S.C.U.E. entry point and game loop
 *
 * This is the central game loop that drives the state machine.
 * All global game state lives here (player, enemies, laser, dynamite,
 * room data). The loop runs at ~17 FPS via ql_wait_frame().
 *
 * Game flow:
 *   TITLE → (ENTER/SPACE) → LEVEL_INTRO → PLAYING
 *   PLAYING → (death) → DYING → LEVEL_FAILED or GAME_OVER
 *   PLAYING → (rescue miner) → LEVEL_COMPLETE → RESCUED → next level
 *
 * Coordinate system:
 *   Game coords: x = -128..127, y = -50..50 (origin at center)
 *   Screen coords: x = 0..255, y = 0..255 (origin top-left)
 *   Conversion via SCREEN_X() / SCREEN_Y() macros in game.h
 */

#include "game.h"
#include "sprites.h"

/* ===================================================================
 * Global variables — all game state
 *
 * These are accessed from multiple .c files via extern declarations
 * in game.h. Using globals is intentional: on the 68008 at 7.5MHz,
 * passing structs by pointer through function calls is expensive.
 * =================================================================== */

/* Player state */
int8_t player_x, player_y;       /* position in game coords */
int8_t player_vx, player_vy;     /* velocity (pixels/frame in game units) */
int8_t player_facing;            /* -1 = left, +1 = right */
uint8_t player_fuel;             /* jetpack fuel (0..255, drains on thrust) */
uint8_t player_dynamite;         /* dynamite sticks remaining */
uint8_t player_lives;            /* lives remaining (0 = game over) */
uint8_t player_on_ground;        /* 1 if standing on floor/wall/segment */
uint8_t player_thrusting;        /* 1 if UP held and fuel > 0 this frame */
uint8_t anim_tick;               /* frame counter for sprite animation */

int16_t score;                   /* player score (16-bit, max 32767) */

/* Game state machine */
uint8_t game_state;              /* current STATE_* value */
uint8_t current_level;           /* 0-based level index (0..NUM_LEVELS-1) */
uint8_t current_room;            /* 0-based room index within level */
uint8_t death_timer;             /* frames remaining in death animation */
uint8_t level_msg_timer;         /* frames remaining for level messages */

/* Enemies — up to MAX_ENEMIES (3) active per room */
Enemy enemies[MAX_ENEMIES];
uint8_t enemy_count;             /* how many enemies in current room */

/* Laser state — one active laser beam at a time */
uint8_t laser_active;            /* 1 if laser currently on screen */
int8_t laser_x, laser_y;        /* laser origin position */
int8_t laser_dir;                /* -1 or +1 (matches player_facing) */
uint8_t laser_timer;             /* frames remaining until laser expires */

/* Dynamite state — one active dynamite/explosion at a time */
uint8_t dyn_active;              /* 1 if dynamite placed or exploding */
int8_t dyn_x, dyn_y;            /* dynamite position */
uint8_t dyn_timer;               /* fuse countdown (frames until explosion) */
uint8_t dyn_exploding;           /* 1 during explosion animation */
uint8_t dyn_expl_timer;          /* explosion animation countdown */

/* Destructible walls — bitmask per room (bit N = wall N destroyed) */
uint8_t walls_destroyed;
uint8_t room_walls_destroyed[MAX_ROOMS]; /* saved per room when exiting */

/* Current room data — set by set_room_data(), used by physics/render */
const int8_t *cur_cave_lines;    /* polyline data for cave walls */
int8_t cur_cave_left, cur_cave_right, cur_cave_top, cur_cave_floor;
const int8_t *cur_walls;         /* destructible wall positions */
uint8_t cur_wall_count;
const int8_t *cur_enemies_data;  /* enemy spawn data for current room */
uint8_t cur_enemy_count;
int8_t *cur_cave_segs;           /* collision segments (extracted from polylines) */
uint8_t cur_seg_count;           /* number of collision segments */
int8_t cur_miner_x, cur_miner_y; /* trapped miner position */
uint8_t cur_has_miner;           /* 1 if this room has the miner */
uint8_t cur_has_lava;            /* 1 if this room has a lava floor */

/* Input state — set by ql_read_keys() in ql_hw.s each frame */
/* (declared as externs in game.h, defined in ql_hw.s data section) */

/* ===================================================================
 * Helpers
 * =================================================================== */

/*
 * box_overlap — Axis-aligned bounding box collision test.
 * All entities use center + half-width/half-height representation.
 * Returns 1 if boxes overlap, 0 otherwise.
 */
uint8_t box_overlap(int8_t ax, int8_t ay, int8_t ahw, int8_t ahh,
                    int8_t bx, int8_t by, int8_t bhw, int8_t bhh) {
    if (ax + ahw < bx - bhw) return 0;
    if (ax - ahw > bx + bhw) return 0;
    if (ay + ahh < by - bhh) return 0;
    if (ay - ahh > by + bhh) return 0;
    return 1;
}

/*
 * Wall accessors — walls are stored as packed int8_t arrays:
 *   [y, x, h, w] per wall (4 bytes each).
 * Note: x,y is the top-left corner, w,h are full dimensions.
 * For collision, walls are converted to center + half-extents.
 */
int8_t wall_y(uint8_t i) { return cur_walls[i * 4]; }
int8_t wall_x(uint8_t i) { return cur_walls[i * 4 + 1]; }
int8_t wall_h(uint8_t i) { return cur_walls[i * 4 + 2]; }
int8_t wall_w(uint8_t i) { return cur_walls[i * 4 + 3]; }

/*
 * player_hits_wall — Check if player's collision box overlaps wall i.
 * Converts wall from [x,y,w,h] to center+half representation first.
 */
uint8_t player_hits_wall(uint8_t i) {
    int8_t wcx = wall_x(i) + (wall_w(i) / 2);
    int8_t wcy = wall_y(i);
    int8_t whw = wall_w(i) / 2;
    int8_t whh = wall_h(i);
    return box_overlap(player_x, player_y, PLAYER_HW, PLAYER_HH,
                       wcx, wcy, whw, whh);
}

/* ===================================================================
 * Main game loop
 *
 * Runs forever until ESC (handled in ql_read_keys via MT.FRJOB).
 * Each iteration:
 *   1. ql_wait_frame() — suspend job for ~60ms (3 ticks at 50Hz)
 *   2. ql_read_keys()  — poll keyboard (KEYROW + IO.FBYTE)
 *   3. State-specific logic (input, physics, rendering)
 * =================================================================== */

int main(void) {
    ql_init();       /* set Mode 8, open console channel */
    render_init();   /* allocate bg_buffer from QDOS heap */

    game_state = STATE_TITLE;
    render_title_screen();

    for (;;) {
        ql_wait_frame();  /* ~17 FPS frame pacing */
        ql_read_keys();   /* poll KEYROW + drain IO.FBYTE buffer */

        /* --- Title screen: wait for ENTER/SPACE to start --- */
        if (game_state == STATE_TITLE) {
            anim_tick++;
            if (key_enter_pressed || key_space_pressed) {
                start_new_game();
                game_state = STATE_PLAYING;
                render_room();
                ql_flush_keys();  /* ignore stale KEYROW for 3 frames */
            }
        }
        /* --- Level intro: skip straight to gameplay (no delay on QL) --- */
        else if (game_state == STATE_LEVEL_INTRO) {
            /* Skip intro delay — go straight to playing */
            game_state = STATE_PLAYING;
            render_room();
        }
        /* --- Main gameplay: input → physics → enemies → render --- */
        else if (game_state == STATE_PLAYING) {
            anim_tick++;

            handle_input();           /* read keys, apply thrust/movement */
            update_player_physics();  /* gravity, collisions, floor/wall */
            update_laser();           /* move laser, check enemy hits */
            update_dynamite();        /* fuse countdown, explosion damage */
            update_enemies();         /* AI movement, player collision */
            check_miner_rescue();     /* check if player reached miner */

            /* Lava death — instant kill if player touches lava strip */
            if (cur_has_lava && player_y - PLAYER_HH <= cur_cave_floor + LAVA_HEIGHT) {
                game_state = STATE_DYING;
                death_timer = DEATH_ANIM_TIME;
            }

            /* Room exits — teleport player when crossing room boundaries.
             * Exit directions: 0=left, 1=right, 2=up, 3=down.
             * NONE (255) means no exit in that direction. */
            {
                uint8_t exit_room = NONE;
                if (player_x <= ROOM_BOUND_LEFT &&
                    room_exits[current_room * 4 + 0] != NONE) {
                    exit_room = room_exits[current_room * 4 + 0];
                    player_x = ROOM_BOUND_RIGHT;
                }
                else if (player_x >= ROOM_BOUND_RIGHT &&
                           room_exits[current_room * 4 + 1] != NONE) {
                    exit_room = room_exits[current_room * 4 + 1];
                    player_x = ROOM_BOUND_LEFT;
                }
                else if (player_y >= ROOM_BOUND_TOP &&
                           room_exits[current_room * 4 + 2] != NONE) {
                    exit_room = room_exits[current_room * 4 + 2];
                    player_y = ROOM_BOUND_FLOOR;
                }
                else if (player_y <= ROOM_BOUND_FLOOR &&
                           room_exits[current_room * 4 + 3] != NONE) {
                    exit_room = room_exits[current_room * 4 + 3];
                    player_y = ROOM_BOUND_TOP;
                }
                if (exit_room != NONE) {
                    /* Save wall destruction state for this room */
                    room_walls_destroyed[current_room] = walls_destroyed;
                    current_room = exit_room;
                    set_room_data();
                    load_enemies();
                    /* Restore previously saved wall state for new room */
                    walls_destroyed = room_walls_destroyed[current_room];
                    render_room();
                }
            }

            render_frame();  /* erase/draw sprites via save-behind system */
            render_hud();    /* incremental HUD update (dirty tracking) */
        }
        /* --- Death animation: flash player sprite --- */
        else if (game_state == STATE_DYING) {
            death_timer--;
            render_frame();  /* keeps rendering (player flashes) */

            if (death_timer == 0) {
                player_lives--;
                if (player_lives == 0) {
                    game_state = STATE_GAME_OVER;
                    render_game_over();
                }
                else {
                    /* Respawn: restore fuel/dynamite, replay same room */
                    player_fuel = START_FUEL;
                    player_dynamite = START_DYNAMITE;
                    game_state = STATE_LEVEL_FAILED;
                    render_failed();
                }
            }
        }
        /* --- Level complete: brief delay then show rescued screen --- */
        else if (game_state == STATE_LEVEL_COMPLETE) {
            level_msg_timer--;
            if (level_msg_timer == 0) {
                game_state = STATE_RESCUED;
                render_rescued();
            }
        }
        /* --- Rescued screen: advance to next level on keypress --- */
        else if (game_state == STATE_RESCUED) {
            if (key_enter_pressed || key_space_pressed) {
                current_level++;
                if (current_level >= NUM_LEVELS) current_level = 0;
                init_level();
                game_state = STATE_PLAYING;
                render_room();
            }
        }
        /* --- Level failed: restart same level on keypress --- */
        else if (game_state == STATE_LEVEL_FAILED) {
            if (key_enter_pressed || key_space_pressed) {
                init_level();
                game_state = STATE_PLAYING;
                render_room();
            }
        }
        /* --- Game over: return to title on keypress --- */
        else if (game_state == STATE_GAME_OVER) {
            if (key_enter_pressed || key_space_pressed) {
                game_state = STATE_TITLE;
                render_title_screen();
                ql_flush_keys();  /* ignore stale KEYROW */
            }
        }
    }

    ql_cleanup();  /* close console channel (never reached) */
    return 0;
}
