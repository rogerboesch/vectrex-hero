/* rom_header.c -- Mega Drive ROM header */
#include <genesis.h>

__attribute__((externally_visible))
const ROMHeader rom_header = {
    "SEGA MEGA DRIVE ",
    "(C)RESCUE  2026 ",
    "R.E.S.C.U.E.                                    ",
    "R.E.S.C.U.E.                                    ",
    "GM 00000000-00",
    0x000,
    "JD              ",
    0x00000000,
    0x000FFFFF,
    0xE0FF0000,
    0xE0FFFFFF,
    "RA",
    0xF820,
    0x00200000,
    0x0020FFFF,
    "            ",
    "R.E.S.C.U.E. CAVE RESCUE GAME           ",
    "JUE             "
};
