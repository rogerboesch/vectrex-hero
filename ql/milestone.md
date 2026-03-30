# QL Port — Milestone Log

## 2026-03-30: Emulator Integration

### Summary
Integrated the iQL Sinclair QL emulator directly into the Python sprite editor tool, matching the Vectrex pattern where sprite/level editing and emulator testing happen in a single unified window.

### What was done
- Created `ql/iql_ext/` — CPython extension module wrapping the iQL emulator core
  - `_iqlmodule.c`: Python bindings (init, tick, framebuffer, keyboard, stop/restart)
  - `iql_platform.c`: Headless platform stubs replacing Apple UI (rb_macapp.m, rb_platform.m)
  - `Makefile`: Compiles ~40 iQL C source files + stubs into `_iql.cpython-*.so`
- Created `ql/emu_tab.py` — Emulator tab module for the sprite editor
  - Displays iQL screen (512x256 RGBA, scaled 2x) via PIL/Pillow
  - Build & Run: exports sprites, runs make, starts emulator
  - Keyboard forwarding (Tkinter keysym → iQL virtual key codes)
  - Speed control (Normal / Slow)
- Modified `ql/sprite_editor.py` — Added ttk.Notebook with two tabs
  - Tab 0: Sprite Editor (existing functionality, unchanged)
  - Tab 1: Emulator (embedded iQL via _iql extension)

### Architecture
- iQL emulator runs in a background thread via `QLStart()` (blocking loop)
- Tkinter `after(20ms)` fires `_iql.tick()` which calls `QLTimer()` to advance emulation
- `pixel_buffer` (512x256 RGBA8888) read directly from emulator memory
- Keyboard events mapped from Tkinter keysyms to iQL RBVirtualKey codes

### Branch
`feature/ql-emulator-integration`

### Dependencies
- iQL source: `/Users/roger/projects/iQL` (compiled from, not modified)
- PIL/Pillow: for RGBA→Tkinter image conversion
- iQL system files: `~/Documents/iQLmac/` (ROMs, ql.ini, device dirs)
