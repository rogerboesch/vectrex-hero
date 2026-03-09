# H.E.R.O. for Vectrex

A Vectrex conversion of Activision's H.E.R.O. (1984), built with the VectreC toolchain and CMOC.

## Build

```bash
make        # Compile to main.bin
make run    # Launch in vec2x emulator
make clean  # Remove build artifacts
```

Requires the VectreC toolchain at `$HOME/retro-tools/vectrec/`.

## Controls

| Input | Action |
|---|---|
| Joystick left/right | Walk (on ground) or fly horizontally |
| Joystick up | Activate prop-pack (thrust upward, drains fuel) |
| Joystick down (on ground) | Place dynamite |
| Button 1 | Fire laser in facing direction |
| No input | Fall under gravity |

## Features

| Feature | Description |
|---|---|
| Player movement | Gravity, thrust, horizontal walk/fly |
| Propeller animation | 2-frame alternating prop blade rotation |
| Fuel system | 255 starting fuel, drains while thrusting |
| Laser | Horizontal beam in facing direction, kills bats |
| Dynamite | Timed fuse, explosion destroys walls and enemies |
| Bats | Horizontal patrol, bounce off edges, 2-frame wing flap, kill player on contact |
| Wall collision | AABB collision with landing, side push-out, ceiling push-out |
| Screen transitions | Flip-screen when crossing edges (up/down/left/right) |
| Level 1 | 3 screens - vertical shaft, 1 bat, miner at bottom |
| Level 2 | 4 screens - branching path, 2 bats, miner at bottom |
| Miner rescue | Touch miner for 1000 pts + fuel/dynamite bonus, advance level |
| HUD | Score, lives, fuel bar, dynamite count |
| Death/respawn | Bat contact or explosion kills player, flash animation, respawn at level start |

## Scoring

- 50 pts per bat killed
- 75 pts per wall destroyed
- 1000 pts per miner rescued + remaining fuel + 50 per remaining dynamite

## Game Flow

```
TITLE -> PLAYING -> GAME_OVER -> TITLE
              |-> LEVEL_COMPLETE -> PLAYING (next level)
              |-> DYING -> PLAYING (respawn) or GAME_OVER
```

## Architecture

Single `.c` file (~1040 lines) matching the VectreC workflow. ROM binary is ~8.3KB.

- Coordinate system: -128 to 127 in both axes (Y, X ordering)
- Each "screen" maps to one Vectrex display with flip-screen transitions
- Walls defined as rectangular blocks with AABB collision
- Level data stored as arrays of screen definitions
- 896 bytes RAM available; game uses ~182 bytes
