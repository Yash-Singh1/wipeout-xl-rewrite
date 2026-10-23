/* Global data of collide.c, generated from the executable by tools/gendata.py. */
#include "functions.h"

/* Types that collide.c declares privately. */
typedef struct { short y[4]; } __attribute__((packed)) HudTextRows;

int collideMinAlpha = 0;
u_char classLaps[4] = { 2, 3, 4, 5 };
int trackNameOffset = -9999;
int energyBlinkFrame = 0;
/* The lap-time rows of each class on NTSC, and the European program's on PAL (UpdateRaceHud reads classRowY[portPal]). */
HudTextRows classRowY[2] = { { { 180, 170, 160, 150 } }, { { 198, 186, 174, 162 } } };
int hudFrame = 0;
char fmtNumber[4] = "%d";
char msgP[4] = "P";
char msgLap[] = "LAP";
char msgOf[4] = "OF";
char msgCheck[8] = "CHECK";
char msgPos[] = "POS";
char extCmp[8] = ".cmp";
