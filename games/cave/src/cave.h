//
// cave.h — Cave Diver for Vectrex
// Game-specific defines, externs, and prototypes
//

#ifndef CAVE_H
#define CAVE_H

#include "engine.h"
#include "sprites.h"
#include "font.h"

// =========================================================================
// Game-specific constants
// =========================================================================

#define GRAVITY         1
#define SWIM_THRUST     2
#define MAX_VEL_Y       3
#define MAX_VEL_X       3
#define WALK_SPEED      2
#define OXYGEN_DRAIN    1
#define START_OXYGEN    255
#define START_LIVES     3
#define SONAR_RANGE     60
#define SONAR_COOLDOWN  30
#define CURRENT_FORCE   2
#define LAVA_HEIGHT     3
#define MAX_ENEMIES     3
#define MAX_ROOMS       16
#define NUM_LEVELS      5

// Enemy types
#define ENEMY_FISH      0
#define ENEMY_JELLYFISH 1
#define ENEMY_EEL       2
#define JELLYFISH_BOB   15

// Timing constants
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
// Cave Diver-specific extern globals
// =========================================================================

extern uint8_t player_oxygen;
extern uint8_t player_swimming;

extern Enemy enemies[MAX_ENEMIES];
extern uint8_t enemy_count;

extern uint8_t sonar_active;
extern uint8_t sonar_timer;

extern uint8_t room_walls_destroyed[MAX_ROOMS];

extern int8_t cur_diver_x;
extern int8_t cur_diver_y;
extern uint8_t cur_has_diver;

// =========================================================================
// Cave Diver function prototypes
// =========================================================================

// player.c
void update_player_physics(void);
void handle_input(void);
void draw_player(void);

// enemies.c
void activate_sonar(void);
void update_sonar(void);
void update_enemies(void);
void check_diver_rescue(void);

// drawing.c
void draw_enemies(void);
void draw_sonar_pulse(void);
void draw_diver(void);
void draw_hud(void);
void draw_oxygen_bar(void);
void draw_title_screen(void);
void draw_game_over_screen(void);
void draw_level_intro_screen(void);
void draw_rescued_screen(void);
void draw_failed_screen(void);

#endif
