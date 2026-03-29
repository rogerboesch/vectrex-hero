# HUD & Drawing Cycle Optimization

Frame budget: ~50k cycles at 1.5 MHz / 30 fps. Goal: ~20-30k.

## Proposals

### 1. Replace `sprintf` with tiny `itoa` — Save ~4500 cycles
CMOC's `sprintf` with `%d` is ~1500-2000 cycles per call. A custom function
converting a small int to decimal chars in `str_buf` is ~50-100 cycles.
Applies to all 4 sprintf calls (3 in HUD, 1 in level message).

### 2. Reduce cave polyline groups — Save ~2000 cycles
Each group costs ~300-400 cycles overhead (zero_beam + intensity_a + set_scale
+ moveto_d). Room 1 has ~7 groups. Merge adjacent groups sharing the same
intensity into longer polylines in the level editor export. Target: 2-3 groups.

### 3. Reduce collision segment count — Save ~1500 cycles
Room 1 has 19 segments iterated twice in `update_player_physics` (X+Y pass)
and once per bat in `update_enemies`. Prune redundant segments (those covered
by wall rects or room bounds) in the level editor. Target: ~8-10 segments.

### 4. Fuel bar: single beam path — Save ~300 cycles
Draw both bright and dim portions in one beam sequence instead of two separate
zero_beam + set_scale + moveto sequences.

### 5. Combine HUD into one string — Save ~750 cycles
Format score, dynamite, lives into one padded string, print with one
`print_str_c` call. Eliminates 2 extra zero_beam + positioning calls.

### 6. Simplify player sprite — Save ~700 cycles
Player has 19 VLC vectors (densest sprite). Reduce to ~12-14 by simplifying
shape detail. ~100 cycles saved per removed vector.

### 7. Reduce `draw_laser_beam` segments — Save ~400 cycles
Currently 5 segments with alternating intensity_a. Draw 2-3 longer segments
or a single line with per-frame flicker instead of spatial alternation.

## Status

- [x] 1. Replace sprintf
- [x] 2. Cave polyline groups
- [ ] 3. Collision segments
- [x] 4. Fuel bar beam path
- [x] 5. Combine HUD string
- [ ] 6. Simplify player sprite
- [x] 7. Laser segments
