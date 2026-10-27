/* Headphone surround: the SPU's output with the sounds behind the player (spu.h's rear) placed behind the listener,
   for headphones. */
#ifndef PSX_SURROUND_H
#define PSX_SURROUND_H

#include <stdint.h>

/* One output sample from the front mix and the rear one (each stereo); out may be front. */
void SurroundRender(const int16_t front[2], const int16_t rear[2], int16_t out[2]);
/* Forgets the filters' and delays' past (when it is switched on). */
void SurroundReset(void);

#endif
