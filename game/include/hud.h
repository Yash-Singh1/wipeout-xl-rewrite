#ifndef HUD_H
#define HUD_H

#include "functions.h"

/* One glyph of a bitmap font: its VRAM origin and advance width. */
typedef struct FontGlyph {
    u_short vramX;
    u_short vramY;
    u_short width;
} FontGlyph;

/* The eight quads of the speedo: frame, speed bar, shield bar and thrust marker, one per screen. */
typedef struct Speedo {
    POLY_FT4 frame[2];
    POLY_FT4 speed[2];
    POLY_FT4 shield[2];
    POLY_FT4 marker[2];
} Speedo;

/* One checkpoint of the race: the track section it sits on, the time allowance,
   and whether the player has reached it. The array is indexed directly. */
struct CheckPoints {
    short       section;
    short       time;
    signed char reached;
    char        padding;
};

/* The in-game menu polygons. Each tile is a Gouraud quad. */
typedef struct InGameMenu {
    u_char  pad0[0xE8];
    POLY_G4 footer[2];
    u_char  textureWindow[2][12];
    POLY_G4 red[20][2];
    POLY_G4 green[20][2];
} InGameMenu;


/* Ship records are ShipData; the HUD and the race-start code use the same layout under this name. */
typedef ShipData RaceShip;

#endif
