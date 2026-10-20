#ifndef SHIPS_H
#define SHIPS_H

#include "wipeout.h"

/* One entry per track section, indexed by section number. */
/* Race order table: position along the lap and the ship it belongs to. */
typedef struct ShipRank {
    short position;
    short shipNo;
} ShipRank;

/* Texture table entry: CLUT and texture page VRAM coordinates. */
/* Ghost CLUT position (texture page relative). */
typedef struct GhostClut {
    u_short clutX;
    u_short clutY;
} GhostClut;

/* Shark body vertex as stored in the model table (8 byte stride). */
typedef struct SharkVert {
    u_short x;
    u_short y;
    short   z;
    short   pad;
} SharkVert;

/* XL ship record (0xF0 bytes). Offsets are taken from the code; the names of the
   16-bit block at 0x90..0xB0 and of 0xC0..0xC7 are provisional (see NOTES-ships.md). */

#endif
