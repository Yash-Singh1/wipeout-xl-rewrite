/* Global data of trackasm.c, generated from the executable by tools/gendata.py. */
#include "functions.h"
#include "../../../wipeout-xl-ps1-data/game/include/psyq.h"
#include "../../../wipeout-xl-ps1-data/game/include/trackasm.h"

/* Port: one polygon pool per screen: AsmTrackHi picks TrackHiPolys[screen]; trackasm.c calls the second TrackHiPolysAlt */
POLY_FT4 *TrackHiPolys[2];
/* Port: one polygon pool per screen (see TrackHiPolys) */
POLY_FT4 *TrackLoPolys[2];
void *frameValues;
/* Port: one polygon pool per screen (see TrackHiPolys) */
POLY_FT4 *TrackMedPolys[2];
/* Port: indexed by the face texture number (crack.c, trackasm.c, AsmLowResTile...): the executable has 64 of the 252-byte surfaces (32 textures, 2 each) up to muzzleright; the sources declared 24 */
Surface tileSurfaces[32][2];
