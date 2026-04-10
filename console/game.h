//
// game.h — Console R.E.S.C.U.E. shared defines, types, externs
//

#ifndef GAME_H
#define GAME_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// Prevent levels.h from pulling in the Vectrex hero.h
#ifndef RESCUE_H
#define RESCUE_H
#endif

#define TRUE 1
#define FALSE 0
#define NONE 255

// =========================================================================
// Game constants (matching Vectrex original)
// =========================================================================

#define GRAVITY         1
#define THRUST          3
#define MAX_VEL_Y       4
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
#define LAVA_HEIGHT      3
#define MAX_ENEMIES     3
#define MAX_ROOMS       16
#define NUM_LEVELS      20

// Enemy types
#define ENEMY_BAT       0
#define ENEMY_SPIDER    1
#define ENEMY_SNAKE     2
#define SPIDER_PATROL   20

// Sprite collision boxes (from sprite data: hw, hh)
#define PLAYER_HW  5
#define PLAYER_HH  11
#define BAT_HW     6
#define BAT_HH     5
#define SPIDER_HW  5
#define SPIDER_HH  4
#define SNAKE_HW   7
#define SNAKE_HH   2
#define MINER_HW   6
#define MINER_HH   6

// Room boundaries (original coordinate space)
#define ROOM_BOUND_LEFT   (-128)
#define ROOM_BOUND_RIGHT  (127)
#define ROOM_BOUND_TOP    (50)
#define ROOM_BOUND_FLOOR  (-50)

// Cave geometry
#define CAVE_LEFT   -90
#define CAVE_RIGHT   90
#define CAVE_TOP     105
#define CAVE_FLOOR  -95

// Timing
#define LEVEL_INTRO_TIME    120
#define RESCUE_ANIM_TIME     60
#define DEATH_ANIM_TIME      30
#define FAILED_DELAY_TIME   120

// Game states
#define STATE_TITLE         0
#define STATE_PLAYING       1
#define STATE_DYING         2
#define STATE_LEVEL_COMPLETE 3
#define STATE_GAME_OVER     4
#define STATE_LEVEL_INTRO   5
#define STATE_RESCUED       6
#define STATE_LEVEL_FAILED  7

// =========================================================================
// Console-specific constants
// =========================================================================

#define SCREEN_W    80
#define SCREEN_H    40
#define HUD_ROWS    2
#define TOTAL_ROWS  (SCREEN_H + HUD_ROWS)

// Key codes
#define KEY_NONE    0
#define KEY_LEFT    1
#define KEY_RIGHT   2
#define KEY_UP      3
#define KEY_DOWN    4
#define KEY_SPACE   5
#define KEY_ENTER   6
#define KEY_D       7
#define KEY_Q       8
#define KEY_R       9

// ANSI colors (foreground)
#define COL_BLACK    0
#define COL_RED      1
#define COL_GREEN    2
#define COL_YELLOW   3
#define COL_BLUE     4
#define COL_MAGENTA  5
#define COL_CYAN     6
#define COL_WHITE    7
#define COL_BRIGHT   8  // add to make bright (e.g. COL_RED | COL_BRIGHT)

// =========================================================================
// Types
// =========================================================================

typedef struct {
    int8_t x;
    int8_t y;
    int8_t vx;
    uint8_t alive;
    uint8_t anim;
    uint8_t type;
    int8_t home_y;
} Enemy;

// =========================================================================
// Extern globals (defined in main.c)
// =========================================================================

extern int8_t player_x, player_y;
extern int8_t player_vx, player_vy;
extern int8_t player_facing;
extern uint8_t player_fuel;
extern uint8_t player_dynamite;
extern uint8_t player_lives;
extern uint8_t player_on_ground;
extern uint8_t player_thrusting;
extern uint8_t anim_tick;

extern int score;

extern uint8_t game_state;
extern uint8_t current_level;
extern uint8_t current_room;
extern uint8_t death_timer;
extern uint8_t level_msg_timer;

extern Enemy enemies[MAX_ENEMIES];
extern uint8_t enemy_count;

extern uint8_t laser_active;
extern int8_t laser_x, laser_y, laser_dir;
extern uint8_t laser_timer;

extern uint8_t dyn_active;
extern int8_t dyn_x, dyn_y;
extern uint8_t dyn_timer;
extern uint8_t dyn_exploding;
extern uint8_t dyn_expl_timer;

extern uint8_t walls_destroyed;
extern uint8_t room_walls_destroyed[MAX_ROOMS];

extern const int8_t *cur_cave_lines;
extern int8_t cur_cave_left, cur_cave_right, cur_cave_top, cur_cave_floor;
extern const int8_t *cur_walls;
extern uint8_t cur_wall_count;
extern const int8_t *cur_enemies_data;
extern uint8_t cur_enemy_count;
extern int8_t *cur_cave_segs;
extern uint8_t cur_seg_count;
extern int8_t cur_miner_x, cur_miner_y;
extern uint8_t cur_has_miner;
extern uint8_t cur_has_lava;

extern const int8_t *room_cave_lines[MAX_ROOMS];
extern const int8_t *room_walls[MAX_ROOMS];
extern uint8_t room_wall_counts[MAX_ROOMS];
extern const int8_t *room_enemies_data[MAX_ROOMS];
extern uint8_t room_enemy_counts[MAX_ROOMS];
extern int8_t room_starts[MAX_ROOMS * 2];
extern int8_t room_miners[MAX_ROOMS * 2];
extern uint8_t room_exits[MAX_ROOMS * 4];
extern uint8_t room_has_miner[MAX_ROOMS];
extern uint8_t room_has_lava[MAX_ROOMS];

// Input state
extern uint8_t key_left, key_right, key_up, key_down;
extern uint8_t key_space_pressed;  // single-shot
extern uint8_t key_d_pressed;      // single-shot
extern uint8_t key_enter_pressed;  // single-shot

// =========================================================================
// Function prototypes
// =========================================================================

// main.c
uint8_t box_overlap(int8_t ax, int8_t ay, int8_t ahw, int8_t ahh,
                    int8_t bx, int8_t by, int8_t bhw, int8_t bhh);
int8_t wall_y(uint8_t i);
int8_t wall_x(uint8_t i);
int8_t wall_h(uint8_t i);
int8_t wall_w(uint8_t i);
uint8_t player_hits_wall(uint8_t i);

// player.c
void update_player_physics(void);
void handle_input(void);

// enemies.c
void fire_laser(void);
void update_laser(void);
void place_dynamite(void);
void update_dynamite(void);
void update_enemies(void);
void check_miner_rescue(void);

// levels.c
void set_level_data(void);
void set_room_data(void);
void load_enemies(void);
void init_level(void);
void start_new_game(void);

// terminal.c
void term_init(void);
void term_cleanup(void);
int  term_read_key(void);
void term_get_size(int *cols, int *rows);

// render.c
void render_clear(void);
void render_char(int row, int col, char ch, uint8_t color);
void render_game(void);
void render_title_screen(void);
void render_level_intro_screen(void);
void render_game_over_screen(void);
void render_rescued_screen(void);
void render_failed_screen(void);
void render_flush(void);

#endif
