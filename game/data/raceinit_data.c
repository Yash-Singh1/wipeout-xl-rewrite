/* Global data of raceinit.c, generated from the executable by tools/gendata.py. */
#include "functions.h"
#include "dynam.h"

int trackNameScroll = -9999;
int raceInfoFrame = 0;
char classNameVector[7] __attribute__((aligned(4))) = "VECTOR";
char classNameVenom[6] __attribute__((aligned(4))) = "VENOM";
char classNameRapier[7] __attribute__((aligned(4))) = "RAPIER";
char classNamePhantom[8] __attribute__((aligned(4))) = "PHANTOM";
char sufGold[8] = " GOLD";
char sufSilver[] = " SILVER";
char sufBronze[] = " BRONZE";
int bonusFlashFrame = 0;
int bonusFrames = 0;
char fmtTwoDigits[8] = "%02d";
char sufCheckPoints[] = "\\cpoint";
char fmtCheckPointClass[8] = "%01d";
char extChk[8] = ".chk";
int attractStep = 1;
WeaponGrid *weaponGrids;
int roadSectionCount;
u_int randSeed;
int weaponGridSections;
s32 linkRaceFlag;
