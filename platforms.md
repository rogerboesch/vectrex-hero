# R.E.S.C.U.E. Port Candidates (86 Platforms)

Platforms where H.E.R.O. does NOT already exist and R.E.S.C.U.E. would be worth porting.

---

## FAVORITES

Best candidates based on existing code reuse (Vectrex/6809, GBC/Z80, QL/68000, Apple II/6502):

| Priority | Platform | Reuses | Toolchain | Effort |
|----------|----------|--------|-----------|--------|
| 1 | Game Boy DMG | GBC code + tileset | GBDK (same) | Days |
| 2 | Atari ST | QL code (same 68000 CPU) | VBCC (same) | 1 week |
| 3 | Master System / Game Gear | GBC tileset, similar Z80 | devkitSMS | 1-2 weeks |
| 4 | Dragon 32 | Vectrex code (same 6809 CPU) | CMOC (same) | 1-2 weeks |
| 5 | Sega Genesis | QL code (68000) + GBC tileset | SGDK | 2-3 weeks |
| 6 | SNES | GBC tileset concept, hw parallax | PVSnesLib | 2-3 weeks |
| 7 | Spectrum Next | ZX tileset (already extracted) | z88dk | 2-3 weeks |
| 8 | NES | GBC tileset concept | cc65 (same as Apple II) | 3-4 weeks |
| 9 | Apple IIGS | Apple II code (65C816 superset) | cc65 | 2-3 weeks |

---

## 🔥 HIGH Market Opportunity (26 platforms)

### Maybe

| Platform | Notes | Dev Difficulty |
|---|---|---|
| SNES | Enhanced 16-bit graphics | Medium |
| Sega Genesis | Two-player potential | Medium |
| Master System | Close to Game Gear | Medium |
| Game Gear | Handheld counterpart | Medium |
| Atari Lynx | 16-bit handheld | High |
| Lynx II | Improved Atari Lynx | High |
| Game Boy | Original monochrome handheld | Low |
| Game Boy Pocket | Improved Game Boy | Low |
| Spectrum Next | Modern reimagining | Low |
| Nintendo DS | Dual-screen handheld | Medium |
| PlayStation Portable | 32-bit handheld | Medium |
| Nintendo 3DS | 3D handheld | Medium |
| ZX Spectrum 128 | Expanded memory version | Medium |
| Switch (homebrew) | Modern handheld | Low |
| Sega Nomad | Portable Genesis | Medium |

### Probably not

| Platform | Notes | Dev Difficulty |
|---|---|---|
| Neo Geo | Arcade-quality home version | High |
| TurboGrafx-16 / PC Engine | 16-bit Japanese console | Medium |
| Famicom | Japanese NES variant | Medium |
| Amstrad CPC 6128 | Upgraded CPC | Medium |
| Raspberry Pi | With RetroPie | Low |
| FPGA Cores (MiSTer/Analogue) | Various arcade/computer emulation | Low |
| Miniature Classic Consoles | NES Mini, SNES Classic | Low |
| 2600+ / ColecoVision Flash Back | Modern retro boards | Low |
| MAME arcade builds | Running on PCs | Low |
| WebGL/Emscripten | Browser-based port | Low |

---

## 🟡 MEDIUM Market Opportunity (19 platforms)

### Maybe

| Platform | Notes | Dev Difficulty |
|---|---|---|
| Atari 7800 | Improved Atari 2600 hardware | High |
| Sega Dreamcast | Final console innovation | Medium |
| Virtual Boy | Novelty 3D handheld | High |
| Dragon 32 | 6809-based (like Vectrex!) | Low |
| Apple II GS | 16-bit Apple successor | Low |
| Apple Macintosh Classic | Early Mac | Low |
| Atari ST | Motorola 68000 computer | Low |

### Probably not

| Platform | Notes | Dev Difficulty |
|---|---|---|
| WonderSwan | Japanese handheld | Medium |
| Intellivision | 1980s console | High |
| Famicom Disk System | Nintendo disk-based variant | Medium |
| Wii (homebrew) | Modern retro capability | Low |
| GameCube (homebrew) | Alternative modern platform | Low |
| ZX Spectrum +3 | With disk drive | Medium |
| IBM PC (DOS) | CGA/EGA graphics | Low |
| Tandy 1000 | DOS variant with enhanced graphics | Low |
| Atari XL/XE | 8-bit home computer line | Medium |
| BeagleBone | ARM-based retro box | Low |
| Odroid | Enhanced RPi variant | Low |
| Unix/Linux terminals | Text-based roguelike adaptation | Low |

---

## 📉 LOW Market Opportunity (41 platforms)

### Maybe

| Platform | Notes | Dev Difficulty |
|---|---|---|
| TI-99/4A | 16-bit home computer | High |
| Commodore PET | Early home computer | High |
| Acorn Archimedes | 32-bit Arm-based | Medium |
| Oric-1/Oric Atmos | 6502-based competing line | Medium |

### Probably not

| Platform | Notes | Dev Difficulty |
|---|---|---|
| Arcade Systems (15 boards) | Galaxian, Pac-Man, Donkey Kong, Frogger, Williams, Capcom, SNK, Taito, Konami, Data East, Sega, Namco, Irem, Stern, Midway | High |
| Sega Master System Arcade | Arcade version | High |
| Sharp Twin Famicom | Home arcade machine | High |
| Sega SG-1000 Arcade | Arcade variant | High |
| BBC Micro | Similar era to QL | High |
| Acorn Electron | BBC Micro variant | High |
| TRS-80 | Radio Shack's 8-bit computer | High |
| Ohio Scientific | 6502-based competitor | High |
| ICL OPD | Lesser-known 8-bit | High |
| PC Booter | Bootable games | Medium |
| CP/M systems | Various 8-bit machines | High |
| Commodore 16 | Budget Commodore | Medium |
| Plus/4 | Commodore's answer to C64 | Medium |
| SAM Coupé | 8-bit cybernetic computer | Medium |
| Fairchild Channel F | Early cartridge console | High |
| Magnavox Odyssey | First home console | High |
| RCA Studio II | Low-end home console | High |
| Mattel Auto Race | LCD handheld | High |
| Milton | Milton Bradley LCD game | High |
| Tiger LCD games | Custom handheld chips | High |
| Project Gotham | Arcade handheld | High |
| Nintendo Color Screen Game & Watch | LCD-based | High |
| Game & Watch Gallery | Nintendo LCD portables | High |
| Battlechip Gate | Mega Man authentication hardware | High |
| Gakken LCD portables | Japanese LCD handhelds | High |
| Entex Select-A-Game | Cartridge handheld | High |
| Dedicated arcade cabinets | Custom arcade hardware | High |
| Arduino | Minimal retro gaming | Low |

---

## Legend

**Development Difficulty:**
- **LOW**: Modern toolchains, excellent documentation, active dev community
- **Medium**: Established systems with reasonable dev tools and documentation  
- **High**: Obscure systems, poor toolchain support, custom/proprietary hardware

---

## Excluded Platforms (H.E.R.O. already exists)

- Amstrad CPC 464
- Atari 2600
- Apple II
- Atari 5200
- Atari 8-bit computers
- ColecoVision
- Commodore 64
- MSX
- SG-1000
- ZX Spectrum
- PlayStation
- PlayStation 2
- Game Boy Advance
- iOS
- Android

