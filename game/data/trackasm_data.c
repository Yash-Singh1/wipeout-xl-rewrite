/* Global data of trackasm.c, generated from the executable by tools/gendata.py. */
#include "functions.h"
#include "trackasm.h"

/* Port: one polygon pool per screen: AsmTrackHi picks TrackHiPolys[screen]; trackasm.c calls the second TrackHiPolysAlt */
POLY_FT4 *TrackHiPolys[2];
/* Port: one polygon pool per screen (see TrackHiPolys) */
POLY_FT4 *TrackLoPolys[2];
void *frameValues;
/* Port: one polygon pool per screen (see TrackHiPolys) */
POLY_FT4 *TrackMedPolys[2];
/* Port: indexed by the face texture number (crack.c, trackasm.c, AsmLowResTile...): 24 textures of 2 surfaces, 252 bytes each; the object renderers keep their projected vertices in the 3584 bytes that follow, up to muzzleright (no face of any track uses a texture above 14) */
Surface tileSurfaces[24][2];
