# R.E.S.C.U.E. -- Apple IIe

Hi-Res graphics port targeting the Apple II Enhanced with its 6502 CPU. Outputs a bootable ProDOS disk image.

## Build

```bash
make        # Build bin/rescue.bin
make disk   # Create ProDOS disk image (bin/rescue.po)
make clean  # Remove build artifacts
```

### Toolchain

- **cc65**: C compiler + ca65 assembler + ld65 linker targeting `apple2enh`
- **AppleCommander**: Java tool for ProDOS disk image creation
- **mkdisk.py**: Custom Python script for self-booting 140KB ProDOS images

Tries system cc65 first, falls back to `$HOME/retro-tools/cc65/`.

## Architecture

### Hardware

- **CPU**: MOS 6502 @ 1.023MHz
- **Display**: 280x192 Hi-Res graphics, 6 colors
- **RAM**: 48KB main + 16KB aux (Apple IIe)

### Rendering

Hi-Res graphics with bit-plane operations:
- Custom screen addressing (Hi-Res memory layout is non-linear)
- Optimized sprite blitting in 6502 assembly (`blit.s`)
- Hardware-specific I/O via memory-mapped registers (`a2_hw.s`)

### Physics

All speeds scaled ~6x from the original design to account for the Apple II's ~10fps frame rate.

### Disk Format

The `make disk` target creates a self-booting ProDOS disk:
- Stage-1 bootloader in boot block
- Custom loader skips ProDOS kernel (direct binary load)
- 140KB floppy format (.po files)

## Source Files

| File | Description |
|------|-------------|
| `main.c` | Game loop |
| `player.c` | Player physics (6502-scaled timing) |
| `enemies.c` | Enemy AI |
| `render.c` | Hi-Res rendering (largest module) |
| `levels.c` | Level data |
| `sprites.c` | Sprite data |
| `a2_hw.s` | Hardware interface (memory-mapped I/O) |
| `blit.s` | Optimized 6502 sprite blitting |
| `apple2.cfg` | Linker memory configuration |
