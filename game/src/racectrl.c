#include "include_asm_g8.h"
#include "functions.h"
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
    int        noWeaponGrids;   /* weapon grids of the track (InitWeaponGrids) */
    short      specialCount;
    signed char cdTrack;        /* SetInGameCDTrack: track the CD drive reports */
    int        cdMode;
    signed char viewMode;       /* the two-player pause status seen last */
    short      raceComplete;
    int        crowdSample;     /* objects found by InitCrowdSound */
    u_short    skyIndex;        /* index in prmObjects of the sky model (returned by WADInitScene) */
    int        rainTimer;       /* frames of rain/thunder, 0..255 */
    int        thunderAt;       /* rainTimer value at which the thunder flash started */
    u_char     firstTime;
} RaceLocals;

#define CHECKPOINTS (checkCells[gameData->trackNum][gameData->shipClass])
void race(ConfigData *gameData, int context, int scene)
{
    RaceLocals locals;
    WADFILE *wad;
    TexTemplate *tiles;
    int i,j,radius,x,y,z,bob;
    int advance;
    int one=1; int two=2;
    locals.tileCount=0;locals.noWeaponGrids=0;locals.cdMode=one;locals.viewMode=two;locals.specialCount=0;locals.raceComplete=0;
    InitDynamicMem(reserveHeap,RESERVE_HEAP_SIZE);
    locals.crowdSample=0;
    advertCount=0;cameraCount=0;neonCount=0;girderCount=0;videoCount=0;zepCount=0;dishCount=0;pylonCount=0;fanCount=0;smokeSlowCount=0;smokeFastCount=0;smokeNumber=0;
    fastCount=0;fastTexCount=0;slowCount=0;slowTexCount=0;torchCount=0;torchNumber=0;redBullCount=0;redBullNumber=0;bobCount=0;prmCount=0;
    attractModeFlag=0;firstTimeIn=one;pauseState.confirmed=two;pauseState.confirmChoice=0;checkTimer=0;savedHeap=heap;heap=reserveHeap;savedTextureCount=TextureTableCount;masterWait=0;writeFlag=0x3938700;
    locals.rainTimer=0;locals.thunderAt=0;locals.firstTime=1;
    if(context==0) raceFlag=1;
    trackCache=DAlloc(reserveHeap,sizeof(AutoMeshData));
    trackCache->triCount=0;trackCache->quadCount=0;trackCache->mode=0;trackCache->active=1;
    locals.clearPolys[0]=BlockFill(0,0,320,240,0,0,0);
    locals.clearPolys[1]=BlockFill(0,256,320,240,0,0,0);
    SetSkeletonPosition(raceCamera.camPos,0,0,0);SetSkeletonDirection(raceCamera.camPos,0,0,0);
    SetSkeletonPosition(upperSkeleton,0,0,0);SetSkeletonDirection(upperSkeleton,0,0,0);
    cameraPtr=&raceCamera;track=&locals.trackData;tempGameData=gameData;raceCamera.camPos->super=upperSkeleton;
    getPath(scenePath,gameData->actTrackNum,trackPathTable);
    wad=openwadfile(scenePath);
    WADInitVertices(wad);WADInitFaces(wad);WADInitSections(wad);WADInitViewList(wad);
    tempTextureCount=TextureTableCount;TextureTableCount=0;
    LoadCmpFiles(WADLoadCompressedTextureSequence(wad));
    tiles=WADLoadTtfFile(wad,&locals.tileCount);
    TextureTableCount=tempTextureCount;
    locals.skyIndex=WADInitScene(prmObjects,&prmCount,raceCamera.camPos,gameData,wad);
    sceneTextureCount=TextureTableCount;closewadfile(wad);
    for(j=0;j<6;j++) {
        CHECKPOINTS[j].bonusSeconds=0;
        CHECKPOINTS[j].section=-1;
        CHECKPOINTS[j].done=-1;
    }
    InitTrackTextures(gameData);
    InitWeaponGrids(weaponGrids,track,&locals.noWeaponGrids,&locals.specialCount,(SpecialGrid *)CHECKPOINTS);
    InitCheckPoints((CheckPoints *)CHECKPOINTS,gameData);
    trackCycleTimer=0;InitTrackCycle();
    InitDroidThings(startDroidObjects,startDroidNames,raceCamera.camPos,(Checkpoint *)CHECKPOINTS,gameData);
    InitDroidThings(lightGroups,lightGroupNames,raceCamera.camPos,(Checkpoint *)CHECKPOINTS,gameData);
    InitRaceLine(track,gameData);InitInGameMenu(&locals.inGameMenu);
    { int crowd=InitCrowdSound(prmObjects,prmCount,locals.crowdObjects,locals.crowdSample);
    unsigned char currentTrack=gameData->actTrackNum;
    locals.crowdSample=crowd;
    if(currentTrack==1) {
        InitObject(prmObjects,prmCount,cameraDroids,cameraDroidNames,6,&cameraCount);
        InitObject(prmObjects,prmCount,fans,fanNames,3,&fanCount);
        InitObject(prmObjects,prmCount,smokeSlowObjects,smokeSlowNames,6,&smokeSlowCount);
        InitObject(prmObjects,prmCount,smokeFastObjects,smokeFastNames,6,&smokeFastCount);
        smokeNumber=TextureTableCount;LoadCmpFiles(LoadCompressedTextureSequence(track1SmokeTextureSequence));
        InitObject(prmObjects,prmCount,redBull,redBullNames,4,&redBullCount);
        redBullNumber=TextureTableCount;LoadCmpFiles(LoadCompressedTextureSequence(sharedTrackTextureSequence));
        smokeColour.r=64;smokeColour.g=64;smokeColour.b=64;
    }
    }
    if(gameData->actTrackNum==6) {
        InitObject(prmObjects,prmCount,advert,advertNames,6,&advertCount);
        InitObject(prmObjects,prmCount,cameraDroids,cameraDroidNames,6,&cameraCount);
        InitObject(prmObjects,prmCount,redBull,redBullNames,4,&redBullCount);
        InitObject(prmObjects,prmCount,videoObjects,videoNames,6,&videoCount);
        redBullNumber=TextureTableCount;LoadCmpFiles(LoadCompressedTextureSequence(track6TextureSequence));
    }
    if(gameData->actTrackNum==8) {
        InitObject(prmObjects,prmCount,advert,advertNames,6,&advertCount);
        InitObject(prmObjects,prmCount,cameraDroids,cameraDroidNames,6,&cameraCount);
        InitObject(prmObjects,prmCount,redBull,redBullNames,4,&redBullCount);
        redBullNumber=TextureTableCount;LoadCmpFiles(LoadCompressedTextureSequence(sharedTrackTextureSequence));
    }
    if(gameData->actTrackNum==20) {
        InitObject(prmObjects,prmCount,radar,radarNames,4,&dishCount);
        InitObject(prmObjects,prmCount,cameraDroids,cameraDroidNames,6,&cameraCount);
        InitObject(prmObjects,prmCount,neonObject,neonNames,5,&neonCount);
        InitObject(prmObjects,prmCount,redBull,redBullNames,4,&redBullCount);
        redBullNumber=TextureTableCount;LoadCmpFiles(LoadCompressedTextureSequence(track20TextureSequence));
    }
    if(gameData->actTrackNum==17) {
        InitObject(prmObjects,prmCount,cameraDroids,cameraDroidNames,6,&cameraCount);
        InitObject(prmObjects,prmCount,redBull,redBullNames,4,&redBullCount);
        redBullNumber=TextureTableCount;LoadCmpFiles(LoadCompressedTextureSequence(track17TextureSequence));
    }
    if(gameData->actTrackNum==7) {
        InitObject(prmObjects,prmCount,videoObjects,videoNames,6,&videoCount);
        InitObject(prmObjects,prmCount,redBull,redBullNames,4,&redBullCount);
        InitObject(prmObjects,prmCount,advert,advertNames,6,&advertCount);
        redBullNumber=TextureTableCount;LoadCmpFiles(LoadCompressedTextureSequence(track7TextureSequence));
    }
    if(gameData->actTrackNum==13) {
        InitWaterFall();
        InitObject(prmObjects,prmCount,torchLight,torchNames,5,&torchCount);
        torchNumber=TextureTableCount;LoadCmpFiles(LoadCompressedTextureSequence(track13TorchTextureSequence));
        InitObject(prmObjects,prmCount,redBull,redBullNames,4,&redBullCount);
        redBullNumber=TextureTableCount;LoadCmpFiles(LoadCompressedTextureSequence(track13TextureSequence));
    }
    if(gameData->actTrackNum==2) {
        LoadTrain(monoRailObjs,monoRailNames,raceCamera.camPos,posMonoRail);
        InitObject(prmObjects,prmCount,cameraDroids,cameraDroidNames,6,&cameraCount);
        InitObject(prmObjects,prmCount,&zeppelinObj,track2ZeppelinNames,8,&zepCount);
        InitObject(prmObjects,prmCount,girderObjects,girderNames,5,&girderCount);
        InitTrainLights(prmObjects,prmCount,monoRailObjs,pauseState.selection);
        InitObject(prmObjects,prmCount,redBull,redBullNames,4,&redBullCount);
        redBullNumber=TextureTableCount;LoadCmpFiles(LoadCompressedTextureSequence(track2TextureSequence));
        { Object *trainObject=zeppelinObj;
        monoRail.index=0;monoRail.pad10[1]=0;monoRail.pad10[2]=0;monoRail.pad10[3]=0;
        { Skeleton *trainSkeleton=trainObject->skeleton; trainSkeleton->relative.t[2]-=8000; } }
        { Object *trainPosition=zeppelinObj; zeppelinBaseX=trainPosition->skeleton->relative.t[0];zeppelinBaseY=trainPosition->skeleton->relative.t[1];zeppelinBaseZ=trainPosition->skeleton->relative.t[2]; }
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
    if(raceType==0) {
        switch(gameData->shipClass) {
        case 0: LoadModel(teamShip,vectorShipName);break;
        case 1: LoadModel(teamShip,venomShipName);break;
        case 2: LoadModel(teamShip,rapierShipName);break;
        case 3: LoadModel(teamShip,phantomShipName);break;
        }
    } else if(raceType==4) {
        if(gameData->venomComplete) LoadModel(teamShip,rapierShipName);
        else {
            goto checkRapier;
quit:
            QuitGame();
            return;
checkRapier:
            if(gameData->rapierComplete) LoadModel(teamShip,phantomShipName);
        }
    }
    one=1;
    trackNameScroll=480;gameData->raceInfo=0;medalDisplay=0;
restart:
    Restart(&locals.resetData);InitRaceData(&pauseState);
    if(context!=one) attractDelayFrames=0;
    InitAttractMode(context,shipIndex);gameData->complete=0;SetUpFirstCheckPoint();
frame:
    buildGameKeyPad(padBuf1,playerControl);buildFixedPad(padBuf1);
    PortRaceControls((shipIndex[ownShip].attr&RACING) && pauseState.selection==-1,keyDef); /* Port: the keyboard steers the ship only while racing */
    if(raceType==one) RecordKeypresses(shipIndex);
    UpdateCheatSequence(0);ClearOTagR(OT[CurrentScreen],8192);ClearOTagR(OT2[CurrentScreen],1600);
    buttonCount=0;quadIndex=CurrentScreen*300;shipClassIndex=gameData->shipClass;clipLevel=gameData->trackNum;
    SetInGameCDTrack(&locals.cdTrack,&locals.cdPosition,gameData,&locals.cdMode);
    if(gameType!=2) doSerialGame(gameData,shipIndex);
    ShowLinkBroken();writeFlag=0;
    CheckShipDestroyed(shipIndex,gameData,&context,scene);PlayCredits(context);
    CheckGamePause(shipIndex,&pauseState.selection,gameData,&keysPressed,&pauseState.musicPaused);
    Check2PlayerPause(pauseState.stage,&locals.viewMode,&pauseState.musicPaused);
    PerformPauseOperations(&pauseState,(struct SelectBox *)&locals.inGameMenu,gameData);
    InitiateTwoPlayerPause(&pauseState);
    if(!raceEndTimer) Perform2PlayerPauseCommand(&pauseState);
    if((pauseState.selection==-1 || oneMoreTime) && !masterWait && !linkDropped) {
        oneMoreTime=0;CheckForButtonPress();UpdateShips(shipIndex,weaponBars,shipShapeTable,weaponIndex);
        rescueData.droidUpdate(&rescueData,&shipIndex[ownShip],shipShapeTable);
        CameraUpdate(&raceCamera,&shipIndex[ownShip],&rescueData);
        if(shipIndex[ownShip].attr&64) {
            if(shipIndex[ownShip].weapon==2 || shipIndex[ownShip].weapon==5) {
                MissileLock(ownShip,shipIndex);if(shipIndex[ownShip].weapon==2) RearMissileLock(ownShip,shipIndex);
            }
            ShakeScreen();
        }
        if(gameType!=2 && (shipIndex[remoteShip].attr&64)) {
            if(shipIndex[remoteShip].weapon==2 || shipIndex[remoteShip].weapon==5) {
                MissileLock(remoteShip,shipIndex);if(shipIndex[remoteShip].weapon==2) RearMissileLock(remoteShip,shipIndex);
            }
        }
        UpdateWeapons(shipIndex,weaponBars,weaponIndex);CheckPitShips();
        if(pauseState.selection==-1 && !linkDropped) {AdjustEngineSound(&shipIndex[ownShip],&raceCamera);AdjustCrowdSound(locals.crowdObjects,locals.crowdSample);}
        TrackSpecificFunctions();ColourDroidLights(shipShapeTable[30]);AddEffects(weaponIndex,shipIndex);raceElapsed=GsGetVcount()-raceStartVcount;
        if(raceType!=one && gameData->weaponsOn) CycleWeaponGrids(shipIndex,(struct WeaponGridZone *)weaponGrids,locals.noWeaponGrids);
        else if(!gameData->weaponsOn && raceType==4) CycleWeaponGrids(shipIndex,(struct WeaponGridZone *)weaponGrids,locals.noWeaponGrids);
    }
    masterWait=0;
    if(!linkSendFlag) {
        if(attractDelayFrames>0) {
            attractDelayFrames--;
            if((context==one && CheckAttractOver(gameData)) || !attractDelayFrames) raceEndTimer=2;
        }
    } else RaceComplete(&locals.inGameMenu,&locals.resetData,gameData,&locals.raceComplete,&scoreTabData,shipIndex);
    if(raceEndTimer>0) {
        if(linked) {
            if(linkRemoteState==4) raceEndTimer=0;
            else if(linkRemoteState==3) {linkMyState=4;raceEndTimer=one;playCredits=one;}
        } else {raceEndTimer--;playCredits=one;}
        if(!raceEndTimer) goto quit;
    }
    if(scoreTabData.highScore!=one && !raceEndTimer) {
        UpdateRaceHud(shipIndex,&speedoHud,pauseState.selection,gameData,&scoreTabData);
        if(shipIndex[ownShip].update==UpdatePlayerShip || shipIndex[ownShip].update==UpdateRescueDroid || (shipIndex[ownShip].update==UpdateDroneShip && shipIndex[ownShip].autopilotCount>0)) {
            if((shipIndex[ownShip].combatAttr&0x8004)!=0x8004) AlterCheckPoint((CheckPoints *)CHECKPOINTS,shipIndex,pauseState.selection,gameData);
        }
    }
    if(gameData->weaponsOn || raceType==one || raceType==4) {
        if((shipIndex[ownShip].attr&64) && ownShip<12) {
            DrawWeaponIcons(weaponIcons,weaponIconTextureTable,&shipIndex[ownShip]);AddPrim(OT[CurrentScreen],&weaponIconPrims[CurrentScreen]);
        }
    }
    if(gameData->actTrackNum==2 && raceType!=two && (unsigned int)(shipIndex[ownShip].nearTrkSect->secNo-200)<76) {
        MoveMonoRail(monoRailObjs,6,&monoRail,shipIndex,pauseState.selection);InitTrainLights(prmObjects,prmCount,monoRailObjs,pauseState.selection);
    }
    if((unsigned int)(gameData->actTrackNum-6)<2) Video(videoObjects,videoCount);
    if(!(shipIndex[ownShip].attr&64) || shipTargetIndex[ownShip]>=0) DrawAllTrack(&raceCamera,trackCache);
    else DrawNewTrack(&raceCamera,trackCache);
    DrawShadow(shipShapeTable,shipIndex,shadowData);
    DrawShips(shipIndex,shipShapeTable,ownShip,raceCamera,pauseState.selection);
    if(!medalDisplay) DrawLandscape(&shipIndex[ownShip],&raceCamera);
    if(!medalDisplay) PortFarDraw(&raceCamera,shipIndex,shipShapeTable); /* Port: the rest of the track in view (portfar.h) */
    prmObjects[(short)locals.skyIndex]->skeleton->update=one;
    savedUpperY=raceCamera.camPos->super->relative.t[1];
    if(gameData->actTrackNum==17) {
        if(raceCamera.camPos->relative.t[1]>0) {
            bob=rsin(skycyc);
            raceCamera.camPos->super->relative.t[1]+=1024+(((raceCamera.camPos->relative.t[1]>>3)*bob)>>12);
        }
        if(pauseState.selection==-1 && (unsigned int)(raceCamera.section->secNo-303)>=26) skycyc=(skycyc+96)&4095;
    }
    AsmDrawObjectFixed(prmObjects[(short)locals.skyIndex],raceCamera.camPos,8191);
    raceCamera.camPos->super->relative.t[1]=savedUpperY;
    if(prevCameraUpdate!=CameraUpdate) {UnsetParticles(particleSlots);prevCameraUpdate=CameraUpdate;}
    AsmDrawParticles(particleSlots,raceCamera.camPos,pauseState.selection+1);DrawExplode();
    if(gameData->actTrackNum==13) DrawBirds();DrawRing();if(sillyShips) UpdateSharkModel();
    if(gameType!=2 && !(shipIndex[remoteShip].combatAttr&0x8000)) DrawLinkSign(shipIndex);
    if(pauseState.selection<0) {
        UpdateShipTrails(shipIndex,attractDelayFrames);locals.firstTime=1;
        if(shipTargetIndex[ownShip]==100) shipIndex[ownShip].attr&=~4;
        if((shipTargetIndex[ownShip]&7)<4 && shipTargetIndex[ownShip]>=16 && !(shipIndex[ownShip].combatAttr&4)) {
            CentreText(overtakeMessage,15,64,12,one);CentreText(msgOvertake,45,128,8,0);
            if(!(shipTargetIndex[ownShip]&7)) PlayNote(48,255,15,15,256);
        }
        for(i=0;i<15;i++) {
            if(!(shipIndex[i].combatAttr&0x8000) && shipTargetIndex[i]>0) {
                if(i==ownShip) { if(shipTargetIndex[i]>=16) goto damp_ship; }
                if(i!=remoteShip) goto damping_done;
                if(shipTargetIndex[i]<16) goto damping_done;
                damp_ship: {
                    ShipData *ship=shipIndex;
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
                    if((shipTargetIndex[i]-shipTargetIndex[i]/6*6)==one) {
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
                if((newRand()&3)==one && shipTargetIndex[i]>=16) AddSmallSmokeExplode(shipIndex[i].ppivot.vx-(shipIndex[i].unitVecNose.vx>>4),shipIndex[i].ppivot.vy-(shipIndex[i].unitVecNose.vy>>4),shipIndex[i].ppivot.vz-(shipIndex[i].unitVecNose.vz>>4),-1);
                if((shipTargetIndex[i]&3)==one && shipTargetIndex[i]>=16) {
                    x=(newRand()&511)-(shipIndex[i].unitVecNose.vx>>5)-256;
                    z=(newRand()&511)-(shipIndex[i].unitVecNose.vz>>5)-256;
                    AddSmallSpurtExplode(x,-(shipIndex[i].unitVecNose.vy>>5)+128,z,i);
                }
                shipTargetIndex[i]--;
                if(!shipTargetIndex[i]) {
                    if(i!=ownShip && i!=remoteShip) {shipIndex[i].attr&=~64;PlayNote(51,480,15,15,256);}
                    shipIndex[i].combatAttr|=0x8000;
                }
            }
            if((i==ownShip && (shipIndex[i].combatAttr&0x8000)) || (i==remoteShip && (shipIndex[i].combatAttr&0x8000))) {
                if((newRand()&31)<3) AddSmallSmokeExplode(shipIndex[i].ppivot.vx,shipIndex[i].nearTrkSect->centre.vy+128,shipIndex[i].ppivot.vz,-1);
            }
        }
        if(gameData->actTrackNum==2 || gameData->actTrackNum==7 || cheatRain) {
            AddRain(particleSlots,one,(VECTOR *)cameraPtr->camPos->relative.t); /* Port: the camera position is the first three ints of the skeleton's relative translation */
            advance=locals.rainTimer+1;locals.rainTimer=advance&255;
            if(!undercover && !(newRand()&3) && locals.rainTimer<32 && !(advance&3)) {
                if(fadeAmount<228) fadeAmount=228;locals.thunderAt=locals.rainTimer;
            }
            if(fadeAmount==220) fadeAmount=118;if(fadeAmount==110) fadeAmount=0;
            advance=locals.thunderAt+32;
            if(locals.rainTimer==advance) PlayNote(35,380,15,15,128);
            if(advance<locals.rainTimer && locals.rainTimer<64 && !(newRand()&3) && !(locals.rainTimer&3)) PlayNote(35,(short)(412-locals.rainTimer),15,15,320-locals.rainTimer);
        }
        if(gameData->actTrackNum==8) AddSnow(particleSlots,20,(VECTOR *)cameraPtr->camPos->relative.t);
    } else {if(locals.firstTime) UpdateShipTrails(shipIndex,attractDelayFrames);locals.firstTime=0;}
    resetMiscFx();DrawShipTrails(cameraPtr);
    if(!linkDropped) {
        UpdateStartDroid(startDroidObjects,shipIndex,(DroidData *)CHECKPOINTS,startTextureBase,gameData,pauseState.selection);
        UpdateLightStart(lightGroups,shipIndex,(DroidData *)CHECKPOINTS,startTextureBase,gameData,pauseState.selection);
    }
    if(playCredits) {ClearOTagR(OT[CurrentScreen],8192);ClearOTagR(OT2[CurrentScreen],1600);}
    AddPrim(OT[CurrentScreen]+8191,locals.clearPolys[CurrentScreen]);
    if(gameType!=one) advance=menuSendFlags;else advance=menuRecvFlags;
    cardNum-=advance*30;heading-=advance*15;
    Swap();firstTimeIn=0;UpdateNotes(0);
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
