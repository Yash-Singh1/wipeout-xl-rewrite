#ifndef GHOST_H
#define GHOST_H

#include "wipeout.h"

/* Written into a take's magic field when it has been recorded. */
#define GHOST_MAGIC 0x3039

/* Keypress slots: a take holds at most this many halfwords of input. */
#define GHOST_KEYS 0x3138

/* A take: what the player did in a race, replayed by the ghost ship of the next race on the
   same track and class. The header is the attract-mode seed, the counters and the identity of
   the take; the ship is a copy of the player's ship as it was at the start of the race. */
typedef struct GhostReplay {
    s32      attractSeed;   /* seed of the attract-mode random numbers when the race started */
    s32      keyCount;      /* keypress halfwords recorded (or played back) so far */
    s32      keyTotal;      /* keypress halfwords in the take */
    s16      control;       /* pad type the take was recorded with: 1 digital, 2 analogue */
    s16      disabled;      /* set when the take is not usable (recording was interrupted) */
    s16      magic;         /* GHOST_MAGIC when a take has been recorded */
    u8       trackNum;
    u8       shipClass;
    ShipData ship;
    u16      keys[GHOST_KEYS];
} GhostReplay;

/* The take being recorded during a race, and the take the next race plays back. */
extern GhostReplay recordBuffer[];
extern GhostReplay replayBuffer[];

#endif
