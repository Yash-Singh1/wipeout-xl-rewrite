#include "functions.h"
int InitCARD();
int StartCARD();
int _bu_init();
int SetMem();
int GsInitVcount();
int ChangeClearSIO();
int ChangeClearPAD();
int memset_80089554();
int srand();
int ResetCallback();
extern int _stacksize;
extern int _ramsize;
extern int __bss;
extern int __bsslen;
signed char activateOT2;
int attractCycles;
short sixTracks;
short sixXPlayed;
short eightTracks;
short eightXPlayed;
short phantomDone;
short piranhaDone;
extern int attractActTrack;
extern int attractNormTrack;
extern int attractTrackNo;
extern int attractLoopFlag;
extern signed char sillyShips;
extern short TextureTableCount;
extern unsigned short ghostClutX;
extern unsigned short ghostClutY;
extern unsigned char raceTracks[];
extern u_short menuTextures[];
extern int cdFlag[];
extern short keyDefineThrust;
extern int fixedHeapAddress;
extern unsigned short gClutX;
extern unsigned short gClutY;
extern int remoteShip;
extern signed char cdPlaySeconds[];
extern u_long *OT[];
int savedTextureTableCount;
extern u_long *OT2[];
extern int trackNo;
extern int attractDelayFrames;
extern unsigned char cdMusicIndex;
extern int ownShip;
HeapBlock *mainheapcopy;
int reserveHeapCopy;
ConfigData *gameData;
extern int cdMusicTrack;
extern unsigned short keysDown;
extern int linkSendFlag;
extern signed char gameType;
extern int raceType;
extern short CurrentScreen;
extern int reserveHeap;
extern int quadIndex;
extern struct PadPacket padBuf1[];
extern short gTextureX;
extern short gTextureY;
WADFILE *textureWad;
int raceContext;
extern int cdMusicCount;
extern int ghostShip;
extern struct WipeCdStatus cdStatus[];
extern signed char continuesLeft;
extern int savedLapTimes[]; /* Port: the lap times and the four words after them */
extern CdlLOC cdTrackLocs[];
extern unsigned char cdMusicTable[];
extern HeapBlock *heap;

void main(void) __attribute__((noreturn));
void main(void) {
    char callWorkspace[64];
    char introFilename[16] = "C:\\ntscanim.exe";
    char initMemory[8];
    char menuPrimitives[0x50];
    char menuBackground[0x90];
    ConfigData config;
    RECT rect;
    char workspace[0x1AF0];
    int heapStart;
    int heapBytes;
    int reservedBytes;
    int frame;
    int ramEnd;
    int titleTimeout;
    int titleFrames;
    int menuRequested;
    int promptTimer;
    void *menuTexture;
    int enabled;
    int championshipType;
    int timeout;
    int musicSelection;

    memset_80089554(initMemory, 0, 4);
    promptTimer = 0;
    ResetCallback();
    SetMem(2);
    fixedHeapAddress = 0x801F7064;
    ramEnd = _ramsize - _stacksize;
    heapStart = __bss + __bsslen;
    reservedBytes = 0xE1000 + (heapStart & 0x7FFFFFFF);
    heapBytes = ramEnd - reservedBytes;
    heap = (HeapBlock *)heapStart;
    reserveHeap = heapStart + heapBytes;
    menuRequested = 0;
    InitDynamicMem((HeapBlock *)heapStart, heapBytes);
    InitDynamicMem((HeapBlock *)reserveHeap, 0xE1000);
    reserveHeapCopy = reserveHeap;
    mainheapcopy = heap;
    InitSystem();
    GsInitVcount();
    ClearVram();
    InitController();
    InitPadSettings();
    buildFixedPad(padBuf1);
    if (keysDown == 0x609) {
        sillyShips = 1;
        printf("silly ships activated!!!!!\n");
    } else {
        sillyShips = 0;
    }
    keyDefineThrust = 0;
    frame = 0;
    InitCDFS(heap);
    InitialiseAmbientColour();
    InitialiseColourMatrix();
    InitialiseLightMatrix();
    ClearVram();
    InitSonyDisplay();
    Load16BitTexture((int)"c:\\wipeout2\\textures\\sce_logo.tim", 1);
    InitCD(cdTrackLocs, 0);
    gameData = &config;
    TextureTableCount = 0x113;
    InitText();
    SetCharTextures(8);
    SetCharTextures(0xC);
    SetCharTextures(0x10);
    do {
        VSync(0);
        frame += 1;
    } while (frame < 0xF0);
    SetDispMask(0);
    InitGameDisplay(1);
    Load16BitTexture((int)"c:\\wipeout2\\textures\\rcopyxl.tim", 1);
    SetDispMask(1);
    activateOT2 = 1;
    frame = 0;
    do {
        VSync(0);
        frame += 1;
    } while (frame < 0x3C);
    Load16BitTexture((int)"c:\\wipeout2\\textures\\dolbynts.tim", 1);
    frame = 0;
    do {
        VSync(0);
        frame += 1;
    } while (frame < 0x3C);
    Load16BitTexture((int)"c:\\wipeout2\\textures\\redbntsc.tim", 1);
    frame = 0;
    do {
        VSync(0);
        frame += 1;
    } while (frame < 0x78);
    frame = 0;
    ShowLoadMessage();
    do {
        VSync(0);
        frame += 1;
    } while (frame < 4);
    InitCARD(1);
    StartCARD();
    _bu_init();
    timeout = 0xE10;
    InitCardEvents();
    championshipType = 4;
    ChangeClearSIO(1);
    ChangeClearPAD(0);
    srand(GsGetVcount());
    ScoreTabInit(gameData);
    InitGameData(gameData);
    _comb_control(1, 1, 0);
    ownShip = 9;
    attractActTrack = 9;
    attractNormTrack = gameData->actTrackNum;
    attractTrackNo = gameData->trackNum;
    ghostShip = 1;
    gameData->shipClass = 0;
    raceType = 0x63;
    continuesLeft = 0;
    gameData->initMagic = 0x88387887;
    _init_cont();
    InitPadSettings();
    FirstSavedGame(gameData);
    raceContext = 1;
    InitResident();
    InitResidentMenu();
    WADLoadTexture(textureWad, 1);
    closewadfilenvc(textureWad);
    DrawSync(0);
    rect.x = 0x280;
    rect.y = 0x100;
    rect.w = 0x20;
    rect.h = 0x40;
    MoveImage(&rect, 0x2A0, 0x100);
    DrawSync(0);
    InitShufflePlay();
    cdMusicIndex = 0;
    cdMusicTrack = cdMusicTable[0];
    CloseCardEvents();
    anim(introFilename);
    InitCardEvents();
    ShowLoadMessage();
    enabled = 1;
    while (1) {
restart_menu:
        if (menuRequested == 0) {
            menuTexture = Load16BitTexture((int)"c:\\wipeout2\\textures\\rstartxl.tim", 0);
        }
        titleFrames = 0;
        if ((raceType != championshipType) || (raceContext == enabled)) {
            if ((menuRequested == 0) && (attractLoopFlag != enabled)) {
                cdMusicIndex = (cdMusicIndex + 1) % (cdMusicCount - 1);
                cdMusicTrack = cdMusicTable[cdMusicIndex];
                goto music_done;
replay_intro:
                CloseCardEvents();
                anim(introFilename);
                InitCardEvents();
                attractCycles = 0;
                goto restart_menu;
            } else {
                cdMusicIndex = (cdMusicIndex + 1) % (cdMusicCount - 1);
                cdMusicTrack = cdMusicTable[cdMusicIndex];
                musicSelection = gameData->cdTrack;
                if (musicSelection == enabled) {
                    musicSelection = cdMusicTrack;
                }
                CdMusicPlay(musicSelection, 0);
            }
music_done:
            rect.h = 0x200;
            rect.w = 0x140;
            rect.x = 0;
            rect.y = 0;
            ClearImage(&rect, 0, 0, 0);
            DrawSync(0);
            SetCdVolume((gameData->musicVolume * 0xFF) / 100);
            SetSoundVolume((gameData->effectVolume * 0xFF) / 100);
            if (menuRequested == 0) {
                while (1) {
                    titleFrames += 1;
                    buildFixedPad(padBuf1);
                    ClearOTagR(OT[CurrentScreen], 0x2000);
                    ClearOTagR(OT2[CurrentScreen], 0x640);
                    keyDefineThrust = 0;
                    quadIndex = CurrentScreen * 0x12C;
                    titleTimeout = 5;
                    if (attractLoopFlag == enabled) {
                        titleTimeout = 0x1E;
                    }
                    SetCdVolume((gameData->musicVolume * 0xFF) / 100);
                    SetSoundVolume((gameData->effectVolume * 0xFF) / 100);
                    if (titleFrames >= (titleTimeout * 0x1E)) {
                        attractCycles++;
                        if (attractCycles >= 3) {
                            goto replay_intro;
                        }
                        raceContext = enabled;
                        gameType = 2;
                        if (gameData->venomComplete != 0) {
                            ownShip = newRand() % 14;
                        } else {
                            ownShip = newRand() % 11;
                        }
                        remoteShip = -1;
                        attractLoopFlag = 0;
                        break;
                    }
                    if (titleFrames >= 0xB && (GetMenuKey(6) << 0x10) != 0) {
                        PlayNote(9, 0xFF, 0xF, 0xF, 0x100);
                        frame = 0;
                        do {
                            VSync(0);
                            UpdateNotes(0);
                            frame += 1;
                        } while (frame < 0xA);
                        menuRequested = 1;
                        raceContext = 0;
                        attractLoopFlag = menuRequested;
                        break;
                    }
                    ShowMenuBackground(menuPrimitives, menuBackground, menuTexture);
                    if (attractLoopFlag == enabled) {
                        SetInGameCDTrack(cdPlaySeconds, cdStatus, gameData, cdFlag);
                    }
                    if ((promptTimer % 30) < 0x15) {
                        AddText("PRESS START", 0xA0, 0xDC, 8, enabled);
                    }
                    promptTimer += 1;
                    Swap();
                }
            }
        }
        gameType = 2;
        if (raceType != championshipType) {
            raceType = 0x63;
            if ((raceContext == 0) && (menuRequested == enabled)) {
                savedTextureTableCount = TextureTableCount;
                ShowLoadMessage();
                raceContext = MenuControl(gameData, menuTexture, &raceContext, timeout);
                TextureTableCount = savedTextureTableCount;
                if (raceContext == menuRequested) {
                    menuRequested = 0;
                    goto restart_menu;
                }
                menuRequested = 0;
                goto block_56;
            }
        }
        if (raceContext == enabled) {
            trackNo = newRand() % 6;
            gameData->trackNum = trackNo;
            gameData->actTrackNum = raceTracks[gameData->trackNum];
        } else {
block_56:
            trackNo = gameData->actTrackNum;
        }
        gTextureX = 0x140;
        gTextureY = 0;
        attractDelayFrames = 0;
        linkSendFlag = 0;
        savedLapTimes[0] = 0;
        raceResetWord0 = 0;
        raceResetWord1 = 0;
        raceResetWord2 = 0;
        raceResetWord3 = 0;
        gClutX = ghostClutX;
        gClutY = ghostClutY;
        if (raceType == championshipType) {
            if (gameData->rapierComplete != 0) {
                gameData->actTrackNum = raceTracks[gameData->class2Race];
                trackNo = gameData->actTrackNum;
                gameData->trackNum = gameData->class2Race;
                gameData->shipClass = 3;
                gameData->rapierChallengeDone = 0;
            } else if (gameData->venomComplete != 0) {
                gameData->actTrackNum = raceTracks[gameData->class1Race];
                trackNo = gameData->actTrackNum;
                gameData->trackNum = gameData->class1Race;
                gameData->shipClass = 2;
                gameData->venomChallengeDone = 0;
            }
        }
        savedTextureTableCount = TextureTableCount;
        if (keyDefineThrust == 0) {
            ShowLoadMessage();
            InitMenuTextures(menuTextures);
        }
        InitRaceLoad();
        keyDefineThrust = 0;
        TextureTableCount = savedTextureTableCount;
        CdMusicStop();
        race(gameData, raceContext, timeout);
        ShowLoadMessage();
        if ((sixTracks != 0) && (sixXPlayed == 0)) {
            CloseCardEvents();
            anim("c:\\xtro1.exe");
            InitCardEvents();
            sixTracks = 0;
            sixXPlayed = enabled;
        } else if ((eightTracks != 0) && (eightXPlayed == 0)) {
            CloseCardEvents();
            anim("c:\\xtro3.exe");
            InitCardEvents();
            eightTracks = 0;
            eightXPlayed = enabled;
        } else if (phantomDone != 0) {
            CloseCardEvents();
            anim("c:\\xtro2.exe");
            InitCardEvents();
            phantomDone = 0;
        } else if (piranhaDone != 0) {
            CloseCardEvents();
            anim("c:\\xtro4.exe");
            InitCardEvents();
            piranhaDone = 0;
        }
        racetidy();
        gameType = 2;
        if (raceType != championshipType) {
            raceType = 0x63;
        }
        menuRequested = 0;
        if (raceType == championshipType) {
            goto restart_menu;
        }
        menuRequested = raceContext == 0;
    }
}
