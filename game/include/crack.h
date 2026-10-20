#ifndef CRACK_H
#define CRACK_H

#include "psyq.h"

/* Flat-textured triangle, laid out as the GPU primitive (32 bytes). */
typedef struct PolyFT3 {
    u_long  tag;
    CVECTOR colour0;
    DVECTOR xy0;
    u_char  u0, v0;
    u_short cba;
    DVECTOR xy1;
    u_char  u1, v1;
    u_short tsb;
    DVECTOR xy2;
    u_char  u2, v2;
    u_short pad1;
} PolyFT3;

/* Scratch space for FillMedLoCrack: the camera offset, the vertices, their screen positions and flags. */
typedef struct CrackLocals {
    VECTOR  camera;
    VECTOR  v[9];
    SVECTOR sv[9];
    DVECTOR screen[3][2];
    int    p;
    int    flags[3];
} CrackLocals;

#endif
