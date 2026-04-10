/*
 * game.h -- Apple IIe R.E.S.C.U.E. shared defines, types, externs
 *
 * Target: MOS 6502 @ 1.023MHz, Hi-Res (280x192, 6 colors)
 *
 * This header is included by all .c files in the Apple II port. It defines:
 *   - Game constants (physics, timing, collision boxes)
 *   - Apple II hardware constants (Hi-Res addresses, screen layout)
 *   - Coordinate conversion macros (game coords -> screen pixels)
 *   - Enemy struct type
 *   - All global variable extern declarations
 *   - All function prototypes
 *
 * Constants are shared across all ports where possible. Apple II-specific
 * values are tuned for the 6502's ~10 FPS refresh rate.
 */

#ifndef GAME_H
#define GAME_H

#include <stdint.h>

/* Guard to prevent levels.h (shared with Vectrex) from pulling in
 * the Vectrex-specific rescue.h header. levels.h checks for RESCUE_H. */
#ifndef RESCUE_H
#define RESCUE_H
#endif

#define TRUE  1
#define FALSE 0
#define NONE  255   /* sentinel: "no exit" or "no room" */

/* ===================================================================
 * Game constants -- scaled for ~10 FPS (original game is 60 FPS)
 * =================================================================== */

/* Physics constants -- scaled for ~10 FPS.
 * The Apple II's 6502 at 1.023MHz achieves ~10 FPS with full
 * rendering, so all speeds/timings are ~6x the original values. */
#define GRAVITY         3       /* downward acceleration per frame */
#define THRUST          12      /* upward acceleration when thrusting */
#define MAX_VEL_Y       14      /* terminal velocity (up and down) */
#define MAX_VEL_X       12      /* max horizontal speed in air */
#define WALK_SPEED      8       /* horizontal speed on ground */
#define FUEL_DRAIN      1       /* fuel consumed per thrust frame */
#define START_FUEL      255     /* full fuel tank (uint8_t max) */
#define START_DYNAMITE  5       /* dynamite sticks at game start */
#define START_LIVES     3       /* lives at game start */

/* Timing -- frame counts at ~10 FPS (original 60 FPS / 6) */
#define LASER_LENGTH    40      /* laser beam length in game units */
#define LASER_LIFETIME  2       /* frames laser stays active */
#define DYNAMITE_FUSE   10      /* frames before dynamite explodes */
#define EXPLOSION_TIME  5       /* frames explosion animation lasts */
#define EXPLOSION_RADIUS 25     /* blast radius for wall/enemy damage */
#define EXPLOSION_KILL   10     /* blast radius for player damage */
#define LAVA_HEIGHT      3      /* lava zone height above cave floor */
#define MAX_ENEMIES     3       /* max enemies per room */
#define MAX_ROOMS       16      /* max rooms per level */
#define NUM_LEVELS      20      /* total levels in the game */

/* Enemy types -- stored in Enemy.type field */
#define ENEMY_BAT       0       /* flies horizontally */
#define ENEMY_SPIDER    1       /* descends on thread, patrols vertically */
#define ENEMY_SNAKE     2       /* walks on floors horizontally */
#define SPIDER_PATROL   10      /* vertical patrol distance from home_y */

/* Collision half-widths and half-heights for each entity.
 * All collision uses center + half-extents (AABB) representation.
 * HW = half-width, HH = half-height, in game coordinate units. */
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

/* Room boundaries -- game coordinate limits.
 * These match the int8_t range (-128..127) for X and a symmetric
 * +/-50 range for Y. Room exits are checked against these bounds. */
#define ROOM_BOUND_LEFT   (-128)
#define ROOM_BOUND_RIGHT  (127)
#define ROOM_BOUND_TOP    (50)
#define ROOM_BOUND_FLOOR  (-50)

/* State transition timers (frame counts at ~10 FPS) */
#define LEVEL_INTRO_TIME    20  /* ~2 seconds intro display */
#define RESCUE_ANIM_TIME     10 /* ~1 second rescue celebration */
#define DEATH_ANIM_TIME      5  /* ~0.5 second death flash */

/* Game state machine values */
#define STATE_TITLE         0   /* title screen, waiting for key */
#define STATE_PLAYING       1   /* active gameplay */
#define STATE_DYING         2   /* death animation (flash sprite) */
#define STATE_LEVEL_COMPLETE 3  /* miner rescued, brief delay */
#define STATE_GAME_OVER     4   /* no lives left */
#define STATE_LEVEL_INTRO   5   /* "LEVEL XX" display */
#define STATE_RESCUED       6   /* "RESCUED!" screen */
#define STATE_LEVEL_FAILED  7   /* "FAILED" screen */

/* ===================================================================
 * Apple II-specific: display and hardware
 * =================================================================== */

/* Hi-Res page addresses */
#define HGR_PAGE1     ((uint8_t *)0x2000)
#define HGR_PAGE2     ((uint8_t *)0x4000)
#define HGR_SIZE      8192

/* Hi-Res screen dimensions */
#define SCREEN_W      280
#define SCREEN_H      192
#define SCREEN_STRIDE 40    /* bytes per row (280/7 = 40 bytes) */

/* Screen layout */
#define HUD_HEIGHT    16    /* pixel rows reserved for HUD at top */
#define PLAY_HEIGHT   160   /* pixel rows for playfield */
#define PLAY_Y_OFF    16    /* playfield starts at pixel row 16 */
#define FUEL_BAR_Y    180   /* fuel bar Y position */

/* Apple II soft switches */
#define SW_TXTCLR     0xC050  /* graphics mode */
#define SW_MIXCLR     0xC052  /* full screen (no text window) */
#define SW_HIRES      0xC057  /* hi-res mode */
#define SW_PAGE1      0xC054  /* display page 1 ($2000) */
#define SW_PAGE2      0xC055  /* display page 2 ($4000) */
#define SW_SPEAKER    0xC030  /* speaker toggle */
#define SW_KEYBOARD   0xC000  /* keyboard data (bit 7 = strobe) */
#define SW_KEYSTROBE  0xC010  /* clear keyboard strobe */

/* Coordinate mapping: game coordinates -> screen pixel coordinates.
 *
 * X axis: game range -128..127 maps to pixel 0..279 (280px).
 *         Scale: 280 / 256 ~= 1.09. Approx: (gx + 128) * 280 / 256
 *         Simplified: ((gx + 128) * 35) >> 5  (35/32 = 1.09375)
 *
 * Y axis: game range 50..-50 maps to ~160 pixels of playfield,
 *         offset by HUD_HEIGHT (16px) at the top.
 *         Scale: 160px / 100 units = 1.6 px/unit = 51/32.
 *         Game Y is inverted: +50 = top, -50 = bottom.
 */
#define SCREEN_X(gx)  ((uint16_t)((((int16_t)(gx) + 128) * 35) >> 5))
#define SCREEN_Y(gy)  ((uint16_t)(PLAY_Y_OFF + (((int16_t)(50 - (gy)) * 51) >> 5)))

/* ===================================================================
 * Types
 * =================================================================== */

/*
 * Enemy struct -- 7 bytes per enemy, max 3 per room.
 * Spider uses vx as vertical velocity (moves along Y axis).
 * home_y is the spider's thread anchor point (top of patrol range).
 */
typedef struct {
    int8_t x;           /* current X position (game coords) */
    int8_t y;           /* current Y position (game coords) */
    int8_t vx;          /* velocity (X for bat/snake, Y for spider) */
    uint8_t alive;      /* 1=alive, 0=dead (killed by laser/dynamite) */
    uint8_t anim;       /* animation frame counter */
    uint8_t type;       /* ENEMY_BAT, ENEMY_SPIDER, or ENEMY_SNAKE */
    int8_t home_y;      /* spider: thread anchor Y. others: initial Y */
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

/* Input state (set by a2_read_keys) */
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

/* a2_hw (asm, C-callable) */
void a2_init(void);
void a2_read_keys(void);
void a2_click(void);

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

/* blit.s (asm, C-callable) */
void hgr_clear_page(uint8_t *page);
void hgr_copy_page(const uint8_t *src, uint8_t *dst);
void hgr_hline(uint8_t *page, uint16_t x1, uint16_t x2, uint8_t y);
void hgr_vline(uint8_t *page, uint16_t x, uint8_t y1, uint8_t y2);
void hgr_restore_rect(const uint8_t *src, uint8_t *dst,
                      uint16_t x, uint8_t y, uint8_t w, uint8_t h);
void hgr_blit_sprite(uint8_t *page, const uint8_t *data,
                     const uint8_t *mask, uint16_t x, uint8_t y,
                     uint8_t w, uint8_t h);

/* Hi-Res row address lookup (defined in blit.s) */
extern const uint16_t hgr_row_addr[192];

#endif
