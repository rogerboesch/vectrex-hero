# Apple II Port of H.E.R.O. (R.E.S.C.U.E.) — Feasibility & Implementation Plan

## Context

The project has working H.E.R.O. ports on three platforms: **Vectrex** (6809/C), **Game Boy Color** (Z80/C via GBDK), and **Sinclair QL** (68008/C via vbcc). All share the same 20-level game data (`vectrex/src/levels.h`, ~12KB compiled from the 172KB source) and follow the same architecture pattern: `main.c` (state machine), `player.c` (physics), `enemies.c` (AI/weapons), `levels.c` (data), `render.c` (platform-specific graphics), plus platform-specific assembly for hardware access.

The goal is to plan an Apple II port — the most constrained target yet (1 MHz 6502, no hardware sprites, no scrolling).

---

## 1. Target Hardware

**Apple IIe enhanced (128KB) as primary, 64KB IIe as minimum**

- 64KB is sufficient for all game code + level data + one Hi-Res page (~30KB total)
- 128KB aux memory enables double-buffering (eliminates tearing) and a dedicated background buffer
- Auto-detect aux memory at startup; degrade gracefully on 64KB
- Do NOT target Apple II/II+ (too limited) or IIgs (different challenge, small audience)

## 2. Graphics Mode

**Hi-Res (280x192, 6 colors)**

- Lo-Res (40x48) is too coarse for cave geometry
- Double Hi-Res (560x192) is too slow to manipulate (soft-switch toggling per pixel)
- Hi-Res is the sweet spot with extensive tooling and community knowledge

**Color strategy:** Design sprites as **monochrome white** to avoid Hi-Res color fringing artifacts. This matches the Vectrex aesthetic. Use color only for HUD elements (fuel bar, lava).

**Screen layout:**
- Top 16px: HUD (score, lives, fuel, dynamite)
- Middle 160px: Playfield (maps game coords -128..127 x -50..50)
- Bottom 16px: spare/fuel bar

## 3. Programming Language & Toolchain

**cc65 (C + assembly hybrid)** — same pattern as the QL port (vbcc C + vasm assembly)

| Module | Language | Reuse from QL |
|--------|----------|---------------|
| `main.c` | C | ~80% copy (state machine identical, HW init differs) |
| `player.c` | C | ~95% copy (only constants change) |
| `enemies.c` | C | ~95% copy (only constants change) |
| `levels.c` | C | ~95% copy (include path change) |
| `render.c` | C + ASM | ~30% (grid+flood-fill reusable; pixel code all new) |
| `sprites.c` | C/data | 0% (Hi-Res format completely different) |
| `a2_hw.s` | ca65 ASM | 0% new (keyboard, speaker, Hi-Res soft switches) |
| `blit.s` | ca65 ASM | 0% new (sprite blit, rect copy, page ops) |

**Build:** Makefile -> cc65/ca65/ld65 -> AppleCommander for `.dsk` image. Cross-compile on macOS.

## 4. Memory Layout

### 64KB Apple IIe
```
$0000-$00FF  Zero page (cc65 runtime + game state hot vars)
$0100-$01FF  Stack
$0200-$07FF  ProDOS/text workspace
$0800-$1FFF  Free (~6KB): cave_seg_buf, flood fill stack, misc buffers
$2000-$3FFF  Hi-Res page 1 (8KB) -- DISPLAY
$4000-$5FFF  Hi-Res page 2 (8KB) -- BACKGROUND BUFFER (clean cave)
$6000-$BEFF  Free (~24KB): game code, level data (~12KB), sprites (~3KB)
$BF00-$BFFF  ProDOS global page
$C000+       I/O / ROM
```

### 128KB (with aux memory)
- Main $2000: display page (visible)
- Main $4000: draw page (render next frame) -- enables page-flipping
- Aux $2000: background buffer (clean cave for save-behind restore)

**Key insight:** All 20 levels (~12KB compiled) fit in RAM. No disk access needed after initial load.

## 5. Rendering Architecture

Follows the QL's **save-behind** approach:

**Room entry:**
1. Clear background buffer (Hi-Res page 2) to black
2. Rasterize cave geometry via grid + flood-fill (ported from QL `render.c`)
3. Render destructible walls, lava
4. Copy background to display page
5. Reset save-behind slots

**Per-frame (hot loop):**
1. For each sprite (player, 3 enemies, miner, dynamite, laser):
   - Skip if unchanged position (same optimization as QL)
   - Restore previous rect from background buffer -> display page
   - Draw sprite at new position with transparency
   - Record position for next frame
2. Update HUD incrementally (dirty tracking)

**Critical assembly routines (in `blit.s`):**
- `hgr_restore_rect` -- copy rect from page 2 to page 1 (handles non-linear Hi-Res addressing)
- `hgr_blit_sprite` -- draw sprite with mask/transparency
- `hgr_hline` / `hgr_vline` -- fast line primitives for cave walls
- `hgr_copy_page` -- full 8KB page copy (room entry only)

## 6. Performance Budget

**Target: 10-12 FPS** (QL achieves ~17 FPS at 7.5 MHz; Apple II is ~7x slower clock)

At 10 FPS -> ~102,300 cycles per frame:

| Task | Est. cycles |
|------|-------------|
| Game logic (physics, AI, collision) | 5,000-8,000 |
| Sprite restore (4-7 rects, ~12x20px) | 3,000-6,000 |
| Sprite draw (4-7 sprites) | 4,000-8,000 |
| HUD update (incremental) | 500-2,000 |
| Input + misc | 500 |
| **Total** | **~13,000-24,000** |

Fits well within budget. Room for optimization headroom.

## 7. Physics Constants (scaled for ~10 FPS)

| Constant | Vectrex (60fps) | QL (17fps) | Apple II (10fps) |
|----------|-----------------|------------|-------------------|
| GRAVITY | 1 | 2 | 3 |
| THRUST | 3 | 7 | 12 |
| MAX_VEL_Y | 4 | 8 | 14 |
| MAX_VEL_X | 3 | 7 | 12 |
| WALK_SPEED | 2 | 5 | 8 |
| LASER_LIFETIME | 10 | 3 | 2 |
| DYNAMITE_FUSE | 60 | 17 | 10 |
| EXPLOSION_TIME | 20 | 8 | 5 |

These are compile-time constants in `game.h` -- zero code changes to physics/AI modules.

## 8. Disk Format

**ProDOS on single 140KB 5.25" floppy.** Binary ~30KB + ProDOS ~30KB = ~60KB. Plenty of room.

Alternative: bare-metal DOS 3.3 boot disk with custom loader (saves 30KB, simpler).

## 9. Sound

**Minimal: speaker clicks** at $C030 for laser, explosion, death. Sound steals CPU from rendering -- keep it simple for initial port. Can enhance later.

## 10. Development & Testing

**Emulators:** AppleWin (Windows), MicroM8 or Virtual II (macOS), MAME (most accurate)

**Phased development:**
1. Toolchain setup + "hello world" on Apple II target
2. Compile game logic (player.c, enemies.c) with cc65 -- verify 6502 compatibility
3. Hi-Res primitives in assembly (pixel, line, rect)
4. Cave rasterization pipeline (port from QL render.c)
5. Save-behind sprite system in assembly
6. Sprite design (Hi-Res format, monochrome)
7. Keyboard input, HUD, sound
8. Integration, debugging, all 20 levels
9. Performance optimization pass

## 11. Key Risks

| Risk | Mitigation |
|------|------------|
| Performance shortfall at 10 FPS | Reduce sprite sizes, simplify cave cells, or accept 8 FPS |
| Hi-Res color fringing on sprites | Monochrome white sprites (matches Vectrex aesthetic) |
| cc65 generates slow physics code | Put hot vars in zero page (`#pragma zpsym`) |
| Flood fill slow at room entry | Acceptable -- original Atari 2600 H.E.R.O. has similar pauses |
| Swept collision needed at high velocities | Already implemented in QL player.c -- copy directly |

## 12. Directory Structure

```
apple2/
  Makefile
  apple2.cfg          # ld65 linker config (memory map)
  src/
    game.h            # Apple II constants (adapted from ql/src/game.h)
    main.c            # Game loop (adapted from ql/src/main.c)
    player.c          # Physics (copy from ql/src/player.c)
    enemies.c         # AI (copy from ql/src/enemies.c)
    levels.c          # Level loading (copy from ql/src/levels.c)
    render.c          # Cave rasterization + sprite dispatch
    sprites.c         # Apple II Hi-Res sprite data
    sprites.h         # Sprite definitions
    a2_hw.s           # Hardware: keyboard, speaker, Hi-Res init
    blit.s            # Sprite blitting, rect ops, page management
```

## 13. Critical Source Files to Reference

- `ql/src/game.h` -- template for Apple II game.h (constants, types, externs)
- `ql/src/player.c` -- nearly verbatim reusable (physics, swept collision)
- `ql/src/enemies.c` -- nearly verbatim reusable (AI, laser, dynamite)
- `ql/src/levels.c` -- nearly verbatim reusable (level/room loading)
- `ql/src/render.c` -- save-behind architecture to adapt (grid+flood-fill reusable)
- `vectrex/src/levels.h` -- shared level data (include via `../../vectrex/src/levels.h`)

## 14. Verification Plan

1. **Compile check:** `make` produces a `.dsk` image without errors
2. **Boot test:** disk boots in emulator, shows title screen
3. **Gameplay test:** play through levels 1-3 verifying physics, collision, enemies, laser, dynamite, room transitions, miner rescue
4. **Performance test:** measure actual FPS (frame counter in HUD or cycle counting)
5. **Full playthrough:** all 20 levels completable
6. **64KB test:** run on emulated 64KB IIe (no aux memory) -- verify graceful fallback
