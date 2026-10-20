/* Global data of iso9660.c, generated from the executable by tools/gendata.py. */
#include "functions.h"

char nameStartBoom[] = "start";
char nameRedLight[] = "red";
int trainLightSeed = 0;
int neonSignTick = 0;
int redBullTick = 0;
int redBullFrame = 0;
int spriteTick = 0;
int spriteFrame = 0;
short lightPhase[2] = { 0 };
/* Port: declared short[4]; effects.c uses [0] and [1] only and trainCmpExt follows at +4 */
short advertAngle[4] = { 0 };
char trainCmpExt[] = ".cmp";
char trainPrmExt[] = ".prm";
int droidBlinkTick = 0;
char nameLight[] = "light";
int lightChaseStep = 0;
int lightUpdateTick = 0;
short monoRailSettle = 0;
int zeppelinDrift = 0;
short radarAngle = 0;
int shakeAmount = 0;
int serialResult = 0;
u_int shakeCount = 0;
int writeFlag = 0;
int combError = 1;
int combErrorSeen = 1;
char sioDevice[] = "sio:";
int cached_dir_loc = 0;
int cached_dir_start_loc = 0;
int cached_dir_file_len = 0;
int cache_size = 0;
int num_of_open_files = 0;
int track1_start_sec = 0;
int initIso = 0;
int cdReadEvent;
HeapBlock *isoHeap;
int pathTableSize;
int cdErrorEvent;
u_char *pathTable;
IsoFile fd_tab[16];
u_char toc_loc[400];
u_char dirCache[8192];
