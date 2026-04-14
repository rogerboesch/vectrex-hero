# R.E.S.C.U.E. -- Commodore 64

Tile-based scrolling platformer using the VIC-II character mode with smooth scrolling. Same level format as the GBC port.

## Build

```bash
make        # Build rescue.prg
make clean  # Remove build artifacts
```

Requires cc65 at `$HOME/retro-tools/cc65/` or on system PATH.

## Architecture

### Hardware

- **CPU**: MOS 6510 (6502 variant) @ 1.023 MHz
- **Display**: 40x25 character grid (320x200 pixels), VIC-II chip
- **Colors**: 16 fixed colors, 1 foreground color per 8x8 cell
- **Sprites**: 8 hardware sprites (24x21 pixels, multicolor)
- **Sound**: SID chip (3 voices + noise)
- **RAM**: 64KB (no banking needed)

### Rendering

- Custom charset in RAM = tileset (up to 256 8x8 tiles)
- Screen RAM (40x25) = tile map, streamed as camera scrolls
- Color RAM = per-cell foreground color
- VIC-II smooth scroll registers (3-bit X/Y, 0-7 pixels)
- Same ring-buffer streaming approach as GBC `render.c`

### Sprite Allocation

| Slot | Usage |
|------|-------|
| 0 | Player body |
| 1 | Propeller |
| 2-4 | Active enemies (bat, spider, snake) |
| 5 | Miner |
| 6 | Dynamite / explosion |
| 7 | Laser |

### Level Data

- Same RLE format as GBC (level_data.c)
- Row-offset decode cache for fast random access
- No banking needed — 64KB RAM is sufficient

## Implementation Plan

1. **Scaffold**: Makefile, game.h, main loop with raster timing
2. **Rendering**: VIC-II charset tiles, screen RAM streaming, smooth scroll
3. **Sprites**: Player, enemies in C64 sprite format
4. **Game logic**: Physics, collision, enemies (ported from GBC)
5. **Levels**: RLE decode, tile_at/tile_solid
6. **Polish**: Title screen, SID sound, color tuning
