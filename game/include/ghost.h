#ifndef GHOST_H
#define GHOST_H

#include "wipeout.h"

/* Written into the record's magic word when a take has been recorded. */
#define GHOST_MAGIC 0x3039

/* A take is a header (attract seed at 0, then the key counts, control type, flags, track,
 * class and a copy of the player's ship at GHOST_SHIP_AT) and then the keypress halfwords
 * at GHOST_KEYS_AT. The header words are separate globals (see ghost.c). */
#define GHOST_SHIP_AT 0x14
#define GHOST_KEYS_AT 0x104

/* Keypress slots: a take holds at most this many halfwords of input. */
#define GHOST_KEYS 0x3138

typedef struct GhostReplay {
    u8  header[GHOST_KEYS_AT];
    u16 keys[GHOST_KEYS];
} GhostReplay;

/* The attract seed (first word of a take), the ship copy in its header and keypress n. */
#define GHOST_SEED(take) (*(s32 *)(take)->header)
#define GHOST_SHIP(take) ((ShipData *)((take)->header + GHOST_SHIP_AT))
#define GHOST_KEY(take, n) (*(u16 *)((u8 *)(take) + GHOST_KEYS_AT + (n) * 2))

/* The take being recorded during a race, and the take the next race plays back. */
extern GhostReplay recordBuffer[];
extern GhostReplay replayBuffer[];

#endif
