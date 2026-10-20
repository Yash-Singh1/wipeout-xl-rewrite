#ifndef SPARKS_H
#define SPARKS_H

/* One entry of the particle table (128 entries) shared by sparks, smoke, rain and snow. */
struct SParticle {
    char    type;           /* 0 = free */
    char    flags;
    SVector vel;
    VECTOR  pos;            /* pos.pad holds the particle's last screen position (a packed x, y) */
    int     life;
    LINE_G2 line[2];        /* the streak drawn for it, one packet per screen buffer */
};

#define PARTICLE_COUNT 128

/* A billboard explosion (table of 32). */
typedef struct Explosion {
    int   x, y, z;
    u_char size;            /* sprite size is halved per step */
    u_char r, g, b;
    short  flash;           /* 1: plays the flash frames instead of the fireball */
    short  blend;           /* semi-transparency mode of the sprite */
    short  life;            /* free when <= 0 */
    short  ship;            /* position is relative to this ship when >= 0 */
} Explosion;

#define EXPLOSION_COUNT 32

/* An expanding shock ring (table of 8). */
typedef struct Ring {
    int  x, y, z;
    int  padC;
    int  pad10;
    short life;
    short pad16;
} Ring;

#define RING_COUNT 8

/* One bird of the flock (table of 32). */
typedef struct Bird {
    int x, y, z;
    int vx, vy, vz;
    int frame;
    int accel;
} Bird;

#define BIRD_COUNT 32

/* An electric bolt (table of 32): a start and an end point, then three line segments,
   each double buffered. */
typedef struct Electric {
    VECTOR  start;
    VECTOR  end;
    int    life;           /* -1 when unused */
    LINE_G2 line1[2];
    LINE_G2 line2[2];
    LINE_G2 line3[2];
} Electric;

#define ELECTRIC_COUNT 32

#endif
