# R.E.S.C.U.E
## Remote Exploration & Sub-surface Cavern Utility Expert

A conversion of Activision's H.E.R.O. (1984), built with the VectreC toolchain and CMOC.

## Build

```bash
make        # Compile to bin/main.bin
make run    # Launch level editor with emulator
make clean  # Remove build artifacts
```

Requires the VectreC toolchain at `$HOME/retro-tools/vectrec/`.

## Level & Sprite Editor

A Python/Tkinter editor for designing levels and sprites.

```bash
python3 tools/level_editor.py                          # Open editor
python3 tools/level_editor.py --project assets/hero.json  # Load project at startup
python3 tools/level_editor.py --rom <rom> --cart <cart> # Open with emulator tab active
```

### Editor Features

- **Level editor** — Place player start, enemies (bats), walls, miner, and draw cave lines as polylines
- **Sprite editor** — Draw VLC sprites with vector lines, preview and export
- **Emulator tab** — Built-in vec2x emulator with CPU register display and pause/resume
- **Test level** — Run a single level directly from the editor; builds to `test_level/` using cave lines for both visuals and physics
- **Test sprite** — Preview a sprite in the emulator; builds to `test_sprite/`
- **Export** — Export `hero.h`, `levels.h`, and `sprites.h` to `src/`

### Test Level Build

The level test compiles `src/` game code via wrapper `.c` files that override key functions using a `#define` rename trick:

- **Cave line physics** — Horizontal segments act as floor/ceiling, vertical segments as side walls
- **Bat bouncing** — Bats bounce off vertical cave segments
- **All walls destroyable** — Editor-placed walls can all be destroyed by dynamite
- **Custom cave drawing** — Cave lines from the editor replace the hardcoded cave shape

## Controls

| Input | Action |
|---|---|
| Joystick left/right | Walk (on ground) or fly horizontally |
| Joystick up | Activate prop-pack (thrust upward, drains fuel) |
| Joystick down (on ground) | Place dynamite |
| Button 1 | Fire laser in facing direction |

## Features

| Feature | Description |
|---|---|
| Player movement | Gravity, thrust, horizontal walk/fly |
| Propeller animation | 2-frame alternating prop blade rotation |
| Fuel system | 255 starting fuel, drains while thrusting |
| Laser | Horizontal beam in facing direction, kills bats |
| Dynamite | Timed fuse, explosion with flash effect, destroys walls and enemies |
| Bats | Horizontal patrol, bounce off walls, 2-frame wing flap, kill player on contact |
| Wall collision | AABB collision with landing, side push-out, ceiling push-out |
| Miner rescue | Touch miner for 1000 pts + fuel/dynamite bonus, advance level |
| HUD | Score, lives, fuel display |
| Death/respawn | Bat contact or explosion kills player, respawn at level start |

### Intensity Levels

| Level | Value | Usage |
|---|---|---|
| `INTENSITY_DIM` | 0x3F | Cave lines and walls |
| `INTENSITY_NORMAL` | 0x5F | Player, enemies, HUD, general elements |
| `INTENSITY_HI` | 0x6F | Laser, dynamite fuse, miner blink, title text |
| `INTENSITY_BRIGHT` | 0x7F | Explosions only (+ cave flash during explosion) |

## Scoring

- 50 pts per bat killed
- 75 pts per wall destroyed
- 1000 pts per miner rescued + remaining fuel + 50 per remaining dynamite

## Game Flow

```
TITLE -> PLAYING -> GAME_OVER -> TITLE
              |-> LEVEL_COMPLETE -> PLAYING (next level)
              |-> DYING -> PLAYING (respawn) or GAME_OVER
```

## Project Structure

```
src/
  main.c        Game loop, initialization, collision helpers
  player.c      Player physics, input handling, drawing
  enemies.c     Enemy AI, laser, dynamite, miner rescue
  drawing.c     Cave, sprites, HUD, title/game-over screens
  levels.c      Level data management, enemy loading
  hero.h        Constants, types, externs, prototypes
  sprites.h     VLC sprite data arrays
  levels.h      Level wall and enemy data arrays
tools/
  level_editor.py   Level & sprite editor with built-in emulator
  vec2x_py/         Python bindings for vec2x emulator
assets/
  hero.json     Main project file
  hero.png      Reference image
```

## Architecture

- **Toolchain**: CMOC cross-compiler targeting Motorola 6809, lwasm assembler, lwlink linker
- **Coordinate system**: -128 to 127 in both axes, `moveto_d(y, x)` (Y first)
- **Walls**: Rectangular blocks defined as (center_y, left_x, half_height, width) with AABB collision
- **Sprites**: VLC (Vector List Count) format — move count followed by (dy, dx) pairs
- **Intensity**: 4-level beam intensity system for visual depth
- **ROM size**: ~8.3KB
- **RAM usage**: ~182 bytes of 896 available
