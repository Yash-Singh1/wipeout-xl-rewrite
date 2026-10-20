#ifndef DRAWASM_H
#define DRAWASM_H

/* Shared by the C translations of the renderer's hand-written assembly routines (trackdraw.c, partdraw.c,
   objdraw.c). */

/* GTE flag bits that reject a point: divide overflow, a screen coordinate out of range, SZ3/OTZ out of range. */
#define GTE_REJECT 0x80460000u

/* A camera-relative coordinate is usable when it fits in a signed 16-bit value: (v >> 15) is 0 or -1. */
static inline int FitsInHalfword(int value)
{
    return (value >> 15) == 0 || (value >> 15) == -1;
}

#endif
