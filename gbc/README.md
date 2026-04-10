# R.E.S.C.U.E. -- Game Boy Color

Tile-based scrolling platformer targeting the Game Boy Color hardware. Runs on real GBC cartridges (MBC5) and all major emulators.

## Build

```bash
make        # Build hero.gbc
make clean  # Remove build artifacts
```

Requires GBDK-2020 at `/Users/roger/retro-tools/gbdk/`.

## Architecture

### Hardware

- **CPU**: Sharp SM83 (Z80-like) @ 8MHz in double-speed CGB mode
- **Display**: 160x144 pixels, 8x8 tiles, 40 sprites (8x16 mode)
- **ROM**: 128KB (8 banks x 16KB), MBC5 cartridge mapper
- **Palettes**: 8 background palettes + 5 sprite palettes (4 colors each, RGB555)

### ROM Banking

| Bank | Content |
|------|---------|
| 0 | Game code, tileset, sprites, palettes (~13KB) |
| 1-5 | Level data (one C file per bank, auto-packed) |
| 6 | Screen data (title screen, etc.) |
| 7 | Free |

Bank switching uses `SWITCH_ROM()` via MBC5 registers. Each level stores its bank number in the `LevelInfo` struct, so `levels.c` switches to the correct bank before reading tile data.

### Rendering

- **Background**: 32x32 VRAM ring buffer, scrolled via SCX/SCY registers. Rows and columns streamed in as the camera moves.
- **Window layer**: 2 rows at bottom for HUD (level, hearts, dynamite, score, fuel bar)
- **Sprites**: 8x16 mode. Player (body + propeller), up to 3 active enemies, miner, dynamite/explosion, laser (3 segments), spider thread.
- **Tile palette**: Each tile has a per-tile palette index stored in `tile_palettes[]`, looked up at render time.

### Tileset

The tileset is fully managed in GBC Workbench and exported to C:

- Tiles 0-3: Base tiles (empty, solid color 1/2/3)
- Tiles 4-103: Game tiles (extracted from ZX Spectrum reference screenshot)
- Tiles 104-109: HUD icons (fuel bar, hearts, dynamite)
- Tiles 110-119: Digits 0-9
- Tiles 120-145: Letters A-Z
- Tiles 146-148: Symbols (dash, colon, dot)

Destroyable walls: tiles 4-6 (range check in `enemies.c`).

### Physics

All values tuned for 60fps GBC timing:

| Constant | Value | Description |
|----------|-------|-------------|
| GRAVITY | 1 | Pixels/frame downward |
| THRUST | 2 | Pixels/frame upward |
| MAX_VEL_Y | 3 | Terminal velocity |
| MAX_VEL_X | 2 | Horizontal flying speed |
| WALK_SPEED | 1 | Ground movement |
| PLAYER_HW | 3 | Hitbox half-width |
| PLAYER_HH | 7 | Hitbox half-height |

Collision snaps player to tile edges on contact (both X and Y).

## Workflow

The entire asset pipeline runs through GBC Workbench:

1. Edit tiles, sprites, levels, palettes, and screens in the Workbench
2. Click **Export** -- generates all C source files
3. Run `make` -- builds `hero.gbc`
4. Test in the Workbench's built-in emulator or any GBC emulator

### Exported Files

| File | Content |
|------|---------|
| `tileset_export.c/h` | Tile pixel data + per-tile palette array |
| `sprites_export.c/h` | 8x16 sprite data |
| `palettes_export.c/h` | BG + sprite palettes (RGB555) |
| `level_data_N.c` | Per-bank RLE level data with row offsets and entities |
| `level_data.h` | LevelInfo struct, NUM_LEVELS, externs |
| `levels_meta.c` | Level info table (width, height, bank, pointers) |
| `screens_export.c/h` | Full-screen tile layouts (title, etc.) |

## Source Files

| File | Lines | Description |
|------|-------|-------------|
| `main.c` | ~220 | Game loop, state machine, input reading |
| `player.c` | ~135 | Physics, collision, input handling |
| `enemies.c` | ~330 | Enemy AI, laser, dynamite, miner rescue |
| `render.c` | ~530 | Tile streaming, HUD, sprites, text screens |
| `levels.c` | ~155 | RLE decode, tile access, level init |
| `tiles.c` | ~50 | Load tileset/sprites/palettes, laser sprite |
| `game.h` | ~200 | Constants, types, externs |
| `tiles.h` | ~100 | Tile/sprite/palette index defines |
