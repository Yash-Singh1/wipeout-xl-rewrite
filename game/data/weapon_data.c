/* Global data of weapon.c, generated from the executable by tools/gendata.py. */
#include "functions.h"
#include "../../../wipeout-xl-ps1-data/game/include/control.h"
#include "../../../wipeout-xl-ps1-data/game/include/functions.h"
#include "../../../wipeout-xl-ps1-data/game/include/psyq.h"
#include "../../../wipeout-xl-ps1-data/game/include/wipeout.h"

/* Types that weapon.c declares privately. */
typedef struct PathSeed {
    char text[22];
} PathSeed;
typedef struct SpeedDividers { int value[4]; } SpeedDividers;

char msgWeaponTableFull[24] = "weapon table full!!\n";
char drFontsFile[36] = "c:\\wipeout2\\textures\\drfonts.cmp";
SpeedDividers speedDividerTable = { { 125, 167, 209, 250 } };
SpeedDividers speedDividerTableAlt = { { 990, 1370, 1800, 2100 } };
char msgLapTimes[12] = "LAP TIMES";
char msgAutopilot[12] = "AUTOPILOT";
char msgDisengaging[] = "DISENGAGING";
char msgRearLock[12] = "REAR LOCK";
char msgPerfectLap[] = "PERFECT LAP";
char fmtCdTrack[24] = "CD TRACK: %d   %s [%s]";
char msgWrongWay[12] = "WRONG WAY";
PathSeed texturePathSeed[1] = { { "c:\\wipeout2\\textures\\" } };
char msgBadLuck[12] = "BAD LUCK";
char msgOutOfTime[] = "YOU RAN OUT OF TIME";
char msgShipDestroyed[] = "YOUR SHIP WAS DESTROYED";
char msgRaceStats[] = "RACE STATISTICS";
char msgCongratulations[] = "CONGRATULATIONS";
char msgAwarded[12] = " AWARDED";
char msgBeatPlayerTwo[] = "BUT YOU BEAT PLAYER TWO";
char msgBeatPlayerOne[] = "BUT YOU BEAT PLAYER ONE";
char msgNoMedal[20] = "NO MEDAL AWARDED";
char msgRacePosition[16] = "RACE POSITION";
char msgEliminations[16] = "ELIMINATIONS";
char msgYouBeatPlayerTwo[] = "YOU BEAT PLAYER TWO";
char msgYouBeatPlayerOne[] = "YOU BEAT PLAYER ONE";
char msgRaceTime[12] = "RACE TIME";
char msgContinue[12] = "CONTINUE";
char msgExtendedTime[16] = "EXTENDED TIME";
char trackPathTable[12] = "\\track.wad";
char track1SmokeTextureSequence[32] = "c:\\wipeout2\\textures\\smoke.cmp";
char sharedTrackTextureSequence[36] = "c:\\wipeout2\\textures\\alphared.cmp";
char track6TextureSequence[36] = "c:\\wipeout2\\textures\\zetared.cmp";
char track20TextureSequence[36] = "c:\\wipeout2\\textures\\upsilred.cmp";
char track17TextureSequence[] = "c:\\wipeout2\\textures\\rhored.cmp";
char track7TextureSequence[] = "c:\\wipeout2\\textures\\etared.cmp";
char track13TorchTextureSequence[32] = "c:\\wipeout2\\textures\\fire.cmp";
char track13TextureSequence[32] = "c:\\wipeout2\\textures\\nured.cmp";
char track2ZeppelinNames[12] = "zeppelin";
char track2TextureSequence[36] = "c:\\wipeout2\\textures\\betared.cmp";
u_char msgOvertake[] = "CRITICAL SHIELD FAILURE";
char startWadPath[] = "c:\\wipeout2\\common\\startwad.wad";
u_char msgCombatCable[16] = "COMBAT CABLE";
u_char rearLockCycle = 0;
u_char perfectLap = 0;
u_char perfectLapCount = 0;
short missileLockTimer = 0;
u_short minigunAmmo = 0;
s32 seedWep = 0x12345678;
short fadeAmount = 0;
u_char minigunFrame = 0;
int olddam = 0;
u_char fadeActive;
WeaponData *weaponIndex;
ConfigData *tempGameData;
CameraUpdateFunc CameraUpdate;
WeaponShape *weaponBars;
short fadeBlue;
int ambientVoice2;
short fadeGreen;
short fadeRed;
short remoteRacePos;
short playerPosition;
SParticle *particleSlots;
int fracdamage[15];
int shipDamage[16];
DR_MODE fadedrm[9];
DR_MODE shieldParams[192][2];
POLY_G4 plasmacharge[2];
int shieldRed[15];
TILE fadeTile[2];
int shieldGreen[15];
