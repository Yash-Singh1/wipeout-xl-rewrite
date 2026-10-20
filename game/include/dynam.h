#ifndef DYNAM_H
#define DYNAM_H

#include "wipeout.h"

/* A weapon grid entry: one track section where a weapon zone is active. */
struct WeaponGrid {
    TrackSection *section;
    u_short       attr;
    short         angle;
    int          count;
    int          colour; /* Packed RGB bytes. */
};

/* A special (pickup) grid entry: the section number and the count of that zone. */
struct SpecialGrid {
    short section;
    short count;
    char  attr;
    char  pad5;
};

#endif
