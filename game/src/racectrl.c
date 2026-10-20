#include "include_asm_g8.h"
#include "functions.h"
#include "hud.h"

/* $gp-relative globals are tentative definitions; the rest are extern. */
u_short          startTextureBase;
int              weaponGrids;
void            *upperSkeleton;
TIMlist         *residentTextureSeq;
int              shadowData;
int              weaponIndex;
int              weaponBars;
Texture         *continuesTexture;
ShipData        *shipIndex;
u_short          savedTextureCount;
short            beamCollide;
short            advertCount;
short            cameraCount;
short            zepCount;
short            dishCount;
short            fanCount;
short            smokeSlowCount;
short            smokeFastCount;
short            smokeNumber;
short            torchCount;
short            torchNumber;
short            redBullCount;
short            redBullNumber;
int              linkFlashFrame;

extern char          startWadPath[];
extern u_short       TextureTableCount;
extern char          startDroidNames[];
extern char          lightGroupNames[];
extern Object       *startDroidObjects[];
extern ConfigData   *gameData;
extern WADFILE      *textureWad;
extern CheckCell     checkCells[][4][6];
extern POLY_FT4      weaponIcons[];
extern Object       *shipShapeTable[];
extern Skeleton     *residentSkeletons[];
extern Speedo        speedoHud;
extern u_char        weaponInitArg[];
extern Object       *lightGroups[];
extern Texture      *weaponIconTextureTable[];
extern int           heap;
extern TrackData    *track;
extern int           mainheapcopy;
extern int           reserveHeap;
extern int           fastCount;
extern int           fastTexCount;
extern int           slowCount;
extern int           slowTexCount;
extern int           bobCount;
extern CVECTOR       smokeColour;
extern Object       *zeppelinObj[];
extern Object       *radar[];
extern Object       *neonObject[];
extern Object       *torchLight[];
extern Object       *advert[];
extern Object       *redBull[];
extern Object       *cameraDroids[];
extern Object       *fans[];
extern Object       *smokeFastObjects[];
extern Object       *smokeSlowObjects[];
extern u_char        msgCombatCable[];
extern u_char        msgLinkBroken[];
extern u_char        msgReturningToMenus[];
extern u_char        msgRemoved[];
extern short         linkTimer;
extern int           raceEndTimer;

typedef unsigned char u_char_eq1;
typedef unsigned short u_short_eq1;
typedef unsigned int u_int_eq1;

typedef struct { unsigned char pad0[4]; unsigned char team,trackNo; unsigned char pad6[2]; signed char shipClass; unsigned char pad9[9]; short raceCount,finished; unsigned char pad16[18]; unsigned char weapons; unsigned char pad29[16]; unsigned char venomComplete,rapierComplete; } GameData;
typedef struct TrackSection_eq1 { unsigned char pad0[16]; int centerY; unsigned char pad14[132]; unsigned short sectionNo; } TrackSection_eq1;
typedef struct { int vx,vy,vz,pad; } VECTOR_eq1;
typedef struct { unsigned char pad0[20]; int x,y,z; unsigned char pad20[32]; short dirty; unsigned char pad42[2]; void *parent; } Skeleton_eq1;
typedef struct { unsigned char pad0[48]; Skeleton_eq1 *skeleton; } Object_eq1;
typedef struct { unsigned char pad0[4]; TrackSection_eq1 *nearTrkSect; TrackSection_eq1 *prevShipSect; int attr; VECTOR_eq1 unitVecNose,unitVecWing,unitVecFin; VECTOR_eq1 ppivot,vpivot,apivot; short hdg,pitch,roll,vhdg; unsigned char pad78[52]; union { unsigned int word; volatile unsigned short half; } state; unsigned char padB0[10]; unsigned char weapon; unsigned char padBB[7]; short timer; unsigned char padC4[40]; void (*update)(); } ShipData_eq1;
typedef struct { TrackSection_eq1 *section; int word[5]; } CameraSections;
typedef struct { unsigned char pad0[0x9000]; short count,draw,pad9004,mode,active; unsigned char tail[34]; } TrackCache;
typedef struct { short value; unsigned char pad[4]; } CheckPointShort;
typedef struct { signed char value; unsigned char pad[5]; } CheckPointByte;
typedef struct { unsigned char data[40]; } POLY_FT4_eq1;
typedef struct TrackData_eq1 { int vertexCount, faceCount, sectionCount; void *vertices, *faces, *sections, *viewList, *colCopy; } TrackData_eq1;
typedef struct RaceLocals { TrackData_eq1 trackData; void *clearPolys[2]; u_char_eq1 pad58[16]; u_char_eq1 cdState[80]; u_char_eq1 cdLocations[336]; u_char_eq1 inputPacket[32]; u_char_eq1 inGameMenu[3312]; short frameCount; u_char_eq1 padF1A[2]; int checkpoint; short checkpointLap; short cdTrack; int cdMode; signed char viewMode; u_char_eq1 padF29; short raceComplete; u_char_eq1 padF2C[4]; int scene; u_char_eq1 padF34[4]; int crowdSample; u_char_eq1 padF3C[4]; u_short_eq1 lightCountdown; u_char_eq1 padF42[6]; int needEndText; u_char_eq1 padF4C[4]; int needWinLose; u_char_eq1 padF54[4]; volatile u_char_eq1 firstTime; u_char_eq1 reserved[7]; volatile int divisionMagic; } RaceLocals;
void *BlockFill__eq1() __asm__("BlockFill");
void *DAlloc__eq1() __asm__("DAlloc");
int InitCheckPoints__eq1() __asm__("InitCheckPoints");
int WADInitFaces__eq1() __asm__("WADInitFaces");
int WADInitSections__eq1() __asm__("WADInitSections");
int InitTrackTextures__eq1() __asm__("InitTrackTextures");
int WADInitVertices__eq1() __asm__("WADInitVertices");
int WADInitViewList__eq1() __asm__("WADInitViewList");
int AsmDrawObjectFixed();
int AsmDrawParticles();
int InitDynamicMem__eq1() __asm__("InitDynamicMem");
int SetSkeletonPosition__eq1() __asm__("SetSkeletonPosition");
int SetSkeletonDirection__eq1() __asm__("SetSkeletonDirection");
int DrawNewTrack__eq1() __asm__("DrawNewTrack");
int DrawAllTrack__eq1() __asm__("DrawAllTrack");
int DrawLandscape__eq1() __asm__("DrawLandscape");
void *WADLoadTtfFile__eq1() __asm__("WADLoadTtfFile");
int InitCamera__eq1() __asm__("InitCamera");
int UpdateCameraRaceStart__eq1() __asm__("UpdateCameraRaceStart");
int DrawShips__eq1() __asm__("DrawShips");
int UpdateSharkModel__eq1() __asm__("UpdateSharkModel");
int initShipData__eq1() __asm__("initShipData");
int UpdateShips__eq1() __asm__("UpdateShips");
int SetUpFirstCheckPoint__eq1() __asm__("SetUpFirstCheckPoint");
int newRand__eq1() __asm__("newRand");
int UpdateWeapons__eq1() __asm__("UpdateWeapons");
void MissileLock__eq1(int shipNo, ShipData_eq1 *ships) __asm__("MissileLock");
void RearMissileLock__eq1(int shipNo, ShipData_eq1 *ships) __asm__("RearMissileLock");
int InitialiseBumps__eq1() __asm__("InitialiseBumps");
int CycleWeaponGrids__eq1() __asm__("CycleWeaponGrids");
int CheckPitShips__eq1() __asm__("CheckPitShips");
int CentreText__eq1() __asm__("CentreText");
int UpdateRaceHud__eq1() __asm__("UpdateRaceHud");
int AlterCheckPoint__eq1() __asm__("AlterCheckPoint");
int InitInGameMenu__eq1() __asm__("InitInGameMenu");
int InitWeaponGrids__eq1() __asm__("InitWeaponGrids");
int InitAttractMode__eq1() __asm__("InitAttractMode");
int InitRaceData__eq1() __asm__("InitRaceData");
int InitRaceResetData__eq1() __asm__("InitRaceResetData");
int InitRaceLine__eq1() __asm__("InitRaceLine");
int SetInGameCDTrack__eq1() __asm__("SetInGameCDTrack");
int CheckShipDestroyed__eq1() __asm__("CheckShipDestroyed");
int CheckForButtonPress__eq1() __asm__("CheckForButtonPress");
int PlayCredits__eq1() __asm__("PlayCredits");
int Restart__eq1() __asm__("Restart");
int QuitGame__eq1() __asm__("QuitGame");
int CheckAttractOver__eq1() __asm__("CheckAttractOver");
int RaceComplete__eq1() __asm__("RaceComplete");
int CheckGamePause__eq1() __asm__("CheckGamePause");
int Check2PlayerPause__eq1() __asm__("Check2PlayerPause");
int PerformPauseOperations__eq1() __asm__("PerformPauseOperations");
int InitiateTwoPlayerPause__eq1() __asm__("InitiateTwoPlayerPause");
int Perform2PlayerPauseCommand__eq1() __asm__("Perform2PlayerPauseCommand");
int TrackSpecificFunctions__eq1() __asm__("TrackSpecificFunctions");
int ShowLinkBroken__eq1() __asm__("ShowLinkBroken");
u_short_eq1 WADInitScene__eq1() __asm__("WADInitScene");
int getPath__eq1() __asm__("getPath");
int LoadCmpFiles__eq1() __asm__("LoadCmpFiles");
int LoadCompressedTextureSequence__eq1() __asm__("LoadCompressedTextureSequence");
int WADLoadCompressedTextureSequence__eq1() __asm__("WADLoadCompressedTextureSequence");
int UpdateNotes__eq1() __asm__("UpdateNotes");
int PlayNote__eq1() __asm__("PlayNote");
int PlayDirectionalNote__eq1() __asm__("PlayDirectionalNote");
int AdjustEngineSound__eq1() __asm__("AdjustEngineSound");
int InitCrowdSound__eq1() __asm__("InitCrowdSound");
int AdjustCrowdSound__eq1() __asm__("AdjustCrowdSound");
int AddEffects__eq1() __asm__("AddEffects");
int InitTrainLights__eq1() __asm__("InitTrainLights");
int Video__eq1() __asm__("Video");
int InitWaterFall__eq1() __asm__("InitWaterFall");
int ColourDroidLights__eq1() __asm__("ColourDroidLights");
int InitObject__eq1() __asm__("InitObject");
int InitMonoData__eq1() __asm__("InitMonoData");
int LoadTrain__eq1() __asm__("LoadTrain");
int UpdateStartDroid__eq1() __asm__("UpdateStartDroid");
int InitDroidThings__eq1() __asm__("InitDroidThings");
int UpdateLightStart__eq1() __asm__("UpdateLightStart");
int MoveMonoRail__eq1() __asm__("MoveMonoRail");
int DrawShadow__eq1() __asm__("DrawShadow");
int DrawWeaponIcons__eq1() __asm__("DrawWeaponIcons");
int ShakeScreen__eq1() __asm__("ShakeScreen");
int InitTrackCycle__eq1() __asm__("InitTrackCycle");
int DrawLinkSign__eq1() __asm__("DrawLinkSign");
int doSerialGame__eq1() __asm__("doSerialGame");
int LoadModel__eq1() __asm__("LoadModel");
int AsmGenerateTiles__eq1() __asm__("AsmGenerateTiles");
int buildGameKeyPad__eq1() __asm__("buildGameKeyPad");
int buildFixedPad__eq1() __asm__("buildFixedPad");
int UpdateCheatSequence__eq1() __asm__("UpdateCheatSequence");
int InitRescueDroid__eq1() __asm__("InitRescueDroid");
int Swap__eq1() __asm__("Swap");
int closewadfile__eq1() __asm__("closewadfile");
int InitParticle__eq1() __asm__("InitParticle");
int AddSnow__eq1() __asm__("AddSnow");
int AddRain__eq1() __asm__("AddRain");
int LoadBirdGFX__eq1() __asm__("LoadBirdGFX");
int InitExplode__eq1() __asm__("InitExplode");
int DrawExplode__eq1() __asm__("DrawExplode");
int DrawBirds__eq1() __asm__("DrawBirds");
int InitRing__eq1() __asm__("InitRing");
int DrawRing__eq1() __asm__("DrawRing");
int AddSpurtExplode__eq1() __asm__("AddSpurtExplode");
int AddExplode__eq1() __asm__("AddExplode");
int AddSmallSpurtExplode__eq1() __asm__("AddSmallSpurtExplode");
int AddSmallSmokeExplode__eq1() __asm__("AddSmallSmokeExplode");
int AddSmokeExplode__eq1() __asm__("AddSmokeExplode");
int resetMiscFx__eq1() __asm__("resetMiscFx");
int UnsetParticles__eq1() __asm__("UnsetParticles");
int InitTrails__eq1() __asm__("InitTrails");
int DrawShipTrails__eq1() __asm__("DrawShipTrails");
int UpdateShipTrails__eq1() __asm__("UpdateShipTrails");
int RecordKeypresses__eq1() __asm__("RecordKeypresses");
int InitScoreTabData__eq1() __asm__("InitScoreTabData");
int openwadfile__eq1() __asm__("openwadfile");
extern u_char_eq1 trackPathTable[];
extern u_char_eq1 track1SmokeTextureSequence[];
extern u_char_eq1 sharedTrackTextureSequence[];
extern u_char_eq1 track6TextureSequence[];
extern u_char_eq1 track20TextureSequence[];
extern u_char_eq1 track17TextureSequence[];
extern u_char_eq1 track7TextureSequence[];
extern u_char_eq1 track13TorchTextureSequence[];
extern u_char_eq1 track13TextureSequence[];
extern u_char_eq1 track2ZeppelinNames[];
extern u_char_eq1 track2TextureSequence[];
extern u_char_eq1 msgOvertake[];
extern short linked;
extern short linkDropped;
extern short linkMyState;
extern short linkRemoteState;
extern u_char_eq1 sillyShips;
extern u_short_eq1 TextureTableCount__v1 __asm__("TextureTableCount");
extern int spareMax;
extern u_char_eq1 medalDisplay;
extern short fadeAmount;
extern int trackNameScroll;
int attractModeFlag;
int firstTimeIn;
int playCredits;
short trackCycleTimer;
int skycyc;
short raceFlag;
int oneMoreTime;
int masterWait;
short advertCount;
short cameraCount;
short neonCount;
short girderCount;
short videoCount;
short zepCount;
short dishCount;
short pylonCount;
short fanCount;
short smokeSlowCount;
short smokeFastCount;
u_short_eq1 smokeNumber__v1 __asm__("smokeNumber");
int fastCount__v1 __asm__("fastCount");
int fastTexCount__v1 __asm__("fastTexCount");
int slowCount__v1 __asm__("slowCount");
int slowTexCount__v1 __asm__("slowTexCount");
short torchCount;
u_short_eq1 torchNumber__v1 __asm__("torchNumber");
short redBullCount;
u_short_eq1 redBullNumber__v1 __asm__("redBullNumber");
int bobCount__v1 __asm__("bobCount");
short prmCount;
short startTextureBase__v1 __asm__("startTextureBase");
extern u_char_eq1 startDroidNames__v1[] __asm__("startDroidNames");
extern u_char_eq1 lightGroupNames__v1[] __asm__("lightGroupNames");
extern u_char_eq1 cameraDroidNames[];
extern u_char_eq1 fanNames[];
extern u_char_eq1 smokeSlowNames[];
extern u_char_eq1 smokeFastNames[];
extern u_char_eq1 redBullNames[];
extern u_char_eq1 advertNames[];
extern u_char_eq1 videoNames[];
extern u_char_eq1 radarNames[];
extern u_char_eq1 neonNames[];
extern u_char_eq1 torchNames[];
extern u_char_eq1 monoRailNames[];
extern u_char_eq1 girderNames[];
extern u_char_eq1 pylonNames[];
extern u_char_eq1 vectorShipName[];
extern u_char_eq1 venomShipName[];
extern u_char_eq1 rapierShipName[];
extern u_char_eq1 phantomShipName[];
extern u_char_eq1 overtakeMessage[];
extern u_char_eq1 undercover;
extern int writeFlag;
extern short buttonCount;
extern int cardNum;
extern int heading;
extern u_char_eq1 cheatRain;
extern u_char_eq1 menuSendFlags;
extern u_char_eq1 menuRecvFlags;
int weaponGrids;
extern int remoteShip;
int raceElapsed;
extern u_char_eq1 startDroidObjects__v1[] __asm__("startDroidObjects");
extern u_char_eq1 remoteRaceDone;
void *upperSkeleton;
extern void *girderObjects[];
int shadowData;
int weaponIndex;
extern unsigned int *OT[];
void *tempGameData;
extern int (*CameraUpdate)();
extern short shipClassIndex;
extern unsigned int *OT2[];
int (*prevCameraUpdate)();
u_short_eq1 sceneTextureCount;
extern int attractDelayFrames;
extern short clipLevel;
extern int ownShip;
int savedUpperY;
int weaponBars;
u_short_eq1 savedTextureCount__v1 __asm__("savedTextureCount");
extern u_char_eq1 teamShip[];
extern int linkSendFlag;
extern u_char_eq1 gameType;
signed char smokeColour__v1 __asm__("smokeColour");
extern signed char smokeColourGreen;
extern signed char smokeColourBlue;
extern int raceType;
TrackCache *trackCache;
extern short CurrentScreen;
extern int reserveHeap;
int raceStartVcount;
extern int quadIndex;
extern int raceEndTimer;
Object_eq1 * volatile zeppelinObj__v1 __asm__("zeppelinObj");
extern u_char_eq1 padBuf1[];
u_short_eq1 tempTextureCount;
int checkTimer;
extern Skeleton_eq1 **cameraPtr;
extern u_char_eq1 keysPressed[];
ShipData_eq1 *shipIndex__v1 __asm__("shipIndex");
extern u_char_eq1 linkRaceDone;
int particleSlots;
int savedHeap;
extern int fracdamage[];
extern void *radar__v1[] __asm__("radar");
extern void *neonObject__v1[] __asm__("neonObject");
extern CheckPointShort checkCells__v1[][4][6] __asm__("checkCells");
extern CheckPointShort raceCheckpointResults[][4][6];
extern CheckPointByte raceCheckpointFlags[][4][6];
extern u_char_eq1 weaponIcons__v1[] __asm__("weaponIcons");
extern void *torchLight__v1[] __asm__("torchLight");
extern Object_eq1 *shipShapeTable__v1[] __asm__("shipShapeTable");
extern int lightDroid;
extern u_char_eq1 scoreTabData[];
extern unsigned char hudDisabled;
extern int shipDamage[];
extern Skeleton_eq1 *residentSkeletons__v1[] __asm__("residentSkeletons");
extern CameraSections cameraSections;
extern u_char_eq1 posMonoRail[];
extern u_char_eq1 rescueData[];
extern int (*rescueUpdateFunc)();
extern u_char_eq1 speedoHud__v1[] __asm__("speedoHud");
extern void *advert__v1[] __asm__("advert");
extern u_char_eq1 lightGroups__v1[] __asm__("lightGroups");
extern volatile int zeppelinBaseX;
extern volatile int zeppelinBaseY;
extern int zeppelinBaseZ;
extern void *pylons[];
extern void *redBull__v1[] __asm__("redBull");
extern void *cameraDroids__v1[] __asm__("cameraDroids");
extern void *videoObjects[];
extern int shipTargetIndex[];
extern volatile int pauseState[];
extern u_char_eq1 twoPlayerPauseStatus;
extern signed char twoPlayerMode;
extern signed char twoPlayerFlag;
extern u_char_eq1 playerControl[];
extern u_char_eq1 monoRailObjs[];
extern Object_eq1 *prmObjects[];
extern u_char_eq1 weaponIconTextureTable__v1[] __asm__("weaponIconTextureTable");
extern void *fans__v1[] __asm__("fans");
extern POLY_FT4_eq1 weaponIconPrims[];
extern u_char_eq1 scenePath[];
extern volatile short monoRailFlag[];
extern volatile int trainMuzzleX;
extern volatile int trainMuzzleY;
extern volatile int trainMuzzleZ;
extern void *smokeFastObjects__v1[] __asm__("smokeFastObjects");
extern void *smokeSlowObjects__v1[] __asm__("smokeSlowObjects");
extern u_char_eq1 UpdatePlayerShip__v1[] __asm__("UpdatePlayerShip");
extern u_char_eq1 UpdateRescueDroid__v1[] __asm__("UpdateRescueDroid");
extern u_char_eq1 UpdateDroneShip__v1[] __asm__("UpdateDroneShip");
extern int heap;
extern TrackData_eq1 *track__v1 __asm__("track");
static inline int divideSix(int value, int magic) { union { long long product; struct { int hi; u_int_eq1 lo; } words; } result; result.product=(long long)value * magic; return result.words.hi - (value >> 31); }
#define CHECKPOINTS (checkCells__v1[gameData->team][gameData->shipClass])
void race__eq1(GameData *gameData, int context, int scene) __asm__("race");
void race__eq1(GameData *gameData, int context, int scene) {
    RaceLocals locals;
    void *wad,*tiles;
    int i,j,radius,x,y,z,bob;
    int advance;
    int one=1; int two=2;
    locals.frameCount=0;locals.checkpoint=0;locals.cdMode=one;locals.viewMode=two;locals.checkpointLap=0;locals.raceComplete=0;locals.scene=scene;
    InitDynamicMem__eq1(reserveHeap,0xE1000);
    locals.crowdSample=0;
    advertCount=0;cameraCount=0;neonCount=0;girderCount=0;videoCount=0;zepCount=0;dishCount=0;pylonCount=0;fanCount=0;smokeSlowCount=0;smokeFastCount=0;smokeNumber__v1=0;
    fastCount__v1=0;fastTexCount__v1=0;slowCount__v1=0;slowTexCount__v1=0;torchCount=0;torchNumber__v1=0;redBullCount=0;redBullNumber__v1=0;bobCount__v1=0;prmCount=0;
    attractModeFlag=0;firstTimeIn=one;twoPlayerMode=two;twoPlayerFlag=0;checkTimer=0;savedHeap=heap;heap=reserveHeap;savedTextureCount__v1=TextureTableCount__v1;masterWait=0;writeFlag=0x3938700;
    locals.needEndText=0;locals.needWinLose=0;locals.firstTime=1;
    if(context==0) raceFlag=1;
    trackCache=DAlloc__eq1(reserveHeap,0x902C);
    trackCache->draw=0;trackCache->count=0;trackCache->mode=0;trackCache->active=1;
    locals.clearPolys[0]=BlockFill__eq1(0,0,320,240,0,0,0);
    locals.clearPolys[1]=BlockFill__eq1(0,256,320,240,0,0,0);
    SetSkeletonPosition__eq1(residentSkeletons__v1[0],0,0,0);SetSkeletonDirection__eq1(residentSkeletons__v1[0],0,0,0);
    SetSkeletonPosition__eq1(upperSkeleton,0,0,0);SetSkeletonDirection__eq1(upperSkeleton,0,0,0);
    cameraPtr=residentSkeletons__v1;track__v1=&locals.trackData;tempGameData=gameData;residentSkeletons__v1[0]->parent=upperSkeleton;
    getPath__eq1(scenePath,gameData->trackNo,trackPathTable);
    wad=openwadfile__eq1(scenePath);
    WADInitVertices__eq1(wad);WADInitFaces__eq1(wad);WADInitSections__eq1(wad);WADInitViewList__eq1(wad);
    tempTextureCount=TextureTableCount__v1;TextureTableCount__v1=0;
    LoadCmpFiles__eq1(WADLoadCompressedTextureSequence__eq1(wad));
    tiles=WADLoadTtfFile__eq1(wad,&locals.frameCount,0);
    TextureTableCount__v1=tempTextureCount;
    locals.lightCountdown=WADInitScene__eq1(prmObjects,&prmCount,residentSkeletons__v1[0],gameData,wad);
    sceneTextureCount=TextureTableCount__v1;closewadfile__eq1(wad);
    for(j=0;j<6;j++) {
        raceCheckpointResults[gameData->team][gameData->shipClass][j].value=0;
        checkCells__v1[gameData->team][gameData->shipClass][j].value=-1;
        raceCheckpointFlags[gameData->team][gameData->shipClass][j].value=-1;
    }
    InitTrackTextures__eq1(gameData);
    InitWeaponGrids__eq1(weaponGrids,track__v1,&locals.checkpoint,&locals.checkpointLap,CHECKPOINTS);
    InitCheckPoints__eq1(CHECKPOINTS,gameData,gameData->shipClass);
    trackCycleTimer=0;InitTrackCycle__eq1();
    InitDroidThings__eq1(startDroidObjects__v1,startDroidNames__v1,residentSkeletons__v1[0],CHECKPOINTS,gameData);
    InitDroidThings__eq1(lightGroups__v1,lightGroupNames__v1,residentSkeletons__v1[0],CHECKPOINTS,gameData);
    InitRaceLine__eq1(track__v1,gameData);InitInGameMenu__eq1(locals.inGameMenu);
    { int crowd=InitCrowdSound__eq1(prmObjects,prmCount,locals.cdState,locals.crowdSample);
    unsigned char currentTrack=*(unsigned char *)&gameData->trackNo;
    locals.crowdSample=crowd;
    if(currentTrack==1) {
        InitObject__eq1(prmObjects,prmCount,cameraDroids__v1,cameraDroidNames,6,&cameraCount);
        InitObject__eq1(prmObjects,prmCount,fans__v1,fanNames,3,&fanCount);
        InitObject__eq1(prmObjects,prmCount,smokeSlowObjects__v1,smokeSlowNames,6,&smokeSlowCount);
        InitObject__eq1(prmObjects,prmCount,smokeFastObjects__v1,smokeFastNames,6,&smokeFastCount);
        smokeNumber__v1=TextureTableCount__v1;LoadCmpFiles__eq1(LoadCompressedTextureSequence__eq1(track1SmokeTextureSequence));
        InitObject__eq1(prmObjects,prmCount,redBull__v1,redBullNames,4,&redBullCount);
        redBullNumber__v1=TextureTableCount__v1;LoadCmpFiles__eq1(LoadCompressedTextureSequence__eq1(sharedTrackTextureSequence));
        smokeColour__v1=64;smokeColourGreen=64;smokeColourBlue=64;
    }
    }
    if(gameData->trackNo==6) {
        InitObject__eq1(prmObjects,prmCount,advert__v1,advertNames,6,&advertCount);
        InitObject__eq1(prmObjects,prmCount,cameraDroids__v1,cameraDroidNames,6,&cameraCount);
        InitObject__eq1(prmObjects,prmCount,redBull__v1,redBullNames,4,&redBullCount);
        InitObject__eq1(prmObjects,prmCount,videoObjects,videoNames,6,&videoCount);
        redBullNumber__v1=TextureTableCount__v1;LoadCmpFiles__eq1(LoadCompressedTextureSequence__eq1(track6TextureSequence));
    }
    if(gameData->trackNo==8) {
        InitObject__eq1(prmObjects,prmCount,advert__v1,advertNames,6,&advertCount);
        InitObject__eq1(prmObjects,prmCount,cameraDroids__v1,cameraDroidNames,6,&cameraCount);
        InitObject__eq1(prmObjects,prmCount,redBull__v1,redBullNames,4,&redBullCount);
        redBullNumber__v1=TextureTableCount__v1;LoadCmpFiles__eq1(LoadCompressedTextureSequence__eq1(sharedTrackTextureSequence));
    }
    if(gameData->trackNo==20) {
        InitObject__eq1(prmObjects,prmCount,radar__v1,radarNames,4,&dishCount);
        InitObject__eq1(prmObjects,prmCount,cameraDroids__v1,cameraDroidNames,6,&cameraCount);
        InitObject__eq1(prmObjects,prmCount,neonObject__v1,neonNames,5,&neonCount);
        InitObject__eq1(prmObjects,prmCount,redBull__v1,redBullNames,4,&redBullCount);
        redBullNumber__v1=TextureTableCount__v1;LoadCmpFiles__eq1(LoadCompressedTextureSequence__eq1(track20TextureSequence));
    }
    if(gameData->trackNo==17) {
        InitObject__eq1(prmObjects,prmCount,cameraDroids__v1,cameraDroidNames,6,&cameraCount);
        InitObject__eq1(prmObjects,prmCount,redBull__v1,redBullNames,4,&redBullCount);
        redBullNumber__v1=TextureTableCount__v1;LoadCmpFiles__eq1(LoadCompressedTextureSequence__eq1(track17TextureSequence));
    }
    if(gameData->trackNo==7) {
        InitObject__eq1(prmObjects,prmCount,videoObjects,videoNames,6,&videoCount);
        InitObject__eq1(prmObjects,prmCount,redBull__v1,redBullNames,4,&redBullCount);
        InitObject__eq1(prmObjects,prmCount,advert__v1,advertNames,6,&advertCount);
        redBullNumber__v1=TextureTableCount__v1;LoadCmpFiles__eq1(LoadCompressedTextureSequence__eq1(track7TextureSequence));
    }
    if(gameData->trackNo==13) {
        InitWaterFall__eq1();
        InitObject__eq1(prmObjects,prmCount,torchLight__v1,torchNames,5,&torchCount);
        torchNumber__v1=TextureTableCount__v1;LoadCmpFiles__eq1(LoadCompressedTextureSequence__eq1(track13TorchTextureSequence));
        InitObject__eq1(prmObjects,prmCount,redBull__v1,redBullNames,4,&redBullCount);
        redBullNumber__v1=TextureTableCount__v1;LoadCmpFiles__eq1(LoadCompressedTextureSequence__eq1(track13TextureSequence));
    }
    if(gameData->trackNo==2) {
        LoadTrain__eq1(monoRailObjs,monoRailNames,residentSkeletons__v1[0],posMonoRail);
        InitObject__eq1(prmObjects,prmCount,cameraDroids__v1,cameraDroidNames,6,&cameraCount);
        InitObject__eq1(prmObjects,prmCount,&zeppelinObj__v1,track2ZeppelinNames,8,&zepCount);
        InitObject__eq1(prmObjects,prmCount,girderObjects,girderNames,5,&girderCount);
        InitTrainLights__eq1(prmObjects,prmCount,monoRailObjs,pauseState[0]);
        InitObject__eq1(prmObjects,prmCount,redBull__v1,redBullNames,4,&redBullCount);
        redBullNumber__v1=TextureTableCount__v1;LoadCmpFiles__eq1(LoadCompressedTextureSequence__eq1(track2TextureSequence));
        { Object_eq1 *trainObject=zeppelinObj__v1;
        monoRailFlag[0]=0;trainMuzzleX=0;trainMuzzleY=0;trainMuzzleZ=0;
        { Skeleton_eq1 *trainSkeleton=*(Skeleton_eq1 * volatile *)&trainObject->skeleton; *(volatile int *)&trainSkeleton->z-=8000; } }
        { Object_eq1 *trainPosition=zeppelinObj__v1; zeppelinBaseX=(*(Skeleton_eq1 * volatile *)&trainPosition->skeleton)->x;zeppelinBaseY=(*(Skeleton_eq1 * volatile *)&trainPosition->skeleton)->y;zeppelinBaseZ=(*(Skeleton_eq1 * volatile *)&trainPosition->skeleton)->z; }
        InitObject__eq1(prmObjects,prmCount,pylons,pylonNames,5,&pylonCount);
        InitMonoData__eq1(pylons,pylonCount,&monoRailFlag[0]);
    }
    buildGameKeyPad__eq1(padBuf1,playerControl);initShipData__eq1(track__v1->sections,shipIndex__v1,gameData);
    if(gameData->trackNo==13) LoadBirdGFX__eq1();
    InitRescueDroid__eq1(rescueData,shipShapeTable__v1,&shipIndex__v1[ownShip]);
    CameraUpdate=UpdateCameraRaceStart;InitCamera__eq1(residentSkeletons__v1,shipIndex__v1[ownShip].nearTrkSect);
    spareMax=0;InitTrails__eq1();AsmGenerateTiles__eq1(tiles,locals.frameCount);
    InitRaceResetData__eq1(locals.inputPacket,shipIndex__v1,gameData,weaponIndex,weaponBars,rescueData,shipShapeTable__v1,CHECKPOINTS);
    for(j=0;j<15;j++) {shipDamage[j]=0;fracdamage[j]=0;}
    particleSlots=InitParticle__eq1();InitExplode__eq1();InitRing__eq1();InitScoreTabData__eq1(scoreTabData);InitialiseBumps__eq1();
    if(raceType==0) {
        switch(gameData->shipClass) {
        case 0: LoadModel__eq1(teamShip,vectorShipName);break;
        case 1: LoadModel__eq1(teamShip,venomShipName);break;
        case 2: LoadModel__eq1(teamShip,rapierShipName);break;
        case 3: LoadModel__eq1(teamShip,phantomShipName);break;
        }
    } else if(raceType==4) {
        if(gameData->venomComplete) LoadModel__eq1(teamShip,rapierShipName);
        else {
            goto checkRapier;
quit:
            QuitGame__eq1();
            return;
checkRapier:
            if(gameData->rapierComplete) LoadModel__eq1(teamShip,phantomShipName);
        }
    }
    one=1;
    locals.divisionMagic=0x2AAAAAAB;trackNameScroll=480;gameData->raceCount=0;medalDisplay=0;
restart:
    Restart__eq1(locals.inputPacket);InitRaceData__eq1(pauseState);
    if(context!=one) attractDelayFrames=0;
    InitAttractMode__eq1(context,shipIndex__v1);gameData->finished=0;SetUpFirstCheckPoint__eq1();
frame:
    buildGameKeyPad__eq1(padBuf1,playerControl);buildFixedPad__eq1(padBuf1);
    if(raceType==one) RecordKeypresses__eq1(shipIndex__v1);
    UpdateCheatSequence__eq1(0);ClearOTagR(OT[CurrentScreen],8192);ClearOTagR(OT2[CurrentScreen],1600);
    buttonCount=0;quadIndex=CurrentScreen*300;shipClassIndex=gameData->shipClass;clipLevel=gameData->team;
    SetInGameCDTrack__eq1(&locals.cdTrack,locals.cdLocations,gameData,&locals.cdMode);
    if(gameType!=2) doSerialGame__eq1(gameData,shipIndex__v1);
    ShowLinkBroken__eq1();writeFlag=0;
    CheckShipDestroyed__eq1(shipIndex__v1,gameData,&context,locals.scene);PlayCredits__eq1(context);
    CheckGamePause__eq1(shipIndex__v1,pauseState,gameData,keysPressed,&pauseState[1]);
    Check2PlayerPause__eq1(twoPlayerPauseStatus,&locals.viewMode,&pauseState[1]);
    PerformPauseOperations__eq1(pauseState,locals.inGameMenu,gameData,locals.inputPacket);
    InitiateTwoPlayerPause__eq1(pauseState,shipIndex__v1);
    if(!raceEndTimer) Perform2PlayerPauseCommand__eq1(pauseState,gameData,locals.inputPacket);
    if((pauseState[0]==-1 || oneMoreTime) && !masterWait && !linkDropped) {
        oneMoreTime=0;CheckForButtonPress__eq1();UpdateShips__eq1(shipIndex__v1,weaponBars,shipShapeTable__v1,weaponIndex);
        rescueUpdateFunc(rescueData,&shipIndex__v1[ownShip],shipShapeTable__v1,shipIndex__v1);
        CameraUpdate(residentSkeletons__v1,&shipIndex__v1[ownShip],rescueData);
        if(shipIndex__v1[ownShip].attr&64) {
            if(shipIndex__v1[ownShip].weapon==2 || shipIndex__v1[ownShip].weapon==5) {
                MissileLock__eq1(ownShip,shipIndex__v1);if(shipIndex__v1[ownShip].weapon==2) RearMissileLock__eq1(ownShip,shipIndex__v1);
            }
            ShakeScreen__eq1();
        }
        if(gameType!=2 && (shipIndex__v1[remoteShip].attr&64)) {
            if(shipIndex__v1[remoteShip].weapon==2 || shipIndex__v1[remoteShip].weapon==5) {
                MissileLock__eq1(remoteShip,shipIndex__v1);if(shipIndex__v1[remoteShip].weapon==2) RearMissileLock__eq1(remoteShip,shipIndex__v1);
            }
        }
        UpdateWeapons__eq1(*(ShipData_eq1 *volatile *)&shipIndex__v1,weaponBars,weaponIndex);CheckPitShips__eq1();
        if(pauseState[0]==-1 && !linkDropped) {AdjustEngineSound__eq1(&shipIndex__v1[ownShip],residentSkeletons__v1);AdjustCrowdSound__eq1(locals.cdState,locals.crowdSample);}
        TrackSpecificFunctions__eq1();ColourDroidLights__eq1(lightDroid);AddEffects__eq1(weaponIndex,shipIndex__v1);raceElapsed=GsGetVcount()-raceStartVcount;
        if(raceType!=one && gameData->weapons) CycleWeaponGrids__eq1(shipIndex__v1,weaponGrids,locals.checkpoint);
        else if(!gameData->weapons && raceType==4) CycleWeaponGrids__eq1(shipIndex__v1,weaponGrids,locals.checkpoint);
    }
    masterWait=0;
    if(!linkSendFlag) {
        if(attractDelayFrames>0) {
            attractDelayFrames--;
            if((context==one && CheckAttractOver__eq1(gameData)) || !attractDelayFrames) raceEndTimer=2;
        }
    } else RaceComplete__eq1(locals.inGameMenu,locals.inputPacket,gameData,&locals.raceComplete,scoreTabData,shipIndex__v1,pauseState);
    if(raceEndTimer>0) {
        if(linked) {
            if(linkRemoteState==4) raceEndTimer=0;
            else if(linkRemoteState==3) {linkMyState=4;raceEndTimer=one;playCredits=one;}
        } else {raceEndTimer--;playCredits=one;}
        if(!raceEndTimer) goto quit;
    }
    if(hudDisabled!=one && !raceEndTimer) {
        UpdateRaceHud__eq1(shipIndex__v1,speedoHud__v1,pauseState[0],gameData,scoreTabData);
        if(shipIndex__v1[ownShip].update==(void (*)())UpdatePlayerShip__v1 || shipIndex__v1[ownShip].update==(void (*)())UpdateRescueDroid__v1 || (shipIndex__v1[ownShip].update==(void (*)())UpdateDroneShip__v1 && shipIndex__v1[ownShip].timer>0)) {
            if((shipIndex__v1[ownShip].state.word&0x8004)!=0x8004) AlterCheckPoint__eq1(CHECKPOINTS,shipIndex__v1,pauseState[0],gameData);
        }
    }
    if(gameData->weapons || raceType==one || raceType==4) {
        if((shipIndex__v1[ownShip].attr&64) && ownShip<12) {
            DrawWeaponIcons__eq1(weaponIcons__v1,weaponIconTextureTable__v1,&shipIndex__v1[ownShip]);AddPrim(OT[CurrentScreen],&weaponIconPrims[CurrentScreen]);
        }
    }
    if(gameData->trackNo==2 && raceType!=two && (unsigned int)(shipIndex__v1[ownShip].nearTrkSect->sectionNo-200)<76) {
        MoveMonoRail__eq1(monoRailObjs,6,&monoRailFlag[0],shipIndex__v1,pauseState[0]);InitTrainLights__eq1(prmObjects,prmCount,monoRailObjs,pauseState[0]);
    }
    if((unsigned int)(gameData->trackNo-6)<2) Video__eq1(videoObjects,videoCount);
    if(!(shipIndex__v1[ownShip].attr&64) || shipTargetIndex[ownShip]>=0) DrawAllTrack__eq1(residentSkeletons__v1,trackCache);
    else DrawNewTrack__eq1(residentSkeletons__v1,trackCache);
    DrawShadow__eq1(shipShapeTable__v1,shipIndex__v1,shadowData);
    { ShipData_eq1 *ships=shipIndex__v1; Skeleton_eq1 *camera=residentSkeletons__v1[0]; int mode=pauseState[0]; DrawShips__eq1(ships,shipShapeTable__v1,ownShip,camera,cameraSections,mode); }
    if(!medalDisplay) DrawLandscape__eq1(&shipIndex__v1[ownShip],residentSkeletons__v1);
    (*(Object_eq1 **)((char *)prmObjects + ((int)((u_int_eq1)*(volatile u_short_eq1 *)&locals.lightCountdown << 16) >> 14)))->skeleton->dirty=one;
    savedUpperY=((Skeleton_eq1 *)residentSkeletons__v1[0]->parent)->y;
    if(gameData->trackNo==17) {
        if(residentSkeletons__v1[0]->y>0) {
            bob=rsin(skycyc);
            ((Skeleton_eq1 *)residentSkeletons__v1[0]->parent)->y+=1024+(((residentSkeletons__v1[0]->y>>3)*bob)>>12);
        }
        if(pauseState[0]==-1 && (unsigned int)(cameraSections.section->sectionNo-303)>=26) skycyc=(skycyc+96)&4095;
    }
    AsmDrawObjectFixed((*(Object_eq1 **)((char *)prmObjects + ((int)((u_int_eq1)*(volatile u_short_eq1 *)&locals.lightCountdown << 16) >> 14))),residentSkeletons__v1[0],8191);
    ((Skeleton_eq1 *)residentSkeletons__v1[0]->parent)->y=savedUpperY;
    if(prevCameraUpdate!=CameraUpdate) {UnsetParticles__eq1(particleSlots);prevCameraUpdate=CameraUpdate;}
    AsmDrawParticles(particleSlots,residentSkeletons__v1[0],pauseState[0]+1);DrawExplode__eq1();
    if(gameData->trackNo==13) DrawBirds__eq1();DrawRing__eq1();if(sillyShips) UpdateSharkModel__eq1();
    if(gameType!=2 && !(shipIndex__v1[remoteShip].state.half&0x8000)) DrawLinkSign__eq1();
    if(pauseState[0]<0) {
        UpdateShipTrails__eq1(shipIndex__v1,attractDelayFrames);locals.firstTime=1;
        if(shipTargetIndex[ownShip]==100) shipIndex__v1[ownShip].attr&=~4;
        if((shipTargetIndex[ownShip]&7)<4 && shipTargetIndex[ownShip]>=16 && !(shipIndex__v1[ownShip].state.half&4)) {
            CentreText__eq1(overtakeMessage,15,64,12,one);CentreText__eq1(msgOvertake,45,128,8,0);
            if(!(shipTargetIndex[ownShip]&7)) PlayNote__eq1(48,255,15,15,256);
        }
        for(i=0;i<15;i++) {
            if(!(shipIndex__v1[i].state.half&0x8000) && shipTargetIndex[i]>0) {
                if(i==ownShip) { if(shipTargetIndex[i]>=16) goto damp_ship; }
                if(i!=remoteShip) goto damping_done;
                if(shipTargetIndex[i]<16) goto damping_done;
                damp_ship: {
                    ShipData_eq1 *ship=*(ShipData_eq1 *volatile *)&shipIndex__v1;
                    int oldVx=ship[i].vpivot.vx, oldVz=ship[i].vpivot.vz;
                    ship[i].vpivot.vx-=ship[i].vpivot.vx>>3;
                    ship[i].vpivot.vz-=ship[i].vpivot.vz>>3;
                    ship[i].ppivot.vx+=oldVx>>6;
                    ship[i].ppivot.vz+=oldVz>>6;
                    ship[i].vhdg=0;
                    ship[i].ppivot.vy+=ship[i].vpivot.vy>>6;
                    ship[i].vpivot.vy-=ship[i].vpivot.vy>>3;
                    ship[i].roll+=(ship[i].vpivot.vx+ship[i].vpivot.vz)>>5;
                }
                damping_done:
                if(shipTargetIndex[i]<15) {
                    radius=(15-shipTargetIndex[i])*64;
                    if((shipTargetIndex[i]&7)==7) {
                        x=shipIndex__v1[i].ppivot.vx+newRand__eq1()%radius-(radius>>1);
                        y=shipIndex__v1[i].ppivot.vy-newRand__eq1()%127;
                        z=shipIndex__v1[i].ppivot.vz+newRand__eq1()%radius-(radius>>1);
                        AddExplode__eq1(x,y,z,-1);
                    }
                    if((divideSix(shipTargetIndex[i],locals.divisionMagic))*6==shipTargetIndex[i]-4) {
                        x=shipIndex__v1[i].ppivot.vx+newRand__eq1()%radius-(radius>>1);
                        y=shipIndex__v1[i].ppivot.vy-newRand__eq1()%127;
                        z=shipIndex__v1[i].ppivot.vz+newRand__eq1()%radius-(radius>>1);
                        AddSmokeExplode__eq1(x,y,z,-1);
                    }
                    radius=(20-shipTargetIndex[i])*64;
                    if((shipTargetIndex[i]-divideSix(shipTargetIndex[i],locals.divisionMagic)*6)==one) {
                        x=shipIndex__v1[i].ppivot.vx+newRand__eq1()%radius-(radius>>1);
                        y=shipIndex__v1[i].ppivot.vy-newRand__eq1()%127;
                        z=shipIndex__v1[i].ppivot.vz+newRand__eq1()%radius-(radius>>1);
                        AddSpurtExplode__eq1(x,y,z,-1);
                    }
                }
                if(shipTargetIndex[i]==14) {
                    PlayDirectionalNote__eq1(5,0,40,shipIndex__v1[i].ppivot);
                    PlayDirectionalNote__eq1(5,0,40,shipIndex__v1[i].ppivot);
                    PlayDirectionalNote__eq1(5,-64,40,shipIndex__v1[i].ppivot);
                    PlayDirectionalNote__eq1(5,-128,40,shipIndex__v1[i].ppivot);
                    PlayDirectionalNote__eq1(5,-128,40,shipIndex__v1[i].ppivot);
                    PlayDirectionalNote__eq1(5,-192,40,shipIndex__v1[i].ppivot);
                }
                if((newRand__eq1()&3)==one && shipTargetIndex[i]>=16) AddSmallSmokeExplode__eq1(shipIndex__v1[i].ppivot.vx-(shipIndex__v1[i].unitVecNose.vx>>4),shipIndex__v1[i].ppivot.vy-(shipIndex__v1[i].unitVecNose.vy>>4),shipIndex__v1[i].ppivot.vz-(shipIndex__v1[i].unitVecNose.vz>>4),-1);
                if((shipTargetIndex[i]&3)==one && shipTargetIndex[i]>=16) {
                    x=(newRand__eq1()&511)-(shipIndex__v1[i].unitVecNose.vx>>5)-256;
                    z=(newRand__eq1()&511)-(shipIndex__v1[i].unitVecNose.vz>>5)-256;
                    AddSmallSpurtExplode__eq1(x,-(shipIndex__v1[i].unitVecNose.vy>>5)+128,z,i);
                }
                shipTargetIndex[i]--;
                if(!shipTargetIndex[i]) {
                    if(i!=ownShip && i!=remoteShip) {shipIndex__v1[i].attr&=~64;PlayNote__eq1(51,480,15,15,256);}
                    shipIndex__v1[i].state.half|=0x8000;
                }
            }
            if((i==ownShip && (shipIndex__v1[i].state.half&0x8000)) || (i==remoteShip && (shipIndex__v1[i].state.half&0x8000))) {
                if((newRand__eq1()&31)<3) AddSmallSmokeExplode__eq1(shipIndex__v1[i].ppivot.vx,shipIndex__v1[i].nearTrkSect->centerY+128,shipIndex__v1[i].ppivot.vz,-1);
            }
        }
        if(gameData->trackNo==2 || gameData->trackNo==7 || cheatRain) {
            AddRain__eq1(particleSlots,one,&(*cameraPtr)->x);
            advance=locals.needEndText+1;locals.needEndText=advance&255;
            if(!undercover && !(newRand__eq1()&3) && locals.needEndText<32 && !(advance&3)) {
                if(fadeAmount<228) fadeAmount=228;locals.needWinLose=locals.needEndText;
            }
            if(fadeAmount==220) fadeAmount=118;if(fadeAmount==110) fadeAmount=0;
            advance=locals.needWinLose+32;
            if(locals.needEndText==advance) PlayNote__eq1(35,380,15,15,128);
            if(advance<locals.needEndText && locals.needEndText<64 && !(newRand__eq1()&3) && !(locals.needEndText&3)) PlayNote__eq1(35,(short)(412-locals.needEndText),15,15,320-locals.needEndText);
        }
        if(gameData->trackNo==8) AddSnow__eq1(particleSlots,20,&(*cameraPtr)->x);
    } else {if(locals.firstTime) UpdateShipTrails__eq1(shipIndex__v1,attractDelayFrames);locals.firstTime=0;}
    resetMiscFx__eq1();DrawShipTrails__eq1(cameraPtr);
    if(!linkDropped) {
        UpdateStartDroid__eq1(startDroidObjects__v1,shipIndex__v1,CHECKPOINTS,startTextureBase__v1,gameData,pauseState[0]);
        UpdateLightStart__eq1(lightGroups__v1,shipIndex__v1,CHECKPOINTS,startTextureBase__v1,gameData,pauseState[0]);
    }
    if(playCredits) {ClearOTagR(OT[CurrentScreen],8192);ClearOTagR(OT2[CurrentScreen],1600);}
    AddPrim(OT[CurrentScreen]+8191,locals.clearPolys[CurrentScreen]);
    if(gameType!=one) advance=menuSendFlags;else advance=menuRecvFlags;
    cardNum-=advance*30;heading-=advance*15;
    Swap__eq1();firstTimeIn=0;UpdateNotes__eq1(0);
    if(!linkRaceDone) goto frame;
    if(gameType!=2 && !remoteRaceDone) goto frame;
    context=0;goto restart;
}
#undef CHECKPOINTS

/* Creates the resident skeletons, ship data, weapons and textures that stay loaded for the whole game. */
void InitResident(void)
{
    ShipData     *shipData;
    TrackSection *sections;
    TIMlist      *textureSequence;
    void         *weaponPool;
    Skeleton     *rootSkeleton;
    Skeleton     *subSkeleton;

    rootSkeleton = NewSkeleton();
    residentSkeletons[0] = rootSkeleton;
    SetSkeletonPosition(rootSkeleton, 0, 0, 0);
    SetSkeletonDirection(residentSkeletons[0], 0, 0, 0);
    subSkeleton = NewSkeleton();
    upperSkeleton = subSkeleton;
    SetSkeletonPosition(subSkeleton, 0, 0, 0);
    SetSkeletonDirection(upperSkeleton, 0, 0, 0);
    residentSkeletons[0]->super = upperSkeleton;
    shadowData = InitShadow(residentSkeletons[0]);
    shipData = DAlloc(heap, 0xFF0);
    sections = track->sections;
    shipIndex = shipData;
    loadShipData(sections, shipData, shipShapeTable, residentSkeletons[0]);
    textureWad = openwadfilenvc(startWadPath);
    InitSpeedo(&speedoHud);
    InitLinkUpSign();
    continuesTexture = WADLoadTexture(textureWad, 1);
    startTextureBase = TextureTableCount;
    textureSequence = WADLoadCompressedTextureSequence(textureWad);
    residentTextureSeq = textureSequence;
    LoadCmpFiles(textureSequence);
    InitWeaponIcons(weaponIcons, weaponIconTextureTable);
    weaponGrids = (int) DAlloc(heap, 0x420);
    weaponBars = (int) DAlloc(heap, 0x118);
    weaponPool = DAlloc(heap, 0x890);
    weaponIndex = (int) weaponPool;
    initWeapons(weaponBars, residentSkeletons[0], weaponPool, weaponInitArg);
    LoadExplodeGFX();
    LoadStartDroid(startDroidObjects, startDroidNames, residentSkeletons[0], checkCells[gameData->trackNum][gameData->shipClass], gameData);
    LoadStartDroid(lightGroups, lightGroupNames, residentSkeletons[0], checkCells[gameData->trackNum][gameData->shipClass], gameData);
}


void racetidy(void)
{
    heap = mainheapcopy;
    InitDynamicMem(reserveHeap, 0xE1000);
    StartControlledSound();
    TextureTableCount = savedTextureCount;
}


/* Runs the animation and moving parts that belong to the current track. */
void TrackSpecificFunctions(void)
{
    u_char trackNumber;

    RedBullAnimate(redBull, redBullCount, 0x14, redBullNumber, 2);
    trackNumber = gameData->actTrackNum;
    if (trackNumber == 1) {
        MoveCameraDroid(cameraDroids, cameraCount, shipIndex, &bobCount);
        MoveFan(fans, fanCount);
        AnimateTexture(smokeSlowObjects, smokeSlowCount, 1, smokeNumber, 25, smokeColour, &fastCount, &fastTexCount, 1);
        AnimateTexture(smokeFastObjects, smokeFastCount, 1, smokeNumber, 25, smokeColour, &slowCount, &slowTexCount, 2);
    } else if (trackNumber == 8) {
        MoveCameraDroid(cameraDroids, cameraCount, shipIndex, &bobCount);
        MoveAdvert(advert, advertCount);
    } else if (trackNumber == 6) {
        MoveCameraDroid(cameraDroids, cameraCount, shipIndex, &bobCount);
        MoveAdvert(advert, advertCount);
    } else if (trackNumber == 20) {
        MoveCameraDroid(cameraDroids, cameraCount, shipIndex, &bobCount);
        /* Called with a third argument that MoveRadar ignores. */
        ((void (*)())MoveRadar)(radar, dishCount, shipIndex);
        DoNeonSign(neonObject, 3);
    } else if (trackNumber == 7) {
        MoveAdvert(advert, advertCount);
    } else if (trackNumber == 17) {
        MoveCameraDroid(cameraDroids, cameraCount, shipIndex, &bobCount);
    } else if (trackNumber == 13) {
        WaterFall();
        AnimateSprite(torchLight, torchCount, 1, torchNumber, 12);
    } else if (trackNumber == 2) {
        MoveCameraDroid(cameraDroids, cameraCount, shipIndex, &bobCount);
        MoveZeppelin(zeppelinObj, zepCount, shipIndex);
        beamCollide = 0;
    }
}


/* Shows the "link broken" screen for a short time, flashing the cable message. */
void ShowLinkBroken(void)
{
    if (linkTimer > 0) {
        linkTimer -= 1;
        if ((linkFlashFrame % 30) < 0x12) {
            CentreText(msgCombatCable, 0xA0, 0x46, 0xC, 1);
            CentreText(msgRemoved, 0xA0, 0x5A, 0xC, 1);
        }
        CentreText(msgLinkBroken, 0xA0, 0x78, 8, 0);
        CentreText(msgReturningToMenus, 0xA0, 0x84, 8, 0);
        if (linkTimer == 1) {
            raceEndTimer = linkTimer;
        }
        linkFlashFrame += 1;
    }
}
