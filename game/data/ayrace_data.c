/* Global data of ayrace.c, generated from the executable by tools/gendata.py. */
#include "functions.h"
#include "../../../wipeout-xl-ps1-data/game/include/control.h"
#include "../../../wipeout-xl-ps1-data/game/include/psyq.h"
#include "../../../wipeout-xl-ps1-data/game/include/wipeout.h"
#include "ghost.h"
#include "hiscores.h"
#include "weapon.h"

char selectLabel[8] = "SELECT";
int attractModeFlag = 0;
int firstTimeIn = 0;
int playCredits = 0;
u_char trackMedals[8] = { 0 };
short trackCycleTimer = 0;
int skycyc = 0;
short beamCollide = 0;
s16 raceFlag = 0;
int lightUpdate = 0;
int oneMoreTime = 0;
int masterWait = 0;
short advertCount = 0;
short cameraCount = 0;
short neonCount = 0;
short girderCount = 0;
short videoCount = 0;
short zepCount = 0;
short dishCount = 0;
short pylonCount = 0;
short fanCount = 0;
short smokeSlowCount = 0;
short smokeFastCount = 0;
short smokeNumber = 0;
int fastCount = 0;
int fastTexCount = 0;
int slowCount = 0;
int slowTexCount = 0;
short torchCount = 0;
short torchNumber = 0;
short redBullCount = 0;
short redBullNumber = 0;
int bobCount = 0;
u_short prmCount = 0;
u_short startTextureBase = 0;
char startDroidNames[8] = "sroid";
char lightGroupNames[8] = "light";
char cameraDroidNames[8] = "camera";
char fanNames[] = "fan";
u_char raceEndState;
TrackData *track;
u_char raceCompleteFlag;
u_short padType;
int cdMusicTrack;
Texture *continuesTexture;
u_short idleTimer;
u_short keysPressed;
short droneSwitchStep;
ShipData *shipIndex;
u_char linkRaceDone;
u_char linkSyncB;
HiScoreData scoreTabData;
GhostReplay recordBuffer[1];
ControlInput playerControl[1];
