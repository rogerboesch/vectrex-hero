# QL Port — Milestone Log

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
