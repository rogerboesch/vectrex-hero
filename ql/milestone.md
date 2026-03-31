# QL Port — Milestone Log

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
