#ifndef RACEINIT_H
#define RACEINIT_H

#include "hud.h"

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
