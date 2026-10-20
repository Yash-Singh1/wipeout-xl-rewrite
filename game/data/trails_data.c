/* Global data of trails.c, generated from the executable by tools/gendata.py. */
#include "functions.h"
#include "trails.h"

int flashNozzleShip = -1;
u_char plasmaShotBusy = 0;
u_char targetBank = 0;
int targetSpin = 0;
short lastTargetShip = -1;
u_char targetvalid = 0;
u_char targetFlags[8];
int plasmaPhaseA;
int plasmaPhaseB;
int plasmaPhaseC;
Trail *shipTrails;
POLY_G3 muzzletop[2][2];
POLY_G4 flare[19][12][2];
POLY_G3 muzzleleft[2][2];
Trail Plasma[4];
POLY_G3 muzzleright[2][2];
DR_MODE muzzletrans[2];
LINE_F2 targetPrims[8][4][2];
DR_MODE targetDrawModes[2];
POLY_G3 muzzlebot[2][2];
