# QL Port — Milestone Log

## 2026-03-31: Cave Wall Collision Fix + Emulator Debug Panel

### Summary
Fixed player tunneling through cave walls (3-iteration fix), then extended the emulator tab with pause/resume, restart confirmation, and a live CPU state debug panel.

### Cave wall collision fix (`player.c`)
- **Problem**: Player could walk and fly through cave segment walls (not destructible walls)
- **Root cause 1**: Exact-position edge case where `player_x == wall_x` missed by strict `<`/`>` checks
- **Root cause 2**: At high speeds (MAX_VEL_X=7 > PLAYER_HW=5), player completely clears wall in one frame
- **Fix**: Swept collision — store `prev_x`/`prev_y` before movement, check if bounding box edge crossed wall between old and new position
- **Also**: Increased MAX_CAVE_SEGS from 34 to 40 (one cave has 36 segments)
- **Also**: Split cave segment collision into X and Y phases (vertical segments checked after X move, horizontal after Y move)

### Emulator tab extensions (`_iqlmodule.c`, `emu_tab.py`)
- **C extension**: Added `pause()`, `resume()`, `is_paused()`, `get_cpu_state()` to `_iql` module
- **get_cpu_state()**: Returns dict with D0-D7, A0-A7, PC, USP, SSP, SR, keyboard state
- **PC calculation**: `(char*)pc - (char*)theROM` gives QL memory address
- **UI changes**: Pause/Resume toggle button, restart confirmation dialog, removed Stop button
- **Debug panel**: Right-side panel showing live CPU registers, decoded SR flags (T S I X N Z V C), USP/SSP, keyboard state
- **Update rate**: Every 5th tick normally, every tick when paused

## 2026-03-31: Keyboard Input — Hybrid KEYROW + IO.FBYTE

### Summary
Implemented hybrid keyboard input: KEYROW matrix scan for cursor keys (simultaneous detection) plus IO.FBYTE buffer for letter/action keys. Cursor keys support pressing UP+LEFT etc. simultaneously. Letter keys (Q/A/O/P) work as single-key aliases but can't be combined.

### Architecture (`ql/ql_hw.s`)

**Two input methods combined:**

| Method | Keys | Simultaneous? | Notes |
|--------|------|---------------|-------|
| KEYROW Row 1 (MT.IPCOM) | Cursor UP/DOWN/LEFT/RIGHT | Yes | Reads matrix bitmask directly |
| IO.FBYTE (console buffer) | Q/A/O/P/D/SPACE/ENTER/ESC | No | One key per read, buffered |

**Why hybrid:** On the iQL emulator (embedded CPython mode), KEYROW only works for Row 1 (cursor keys). Rows 4-6 (letter keys) return zero. IO.FBYTE works for all keys but can only return one at a time.

**Stale state workaround:** KEYROW returns the current physical key state, which can persist across state transitions (e.g. ENTER held on title screen appears as stale bits on first gameplay frame). `ql_flush_keys()` sets a skip counter (3 frames) during which KEYROW is ignored. Called on title→play and game over→title transitions.

### Key findings (iQL emulator)
- `KeyRow()` returns **1=pressed** (opposite of real QL active-low: 0=pressed)
- KEYROW **Rows 4-6 don't work** in iQL embedded mode (CPython extension)
- Row 1 (cursor keys) works correctly
- Keys sharing the same row (e.g. A+P on Row 4) cause ghost input

### Porting to real QL hardware
When testing on real QL hardware, the following changes will likely be needed in `ql_hw.s`:
1. **Invert KEYROW result**: Add `not.b d1` after the MT.IPCOM trap (real QL uses active-low: 0=pressed)
2. **Enable KEYROW for Rows 4-6**: Q (Row 6 bit 3), A (Row 4 bit 4), O (Row 5 bit 7), P (Row 4 bit 5) — move these from IO.FBYTE to KEYROW for proper simultaneous detection
3. **Ghost key testing**: A and P share Row 4 — test if pressing both causes phantom bits on other Row 4 keys (D=bit6, J=bit7 etc.)
4. **Stale state**: The keyrow_skip mechanism may still be needed, or the issue may not occur on real hardware — needs testing
5. **Keyboard auto-repeat**: IO.FBYTE may generate repeated characters from QDOS auto-repeat — not an issue for KEYROW which reads instantaneous state

### KEYROW Row Reference (for future Row 4-6 work)
```
Row 1: b7=DOWN b6=SPACE b4=RIGHT b3=ESC b2=UP b1=LEFT b0=ENTER
Row 4: b7=J    b6=D     b5=P     b4=A   b3=1  b2=H   b1=3  b0=L
Row 5: b7=O    b6=Y     b5=-     b4=R   b3=TAB b2=I  b1=W  b0=9
Row 6: b7=U    b6=T     b5=0     b4=E   b3=Q   b2=6  b1=2  b0=8
```

## 2026-03-31: Editor Improvements + Fly Animation

### Summary
Fixed rename bug in QL Studio, added move/flip operations to both sprite and image editors, redesigned the power bar HUD, and added 3-frame fly animation cycle.

### What was done
- **Rename fix**: Color shortcut keys (0-7) no longer intercept Entry widget input
- **Sprite operations**: Added flip_v, move up/down/left/right to Sprite class and Edit menu
- **Image operations**: Added flip_v, move up/down/left/right buttons to Image Editor
- **Power bar redesign**: Yellow bar on black background, "POWER" label, blue for depleted, centered and smaller (120px)
- **HUD lives**: Switched from player_walk_0 (10x20) to hud_live (4x10) sprite
- **Fly animation**: 0-1-2-1-0 cycle using player_fly_0/1/2 sprites, ~4 ticks per frame
- **Sprite re-export**: player_fly_1 and player_fly_2 now in sprites.c/h

## 2026-03-31: Image Editor Tab

### Summary
Added a third tab to QL Studio — an Image Editor for full 256x256 pixel images using the QL Mode 8 palette. Same drawing tools as the sprite editor (click, drag, right-click pick, keyboard 0-7) but for full-screen images.

### What was done
- Created `ql/tools/image_editor.py` — ImageEditorTab class (~480 lines)
  - PIL-based rendering (putpixel + NEAREST resize) instead of 65K Tkinter rectangles
  - 8-color QL palette, same swatch UI pattern as sprite editor
  - 1x/2x/4x zoom via radio buttons, scrollbars appear at higher zoom
  - Image list management (add/delete/duplicate/rename)
  - JSON serialization (get_images_data/set_images_data)
  - C export: images.c/images.h with raw Mode 8 byte encoding (32KB per image)
  - Mode 8 encoding: 4 pixels per 2 bytes, green+flash/red+blue planes, col_remap swaps 5/6
  - Coalesced redraws via after_idle for smooth painting
- Modified `ql/tools/sprite_editor.py` — Added Images tab (Tab 1)
  - Notebook now has 3 tabs: Sprites (0), Images (1), Emulator (2)
  - Central key binding management in _on_tab_changed (tracks _prev_tab)
  - Save/load/new project includes images data (backward-compatible)
- Modified `ql/tools/emu_tab.py` — Removed hardcoded key binding restore
  - on_tab_deselected() no longer binds editor._on_key (managed centrally)

### JSON format
```json
{ "sprites": [...], "images": [{"name": "...", "width": 256, "height": 256, "pixels": [[...]]}] }
```

### Branch
`feature/image-editor-tab`

## 2026-03-30: Emulator Integration

### Summary
Integrated the iQL Sinclair QL emulator directly into QL Studio (formerly "QL Sprite Editor"), matching the Vectrex pattern where sprite/level editing and emulator testing happen in a single unified window.

### What was done
- Created `ql/tools/iql_ext/` — CPython extension module wrapping the iQL emulator core
  - `_iqlmodule.c`: Python bindings (init, tick, framebuffer, keyboard, stop/restart)
  - `iql_platform.c`: Headless platform stubs replacing Apple UI (rb_macapp.m, rb_platform.m)
  - `Makefile`: Compiles ~40 iQL C source files + stubs into `_iql.cpython-*.so`
- Created `ql/tools/emu_tab.py` — Emulator tab module for QL Studio
  - Displays iQL screen (512x256 RGBA, scaled 2x) via PIL/Pillow
  - Build & Run: exports sprites, runs make, starts emulator
  - Keyboard forwarding (character-based, replicating iQL's character_to_vk + ql_shift_key_fixes)
  - Speed control (Normal / Slow)
- Modified `ql/tools/sprite_editor.py` — Added ttk.Notebook with two tabs
  - Tab 0: Sprites (sprite editing)
  - Tab 1: Emulator (embedded iQL via _iql extension)
- All Python tools live in `ql/tools/` (sprite_editor, emu_tab, tos2ql, iql_ext)

### Architecture
- iQL emulator runs in a background thread via `QLStart()` (blocking loop)
- Tkinter `after(20ms)` fires `_iql.tick()` which calls `QLTimer()` to advance emulation
- `pixel_buffer` (512x256 RGBA8888) read directly from emulator memory
- Keyboard: uses event.char with iQL's KeyCharacterSet + ql_shift_key_fixes for full character support
- Exported VK_LBRACKET/VK_RBRACKET from _iqlmodule.c for bracket/brace keys

### Branch
`feature/ql-emulator-integration`

### Dependencies
- iQL source: `/Users/roger/projects/iQL` (compiled from, not modified)
- PIL/Pillow: for RGBA→Tkinter image conversion
- iQL system files: `~/Documents/iQLmac/` (ROMs, ql.ini, device dirs)
