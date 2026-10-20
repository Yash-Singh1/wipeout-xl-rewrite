#ifndef CONTROL_H
#define CONTROL_H

#include "wipeout.h"

/* Controller state as the ship update code reads it (one per player / ghost / remote). */
typedef struct ControlInput {
    u_short head;
    u_short buttons;
    u_short pressed;
    u_char  pad6[2];
    u_char  thrust;
    u_char  steering;
} ControlInput;

/* Steering dead band per screen mode. */
typedef struct BrakeModes {
    short value[4];
} BrakeModes;

/* Checkpoint bonus cell, indexed by track, ship class and cell number. */
typedef struct CheckCell {
    short section;
    short bonusSeconds;
    char  done;
} CheckCell;

#endif
