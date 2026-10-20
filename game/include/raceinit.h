#ifndef RACEINIT_H
#define RACEINIT_H

#include "hud.h"

/* Pause-menu state initialized before a race starts. */
struct RaceData {
    int         selection;
    int         musicPaused;
    int         command;
    int         count;
    int         frameCount;
    signed char pad14;
    signed char active;
    signed char showOptions;
    signed char stage;
    signed char confirmed;
    signed char confirmChoice;
};

/* The pointers that InitRaceResetData records for a restart of the race. */
struct ResetData {
    ShipData   *ships;
    ConfigData *config;
    WeaponData *weapons;
    WeaponShape *weaponShapes;
    RescueData *rescue;
    Object    **shipShapes;
    CheckPoints *checkPoints;
};

#endif
