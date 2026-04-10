# R.E.S.C.U.E.
## Remote Exploration & Sub-surface Cavern Utility Expert

A multi-platform retro game inspired by Activision's H.E.R.O. (1984). Navigate underground caverns, rescue trapped miners, and fight your way through enemies using a propeller backpack, laser, and dynamite.

Each platform port is built from scratch in C, targeting the original hardware constraints. All platforms share the same game design but adapt the rendering, physics, and controls to each system's capabilities.

## Platforms

| Platform | CPU | Display | Toolchain | Status |
|----------|-----|---------|-----------|--------|
| [Vectrex](vectrex/) | 6809 @ 1.5MHz | Vector CRT | CMOC | Playable |
| [Game Boy Color](gbc/) | SM83 @ 8MHz | 160x144 LCD | GBDK-2020 | Playable |
| [Sinclair QL](ql/) | 68008 @ 7.5MHz | 256x256, 8 colors | VBCC | Playable |
| [Apple IIe](apple2/) | 6502 @ 1MHz | 280x192 Hi-Res | cc65 | Playable |

## Gameplay

You control a rescue worker equipped with:
- **Propeller backpack** -- fly through caverns (drains fuel)
- **Laser** -- horizontal beam to destroy enemies
- **Dynamite** -- destroys walls blocking your path

Each level has a miner trapped deep underground. Navigate through enemies (bats, spiders, snakes) and destructible walls to reach them.

### Controls

| Action | Vectrex | GBC | QL / Apple II |
|--------|---------|-----|---------------|
| Move left/right | Joystick | D-pad | Arrow keys |
| Thrust up | Joystick up | Up | Up arrow |
| Fire laser | Button 1 | A / Space | Space |
| Place dynamite | Joystick down | B / S | S |

### Scoring

- 50 pts per enemy killed
- 75 pts per wall destroyed
- 1000 pts per miner rescued + fuel bonus

## Development Tools

Each platform has a dedicated SDL2-based editor with a built-in emulator:

| Tool | Platform | Features |
|------|----------|----------|
| **Vectrex Studio** | Vectrex | Vector sprite editor, room editor, 6809 debugger |
| **GBC Workbench** | Game Boy Color | Tile/sprite/screen editor, level editor, Z80 debugger |
| **QL Studio 2** | Sinclair QL | Sprite editor, M68K debugger, QDOS emulation |

All tools share a common UI framework (`tools/shared/`) built on SDL2 with native macOS integration.

## Project Structure

```
vectrex/          Vectrex port (vector graphics, 6809 assembly)
gbc/              Game Boy Color port (tile-based, ROM banking)
ql/               Sinclair QL port (68K, QDOS executable)
apple2/           Apple IIe port (6502, Hi-Res graphics)
tools/
  shared/         Common SDL2 UI framework
  vectrex/studio/ Vectrex Studio editor + vec2x emulator
  gbc/studio/     GBC Workbench editor + iGB emulator
  ql/studio/      QL Studio 2 editor + iQL emulator
assets/           Reference images and original project files
```

## Building

Each platform has its own `Makefile` in its directory. See the platform-specific README for build instructions and toolchain requirements.
