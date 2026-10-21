/* The PlayStation's geometry coprocessor (GTE), as the PsyQ libgte routines
   the game calls use it, and the full-precision vertex cache the renderer
   draws from. */
#ifndef PSX_GTE_H
#define PSX_GTE_H

#include <stdint.h>

/* Registers, numbered as the hardware numbers them (data 0-31, control 0-31). */
typedef struct Gte {
    int32_t data[32];
    int32_t control[32];
} Gte;

extern Gte gte;

/* Coprocessor instructions, by their 25-bit command word. */
void GteExecute(uint32_t command);
/* MTC2/MFC2/CTC2/CFC2 with the register side effects the hardware has. */
void GteWriteData(int reg, int32_t value);
int32_t GteReadData(int reg);
void GteWriteControl(int reg, int32_t value);
int32_t GteReadControl(int reg);

/* The library's read-only tables (gte_tables.c, from the retail executable). */
extern const unsigned int gteTableBase, gteTableEnd;
extern const unsigned char gteTables[];
int GteTable16(unsigned int address);

/* Full-precision projections of this frame. Every RTPS/RTPT stores the exact
   screen position (before the hardware's rounding to whole pixels) and the
   view depth of each vertex, under the integer screen position the game
   received. The renderer resolves a primitive's vertices here together (see
   gte.c); a vertex not found did not come from a projection (2D arithmetic)
   and is drawn as given. */
typedef struct GtePrecise {
    float x, y;  /* screen position, in the game's 320x240 units */
    float z;     /* view-space depth (for perspective-correct texturing) */
} GtePrecise;

#define GTE_RESOLVE_MAX 4
/* The vertices xy[0..count-1] (GPU packet words) of one primitive: fills
   out[i] for each vertex found and returns the mask of those. */
unsigned GteResolvePrecise(const uint32_t *xy, int count, GtePrecise *out);
/* A projection made outside the GTE (the port's far view): packet word sxy,
   precise screen position and depth. */
void GteRecordProjection(uint32_t sxy, float x, float y, float z);
/* A size x size grid of points the game placed on screen by halving between
   four projected corners (corner[]: the packet words of grid points 0,
   size-1, size*(size-1) and size*size-1; x[], y[]: the grid row by row, as
   the game computed it). Records each point at the matching place between
   the corners' precise positions, without a depth (drawn affine, as the
   console draws it). */
void GteRecordGrid(const uint32_t corner[4], const int *x, const int *y, int size);
void GteClearPrecise(void);
/* libgpu has drawn a frame's ordering table: the next projection starts a
   new frame's cache. */
void GteFrameDrawn(void);
/* When false, no precise positions are recorded (the original look). */
extern int gtePrecision;

#endif
