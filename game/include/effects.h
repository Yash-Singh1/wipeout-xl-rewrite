#ifndef EFFECTS_H
#define EFFECTS_H

#include "functions.h"
#include "include_asm_g8.h"
#include "tim.h"

/* Mono-rail carriage: current step index and per-frame deltas along the rail. */
struct MonoRailData {
    short index;                  /* 0x00 */
    short pad02;                  /* 0x02 */
    int  stepX, stepY, stepZ;    /* 0x04 */
    int  pad10[5];               /* 0x10 */
    int  totalX, totalY, totalZ; /* 0x24 */
};

/* Start-grid droid: section it stands on, whether it is racing, and a spare word. */
struct DroidData {
    short section;
    short active;
    short pad;
};

/* Checkpoint on the race line: section number, seconds allowed and lap. */
struct Checkpoint {
    short       section;
    short       seconds;
    signed char lap;
    signed char pad;
};

/* Racing line point: the section it belongs to and a position. */
/* Lookup table with one 16-bit entry per track or class index. */
typedef struct SideTable {
    short value[8];
} SideTable;

/* Rectangle with unsigned fields, as the video code stores them. */
typedef struct Rect {
    u_short x, y, w, h;
} Rect;


/* One train-light entry: the colour bytes of a G3/G4 primitive, as the lighting code addresses them. */
typedef struct TrainLight {
    unsigned char pad[0xC];
    unsigned char c1[4];
    unsigned char c2[4];
    unsigned char c3[4];
    unsigned char c4[3];
} TrainLight;

/* Per-screen shadow quads for one ship: 16 quads, one per screen buffer. */
struct Shadow {
    POLY_FT4 quad[16][2];
};

/* Header entry at the start of an object's primitive list: the type word and a flag word. */
typedef struct PrimHeader {
    short   type;
    u_short flag;
} PrimHeader;

/* Texture fields of a textured quad (GT4 or FT4) from its tsb word on. The clut word (cba) sits
   just before tsb. Colours are only present in GT4 records. */
typedef struct TexFields {
    short   tsb;
    u_char  u0, v0, u1, v1, u2, v2, u3, v3;
    short   pad;
    CVECTOR colour[4];
} TexFields;

#endif
