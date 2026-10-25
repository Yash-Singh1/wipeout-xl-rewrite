#ifndef TRACKASM_H
#define TRACKASM_H

#include "wipeout.h"

/* One tile as stored in a Surface. */
typedef struct Tile {
    u_short tsb;
    u_short cba;
    u_char  u0, v0;
    u_char  u1, v1;
    u_char  u2, v2;
    u_char  u3, v3;
} Tile;

/* Tiles of one texture at each level of detail: 1 low, 4 medium, 16 high. */
typedef struct Surface {
    Tile low[1];
    Tile med[4];
    Tile high[16];
} Surface;

/* The polygon pools of the three tile levels, one per screen buffer (indexed by CurrentScreen). */
extern POLY_FT4 *TrackLoPolys[2];
extern POLY_FT4 *TrackMedPolys[2];
extern POLY_FT4 *TrackHiPolys[2];
/* Port: the pools of this screen buffer named as the track's packets (PortTrackPackets). */
void PortTrackPools(void);

#endif
