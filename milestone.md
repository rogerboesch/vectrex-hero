# H.E.R.O. / R.E.S.C.U.E. — Project Milestones

Multi-platform port of the classic Activision cave-exploration game.
The player uses a propeller pack, laser, and dynamite to navigate caves, destroy walls, and rescue trapped miners.

---

## Vectrex (Reference Platform)

The original and primary implementation. 6809 CPU, vector display.

- Initial H.E.R.O. implementation with vector sprites and cave rendering
- Level editor (Python/Tkinter) with built-in vec2x emulator
- Sprite editor with VLC (Vector List Count) format
- 20 levels designed and tested
- Player physics: propeller flight, gravity, wall sliding
- Enemies: bat (bouncing), spider (descending thread), snake (patrol)
- Weapons: laser beam, dynamite with expanding explosion
- HUD: power bar, lives, score, dynamite count
- Vectrex Studio (SDL2): room/row/sprite editors + integrated emulator
- Vectrex Studio: CPU state, memory viewer, 6809 disassembler, breakpoints
- VS Code-style UI: activity bar, status bar, breadcrumb, thin scrollbars

## Sinclair QL

Full port renamed R.E.S.C.U.E. 68008 CPU, Mode 8 display (256x256, 8 colors).

- Self-relocating QDOS executable via vbcc/vasm/vlink toolchain
- Custom tos2ql binary wrapper for iQL/sQLux/real QL compatibility
- Mode 8 rendering with correct even/odd byte color plane handling
- Background-restore sprite system (no hardware blitter)
- Per-sprite erase/draw cycle to eliminate flicker
- Fast axis-aligned line drawing (hline/vline with longword copies)
- Tile sprites for cave walls (brick pattern) and lava (wavy red/yellow)
- Custom 4x6 bitmap font with full character set
- HUD: colored labels, fuel bar, life/dynamite icons
- Player animation: walk cycle (2 frames), fly cycle (3 frames)
- QDOS keyboard input via IO.FBYTE and KEYROW matrix scan
- SuperBASIC launcher with screen clear/restore
- QL Studio (SDL2): sprite/image editors + integrated iQL emulator
- QL Studio: breakpoints, CPU state, memory viewer, 68000 disassembler
- QL Studio: image editor (256x256, Mode 8 export)
- VS Code-style UI: activity bar, status bar, breadcrumb
- Detailed article documenting the entire porting process

## Game Boy Color

Full port running on GBC hardware. Z80 CPU, 160x144 LCD, 2bpp tiles.

- GBDK-2020 toolchain producing 128KB ROM (2 banks)
- Scrolling tilemap system replacing room-based levels
- Smooth camera scrolling with window-based HUD at bottom
- 20 levels with RLE-compressed tile data in banked ROM
- Per-tile palette attributes (8 BG palettes, 4 colors each)
- Entity system: activate enemies near camera, deactivate when offscreen
- Sprite palettes for player, enemies, miner, items
- GBC Workbench (SDL2): full-featured tile/sprite/level editor
  - Level editor: tilemap canvas with zoom, clear, scroll, per-tile palette painting
  - Tile editor: 8x8 pixel editor with color picker and palette swatches
  - Sprite editor: 8x16 pixel editor with flip H/V, name, palette assignment
  - Asset list with 64x64 zoomed tile preview
  - BG palette selector with "Apply to all" button
  - Entity placement on sprite layer (5 types: player, bat, spider, snake, miner)
  - Integrated igb Game Boy Color emulator
  - Project JSON format with tiles, palettes, sprites, levels, per-tile palette map
  - C export with RLE-compressed level data
- ZX Spectrum tile extractor tools (Python)
  - Extract 8x8 tiles from Spectrum screenshots (256x192, 14 colors)
  - Auto-build up to 8 GBC palettes covering all Spectrum colors
  - Per-tile palette assignment for full-color reproduction
  - 17 sprites converted from QL project to GBC 8x16 2bpp format
- VS Code-style UI: activity bar, status bar, breadcrumb, thin scrollbars
- Codicon icons throughout, large activity bar icons (24pt)
- macOS dock icon set programmatically for bare binary execution

## Apple II

Port in early development. 6502 CPU, Hi-Res graphics (280x192).

- cc65 cross-compiler toolchain configured
- AppleCommander disk image builder (ProDOS .po format)
- Linker configuration for Apple IIe enhanced (36KB available)
- Feasibility plan covering:
  - Memory layout for 64KB and 128KB variants
  - Hi-Res monochrome rendering (white sprites to avoid color fringing)
  - Performance budget targeting ~10 FPS
  - Physics constant scaling for slower frame rate
  - 11-step phased development roadmap
- Source framework: main.c, player.c, enemies.c, levels.c, render.c, sprites.c

## Console

Headless terminal build for testing core game logic on Linux/macOS.

- Standard C build (gcc), no platform-specific graphics
- Used for build sanity checks and logic testing

## Shared Tools & Infrastructure

Cross-platform editor framework used by all studio tools.

- Shared SDL2 UI library (ui.h/ui.c): panels, buttons, text, icons, input fields
- Codicon icon font integration (codicon.ttf, 30+ icons)
- Native macOS support: file dialogs, menu bar, dock icon
- VS Code-inspired layout: activity bar, status bar, breadcrumb bar
- Thin overlay scrollbars (6px, semi-transparent, expand on hover)
- Dark theme with blue accents matching VS Code color scheme
- San Francisco system font (14pt) + Monaco monospace (12pt)
- All three studios (GBC, Vectrex, QL) share the same UI framework
- Shared level data format (levels.h) across Vectrex, QL, GBC, Console
