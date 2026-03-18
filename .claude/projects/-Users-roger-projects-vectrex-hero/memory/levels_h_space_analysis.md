---
name: levels.h space analysis
description: Detailed breakdown of ROM space usage in levels.h (18,200 bytes total) — cave segs are biggest target for optimization
type: project
---

ROM is 34,005 / 32,768 bytes (1,237 over limit).

levels.h rodata breakdown (18,200 bytes, 54% of ROM):

| Category | Arrays | Bytes | % |
|---|---|---|---|
| **cave_segs** (collision segments) | 90 | **6,228** | 34.2% |
| **cave_lines** (draw data) | 70 | **3,954** | 21.7% |
| **enemy data** | 276 | **1,499** | 8.2% |
| **room exits** | 20 | **1,044** | 5.7% |
| **room cave pointer tables** | 20 | **788** | 4.3% |
| room_bounds (per level) | 20 | **1,011** | 5.6% |
| room_starts + room_miners | 40 | **672** | 3.7% |
| wall data (per room) | ~300 | **1,448** | 8.0% |
| room wall/seg/miner/lava tables | 80 | **1,024** | 5.6% |
| room wall/enemy counts | 40 | **512** | 2.8% |

**Why:** Need to cut at least 1,237 bytes to fit in 32KB ROM.
**How to apply:** Cave segs (6,228 bytes) is the #1 target — redundant with cave_lines draw data. Room bounds can be constants (same for all levels). Levels 11-20 stubs should be removed by re-exporting.
