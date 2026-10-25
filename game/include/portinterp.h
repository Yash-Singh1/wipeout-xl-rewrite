/* Port: frame interpolation (platform/psx/gte.h) of what the game draws in 2D: the corners of a billboard, a ring or a
   flare placed about a projected point, and points kept from the frame before. Each is recorded as a projection known
   by a key, so the pictures drawn between two of the game's frames move it from where the frame before had it,
   instead of leaving it where this frame has it (a step at the game's rate). They are recorded without a depth: the
   game's 2D is drawn as before (affine, outside the depth buffer). */
#ifndef PORTINTERP_H
#define PORTINTERP_H

#include <stdint.h>

/* The projection libgte stored at sxy (an SVECTOR's or DVECTOR's x and y, still holding it), or 0. */
uint32_t PortProjection(const void *sxy);
/* A packet's xy word placed in 2D about a projection: (x, y) is where the game took the projection to be, the word
   holds the corner. Known by the projection's key and `which` (the same corner of the same thing every frame). Left
   as it is when the projection has no key or no view-space position. */
void PortBindAbout(void *corner, uint32_t centre, int x, int y, uint64_t which);
/* A packet's xy word that is a point of the screen with nothing projected behind it (a point the frame before
   projected, kept), known by `key`: between frames it goes in a straight line on the screen. */
void PortBindScreen(void *corner, uint64_t key);

/* sparks.c: the weapon whose missile or rocket flare is drawn next. */
void PortFlareOwner(const void *weapon);

#endif
