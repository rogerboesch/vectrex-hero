# R.E.S.C.U.E. -- Sega Mega Drive / Genesis

Tile-based scrolling platformer with dual-plane parallax using the VDP.
Same level format as the GBC port, with enhanced graphics and parallax.

## Build

### Prerequisites

```bash
# Install M68K cross-compiler (macOS)
brew install m68k-elf-binutils m68k-elf-gcc

# Install SGDK (headers + libs)
git clone https://github.com/Stephane-D/SGDK.git ~/retro-tools/sgdk
cd ~/retro-tools/sgdk && make lib
```

### Build

```bash
make export  # Generate assets from GBC project file
make         # Build out/rom.bin
make clean   # Remove build artifacts
```

## Architecture

### Hardware

- **CPU**: Motorola 68000 @ 7.67MHz + Z80 @ 3.58MHz (sound)
- **Display**: 320x224 pixels, 40x28 tile grid (8x8 tiles)
- **VRAM**: 64KB (up to 2048 tiles)
- **Palettes**: 4 palettes x 16 colors (from 512 color RGB333)
- **Sprites**: 80 hardware sprites, 8x8 to 32x32
- **Scroll**: 2 independent background planes + window, per-line scroll
- **Sound**: YM2612 FM synthesis (6 channels) + PSG (4 channels)

### Rendering

- **Plane A (foreground)**: Game level tiles, streamed as camera scrolls
- **Plane B (parallax)**: Cave texture background, scrolls at 50% camera speed
- **Window plane**: HUD (fixed, 2 rows)
- Same ring-buffer tile streaming as GBC `render.c`

### Sprite Allocation

| Slots | Usage |
|-------|-------|
| 0 | Player body (16x24) |
| 1 | Propeller (16x8) |
| 2-9 | Active enemies |
| 10 | Miner |
| 11 | Dynamite / explosion |
| 12-14 | Laser segments |
| 15 | Spider thread |

### Level Data

- Same RLE format as GBC
- Same tile indices (compatible with GBC Workbench)
- No banking needed (64KB RAM)
- Larger viewport: 40x28 vs GBC's 20x18

### Joypad

| Button | Action |
|--------|--------|
| D-pad | Move / thrust |
| A | Fire laser |
| B | Place dynamite |
| Start | Start game |
