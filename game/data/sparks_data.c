/* Global data of sparks.c, generated from the executable by tools/gendata.py. */
#include "functions.h"
#include "../../../wipeout-xl-ps1-data/game/include/psyq.h"
#include "../../../wipeout-xl-ps1-data/game/include/wipeout.h"
#include "sparks.h"

u_char track1DescriptionLine2[8] = "74 M";
u_char track2DescriptionLine0[8] = "FRANCE";
u_char track2DescriptionLine1[8] = "3.5 KM";
u_char track2DescriptionLine2[8] = "179 M";
u_char track6DescriptionLine1[8] = "5.1 KM";
u_char track6DescriptionLine2[8] = "97 M";
u_char track7DescriptionLine1[8] = "4.0 KM";
u_char track7DescriptionLine2[8] = "82 M";
u_char track8DescriptionLine0[8] = "NEPAL";
u_char track8DescriptionLine1[8] = "4.3 KM";
u_char track8DescriptionLine2[8] = "158 M";
u_char track13DescriptionLine0[8] = "CHILE";
u_char track13DescriptionLine1[8] = "3.9 KM";
u_char track13DescriptionLine2[8] = "220 M";
u_char track17DescriptionLine1[8] = "4.4 KM";
u_char track17DescriptionLine2[8] = "121 M";
u_char track20DescriptionLine0[] = "GERMANY";
u_char track20DescriptionLine1[8] = "3.8 KM";
u_char track20DescriptionLine2[8] = "137 M";
char singleDigitFormat[4] = "%d";
u8 loadScreenFooter[4] = "TM";
int missflareat = 0;
int pitsparkat = 0;
int rockflareat = 0;
int rockang = 0;
int exdelay = 0;
u8 partpos = 0;
int snowCount = 0;
u_char birdDelay = 0;
TrackSection *birdSectionA;
TrackSection *birdSectionB;
POLY_FT4 *birdPrims;
Bird *birds;
TrackCamera *cameraPtr;
Texture *birdTexture[33];
Ring rings[8];
Explosion explosions[32];
POLY_FT4 explodePrims[32][2];
Electric electrics[32];
POLY_G4 ringPrims[2][16][8];
POLY_G3 rocketFlarePrims[2][4][15];
POLY_G3 missileFlarePrims[2][4][8];
Texture *explodeTexture[32];
