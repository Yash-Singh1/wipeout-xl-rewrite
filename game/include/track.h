#ifndef TRACK_H
#define TRACK_H

#include "psyq.h"

/* Automesh output: the automeshed polygons, then the quad and tri counters. */
struct AutoMeshData {
    char  primitives[0x9000];
    short quadCount;
    short triCount;
    short attemptedAutos;
    short mode;             /* race() clears it; nothing reads it */
    short active;           /* race() sets it to 1; nothing reads it */
    char  reserved[34];     /* the console allocated 0x902C bytes for the whole record */
};

/* Per-face colour copy, kept beside the track faces. */
typedef struct Colors {
    u_char red;
    u_char green;
    u_char blue;
} Colors;

#endif
