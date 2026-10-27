#ifndef SHIPS_H
#define SHIPS_H

/* ShipData.combatAttr */
#define REMOTE_SHIP 0x0002
#define LAP_STARTED 0x4000

#include "wipeout.h"

/* Frames of the start countdown (ShipData.updateCount while the ships are held on the grid). */
#define START_COUNTDOWN_FRAMES PAL_TIME(200)
/* updateCount values at which the grid countdown steps to its next light and sounds its beep. */
#define COUNTDOWN_STEP_1 REGION(33, 41)
#define COUNTDOWN_STEP_2 REGION(66, 83)
#define COUNTDOWN_STEP_3 REGION(100, 125)

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
