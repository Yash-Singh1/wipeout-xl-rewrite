#include "functions.h"
#include "racectrl.h"
#include "sfx.h"
#include "portorig.h" /* Port: the original Wipeout's tracks */
#include "portlink.h" /* Port: two players on one machine */
#include "hud.h"
#include "ayrace.h"
#include "hiscores.h"
#include "effects.h"
#include "weapon.h"
#include "raceinit.h"
#include "ghost.h"
#include "trackasm.h"
#include "control.h"
#include "track.h"
#include "portfar.h" /* Port: the far view */
#include "dynam.h"
#include "pad.h"
#include "../../platform/controls.h" /* Port: keyboard and gamepad glyphs and contexts */

/* $gp-relative globals are tentative definitions; the rest are extern. */
extern u_short          startTextureBase;
extern u_short          keyDef[6];
extern WeaponGrid      *weaponGrids;
extern TIMlist         *residentTextureSeq;
extern Shadow          *shadowData;
extern WeaponData      *weaponIndex;
extern WeaponShape     *weaponBars;
extern Texture         *continuesTexture;
extern ShipData        *shipIndex;
extern u_short          savedTextureCount;
extern short            beamCollide;
extern short            advertCount;
extern short            cameraCount;
extern short            neonCount;
extern short            girderCount;
extern short            videoCount;
extern short            zepCount;
extern short            dishCount;
extern short            pylonCount;
extern short            fanCount;
extern short            smokeSlowCount;
extern short            smokeFastCount;
extern short            smokeNumber;
extern short            torchCount;
extern short            torchNumber;
extern short            redBullCount;
extern short            redBullNumber;
extern int              linkFlashFrame;
extern int              attractModeFlag;
extern int              firstTimeIn;
extern int              playCredits;
extern short            trackCycleTimer;
extern int              skycyc;
extern short            raceFlag;
extern int              oneMoreTime;
extern int              masterWait;
extern u_short          prmCount;
extern int              raceElapsed;
extern int              savedUpperY;
extern void           (*prevCameraUpdate)();
extern u_short          sceneTextureCount;
extern CVECTOR          smokeColour;
extern AutoMeshData    *trackCache;
extern int              raceStartVcount;
extern u_short          tempTextureCount;
extern int              checkTimer;
extern HeapBlock       *savedHeap;

extern char          startWadPath[];
extern char          startDroidNames[];
extern char          lightGroupNames[];
extern Object       *startDroidObjects[];
extern ConfigData   *gameData;
extern CheckCell     checkCells[][4][6];
extern POLY_FT4      weaponIcons[];
extern Texture      *weaponIconTextureTable[];
extern Speedo        speedoHud;
extern Object       *lightGroups[];
extern TrackData    *track;
extern int           fastCount;
extern int           fastTexCount;
extern int           slowCount;
extern int           slowTexCount;
extern int           bobCount;
extern Object       *radar[];
extern Object       *neonObject[];
extern Object       *torchLight[];
extern Object       *advert[];
extern Object       *redBull[];
extern Object       *cameraDroids[];
extern Object       *fans[];
extern Object       *smokeFastObjects[];
extern Object       *smokeSlowObjects[];
extern Object       *girderObjects[];
extern Object       *pylons[];
extern Object       *videoObjects[];
extern Object       *prmObjects[];
extern int           fracdamage[];
extern int           shipDamage[];
/* Port: hudDisabled is part of scoreTabData; the uses read it directly. */
extern HiScoreData   scoreTabData;
extern RescueData    rescueData;
/* Port: twoPlayerFlag, twoPlayerMode, twoPlayerPauseStatus are parts of pauseState; the uses read them directly. */
extern int           zeppelinBaseX;
extern int           zeppelinBaseY;
extern int           zeppelinBaseZ;
/* Port: trainMuzzleX, trainMuzzleY, trainMuzzleZ are parts of monoRail; the uses read them directly. */
/* Port: lightDroid is part of shipShapeTable; the uses read it directly. */
extern char          trackPathTable[];
extern char          track1SmokeTextureSequence[];
extern char          sharedTrackTextureSequence[];
extern char          track6TextureSequence[];
extern char          track20TextureSequence[];
extern char          track17TextureSequence[];
extern char          track7TextureSequence[];
extern char          track13TorchTextureSequence[];
extern char          track13TextureSequence[];
extern char          track2ZeppelinNames[];
extern char          track2TextureSequence[];
extern u_char        msgOvertake[];
extern u_char        overtakeMessage[];
extern short         linkTimer;
extern short         linked;
extern short         linkDropped;
extern short         linkMyState;
extern short         linkRemoteState;
extern int           spareMax;
extern u_char        medalDisplay;
extern short         fadeAmount;
extern int           trackNameScroll;
extern char          cameraDroidNames[];
extern char          fanNames[];
extern char          smokeSlowNames[];
extern char          smokeFastNames[];
extern char          redBullNames[];
extern char          advertNames[];
extern char          videoNames[];
extern char          radarNames[];
extern char          neonNames[];
extern char          torchNames[];
extern char          monoRailNames[];
extern char          girderNames[];
extern char          pylonNames[];
extern char          vectorShipName[];
extern char          venomShipName[];
extern char          rapierShipName[];
extern char          phantomShipName[];
extern u_char        undercover;
extern u_char        msgCombatCable[];
extern u_char        msgLinkBroken[];
extern u_char        msgReturningToMenus[];
extern u_char        msgRemoved[];
extern int           writeFlag;
extern short         buttonCount;
extern int           cardNum;
extern int           heading;
extern u_char        menuRecvFlags;
extern int           remoteShip;
extern int           ownShip;
extern int           raceType;
extern int           attractDelayFrames;
extern int           quadIndex;
extern int           raceEndTimer;
extern short         CurrentScreen;
extern u_char        linkRaceDone;
extern u_char        remoteRaceDone;

/* Everything race() keeps on its stack. */
typedef struct RaceLocals {
    TrackData  trackData;       /* the track being raced; `track` points here */
    BLK_FILL  *clearPolys[2];   /* the black fills drawn behind the scene, one per screen */
    Object    *crowdObjects[20]; /* the crowd objects of the scene (InitCrowdSound; no track has more than 7) */
    struct WipeCdStatus cdPosition;   /* CdControlB(CdlGetlocP) result */
    ResetData  resetData;       /* what a restart of the race needs (InitRaceResetData) */
    InGameMenu inGameMenu;      /* polygons of the pause menu */
    short      tileCount;       /* texture tiles of the track (WADLoadTtfFile, AsmGenerateTiles) */
    int        weaponGridCount; /* weapon grids of the track (InitWeaponGrids) */
    short      specialGridCount;
    signed char cdTrack;        /* SetInGameCDTrack: track the CD drive reports */
    int        cdMode;
    signed char viewMode;       /* the two-player pause status seen last */
    short      raceComplete;
    int        crowdCount;      /* crowd objects found by InitCrowdSound */
    u_short    skyIndex;        /* index in prmObjects of the sky model (returned by WADInitScene) */
    int        rainTimer;       /* frames of rain/thunder, 0..255 */
    int        thunderAt;       /* rainTimer value at which the thunder flash started */
    u_char     updateTrails;    /* the trails are updated once more on the first paused frame */
} RaceLocals;

/* Port: the PC version's race loads the Red Bull boards of track 1 only, as one picture (platform/pc.c makes
   alphared.cmp of it) where the PlayStation's sequence for each track holds two frames to alternate; the other
   tracks' boards, the video screens and the torches stay as the track's wad draws them (the PC never animates them). */
static void InitRedBull(char *textureSequence)
{
    if (PortPcData() && gameData->actTrackNum != 1) {
        return;
    }
    InitObject(prmObjects, prmCount, redBull, redBullNames, 4, &redBullCount);
    redBullNumber = TextureTableCount;
    LoadCmpFiles(LoadCompressedTextureSequence(textureSequence));
}

/* Loads the track and runs the race: its frames until it ends, again from the start after each restart. attractMode
   is 1 for the attract mode's race; idleLimit, the frames without input after which a player's ship goes on alone. */
#define CHECKPOINTS (checkCells[gameData->trackNum][gameData->shipClass])
void race(ConfigData *gameData, int attractMode, int idleLimit)
{
    RaceLocals locals;
    WADFILE *wad;
    TexTemplate *tiles;
    int i,j,radius,x,y,z,bob;
    int advance;
    locals.tileCount=0;locals.weaponGridCount=0;locals.cdMode=1;locals.viewMode=2;locals.specialGridCount=0;locals.raceComplete=0;
    InitDynamicMem(reserveHeap,RESERVE_HEAP_SIZE);
    locals.crowdCount=0;
    advertCount=0;cameraCount=0;neonCount=0;girderCount=0;videoCount=0;zepCount=0;dishCount=0;pylonCount=0;fanCount=0;smokeSlowCount=0;smokeFastCount=0;smokeNumber=0;
    fastCount=0;fastTexCount=0;slowCount=0;slowTexCount=0;torchCount=0;torchNumber=0;redBullCount=0;redBullNumber=0;bobCount=0;prmCount=0;
    attractModeFlag=0;firstTimeIn=1;pauseState.confirmed=2;pauseState.confirmChoice=0;checkTimer=0;savedHeap=heap;heap=reserveHeap;savedTextureCount=TextureTableCount;masterWait=0;writeFlag=0x3938700;
    locals.rainTimer=0;locals.thunderAt=0;locals.updateTrails=1;
    if(attractMode==0) raceFlag=1;
    trackCache=DAlloc(reserveHeap,sizeof(AutoMeshData));
    trackCache->triCount=0;trackCache->quadCount=0;trackCache->mode=0;trackCache->active=1;
    locals.clearPolys[0]=BlockFill(0,0,320,SCREEN_HEIGHT,0,0,0);
    locals.clearPolys[1]=BlockFill(0,256,320,SCREEN_HEIGHT,0,0,0);
    SetSkeletonPosition(raceCamera.camPos,0,0,0);SetSkeletonDirection(raceCamera.camPos,0,0,0);
    SetSkeletonPosition(upperSkeleton,0,0,0);SetSkeletonDirection(upperSkeleton,0,0,0);
    cameraPtr=&raceCamera;track=&locals.trackData;tempGameData=gameData;raceCamera.camPos->super=upperSkeleton;
    PortOriginalRaceBegins(); /* Port: an original Wipeout track, or Talon's Reach's own values (portorig.c) */
    getPath(scenePath,gameData->actTrackNum,trackPathTable);
    wad=openwadfile(scenePath);
    WADInitVertices(wad);WADInitFaces(wad);WADInitSections(wad);WADInitViewList(wad);
    tempTextureCount=TextureTableCount;TextureTableCount=PortOriginalLibraryBase(tempTextureCount); /* Port: 0 but for a guest's large library (portorig.c) */
    LoadCmpFiles(WADLoadCompressedTextureSequence(wad));
    tiles=WADLoadTtfFile(wad,&locals.tileCount);
    TextureTableCount=PortOriginalLibraryPlaced(tiles,locals.tileCount,tempTextureCount); /* Port: tempTextureCount but ... (portorig.c) */
    locals.skyIndex=WADInitScene(prmObjects,&prmCount,raceCamera.camPos,gameData,wad);
    sceneTextureCount=TextureTableCount;closewadfile(wad);
    for(j=0;j<6;j++) {
        CHECKPOINTS[j].bonusSeconds=0;
        CHECKPOINTS[j].section=-1;
        CHECKPOINTS[j].done=-1;
    }
    InitTrackTextures(gameData);
    InitWeaponGrids(weaponGrids,track,&locals.weaponGridCount,&locals.specialGridCount,(SpecialGrid *)CHECKPOINTS);
    InitCheckPoints((CheckPoints *)CHECKPOINTS,gameData);
    trackCycleTimer=0;InitTrackCycle();
    InitDroidThings(startDroidObjects,startDroidNames,raceCamera.camPos,(Checkpoint *)CHECKPOINTS,gameData);
    InitDroidThings(lightGroups,lightGroupNames,raceCamera.camPos,(Checkpoint *)CHECKPOINTS,gameData);
    PortOriginalSceneInit(prmObjects,prmCount); /* Port: an original Wipeout track's start gantries, beacons, pumps (portorig.c) */
    InitRaceLine(track,gameData);InitInGameMenu(&locals.inGameMenu);
    locals.crowdCount=InitCrowdSound(prmObjects,prmCount,locals.crowdObjects,locals.crowdCount);
    if(gameData->actTrackNum==1) {
        InitObject(prmObjects,prmCount,cameraDroids,cameraDroidNames,6,&cameraCount);
        InitObject(prmObjects,prmCount,fans,fanNames,3,&fanCount);
        InitObject(prmObjects,prmCount,smokeSlowObjects,smokeSlowNames,6,&smokeSlowCount);
        InitObject(prmObjects,prmCount,smokeFastObjects,smokeFastNames,6,&smokeFastCount);
        smokeNumber=TextureTableCount;LoadCmpFiles(LoadCompressedTextureSequence(track1SmokeTextureSequence));
        InitRedBull(sharedTrackTextureSequence);
        smokeColour.r=64;smokeColour.g=64;smokeColour.b=64;
    }
    if(gameData->actTrackNum==6) {
        InitObject(prmObjects,prmCount,advert,advertNames,6,&advertCount);
        InitObject(prmObjects,prmCount,cameraDroids,cameraDroidNames,6,&cameraCount);
        if(!PortPcData()) InitObject(prmObjects,prmCount,videoObjects,videoNames,6,&videoCount); /* Port: (see InitRedBull) */
        InitRedBull(track6TextureSequence);
    }
    if(gameData->actTrackNum==8) {
        InitObject(prmObjects,prmCount,advert,advertNames,6,&advertCount);
        InitObject(prmObjects,prmCount,cameraDroids,cameraDroidNames,6,&cameraCount);
        InitRedBull(sharedTrackTextureSequence);
    }
    if(gameData->actTrackNum==20) {
        InitObject(prmObjects,prmCount,radar,radarNames,4,&dishCount);
        InitObject(prmObjects,prmCount,cameraDroids,cameraDroidNames,6,&cameraCount);
        InitObject(prmObjects,prmCount,neonObject,neonNames,5,&neonCount);
        InitRedBull(track20TextureSequence);
    }
    if(gameData->actTrackNum==17) {
        InitObject(prmObjects,prmCount,cameraDroids,cameraDroidNames,6,&cameraCount);
        InitRedBull(track17TextureSequence);
    }
    if(gameData->actTrackNum==7) {
        if(!PortPcData()) InitObject(prmObjects,prmCount,videoObjects,videoNames,6,&videoCount); /* Port: (see InitRedBull) */
        InitObject(prmObjects,prmCount,advert,advertNames,6,&advertCount);
        InitRedBull(track7TextureSequence);
    }
    if(gameData->actTrackNum==13) {
        InitWaterFall();
        if(!PortPcData()) { /* Port: (see InitRedBull) */
            InitObject(prmObjects,prmCount,torchLight,torchNames,5,&torchCount);
            torchNumber=TextureTableCount;LoadCmpFiles(LoadCompressedTextureSequence(track13TorchTextureSequence));
        }
        InitRedBull(track13TextureSequence);
    }
    if(gameData->actTrackNum==2) {
        LoadTrain(monoRailObjs,monoRailNames,raceCamera.camPos,posMonoRail);
        InitObject(prmObjects,prmCount,cameraDroids,cameraDroidNames,6,&cameraCount);
        InitObject(prmObjects,prmCount,&zeppelinObj,track2ZeppelinNames,8,&zepCount);
        InitObject(prmObjects,prmCount,girderObjects,girderNames,5,&girderCount);
        InitTrainLights(prmObjects,prmCount,monoRailObjs,pauseState.selection);
        InitRedBull(track2TextureSequence);
        /* the train's flag and its muzzle position (pad10[1..3]) are cleared; the zeppelin is moved 8000 down z,
           where it is based */
        monoRail.index=0;monoRail.pad10[1]=0;monoRail.pad10[2]=0;monoRail.pad10[3]=0;
        zeppelinObj->skeleton->relative.t[2]-=8000;
        zeppelinBaseX=zeppelinObj->skeleton->relative.t[0];zeppelinBaseY=zeppelinObj->skeleton->relative.t[1];zeppelinBaseZ=zeppelinObj->skeleton->relative.t[2];
        InitObject(prmObjects,prmCount,pylons,pylonNames,5,&pylonCount);
        InitMonoData(pylons,pylonCount,&monoRail);
    }
    buildGameKeyPad(padBuf1,playerControl);initShipData(track->sections,shipIndex,gameData);
    if(gameData->actTrackNum==13) LoadBirdGFX();
    InitRescueDroid(&rescueData,shipShapeTable,&shipIndex[ownShip]);
    CameraUpdate=UpdateCameraRaceStart;InitCamera(&raceCamera,shipIndex[ownShip].nearTrkSect);
    spareMax=0;InitTrails();AsmGenerateTiles(tiles,locals.tileCount);
    InitRaceResetData(&locals.resetData,shipIndex,gameData,weaponIndex,weaponBars,&rescueData,shipShapeTable,(CheckPoints *)CHECKPOINTS);
    for(j=0;j<15;j++) {shipDamage[j]=0;fracdamage[j]=0;}
    particleSlots=InitParticle();InitExplode();InitRing();InitScoreTabData(&scoreTabData);InitialiseBumps();
    if(raceType==RACE_ARCADE) {
        switch(gameData->shipClass) {
        case 0: LoadModel(teamShip,vectorShipName);break;
        case 1: LoadModel(teamShip,venomShipName);break;
        case 2: LoadModel(teamShip,rapierShipName);break;
        case 3: LoadModel(teamShip,phantomShipName);break;
        }
    } else if(raceType==RACE_CHALLENGE) {
        if(gameData->venomComplete) LoadModel(teamShip,rapierShipName);
        else if(gameData->rapierComplete) LoadModel(teamShip,phantomShipName);
    }
    trackNameScroll=480;gameData->raceInfo=0;medalDisplay=0;
    PortLinkRaceView(1); /* Port: a linked race splits the window between the two players (portlink.c) */
    for(;;) {
        Restart(&locals.resetData);InitRaceData(&pauseState);
        if(attractMode!=1) attractDelayFrames=0;
        InitAttractMode(attractMode,shipIndex);gameData->complete=0;SetUpFirstCheckPoint();
        /* Port: the droid is drawn before the camera moves, with the last frame's view: on a restart that was the
           paused race's, and the first frame showed it huge beside the track. The start's view is set first. */
        if(CameraUpdate==UpdateCameraRaceStart && !PortTracing()) CameraUpdate(&raceCamera,&shipIndex[ownShip],&rescueData);
        do {
            if(portPal) GetJoyPadPacketOnVsync(pauseState.stage); /* the European program: the link slave waits a field while a two-player pause is requested */
            buildGameKeyPad(padBuf1,playerControl);buildFixedPad(padBuf1);
            PortRaceControls((shipIndex[ownShip].attr&RACING) && pauseState.selection==-1,keyDef); /* Port: the keyboard steers the ship only while racing */
            if(raceType==RACE_TIME_TRIAL) RecordKeypresses(shipIndex);
            UpdateCheatSequence(0);ClearOTagR(OT[CurrentScreen],8192);ClearOTagR(OT2[CurrentScreen],1600);
            PortFade(1); /* Port: distant geometry fades out (portfar.c) */
            buttonCount=0;quadIndex=CurrentScreen*300;shipClassIndex=gameData->shipClass;clipLevel=gameData->trackNum;
            SetInGameCDTrack(&locals.cdTrack,&locals.cdPosition,gameData,&locals.cdMode);
            if(gameType!=GAME_SINGLE_PLAYER) doSerialGame(gameData,shipIndex);
            ShowLinkBroken();writeFlag=0;
            CheckShipDestroyed(shipIndex,gameData,&attractMode,idleLimit);PlayCredits(attractMode);
            CheckGamePause(shipIndex,&pauseState.selection,gameData,&keysPressed,&pauseState.musicPaused);
            Check2PlayerPause(pauseState.stage,&locals.viewMode,&pauseState.musicPaused);
            PerformPauseOperations(&pauseState,(struct SelectBox *)&locals.inGameMenu,gameData);
            InitiateTwoPlayerPause(&pauseState);
            if(!raceEndTimer) Perform2PlayerPauseCommand(&pauseState);
            if((pauseState.selection==-1 || oneMoreTime) && !masterWait && !linkDropped) {
                oneMoreTime=0;CheckForButtonPress();UpdateShips(shipIndex,weaponBars,shipShapeTable,weaponIndex);
                rescueData.droidUpdate(&rescueData,&shipIndex[ownShip],shipShapeTable);
                CameraUpdate(&raceCamera,&shipIndex[ownShip],&rescueData);
                if(shipIndex[ownShip].attr&RACING) {
                    if(shipIndex[ownShip].weapon==WEAPON_MISSILE || shipIndex[ownShip].weapon==WEAPON_ELECTROBOLT) {
                        MissileLock(ownShip,shipIndex);if(shipIndex[ownShip].weapon==WEAPON_MISSILE) RearMissileLock(ownShip,shipIndex);
                    }
                    ShakeScreen();
                }
                if(gameType!=GAME_SINGLE_PLAYER && (shipIndex[remoteShip].attr&RACING)) {
                    if(shipIndex[remoteShip].weapon==WEAPON_MISSILE || shipIndex[remoteShip].weapon==WEAPON_ELECTROBOLT) {
                        MissileLock(remoteShip,shipIndex);if(shipIndex[remoteShip].weapon==WEAPON_MISSILE) RearMissileLock(remoteShip,shipIndex);
                    }
                }
                UpdateWeapons(shipIndex,weaponBars,weaponIndex);CheckPitShips();
                if(pauseState.selection==-1 && !linkDropped) {AdjustEngineSound(&shipIndex[ownShip],&raceCamera);AdjustCrowdSound(locals.crowdObjects,locals.crowdCount);}
                TrackSpecificFunctions();ColourDroidLights(shipShapeTable[30]);AddEffects(weaponIndex,shipIndex);raceElapsed=GsGetVcount()-raceStartVcount;
                if(raceType!=RACE_TIME_TRIAL && gameData->weaponsOn) CycleWeaponGrids(shipIndex,(struct WeaponGridZone *)weaponGrids,locals.weaponGridCount);
                else if(!gameData->weaponsOn && raceType==RACE_CHALLENGE) CycleWeaponGrids(shipIndex,(struct WeaponGridZone *)weaponGrids,locals.weaponGridCount);
            }
            masterWait=0;
            if(!linkSendFlag) {
                if(attractDelayFrames>0) {
                    attractDelayFrames--;
                    if((attractMode==1 && CheckAttractOver(gameData)) || !attractDelayFrames) raceEndTimer=2;
                }
            } else RaceComplete(&locals.inGameMenu,&locals.resetData,gameData,&locals.raceComplete,&scoreTabData,shipIndex);
            if(raceEndTimer>0) {
                if(linked) {
                    if(linkRemoteState==4) raceEndTimer=0;
                    else if(linkRemoteState==3) {linkMyState=4;raceEndTimer=1;playCredits=1;}
                } else {raceEndTimer--;playCredits=1;}
                if(!raceEndTimer) {
                    PortLinkRaceView(0); /* Port: the window is whole again (portlink.c) */
                    PortFade(0);
                    QuitGame();
                    return;
                }
            }
            if(scoreTabData.nameEntryActive!=1 && !raceEndTimer) {
                UpdateRaceHud(shipIndex,&speedoHud,pauseState.selection,gameData,&scoreTabData);
                if(shipIndex[ownShip].update==UpdatePlayerShip || shipIndex[ownShip].update==UpdateRescueDroid || (shipIndex[ownShip].update==UpdateDroneShip && shipIndex[ownShip].autopilotCount>0)) {
                    if((shipIndex[ownShip].combatAttr&0x8004)!=0x8004) AlterCheckPoint((CheckPoints *)CHECKPOINTS,shipIndex,pauseState.selection,gameData);
                }
            }
            if(gameData->weaponsOn || raceType==RACE_TIME_TRIAL || raceType==RACE_CHALLENGE) {
                if((shipIndex[ownShip].attr&RACING) && ownShip<12) {
                    DrawWeaponIcons(weaponIcons,weaponIconTextureTable,&shipIndex[ownShip]);AddPrim(OT[CurrentScreen],&weaponIconPrims[CurrentScreen]);
                }
            }
            if(gameData->actTrackNum==2 && raceType!=RACE_ARCADE_LINK && (unsigned int)(shipIndex[ownShip].nearTrkSect->secNo-200)<76) {
                MoveMonoRail(monoRailObjs,6,&monoRail,shipIndex,pauseState.selection);InitTrainLights(prmObjects,prmCount,monoRailObjs,pauseState.selection);
            }
            if(gameData->actTrackNum==6 || gameData->actTrackNum==7) Video(videoObjects,videoCount);
            {
                char *trackPackets = nextPrim.ptr; /* Port: the track's packets, for the shadows (libgpu.c) */
                if(!(shipIndex[ownShip].attr&RACING) || shipTargetIndex[ownShip]>=0) DrawAllTrack(&raceCamera,trackCache);
                else DrawNewTrack(&raceCamera,trackCache);
                PortTrackPackets(trackPackets, nextPrim.ptr);
                PortTrackPools();
            }
            DrawShadow(shipShapeTable,shipIndex,shadowData);
            DrawShips(shipIndex,shipShapeTable,ownShip,raceCamera,pauseState.selection);
            if(!medalDisplay) DrawLandscape(&shipIndex[ownShip],&raceCamera);
            if(!medalDisplay) PortFarDraw(&raceCamera,shipIndex,shipShapeTable); /* Port: the rest of the track in view (portfar.h) */
            PortWideScene(); /* Port: widescreen shows the race wide */
            PortInfoRaceDrawn(); /* Port: ... and the frame recorder's info.json says it is a race (portinfo.c) */
            if(medalDisplay) PortHudInPlace(); /* Port: ... the results' text and medal where the game puts them */
            /* the sky is drawn around the camera; on track 17 it also bobs with the camera's height */
            prmObjects[(short)locals.skyIndex]->skeleton->update=1;
            savedUpperY=raceCamera.camPos->super->relative.t[1];
            if(gameData->actTrackNum==17) {
                if(raceCamera.camPos->relative.t[1]>0) {
                    bob=rsin(skycyc);
                    raceCamera.camPos->super->relative.t[1]+=1024+(((raceCamera.camPos->relative.t[1]>>3)*bob)>>12);
                }
                if(pauseState.selection==-1 && (unsigned int)(raceCamera.section->secNo-303)>=26) skycyc=(skycyc+96)&4095;
            }
            PortFade(0); /* Port: the sky is not faded with distance: it is behind everything */
            AsmDrawObjectFixed(prmObjects[(short)locals.skyIndex],raceCamera.camPos,8191);
            PortFade(1);
            raceCamera.camPos->super->relative.t[1]=savedUpperY;
            if(prevCameraUpdate!=CameraUpdate) {UnsetParticles(particleSlots);prevCameraUpdate=CameraUpdate;}
            AsmDrawParticles(particleSlots,raceCamera.camPos,pauseState.selection+1);DrawExplode();
            if(gameData->actTrackNum==13) DrawBirds();
            DrawRing();if(sillyShips) UpdateSharkModel();
            if(gameType!=GAME_SINGLE_PLAYER && !(shipIndex[remoteShip].combatAttr&DESTROYED)) DrawLinkSign(shipIndex);
            if(pauseState.selection<0) {
                UpdateShipTrails(shipIndex,attractDelayFrames);locals.updateTrails=1;
                if(shipTargetIndex[ownShip]==100) shipIndex[ownShip].attr&=~VIEW_IN;
                if((shipTargetIndex[ownShip]&7)<4 && shipTargetIndex[ownShip]>=16 && !(shipIndex[ownShip].combatAttr&4)) {
                    CentreText(overtakeMessage,15,64,12,1);CentreText(msgOvertake,45,128,8,0);
                    if(!(shipTargetIndex[ownShip]&7)) PlayNote(48,255,15,15,256);
                }
                /* the ships being destroyed: shipTargetIndex counts down their last frames */
                for(i=0;i<15;i++) {
                    if(!(shipIndex[i].combatAttr&DESTROYED) && shipTargetIndex[i]>0) {
                        if((i==ownShip && shipTargetIndex[ownShip]>=16) || (i==remoteShip && shipTargetIndex[remoteShip]>=16)) {
                            /* the players' own ships drift to a stop */
                            ShipData *ship=shipIndex;
                            ship[i].ppivot.vx+=ship[i].vpivot.vx>>6;
                            ship[i].ppivot.vy+=ship[i].vpivot.vy>>6;
                            ship[i].ppivot.vz+=ship[i].vpivot.vz>>6;
                            ship[i].vpivot.vx-=ship[i].vpivot.vx>>3;
                            ship[i].vpivot.vy-=ship[i].vpivot.vy>>3;
                            ship[i].vpivot.vz-=ship[i].vpivot.vz>>3;
                            ship[i].vhdg=0;
                            ship[i].roll+=(ship[i].vpivot.vx+ship[i].vpivot.vz)>>5;
                        }
                        if(shipTargetIndex[i]<15) {
                            radius=(15-shipTargetIndex[i])*64;
                            /* the European program spaces the explosions by its own tenth of a second (5 frames) */
                            if(portPal ? shipTargetIndex[i]%5==1 : (shipTargetIndex[i]&7)==7) {
                                x=shipIndex[i].ppivot.vx+newRand()%radius-(radius>>1);
                                y=shipIndex[i].ppivot.vy-newRand()%127;
                                z=shipIndex[i].ppivot.vz+newRand()%radius-(radius>>1);
                                AddExplode(x,y,z,-1);
                            }
                            if((shipTargetIndex[i]/6)*6==shipTargetIndex[i]-4) {
                                x=shipIndex[i].ppivot.vx+newRand()%radius-(radius>>1);
                                y=shipIndex[i].ppivot.vy-newRand()%127;
                                z=shipIndex[i].ppivot.vz+newRand()%radius-(radius>>1);
                                AddSmokeExplode(x,y,z,-1);
                            }
                            radius=(20-shipTargetIndex[i])*64;
                            if(shipTargetIndex[i]%FRAMES_PER_TENTH==1) {
                                x=shipIndex[i].ppivot.vx+newRand()%radius-(radius>>1);
                                y=shipIndex[i].ppivot.vy-newRand()%127;
                                z=shipIndex[i].ppivot.vz+newRand()%radius-(radius>>1);
                                AddSpurtExplode(x,y,z,-1);
                            }
                        }
                        if(shipTargetIndex[i]==14) {
                            PlayDirectionalNote(5,0,40,shipIndex[i].ppivot);
                            PlayDirectionalNote(5,0,40,shipIndex[i].ppivot);
                            PlayDirectionalNote(5,-64,40,shipIndex[i].ppivot);
                            PlayDirectionalNote(5,-128,40,shipIndex[i].ppivot);
                            PlayDirectionalNote(5,-128,40,shipIndex[i].ppivot);
                            PlayDirectionalNote(5,-192,40,shipIndex[i].ppivot);
                        }
                        if((newRand()&3)==1 && shipTargetIndex[i]>=16) AddSmallSmokeExplode(shipIndex[i].ppivot.vx-(shipIndex[i].unitVecNose.vx>>4),shipIndex[i].ppivot.vy-(shipIndex[i].unitVecNose.vy>>4),shipIndex[i].ppivot.vz-(shipIndex[i].unitVecNose.vz>>4),-1);
                        if((shipTargetIndex[i]&3)==1 && shipTargetIndex[i]>=16) {
                            x=(newRand()&511)-(shipIndex[i].unitVecNose.vx>>5)-256;
                            z=(newRand()&511)-(shipIndex[i].unitVecNose.vz>>5)-256;
                            AddSmallSpurtExplode(x,-(shipIndex[i].unitVecNose.vy>>5)+128,z,i);
                        }
                        shipTargetIndex[i]--;
                        if(!shipTargetIndex[i]) {
                            if(i!=ownShip && i!=remoteShip) {shipIndex[i].attr&=~RACING;PlayNote(51,480,15,15,256);}
                            shipIndex[i].combatAttr|=DESTROYED;
                        }
                    }
                    if((i==ownShip && (shipIndex[i].combatAttr&DESTROYED)) || (i==remoteShip && (shipIndex[i].combatAttr&DESTROYED))) {
                        if((newRand()&31)<3) AddSmallSmokeExplode(shipIndex[i].ppivot.vx,shipIndex[i].nearTrkSect->centre.vy+128,shipIndex[i].ppivot.vz,-1);
                    }
                }
                /* rain and lightning on tracks 2 and 7 (and with the rain cheat), snow on track 8 */
                if(gameData->actTrackNum==2 || gameData->actTrackNum==7 || cheatRain) {
                    AddRain(particleSlots,1,(VECTOR *)cameraPtr->camPos->relative.t); /* Port: the camera position is the first three ints of the skeleton's relative translation */
                    advance=locals.rainTimer+1;locals.rainTimer=advance&255;
                    if(!undercover && !(newRand()&3) && locals.rainTimer<32 && !(advance&3)) {
                        if(fadeAmount<228) fadeAmount=228;
                        locals.thunderAt=locals.rainTimer;
                    }
                    if(fadeAmount==220) fadeAmount=118;
                    if(fadeAmount==110) fadeAmount=0;
                    advance=locals.thunderAt+32;
                    if(locals.rainTimer==advance) PlayNote(35,380,15,15,128);
                    if(advance<locals.rainTimer && locals.rainTimer<64 && !(newRand()&3) && !(locals.rainTimer&3)) PlayNote(35,(short)(412-locals.rainTimer),15,15,320-locals.rainTimer);
                }
                if(gameData->actTrackNum==8) AddSnow(particleSlots,20,(VECTOR *)cameraPtr->camPos->relative.t);
            } else {
                if(locals.updateTrails) UpdateShipTrails(shipIndex,attractDelayFrames);
                locals.updateTrails=0;
            }
            resetMiscFx();DrawShipTrails(cameraPtr);
            if(!linkDropped && PortOriginalTrackIs64()) PortOriginalSceneUpdate(pauseState.selection!=-1); /* Port: a Wipeout 64 track's animated scenery (its droids are XL's) */
            if(!linkDropped && PortOriginalLooks()) PortOriginalSceneUpdate(pauseState.selection!=-1); /* Port: ... its own scenery, not XL's droids */
            else if(!linkDropped) {
                UpdateStartDroid(startDroidObjects,shipIndex,(DroidData *)CHECKPOINTS,startTextureBase,gameData,pauseState.selection);
                UpdateLightStart(lightGroups,shipIndex,(DroidData *)CHECKPOINTS,startTextureBase,gameData,pauseState.selection);
            }
            if(playCredits) {ClearOTagR(OT[CurrentScreen],8192);ClearOTagR(OT2[CurrentScreen],1600);}
            AddPrim(OT[CurrentScreen]+8191,locals.clearPolys[CurrentScreen]);
            PortTagPrims(locals.clearPolys[CurrentScreen],locals.clearPolys[CurrentScreen]+1,"screen clear",NULL,CurrentScreen); /* Port: the frame recorder */
            if(portPal && !pauseState.stage && gameType==GAME_LINK_SLAVE) VSync(0); /* the European program: the link slave waits another field while a two-player pause is requested */
            /* the fields the last frame took (Swap); a link slave uses the master's count */
            if(gameType!=GAME_LINK_SLAVE) advance=menuSendFlags;else advance=menuRecvFlags;
            cardNum-=advance*30;heading-=advance*15;
            Swap();firstTimeIn=0;UpdateNotes(0);
            PortAutoSave(gameData,0); /* Port: the game saves itself: records, medals (portsave.c) */
        } while(!linkRaceDone || (gameType!=GAME_SINGLE_PLAYER && !remoteRaceDone));
        attractMode=0;
    }
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
    raceCamera.camPos = rootSkeleton;
    SetSkeletonPosition(rootSkeleton, 0, 0, 0);
    SetSkeletonDirection(raceCamera.camPos, 0, 0, 0);
    subSkeleton = NewSkeleton();
    upperSkeleton = subSkeleton;
    SetSkeletonPosition(subSkeleton, 0, 0, 0);
    SetSkeletonDirection(upperSkeleton, 0, 0, 0);
    raceCamera.camPos->super = upperSkeleton;
    shadowData = InitShadow();
    shipData = DAlloc(heap, 17 * sizeof(ShipData)); /* Port: was 0xFF0 = 17 * 240, the console's ShipData size */
    /* Port: no track is loaded yet, so `track` is still null; the console read the word at address 0x14 here.
       loadShipData never uses the value. */
    sections = track ? track->sections : 0;
    shipIndex = shipData;
    loadShipData(sections, shipData, shipShapeTable, raceCamera.camPos);
    textureWad = openwadfilenvc(startWadPath);
    InitSpeedo(&speedoHud);
    InitLinkUpSign();
    continuesTexture = WADLoadTexture(textureWad, 1);
    startTextureBase = TextureTableCount;
    textureSequence = WADLoadCompressedTextureSequence(textureWad);
    residentTextureSeq = textureSequence;
    LoadCmpFiles(textureSequence);
    InitWeaponIcons(weaponIcons, weaponIconTextureTable);
    weaponGrids = DAlloc(heap, 64 * sizeof(WeaponGrid) + 32); /* Port: was 0x420 = 64 * 16 + 32 */
    weaponBars = DAlloc(heap, 31 * sizeof(WeaponShape) + 32); /* Port: was 0x118 = 31 * 8 + 32 */
    weaponPool = DAlloc(heap, 30 * sizeof(WeaponData) + 32); /* Port: was 0x890 = 30 * 72 + 32 */
    weaponIndex = weaponPool;
    initWeapons(weaponBars, raceCamera.camPos, weaponPool);
    LoadExplodeGFX();
    LoadStartDroid(startDroidObjects, startDroidNames, raceCamera.camPos);
    LoadStartDroid(lightGroups, lightGroupNames, raceCamera.camPos);
}


void racetidy(void)
{
    heap = mainheapcopy;
    InitDynamicMem(reserveHeap, RESERVE_HEAP_SIZE);
    StartControlledSound();
    TextureTableCount = savedTextureCount;
}


/* Runs the animation and moving parts that belong to the current track. */
void TrackSpecificFunctions(void)
{
    u_char trackNumber;

    RedBullAnimate(redBull, redBullCount, 0x14, redBullNumber, PortPcData() ? 1 : 2); /* Port: (see InitRedBull) */
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
        MoveRadar(radar, dishCount);
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
        MoveZeppelin(&zeppelinObj, zepCount, shipIndex);
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
