//
// engine.h — Vectrex room-based game engine
// Shared defines, types, and prototypes used by all games
//

#ifndef ENGINE_H
#define ENGINE_H

#include <vectrex.h>
#include <vectrex/stdlib.h>

// =========================================================================
// Engine constants
// =========================================================================

#define NONE            255

// Sprite data access: arrays are [hw, hh, oy, ox, count-1, dy1, dx1, ...]
#define SPRITE_HW(s)   ((s)[0])
#define SPRITE_HH(s)   ((s)[1])
#define SPRITE_OY(s)   ((s)[2])
#define SPRITE_OX(s)   ((s)[3])
#define SPRITE_VLC(s)  (&(s)[4])

// Beam intensity
#define INTENSITY_DIM    0x3F
#define INTENSITY_NORMAL 0x5F
#define INTENSITY_HI     0x6F
#define INTENSITY_BRIGHT 0x7F

// Cave geometry
#define CAVE_LEFT   -90
#define CAVE_RIGHT   90
#define CAVE_TOP     105
#define CAVE_FLOOR  -95
#define LEDGE_Y      65
#define SHAFT_LEFT  -25
#define SHAFT_RIGHT  25

// Room bounds
#define ROOM_BOUND_LEFT   (-128)
#define ROOM_BOUND_RIGHT  (127)
#define ROOM_BOUND_TOP    (50)
#define ROOM_BOUND_FLOOR  (-50)

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
// Extern globals — engine-level state
// =========================================================================

// Player core
extern int8_t player_x;
extern int8_t player_y;
extern int8_t player_vx;
extern int8_t player_vy;
extern int8_t player_facing;
extern uint8_t player_on_ground;
extern uint8_t player_lives;
extern uint8_t anim_tick;

extern int score;

// Game flow
extern uint8_t game_state;
extern uint8_t current_level;
extern uint8_t current_room;
extern uint8_t death_timer;
extern uint8_t level_msg_timer;

// Room data (populated by levels.c)
extern const int8_t *cur_cave_lines;
extern int8_t cur_cave_left, cur_cave_right, cur_cave_top, cur_cave_floor;
extern const int8_t *cur_walls;
extern uint8_t cur_wall_count;
extern const int8_t *cur_enemies_data;
extern uint8_t cur_enemy_count;
extern int8_t *cur_cave_segs;
extern uint8_t cur_seg_count;
extern uint8_t cur_has_lava;

// Room tables
extern const int8_t *room_cave_lines[];
extern const int8_t *room_walls[];
extern uint8_t room_wall_counts[];
extern const int8_t *room_enemies_data[];
extern uint8_t room_enemy_counts[];
extern int8_t room_starts[];
extern int8_t room_miners[];
extern uint8_t room_exits[];
extern uint8_t room_has_miner[];
extern uint8_t room_has_lava[];

extern uint8_t walls_destroyed;

extern char str_buf[16];

// =========================================================================
// Engine function prototypes
// =========================================================================

// Collision
uint8_t box_overlap(int8_t ax, int8_t ay, int8_t ahw, int8_t ahh,
                    int8_t bx, int8_t by, int8_t bhw, int8_t bhh);

// Wall access
int8_t wall_y(uint8_t i);
int8_t wall_x(uint8_t i);
int8_t wall_h(uint8_t i);
int8_t wall_w(uint8_t i);
uint8_t player_hits_wall(uint8_t i);

// Init
void vectrex_init(void);

// Drawing (engine-level)
uint8_t int_to_str(int val, uint8_t pos);
void draw_sprite(int8_t y, int8_t x, int8_t *shape);
void draw_cave(void);
void draw_lava(void);

// Levels (engine-level)
void set_level_data(void);
void set_room_data(void);
void load_enemies(void);
void init_level(void);
void start_new_game(void);

#endif
