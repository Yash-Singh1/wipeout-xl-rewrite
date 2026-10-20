/* Global data of sparks.c, generated from the executable by tools/gendata.py. */
#include "functions.h"
#include "sparks.h"

u_char track1DescriptionLine2[8] = { 55, 52, 32, 77, 0, 0, 0, 0 };
u_char track2DescriptionLine0[8] = { 70, 82, 65, 78, 67, 69, 0, 0 };
u_char track2DescriptionLine1[8] = { 51, 46, 53, 32, 75, 77, 0, 0 };
u_char track2DescriptionLine2[8] = { 49, 55, 57, 32, 77, 0, 0, 0 };
u_char track6DescriptionLine1[8] = { 53, 46, 49, 32, 75, 77, 0, 0 };
u_char track6DescriptionLine2[8] = { 57, 55, 32, 77, 0, 0, 0, 0 };
u_char track7DescriptionLine1[8] = { 52, 46, 48, 32, 75, 77, 0, 0 };
u_char track7DescriptionLine2[8] = { 56, 50, 32, 77, 0, 0, 0, 0 };
u_char track8DescriptionLine0[8] = { 78, 69, 80, 65, 76, 0, 0, 0 };
u_char track8DescriptionLine1[8] = { 52, 46, 51, 32, 75, 77, 0, 0 };
u_char track8DescriptionLine2[8] = { 49, 53, 56, 32, 77, 0, 0, 0 };
u_char track13DescriptionLine0[8] = { 67, 72, 73, 76, 69, 0, 0, 0 };
u_char track13DescriptionLine1[8] = { 51, 46, 57, 32, 75, 77, 0, 0 };
u_char track13DescriptionLine2[8] = { 50, 50, 48, 32, 77, 0, 0, 0 };
u_char track17DescriptionLine1[8] = { 52, 46, 52, 32, 75, 77, 0, 0 };
u_char track17DescriptionLine2[8] = { 49, 50, 49, 32, 77, 0, 0, 0 };
u_char track20DescriptionLine0[8] = { 71, 69, 82, 77, 65, 78, 89, 0 };
u_char track20DescriptionLine1[8] = { 51, 46, 56, 32, 75, 77, 0, 0 };
u_char track20DescriptionLine2[8] = { 49, 51, 55, 32, 77, 0, 0, 0 };
char singleDigitFormat[] = "%d";
u8 loadScreenFooter[4] = { 84, 77, 0, 0 };
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
