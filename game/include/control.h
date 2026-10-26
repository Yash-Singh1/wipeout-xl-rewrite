#ifndef CONTROL_H
#define CONTROL_H

/* ControlInput button bits after the configurable bindings are applied. */
#define CONTROL_SELECT           0x0001
#define CONTROL_ANALOG_THRUST    0x0002
#define CONTROL_ANALOG_STEERING  0x0004
#define CONTROL_START            0x0008
#define CONTROL_UP               0x0010
#define CONTROL_RIGHT            0x0020
#define CONTROL_DOWN             0x0040
#define CONTROL_LEFT             0x0080
#define CONTROL_LEFT_BRAKE       0x0400
#define CONTROL_RIGHT_BRAKE      0x0800
#define CONTROL_VIEW             0x1000
#define CONTROL_FIRE             0x2000
#define CONTROL_THRUST           0x4000
#define CONTROL_DITCH            0x8000

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
