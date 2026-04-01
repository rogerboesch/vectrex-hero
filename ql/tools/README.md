# QL Studio — Development Tools for Sinclair QL

QL Studio is an integrated development environment for Sinclair QL game development, built in Python/Tkinter with an embedded iQL emulator.

## Quick Start

```bash
cd ql/tools
python3 qlstudio.py ../sprites.json
```

## Tabs

### Sprites Tab
Design sprites for QL Mode 8 (256x256, 8 colors).

- **Left panel**: Sprite list with add/delete/duplicate
- **Center**: Pixel grid editor — left-click to paint, right-click to pick color
- **Right panel**: 8-color palette, 2x preview, byte size counter
- **Keyboard**: Press `0`-`7` to quickly select a color

**Sprite format**: Nibble-packed, 2 pixels per byte (high nibble = left pixel). Width must be even.

**Export**: File → Export C files → generates `sprites.c` and `sprites.h` ready to compile.

### Images Tab
Design full-screen images and title screens.

### Emulator Tab
Embedded iQL Sinclair QL emulator with full debugging tools.

## Emulator Features

### Toolbar

| Button | Description |
|--------|-------------|
| **Build & Run** | Exports sprites, runs `make`, starts emulator |
| **Pause / Resume** | Toggle emulation pause |
| **Restart** | Restart the emulator |
| **Speed** | Normal or Slow emulation speed |
| **Screenshot** | Save emulator display as PNG |
| **Trap Log** | Toggle QDOS trap call logging (checkbox) |

### Debug Panels

#### CPU Tab
Live display of all 68K registers while running:
- **D0-D7**: Data registers (hex)
- **A0-A7**: Address registers (hex)
- **PC**: Program counter
- **SR**: Status register with decoded flags (T/S/IPL/X/N/Z/V/C)
- **USP/SSP**: User and supervisor stack pointers
- **Keyboard state**: Current key, shift, control, alt

**Step buttons** (work when paused):
- **Step 1**: Execute 1 machine instruction
- **Step 10**: Execute 10 instructions
- **Step 100**: Execute 100 instructions

#### Memory Tab
Hex dump viewer for any QL memory address.

- Enter hex address in the **Addr** field, press **Go** or Enter
- Configurable row count (4-64 rows, 16 bytes each)
- Non-zero bytes highlighted in orange
- ASCII sidebar for readable characters

**Useful addresses**:
| Address | Contents |
|---------|----------|
| `$20000` | Screen memory (32KB) |
| `$28000` | System variables (QDOS) |
| `$00000` | ROM |

#### Break Tab
Set breakpoints by PC address.

- Enter hex address, click **Add** or press Enter
- **Remove**: Delete selected breakpoint
- **Clear All**: Remove all breakpoints
- When a breakpoint hits: emulator pauses, status bar shows address

#### Watch Tab
Monitor named memory addresses with live values.

- Enter hex **Addr**, optional **Name**, select type (**byte** / **word** / **long**)
- Click **Add** to start watching
- Values update automatically every emulator tick
- **Remove**: Delete last watch entry

## Software Breakpoints

The most powerful debugging feature. Add breakpoints directly in your C game code — no need to know PC addresses.

### Setup

Include `debug.h` in your game source:

```c
#include "debug.h"
```

### Usage

```c
// Simple breakpoint — emulator pauses with "SOFTWARE BP #1"
BREAK(1);

// Use different IDs to identify which breakpoint hit
BREAK(2);   // "SOFTWARE BP #2"
BREAK(3);   // "SOFTWARE BP #3"

// Conditional breakpoint
BREAK_IF(player_y < -45);   // break when player near floor

// Debug a specific code path
void update_dynamite(void) {
    if (dyn_exploding) {
        BREAK(10);  // pause here to inspect explosion state
        ...
    }
}

// Debug collision
if (box_overlap(player_x, player_y, PLAYER_HW, PLAYER_HH,
                enemies[i].x, enemies[i].y, ehw, ehh)) {
    BREAK(20);  // pause when player hits enemy
    game_state = STATE_DYING;
}
```

### How It Works

1. `BREAK(n)` writes byte `n` (1-255) to memory address `$3FFFE`
2. The emulator checks this address every tick (~50 times/second)
3. When non-zero: emulator pauses, resets the byte to 0
4. Console shows: `*** SOFTWARE BP #10 at $xxxxxx ***`
5. All debug panels (CPU, Memory, Watch) update with current state
6. Press **Resume** to continue execution

### On Real Hardware

`BREAK()` writes to unused RAM at `$3FFFE`. On a real QL this has zero effect — the game runs normally. No need to remove breakpoints for release builds.

### Macros

| Macro | Description |
|-------|-------------|
| `BREAK(n)` | Unconditional break with ID `n` (1-255) |
| `BREAK_IF(cond)` | Break only if condition is true (uses ID 1) |

## Trap Logger

When the **Trap Log** checkbox is enabled, every QDOS system call is logged to the Console panel:

```
TRAP #1  d0=$08 MT.SUSJB   d1=$FFFFFFFF d3=$0003
TRAP #3  d0=$01 IO.FBYTE   d3=$0000
TRAP #1  d0=$10 MT.DMODE   d1=$00000008 d3=$0000
TRAP #2  d0=$01 IO.OPEN    d1=$FFFFFFFF
```

### QDOS Trap Reference

**TRAP #1 — Manager Traps**:

| d0 | Name | Description |
|----|------|-------------|
| $00 | MT.INF | Get system info |
| $01 | MT.CJOB | Create job |
| $05 | MT.FRJOB | Force remove job |
| $08 | MT.SUSJB | Suspend job (frame wait) |
| $09 | MT.RELJB | Release job |
| $0B | MT.PRIOR | Set priority |
| $10 | MT.DMODE | Set display mode |
| $13 | MT.RCLCK | Read clock |
| $18 | MT.ALCHP | Allocate common heap |

**TRAP #2 — I/O Open/Close**:

| d0 | Name | Description |
|----|------|-------------|
| $01 | IO.OPEN | Open channel |
| $02 | IO.CLOSE | Close channel |

**TRAP #3 — I/O Operations**:

| d0 | Name | Description |
|----|------|-------------|
| $00 | IO.PEND | Check pending input |
| $01 | IO.FBYTE | Fetch byte (keyboard read) |
| $05 | IO.SBYTE | Send byte |

## Build System Integration

The **Build & Run** button:
1. Exports sprites from the editor to `sprites.c` / `sprites.h`
2. Runs `make` in the `ql/` directory
3. Starts the iQL emulator with the compiled binary
4. Keyboard input goes directly to the emulated QL

This means: edit a sprite → click Build & Run → see the change in-game. Full round-trip in seconds.

## iQL Extension API

The emulator is wrapped as a CPython extension (`_iql`). Available functions:

### Emulator Control
| Function | Description |
|----------|-------------|
| `init(path)` | Start emulator with system path |
| `tick()` | Advance one emulation step |
| `stop()` | Stop emulator thread |
| `restart()` | Restart emulator |
| `pause()` | Pause emulation |
| `resume()` | Resume emulation |
| `is_running()` | Check if emulator is alive |
| `is_paused()` | Check if paused |
| `set_speed(ms)` | Set CPU throttle (0=normal, 4=slow) |

### Display
| Function | Description |
|----------|-------------|
| `get_framebuffer()` | Return RGBA pixel buffer as bytes |
| `get_screen_size()` | Return (width, height) tuple |
| `screenshot(filename)` | Return RGBA data dict for PNG saving |

### Input
| Function | Description |
|----------|-------------|
| `send_key(vk, pressed, shift, ctrl, alt)` | Send key event |

### Memory Access
| Function | Description |
|----------|-------------|
| `read_byte(addr)` | Read byte from QL memory |
| `read_word(addr)` | Read 16-bit word |
| `read_long(addr)` | Read 32-bit long |
| `read_mem(addr, length)` | Read block as bytes (max 64KB) |
| `write_byte(addr, value)` | Write byte |
| `write_word(addr, value)` | Write 16-bit word |
| `write_long(addr, value)` | Write 32-bit long |

### Debugging
| Function | Description |
|----------|-------------|
| `step(count)` | Execute N 68K instructions |
| `get_cpu_state()` | Return dict of all registers |
| `get_log()` | Drain emulator log buffer |

### Breakpoints
| Function | Description |
|----------|-------------|
| `add_breakpoint(addr)` | Set PC breakpoint (max 16) |
| `remove_breakpoint(addr)` | Remove breakpoint |
| `clear_breakpoints()` | Remove all breakpoints |
| `list_breakpoints()` | Get list of active addresses |
| `check_breakpoint()` | Return hit info dict or None |

### Trap Logging
| Function | Description |
|----------|-------------|
| `set_trap_logging(enable)` | Enable/disable QDOS trap logging |
| `get_trap_logging()` | Check if trap logging is on |
| `get_trap_log()` | Drain trap log buffer |
