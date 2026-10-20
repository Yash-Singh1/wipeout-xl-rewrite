/* Global data of racectrl.c, generated from the executable by tools/gendata.py. */
#include "functions.h"
#include "hud.h"
#include "pad.h"

/* Types that racectrl.c declares privately. */
typedef unsigned char u_char_eq1;
typedef unsigned short u_short_eq1;
typedef struct TrackSection_eq1 { unsigned char pad0[16]; int centerY; unsigned char pad14[132]; unsigned short sectionNo; } TrackSection_eq1;
typedef struct { unsigned char pad0[20]; int x,y,z; unsigned char pad20[32]; short dirty; unsigned char pad42[2]; void *parent; } Skeleton_eq1;
typedef struct { unsigned char pad0[48]; Skeleton_eq1 *skeleton; } Object_eq1;
typedef struct { TrackSection_eq1 *section; int word[5]; } CameraSections;
typedef struct { unsigned char pad0[0x9000]; short count,draw,pad9004,mode,active; unsigned char tail[34]; } TrackCache;

int linkFlashFrame;
int weaponGrids;
int raceElapsed;
Object *startDroidObjects[3];
u8 remoteRaceDone;
void *girderObjects[3];
TIMlist *residentTextureSeq;
int shadowData;
u_short shipClassIndex;
int (*prevCameraUpdate)();
u_short_eq1 sceneTextureCount;
int savedUpperY;
u_short savedTextureCount;
HeapBlock *mainheapcopy;
u_char gameType;
CVECTOR smokeColour;
TrackCache *trackCache;
HeapBlock *reserveHeap;
int raceStartVcount;
int quadIndex;
int raceEndTimer;
Object *zeppelinObj[1];
PadPacket padBuf1;
u_short_eq1 tempTextureCount;
int savedHeap;
Object *radar[6];
Object *neonObject[12];
CheckCell checkCells[8][4][6];
POLY_FT4 weaponIcons[2];
Object *torchLight[20];
u_char_eq1 scoreTabData[36];
u_char hudDisabled;
Skeleton *residentSkeletons[1];
CameraSections cameraSections;
VECTOR posMonoRail[8];
u_char_eq1 rescueData[72];
int (*rescueUpdateFunc)();
Speedo speedoHud;
Object *advert[10];
u_char weaponInitArg[80];
Object *lightGroups[3];
int zeppelinBaseX;
int zeppelinBaseY;
int zeppelinBaseZ;
void *pylons[17];
Object *redBull[20];
Object *cameraDroids[25];
void *videoObjects[10];
volatile int pauseState[5];
u_char_eq1 twoPlayerPauseStatus;
signed char twoPlayerMode;
signed char twoPlayerFlag;
Object *monoRailObjs[6];
Object_eq1 *prmObjects[64];
Texture *weaponIconTextureTable[20];
Object *fans[5];
POLY_FT4 weaponIconPrims[2];
char scenePath[64];
volatile short monoRailFlag[10];
volatile int trainMuzzleX;
volatile int trainMuzzleY;
volatile int trainMuzzleZ;
Object *smokeFastObjects[10];
Object *smokeSlowObjects[10];
