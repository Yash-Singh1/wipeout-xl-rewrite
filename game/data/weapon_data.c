/* Global data of weapon.c, generated from the executable by tools/gendata.py. */
#include "functions.h"
#include "weapon.h"

/* Types that weapon.c declares privately. */
typedef struct SpeedDividers { int value[4]; } SpeedDividers;
typedef unsigned char u_char_eq1;

char msgWeaponTableFull[] = "weapon table full!!\n";
char drFontsFile[] = "c:\\wipeout2\\textures\\drfonts.cmp";
SpeedDividers speedDividerTable = { { 125, 167, 209, 250 } };
SpeedDividers speedDividerTableAlt = { { 990, 1370, 1800, 2100 } };
char msgLapTimes[] = "LAP TIMES";
char msgAutopilot[] = "AUTOPILOT";
char msgDisengaging[] = "DISENGAGING";
char msgRearLock[] = "REAR LOCK";
char msgPerfectLap[] = "PERFECT LAP";
char fmtCdTrack[] = "CD TRACK: %d   %s [%s]";
char msgWrongWay[] = "WRONG WAY";
int texturePathSeed = 0x775c3a63;
char msgBadLuck[] = "BAD LUCK";
char msgOutOfTime[] = "YOU RAN OUT OF TIME";
char msgShipDestroyed[] = "YOUR SHIP WAS DESTROYED";
char msgRaceStats[] = "RACE STATISTICS";
char msgCongratulations[] = "CONGRATULATIONS";
char msgAwarded[] = " AWARDED";
char msgBeatPlayerTwo[] = "BUT YOU BEAT PLAYER TWO";
char msgBeatPlayerOne[] = "BUT YOU BEAT PLAYER ONE";
char msgNoMedal[] = "NO MEDAL AWARDED";
char msgRacePosition[] = "RACE POSITION";
char msgEliminations[] = "ELIMINATIONS";
char msgYouBeatPlayerTwo[] = "YOU BEAT PLAYER TWO";
char msgYouBeatPlayerOne[] = "YOU BEAT PLAYER ONE";
char msgRaceTime[] = "RACE TIME";
char msgContinue[] = "CONTINUE";
char msgExtendedTime[] = "EXTENDED TIME";
u_char_eq1 trackPathTable[12] = { 92, 116, 114, 97, 99, 107, 46, 119, 97, 100, 0, 0 };
u_char_eq1 track1SmokeTextureSequence[32] = {
    99, 58, 92, 119, 105, 112, 101, 111, 117, 116, 50, 92, 116, 101, 120, 116, 117, 114, 101, 115,
    92, 115, 109, 111, 107, 101, 46, 99, 109, 112, 0, 0,
};
u_char_eq1 sharedTrackTextureSequence[36] = {
    99, 58, 92, 119, 105, 112, 101, 111, 117, 116, 50, 92, 116, 101, 120, 116, 117, 114, 101, 115,
    92, 97, 108, 112, 104, 97, 114, 101, 100, 46, 99, 109, 112, 0, 0, 0,
};
u_char_eq1 track6TextureSequence[36] = {
    99, 58, 92, 119, 105, 112, 101, 111, 117, 116, 50, 92, 116, 101, 120, 116, 117, 114, 101, 115,
    92, 122, 101, 116, 97, 114, 101, 100, 46, 99, 109, 112, 0, 0, 0, 0,
};
u_char_eq1 track20TextureSequence[36] = {
    99, 58, 92, 119, 105, 112, 101, 111, 117, 116, 50, 92, 116, 101, 120, 116, 117, 114, 101, 115,
    92, 117, 112, 115, 105, 108, 114, 101, 100, 46, 99, 109, 112, 0, 0, 0,
};
u_char_eq1 track17TextureSequence[32] = {
    99, 58, 92, 119, 105, 112, 101, 111, 117, 116, 50, 92, 116, 101, 120, 116, 117, 114, 101, 115,
    92, 114, 104, 111, 114, 101, 100, 46, 99, 109, 112, 0,
};
u_char_eq1 track7TextureSequence[32] = {
    99, 58, 92, 119, 105, 112, 101, 111, 117, 116, 50, 92, 116, 101, 120, 116, 117, 114, 101, 115,
    92, 101, 116, 97, 114, 101, 100, 46, 99, 109, 112, 0,
};
u_char_eq1 track13TorchTextureSequence[32] = {
    99, 58, 92, 119, 105, 112, 101, 111, 117, 116, 50, 92, 116, 101, 120, 116, 117, 114, 101, 115,
    92, 102, 105, 114, 101, 46, 99, 109, 112, 0, 0, 0,
};
u_char_eq1 track13TextureSequence[32] = {
    99, 58, 92, 119, 105, 112, 101, 111, 117, 116, 50, 92, 116, 101, 120, 116, 117, 114, 101, 115,
    92, 110, 117, 114, 101, 100, 46, 99, 109, 112, 0, 0,
};
u_char_eq1 track2ZeppelinNames[12] = { 122, 101, 112, 112, 101, 108, 105, 110, 0, 0, 0, 0 };
u_char_eq1 track2TextureSequence[36] = {
    99, 58, 92, 119, 105, 112, 101, 111, 117, 116, 50, 92, 116, 101, 120, 116, 117, 114, 101, 115,
    92, 98, 101, 116, 97, 114, 101, 100, 46, 99, 109, 112, 0, 0, 0, 0,
};
u_char_eq1 msgOvertake[24] = {
    67, 82, 73, 84, 73, 67, 65, 76, 32, 83, 72, 73, 69, 76, 68, 32, 70, 65, 73, 76, 85, 82, 69, 0,
};
char startWadPath[] = "c:\\wipeout2\\common\\startwad.wad";
u_char msgCombatCable[16] = { 67, 79, 77, 66, 65, 84, 32, 67, 65, 66, 76, 69, 0, 0, 0, 0 };
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
s16 remoteRacePos;
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
