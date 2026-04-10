# R.E.S.C.U.E. -- Vectrex

Vector graphics port targeting the Vectrex arcade console. Uses the native vector CRT display for sharp, scalable line graphics at any distance.

## Build

```bash
make        # Build bin/main.bin
make run    # Launch level editor with emulator
make clean  # Remove build artifacts
```

Requires the VectreC toolchain at `$HOME/retro-tools/vectrec/`.

## Architecture

### Hardware

- **CPU**: Motorola 6809 @ 1.5MHz
- **Display**: Vector CRT (analog beam, no pixels)
- **RAM**: 896 bytes available (~182 bytes used)
- **ROM**: 32KB max (~8.3KB used)

### Rendering

All graphics are drawn as vector lines:
- **Cave walls**: Polyline segments with AABB collision
- **Sprites**: VLC format (Vector List Count) -- move count followed by (dy, dx) pairs
- **Beam intensity**: 4 levels for visual depth (dim=0x3F, normal=0x5F, hi=0x6F, bright=0x7F)

### Coordinate System

- Range: -128 to 127 in both axes
- Drawing uses `moveto_d(y, x)` (Y first, matching 6809 register convention)
- Walls defined as (center_y, left_x, half_height, width)

### Level System

Room-based layout. Each room contains:
- Cave polyline segments (horizontal = floor/ceiling, vertical = side walls)
- Enemy spawn points
- Wall blocks (destructible)
- Exits connecting to other rooms

## Source Files

| File | Description |
|------|-------------|
| `main.c` | Game loop, initialization, collision helpers |
| `player.c` | Player physics, input handling, drawing |
| `enemies.c` | Bat AI, laser, dynamite, miner rescue |
| `drawing.c` | Cave, sprites, HUD, title/game-over screens |
| `levels.c` | Level/room data management, enemy loading |
| `sprites.h` | VLC sprite data arrays |
| `levels.h` | Level wall and enemy data |
| `font.c` | Vector text rendering |

## Development Tool

**Vectrex Studio** (`tools/vectrex/studio/`) provides:
- Level editor with cave polyline drawing
- VLC sprite editor with vector line preview
- Built-in vec2x emulator with CPU register display
- Export to C source files
