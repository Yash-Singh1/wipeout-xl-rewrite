#ifndef TIM_H
#define TIM_H

#include "wipeout.h"

/* TIM file header: id, flags (low 3 bits: CLUT type). */
typedef struct Tim {
    int id;
    int flags;
} Tim;

/* TIM with direct 16-bit pixels and no CLUT. */
typedef struct TimDirect16 {
    int    id;
    int    flags;
    int    textureBytes;
    u_short textureX, textureY, textureW, textureH;
} TimDirect16;

/* TIM with a 16-entry CLUT (4-bit pixels). */
typedef struct TimClut4 {
    int   id;
    int   flags;
    int   clutBytes;
    short  clutX, clutY, clutW, clutH;
    short  clut[16];
    int   textureBytes;
    short  textureX, textureY, textureW, textureH;
} TimClut4;

/* TIM with a 256-entry CLUT (8-bit pixels). */
typedef struct TimClut8 {
    int   id;
    int   flags;
    int   clutBytes;
    short  clutX, clutY, clutW, clutH;
    short  clut[256];
    int   textureBytes;
    short  textureX, textureY, textureW, textureH;
} TimClut8;

#endif
