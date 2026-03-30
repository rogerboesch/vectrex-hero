/*
 * game.h — Sinclair QL R.E.S.C.U.E. shared defines, types, externs
 *
 * Target: Motorola 68008 @ 7.5MHz, QDOS, Mode 8 (256x256, 8 colors)
 */

#ifndef GAME_H
#define GAME_H

/* vbcc provides these via <stdint.h>, but define fallbacks */
typedef signed char    int8_t;
typedef unsigned char  uint8_t;
typedef signed short   int16_t;
typedef unsigned short uint16_t;
typedef signed long    int32_t;
typedef unsigned long  uint32_t;

/* Prevent levels.h from pulling in Vectrex hero.h */
#ifndef HERO_H
#define HERO_H
#endif

#define TRUE  1
#define FALSE 0
#define NONE  255

/* ===================================================================
 * Game constants (identical to all ports)
 * =================================================================== */

/* Speed constants scaled for ~17 FPS (original is 60 FPS, ratio ~3.5x) */
#define GRAVITY         2
#define THRUST          7
#define MAX_VEL_Y       8
#define MAX_VEL_X       7
#define WALK_SPEED      5
#define FUEL_DRAIN      1
#define START_FUEL      255
#define START_DYNAMITE  3
#define START_LIVES     3
/* Timing scaled for ~17 FPS (original 60 FPS, divide by ~3.5) */
#define LASER_LENGTH    40
#define LASER_LIFETIME  3
#define DYNAMITE_FUSE   17
#define EXPLOSION_TIME  8
#define EXPLOSION_RADIUS 25
#define EXPLOSION_KILL   10
#define LAVA_HEIGHT      3
#define MAX_ENEMIES     3
#define MAX_ROOMS       16
#define NUM_LEVELS      20

#define ENEMY_BAT       0
#define ENEMY_SPIDER    1
#define ENEMY_SNAKE     2
#define SPIDER_PATROL   10

/* Sprite collision boxes */
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

/* Room boundaries */
#define ROOM_BOUND_LEFT   (-128)
#define ROOM_BOUND_RIGHT  (127)
#define ROOM_BOUND_TOP    (50)
#define ROOM_BOUND_FLOOR  (-50)

/* Timing */
/* Timing scaled for ~17 FPS */
#define LEVEL_INTRO_TIME    34
#define RESCUE_ANIM_TIME     17
#define DEATH_ANIM_TIME      9

/* Game states */
#define STATE_TITLE         0
#define STATE_PLAYING       1
#define STATE_DYING         2
#define STATE_LEVEL_COMPLETE 3
#define STATE_GAME_OVER     4
#define STATE_LEVEL_INTRO   5
#define STATE_RESCUED       6
#define STATE_LEVEL_FAILED  7

/* ===================================================================
 * QL-specific: display and hardware
 * =================================================================== */

#define SCREEN_BASE   ((uint8_t *)0x20000L)
#define SCREEN_SIZE   32768
#define SCREEN_W      256
#define SCREEN_H      256
#define SCREEN_STRIDE 128   /* bytes per row */
#define HUD_HEIGHT    36    /* pixel rows reserved for HUD at top */
#define PLAY_HEIGHT   240   /* pixel rows for playfield */

/*
 * Mode 8 colors (3 bits: flash, green, red)
 * QL Mode 8 pixel encoding: each byte = 2 pixels
 *   high nibble = left pixel, low nibble = right pixel
 *   bits: x F G R  (x=unused, F=flash, G=green, R=red)
 */
#define COL_BLACK   0
#define COL_RED     1
#define COL_GREEN   4
#define COL_YELLOW  6
#define COL_BLUE    2
#define COL_MAGENTA 3
#define COL_CYAN    5
#define COL_WHITE   7

/* Coordinate mapping: game -> screen pixels */
/* X: game -128..127 -> pixel 0..255 (1:1!) */
#define SCREEN_X(gx)  ((uint16_t)((int16_t)(gx) + 128))

/* Y: game 50..-50 -> ~180 pixels playfield (with HUD offset) */
/* 180 pixels / 100 units = 1.8px/unit ≈ 58/32 */
#define SCREEN_Y(gy)  ((uint16_t)(HUD_HEIGHT + (((int16_t)(50 - (gy)) * 58) >> 5)))

/* ===================================================================
 * Types
 * =================================================================== */

typedef struct {
    int8_t x;
    int8_t y;
    int8_t vx;
    uint8_t alive;
    uint8_t anim;
    uint8_t type;
    int8_t home_y;
} Enemy;

/* ===================================================================
 * Externs (defined in main.c)
 * =================================================================== */

extern int8_t player_x, player_y;
extern int8_t player_vx, player_vy;
extern int8_t player_facing;
extern uint8_t player_fuel;
extern uint8_t player_dynamite;
extern uint8_t player_lives;
extern uint8_t player_on_ground;
extern uint8_t player_thrusting;
extern uint8_t anim_tick;

extern int16_t score;

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

/* Input state (set by ql_read_keys) */
extern uint8_t key_left, key_right, key_up, key_down;
extern uint8_t key_space_pressed;
extern uint8_t key_d_pressed;
extern uint8_t key_enter_pressed;

/* ===================================================================
 * Prototypes
 * =================================================================== */

/* main.c */
uint8_t box_overlap(int8_t ax, int8_t ay, int8_t ahw, int8_t ahh,
                    int8_t bx, int8_t by, int8_t bhw, int8_t bhh);
int8_t wall_y(uint8_t i);
int8_t wall_x(uint8_t i);
int8_t wall_h(uint8_t i);
int8_t wall_w(uint8_t i);
uint8_t player_hits_wall(uint8_t i);

/* player.c */
void update_player_physics(void);
void handle_input(void);

/* enemies.c */
void fire_laser(void);
void update_laser(void);
void place_dynamite(void);
void update_dynamite(void);
void update_enemies(void);
void check_miner_rescue(void);

/* levels.c */
void set_level_data(void);
void set_room_data(void);
void load_enemies(void);
void init_level(void);
void start_new_game(void);

/* ql_hw (asm, C-callable) */
void ql_init(void);
void ql_cleanup(void);
void ql_read_keys(void);
uint16_t ql_get_ticks(void);
void ql_beep(uint8_t pitch, uint8_t duration);
void ql_wait_frame(void);
void *ql_alloc(uint32_t size);
/* asm_blit_sprite declared where Sprite type is known (render.c) */

/* render.c */
void render_init(void);
void render_room(void);
void render_frame(void);
void render_hud(void);
void render_destroy_wall(uint8_t idx);
void render_title_screen(void);
void render_level_intro(void);
void render_game_over(void);
void render_rescued(void);
void render_failed(void);

#endif
