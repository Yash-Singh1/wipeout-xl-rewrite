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
   view depth of each vertex, keyed by the integer screen position the game
   received. The renderer looks a primitive's vertices up here; a miss means
   the vertex did not come from a projection (2D) and is drawn as given. */
typedef struct GtePrecise {
    float x, y;  /* screen position, in the game's 320x240 units */
    float z;     /* view-space depth (for perspective-correct texturing) */
} GtePrecise;

int GteLookupPrecise(int x, int y, GtePrecise *precise);
void GteClearPrecise(void);
/* libgpu has drawn a frame's ordering table: the next projection starts a
   new frame's cache. */
void GteFrameDrawn(void);
/* When false, no precise positions are recorded (the original look). */
extern int gtePrecision;

#endif
