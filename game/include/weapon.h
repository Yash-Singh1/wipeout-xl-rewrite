#ifndef WEAPON_H
#define WEAPON_H

#define WEAPON_NONE            0
#define WEAPON_ROCKET          1
#define WEAPON_MISSILE         2
#define WEAPON_MINE            3
#define WEAPON_ELECTROBOLT     5
#define WEAPON_ENERGY          6
#define WEAPON_AUTOPILOT       7
#define WEAPON_SHIELD          8
#define WEAPON_TURBO           9
#define WEAPON_ROCKET_LEFT     10
#define WEAPON_ROCKET_RIGHT    11
#define WEAPON_QUAKE           12
#define WEAPON_PLASMA          13
#define WEAPON_SMART_BOMB      14
#define WEAPON_MINIGUN         15

#include "wipeout.h"

/* One entry of the weapon shape table: the object used to draw a weapon, and whether it is in use. */
struct WeaponShape {
    Object *shapePtr;
    short   used;
    short   pad;
};

/* One weapon in flight (0x48 bytes). The weapons array holds 30 of them.
   count: frames left while flying; -1 = hit (ring + explosion), -2 = expired, 0 = free.
   type:  1 rocket, 2 missile, 3 mine, 5 electric bolt, 0xF minigun.
   target: ship index this weapon has hit or is locked on. */
struct WeaponData {
    short        count;          /* 0x00 */
    short        quakeBumpStep;          /* 0x02: bump step counter used by the quake weapon */
    short        shapeNo;        /* 0x04: index into the WeaponShape table */
    short        pad06;          /* 0x06 */
    TrackSection *section;       /* 0x08: track section the weapon is over */
    VECTOR       accel;          /* 0x0C */
    VECTOR       velocity;       /* 0x1C */
    VECTOR       pos;            /* 0x2C */
    short        hdg;            /* 0x3C */
    short        pitch;          /* 0x3E */
    signed char  target;         /* 0x40 */
    unsigned char owner;         /* 0x41: index of the firing ship */
    unsigned char type;          /* 0x42 */
    unsigned char pad43;         /* 0x43 */
    void        (*update)(ShipData *ships, struct WeaponShape *shapes, struct WeaponData *weapon, struct WeaponData *weapons); /* 0x44 */
};


/* One weapon grid zone (16 bytes): the same entry as WeaponGrid in dynam.h, with the fields the
   grid animation uses. count: the zone is armed (>= 0) or counting down a hit. */
struct WeaponGridZone {
    TrackSection *section;
    u_short       attr;
    short         angle;
    int          count;
    u_char        r, g, b, pad;
};

/* A Gouraud-shaded triangle and quad as written into the primitive buffer (28 and 36 bytes). */
typedef struct ShadedTri {
    unsigned int tag;
    CVECTOR       colour0;
    DVECTOR       xy0;
    CVECTOR       colour1;
    DVECTOR       xy1;
    CVECTOR       colour2;
    DVECTOR       xy2;
} ShadedTri;

typedef struct ShadedQuad {
    unsigned int tag;
    CVECTOR       colour0;
    DVECTOR       xy0;
    CVECTOR       colour1;
    DVECTOR       xy1;
    CVECTOR       colour2;
    DVECTOR       xy2;
    CVECTOR       colour3;
    DVECTOR       xy3;
} ShadedQuad;

#endif
