#ifndef OPTIONS_H
#define OPTIONS_H

#include "psyq.h"

/* Background box and its semi-transparent blend mode, one pair per screen. */
struct SelectBox {
    POLY_F4 box[2];
    DR_MODE trans[2];
};

/* Pause menu state: the highlighted entry, a frame counter and the flags the caller reads. */
typedef struct PauseState {
    int           choice;
    int           cdPause;
    unsigned char pad08[8];
    unsigned int counter;
    unsigned char pad14[3];
    unsigned char machinePaused;
    unsigned char confirmed;
    unsigned char selection;
} PauseState;

/* Polygons of the pause menu: the sliding box, its tiles and the two volume bars. */
typedef struct OptionsPolys {
    unsigned char pad0[232];
    POLY_G4       box[2];
    TILE_1        tile[2];
    POLY_G4       musicBars[20][2];
    POLY_G4       sfxBars[20][2];
} OptionsPolys;

/* A menu entry's text, also copied as words. */
typedef union MenuText {
    int align[4];
    char text[16];
} MenuText;

#endif
