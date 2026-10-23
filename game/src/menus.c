#include "functions.h"
#include "portlink.h" /* Port: two players on one machine */
#include "portorig.h" /* Port: the original Wipeout's tracks */
#include "menus.h"
#include "hiscores.h"
#include "card.h"
#include "pad.h"
#include "../../platform/controls.h" /* Port: keyboard and gamepad glyphs and button names */
#include "ayrace.h"
#include "ghost.h"
#include "include_asm_g8.h"

/* String data that is copied a word at a time lives on a word boundary. */
#define ALIGN4 __attribute__ ((aligned (4)))
#define ALIGN2 __attribute__ ((aligned (2)))

/* Menu sound effects, popups and card helpers used by several screens. */
#define Sound(n) PlayNote(n,255,15,15,256)
#define Popup() AddPrim(OT[CurrentScreen],&polys->boxFill[CurrentScreen]);AddPrim(OT[CurrentScreen],&polys->boxBorder[CurrentScreen])
#define CardMessage(card) AddPrim(OT[CurrentScreen],&polys->lowerFill[card][CurrentScreen]);AddPrim(OT[CurrentScreen],&polys->lowerBorder[card][CurrentScreen])
#define SelectedCard() (&cardSlots[myFinished].entry[fadescreen])
/* Port: cardUsed, cardFileNames, cardNames and cardFlags were names for single fields of the cardSlots entries
   (offsets 10, 11, 43 and 75 of each 90-byte entry); the fields are used directly. */
#define SelectedCardFlags() (cardSlots[myFinished].entry[fadescreen].type)
typedef struct Str20 { s32 w[5]; } Str20;
typedef struct TimImageBlock { u8 pad0[0xC]; u16 x; u16 y; u16 width; u16 height; } TimImageBlock;
extern short buttonCount;
extern int buttonTextures;
/* Port: endTextures, menuTexturePtrs, menuTexturePtrsB, menuTexturePtrsC, menuTexturePtrsD are parts of textureTable; the uses read them directly. */
extern Texture *textureTable[];
extern short CurrentScreen;
extern POLY_FT4 menuButtons[][2];
extern char textureSequenceName[];
extern char msgNoCard[];
extern CardSlot cardSlots[];
extern u8 TrackHiCount;
extern u8 cardDirLoaded1;
extern s32 cardUnformatted0;
extern s32 cardUnformatted1;
extern Label6 defaultCardName[];
extern s32 prevCardStatus0;
extern s32 prevCardStatus1;
extern s32 cardSettleCount0;
extern s32 cardSettleCount1;
extern s16 card1Exists;
extern s16 cardPresent1;
extern u8 cardDirChanged0;
extern u8 cardDirChanged1;
extern s32 myFinished;
extern s32 fadescreen;
extern u8 msgErrorOpenRead[];
extern u8 msgErrorReadHeader[];
extern u8 msgOverwriteFile[];
extern u8 msgDeleteFile[];
extern u8 msgMemoryCardIs[];
extern u8 msgUnformattedDoYou[];
extern u8 msgWishToFormat[];
extern u8 msgYes[];
extern u8 msgNo[];
extern LabelList loadModeLabels;
extern int shakeAmount;
extern int currMenu;
extern int currentCard;
extern short prevMenu;
extern CardTimer cardNotice;
extern CardTimer cardPending;
extern CardDevice cardDevice0;
extern CardDevice cardDevice1;
extern char msgCards[];
extern char msgIsFull[];
extern char msgCard[];
extern unsigned short keysDown;
extern MenuEntry *menu;
extern u8 msgCheckingMemory[];
extern u8 msgFileExists[];
extern u8 msgNotAWipeoutXl[];
extern u8 msgSaveGame[];
extern u8 msgNotFormatted[];
extern u8 msgErrorFormatting[];
extern u8 msgMemoryCard[];
extern u8 msgCannotSaveFileNo[];
extern u8 msgMemoryCardPresent[];
extern u8 msgErrorLoadingFile[];
extern u8 msgErrorSavingFile[];
extern u8 msgErrorDeletingFile[];
extern u8 msgLoadingGameData[];
extern u8 msgSavingGameData[];
extern u8 msgDeletingGameData[];
extern u8 msgSelectMemory[];
extern u8 msgCardBlockTo[];
extern u8 msgLoadGameFrom[];
extern u8 msgSaveGameTo[];
extern u8 msgDeleteSaveFrom[];
extern u8 msgSelectMemoryCard[];
extern s8 fmtMemoryCard[];
extern u8 msgUnformatted[];
extern u8 msgNoMemory[];
extern PasswordSeeds passwordSeeds[];
extern PasswordSeeds passwordSeedsClear[];
extern u_char trackMedals[];
extern char numTex[];
extern char msgPasswordIsIncorrect[];
extern char msgUseUpAndDownOnTheDirectional[];
extern char msgButtonsToChangeTheSymbol[];
extern char msgCurrentPassword[];
extern char msgNewPasswordEntry[];
extern s32 passwordErrorTimer;
/* Port: menuGlyphWidth is part of fontGlyphs8; the uses read it directly. */
#include "hud.h"
extern FontGlyph fontGlyphs8[];
/* Port: glyphWidth is part of fontGlyphs16; the uses read it directly. */
#include "hud.h"
extern FontGlyph fontGlyphs16[];
extern s32 ownShip;
extern u_short keyDefineThrustPad[2];
extern u_short keyDefineFire[2];
extern u_short keyDefineView[2];
extern u_short keyDefineDitch[2];
extern u_short keyDefineRBrake[2];
extern u_short keyDefineLBrake[2];
extern int myseed;
extern int heading;
extern unsigned char raceTracks[];
extern short trackModelOrder[];
extern Object *shipModels[];
extern char hiScoreLabelName[];
extern char hiScoreLabelTime[];
extern char hiScoreLabelShip[];
extern char hiScoreLabelLap[];
extern char teamNameAgSystems[] ALIGN4;
extern char teamNameAuricom[] ALIGN4;
extern char teamNameQirex[] ALIGN4;
extern char teamNameFeisar[] ALIGN4;
extern char teamNamePiranha[] ALIGN4;
extern char msgRedBull[];
extern char hiScoreRank0[] ALIGN2;
extern char hiScoreRank1[] ALIGN2;
extern char hiScoreRank2[] ALIGN2;
extern char hiScoreRank3[];
extern DB doubleBuffer[];
extern DISPENV DispEnv0;
extern DISPENV DispEnv1;
extern u8 msgInactive[];
extern u8 msgDefaultView[];
extern u8 msgMoveScreen[];
extern u8 msgDefaultName[];
extern u8 msgUseUpDownLeftAndRightOnThe[];
extern u8 msgToAdjustScreenPosition[];
extern u8 msgSaveScreenPosition[];
extern u8 msgActivateMoveScreen[];
extern u8 msgConfirmLetterEntry[];
extern u8 msgNameIsUsedAsDefaultHighScore[];
extern s32 blinkCounter;
extern u8 msgOn[];
extern u8 msgOff[];
extern u8 msgInt[];
extern u8 msgExt[];
extern u8 msgActive[];
extern u8 msgWeapons[];
extern char *keyFunctionNames[];
extern Positions knobPosX;
extern Positions knobPosY;
extern TwistAngles twistAngles;
extern short negconEditing[];
extern short menuTextures;
extern int negconBlink;
extern char fmtNewAngle[];
extern char msgMaximumLock[];
extern char msgCentreSetting[];
extern char msgDeadSpot[];
extern char fmtCurrent[];
extern char msgToSetOrResetNegconPosition[];
extern DigitalButtons padDigitalKeys;
extern AnalogButtons padAnalogKeys;
extern HighlightWidths keyHighlightWidth;
extern struct PadPacket padBuf1[];
extern int duplicateKeyTimer;
extern char msgAButtonIsDefined[];
extern char msgMoreThanOnce[];
extern char msgL1Button[];
extern char msgL2Button[];
extern char msgRButton[];
extern char msgR1Button[];
extern char msgR2Button[];
extern char msgLButton[];
extern char msgDefaultConfig[];
extern char msgUseTheLeftAndRightDirectional[];
extern char msgToChangeCurrentButtonSettings[];
extern char labelCdTrackMenu[];
extern u8 sfxMode;
extern int cdFlag[];
extern s32 audioBlink;
extern char labelMusic[];
extern char labelSfx[];
extern char labelSound[];
extern char labelArtist[];
extern char labelTitle[];
extern char labelMono[];
extern char labelStereo[];
extern int ambientVoice1;
extern signed char cdPlaySeconds[];
extern u_char cdMusicIndex;
extern s32 cdMusicTrack;
extern s32 cdMusicCount;
extern struct WipeCdStatus cdStatus[];
extern TeamOrder teamShipGroup[];
extern char msgTeamCheatActive[];
extern char msgExcellentTurningAbilityAnd[];
extern char msgButNotTooQuickBeginner[];
extern char msgGoodAccelerationAndTurning[];
extern char msgWeakShieldAmateur[];
extern char msgGoodAllRounderButAverage[];
extern char msgIntermediate[];
extern char msgFastWithGoodShieldCapability[];
extern char msgHeavyAtTurningExpert[];
extern char msgVeryFastWithExcellentShield[];
extern char msgWeaponTechnologyMaster[];
extern s32 teamSelectFrames;
extern ClassOrder classOrderTable[];
extern char msgPhantomTrackCheatActive[];
extern char msgTrackCheatActive[];
extern char msgIntermediate2[];
extern char msgSpeedDemon[];
extern char msgAMajorCanadianIndustrial[];
extern char msgComplexEasy[];
extern char msgASnowyTibetanMountainCircuit[];
extern char msgASouthAmericanJungleCircuit[];
extern char msgTricky[];
extern char msgANewlyConstructedGerman[];
extern char msgCommercialParkTricky[];
extern char msgADisusedFrenchMetroSystem[];
extern char msgAHugeSuspendedCircuitOver[];
extern char msgBlackSeaHard[];
extern char msgAnObscureVolcanicIslandLocated[];
extern char msgInTheSouthPacificVeryHard[];
extern char msgAnAmericanCityCircuitAffected[];
extern char msgBySeismicActivityVeryHard[];
extern s32 envmapr;
extern s32 envmapg;
extern s32 envmapb;
extern s32 classBlink;
extern char msgNovice[];
extern char msgExpert[];
extern char msgEasy[];
extern char msgHard[];
extern char msgGold[];
extern char tempheapcopy[];
extern char msgSilver[];
extern char msgBronze[];
extern Object *menuModels[];
extern char msgLinkedUp[] ALIGN4;
extern char msgTimeTrial[] ALIGN4;
extern char msgOneOnOne[] ALIGN4;
extern char msgArcadeLink[] ALIGN4;
extern char msgChallengeI[] ALIGN4;
extern char msgChallengeII[] ALIGN4;
extern char msgEssentialOptions[] ALIGN4;
extern char msgRacingMode[] ALIGN4;
extern char msgStructureMode[] ALIGN4;
extern char msgOptionsMode[] ALIGN4;
extern char msgTrackMode[] ALIGN4;
extern char msgTeamMode[] ALIGN4;
extern char msgPrecisionLevels[] ALIGN4;
extern char msgInteractionMode[] ALIGN4;
extern char msgNegconSelectionMode[] ALIGN4;
extern char msgNegconMode[] ALIGN4;
extern char msgNegconTwistMode[] ALIGN4;
extern char msgPasswordMode[] ALIGN4;
extern char msgProgressionMode[] ALIGN4;
extern char msgDataMode[] ALIGN4;
extern char msgPreferencesMode[] ALIGN4;
extern char msgScreenMode[] ALIGN4;
extern char msgTimesMode[] ALIGN4;
extern char msgArcadeMode[] ALIGN4;
extern char msgTimeTrialMode[] ALIGN4;
/* The same strings, reached through $gp by AddTextBits. */
extern char msgArcade[8] ALIGN4;
extern char msgLink[8] ALIGN4;
extern s32 menuTitleBlink;
extern s32 raceType;
extern s16 linked;
extern RaceOrder raceOrderTable[];
extern char msgOnePlayerSingleRaceAgainst[];
extern char msgGridOfCompetitors[];
extern char msgOnePlayerSingleRaceTimeTrial[];
extern char msgNoCompetitors[];
extern char msgTwoPlayerSingleRaceAgainst[];
extern char msgTwoPlayerSingleRaceWithNo[];
extern char msgCompetitors[];
extern char msgOnePlayerPhantomChallenge[];
extern char msgAtRapierClassWeaponsAreActivated[];
extern char msgOnePlayerPiranhaChallenge[];
extern char msgAtPhantomClassWeaponsAre[];
extern short menuEditor;
extern int cardNum;
extern short lastTopOption;
extern char msgSelect2[];
extern char msgAccept[];
extern char msgBack[];
extern MenuLayout *menuLayout;
extern char pathMenuDat[];
extern char errLoadMenuLineDataFileHasZero[];
extern char errLoadMenuLineDataFailedTo[];
extern char errLoadMenuLineDataFailedTo2[];
extern char errorString[];
extern char msgTalonSReach[];
extern char msgGareDEuropa[];
extern char msgVostokIsland[];
extern char msgSpilskinanke[];
extern char msgSagarmatha[];
extern char msgValparaiso[];
extern char msgOdessaKeys[];
extern char msgPhenitiaPark[];
extern Texture *fontTexture; /* Port: was a private padded copy of Texture; u0 and v0 were read as bytes (values are below 256) */
extern s32 quadIndex;
extern POLY_FT4 quadPool[];
extern char errLoadTextureFileNotFound[];
extern char errLoadTextureFailedToAllocate[];
extern char errLoadTextureFailedToLoadFile[];
extern Str20 pathCommonDir;
extern char extPrm[];
extern s32 shipSpin;
extern void *preCalcObject;
extern ConfigData *gameData;
extern SVECTOR VertexNormal[];
extern u8 turnDivisor;
extern s32 currScreen;
extern u8 twist;


typedef struct { s16 h[5]; } Table10;
extern char msgInitializingCombatLink[];
extern char msgLinkFailed[];
extern char msgMasterLinkEstablished[];
extern char msgSlaveLinkEstablished[];
extern s32 linkBlinkCount;
extern s16 linkRetries;
extern s32 prevLinkInB;
extern s32 prevLinkInA;
extern s16 linkToggleB;
extern s16 linkWaitCount;
extern s16 linkToggleA;
extern s16 linkReady;
extern s32 linkInB;
extern s32 linkInA;
extern s16 linkCloseCount;
extern s16 linkTimedOut;
extern s16 linkState;
extern s16 linkTickCount;
extern s16 menuRecvKey;
extern u8 loadMessage[];
extern POLY_FT4 loadPolys0[];
extern POLY_FT4 loadPolys1[];
extern Table10 shipGroupTeam;
extern s16 sixTracks;
extern s16 sixXPlayed;
extern s16 eightTracks;
extern s16 eightXPlayed;
extern s32 screenCount;
extern u32 slideCount;
extern u16 topOffSet;
extern u16 middleOffSet;
extern u16 bottomOffSet;
typedef struct TrackList { char track[15]; } TrackList;
extern TrackList defaultTrackList[];

typedef struct MenuWorkspace {
    char cdState[8];
    MenuBars menus;
    MenuPolys menuPolys;
    volatile short screenX;
    volatile short screenY;
    volatile short screenWidth;
    volatile short screenHeight;
    char pad1910[12];
    volatile short musicX;
    volatile short musicY;
    volatile short musicWidth;
    volatile short musicHeight;
    char pad1924[164];
    MenuSelection selection;
    HiScoreData cardState;
    MusicScreen musicScreen;
    char displayScreen[1168];
    volatile int cdTrack;
    char pad2354[4];
    int inputDelay;
    char pad235c[4];
    unsigned short fadeLevel;
    char pad2362[6];
    Texture *volatile musicTexture;
    char pad236c[4];
    unsigned short attractMode;
} MenuWorkspace;

void InitMenuBox(MenuBars *bars);
void ShowMenu(ConfigData *gameData, MenuBars *polys, MenuSelection *selection);
void InitNewDisplay(ConfigData *gameData);
void LoadModel(Object **modelShapes, char *fileName);
extern u8 msgCannotSelect[];
extern u8 msgAsOtherPlayer[];
extern u8 msgHasStarted[];
extern u8 msgWaitingForOtherPlayer[];
extern unsigned short savedSelection[2];
extern unsigned short savedClassSelection[2];
extern unsigned short savedTrackSelection;
extern u16 savedMainSelection;
extern s16 fastFrame;
extern s16 menuRecvKey;
extern s16 menuRecvMode;
extern s16 raceFlag;
extern s32 negconCentre;
extern s32 trackNo;
extern s32 remoteStartGame;
extern s32 screenCount;
extern s32 menuShip;
extern s32 menuActTrack;
extern s32 menuNormTrack;
extern s32 menuTrackNo;
extern s32 menuFlag;
extern s16 keyDefineThrust;
extern char modelNameHarry[];
extern char modelNameJune[];
extern u8 menuRecvFlags;
extern s32 remoteShip;
extern s16 linkTimedOut;
extern s16 linkState;

int MenuControl(ConfigData *gameData, void *menuTexture, int *state, int raceMemorySize)
{
    TrackList trackList;
    int gap[2];
    MenuWorkspace work;
    int musicIndex;
    int i;
    int steps, selectionNo;
    int idleCount;
    short negcon;
    short errorCount;
    work.inputDelay = 2;
    trackList = defaultTrackList[0];
    work.fadeLevel = 200;
    negcon = 0;
    idleCount = 0;
    errorCount = 0;
    work.attractMode = 0;
    work.cdTrack = 1;
    InitDynamicMem(reserveHeap, RESERVE_HEAP_SIZE);
    keyDefineThrust = 1;
    fastFrame = 1;
    negconCentre = 0;
    remoteStartGame = 0;
    cardNum = 0;
    heading = 0;
    linkTimedOut = 0;
    tempGameData = gameData;
    SfxSetReverbDepth(0);
    KillControlledSound();
    {
        u8 controllerCenter = gameData->negconCentre;
        work.musicX = 512;
        work.musicY = 0;
        work.musicWidth = 240;
        work.musicHeight = 320;
        turnDivisor = controllerCenter;
    }
    SetSkeletonPosition(raceCamera.camPos, 0, 0, 0);
    SetSkeletonDirection(raceCamera.camPos, 0, 0, 0);
    SetSkeletonPosition(upperSkeleton, 0, 0, 0);
    SetSkeletonDirection(upperSkeleton, 0, 0, 0);
    InitMenuDisplay(150, 0);
    {
        int setup;
        Object **setupObject = shipShapeTable;
        for (setup = 0; setup < 15; setup++, setupObject++) {
            InitMatrix(&(*setupObject)->skeleton->relative);
            InitMatrix(&(*setupObject)->skeleton->absolute);
            (*setupObject)->skeleton->update = 1;
        }
    }
    ClearOTagR(OT[CurrentScreen], 8192);
    ClearOTagR(OT2[CurrentScreen], 1600);
    UpdateNotes(0);
    Swap();
    menu = DAlloc(reserveHeap, 0x6A70);
    menuLayout = DAlloc(reserveHeap, 0x10760);
    InitMenus();
    InitChallenges();
    if (!raceFlag) {
        if (menuFlag) {
            ownShip = menuShip;
            gameData->actTrackNum = menuActTrack;
            gameData->trackNum = menuNormTrack;
            trackNo = menuTrackNo;
        } else {
            ownShip = 9;
            trackNo = 0;
            gameData->trackNum = 0;
            gameData->actTrackNum = raceTracks[0];
            raceType = 99;
        }
    } else raceFlag = 0;
    *(volatile short *)&menu[1].selection = savedSelection[0];
    menu[4].selection = savedSelection[1];
    menu[2].selection = savedClassSelection[0];
    if (gameData->shipClass == 0) menu[5].selection = savedTrackSelection;
    if (gameData->shipClass == 1) menu[6].selection = savedTrackSelection;
    if (gameData->shipClass == 2) menu[7].selection = savedTrackSelection;
    if (gameData->shipClass == 3) menu[8].selection = savedTrackSelection;
    menu[0].selection = savedMainSelection;
    switch (menu[1].selection) {
    case 0:
        raceType = 0;
        break;
    case 1:
        raceType = 1;
        break;
    case 2:
    case 3:
        raceType = 0;
        menu[1].selection = 0;
        break;
    case 4:
        raceType = 4;
        break;
    }
    PortLinkMenusStart(); /* Port: the two-player modes start at the lobby (portlink.c) */
    SetShake(0);
    InitMenuBox(&work.menus);
    InitScreenBox(&work.menuPolys);
    InitTempData(gameData, &work.selection);
    Init2DParticle();
    InitMenuTextures(&menuTextures);
    InitMenuPolys((MenuPrims *)&work.menuPolys);
    LoadCardTexture();
    work.musicTexture = textureTable[menuTextures];
    InitDrBox(work.musicTexture, (DrBox *)&work.musicScreen);
    InitMusicScreen(&work.musicScreen, menuTextures);
    InitKeyConfig((KeyConfig *)&savedKeyConfig);
    LoadMenuLineData();
    LoadModel(menuModels, modelNameHarry);
    LoadModel(shipModels, modelNameJune);
    if (raceType == 4) {
        gameData->shipClass = gameData->savedClass;
        gameData->actTrackNum = gameData->savedActTrackNum;
        gameData->trackNum = gameData->savedTrackNum;
    }
    InitScoreTabData(&work.cardState);
    work.cardState.charX = 180;
    work.screenWidth = 320;
    work.screenHeight = 240;
    musicIndex = gameData->cdTrack;
    if (musicIndex == 1) {
        unsigned char q = (unsigned char)cdMusicIndex;
        q = (unsigned char)(((unsigned char)cdMusicIndex + 1)%(cdMusicCount - 1));
        cdMusicIndex = q;
        musicIndex = cdMusicTable[q];
        cdMusicTrack = musicIndex;
    }
    CdMusicPlay(musicIndex, 0);
    while (!negconCentre) {
        if ((unsigned)currMenu < 2 || currMenu == 2 || (unsigned)(currMenu - 17) < 2) fastFrame = 0;
        else fastFrame = 1;
        buttonCount = 0;
        SfxDoSfx();
        buildFixedPad(padBuf1);
        buildGameKeyPad(padBuf1, playerControl);
        UpdateCheatSequence(1);
        quadIndex = CurrentScreen * 300;
        ClearOTagR(OT[CurrentScreen], 8192);
        ClearOTagR(OT2[CurrentScreen], 1600);
        if (currMenu != 9) SetInGameCDTrack(cdPlaySeconds, (struct WipeCdStatus *)work.cdState, gameData, (int *)&work.cdTrack);
        {
            unsigned short fade = *(volatile unsigned short *)&work.fadeLevel;
            if ((short)fade < 200) work.fadeLevel = fade + 8;
        }
        if (!negcon) {
            if (playerControl->head == 2) {
                if (currMenu == 10) currMenu = 20;
                negcon = 1;
                menu[3].options[1].nextMenu = 20;
            }
        }
        if (negcon && playerControl->head == 1) {
            if (currMenu == 20 || (unsigned)(currMenu - 21) < 2) currMenu = 10;
            negcon = 0;
            menu[3].options[1].nextMenu = 10;
        }
        if (errorCount > 0 && !screenCount) {
            PrintCX(msgCannotSelect, 160, 110);
            PrintCX(msgAsOtherPlayer, 160, 118);
            PrintCX(msgHasStarted, 160, 126);
            AddPrim(OT[CurrentScreen], &work.menuPolys.topFill[CurrentScreen]);
            errorCount--;
            AddPrim(OT[CurrentScreen], &work.menuPolys.topBorder[CurrentScreen]);
        }
        if (!screenCount) {
            ShowMenu(gameData, &work.menus, &work.selection);
            if (currMenu == 9) DrawMusicScreen(&work.musicScreen);
        } else {
            SlideScreen(&work.musicScreen);
            fastFrame = 1;
        }
        if (shakeAmount) fastFrame = 1;
        ShakeScreen();
        Update2DParticle();
        DrawDrBox(work.musicTexture, (DrBox *)&work.musicScreen);
        DisplayScreen(textureTable[5 + menuTextures], (DrBox *)work.displayScreen);
        if (!screenCount) MenuFunction(gameData, &work.menuPolys, &savedKeyConfig, &work.selection, &work.cardState, &work.musicScreen);
        if (work.inputDelay > 0) {
            steps = 2;
            work.inputDelay = *(volatile int *)&work.inputDelay - 1;
        } else {
            if (gameType != 1) steps = menuSendFlags;
            else steps = menuRecvFlags;
        }
        cardNum -= steps * 30;
        heading -= steps * 15;
        Swap();
        if (cardPending.timer <= 0 && cardNotice.timer <= 0) {
            PortAutoSave(gameData, 1); /* Port: the game saves itself (portsave.c) */
        }
        if (menuEditor) goto serial;
        if (((GetMenuKey(5) || PortLinkMenuSelect(screenCount)) && !screenCount && currMenu != 19) || (currMenu == 19 && ((linked && linkState == 5) || linkTimedOut == 1))) { /* Port: the linked players' way through the menus (portlink.c) */
            if (linkTimedOut) {
                raceType = 0;
                linkTimedOut = 0;
                killLink();
            }
            if (currMenu == 0) {
                selectionNo = menu[0].selection;
                if (selectionNo == 2 && raceType == 4) goto cancel;
                if ((selectionNo == 0 || selectionNo == 4) && remoteStartGame) goto error;
                if (currMenu == 0 && gameType == 1 && menu[0].selection == 2) goto cancel;
            }
            PlayNote(9, 255, 15, 15, 256);
            if (currMenu == 0 && menu[0].selection == 3) {
                savedSelection[0] = menu[1].selection;
                savedSelection[1] = menu[4].selection;
                savedClassSelection[0] = menu[2].selection;
                negconCentre = 1;
                menuSendKey = 2;
                errorCount = 0;
                if (gameData->shipClass == 0) savedTrackSelection = menu[5].selection;
                if (gameData->shipClass == 1) savedTrackSelection = menu[6].selection;
                if (gameData->shipClass == 2) savedTrackSelection = menu[7].selection;
                if (gameData->shipClass == 3) savedTrackSelection = menu[8].selection;
                savedMainSelection = menu[0].selection;
            } else if (menu[currMenu].numOptions && prevMenu != 10 && (unsigned short)(prevMenu - 22) >= 2 && prevMenu != 12 && (unsigned short)(prevMenu - 17) >= 2) {
                CopyGameData(gameData, &work.selection, 1, (KeyConfig *)&savedKeyConfig, &work.cardState);
                PortLinkMenuSelected(currMenu); /* Port */
                currMenu = menu[currMenu].options[menu[currMenu].selection].nextMenu;
                CopyGameData(gameData, &work.selection, 0, (KeyConfig *)&savedKeyConfig, &work.cardState);
                errorCount = 0;
                InitMenuDisplay(currMenu == 4 ? 110 : 150, 0);
                if (CurrentScreen) {
                    work.screenX = 0;
                    work.screenY = 240;
                } else {
                    work.screenX = 0;
                    work.screenY = 0;
                }
                InitSlider();
                goto entered;
            } else if (currMenu != 10 && prevMenu != 10 && currMenu != 23 && prevMenu != 23 && currMenu != 22 && prevMenu != 22 && currMenu != 12 && prevMenu != 12 && currMenu != 17 && prevMenu != 17 && currMenu != 18 && prevMenu != 18) {
                CopyGameData(gameData, &work.selection, 1, (KeyConfig *)&savedKeyConfig, &work.cardState);
                PortLinkMenuSelected(currMenu); /* Port */
                currMenu = menu[currMenu].nextMenu;
                CopyGameData(gameData, &work.selection, 0, (KeyConfig *)&savedKeyConfig, &work.cardState);
                InitSlider();
                errorCount = 0;
                InitMenuDisplay(currMenu == 4 ? 110 : 150, 0);
entered:
                if (currMenu == 1 && linked) {
                    menuSendKey = 1;
                    raceType = 0;
                    menu[1].selection = 0;
                }
            }
        }
        goto cancel;
error:
        errorCount = 90;
cancel:
        if (GetMenuKey(4)) {
            if (currMenu == 19) {
                if ((unsigned short)(linkState - 3) < 2) goto noCancel;
                raceType = 0;
                killLink();
                PortLobbyLeave(); /* Port: out of the lobby: the second player's game closes */
            }
            if (menu[currMenu].prevMenu != 999 && currMenu != 10 && prevMenu != 10 && currMenu != 23 && prevMenu != 23 && currMenu != 22 && prevMenu != 22 && currMenu != 12 && prevMenu != 12) {
                PlayNote(49, 255, 15, 15, 256);
                CopyGameData(gameData, &work.selection, 0, (KeyConfig *)&savedKeyConfig, &work.cardState);
                { int from = currMenu; currMenu = menu[currMenu].prevMenu; PortLinkMenuCancelled(from, currMenu); } /* Port */
                errorCount = 0;
                InitMenuDisplay(currMenu == 4 ? 110 : 150, 0);
                InitSlider();
            }
        }
noCancel:
        prevMenu = 0;
serial:
        if (linked) {
            doSerialMenu(gameData, &work.selection);
            if (menuSendKey == 1 || menuRecvKey == 1) {
                killLink();
                raceType = 0;
                menu[1].selection = 0;
            }
        }
        if (currMenu == 0) {
            if (negcon) {
                if (playerControl[0].buttons != 0) goto idleReset;
                if (playerControl[0].thrust >= 31) goto idleReset;
                if ((unsigned)(playerControl[0].steering-120) >= 47) goto idleReset;
                idleCount = (s32)((u32)idleCount + 1);
                goto idleCheck;
            } else {
                if (playerControl[0].buttons == 0) {
                    idleCount = (s32)((u32)idleCount + 1);
                    goto idleCheck;
                }
            }
idleReset:
            idleCount = 0;
idleCheck:
            if (idleCount == 900) {
                if (!linked) {
                    negconCentre = 1;
                    work.attractMode = 1;
                }
            }
        }
    }
    if (gameType != 2 || raceType == 1) {
        cheatWeapon = 0;
        cheatDamage = 0;
        cheatRain = 0;
        cheatTime = 0;
        cheatMinigun = 0;
    }
    if (linked) {
        while (menuRecvKey != 2) {
            SfxDoSfx();
            buildFixedPad(padBuf1);
            quadIndex = CurrentScreen * 300;
            ClearOTagR(OT[CurrentScreen], 8192);
            ClearOTagR(OT2[CurrentScreen], 1600);
            DisplayScreen(textureTable[5 + menuTextures], (DrBox *)work.displayScreen);
            CentreText(msgWaitingForOtherPlayer, 1, 120, 8, 0);
            doSerialMenu(gameData, &work.selection);
            Swap();
        }
        ownShip = work.selection.ship;
        remoteShip = menuRecvMode;
        if (ownShip == remoteShip) {
            if (gameType == 0) remoteShip++;
            else ownShip++;
        }
    } else {
        remoteShip = -1;
        gameType = 2;
        ownShip = work.selection.ship;
        if (raceType == 4) {
            gameData->savedClass = gameData->shipClass;
            gameData->savedActTrackNum = gameData->actTrackNum;
            gameData->savedTrackNum = gameData->trackNum;
            continuesLeft = 3;
            gameData->class1Race = 0;
        } else {
            continuesLeft = 0;
            gameData->class1Race = 0;
        }
        gameData->class2Race = 0;
    }
    DrawSync(0);
    InitNewDisplay(gameData);
    CdMusicStop();
    fastFrame = 0;
    Close2DParticle();
    if (work.attractMode) {
        menuFlag = 1;
        menuShip = ownShip;
        menuActTrack = (unsigned char)gameData->actTrackNum;
        menuNormTrack = (unsigned char)gameData->trackNum;
        menuTrackNo = trackNo;
        savedSelection[0] = menu[1].selection;
        raceType = 99;
        return 1;
    }
    for (i = 0; i < 5; i++) {
        UpdateNotes(0);
        VSync(0);
    }
    return 0;
}


/* The polygon of the button being drawn, addressed from the start of the button table. */
#define BUTTON_POLY polys[buttonCount][CurrentScreen]

void DoButton(short x, short y, int requestedIcon, char *text)
{
    Texture *texture;
    short iconNo=requestedIcon;
    POLY_FT4 (*polys)[2];
    int frameSpace[2];
    int labelShift;
    if(buttonCount<30) {
        if(playerControl[0].head==2 && (unsigned short)requestedIcon<4) iconNo+=6;
        if((unsigned short)(iconNo-10)<2) texture=textureTable[4 + buttonTextures];
        else texture=textureTable[buttonTextures+(iconNo<PORT_ICON_ACTION?iconNo:0)]; /* Port: the controller screen's key icons have no console texture */
        {int glyphNo=buttonCount;int frame=CurrentScreen;
        polys=menuButtons;
        SetPolyFT4(&polys[glyphNo][frame]);}
        menuButtons[buttonCount][CurrentScreen] .r0=128;
        menuButtons[buttonCount][CurrentScreen] .g0=128;
        menuButtons[buttonCount][CurrentScreen] .b0=128;
        if(iconNo==10) {
            BUTTON_POLY.x0=x;
            BUTTON_POLY.y0=y;
            {unsigned short span=texture->textureH;
            BUTTON_POLY.y1=y;
            BUTTON_POLY.x2=x;
            BUTTON_POLY.x1=x+span;}
            BUTTON_POLY.y2=y+texture->textureW*4;
            BUTTON_POLY.x3=x+texture->textureH;
            BUTTON_POLY.y3=y+texture->textureW*4;
            BUTTON_POLY.u0=texture->u2;
            BUTTON_POLY.v0=texture->v2;
            BUTTON_POLY.u1=texture->u0;
            BUTTON_POLY.v1=texture->v0;
            BUTTON_POLY.u2=texture->u3;
            BUTTON_POLY.v2=texture->v3;
            BUTTON_POLY.u3=texture->u1;
            BUTTON_POLY.v3=texture->v1;
        } else if(iconNo==11) {
            BUTTON_POLY.x0=x;
            BUTTON_POLY.y0=y;
            {unsigned short span=texture->textureH;
            BUTTON_POLY.y1=y;
            BUTTON_POLY.x2=x;
            BUTTON_POLY.x1=x+span;}
            BUTTON_POLY.y2=y+texture->textureW*4;
            BUTTON_POLY.x3=x+texture->textureH;
            BUTTON_POLY.y3=y+texture->textureW*4;
            BUTTON_POLY.u0=texture->u1;
            BUTTON_POLY.v0=texture->v1;
            BUTTON_POLY.u1=texture->u3;
            BUTTON_POLY.v1=texture->v3;
            BUTTON_POLY.u2=texture->u0;
            BUTTON_POLY.v2=texture->v0;
            BUTTON_POLY.u3=texture->u2;
            BUTTON_POLY.v3=texture->v2;
        } else {
            BUTTON_POLY.x0=x;
            BUTTON_POLY.y0=y;
            {int span=texture->textureW;
            BUTTON_POLY.y1=y;
            BUTTON_POLY.x2=x;
            BUTTON_POLY.x1=x+span*4;}
            BUTTON_POLY.y2=y+texture->textureH;
            BUTTON_POLY.x3=x+texture->textureW*4;
            BUTTON_POLY.y3=y+texture->textureH;
            BUTTON_POLY.u0=texture->u0;
            BUTTON_POLY.v0=texture->v0;
            BUTTON_POLY.u1=(unsigned char)texture->u1+1;
            BUTTON_POLY.v1=texture->v1;
            BUTTON_POLY.u2=texture->u2;
            BUTTON_POLY.v2=(unsigned char)texture->v2+1;
            BUTTON_POLY.u3=(unsigned char)texture->u3+1;
            BUTTON_POLY.v3=(unsigned char)texture->v3+1;
        }
        {unsigned short page=GetTPage(0,0,texture->textureX,texture->textureY);
        short glyphNo=buttonCount;short frame=CurrentScreen;
        menuButtons[glyphNo][frame].tpage=page;
        menuButtons[glyphNo][frame].clut=texture->cba;}
        labelShift=PortButtonGlyph(&BUTTON_POLY,iconNo,x,y); /* Port: the glyph of the keyboard or gamepad in use; a wider glyph pushes the label right */
        AddPrim(OT[CurrentScreen],& menuButtons[buttonCount][CurrentScreen] );
        if(playerControl[0].head==2) {
            if(iconNo==9) Print(text,x-25,y+6,8);
            else Print(text,x+18+labelShift,y+6,8);
        } else {
            if(iconNo==3) Print(text,x-25,y+6,8);
            else Print(text,x+18+labelShift,y+6,8);
        }
        buttonCount++;
    }
}

#undef BUTTON_POLY


void InitMenuTextures(u_short *textureCount)
{
    heap = reserveHeap;
    *textureCount = TextureTableCount;
    LoadCmpFiles(LoadCompressedTextureSequence(textureSequenceName));
    heap = mainheapcopy;
}


void DrawMusicScreen(MusicScreen *menu)
{
    short row;

    for (row = 0; row < 6; row++) {
        AddPrim(OT[CurrentScreen], &menu->rows[row][CurrentScreen]);
    }
}


/* Sets up the textured bars, markers and arrows of the audio configuration screen. */
void InitMusicScreen(MusicScreen *musicScreen, int textureBase)
{
    s16 row, col;
    u16 textureIndex;
    Texture *texture;

    textureIndex = textureBase + 1;
    texture = textureTable[(s16)textureIndex];
    for (row = 0; row < 6; row++) {
        for (col = 0; col < 2; col++) {
            SetPolyFT4(&musicScreen->rows[row][col]);
            musicScreen->rows[row][col].r0 = 128;
            musicScreen->rows[row][col].g0 = 128;
            musicScreen->rows[row][col].b0 = 128;
            if (row == MUSIC_BAR) {
                (musicScreen->rows[MUSIC_BAR] + col)->x0 = 100;
                (musicScreen->rows[MUSIC_BAR] + col)->y0 = 0x56;
                (musicScreen->rows[MUSIC_BAR] + col)->x1 = texture->textureW * 4 + 100;
                (musicScreen->rows[MUSIC_BAR] + col)->y1 = 0x56;
                (musicScreen->rows[MUSIC_BAR] + col)->x2 = 100;
                (musicScreen->rows[MUSIC_BAR] + col)->y2 = texture->textureH + 0x56;
                (musicScreen->rows[MUSIC_BAR] + col)->x3 = texture->textureW * 4 + 100;
                (musicScreen->rows[MUSIC_BAR] + col)->y3 = texture->textureH + 0x56;
            } else if (row == MUSIC_MARKER) {
                (musicScreen->rows[MUSIC_MARKER] + col)->x0 = 100;
                (musicScreen->rows[MUSIC_MARKER] + col)->y0 = 0x56;
                (musicScreen->rows[MUSIC_MARKER] + col)->x1 = texture->textureW * 4 + 100;
                (musicScreen->rows[MUSIC_MARKER] + col)->y1 = 0x56;
                (musicScreen->rows[MUSIC_MARKER] + col)->x2 = 100;
                (musicScreen->rows[MUSIC_MARKER] + col)->y2 = texture->textureH + 0x56;
                (musicScreen->rows[MUSIC_MARKER] + col)->x3 = texture->textureW * 4 + 100;
                (musicScreen->rows[MUSIC_MARKER] + col)->y3 = texture->textureH + 0x56;
            } else if (row == SFX_BAR) {
                (musicScreen->rows[SFX_BAR] + col)->x0 = 100;
                (musicScreen->rows[SFX_BAR] + col)->y0 = 0x73;
                (musicScreen->rows[SFX_BAR] + col)->x1 = texture->textureW * 4 + 100;
                (musicScreen->rows[SFX_BAR] + col)->y1 = 0x73;
                (musicScreen->rows[SFX_BAR] + col)->x2 = 100;
                (musicScreen->rows[SFX_BAR] + col)->y2 = texture->textureH + 0x73;
                (musicScreen->rows[SFX_BAR] + col)->x3 = texture->textureW * 4 + 100;
                (musicScreen->rows[SFX_BAR] + col)->y3 = texture->textureH + 0x73;
            } else if (row == SFX_MARKER) {
                (musicScreen->rows[SFX_MARKER] + col)->x0 = 100;
                (musicScreen->rows[SFX_MARKER] + col)->y0 = 0x73;
                (musicScreen->rows[SFX_MARKER] + col)->x1 = texture->textureW * 4 + 100;
                (musicScreen->rows[SFX_MARKER] + col)->y1 = 0x73;
                (musicScreen->rows[SFX_MARKER] + col)->x2 = 100;
                (musicScreen->rows[SFX_MARKER] + col)->y2 = texture->textureH + 0x73;
                (musicScreen->rows[SFX_MARKER] + col)->x3 = texture->textureW * 4 + 100;
                (musicScreen->rows[SFX_MARKER] + col)->y3 = texture->textureH + 0x73;
            } else if (row == LEFT_ARROW) {
                (musicScreen->rows[LEFT_ARROW] + col)->x0 = 0x32;
                (musicScreen->rows[LEFT_ARROW] + col)->y0 = 0x91;
                (musicScreen->rows[LEFT_ARROW] + col)->x1 = texture->textureW * 4 + 0x32;
                (musicScreen->rows[LEFT_ARROW] + col)->y1 = 0x91;
                (musicScreen->rows[LEFT_ARROW] + col)->x2 = 0x32;
                (musicScreen->rows[LEFT_ARROW] + col)->y2 = texture->textureH + 0x91;
                (musicScreen->rows[LEFT_ARROW] + col)->x3 = texture->textureW * 4 + 0x32;
                (musicScreen->rows[LEFT_ARROW] + col)->y3 = texture->textureH + 0x91;
            } else {
                (musicScreen->rows[RIGHT_ARROW] + col)->x0 = 0xF0;
                (musicScreen->rows[RIGHT_ARROW] + col)->y0 = 0x91;
                (musicScreen->rows[RIGHT_ARROW] + col)->x1 = texture->textureW * 4 + 0xF0;
                (musicScreen->rows[RIGHT_ARROW] + col)->y1 = 0x91;
                (musicScreen->rows[RIGHT_ARROW] + col)->x2 = 0xF0;
                (musicScreen->rows[RIGHT_ARROW] + col)->y2 = texture->textureH + 0x91;
                (musicScreen->rows[RIGHT_ARROW] + col)->x3 = texture->textureW * 4 + 0xF0;
                (musicScreen->rows[RIGHT_ARROW] + col)->y3 = texture->textureH + 0x91;
            }
            (musicScreen->rows[row] + col)->u0 = texture->u0;
            (musicScreen->rows[row] + col)->v0 = texture->v0;
            (musicScreen->rows[row] + col)->u1 = texture->u1 + 1;
            (musicScreen->rows[row] + col)->v1 = texture->v1;
            (musicScreen->rows[row] + col)->u2 = texture->u2;
            (musicScreen->rows[row] + col)->v2 = texture->v2 + 1;
            (musicScreen->rows[row] + col)->u3 = texture->u3 + 1;
            (musicScreen->rows[row] + col)->v3 = texture->v3 + 1;
            (musicScreen->rows[row] + col)->tpage = GetTPage(0, 0, texture->textureX, texture->textureY);
            musicScreen->rows[row][col].clut = texture->cba;
        }
        if (row != MUSIC_BAR && row != MUSIC_MARKER) textureIndex++;
        texture = textureTable[(s16)textureIndex];
    }
}


void InitDrBox(Texture *texture, DrBox *box)
{
    short i;
    int y = 32;

    i = 0;
    box->coords[0] = 0x14;
    box->coords[1] = y;
    box->coords[2] = 0x100;
    box->coords[3] = 0x32;
    box->coords[4] = 0x114;
    box->coords[5] = y;
    box->coords[6] = 0x20;
    box->coords[7] = 0x32;
    box->coords[8] = 0x14;
    box->coords[9] = 0x52;
    box->coords[10] = 0x100;
    box->coords[11] = 0x62;
    box->coords[12] = 0x114;
    box->coords[13] = 0x52;
    box->coords[14] = 0x20;
    box->coords[15] = 0x62;
    box->coords[16] = 0x14;
    box->coords[17] = 0xB4;
    box->coords[18] = 0x100;
    box->coords[19] = 0x24;
    box->coords[20] = 0x114;
    box->coords[21] = 0xB4;
    box->coords[22] = 0x20;
    box->coords[23] = 0x24;
    do {
        SetPolyFT4(&box->rows[0][i]);
        box->rows[0][i].r0 = 0x80;
        box->rows[0][i].g0 = 0x80;
        box->rows[0][i].b0 = 0x80;
        box->rows[0][i].x0 = box->coords[0];
        box->rows[0][i].y0 = box->coords[1];
        box->rows[0][i].x1 = box->coords[0] + box->coords[2];
        box->rows[0][i].y1 = box->coords[1];
        box->rows[0][i].x2 = box->coords[0];
        box->rows[0][i].y2 = box->coords[1] + box->coords[3];
        box->rows[0][i].x3 = box->coords[0] + box->coords[2];
        box->rows[0][i].y3 = box->coords[1] + box->coords[3];
        box->rows[0][i].u0 = texture->u0;
        box->rows[0][i].v0 = texture->v0;
        box->rows[0][i].u1 = 0xFF;
        box->rows[0][i].v1 = texture->v1;
        box->rows[0][i].u2 = texture->u2;
        box->rows[0][i].v2 = (u8)texture->v0 + 0x32;
        box->rows[0][i].u3 = 0xFF;
        box->rows[0][i].v3 = (u8)texture->v0 + 0x32;
        box->rows[0][i].tpage = GetTPage(1, 0, texture->textureX, texture->textureY);
        box->rows[0][i].clut = texture->cba;

        SetPolyFT4(&box->rows[2][i]);
        box->rows[2][i].r0 = 0x80;
        box->rows[2][i].g0 = 0x80;
        box->rows[2][i].b0 = 0x80;
        box->rows[2][i].x0 = box->coords[8];
        box->rows[2][i].y0 = box->coords[9];
        box->rows[2][i].x1 = box->coords[8] + box->coords[10];
        box->rows[2][i].y1 = box->coords[9];
        box->rows[2][i].x2 = box->coords[8];
        box->rows[2][i].y2 = box->coords[9] + box->coords[11];
        box->rows[2][i].x3 = box->coords[8] + box->coords[10];
        box->rows[2][i].y3 = box->coords[9] + box->coords[11];
        box->rows[2][i].u0 = texture->u0;
        box->rows[2][i].v0 = (u8)texture->v0 + 0x32;
        box->rows[2][i].u1 = 0xFF;
        box->rows[2][i].v1 = (u8)texture->v1 + 0x32;
        box->rows[2][i].u2 = texture->u2;
        box->rows[2][i].v2 = (u8)texture->v0 - 0x6C;
        box->rows[2][i].u3 = 0xFF;
        box->rows[2][i].v3 = (u8)texture->v0 - 0x6C;
        box->rows[2][i].tpage = GetTPage(1, 0, texture->textureX, texture->textureY);
        box->rows[2][i].clut = texture->cba;

        SetPolyFT4(&box->rows[4][i]);
        box->rows[4][i].r0 = 0x80;
        box->rows[4][i].g0 = 0x80;
        box->rows[4][i].b0 = 0x80;
        box->rows[4][i].x0 = box->coords[16];
        box->rows[4][i].y0 = box->coords[17];
        box->rows[4][i].x1 = box->coords[16] + box->coords[18];
        box->rows[4][i].y1 = box->coords[17];
        box->rows[4][i].x2 = box->coords[16];
        box->rows[4][i].y2 = box->coords[17] + box->coords[19];
        box->rows[4][i].x3 = box->coords[16] + box->coords[18];
        box->rows[4][i].y3 = box->coords[17] + box->coords[19];
        box->rows[4][i].u0 = texture->u0;
        box->rows[4][i].v0 = (u8)texture->v0 - 0x6C;
        box->rows[4][i].u1 = 0xFF;
        box->rows[4][i].v1 = (u8)texture->v1 - 0x6C;
        box->rows[4][i].u2 = texture->u2;
        box->rows[4][i].v2 = (u8)texture->v0 - 0x48;
        box->rows[4][i].u3 = 0xFF;
        box->rows[4][i].v3 = (u8)texture->v0 - 0x48;
        box->rows[4][i].tpage = GetTPage(1, 0, texture->textureX, texture->textureY);
        box->rows[4][i].clut = texture->cba;
        i++;
    } while (i < 2);
    i = 0;
    do {
        SetPolyFT4(&box->rows[1][i]);
        box->rows[1][i].r0 = 0x80;
        box->rows[1][i].g0 = 0x80;
        box->rows[1][i].b0 = 0x80;
        box->rows[1][i].x0 = box->coords[4];
        box->rows[1][i].y0 = box->coords[5];
        box->rows[1][i].x1 = box->coords[4] + box->coords[6];
        box->rows[1][i].y1 = box->coords[5];
        box->rows[1][i].x2 = box->coords[4];
        box->rows[1][i].y2 = box->coords[5] + box->coords[7];
        box->rows[1][i].x3 = box->coords[4] + box->coords[6];
        box->rows[1][i].y3 = box->coords[5] + box->coords[7];
        box->rows[1][i].u0 = texture->u0;
        box->rows[1][i].v0 = texture->v0;
        box->rows[1][i].u1 = 0x20;
        box->rows[1][i].v1 = texture->v1;
        box->rows[1][i].u2 = texture->u2;
        box->rows[1][i].v2 = (u8)texture->v0 + 0x32;
        box->rows[1][i].u3 = 0x20;
        box->rows[1][i].v3 = (u8)texture->v0 + 0x32;
        box->rows[1][i].tpage = GetTPage(1, 0, texture->textureX + 0x80, texture->textureY);
        box->rows[1][i].clut = texture->cba;

        SetPolyFT4(&box->rows[3][i]);
        box->rows[3][i].r0 = 0x80;
        box->rows[3][i].g0 = 0x80;
        box->rows[3][i].b0 = 0x80;
        box->rows[3][i].x0 = box->coords[12];
        box->rows[3][i].y0 = box->coords[13];
        box->rows[3][i].x1 = box->coords[12] + box->coords[14];
        box->rows[3][i].y1 = box->coords[13];
        box->rows[3][i].x2 = box->coords[12];
        box->rows[3][i].y2 = box->coords[13] + box->coords[15];
        box->rows[3][i].x3 = box->coords[12] + box->coords[14];
        box->rows[3][i].y3 = box->coords[13] + box->coords[15];
        box->rows[3][i].u0 = texture->u0;
        box->rows[3][i].v0 = (u8)texture->v0 + 0x32;
        box->rows[3][i].u1 = 0x20;
        box->rows[3][i].v1 = (u8)texture->v1 + 0x32;
        box->rows[3][i].u2 = texture->u2;
        box->rows[3][i].v2 = (u8)texture->v0 - 0x6C;
        box->rows[3][i].u3 = 0x20;
        box->rows[3][i].v3 = (u8)texture->v0 - 0x6C;
        box->rows[3][i].tpage = GetTPage(1, 0, texture->textureX + 0x80, texture->textureY);
        box->rows[3][i].clut = texture->cba;

        SetPolyFT4(&box->rows[5][i]);
        box->rows[5][i].r0 = 0x80;
        box->rows[5][i].g0 = 0x80;
        box->rows[5][i].b0 = 0x80;
        box->rows[5][i].x0 = box->coords[20];
        box->rows[5][i].y0 = box->coords[21];
        box->rows[5][i].x1 = box->coords[20] + box->coords[22];
        box->rows[5][i].y1 = box->coords[21];
        box->rows[5][i].x2 = box->coords[20];
        box->rows[5][i].y2 = box->coords[21] + box->coords[23];
        box->rows[5][i].x3 = box->coords[20] + box->coords[22];
        box->rows[5][i].y3 = box->coords[21] + box->coords[23];
        box->rows[5][i].u0 = texture->u0;
        box->rows[5][i].v0 = (u8)texture->v0 - 0x6C;
        box->rows[5][i].u1 = 0x20;
        box->rows[5][i].v1 = (u8)texture->v1 - 0x6C;
        box->rows[5][i].u2 = texture->u2;
        box->rows[5][i].v2 = (u8)texture->v0 - 0x48;
        box->rows[5][i].u3 = 0x20;
        box->rows[5][i].v3 = (u8)texture->v0 - 0x48;
        box->rows[5][i].tpage = GetTPage(1, 0, texture->textureX + 0x80, texture->textureY);
        box->rows[5][i].clut = texture->cba;
        i++;
    } while (i < 2);
}


void DisplayScreen(Texture *texture, DrBox *box)
{
    short i;
    short j;
    s32 uv = 0x20;

    i = 0;
    do {
        SetPolyFT4(&box->rows[0][i]);
        box->rows[0][i].r0 = 0xFF;
        box->rows[0][i].g0 = 0xFF;
        box->rows[0][i].b0 = 0xFF;
        box->rows[0][i].x0 = 0;
        box->rows[0][i].y0 = 0;
        box->rows[0][i].x1 = texture->textureW;
        box->rows[0][i].y1 = 0;
        box->rows[0][i].x2 = 0;
        box->rows[0][i].y2 = texture->textureH;
        box->rows[0][i].x3 = texture->textureW;
        box->rows[0][i].y3 = texture->textureH;
        box->rows[0][i].u0 = 0;
        box->rows[0][i].v0 = 0;
        box->rows[0][i].u1 = texture->textureW;
        box->rows[0][i].v1 = 0;
        box->rows[0][i].u2 = 0;
        box->rows[0][i].v2 = (u8)texture->textureH - 1;
        box->rows[0][i].u3 = texture->textureW;
        box->rows[0][i].v3 = (u8)texture->textureH - 1;
        box->rows[0][i].tpage = GetTPage(1, 0, texture->textureX, texture->textureY);
        box->rows[0][i].clut = texture->cba;
        i++;
    } while (i < 2);
    i = 0;
    do {
        SetPolyFT4(&box->rows[1][i]);
        box->rows[1][i].r0 = 0xFF;
        box->rows[1][i].g0 = 0xFF;
        box->rows[1][i].b0 = 0xFF;
        box->rows[1][i].x0 = texture->textureW;
        box->rows[1][i].y0 = 0;
        box->rows[1][i].x1 = texture->textureW * 2;
        box->rows[1][i].y1 = 0;
        box->rows[1][i].x2 = texture->textureW;
        box->rows[1][i].y2 = texture->textureH;
        box->rows[1][i].x3 = texture->textureW * 2;
        box->rows[1][i].y3 = texture->textureH;
        box->rows[1][i].u0 = uv;
        box->rows[1][i].v0 = 0;
        box->rows[1][i].u1 = uv + texture->textureW;
        box->rows[1][i].v1 = 0;
        box->rows[1][i].u2 = uv;
        box->rows[1][i].v2 = (u8)texture->textureH - 1;
        box->rows[1][i].u3 = uv + texture->textureW;
        box->rows[1][i].v3 = (u8)texture->textureH - 1;
        box->rows[1][i].tpage = GetTPage(1, 0, texture->textureX + 0x40, texture->textureY);
        box->rows[1][i].clut = texture->cba;
        i++;
    } while (i < 2);
    j = 0;
    do {
        AddPrim(&OT[CurrentScreen][4095], &box->rows[j][CurrentScreen]);
        PortWideBackdrop(&box->rows[j][CurrentScreen]); /* Port: widescreen fills the sides with the backdrop */
        j++;
    } while (j < 2);
}


void DrawDrBox(Texture *unused, DrBox *box)
{
    short row;

    for (row = 0; row < 6; row++) {
        AddPrim(&OT[CurrentScreen][4095], &box->rows[row][CurrentScreen]);
    }
}


void CopyGameData(ConfigData *gameData, MenuSelection *selection, short saveSelection, KeyConfig *keyConfig, HiScoreData *cardState)
{
    short i;
    int tmp942, tmpc1, tmpc2;
    int tsx;
    unsigned short tsy;
    unsigned char nc;

    switch (currMenu) {
    case 1:
        if (saveSelection == 0) menu[currMenu].selection = raceType;
        else switch (menu[currMenu].selection) {
        case 0: raceType = 0; break;
        case 1: raceType = 1; break;
        case 2: raceType = 2; break;
        case 3: raceType = 3; break;
        case 4:
            if (gameData->rapierComplete) {
                gameData->actTrackNum = 1;
                gameData->trackNum = 0;
                raceType = 4;
                gameData->class2Race = 0;
            } else {
                gameData->actTrackNum = 1;
                gameData->trackNum = 0;
                raceType = 4;
                gameData->class1Race = 0;
            }
            break;
        }
        break;
    case 2:
        if (saveSelection == 0) {
            selection->shipClass = gameData->shipClass;
            selection->actTrackNum = gameData->actTrackNum;
            selection->trackNum = gameData->trackNum;
            if (gameData->shipClass == 0) selection->trackSelection = menu[5].selection;
            if (gameData->shipClass == 1) selection->trackSelection = menu[6].selection;
            if (gameData->shipClass == 2) selection->trackSelection = menu[7].selection;
            if (gameData->shipClass == 3) selection->trackSelection = menu[8].selection;
            selection->current = gameData->shipClass;
            PortOriginalMenuRestore(selection); /* Port: an original track chosen: its line */
        } else {
            gameData->shipClass = selection->shipClass;
            gameData->actTrackNum = selection->actTrackNum;
            gameData->trackNum = selection->trackNum;
            if (gameData->shipClass == 0) menu[5].selection = selection->trackSelection;
            if (gameData->shipClass == 1) menu[6].selection = selection->trackSelection;
            if (gameData->shipClass == 2) menu[7].selection = selection->trackSelection;
            if (gameData->shipClass == 3) menu[8].selection = selection->trackSelection;
            menu[2].selection = selection->current;
        }
        break;
    case 5:
        if (saveSelection == 0) {
            selection->shipClass = gameData->shipClass;
            selection->actTrackNum = gameData->actTrackNum;
            selection->current = menu[currMenu].selection;
        } else {
            gameData->shipClass = selection->shipClass;
            gameData->actTrackNum = selection->actTrackNum;
            menu[currMenu].selection = selection->current;
        }
        break;
    case 6:
        if (saveSelection == 0) {
            selection->shipClass = gameData->shipClass;
            selection->actTrackNum = gameData->actTrackNum;
            selection->current = menu[currMenu].selection;
        } else {
            gameData->shipClass = selection->shipClass;
            gameData->actTrackNum = selection->actTrackNum;
            menu[currMenu].selection = selection->current;
        }
        break;
    case 7:
        if (saveSelection == 0) {
            selection->shipClass = gameData->shipClass;
            selection->actTrackNum = gameData->actTrackNum;
            selection->current = menu[currMenu].selection;
        } else {
            gameData->shipClass = selection->shipClass;
            gameData->actTrackNum = selection->actTrackNum;
            menu[currMenu].selection = selection->current;
        }
        break;
    case 8:
        if (saveSelection == 0) {
            selection->shipClass = gameData->shipClass;
            selection->actTrackNum = gameData->actTrackNum;
            selection->current = menu[currMenu].selection;
        } else {
            gameData->shipClass = selection->shipClass;
            gameData->actTrackNum = selection->actTrackNum;
            menu[currMenu].selection = selection->current;
        }
        break;
    case 4:
        if (saveSelection == 0) {
            selection->ship = ownShip;
            selection->current = menu[currMenu].selection;
        } else {
            ownShip = selection->ship;
            menu[currMenu].selection = selection->current;
        }
        break;
    case 9:
        if (saveSelection == 0) {
            selection->musicVolume = gameData->musicVolume;
            selection->otherMusic = gameData->otherMusicVolume;
            selection->effectVolume = gameData->effectVolume;
            selection->otherEffect = gameData->otherEffectVolume;
            selection->music = gameData->cdTrack;
            selection->stereo = gameData->soundMode;
            selection->current = 0;
            menu[currMenu].selection = 0;
            SfxEffectLRPan(ambientVoice1, 15);
            SfxEffectFBPan(ambientVoice1, 15);
            if (gameData->soundMode) sfxMode = 1;
            else sfxMode = 4;
        } else {
            gameData->musicVolume = selection->musicVolume;
            gameData->otherMusicVolume = selection->otherMusic;
            gameData->effectVolume = selection->effectVolume;
            gameData->otherEffectVolume = selection->otherEffect;
            gameData->cdTrack = selection->music;
            gameData->soundMode = selection->stereo;
            selection->current = 0;
            menu[currMenu].selection = 0;
            SfxEffectLRPan(ambientVoice1, 15);
            SfxEffectFBPan(ambientVoice1, 15);
            if (gameData->soundMode) sfxMode = 1;
            else sfxMode = 4;
        }
        TerminateSound();
        SfxSetMode(sfxMode);
        StartControlledSound();
        SetCdVolume(((gameData->musicVolume * 255) / 100) & 255);
        SetSoundVolume(((gameData->effectVolume * 255) / 100) & 255);
        SetVolume(ambientVoice1, 0);
        cdFlag[0] = 1;
        break;
    case 10:
    case 22:
        if (saveSelection == 0) {
            selection->keys[0] = keyConfig->keys[0];
            selection->calibration[0] = keyConfig->calibration[0];
            selection->keys[1] = keyConfig->keys[1];
            selection->calibration[1] = keyConfig->calibration[1];
            keyConfig->active = 0;
            selection->current = 0;
            replayBuffer->disabled = 1;
        }
        break;
    case 21:
        if (saveSelection == 0) {
            selection->negconMin = gameData->negconMin;
            selection->negconCenter = gameData->negconCentre;
            selection->negconType = gameData->negconType;
            selection->negconFlags[0] = 0;
            selection->negconFlags[1] = 0;
            selection->negconFlags[2] = 0;
            twist = gameData->negconMin;
            currScreen = gameData->negconType;
            nc = gameData->negconCentre;
            turnDivisor = nc;
            if (nc < 128) turnDivisor = ~nc;
            menu[currMenu].selection = 0;
            for (i = 0; i < 3; i++) negconEditing[i] = 0;
            replayBuffer->disabled = 1;
        } else {
            gameData->negconMin = selection->negconMin;
            gameData->negconCentre = selection->negconCenter;
            gameData->negconType = selection->negconType;
            selection->negconFlags[0] = 0;
            selection->negconFlags[1] = 0;
            selection->negconFlags[2] = 0;
            twist = selection->negconMin;
            currScreen = selection->negconType;
            nc = selection->negconCenter;
            turnDivisor = nc;
            if (nc < 128) turnDivisor = ~nc;
            menu[currMenu].selection = 0;
            for (i = 0; i < 3; i++) negconEditing[i] = 0;
        }
        break;
    case 15:
        if (saveSelection == 0) {
            selection->screenX = gameData->screenX;
            ((volatile MenuSelection *)selection)->screenY = gameData->screenY;
            tsx = (unsigned short)selection->screenX;
            DispEnv1.screen.x = tsx;
            DispEnv0.screen.x = tsx;
            tsy = *(volatile unsigned short *)&selection->screenY;
            DispEnv1.screen.y = tsy;
            DispEnv0.screen.y = tsy;
            selection->screenOffsetState = gameData->screenOffsetState;
            selection->weapons = gameData->weaponsOn;
            selection->moveScreen = 0;
            selection->internalView = gameData->internalView;
            strcpy(selection->fileName, gameData->name);
            strcpy((char *)cardState->nameEntered, selection->fileName);
            selection->current = 0;
            menu[currMenu].selection = 0;
            InitDefaultName(gameData, cardState, 1);
        } else {
            gameData->screenOffsetState = selection->screenOffsetState;
            gameData->weaponsOn = selection->weapons;
            gameData->moveScreen = 0;
            gameData->internalView = selection->internalView;
            strcpy(selection->fileName, (char *)cardState->nameEntered);
            strcpy(gameData->name, selection->fileName);
            menu[currMenu].selection = 0;
            selection->current = 0;
            gameData->screenX = selection->screenX;
            gameData->screenY = selection->screenY;
            InitMenuDisplay(120, 0);
        }
        break;
    case 16:
        if (saveSelection == 0) {
            selection->screenX = gameData->screenX;
            ((volatile MenuSelection *)selection)->screenY = gameData->screenY;
            tsx = (unsigned short)selection->screenX;
            DispEnv1.screen.x = tsx;
            DispEnv0.screen.x = tsx;
            tsy = *(volatile unsigned short *)&selection->screenY;
            DispEnv1.screen.y = tsy;
            DispEnv0.screen.y = tsy;
        } else {
            gameData->screenX = selection->screenX;
            gameData->screenY = selection->screenY;
            InitMenuDisplay(120, 0);
        }
        break;
    case 12:
        if (saveSelection == 0) {
            myFinished = 0;
            fadescreen = 0;
            selection->timer = 4;
            selection->scoreIndex = 0;
            selection->record = 0;
            for (i = 0; i < 10; i++) selection->scoreFlags[i] = 0;
            __builtin_memcpy(&selection->scoreName, defaultCardName, 6);
            selection->highScoreScratchA6 = 0;
            selection->highScoreScratchA4 = 0;
            selection->highScoreScratchA8 = 0;
            selection->highScoreScratchAA = 0;
            selection->highScoreScratchAC = 0;
        }
        break;
    case 23:
        if (saveSelection == 0) {
            MakePassword(gameData);
            selection->password = gameData->generated;
            tmpc1 = gameData->generatedCheck;
            ((volatile MenuSelection *)selection)->passwordCheck = tmpc1;
        } else {
            gameData->entered = selection->password;
            tmpc2 = selection->passwordCheck;
            gameData->enteredCheck = tmpc2;
        }
        menu[currMenu].selection = 0;
        menu[currMenu].mode = 0;
        break;
    }
}


typedef union CardGridCell {
    s32 w;
    u16 h;
    SaveSelection *volatile save;
} CardGridCell;

#define cursorSlot   (*(volatile s32 *)&grid[0].w)
#define activeCard   (*(volatile s32 *)&grid[1].w)
#define selection    grid[2].save
#define highlight    grid[8].w
#define rowCount     grid[3].h
#define colCount     grid[4].h
#define cardCount    grid[5].h
#define iconYPos     grid[6].h
#define slotIndex    grid[7].h
#define fadeValue    grid[9].h

extern u8 msgE[];
extern s32 cardFlash;
extern s32 prevCard;
int DisplayCardBack(MenuPolys *polys, int cursorArg, int cardArg, SaveSelection *selectionArg)
{
    CardGridCell grid[10];
    u8 fadeByte;
    s16 bottom;
    s16 fadeSpriteNext;
    s16 fadeSpriteNextB;
    s16 plainSpriteNext;
    s16 resetNextA;
    s16 resetNextB;
    s16 lastNext;
    s16 right;
    s16 plainSprite;
    s16 fadeSprite;
    s16 resetSprite;
    s16 lastSprite;
    s16 entryIndex;
    s16 prevEntry;
    s16 iconX;
    s16 nextEntry;
    s16 prevNext;
    s32 nameDiff;
    s32 plainSpriteShifted;
    s32 resetShiftedA;
    s32 resetShiftedB;
    s32 lastShifted;
    s32 slotShifted;
    s16 slotNow, iconY, slotRow;
    s16 screenNow;
    Texture *iconTexture;
    u16 count4, count3, count5;
    s32 page;
    u_long *otEntry;
    u16 clutX;
    u16 clutY;
    u8 typeB;
    u8 typeA;
    POLY_F4 *background;
    POLY_FT4 *backdrop;
    POLY_FT4 *fadePolyA;
    POLY_FT4 *fadePolyB;
    POLY_F4 *plainPoly;
    POLY_FT4 *resetPolyA;
    POLY_FT4 *resetPolyB;
    POLY_FT4 *lastPoly;
    POLY_FT4 *iconPoly;
    iconX = 0x2D;
    cursorSlot = cursorArg;
    activeCard = cardArg;
    selection = selectionArg;
    slotIndex = 0;
    iconYPos = 0x55;
    fadeValue = cardFlash % 255;
    if (cardArg == 0) {
        highlight = cursorSlot;
    } else {
        highlight = cursorSlot + 0xF;
    }
    cardCount = 0;
    do {
        rowCount = 0;
        do {
            colCount = 0;
            do {
                slotRow = (*(volatile u16 *)&slotIndex);
                right = iconX + 0x10;
                background = &polys->background[slotRow][CurrentScreen];
                background->x0 = iconX;
                iconY = (*(volatile u16 *)&iconYPos);
                background->x1 = right;
                background->x2 = iconX;
                background->x3 = right;
                bottom = iconY + 0x10;
                background->y0 = iconY;
                background->y1 = iconY;
                background->y2 = bottom;
                background->y3 = bottom;
                backdrop = &polys->backdrop[slotRow][CurrentScreen];
                backdrop->x0 = iconX;
                backdrop->y0 = iconY;
                backdrop->x1 = right;
                backdrop->y1 = iconY;
                backdrop->x2 = iconX;
                backdrop->y2 = bottom;
                backdrop->x3 = right;
                backdrop->y3 = bottom;
                if (selection->selected != 0) {
                    plainSprite = 0;
                    if (slotRow == highlight) {
                        polys->background[slotRow][CurrentScreen].r0 = 0x80;
                        polys->background[slotRow][CurrentScreen].g0 = 0;
                        entryIndex = 0;
                        polys->background[slotRow][CurrentScreen].b0 = 0;
                        do {
                            nameDiff = strncmp(cardSlots[activeCard].entry[cursorSlot].filename, cardSlots[activeCard].entry[entryIndex].filename, 0x14);
                            nextEntry = entryIndex + 1;
                            if ((nameDiff == 0) && ((cardSlots[activeCard].entry[entryIndex].multi == 1) || (nextEntry = entryIndex + 1, (cursorSlot == entryIndex)))) {
                                fadeSprite = 0;
                                if (activeCard == 0) {
                                    do {
                                        fadePolyA = &polys->backdrop[entryIndex][fadeSprite];
                                        fadeSpriteNext = fadeSprite + 1;
                                        fadeByte = (u8)fadeValue;
                                        fadePolyA->r0 = fadeByte;
                                        fadeSprite = fadeSpriteNext;
                                        fadePolyA->g0 = fadeByte;
                                        fadePolyA->b0 = fadeByte;
                                    }
                                    while (fadeSpriteNext < 2);
                                    nextEntry = entryIndex + 1;
                                } else {
                                    do {
                                        fadePolyB = &polys->backdrop[entryIndex + 0xF][fadeSprite];
                                        fadeSpriteNextB = fadeSprite + 1;
                                        fadeByte = (u8)fadeValue;
                                        fadePolyB->r0 = fadeByte;
                                        fadeSprite = fadeSpriteNextB;
                                        fadePolyB->g0 = fadeByte;
                                        fadePolyB->b0 = fadeByte;
                                    }
                                    while (fadeSpriteNextB < 2);
                                    nextEntry = entryIndex + 1;
                                }
                            }
                            entryIndex = nextEntry;
                        }
                        while (nextEntry < 0xF);
                    } else {
                        do {
                            plainSpriteShifted = plainSprite << 0x10;
                            plainSpriteNext = plainSprite + 1;
                            plainSprite = plainSpriteNext;
                            plainPoly = &polys->background[slotRow][plainSpriteShifted >> 0x10];
                            plainPoly->r0 = 0x27;
                            plainPoly->g0 = 0xC8;
                            plainPoly->b0 = 0xDE;
                        }
                        while (plainSpriteNext < 2);
                        prevEntry = 0;
                        do {
                            if (strncmp(cardSlots[prevCard].entry[cursorSlot].filename, cardSlots[prevCard].entry[prevEntry].filename, 0x14) == 0) {
                                if (cardSlots[prevCard].entry[prevEntry].multi == 0) {
                                    prevNext = prevEntry + 1;
                                    if (cursorSlot != prevEntry) {
                                        goto unhighlight;
                                    }
                                } else {
                                    goto unhighlightDone;
                                }
                            } else {
unhighlight:
                                resetSprite = 0;
                                if (prevCard == 0) {
                                    do {
                                        resetShiftedA = resetSprite << 0x10;
                                        resetNextA = resetSprite + 1;
                                        resetSprite = resetNextA;
                                        resetPolyA = &polys->backdrop[prevEntry][resetShiftedA >> 0x10];
                                        resetPolyA->r0 = 0x80;
                                        resetPolyA->g0 = 0x80;
                                        resetPolyA->b0 = 0x80;
                                    }
                                    while (resetNextA < 2);
                                    prevNext = prevEntry + 1;
                                } else {
                                    do {
                                        resetShiftedB = resetSprite << 0x10;
                                        resetNextB = resetSprite + 1;
                                        resetSprite = resetNextB;
                                        resetPolyB = &polys->backdrop[prevEntry + 0xF][resetShiftedB >> 0x10];
                                        resetPolyB->r0 = 0x80;
                                        resetPolyB->g0 = 0x80;
                                        resetPolyB->b0 = 0x80;
                                    }
                                    while (resetNextB < 2);
unhighlightDone:
                                    prevNext = prevEntry + 1;
                                }
                            }
                            prevEntry = prevNext;
                        }
                        while (prevNext < 0xF);
                        lastSprite = 0;
                        if (prevCard != activeCard) {
                            s16 lastSlot = (*(volatile u16 *)&slotIndex);
                            do {
                                lastShifted = lastSprite << 0x10;
                                lastNext = lastSprite + 1;
                                lastSprite = lastNext;
                                lastPoly = &polys->backdrop[lastSlot][lastShifted >> 0x10];
                                lastPoly->r0 = 0x80;
                                lastPoly->g0 = 0x80;
                                lastPoly->b0 = 0x80;
                            }
                            while (lastNext < 2);
                        }
                    }
                } else {
                    polys->background[slotRow][CurrentScreen].r0 = 0x27;
                    polys->background[slotRow][CurrentScreen].g0 = 0xC8;
                    polys->background[slotRow][CurrentScreen].b0 = 0xDE;
                    polys->backdrop[slotRow][CurrentScreen].r0 = 0x80;
                    polys->backdrop[slotRow][CurrentScreen].g0 = 0x80;
                    polys->backdrop[slotRow][CurrentScreen].b0 = 0x80;
                }
                slotNow = (*(volatile u16 *)&slotIndex);
                if ((((s32)slotNow / 15) << 0x10) == 0) {
                    if (card1Exists != 0) {
                        if (selection->timer == 0) {
                            typeA = cardSlots[0].entry[slotNow].type;
                            if (typeA & 4) {
                                polys->backdrop[slotNow][CurrentScreen].u0 = cardIconInfo->u0;
                                polys->backdrop[slotNow][CurrentScreen].v0 = cardIconInfo->v0;
                                polys->backdrop[slotNow][CurrentScreen].u1 = cardIconInfo->u1;
                                polys->backdrop[slotNow][CurrentScreen].v1 = cardIconInfo->v1;
                                polys->backdrop[slotNow][CurrentScreen].u2 = cardIconInfo->u2;
                                polys->backdrop[slotNow][CurrentScreen].v2 = cardIconInfo->v2;
                                polys->backdrop[slotNow][CurrentScreen].u3 = cardIconInfo->u3;
                                polys->backdrop[slotNow][CurrentScreen].v3 = cardIconInfo->v3;
                                page = GetTPage(0, 0, cardIconInfo->textureX, cardIconInfo->textureY);
                                screenNow = CurrentScreen;
                                iconTexture = cardIconInfo;
                                polys->backdrop[slotNow][screenNow].tpage = page;
                                polys->backdrop[slotNow][screenNow].clut = iconTexture->cba;
                                otEntry = OT[screenNow];
                                iconPoly = &polys->backdrop[slotNow][screenNow];
                                goto drawIcon;
                            }
                            if (typeA & 2) {
                                polys->backdrop[slotNow][CurrentScreen].u0 = cardSlots[0].entry[slotNow].icon[0];
                                polys->backdrop[slotNow][CurrentScreen].v0 = cardSlots[0].entry[slotNow].icon[1];
                                polys->backdrop[slotNow][CurrentScreen].u1 = cardSlots[0].entry[slotNow].icon[2];
                                polys->backdrop[slotNow][CurrentScreen].v1 = cardSlots[0].entry[slotNow].icon[3];
                                polys->backdrop[slotNow][CurrentScreen].u2 = cardSlots[0].entry[slotNow].icon[4];
                                polys->backdrop[slotNow][CurrentScreen].v2 = cardSlots[0].entry[slotNow].icon[5];
                                polys->backdrop[slotNow][CurrentScreen].u3 = cardSlots[0].entry[slotNow].icon[6];
                                polys->backdrop[slotNow][CurrentScreen].v3 = cardSlots[0].entry[slotNow].icon[7];
                                page = GetTPage(0, 0, 0x280, 0);
                                polys->backdrop[slotNow][CurrentScreen].tpage = page;
                                clutY = cardSlots[0].entry[slotNow].y;
                                clutX = cardSlots[0].entry[slotNow].x;
                                iconPoly = &polys->backdrop[slotNow][CurrentScreen];
                                goto setClut;
                            }
                            goto drawError;
                        }
                        goto rowRefresh;
                    }
                    goto rowDone;
                } else if ((cardPresent1 != 0) && (selection->timer == 0)) {
                    typeB = cardSlots[1].entry[slotNow - 0xF].type;
                    if (typeB & 4) {
                        polys->backdrop[slotNow][CurrentScreen].u0 = cardIconInfo->u0;
                        polys->backdrop[slotNow][CurrentScreen].v0 = cardIconInfo->v0;
                        polys->backdrop[slotNow][CurrentScreen].u1 = cardIconInfo->u1;
                        polys->backdrop[slotNow][CurrentScreen].v1 = cardIconInfo->v1;
                        polys->backdrop[slotNow][CurrentScreen].u2 = cardIconInfo->u2;
                        polys->backdrop[slotNow][CurrentScreen].v2 = cardIconInfo->v2;
                        polys->backdrop[slotNow][CurrentScreen].u3 = cardIconInfo->u3;
                        polys->backdrop[slotNow][CurrentScreen].v3 = cardIconInfo->v3;
                        page = GetTPage(0, 0, cardIconInfo->textureX, cardIconInfo->textureY);
                        screenNow = CurrentScreen;
                        iconTexture = cardIconInfo;
                        polys->backdrop[slotNow][screenNow].tpage = page;
                        polys->backdrop[slotNow][screenNow].clut = iconTexture->cba;
                        otEntry = OT[screenNow];
                        iconPoly = &polys->backdrop[slotNow][screenNow];
                        goto drawIcon;
                    }
                    if (typeB & 2) {
                        polys->backdrop[slotNow][CurrentScreen].u0 = cardSlots[1].entry[slotNow - 0xF].icon[0];
                        polys->backdrop[slotNow][CurrentScreen].v0 = cardSlots[1].entry[slotNow - 0xF].icon[1];
                        polys->backdrop[slotNow][CurrentScreen].u1 = cardSlots[1].entry[slotNow - 0xF].icon[2];
                        polys->backdrop[slotNow][CurrentScreen].v1 = cardSlots[1].entry[slotNow - 0xF].icon[3];
                        polys->backdrop[slotNow][CurrentScreen].u2 = cardSlots[1].entry[slotNow - 0xF].icon[4];
                        polys->backdrop[slotNow][CurrentScreen].v2 = cardSlots[1].entry[slotNow - 0xF].icon[5];
                        polys->backdrop[slotNow][CurrentScreen].u3 = cardSlots[1].entry[slotNow - 0xF].icon[6];
                        polys->backdrop[slotNow][CurrentScreen].v3 = cardSlots[1].entry[slotNow - 0xF].icon[7];
                        page = GetTPage(0, 0, 0x280, 0);
                        polys->backdrop[slotNow][CurrentScreen].tpage = page;
                        clutY = cardSlots[1].entry[slotNow - 0xF].y;
                        clutX = cardSlots[1].entry[slotNow - 0xF].x;
                        iconPoly = &polys->backdrop[slotNow][CurrentScreen];
setClut:
                        iconPoly->clut = ((clutY << 6) | ((s32) ((u32)clutX << 0x10) >> 0x14));
                        otEntry = OT[CurrentScreen];
drawIcon:
                        AddPrim(otEntry, iconPoly);
                    } else {
drawError:
                        Print(msgE, iconX, (s16) (*(volatile u16 *)&iconYPos), 8);
                    }
                }
rowRefresh:
                slotNow = *(volatile s16 *)&slotIndex;
rowDone:
                slotShifted = slotNow << 0x10;
                slotIndex = slotNow+1;
                AddPrim(OT[CurrentScreen], &polys->background[slotShifted >> 0x10][CurrentScreen]); /* Port: the original passed CurrentScreen as an extra, unread third argument */
                iconX += 19;
                count4 = (*(volatile u16 *)&colCount) + 1;
                *(volatile u16 *)&colCount = count4;
            }
            while ((s16)count4 < 3);
            iconX = 223;
            if ((s16)cardCount == 0) iconX = 45;
            *(volatile u16 *)&iconYPos = (*(volatile u16 *)&iconYPos) + 19;
            count3 = (*(volatile u16 *)&rowCount) + 1;
            *(volatile u16 *)&rowCount = count3;
        }
        while ((s16)count3 < 5);
        iconYPos = 85;
        iconX = 223;
        count5 = (*(volatile u16 *)&cardCount) + 1;
        cardCount = count5;
    }
    while ((s16)count5 < 2);
    prevCard = activeCard;
    cardFlash = (s32)((u32)cardFlash+0x14U);
    return 0; /* Port: the console returned whatever was left in $v0; the only caller (LoadSave) ignores the result */
}

#undef cursorSlot
#undef activeCard
#undef selection
#undef highlight
#undef rowCount
#undef colCount
#undef cardCount
#undef iconYPos
#undef slotIndex
#undef fadeValue


void setupBackgrndPolys(short firstCard, int endCard)
{
    short card;
    int status;

    for (card = firstCard; card < endCard; card++) {
        status = GetCardInfo(card);
        if (status == 1) goto statusDone;
        if (status < 2) goto statusDone;
        if (status == 2) goto two;
        if (status == 3) goto three;
        goto statusDone;
two:
        printf("%s", msgNoCard);
        if (card == 0) TrackHiCount = 0;
        if (card == 1) cardDirLoaded1 = 0;
        goto statusDone;
three:
        ClearEventHW();
        if (card == 0) _card_clear(0);
        else _card_clear(16);
        CheckHWCardEvents();
statusDone:
        LoadCard(card);
        switch (CardContentStatus()) {
        case 0:
            if (card == 0) cardUnformatted0 = 0;
            else cardUnformatted1 = 0;
            break;
        case 3:
            if (card == 0) cardUnformatted0 = 1;
            else cardUnformatted1 = 1;
            break;
        }
    }
    LoadCardFiles(cardSlots, 0);
    LoadCardFiles(cardSlots, 1);
}


void InitCard(s32 card)
{
    s16 slot = 0;
    Label6 *defaultName = defaultCardName;

    for (; slot < 15; slot++) {
        cardSlots[card].entry[slot].name[0] = 0;
        cardSlots[card].entry[slot].label = *defaultName;
        cardSlots[card].entry[slot].used = 0;
        cardSlots[card].entry[slot].filename[0] = 0;
        cardSlots[card].entry[slot].type = 1;
    }
}


void setInputDevices(int port)
{
    s32 freeSlots;
    s32 status;
    s32 initialStatus;
    s16 fileState;
    s16 cardState;

    cardState = 10;
    fileState = 10;
    status = GetCardInfo(port);
    initialStatus = status;
    switch (status) {
    case 0:
        if (port == 0) {
            cardSettleCount0 += 1;
            cardState = status;
        } else {
            cardSettleCount1 += 1;
            fileState = status;
        }
        break;

    case 1:
        if (port != 0) {
            cardPresent1 = 0;
            cardSettleCount1 = 0;
        } else {
            card1Exists = 0;
            cardSettleCount0 = 0;
        }
        break;

    case 2:
        if (port == 0) {
            cardSettleCount0 = 0;
            TrackHiCount = 0;
            cardState = status;
        } else {
            fileState = status;
            cardSettleCount1 = 0;
            cardDirLoaded1 = 0;
        }
        InitCard(port);
        if (port == 0) {
            status = 0;
            card1Exists = status;
        } else {
            status = 0;
            cardPresent1 = status;
        }
        break;

    case 3:
        ClearEventHW();
        if (port == 0) {
            _card_clear(0);
            cardSettleCount0 = 0;
        } else {
            _card_clear(0x10);
            cardSettleCount1 = 0;
        }
        CheckHWCardEvents();
        LoadCard(port);
        status = CardContentStatus();
        switch (status) {
        case 0:
            LoadCardFiles(cardSlots, port);
            if (port == 0) {
                cardUnformatted0 = 0;
                cardSettleCount0 = 0;
            } else {
                cardUnformatted1 = 0;
                cardSettleCount1 = 0;
            }
            break;

        case 1:
            if (port == 0) {
                card1Exists = 0;
                cardSettleCount0 = 0;
            } else {
                cardPresent1 = 0;
                cardSettleCount1 = 0;
            }
            break;

        case 2:
            if (port == 0) {
                card1Exists = 0;
                cardSettleCount0 = 0;
            } else {
                cardPresent1 = 0;
                cardSettleCount1 = 0;
            }
            break;

        case 3:
            if (port == 0) {
                cardUnformatted0 = 1;
                cardSettleCount0 = 0;
            } else {
                cardUnformatted1 = 1;
                cardSettleCount1 = 0;
            }
            break;
        }
        break;
    }

    port += ((u32)initialStatus << 5) & 31;
    freeSlots = CheckCardFull(port);
    if (port == 0 && cardSettleCount0 == 1 && (short)freeSlots == 0) {
        LoadCardFiles(cardSlots, 0);
    } else if (port == 1 && cardSettleCount1 == port && (short)freeSlots == 0) {
        LoadCardFiles(cardSlots, 1);
    }
    if (port == 0) {
        if (prevCardStatus0 == 2 && cardState == 0) {
            LoadCardFiles(cardSlots, 0);
        }
        if (cardState == 0 && TrackHiCount == 0) {
            LoadCardFiles(cardSlots, 0);
        }
    } else {
        if (prevCardStatus1 == 2 && fileState == 0) {
            LoadCardFiles(cardSlots, port);
        }
        if (fileState == 0 && cardDirLoaded1 == 0) {
            LoadCardFiles(cardSlots, port);
        }
    }
    if (port == 0) {
        prevCardStatus0 = cardState;
        if (cardDirChanged0 != 0) {
            card1Exists = 0;
        }
    } else {
        prevCardStatus1 = fileState;
        if (cardDirChanged1 != 0) {
            cardPresent1 = 0;
        }
    }
}


extern char msgEnterFileName[];
extern char msgDel[];
extern char msgEnd[];

/* Port: the byte offsets into the polygons are added to uintptr_t, not int. */
short EnterFileName(MenuPolys *polys, SaveSelection *selection)
{
    s16 glyphX = 0x6C;
    struct { s8 character; s8 end; } entry = {'A', 0};
    s16 row, column;
    s16 right, left;
    s16 glyph;
    int screen;
    MenuPolys *box;

    if (keysPressed & 0x20) {
        selection->nameActive += 1;
        if (selection->nameActive >= 0x1C) selection->nameActive = 0x1B;
        else Sound(8);
    } else if (keysPressed & 0x80) {
        selection->nameActive -= 1;
        if (selection->nameActive <= 0) selection->nameActive = 0;
        else Sound(8);
    } else if (keysPressed & 0x10) {
        if (selection->nameActive == 0x1A) selection->nameActive -= 1;
        else if (selection->nameActive == 0x1B) selection->nameActive -= 2;
        else selection->nameActive -= 0xD;
        if (selection->nameActive < 0) selection->nameActive += 0xD;
        else Sound(8);
    } else if (keysPressed & 0x40) {
        if (selection->nameActive == 0x1A) selection->nameActive += 1;
        else if (selection->nameActive == 0x19) selection->nameActive += 2;
        else if (selection->nameActive >= 0xD) selection->nameActive = 0x1B;
        else selection->nameActive += 0xD;
        if (selection->nameActive >= 0x1C) selection->nameActive = 0x1B;
        else Sound(8);
    } else if (GetMenuKey(5)) {
        Sound(9);
        if ((u32)((u16)selection->nameActive - 0x1A) >= 2) {
            if (selection->nameLength < 8) {
                selection->fileName[selection->nameLength] = selection->nameActive + 'A';
                selection->nameLength += 1;
            }
        } else {
            if (selection->nameActive != 0x1A) return 1;
            if (selection->nameLength > 0) {
                selection->nameLength -= 1;
                selection->fileName[selection->nameLength] = 0;
            }
        }
    } else if (GetMenuKey(4)) {
        Sound(0x31);
        selection->dupeCheck = 0;
        selection->confirmed = 0;
        selection->selected = 0;
        selection->enteringName = 0;
        selection->nameReady = 0;
        return 0;
    }
    row = selection->nameActive / 13;
    column = selection->nameActive % 13;
    if (row == 2) {
        if (selection->nameActive == 0x1A) {
            screen = CurrentScreen;
            left = 0x77;
            right = 0x8B;
        } else {
            left = 0xB3;
            screen = CurrentScreen;
            right = 0xC7;
        }
        box = (MenuPolys *)(screen * sizeof(POLY_F4) + (uintptr_t)polys);
        box->redBox[0].y0 = 0x79;
        box->redBox[0].y1 = 0x79;
        box->redBox[0].x0 = left;
        box->redBox[0].x1 = right;
        box->redBox[0].x2 = left;
        box->redBox[0].y2 = 0x80;
        box->redBox[0].x3 = right;
        box->redBox[0].y3 = 0x80;
    } else {
        ((MenuPolys *)(CurrentScreen * sizeof(POLY_F4) + (uintptr_t)polys))->redBox[0].x0 = column * 8 + 0x6B;
        ((MenuPolys *)(CurrentScreen * sizeof(POLY_F4) + (uintptr_t)polys))->redBox[0].y0 = row * 10 + 0x65;
        ((MenuPolys *)(CurrentScreen * sizeof(POLY_F4) + (uintptr_t)polys))->redBox[0].x1 = column * 8 + 0x72;
        ((MenuPolys *)(CurrentScreen * sizeof(POLY_F4) + (uintptr_t)polys))->redBox[0].y1 = row * 10 + 0x65;
        ((MenuPolys *)(CurrentScreen * sizeof(POLY_F4) + (uintptr_t)polys))->redBox[0].x2 = column * 8 + 0x6B;
        ((MenuPolys *)(CurrentScreen * sizeof(POLY_F4) + (uintptr_t)polys))->redBox[0].y2 = row * 10 + 0x6C;
        ((MenuPolys *)(CurrentScreen * sizeof(POLY_F4) + (uintptr_t)polys))->redBox[0].x3 = column * 8 + 0x72;
        ((MenuPolys *)(CurrentScreen * sizeof(POLY_F4) + (uintptr_t)polys))->redBox[0].y3 = row * 10 + 0x6C;
    }
    glyph = 0;
    do {
        if (glyph == 0xD) glyphX = 0x6C;
        if (glyph / 13 == 1) Print(&entry.character, glyphX, 0x70, 8);
        else Print(&entry.character, glyphX, 0x66, 8);
        glyphX += 8;
        glyph += 1;
        entry.character += 1;
    } while (glyph < 0x1A);
    PrintCX(msgDel, 0x82, 0x7A);
    PrintCX(msgEnd, 0xBE, 0x7A);
    PrintCX(msgEnterFileName, 0xA0, 0x89);
    PrintCX(selection->fileName, 0xA0, 0x98);
    AddPrim(OT[CurrentScreen], &polys->redBox[CurrentScreen]);
    Popup();
    return 0;
}


short DupFileCheck(SaveSelection *selection)
{
    short slot;

    for (slot = 0; slot < 15; slot++) {
        if (strncmp(selection->fileName, cardSlots[myFinished].entry[slot].name, 8) == 0) {
            if (slot != fadescreen) {
                if (cardSlots[myFinished].entry[slot].type & 4) return 0;
            }
        }
    }
    return 1;
}


int LoadSaveIcons(char *fileName, int card, u_char *iconX, u_char *iconY, u_short *posX, u_short *posY, int slot)
{
    char fileBuffer[512];
    RECT rect;
    char *message;
    int fd;

    fd = open(fileName, 1);
    if (fd < 0) {
        message = msgErrorOpenRead;
        goto fail;
    }
    if (read(fd, fileBuffer, 0x200) == 0x200) {
        rect.x = *posX;
        rect.y = *posY;
        rect.h = 1;
        rect.w = 0x10;
        cardSlots[card].entry[slot].x = *posX;
        cardSlots[card].entry[slot].y = *posY;
        LoadImage(&rect, (u_long *)(fileBuffer + 0x60));
        DrawSync(0);
        *posY += 1;
        rect.x = ((u_char)*iconX >> 2) + 0x280;
        rect.y = (u_short)*iconY;
        rect.w = 4;
        rect.h = 0x10;
        cardSlots[card].entry[slot].icon[0] = *iconX;
        cardSlots[card].entry[slot].icon[1] = *iconY;
        cardSlots[card].entry[slot].icon[2] = *iconX + 0x10;
        cardSlots[card].entry[slot].icon[3] = *iconY;
        cardSlots[card].entry[slot].icon[4] = *iconX;
        cardSlots[card].entry[slot].icon[5] = *iconY + 0x10;
        cardSlots[card].entry[slot].icon[6] = *iconX + 0x10;
        cardSlots[card].entry[slot].icon[7] = *iconY + 0x10;
        LoadImage(&rect, (u_long *)(fileBuffer + 0x80));
        DrawSync(0);
        *iconX += 0x10;
        close(fd);
        return 1;
    }
    message = msgErrorReadHeader;
fail:
    printf("%s", message);
    close(fd);
    return 0;
}


void DoConfirm(SaveSelection *selection, MenuPolys *polys, short messageType)
{
    if (keysPressed & 0x20) {
        selection->confirmed--;
        if (selection->confirmed < 0) {
            selection->confirmed = 0;
        } else {
            Sound(8);
        }
    } else if (keysPressed & 0x80) {
        selection->confirmed++;
        if (selection->confirmed >= 2) {
            selection->confirmed = 1;
        } else {
            Sound(8);
        }
    }
    if (selection->confirmed != 0) {
        (polys->redBox + CurrentScreen)->x0 = 129;
        (polys->redBox + CurrentScreen)->y0 = 129;
        (polys->redBox + CurrentScreen)->x1 = 149;
        (polys->redBox + CurrentScreen)->y1 = 129;
        (polys->redBox + CurrentScreen)->x2 = 129;
        (polys->redBox + CurrentScreen)->y2 = 136;
        (polys->redBox + CurrentScreen)->x3 = 149;
        (polys->redBox + CurrentScreen)->y3 = 136;
    } else {
        (polys->redBox + CurrentScreen)->x0 = 176;
        (polys->redBox + CurrentScreen)->y0 = 129;
        (polys->redBox + CurrentScreen)->x1 = 190;
        (polys->redBox + CurrentScreen)->y1 = 129;
        (polys->redBox + CurrentScreen)->x2 = 176;
        (polys->redBox + CurrentScreen)->y2 = 136;
        (polys->redBox + CurrentScreen)->x3 = 190;
        (polys->redBox + CurrentScreen)->y3 = 136;
    }
    if (messageType == 1) {
        PrintCX(msgOverwriteFile, 160, 120);
    } else if (messageType == 2) {
        PrintCX(msgDeleteFile, 160, 120);
    } else {
        PrintCX(msgMemoryCardIs, 160, 96);
        PrintCX(msgUnformattedDoYou, 160, 104);
        PrintCX(msgWishToFormat, 160, 112);
    }
    Print(msgYes, 130, 130, 8);
    Print(msgNo, 177, 130, 8);
    AddPrim(OT[CurrentScreen], &polys->redBox[CurrentScreen]);
    AddPrim(OT[CurrentScreen], &polys->boxFill[CurrentScreen]);
    AddPrim(OT[CurrentScreen], &polys->boxBorder[CurrentScreen]);
}


short IsTeamCardEmpty(int team)
{
    short slot;

    for (slot = 0; slot < 15; slot++) {
        if (cardSlots[team].entry[slot].type & 1) {
            return 0;
        }
    }
    return 1;
}


int CheckCardFull(int card)
{
    short count = 0;
    short i = 0;

    do {
        if (!(cardSlots[card].entry[i].type & 1)) {
            count++;
        }
        i++;
    } while (i < 15);
    return count;
}


void ResetLoadMenu(SaveSelection *selection)
{
    selection->dupeCheck = 0;
    selection->confirmed = 0;
    selection->selected = 0;
    selection->enteringName = 0;
    selection->nameReady = 0;
}


void LoadSave(ConfigData *gameData, MenuPolys *polys, SaveSelection *selection)
{
    LabelList labels;
    CardFilename path;
    char status[40];
    short i;
    int previous;

    labels = loadModeLabels;
    card1Exists = 1;
    cardPresent1 = 1;
    if (shakeAmount <= 0) {
        if (selection->timer > 0) {
            PrintCX(msgCheckingMemory, 160, 120);
            PrintCX(msgCards, 160, 128);
            Popup();
            if (selection->timer == 1) {
                setupBackgrndPolys(0, 2);
                PortSaveCursor(&myFinished, &fadescreen); /* Port: at the save in use */
            }
            selection->timer--;
        } else {
            setInputDevices(0);
            setInputDevices(1);
        }
    }
    if (cardNotice.timer >= 3) {
        switch (cardNotice.id) {
        case 1:
            PrintCX(msgFileExists, 160, 162);
            break;
        case 2:
            PrintCX(msgNotAWipeoutXl, 160, 120);
            PrintCX(msgSaveGame, 160, 128);
            break;
        case 3:
            PrintCX(msgMemoryCardIs, 160, 120);
            PrintCX(msgNotFormatted, 160, 128);
            break;
        case 4:
            PrintCX(msgErrorFormatting, 160, 120);
            PrintCX(msgMemoryCard, 160, 128);
            break;
        case 5:
            PrintCX(msgCannotSaveFileNo, 160, 120);
            PrintCX(msgMemoryCardPresent, 160, 128);
            break;
        case 6:
            PrintCX(msgErrorLoadingFile, 160, 120);
            break;
        case 7:
            PrintCX(msgErrorSavingFile, 160, 120);
            break;
        case 8:
            PrintCX(msgErrorDeletingFile, 160, 120);
            break;
        }
        if (cardNotice.id != 1) {
            Popup();
        }
        cardNotice.timer--;
    }
    if (cardPending.timer >= 2) {
        if (cardPending.id & 4) {
            PrintCX(msgLoadingGameData, 160, 120);
            if (cardPending.timer == 2) {
                if (!LoadCardData(SelectedCard()->name, myFinished, gameData)) {
                    cardNotice.timer = 60;
                    cardNotice.id = 6;
                } else {
                    PortSaveChosen(myFinished, SelectedCard()->name, gameData); /* Port: the save in use */
                }
                cardPending.id = 0;
            }
        } else if (cardPending.id & 1) {
            PrintCX(msgSavingGameData, 160, 120);
            if (cardPending.timer == 2) {
                if (!SaveCardFile(selection->fileName, myFinished, gameData)) {
                    cardNotice.timer = 60;
                    cardNotice.id = 7;
                } else {
                    PortSaveChosen(myFinished, selection->fileName, gameData); /* Port: the save in use */
                    if (!myFinished) {
                        setupBackgrndPolys(0, 1);
                    } else {
                        setupBackgrndPolys(1, 2);
                    }
                }
                cardPending.id = 0;
            }
        } else if (cardPending.id & 2) {
            PrintCX(msgSavingGameData, 160, 120);
            if (cardPending.timer == 2) {
                if (!myFinished) {
                    __builtin_memcpy(path.bytes, &cardDevice0, 6);
                } else {
                    __builtin_memcpy(path.bytes, &cardDevice1, 6);
                }
                strcat(path.bytes, SelectedCard()->filename);
                delete(path.bytes);
                if (!SaveCardFile(selection->fileName, myFinished, gameData)) {
                    cardNotice.timer = 60;
                    cardNotice.id = 7;
                } else {
                    PortSaveChosen(myFinished, selection->fileName, gameData); /* Port: the save in use */
                    if (!myFinished) {
                        setupBackgrndPolys(0, 1);
                    } else {
                        setupBackgrndPolys(1, 2);
                    }
                }
                cardPending.id = 0;
            }
        } else if (cardPending.id & 8) {
            PrintCX(msgDeletingGameData, 160, 120);
            if (cardPending.timer == 2) {
                if (!myFinished) {
                    __builtin_memcpy(path.bytes, &cardDevice0, 6);
                } else {
                    __builtin_memcpy(path.bytes, &cardDevice1, 6);
                }
                strcat(path.bytes, SelectedCard()->filename);
                if (!delete(path.bytes)) {
                    cardNotice.timer = 60;
                    cardNotice.id = 8;
                } else {
                    PortSaveDeleted(myFinished, SelectedCard()->name); /* Port: (if it was the save in use) */
                    if (!myFinished) {
                        setupBackgrndPolys(0, 1);
                    } else {
                        setupBackgrndPolys(1, 2);
                    }
                }
                cardPending.id = 0;
            }
        }
        Popup();
        cardPending.timer--;
    }
    currentCard = myFinished;
    if (selection->selected) {
        if (!selection->enteringName) {
            if (keysPressed & 32) {
                fadescreen++;
                if (fadescreen >= 14) {
                    fadescreen = 14;
                } else {
                    Sound(8);
                }
            } else if (keysPressed & 128) {
                fadescreen--;
                if (fadescreen <= 0) {
                    fadescreen = 0;
                } else {
                    Sound(8);
                }
            } else if (keysPressed & 16) {
                previous = fadescreen;
                fadescreen -= 3;
                if (fadescreen < 0) {
                    fadescreen = previous;
                } else {
                    Sound(8);
                }
            } else if (keysPressed & 64) {
                previous = fadescreen;
                fadescreen += 3;
                if (fadescreen >= 15) {
                    fadescreen = previous;
                } else {
                    Sound(8);
                }
            } else if ((keysPressed & 8192) && !(keysDown & 0xC01) && card1Exists && cardPresent1) {
                Sound(8);
                myFinished ^= 1;
            }
            switch (selection->loadMode) {
            case 0:
                PrintCX(msgSelectMemory, 160, 120);
                PrintCX(msgCardBlockTo, 160, 128);
                PrintCX(msgLoadGameFrom, 160, 136);
                break;
            case 1:
                PrintCX(msgSelectMemory, 160, 120);
                PrintCX(msgCardBlockTo, 160, 128);
                PrintCX(msgSaveGameTo, 160, 136);
                break;
            case 2:
                PrintCX(msgSelectMemory, 160, 120);
                PrintCX(msgCardBlockTo, 160, 128);
                PrintCX(msgDeleteSaveFrom, 160, 136);
                break;
            }
            if (card1Exists && cardPresent1) {
                DoButton(105, 198, 0, msgSelectMemoryCard);
                sprintf(status, fmtMemoryCard, myFinished + 1);
                status[13] = 0;
                PrintCX(status, 160, 160);
            }
        }
        switch (selection->loadMode) {
        case 0:
            if (GetMenuKey(5)) {
                Sound(9);
                selection->selected = 0;
                if ((myFinished == 0 && cardUnformatted0 == 1) || (myFinished == 1 && cardUnformatted1 == 1)) {
                    cardNotice.timer = 60;
                    cardNotice.id = 3;
                } else if (SelectedCardFlags() & 4) {
                    cardPending.timer = 30;
                    cardPending.id = 4;
                } else {
                    cardNotice.timer = 60;
                    cardNotice.id = 2;
                }
            } else {
                goto cancel;
            }
            break;
        case 1:
            if (!selection->enteringName) {
                if (GetMenuKey(5)) {
                    Sound(9);
                    selection->nameLength = 0;
                    for (i = 0; i < 10; i++) {
                        selection->fileName[i] = 0;
                    }
                    selection->enteringName = 1;
                    if ((myFinished == 0 && !card1Exists) || (myFinished == 1 && !cardPresent1)) {
                        cardNotice.timer = 60;
                        cardNotice.id = 5;
                        ResetLoadMenu(selection);
                    }
                } else {
                    goto cancel;
                }
            } else if ((myFinished == 0 && cardUnformatted0 == 1) || (myFinished == 1 && cardUnformatted1 == 1)) {
                DoConfirm(selection, polys, 3);
                if (GetMenuKey(5)) {
                    Sound(9);
                    if (selection->confirmed) {
                        char *device = (char *)&cardDevice1;
                        if (myFinished == 0) {
                            device = (char *)&cardDevice0;
                        }
                        if (!format(device)) {
                            cardNotice.timer = 60;
                            cardNotice.id = 4;
                            ResetLoadMenu(selection);
                            selection->confirmed = 0;
                        } else {
                            if (!myFinished) {
                                cardUnformatted0 = 0;
                            } else {
                                cardUnformatted1 = 0;
                            }
                            selection->confirmed = 0;
                        }
                    } else {
                        goto reset;
                    }
                } else {
                    goto cancel;
                }
            } else if (!selection->dupeCheck) {
                if (SelectedCardFlags() & 1) {
                    selection->enteringName = 1;
                    selection->dupeCheck = 1;
                    break;
                }
                DoConfirm(selection, polys, 1);
                if (GetMenuKey(5)) {
                    Sound(9);
                    if (selection->confirmed) {
                        selection->enteringName = 1;
                        selection->confirmed = 0;
                        selection->dupeCheck = 1;
                    } else {
                        ResetLoadMenu(selection);
                    }
                } else if (GetMenuKey(4)) {
                    Sound(49);
                    ResetLoadMenu(selection);
                }
                selection->nameActive = 0;
                if (SelectedCardFlags() & 4) {
                    strcpy(selection->fileName, SelectedCard()->name);
                    selection->nameLength = strlen(selection->fileName);
                } else {
                    selection->nameLength = 0;
                    for (i = 0; i < 10; i++) {
                        selection->fileName[i] = 0;
                    }
                }
            } else {
                if (!selection->nameReady) {
                    if (selection->enteringName && EnterFileName(polys, selection)) {
                        if (DupFileCheck(selection)) {
                            selection->nameReady = 1;
                        } else {
                            cardNotice.timer = 60;
                            cardNotice.id = 1;
                        }
                    }
                    if (!selection->nameReady) {
                        break;
                    }
                }
                strcpy(SelectedCard()->name, selection->fileName);
                {
                    short mode;
                    if (SelectedCardFlags() & 1) {
                        cardPending.timer = 4;
                        mode = 1;
                    } else {
                        cardPending.timer = 4;
                        mode = 2;
                    }
                    cardPending.id = mode;
                }
                goto reset;
            }
            break;
        case 2:
            if ((myFinished == 0 && cardUnformatted0 == 1) || (myFinished == 1 && cardUnformatted1 == myFinished)) {
                cardNotice.timer = 60;
                cardNotice.id = 3;
                selection->selected = 0;
            } else if (!selection->dupeCheck) {
                if (GetMenuKey(5)) {
                    Sound(9);
                    if ((SelectedCardFlags() & 4) || (SelectedCardFlags() & 2)) {
                        selection->dupeCheck = 1;
                        selection->enteringName = 1;
                    } else {
                        selection->selected = 0;
                    }
                } else {
                    goto cancel;
                }
            } else {
                DoConfirm(selection, polys, 2);
                if (GetMenuKey(5)) {
                    Sound(9);
                    if (selection->confirmed) {
                        if ((SelectedCardFlags() & 4) || (SelectedCardFlags() & 2)) {
                            cardPending.timer = 4;
                            cardPending.id = 8;
                        }
                    }
                    selection->dupeCheck = 0;
                    selection->enteringName = 0;
                    *(volatile short *)&selection->selected = 0;
                    selection->confirmed = 0;
                    goto display;
                } else {
                    goto cancel;
                }
            }
            break;
        }
        goto display;
cancel:
        if (GetMenuKey(4)) {
            Sound(49);
            goto reset;
        }
        goto display;
reset:
        ResetLoadMenu(selection);
    } else {
        for (i = 0; i < 3; i++) {
            if (i == selection->loadMode) {
                CentreText(labels.p[i], 160, 100 + i * 12, 8, 0);
            } else {
                CentreText(labels.p[i], 160, 100 + i * 12, 8, 1);
            }
        }
        if (!cardPending.timer) {
            if (keysPressed & 16) {
                selection->loadMode--;
                if (selection->loadMode < 0) {
                    selection->loadMode = 0;
                } else {
                    Sound(8);
                }
            } else if (keysPressed & 64) {
                selection->loadMode++;
                if (selection->loadMode >= 3) {
                    selection->loadMode = 2;
                } else {
                    Sound(8);
                }
            }
        }
        if (!cardNotice.timer && !cardPending.timer && !selection->timer) {
            if (GetMenuKey(5) && (card1Exists || cardPresent1)) {
                Sound(9);
                selection->selected = 1;
            } else if (GetMenuKey(4)) {
                Sound(49);
                {
                    int old = currMenu;
                    int next = menu[old].prevMenu;
                    cardNotice.timer = 0;
                    cardNotice.id = 0;
                    prevMenu = old;
                    currMenu = next;
                }
                InitSlider();
                return;
            }
        }
    }
display:
    if (!myFinished) {
        selection->cardSelection[0] = fadescreen;
    } else {
        selection->cardSelection[1] = fadescreen;
    }
    if (!selection->timer) {
        if (card1Exists) {
            if (cardUnformatted0 == 1) {
                PrintCX(msgMemoryCard, 72, 120);
                PrintCX(msgUnformatted, 72, 128);
                CardMessage(0);
            }
            if (IsTeamCardEmpty(0)) {
                PrintCX(msgMemoryCard, 72, 120);
                PrintCX(msgIsFull, 72, 128);
                CardMessage(0);
            }
        }
        if (cardPresent1) {
            if (cardUnformatted1 == 1) {
                PrintCX(msgMemoryCard, 250, 120);
                PrintCX(msgUnformatted, 250, 128);
                CardMessage(1);
            }
            if (IsTeamCardEmpty(1)) {
                PrintCX(msgMemoryCard, 250, 120);
                PrintCX(msgIsFull, 250, 128);
                CardMessage(1);
            }
        }
        if (!card1Exists) {
            PrintCX(msgNoMemory, 75, 120);
            PrintCX(msgCard, 75, 128);
            CardMessage(0);
        }
        if (!cardPresent1) {
            PrintCX(msgNoMemory, 250, 120);
            PrintCX(msgCard, 250, 128);
            CardMessage(1);
        }
    }
    Print((u_char *)cardSlots[0].entry[selection->cardSelection[0]].label.bytes, 30, 182, 8);
    Print((u_char *)cardSlots[1].entry[selection->cardSelection[1]].label.bytes, 210, 182, 8);
    if (card1Exists) {
        if (!cardPresent1) {
            myFinished = 0;
        }
    } else if (cardPresent1) {
        myFinished = 1;
    }
    DisplayCardBack(polys, fadescreen, myFinished, selection);
    if (cardNotice.timer == 2) {
        cardNotice.timer = 0;
    }
    if (!cardNotice.timer) {
        cardNotice.id = 0;
    }
    if (cardPending.timer == 1) {
        cardPending.timer = 0;
    }
}


void MakePassword(ConfigData *gameData)
{
    PasswordSeeds seeds = passwordSeeds[0];
    PasswordSeeds decoded = passwordSeedsClear[0];
    short i;
    int sum = 0;
    int result;

    for (i = 0; i < 8; i++) {
        gameData->generated.digits[i] = trackMedals[i] ^ (u_char)seeds.values[i];
    }
    gameData->generated.digits[8] = (gameData->rapierComplete | gameData->venomComplete * 2) ^ (u_char)seeds.values[8];
    gameData->generated.digits[9] = (gameData->class2Unlocked | gameData->class1Unlocked * 2) ^ (u_char)seeds.values[9];
    for (i = 0; i < 10; i++) {
        result = sum + (u_char)gameData->generated.digits[i] * (i + 1);
        sum = result;
    }
    gameData->generated.digits[10] = (result & 3) ^ (u_char)seeds.values[10];
    gameData->generated.digits[11] = ((u_int)(result & 12) >> 2) ^ (u_char)seeds.values[11];
    gameData->generated.digits[12] = ((u_int)(result & 48) >> 4) ^ (u_char)seeds.values[12];
    gameData->generatedCheck = ((u_int)(result & 192) >> 6) ^ (u_char)seeds.values[13];
    for (i = 0; i < 8; i++) {
        decoded.values[i] = (u_char)gameData->generated.digits[i] ^ seeds.values[i];
    }
}


u_char DecodePassword(ConfigData *gameData, u_char *password)
{
    PasswordSeeds seeds = passwordSeeds[0];
    PasswordSeeds decoded = passwordSeedsClear[0];
    u_short hi1, hi2;
    short i;
    short sum = 0;
    short sumResult;
    int a, b, c, d;
    u_short classFlag;
    u_int otherFlag;
    u_short input9;
    u_short seed9;

    for (i = 0; i < 8; i++) {
        decoded.values[i] = password[i] ^ seeds.values[i];
    }
    input9 = password[9];
    seed9 = seeds.values[9];
    classFlag = password[8] ^ seeds.values[8];
    decoded.values[8] = classFlag;
    otherFlag = input9 ^ seed9;
    decoded.values[9] = password[9] ^ seeds.values[9];
    for (i = 0; i < 10; i++) {
        sumResult = sum + password[i] * (i + 1);
        sum = sumResult;
    }
    a = password[10] ^ seeds.values[10];
    decoded.values[10] = a;
    b = password[11] ^ seeds.values[11];
    decoded.values[11] = b;
    c = password[12] ^ seeds.values[12];
    decoded.values[12] = c;
    d = password[13] ^ seeds.values[13];
    decoded.values[13] = d;
    if ((short)(a | (((d * 64) | (c * 16)) | (b * 4))) == sumResult) {
        for (i = 0; i < 8; i++) {
            trackMedals[i] = decoded.values[i];
        }
        ReInitGameProgress();
        hi1 = (classFlag >> 1) & 1;
        hi2 = (otherFlag >> 1) & 1;
        gameData->venomComplete = hi1;
        gameData->rapierComplete = classFlag & 1;
        gameData->class1Unlocked = hi2;
        gameData->class2Unlocked = otherFlag & 1;
        return 1;
    }
    return 0;
}


void DisplayPassword(ConfigData *gameData)
{
    char passwordBuffer[20];
    short digit;

    MakePassword(gameData);
    PortConsoleGlyphs(1); /* Port: the password's symbols are shapes, whatever the device */
    for (digit = 0; digit < 14; digit++) {
        switch ((u_char)gameData->generated.digits[digit]) {
        case 0: DoButton((short)(digit * 18 + 37), 100, 3, numTex); break;
        case 1: DoButton((short)(digit * 18 + 37), 100, 1, numTex); break;
        case 2: DoButton((short)(digit * 18 + 37), 100, 2, numTex); break;
        case 3: DoButton((short)(digit * 18 + 37), 100, 0, numTex); break;
        }
    }
    PortConsoleGlyphs(0);
}


void EnterPassword(ConfigData *gameData, MenuSelection *selection, MenuPolys *polys)
{
    short digit;
    int backFrom;
    int okFrom;
    int check;

    if (GetMenuKey(1)) {
        selection->password.digits[menu[currMenu].mode]++;
        if (selection->password.digits[menu[currMenu].mode] >= 4) {
            selection->password.digits[menu[currMenu].mode] = 3;
        } else {
            Sound(8);
        }
    } else if (GetMenuKey(0)) {
        selection->password.digits[menu[currMenu].mode]--;
        if (selection->password.digits[menu[currMenu].mode] < 0) {
            selection->password.digits[menu[currMenu].mode] = 0;
        } else {
            Sound(8);
        }
    } else if (GetMenuKey(3)) {
        menu[currMenu].mode++;
        if (menu[currMenu].mode >= 14) {
            menu[currMenu].mode = 13;
        } else {
            Sound(8);
        }
    } else if (GetMenuKey(2)) {
        menu[currMenu].mode--;
        if (menu[currMenu].mode < 0) {
            menu[currMenu].mode = 0;
        } else {
            Sound(8);
        }
    }
    PortConsoleGlyphs(1); /* Port: the password's symbols are shapes, whatever the device */
    for (digit = 0; digit < 14; digit++) {
        switch (selection->password.digits[digit]) {
        case 0:
            DoButton((short)(digit * 18 + 37), 140, 3, numTex);
            break;
        case 1:
            DoButton((short)(digit * 18 + 37), 140, 1, numTex);
            break;
        case 2:
            DoButton((short)(digit * 18 + 37), 140, 2, numTex);
            break;
        case 3:
            DoButton((short)(digit * 18 + 37), 140, 0, numTex);
            break;
        }
    }
    PortConsoleGlyphs(0);
    (polys->purple + CurrentScreen)->x0 = menu[currMenu].mode * 18 + 35;
    (polys->purple + CurrentScreen)->y0 = 138;
    (polys->purple + CurrentScreen)->x1 = menu[currMenu].mode * 18 + 54;
    (polys->purple + CurrentScreen)->y1 = 138;
    (polys->purple + CurrentScreen)->x2 = menu[currMenu].mode * 18 + 35;
    (polys->purple + CurrentScreen)->y2 = 157;
    (polys->purple + CurrentScreen)->x3 = menu[currMenu].mode * 18 + 54;
    (polys->purple + CurrentScreen)->y3 = 157;
    if (passwordErrorTimer == 0 && GetMenuKey(5)) {
        Sound(9);
        if (DecodePassword(gameData, (u_char *)selection->password.digits)) {
            gameData->entered = selection->password;
            check = selection->passwordCheck;
            gameData->enteredCheck = check;
            okFrom = currMenu;
            currMenu = menu[okFrom].nextMenu;
            prevMenu = okFrom;
            InitSlider();
            InitChallenges();
            InitMovies();
            return;
        }
        passwordErrorTimer = 60;
    }
    if (GetMenuKey(4)) {
        Sound(49);
        backFrom = currMenu;
        currMenu = menu[backFrom].prevMenu;
        prevMenu = backFrom;
        InitSlider();
        return;
    }
    if (passwordErrorTimer != 0) {
        PrintCX(msgPasswordIsIncorrect, 160, 165);
        passwordErrorTimer -= 1;
    }
    Print(msgUseUpAndDownOnTheDirectional, 30, 182, 8);
    Print(msgButtonsToChangeTheSymbol, 30, 189, 8);
    PrintCX(msgCurrentPassword, 160, 90);
    PrintCX(msgNewPasswordEntry, 160, 130);
    DisplayPassword(gameData);
    AddPrim(OT[CurrentScreen], &polys->purple[CurrentScreen]);
}


void InitDefaultName(ConfigData *gameData, HiScoreData *data, char mode)
{
    short i;
    u8 cursor;

    cursor = 0;
    if (mode == 0) {
        cursor = 2;
    } else if (mode == 1) {
        cursor = 1;
    }
    if (strlen(gameData->name) != 0) {
        strcpy(data->nameEntered, gameData->name);
        data->numChars = strlen(data->nameEntered);
        if (data->numChars == 3) {
            data->dispChar[0] = cursor + 0x19;
            data->charVal = cursor + 0x19;
            data->currChar = cursor + 0x5A;
        } else {
            data->dispChar[0] = cursor + 0x19;
            data->currChar = cursor + 0x5A;
            data->charVal = cursor + 0x19;
        }
        if (mode == 0) {
            i = 0;
            data->charX = 40;
            for (; data->nameEntered[i] != 0; i++) {
                data->charX += fontGlyphs16[data->nameEntered[i] - 'A'].width;
            }
        } else if (mode == 1) {
            i = 0;
            data->charX = 180;
            for (; data->nameEntered[i] != 0; i++) {
                data->charX += fontGlyphs8[data->nameEntered[i] - 'A'].width;
            }
        }
    } else {
        data->nameEntered[0] = 0;
        data->currChar = 'A';
        data->charVal = 0;
        data->numChars = 0;
        data->dispChar[0] = 0;
        if (mode == 0) {
            data->charX = 40;
        } else if (mode == 1) {
            data->charX = 180;
        }
    }
}


void InitTempData(ConfigData *gameData, MenuSelection *selection) {
    s16 i;

    selection->screenX = gameData->screenX;
    selection->screenY = gameData->screenY;
    selection->screenOffsetState = gameData->screenOffsetState;
    selection->weapons = gameData->weaponsOn;
    selection->moveScreen = gameData->moveScreen;
    selection->internalView = gameData->internalView;
    strcpy(selection->fileName, gameData->name);
    selection->musicVolume = gameData->musicVolume;
    selection->effectVolume = gameData->effectVolume;
    selection->otherMusic = gameData->otherMusicVolume;
    selection->otherEffect = gameData->otherEffectVolume;
    selection->music = gameData->cdTrack;
    selection->stereo = gameData->soundMode;
    selection->actTrackNum = gameData->actTrackNum;
    selection->shipClass = gameData->shipClass;
    selection->ship = ownShip;
    selection->trackSelection = 0;
    selection->negconCenter = gameData->negconCentre;
    selection->negconMin = gameData->negconMin;
    selection->negconType = gameData->negconType;
    selection->negconFlags[0] = 0;
    selection->negconFlags[1] = 0;
    selection->negconFlags[2] = 0;
    selection->scoreIndex = 0;
    for (i = 0; i < 10; i++) {
        selection->scoreFlags[i] = 0;
    }
    selection->record = 0;
    selection->highScoreScratchA2 = 0;
    selection->highScoreScratchA4 = 0;
    selection->highScoreScratchA6 = 0;
    selection->highScoreScratchA8 = 0;
    selection->highScoreScratchAA = 0;
    selection->highScoreScratchAC = 0;
    selection->highScoreScratchB0 = 0;
    selection->highScoreScratchB4 = 0;
    selection->timer = 0;
}


void InitKeyConfig(KeyConfig *config) {
    DigitalButtons digitalButtons = padDigitalKeys;
    AnalogButtons analogButtons = padAnalogKeys;
    s16 button;
    s16 key;

    config->keys[0].key[0] = keyDefineThrustPad[0];
    config->keys[0].key[1] = keyDefineFire[0];
    config->keys[0].key[2] = keyDefineView[0];
    config->keys[0].key[3] = keyDefineDitch[0];
    config->keys[0].key[4] = keyDefineRBrake[0];
    config->keys[0].key[5] = keyDefineLBrake[0];
    config->keys[1].key[0] = keyDefineThrustPad[1];
    config->keys[1].key[1] = keyDefineFire[1];
    config->keys[1].key[2] = keyDefineView[1];
    config->keys[1].key[3] = keyDefineDitch[1];
    config->keys[1].key[4] = keyDefineRBrake[1];
    config->keys[1].key[5] = keyDefineLBrake[1];
    for (button = 6; button < 9; button++) {
        config->keys[0].key[button] = 0;
        config->keys[1].key[button] = 0;
    }
    for (button = 0; button < 6; button++) {
        {
            u16 want = config->keys[0].key[button];
            for (key = 0; key < 8; key++) {
                if (digitalButtons.button[key] == want) break;
            }
        }
        config->calibration[0].value[button] = key;
        {
            u16 want2 = config->keys[1].key[button];
            for (key = 0; key < 6; key++) {
                if (analogButtons.button[key] == want2) break;
            }
        }
        config->calibration[1].value[button] = key;
    }
    config->active = 0;
}


void ResetKeyConfig(MenuSelection *selection) {
    DigitalButtons digitalButtons = padDigitalKeys;
    AnalogButtons analogButtons = padAnalogKeys;
    s16 button;
    s16 key;

    selection->keys[0].key[0] = 0x4000;
    selection->keys[0].key[1] = 0x2000;
    selection->keys[0].key[2] = 0x1000;
    selection->keys[0].key[3] = 0x8000;
    selection->keys[0].key[4] = 0x100;
    selection->keys[0].key[5] = 0x200;
    selection->keys[1].key[0] = 0x8ff0;
    selection->keys[1].key[1] = 0x8ff1;
    selection->keys[1].key[2] = 0x1000;
    selection->keys[1].key[3] = 0x2000;
    selection->keys[1].key[4] = 0x8ff2;
    selection->keys[1].key[5] = 0x800;
    for (button = 6; button < 9; button++) {
        selection->keys[0].key[button] = 0;
        selection->keys[1].key[button] = 0;
    }
    for (button = 0; button < 6; button++) {
        {
            u16 want = selection->keys[0].key[button];
            for (key = 0; key < 8; key++) {
                if (digitalButtons.button[key] == want) break;
            }
        }
        selection->calibration[0].value[button] = key;
        {
            u16 want2 = selection->keys[1].key[button];
            for (key = 0; key < 6; key++) {
                if (analogButtons.button[key] == want2) break;
            }
        }
        selection->calibration[1].value[button] = key;
    }
}


#define setRGB0(p, r, g, b) ((p)->r0 = (r), (p)->g0 = (g), (p)->b0 = (b))
#define setRGB1(p, r, g, b) ((p)->r1 = (r), (p)->g1 = (g), (p)->b1 = (b))
#define setRGB2(p, r, g, b) ((p)->r2 = (r), (p)->g2 = (g), (p)->b2 = (b))
#define setRGB3(p, r, g, b) ((p)->r3 = (r), (p)->g3 = (g), (p)->b3 = (b))
#define setBox(p, x, y, w, h) ((p)->x0 = (x), (p)->y0 = (y), (p)->x1 = (w), (p)->y1 = (y), (p)->x2 = (x), (p)->y2 = (h), (p)->x3 = (w), (p)->y3 = (h))

#define InitShade(shade, mode) \
    SetPolyG4(&polys->shade[i]); \
    setRGB0(&polys->shade[i], 24, 66, 11); \
    setRGB1(&polys->shade[i], 24, 66, 11); \
    setRGB2(&polys->shade[i], 24, 66, 11); \
    setRGB3(&polys->shade[i], 24, 66, 11); \
    SetDrawMode(&polys->mode[i], 1, 1, GetTPage(0, 1, 0, 0), 0)

void InitMenuPolys(MenuPrims *prims)
{
    MenuPolys *polys;
    short i, j;
    Texture *texture;
    POLY_G4 *quad;
    MenuPolys *row;
    POLY_FT4 *tex;

    polys = (MenuPolys *)prims;
    for (i = 0; i < 2; i++) {
        SetPolyG4(&polys->redA[i]);
        polys->redA[i].r0 = 63;
        polys->redA[i].r1 = 127;
        polys->redA[i].r2 = 191;
        polys->redA[i].g0 = 0;
        polys->redA[i].b0 = 0;
        polys->redA[i].g1 = 0;
        polys->redA[i].b1 = 0;
        polys->redA[i].g2 = 0;
        polys->redA[i].b2 = 0;
        polys->redA[i].r3 = 255;
        polys->redA[i].g3 = 0;
        polys->redA[i].b3 = 0;
        SetPolyG4(&polys->redB[i]);
        polys->redB[i].r0 = 63;
        polys->redB[i].r1 = 127;
        polys->redB[i].r2 = 191;
        polys->redB[i].g0 = 0;
        polys->redB[i].b0 = 0;
        polys->redB[i].g1 = 0;
        polys->redB[i].b1 = 0;
        polys->redB[i].g2 = 0;
        polys->redB[i].b2 = 0;
        polys->redB[i].r3 = 255;
        polys->redB[i].g3 = 0;
        polys->redB[i].b3 = 0;
        SetPolyG4(&polys->purple[i]);
        setRGB0(&polys->purple[i], 85, 0, 40);
        setRGB1(&polys->purple[i], 85, 0, 40);
        setRGB2(&polys->purple[i], 85, 0, 40);
        setRGB3(&polys->purple[i], 85, 0, 40);
        InitShade(shade0, mode0);
        InitShade(shade1, mode1);
        InitShade(shade2, mode2);
        InitShade(shade3, mode3);
        InitShade(shade4, mode4);
        row = (MenuPolys *)((char *)polys + i * 24);
        SetPolyF4(&polys->boxFill[i]);
        setRGB0(&row->boxFill[0], 0, 0, 46);
        setBox(&row->boxFill[0], 103, 92, 219, 172);
        SetPolyF4(&polys->boxBorder[i]);
        setRGB0(&row->boxBorder[0], 192, 192, 192);
        setBox(&row->boxBorder[0], 102, 91, 220, 173);
        SetPolyF4(&polys->topFill[i]);
        setRGB0(&row->topFill[0], 0, 0, 46);
        setBox(&row->topFill[0], 103, 86, 219, 166);
        SetPolyF4(&polys->topBorder[i]);
        setRGB0(&row->topBorder[0], 192, 192, 192);
        setBox(&row->topBorder[0], 102, 85, 220, 167);
        SetPolyF4(&polys->redBox[i]);
        setRGB0(&row->redBox[0], 128, 0, 0);
    }
    for (i = 0; i < 2; i++) {
        for (j = 0; j < 2; j++) {
            SetPolyF4(&polys->lowerFill[i][j]);
            setRGB0(&((MenuPolys *)((char *)polys + (i * 48 + j * 24)))->lowerFill[0][0], 0, 0, 46);
            SetPolyF4(&polys->lowerBorder[i][j]);
            setRGB0(&((MenuPolys *)((char *)polys + (i * 48 + j * 24)))->lowerBorder[0][0], 255, 255, 255);
            SetSemiTrans(&polys->lowerFill[i][j], 1);
            SetSemiTrans(&polys->lowerBorder[i][j], 1);
            if (i == 0) {
                setBox(&polys->lowerFill[0][j], 32, 118, 112, 135);
                setBox(&polys->lowerBorder[0][j], 31, 117, 113, 136);
            } else {
                /* Preserve the proven score-box offsets; direct members change codegen. */
                setBox((POLY_F4 *)((char *)polys + i * 48 + j * 24 + 0x15E0), 210, 118, 290, 135);
                setBox((POLY_F4 *)((char *)polys + i * 48 + j * 24 + 0x1640), 209, 117, 291, 136);
            }
        }
    }
    for (i = 0; i < 5; i++) {
        for (j = 0; j < 2; j++) {
            SetPolyG4(&polys->selection[i][j]);
            quad = &((MenuPolys *)((char *)polys + (j * 36 + i * 72)))->selection[0][0];
            setRGB0(quad, 85, 0, 40);
            setRGB1(quad, 85, 0, 40);
            setRGB2(quad, 85, 0, 40);
            setRGB3(quad, 85, 0, 40);
        }
    }
    for (i = 0; i < 6; i++) {
        for (j = 0; j < 2; j++) {
            if (i < 3) {
                texture = (Texture *)textureTable[7 + menuTextures];
                SetPolyFT4(&polys->arrowTexture[i][j]);
                tex = &((MenuPolys *)((char *)polys + (j * 40 + i * 80)))->arrowTexture[0][0];
                setRGB0(tex, 128, 128, 128);
                (&polys->arrowTexture[0][0] + i * 2)[j].u0 = texture->u0;
                (&polys->arrowTexture[0][0] + i * 2)[j].v0 = texture->v0;
                (&polys->arrowTexture[0][0] + i * 2)[j].u1 = (unsigned char)texture->u1 + 1;
                (&polys->arrowTexture[0][0] + i * 2)[j].v1 = texture->v1;
                (&polys->arrowTexture[0][0] + i * 2)[j].u2 = texture->u2;
                (&polys->arrowTexture[0][0] + i * 2)[j].v2 = (unsigned char)texture->v2 + 1;
                (&polys->arrowTexture[0][0] + i * 2)[j].u3 = (unsigned char)texture->u3 + 1;
                (&polys->arrowTexture[0][0] + i * 2)[j].v3 = (unsigned char)texture->v3 + 1;
                (&polys->arrowTexture[0][0] + i * 2)[j].tpage = GetTPage(0, 0, texture->textureX, texture->textureY);
                tex->clut = texture->cba;
            }
            SetLineF2(&polys->arrowLine[i][j]);
            if (i < 3) {
                setRGB0(&((MenuPolys *)((char *)polys + (i * 32 + j * 16)))->arrowLine[0][0], 128, 128, 0);
            } else {
                setRGB0(&((MenuPolys *)((char *)polys + (i * 32 + j * 16)))->arrowLine[0][0], 39, 200, 222);
            }
        }
    }
    for (i = 0; i < 30; i++) {
        for (j = 0; j < 2; j++) {
            SetPolyF4(&polys->background[i][j]);
            setRGB0(&((MenuPolys *)((char *)polys + (i * 48 + j * 24)))->background[0][0], 39, 200, 222);
            SetPolyFT4(&polys->backdrop[i][j]);
            setRGB0(&((MenuPolys *)((char *)polys + (i * 80 + j * 40)))->backdrop[0][0], 128, 128, 128);
        }
    }
}

#undef setRGB0
#undef setRGB1
#undef setRGB2
#undef setRGB3
#undef setBox
#undef InitShade


void DispHiScores(UnlockData *unlock, void *unused, unsigned char raceMode)
{
    short originalTrack;
    short trackNo;
    short i;
    int maxTracks;
    signed char ship;

    maxTracks = 6;
    if (unlock->extraTracks || (unlock->unlockedTracks & 0xffff0000)) maxTracks = 8;
    if (GetMenuKey(3)) {
        menu[currMenu].selection++;
        if (menu[currMenu].selection >= maxTracks) menu[currMenu].selection = maxTracks - 1;
        else PlayNote(8, 255, 15, 15, 256);
    }
    if (GetMenuKey(2)) {
        menu[currMenu].selection--;
        if (menu[currMenu].selection < 0) menu[currMenu].selection = 0;
        else PlayNote(8, 255, 15, 15, 256);
    }
    if (menu[currMenu].selection < maxTracks - 1 && myseed % 30 < 21) DoButton(275, 186, 5, numTex);
    if (menu[currMenu].selection > 0 && myseed % 30 < 21) DoButton(30, 186, 4, numTex);
    trackNo = menu[currMenu].selection;
    PrintCX(GetTrackName(raceTracks[menu[currMenu].selection], 0, 40, 0), 160, 186);
    originalTrack = trackNo;
    if (raceMode == 6) trackNo += 8;
    Print(hiScoreLabelName, 30, 90, 8);
    Print(hiScoreLabelTime, 30, 97, 8);
    Print(hiScoreLabelShip, 30, 104, 8);
    Print(hiScoreLabelName, 30, 135, 8);
    Print(hiScoreLabelLap, 30, 142, 8);
    Print(hiScoreLabelShip, 30, 149, 8);
    for (i = 0; i < 4; i++) {
        if (!(i < 2U && (trackNo == 6 || trackNo == 7 || trackNo == 14 || trackNo == 15))) {
            AddText(hiScoreTable[trackNo][i].name, 75, 90 + i * 10, 8, 0);
            AddTime(hiScoreTable[trackNo][i].time, 130, 90 + i * 10, 8, 0, 1);
            AddText(hiScoreTable[trackNo][i].lapName, 75, 135 + i * 10, 8, 0);
            AddTime(hiScoreTable[trackNo][i].lapTime, 130, 135 + i * 10, 8, 0, 1);
        }
        if (!(i < 2U && (trackNo == 6 || trackNo == 7 || trackNo == 14 || trackNo == 15))) {
            ship = hiScoreTable[trackNo][i].ship / 3;
            switch (ship) {
                case 0: Print(teamNameAgSystems, 240, 92 + i * 10, 8); break;
                case 1: Print(teamNameAuricom, 240, 92 + i * 10, 8); break;
                case 2: Print(teamNameQirex, 240, 92 + i * 10, 8); break;
                case 3: Print(teamNameFeisar, 240, 92 + i * 10, 8); break;
                case 4: Print(teamNamePiranha, 240, 92 + i * 10, 8); break;
                case 5: Print(msgRedBull, 240, 92 + i * 10, 8); break;
            }
        }
        if (!(i < 2U && (trackNo == 6 || trackNo == 7 || trackNo == 14 || trackNo == 15))) {
            ship = hiScoreTable[trackNo][i].lapShip / 3;
            switch (ship) {
                case 0: Print(teamNameAgSystems, 240, 137 + i * 10, 8); break;
                case 1: Print(teamNameAuricom, 240, 137 + i * 10, 8); break;
                case 2: Print(teamNameQirex, 240, 137 + i * 10, 8); break;
                case 3: Print(teamNameFeisar, 240, 137 + i * 10, 8); break;
                case 4: Print(teamNamePiranha, 240, 137 + i * 10, 8); break;
                case 5: Print(msgRedBull, 240, 137 + i * 10, 8); break;
            }
        }
        switch (i) {
            case 0: Print(hiScoreRank0, 190, 92, 8); Print(hiScoreRank0, 190, 137, 8); break;
            case 1: Print(hiScoreRank1, 190, 102, 8); Print(hiScoreRank1, 190, 147, 8); break;
            case 2: Print(hiScoreRank2, 190, 112, 8); Print(hiScoreRank2, 190, 157, 8); break;
            case 3: Print(hiScoreRank3, 190, 122, 8); Print(hiScoreRank3, 190, 167, 8); break;
        }
    }
    PortTwoSided(1); /* Port: the track model's surfaces shown from behind too */
    DrawTeamShips(0, -260, 1500, heading, 220, shipModels[trackModelOrder[originalTrack]]);
    PortTwoSided(0);
    myseed++;
}


void InitNewDisplay(ConfigData *gameData)
{
    SetDefDispEnv(&DispEnv0, 0, 0, 0x140, 0xF0);
    SetDefDispEnv(&DispEnv1, 0, 0x100, 0x140, 0xF0);
    DispEnv0.screen.x = *(u16 *)&gameData->screenX;
    DispEnv0.screen.y = *(u16 *)&gameData->screenY;
    DispEnv1.screen.x = *(u16 *)&gameData->screenX;
    DispEnv1.screen.y = *(u16 *)&gameData->screenY;
    SetGeomOffset(0xA0, 0x78);
    SetGeomScreen(0xA0);
    PutDispEnv(doubleBuffer[CurrentScreen].disp);
}


void InitScreenBox(MenuPolys *screen)
{
    u_char edge;
    u_char buffer;

    for (edge = 0; edge < 4; edge++) {
        for (buffer = 0; buffer < 2; buffer++) {
            SetLineF2(&screen->border[edge][buffer]);
            screen->border[edge][buffer].r0 = 255;
            screen->border[edge][buffer].g0 = 0;
            screen->border[edge][buffer].b0 = 0;
        }
    }
    for (buffer = 0; buffer < 2; buffer++) {
        screen->border[0][buffer].x0 = 0;
        screen->border[0][buffer].y0 = 0;
        screen->border[0][buffer].x1 = 319;
        screen->border[0][buffer].y1 = 0;
    }
    for (buffer = 0; buffer < 2; buffer++) {
        screen->border[1][buffer].x0 = 319;
        screen->border[1][buffer].y0 = 0;
        screen->border[1][buffer].x1 = 319;
        screen->border[1][buffer].y1 = 239;
    }
    for (buffer = 0; buffer < 2; buffer++) {
        screen->border[2][buffer].x0 = 319;
        screen->border[2][buffer].y0 = 239;
        screen->border[2][buffer].x1 = 0;
        screen->border[2][buffer].y1 = 239;
    }
    for (buffer = 0; buffer < 2; buffer++) {
        screen->border[3][buffer].x0 = 0;
        screen->border[3][buffer].y0 = 239;
        screen->border[3][buffer].x1 = 0;
        screen->border[3][buffer].y1 = 0;
    }
}


void ScreenConfig(ConfigData *gameData, MenuPolys *polys, short *screenPosition)
{
    u_char corner;

    if (keysDown & 0x20) {
        screenPosition[0]++;
        if (screenPosition[0] >= 25) {
            screenPosition[0] = 24;
        }
    } else if (keysDown & 0x80) {
        screenPosition[0]--;
        if (screenPosition[0] < -10) {
            screenPosition[0] = -10;
        }
    } else if (keysDown & 0x10) {
        screenPosition[1]--;
        if (screenPosition[1] < -10) {
            screenPosition[1] = -10;
        }
    } else if (keysDown & 0x40) {
        screenPosition[1]++;
        if (screenPosition[1] >= 25) {
            screenPosition[1] = 24;
        }
    }
    DispEnv0.screen.x = *(u_short *)&screenPosition[0];
    DispEnv0.screen.y = *(u_short *)&screenPosition[1];
    DispEnv1.screen.x = *(u_short *)&screenPosition[0];
    DispEnv1.screen.y = *(u_short *)&screenPosition[1];
    for (corner = 0; corner < 4; corner++) {
        AddPrim(OT[CurrentScreen], &polys->border[corner][CurrentScreen]);
    }
}


void DisplayConfig(ConfigData *gameData, MenuSelection *selection, HiScoreData *score, MenuPolys *polys)
{
    if (selection->moveScreen == 0) {
        if (GetMenuKey(0)) {
            selection->current--;
            if (selection->current < 0) {
                selection->current = 0;
            } else {
                Sound(8);
            }
        }
        if (GetMenuKey(1)) {
            selection->current++;
            if (selection->current >= 4) {
                selection->current = 3;
            } else {
                Sound(8);
            }
        }
    }
    if (selection->weapons != 0) {
        AddText(msgOn, 180, 90, 8, 0);
        AddText(msgOff, 210, 90, 8, 1);
    } else {
        AddText(msgOn, 180, 90, 8, 1);
        AddText(msgOff, 210, 90, 8, 0);
    }
    if (selection->internalView != 0) {
        AddText(msgInt, 181, 110, 8, 0);
        AddText(msgExt, 210, 110, 8, 1);
    } else {
        AddText(msgInt, 181, 110, 8, 1);
        AddText(msgExt, 210, 110, 8, 0);
    }
    if (selection->moveScreen != 0) {
        AddText(msgActive, 180, 130, 8, 0);
    } else {
        AddText(msgInactive, 180, 130, 8, 1);
    }
    switch (selection->current) {
    case 0:
        if (blinkCounter % 30 < 21) {
            AddText(msgWeapons, 30, 90, 8, 0);
        }
        AddText(msgDefaultView, 30, 110, 8, 1);
        AddText(msgMoveScreen, 30, 130, 8, 1);
        AddText(msgDefaultName, 30, 150, 8, 1);
        score->userEntry = 0;
        EnterName(gameData, score);
        if (keysPressed & 0x20) {
            if (selection->weapons != 0) {
                Sound(8);
            }
            selection->weapons = 0;
        }
        if (keysPressed & 0x80) {
            if (selection->weapons == 0) {
                Sound(8);
            }
            selection->weapons = 1;
        }
        break;
    case 1:
        AddText(msgWeapons, 30, 90, 8, 1);
        if (blinkCounter % 30 < 21) {
            AddText(msgDefaultView, 30, 110, 8, 0);
        }
        AddText(msgMoveScreen, 30, 130, 8, 1);
        AddText(msgDefaultName, 30, 150, 8, 1);
        score->userEntry = 0;
        EnterName(gameData, score);
        if (keysPressed & 0x20) {
            if (selection->internalView == 1) {
                Sound(8);
            }
            selection->internalView = 0;
        }
        if (keysPressed & 0x80) {
            if (selection->internalView == 0) {
                Sound(8);
            }
            selection->internalView = 1;
        }
        break;
    case 2:
        AddText(msgWeapons, 30, 90, 8, 1);
        AddText(msgDefaultView, 30, 110, 8, 1);
        if (blinkCounter % 30 < 21) {
            AddText(msgMoveScreen, 30, 130, 8, 0);
        }
        AddText(msgDefaultName, 30, 150, 8, 1);
        score->userEntry = 0;
        EnterName(gameData, score);
        if ((keysPressed & 0x2000) && !(keysDown & 0xC01)) {
            Sound(9);
            selection->moveScreen ^= 1;
        }
        if (selection->moveScreen != 0) {
            ScreenConfig(gameData, polys, &selection->screenX);
            Print(msgUseUpDownLeftAndRightOnThe, 30, 182, 8);
            Print(msgToAdjustScreenPosition, 30, 189, 8);
            DoButton(105, 198, 0, msgSaveScreenPosition);
        } else {
            DoButton(102, 198, 0, msgActivateMoveScreen);
        }
        break;
    case 3:
        AddText(msgWeapons, 30, 90, 8, 1);
        AddText(msgDefaultView, 30, 110, 8, 1);
        AddText(msgMoveScreen, 30, 130, 8, 1);
        if (blinkCounter % 30 < 21) {
            AddText(msgDefaultName, 30, 150, 8, 0);
        }
        score->userEntry = 1;
        DoButton(105, 198, 0, msgConfirmLetterEntry);
        PrintCX(msgNameIsUsedAsDefaultHighScore, 160, 186);
        EnterName(gameData, score);
        break;
    }
    blinkCounter += 1;
}


char *GetKeyFunction(int index)
{
    return keyFunctionNames[index];
}


void NegConConfig(ConfigData *gameData, KeyConfig *keyConfig, MenuPolys *polys, MenuSelection *selection)
{
    Positions x;
    Positions y;
    TwistAngles angles;
    char text[40];
    short i, cosine, sine;
    int degrees;
    short left, top;
    Texture *texture;
    unsigned short px;

    x = knobPosX;
    y = knobPosY;
    angles = twistAngles;
    cosine = 0;
    sine = 0;
    if (!negconEditing[menu[currMenu].selection]) {
        if (keysPressed & 128) {
            Sound(8);
            if (--menu[currMenu].selection < 0) {
                menu[currMenu].selection = 0;
            }
        } else if (keysPressed & 32) {
            Sound(8);
            if (++menu[currMenu].selection >= 2) {
                menu[currMenu].selection = 2;
            }
        }
    }
    if (keysPressed & 8192) {
        if (!(keysDown & 3073)) {
            Sound(9);
            negconEditing[menu[currMenu].selection] ^= 1;
            selection->negconFlags[menu[currMenu].selection] = 1;
        }
    }
    for (i = 0; i < 3; i++) {
        if (!selection->negconFlags[i]) {
            continue;
        }
        if (negconEditing[i]) {
            switch (i) {
            case 0:
                selection->negconCenter = padBuf1[0].steering;
                break;
            case 1:
                selection->negconMin = padBuf1[0].steering;
                break;
            case 2:
                if (keysPressed & 16) {
                    Sound(8);
                    if (--selection->negconType <= 0) {
                        selection->negconType = 0;
                    }
                } else if (keysPressed & 64) {
                    Sound(8);
                    if (++selection->negconType < 3) {
                        goto showTypeUp;
                    }
                    selection->negconType = 3;
                }
                if (selection->negconType < 3) {
showTypeUp:
                    if ((*(volatile int *)&negconBlink) % 30 < 21) {
                        DoButton(290, 149, 11, numTex);
                    }
                }
                if (selection->negconType > 0) {
                    if (negconBlink % 30 < 21) {
                        DoButton(290, 86, 10, numTex);
                    }
                }
                break;
            }
        }
        switch (i) {
        case 0:
            cosine = rcos((selection->negconCenter << 4) - 3072);
            sine = rsin((selection->negconCenter << 4) - 3072);
            if (selection->negconCenter <= 128) {
                degrees = 180 - (((selection->negconCenter * 3) * 15) >> 5);
            } else {
                degrees = (((selection->negconCenter - 128) * 3) * 15) >> 5;
            }
            if (selection->negconCenter == 0) {
                degrees = 180;
                break;
            }
            if (selection->negconCenter == 255) {
                degrees = 180;
            }
            break;
        case 1:
            cosine = rcos((selection->negconMin << 4) - 3072);
            sine = rsin((selection->negconMin << 4) - 3072);
            if (selection->negconMin <= 128) {
                degrees = 180 - (((selection->negconMin * 3) * 15) >> 5);
            } else {
                degrees = (((selection->negconMin - 128) * 3) * 15) >> 5;
            }
            if (selection->negconMin == 0) {
                degrees = 180;
            }
            break;
        case 2:
            cosine = rcos((angles.value[selection->negconType] << 4) - 1024);
            sine = rsin((angles.value[selection->negconType] << 4) - 1024);
            sprintf(text, fmtNewAngle, angles.value[selection->negconType]);
            goto drawSelection;
        default:
            goto drawSelection;
        }
        sprintf(text, fmtNewAngle, degrees);
drawSelection:
        PrintCX(text, x.value[i], 172);
        px = x.value[i];
        polys->arrowLine[i + 3][CurrentScreen].x0 = px;
        polys->arrowLine[i + 3][CurrentScreen].y0 = y.value[i];
        polys->arrowLine[i + 3][CurrentScreen].x1 = x.value[i] + ((cosine * 15) >> 11);
        polys->arrowLine[i + 3][CurrentScreen].y1 = y.value[i] + ((sine * 15) >> 11);
        AddPrim(OT[CurrentScreen], &polys->arrowLine[i + 3][CurrentScreen]);
    }
    PrintCX(msgMaximumLock, x.value[0], 86);
    PrintCX(msgCentreSetting, x.value[1], 86);
    PrintCX(msgDeadSpot, x.value[2], 86);
    cosine = rcos((padBuf1[0].steering << 4) - 3072);
    sine = rsin((padBuf1[0].steering << 4) - 3072);
    texture = textureTable[7 + menuTextures];
    for (i = 0; i < 3; i++) {
        switch (i) {
        case 0:
            cosine = rcos((gameData->negconCentre << 4) - 3072);
            sine = rsin((gameData->negconCentre << 4) - 3072);
            if (gameData->negconCentre <= 128) {
                degrees = 180 - (((gameData->negconCentre * 3) * 15) >> 5);
            } else {
                degrees = (((gameData->negconCentre - 128) * 3) * 15) >> 5;
            }
            if (gameData->negconCentre == 0) {
                degrees = 180;
                break;
            }
            if (gameData->negconCentre == 255) {
                degrees = 180;
            }
            break;
        case 1:
            cosine = rcos((gameData->negconMin << 4) - 3072);
            sine = rsin((gameData->negconMin << 4) - 3072);
            if (gameData->negconMin <= 128) {
                degrees = 180 - (((gameData->negconMin * 3) * 15) >> 5);
            } else {
                degrees = (((gameData->negconMin - 128) * 3) * 15) >> 5;
            }
            if (gameData->negconMin == 0) {
                degrees = 180;
            }
            break;
        case 2:
            cosine = rcos((angles.value[gameData->negconType] << 4) - 1024);
            sine = rsin((angles.value[gameData->negconType] << 4) - 1024);
            sprintf(text, fmtCurrent, angles.value[gameData->negconType]);
            goto drawGame;
        default:
            goto drawGame;
        }
        sprintf(text, fmtCurrent, degrees);
drawGame:
        PrintCX(text, x.value[i], 160);
        left = x.value[i] - 32;
        top = y.value[i] - 31;
        (polys->arrowTexture[i] + CurrentScreen)->x0 = left;
        (polys->arrowTexture[i] + CurrentScreen)->y0 = top;
        (polys->arrowTexture[i] + CurrentScreen)->x1 = left + (texture->textureW * 4);
        (polys->arrowTexture[i] + CurrentScreen)->y1 = top;
        (polys->arrowTexture[i] + CurrentScreen)->x2 = left;
        (polys->arrowTexture[i] + CurrentScreen)->y2 = top + texture->textureH;
        (polys->arrowTexture[i] + CurrentScreen)->x3 = left + (texture->textureW * 4);
        (polys->arrowTexture[i] + CurrentScreen)->y3 = top + texture->textureH;
        AddPrim(OT[CurrentScreen], &polys->arrowTexture[i][CurrentScreen]);
        polys->arrowLine[i][CurrentScreen].x0 = x.value[i];
        polys->arrowLine[i][CurrentScreen].y0 = y.value[i];
        polys->arrowLine[i][CurrentScreen].x1 = x.value[i] + ((cosine * 15) >> 11);
        polys->arrowLine[i][CurrentScreen].y1 = y.value[i] + ((sine * 15) >> 11);
        AddPrim(OT[CurrentScreen], &polys->arrowLine[i][CurrentScreen]);
    }
    DoButton(75, 180, 0, msgToSetOrResetNegconPosition);
    negconBlink++;
}


void JoyPadConfig(ConfigData *gameData, KeyConfig *keyConfig, MenuPolys *polys, MenuSelection *selection)
{
    DigitalButtons standard = padDigitalKeys;
    AnalogButtons alternate = padAnalogKeys;
    HighlightWidths widths = keyHighlightWidth;
    short i, j, k, duplicates, layout;
    int other;
    unsigned short button;
    unsigned short *keys;
    ControlInput *pad = playerControl;

    buildGameKeyPad(padBuf1, pad);
    duplicates = 0;
    layout = 0;
    if (duplicateKeyTimer > 0) {
        PrintCX(msgAButtonIsDefined, 160, 120);
        PrintCX(msgMoreThanOnce, 160, 127);
        Popup();
        duplicateKeyTimer--;
    }
    switch (pad->head) {
    case 1:
        break;
    case 2:
        layout = 1;
        break;
    }
    for (i = 0; i < 6; i++) {
        Print(GetKeyFunction(i), 30, i * 16 + 88, 8);
    }
    if (GetMenuKey(1)) {
        keyConfig->active++;
        if (keyConfig->active >= 5) {
            keyConfig->active = 5;
        } else {
            Sound(8);
        }
    }
    if (GetMenuKey(0)) {
        keyConfig->active--;
        if (keyConfig->active < 0) {
            keyConfig->active = 0;
        } else {
            Sound(8);
        }
    }
    for (i = 0; (selection->keys + layout)->key[i] != 0; i++) {
        if (PortShowsKeys()) {
            /* Port: the keyboard's keys are fixed, so each row shows the key of its action, not the button bound to it */
            DoButton(150, (short)(i * 16 + 83), PORT_ICON_ACTION + i, numTex);
            continue;
        }
        switch ((unsigned short)(selection->keys + layout)->key[i]) {
        case 0x1000:
            DoButton(150, (short)(i * 16 + 83), 3, numTex);
            break;
        case 0x4000:
            DoButton(150, (short)(i * 16 + 83), 1, numTex);
            break;
        case 0x2000:
            DoButton(150, (short)(i * 16 + 83), 0, numTex);
            break;
        case 0x400:
            Print(PortButtonLabel(PORT_BUTTON_L1, msgL1Button), 150, i * 16 + 88, 8); /* Port: the name of the pad's button */
            break;
        case 0x100:
            Print(PortButtonLabel(PORT_BUTTON_L2, msgL2Button), 150, i * 16 + 88, 8); /* Port: the name of the pad's button */
            break;
        case 0x800:
            if (playerControl[0].head == 2) {
                Print(msgRButton, 150, i * 16 + 88, 8);
            } else {
                Print(PortButtonLabel(PORT_BUTTON_R1, msgR1Button), 150, i * 16 + 88, 8); /* Port: the name of the pad's button */
            }
            break;
        case 0x200:
            Print(PortButtonLabel(PORT_BUTTON_R2, msgR2Button), 150, i * 16 + 88, 8); /* Port: the name of the pad's button */
            break;
        case 0x8FF0:
            DoButton(150, (short)(i * 16 + 83), 1, numTex);
            break;
        case 0x8000:
            DoButton(150, (short)(i * 16 + 83), 2, numTex);
            break;
        case 0x8FF1:
            DoButton(150, (short)(i * 16 + 83), 2, numTex);
            break;
        case 0x8FF2:
            Print(msgLButton, 150, i * 16 + 88, 8);
            break;
        }
    }
    (polys->purple + CurrentScreen)->x0 = 28;
    polys->purple[CurrentScreen].y0 = keyConfig->active * 16 + 86;
    polys->purple[CurrentScreen].x1 = widths.value[keyConfig->active] + 28;
    polys->purple[CurrentScreen].y1 = keyConfig->active * 16 + 86;
    polys->purple[CurrentScreen].x2 = 28;
    polys->purple[CurrentScreen].y2 = keyConfig->active * 16 + 95;
    polys->purple[CurrentScreen].x3 = widths.value[keyConfig->active] + 28;
    polys->purple[CurrentScreen].y3 = keyConfig->active * 16 + 95;
    AddPrim(OT[CurrentScreen], &polys->purple[CurrentScreen]);
    if (GetMenuKey(5)) {
        for (k = 0; k < 6; k++) {
            for (j = 0; j < 6; j++) {
                if ((selection->keys + layout)->key[k] == (selection->keys + layout)->key[j]) {
                    duplicates++;
                }
            }
            if (duplicates >= 2) {
                duplicateKeyTimer = 90;
                break;
            }
            duplicates = 0;
        }
        if (duplicateKeyTimer == 0) {
            keyDefineThrustPad[layout] = selection->keys[layout].key[0];
            keyDefineFire[layout] = selection->keys[layout].key[1];
            keyDefineView[layout] = selection->keys[layout].key[2];
            keyDefineDitch[layout] = selection->keys[layout].key[3];
            keyDefineRBrake[layout] = selection->keys[layout].key[4];
            keyDefineLBrake[layout] = selection->keys[layout].key[5];
            *(keyConfig->keys + layout) = *(selection->keys + layout);
            *(keyConfig->calibration + layout) = *(selection->calibration + layout);
            prevMenu = currMenu;
            currMenu = menu[currMenu].nextMenu;
            keyConfig->active = 0;
            InitSlider();
            Sound(9);
            return;
        }
    } else if (GetMenuKey(4)) {
        prevMenu = currMenu;
        currMenu = menu[currMenu].prevMenu;
        *(selection->keys + layout) = *(keyConfig->keys + layout);
        *(selection->calibration + layout) = *(keyConfig->calibration + layout);
        keyConfig->active = 0;
        InitSlider();
        Sound(49);
        return;
    }
    if (GetMenuKey(3)) {
        (selection->calibration + layout)->value[keyConfig->active]++;
        other = layout ^ 1;
        if ((selection->calibration + layout)->value[keyConfig->active] >= other * 2 + 5) {
            (selection->calibration + layout)->value[keyConfig->active] = (unsigned short)other * 2 + 5;
        } else {
            Sound(8);
        }
    }
    if (GetMenuKey(2)) {
        (selection->calibration + layout)->value[keyConfig->active]--;
        if ((selection->calibration + layout)->value[keyConfig->active] <= 0) {
            (selection->calibration + layout)->value[keyConfig->active] = 0;
        } else {
            Sound(8);
        }
    }
    DoButton(125, 198, 0, msgDefaultConfig);
    if ((keysPressed & 0x2000) && !(keysDown & 0xC01)) {
        ResetKeyConfig(selection);
        Sound(9);
    }
    if (layout) {
        (selection->keys + layout)->key[keyConfig->active] = alternate.button[(selection->calibration + layout)->value[keyConfig->active]];
    } else {
        button = standard.button[(selection->calibration + layout)->value[keyConfig->active]];
        keys = selection->keys[0].key;
        keys[keyConfig->active] = button;
    }
    Print(msgUseTheLeftAndRightDirectional, 30, 182, 8);
    Print(msgToChangeCurrentButtonSettings, 30, 189, 8);
}


void AudioConfig(ConfigData *gameData, MenuPolys *polys, MenuSelection *selection, MusicScreen *musicScreen)
{
    signed char savedTrack;
    Texture *texture;
    int track;
    int musicVolume;
    int sfxVolume;
    int musicX;
    int sfxX;
    int screenIndex;

    savedTrack = gameData->cdTrack;
    gameData->cdTrack = selection->music;
    SetInGameCDTrack(cdPlaySeconds, cdStatus, gameData, cdFlag);
    gameData->cdTrack = savedTrack;
    texture = textureTable[2 + menuTextures];
    if (GetMenuKey(0)) {
        selection->current--;
        if (selection->current < 0) {
            selection->current = 0;
        } else {
            Sound(8);
        }
    }
    if (GetMenuKey(1)) {
        selection->current++;
        if (selection->current >= 4) {
            selection->current = 3;
        } else {
            Sound(8);
        }
    }
    if (selection->current != 1) {
        SetVolume(*(short *)&ambientVoice1, 0);
        SfxEffectLRPan(ambientVoice1, 15);
        SfxEffectFBPan(ambientVoice1, 15);
    }
    switch (selection->current) {
    case 0:
        if (audioBlink % 30 < 21) {
            AddText(labelMusic, 30, 94, 8, 0);
        }
        AddText(labelSfx, 30, 123, 8, 1);
        AddText(labelCdTrackMenu, 25, 184, 8, 1);
        AddText(labelSound, 133, 150, 8, 1);
        if (keysDown & 0x20) {
            selection->musicVolume++;
            if (selection->musicVolume >= 100) {
                selection->musicVolume = 100;
            }
        }
        if (keysDown & 0x80) {
            selection->musicVolume--;
            if (selection->musicVolume < 0) {
                selection->musicVolume = 0;
            }
        }
        selection->otherMusic = selection->musicVolume;
        SetCdVolume(((selection->musicVolume * 255) / 100) & 255);
        break;
    case 1:
        AddText(labelMusic, 30, 94, 8, 1);
        if (audioBlink % 30 < 21) {
            AddText(labelSfx, 30, 123, 8, 0);
        }
        AddText(labelCdTrackMenu, 25, 184, 8, 1);
        AddText(labelSound, 133, 150, 8, 1);
        if (keysDown & 0x20) {
            selection->effectVolume++;
            if (selection->effectVolume >= 100) {
                selection->effectVolume = 100;
            }
        }
        if (keysDown & 0x80) {
            selection->effectVolume--;
            if (selection->effectVolume < 0) {
                selection->effectVolume = 0;
            }
        }
        selection->otherEffect = selection->effectVolume;
        SetSoundVolume(((selection->effectVolume * 255) / 100) & 255);
        SetVolume(*(short *)&ambientVoice1, 255);
        SfxEffectLRPan(ambientVoice1, 15);
        SfxEffectFBPan(ambientVoice1, 15);
        break;
    case 3:
        AddText(labelMusic, 30, 94, 8, 1);
        AddText(labelSfx, 30, 123, 8, 1);
        if (audioBlink % 30 < 21) {
            AddText(labelCdTrackMenu, 25, 184, 8, 0);
        }
        AddText(labelSound, 133, 150, 8, 1);
        if (GetMenuKey(2)) {
            Sound(8);
            selection->music--;
            if (selection->music <= 0) {
                selection->music = cdMusicCount;
            }
            track = selection->music;
            if (track == 1) {
                InitShufflePlay();
                track = cdMusicTable[0];
                *(volatile char *)&cdMusicIndex = 0;
                cdMusicTrack = track;
                CdMusicPlay(track, 0);
            } else {
                CdMusicPlay(track, 0);
            }
        }
        if (GetMenuKey(3)) {
            Sound(8);
            selection->music++;
            if (cdMusicCount < selection->music) {
                selection->music = 1;
            }
            track = selection->music;
            if (track != 1) {
                CdMusicPlay(track, 0);
            } else {
                InitShufflePlay();
                cdMusicIndex = 0;
                cdMusicTrack = cdMusicTable[0];
                CdMusicPlay(cdMusicTable[0], 0);
            }
        }
        break;
    case 2:
        AddText(labelMusic, 30, 94, 8, 1);
        AddText(labelSfx, 30, 123, 8, 1);
        AddText(labelCdTrackMenu, 25, 184, 8, 1);
        if (audioBlink % 30 < 21) {
            AddText(labelSound, 133, 150, 8, 0);
        }
        if (GetMenuKey(3)) {
            selection->stereo--;
            if (selection->stereo < 0) {
                selection->stereo = 0;
            } else {
                Sound(8);
                if (selection->stereo == 0) {
                    sfxMode = 4;
                } else {
                    sfxMode = 1;
                }
                TerminateSound();
                SfxSetMode(sfxMode);
                StartControlledSound();
            }
        }
        if (GetMenuKey(2)) {
            selection->stereo++;
            if (selection->stereo >= 2) {
                selection->stereo = 1;
            } else {
                Sound(8);
                if (selection->stereo == 0) {
                    sfxMode = 4;
                } else {
                    sfxMode = 1;
                }
                TerminateSound();
                SfxSetMode(sfxMode);
                StartControlledSound();
            }
        }
        break;
    }
    Print(GetCdTrackName(selection->music - 1), 210, 190, 8);
    AddNumber(selection->music - 1, 135, 184, 8, 1);
    Print(GetCdTrackArtist(selection->music - 1), 210, 182, 8);
    Print(labelArtist, 170, 182, 8);
    Print(labelTitle, 170, 190, 8);
    if (selection->stereo != 0) {
        CentreText(labelMono, 160, 164, 8, 0);
    } else {
        CentreText(labelStereo, 160, 164, 8, 0);
    }
    musicVolume = selection->musicVolume;
    screenIndex = CurrentScreen;
    sfxVolume = selection->effectVolume;
    musicX = (musicVolume * 0x1947) >> 12;
    (musicScreen->rows[MUSIC_MARKER] + screenIndex)->x0 = musicX + 100;
    musicX = musicX + 100;
    (musicScreen->rows[MUSIC_MARKER] + screenIndex)->y0 = 86;
    (musicScreen->rows[MUSIC_MARKER] + screenIndex)->x1 = texture->textureW * 4 + musicX;
    (musicScreen->rows[MUSIC_MARKER] + screenIndex)->y1 = 86;
    (musicScreen->rows[MUSIC_MARKER] + screenIndex)->x2 = musicX;
    (musicScreen->rows[MUSIC_MARKER] + screenIndex)->y2 = texture->textureH + 86;
    (musicScreen->rows[MUSIC_MARKER] + screenIndex)->x3 = texture->textureW * 4 + musicX;
    sfxX = ((sfxVolume * 0x1947) >> 12) + 100;
    (musicScreen->rows[MUSIC_MARKER] + screenIndex)->y3 = texture->textureH + 86;
    (musicScreen->rows[SFX_MARKER] + screenIndex)->x0 = sfxX;
    (musicScreen->rows[SFX_MARKER] + screenIndex)->y0 = 115;
    (musicScreen->rows[SFX_MARKER] + screenIndex)->x1 = texture->textureW * 4 + sfxX;
    (musicScreen->rows[SFX_MARKER] + screenIndex)->y1 = 115;
    (musicScreen->rows[SFX_MARKER] + screenIndex)->x2 = sfxX;
    (musicScreen->rows[SFX_MARKER] + screenIndex)->y2 = texture->textureH + 115;
    (musicScreen->rows[SFX_MARKER] + screenIndex)->x3 = texture->textureW * 4 + sfxX;
    (musicScreen->rows[SFX_MARKER] + screenIndex)->y3 = texture->textureH + 115;
    audioBlink += 1;
}


extern char msgThrust[];
extern char msgTopSpeed[];
extern char msgTurningAbility[];
extern char msgShieldEnergy[];
extern char msgAerodynamics[];

typedef struct ShipParameterBars {
    u8 bar[5][5];
} ShipParameterBars;
extern ShipParameterBars shipParameterBars;

/* Draws the five performance bars of the selected ship at (x, y).
   Port: the byte offsets into the polygons are added to uintptr_t, not int. */
#define polys ((MenuPolys *)shipShapes)

void ShipParameters(MenuSelection *selection, ConfigData *gameData, void *shipShapes, int x, int y)
{
    ShipParameterBars table;
    u8 *bars;
    u8 p0, p1, p2, p3, p4;
    int barScale;
    int left32, right, bottom0, bottom1, bottom2, bottom3, bottom4;
    int w0, w1, w2, w3, w4;
    MenuPolys *row1, *row2, *row3, *row4, *bar;
    short left, rowTop, team, screen, i;

    table = shipParameterBars;
    rowTop = y;
    team = (signed char)((unsigned char)selection->ship) / 3;
    Print(msgThrust, (short)x, (short)y, 8);
    Print(msgTopSpeed, (short)x, (short)y + 7, 8);
    Print(msgTurningAbility, (short)x, (short)y + 0xE, 8);
    Print(msgShieldEnergy, (short)x, (short)y + 0x15, 8);
    Print(msgAerodynamics, (short)x, (short)y + 0x1C, 8);
    bars = table.bar[team];
    p0 = bars[0];
    p1 = bars[1];
    p2 = bars[2];
    p3 = bars[3];
    p4 = bars[4];
    left32 = x + 0x4D;
    barScale = 7;
    right = x + 0x93;
    screen = CurrentScreen;
    bar = (MenuPolys *)(screen * sizeof(POLY_G4) + (uintptr_t)polys);
    /* These interior aliases preserve the proven menu selection update codegen. */
    row1 = (MenuPolys *)((char *)polys + 0x48);
    row2 = (MenuPolys *)((char *)polys + 0x90);
    row3 = (MenuPolys *)((char *)polys + 0xD8);
    row4 = (MenuPolys *)((char *)polys + 0x120);
    bar->shade0[0].y0 = rowTop;
    bar->shade0[0].y1 = rowTop;
    bar->selection[0][0].y0 = rowTop;
    bar->selection[0][0].y1 = rowTop;
    rowTop = y + 7;
    bottom0 = y + 5;
    bar->shade0[0].x0 = left32;
    left = left32;
    bar->shade0[0].x2 = left;
    bottom1 = y + 0xC;
    bar->shade0[0].y2 = bottom0;
    bar->shade0[0].y3 = bottom0;
    bar->selection[0][0].x0 = left;
    bar->selection[0][0].x1 = right;
    bar->selection[0][0].x2 = left;
    bar->selection[0][0].y2 = bottom0;
    bar->selection[0][0].x3 = right;
    bar->selection[0][0].y3 = bottom0;
    bar->shade1[0].x0 = left;
    bar->shade1[0].y0 = rowTop;
    bar->shade1[0].y2 = bottom1;
    bar->shade1[0].y1 = rowTop;
    bar->shade1[0].x2 = left;
    w0 = p0 * barScale + left32;
    bar->shade1[0].y3 = bottom1;
    bar->shade0[0].x1 = w0;
    bar->shade0[0].x3 = w0;
    w1 = p1 * barScale + left32;
    bar->shade1[0].x1 = w1;
    bar->shade1[0].x3 = w1;
    ((MenuPolys *)(screen * sizeof(POLY_G4) + (uintptr_t)row1))->selection[0][0].x0 = left;
    ((MenuPolys *)(screen * sizeof(POLY_G4) + (uintptr_t)row1))->selection[0][0].y0 = rowTop;
    ((MenuPolys *)(screen * sizeof(POLY_G4) + (uintptr_t)row1))->selection[0][0].x1 = right;
    ((MenuPolys *)(screen * sizeof(POLY_G4) + (uintptr_t)row1))->selection[0][0].y1 = rowTop;
    rowTop = y + 0xE;
    ((MenuPolys *)(screen * sizeof(POLY_G4) + (uintptr_t)row1))->selection[0][0].y2 = bottom1;
    ((MenuPolys *)(screen * sizeof(POLY_G4) + (uintptr_t)row1))->selection[0][0].y3 = bottom1;
    bottom2 = y + 0x13;
    ((MenuPolys *)(screen * sizeof(POLY_G4) + (uintptr_t)row1))->selection[0][0].x2 = left;
    ((MenuPolys *)(screen * sizeof(POLY_G4) + (uintptr_t)row1))->selection[0][0].x3 = right;
    bar->shade2[0].x0 = left;
    bar->shade2[0].y0 = rowTop;
    bar->shade2[0].y1 = rowTop;
    bar->shade2[0].x2 = left;
    bar->shade2[0].y2 = bottom2;
    bar->shade2[0].y3 = bottom2;
    w2 = p2 * barScale + left32;
    bar->shade2[0].x1 = w2;
    bar->shade2[0].x3 = w2;
    ((MenuPolys *)(screen * sizeof(POLY_G4) + (uintptr_t)row2))->selection[0][0].y0 = rowTop;
    ((MenuPolys *)(screen * sizeof(POLY_G4) + (uintptr_t)row2))->selection[0][0].y1 = rowTop;
    rowTop = y + 0x15;
    ((MenuPolys *)(screen * sizeof(POLY_G4) + (uintptr_t)row2))->selection[0][0].y2 = bottom2;
    ((MenuPolys *)(screen * sizeof(POLY_G4) + (uintptr_t)row2))->selection[0][0].x0 = left;
    ((MenuPolys *)(screen * sizeof(POLY_G4) + (uintptr_t)row2))->selection[0][0].y3 = bottom2;
    bottom3 = y + 0x1A;
    ((MenuPolys *)(screen * sizeof(POLY_G4) + (uintptr_t)row2))->selection[0][0].x1 = right;
    ((MenuPolys *)(screen * sizeof(POLY_G4) + (uintptr_t)row2))->selection[0][0].x2 = left;
    ((MenuPolys *)(screen * sizeof(POLY_G4) + (uintptr_t)row2))->selection[0][0].x3 = right;
    bar->shade3[0].x0 = left;
    bar->shade3[0].y0 = rowTop;
    bar->shade3[0].y1 = rowTop;
    bar->shade3[0].x2 = left;
    bar->shade3[0].y2 = bottom3;
    bar->shade3[0].y3 = bottom3;
    w3 = p3 * barScale + left32;
    bar->shade3[0].x1 = w3;
    bar->shade3[0].x3 = w3;
    ((MenuPolys *)(screen * sizeof(POLY_G4) + (uintptr_t)row3))->selection[0][0].y0 = rowTop;
    ((MenuPolys *)(screen * sizeof(POLY_G4) + (uintptr_t)row3))->selection[0][0].y1 = rowTop;
    rowTop = y + 0x1C;
    bottom4 = y + 0x21;
    ((MenuPolys *)(screen * sizeof(POLY_G4) + (uintptr_t)row3))->selection[0][0].x0 = left;
    ((MenuPolys *)(screen * sizeof(POLY_G4) + (uintptr_t)row3))->selection[0][0].x1 = right;
    ((MenuPolys *)(screen * sizeof(POLY_G4) + (uintptr_t)row3))->selection[0][0].x2 = left;
    ((MenuPolys *)(screen * sizeof(POLY_G4) + (uintptr_t)row3))->selection[0][0].y2 = bottom3;
    ((MenuPolys *)(screen * sizeof(POLY_G4) + (uintptr_t)row3))->selection[0][0].x3 = right;
    ((MenuPolys *)(screen * sizeof(POLY_G4) + (uintptr_t)row3))->selection[0][0].y3 = bottom3;
    bar->shade4[0].x0 = left;
    bar->shade4[0].y0 = rowTop;
    bar->shade4[0].y1 = rowTop;
    bar->shade4[0].x2 = left;
    bar->shade4[0].y2 = bottom4;
    bar->shade4[0].y3 = bottom4;
    w4 = p4 * barScale + left32;
    bar->shade4[0].x1 = w4;
    bar->shade4[0].x3 = w4;
    ((MenuPolys *)(screen * sizeof(POLY_G4) + (uintptr_t)row4))->selection[0][0].x0 = left;
    ((MenuPolys *)(screen * sizeof(POLY_G4) + (uintptr_t)row4))->selection[0][0].y0 = rowTop;
    ((MenuPolys *)(screen * sizeof(POLY_G4) + (uintptr_t)row4))->selection[0][0].x1 = right;
    ((MenuPolys *)(screen * sizeof(POLY_G4) + (uintptr_t)row4))->selection[0][0].y1 = rowTop;
    ((MenuPolys *)(screen * sizeof(POLY_G4) + (uintptr_t)row4))->selection[0][0].x2 = left;
    ((MenuPolys *)(screen * sizeof(POLY_G4) + (uintptr_t)row4))->selection[0][0].y2 = bottom4;
    ((MenuPolys *)(screen * sizeof(POLY_G4) + (uintptr_t)row4))->selection[0][0].x3 = right;
    ((MenuPolys *)(screen * sizeof(POLY_G4) + (uintptr_t)row4))->selection[0][0].y3 = bottom4;
    AddPrim(OT[CurrentScreen], &polys->shade0[CurrentScreen]);
    AddPrim(OT[CurrentScreen], &polys->mode0[CurrentScreen]);
    AddPrim(OT[CurrentScreen], &polys->shade1[CurrentScreen]);
    AddPrim(OT[CurrentScreen], &polys->mode1[CurrentScreen]);
    AddPrim(OT[CurrentScreen], &polys->shade2[CurrentScreen]);
    AddPrim(OT[CurrentScreen], &polys->mode2[CurrentScreen]);
    AddPrim(OT[CurrentScreen], &polys->shade3[CurrentScreen]);
    AddPrim(OT[CurrentScreen], &polys->mode3[CurrentScreen]);
    AddPrim(OT[CurrentScreen], &polys->shade4[CurrentScreen]);
    i = 0;
    AddPrim(OT[CurrentScreen], &polys->mode4[CurrentScreen]);
    do {
        AddPrim(OT[CurrentScreen], &polys->selection[i][CurrentScreen]);
        i++;
    } while (i < 5);
}

#undef polys


void TeamSelect(ConfigData *gameData, MenuSelection *selection, void *shipShapes)
{
    int pilotsPerTeam = 3;
    int shipNo;
    int teamPreviewScratch[4];
    TeamOrder teamOrder = *teamShipGroup;
    char *teamDescription;
    s16 selectedTeam;
    s32 teamCount;
    u16 nextTeam;
    u16 previousTeam;

    teamCount = 4;
    if (gameData->class2Unlocked != 0) {
        teamCount = 5;
    }
    if (gameData->teamCheatActive != 0) {
        teamCount = 5;
        PrintCX(msgTeamCheatActive, 160, 204);
    }
    if (GetMenuKey(1)) {
        nextTeam = selection->current + 1;
        selection->current = nextTeam;
        if ((s16) nextTeam >= teamCount) {
            selection->current = (u16) (teamCount - 1);
        } else {
            Sound(8);
        }
    }
    if (GetMenuKey(0)) {
        previousTeam = selection->current - 1;
        selection->current = previousTeam;
        if ((short) previousTeam < 0) {
            selection->current = 0U;
        } else {
            Sound(8);
        }
    }
    shipNo = pilotsPerTeam * teamOrder.order[(s16) selection->current];
    teamSelectFrames += 1;
    selection->ship = shipNo;
    if (sillyShips == 1) {
        DrawTeamShips(450, 50, 1200, heading, 0, shipShapeTable[highResTable[selection->ship]]);
    } else {
        DrawTeamShips(450, 0, 1200, heading, 0, shipShapeTable[highResTable[selection->ship]]);
    }
    if (selection->current == 0) {
        AddText(teamNameFeisar, 30, 90, 8, 0);
    } else {
        AddText(teamNameFeisar, 30, 90, 8, 1);
    }
    if (selection->current == 1) {
        AddText(teamNameAgSystems, 30, 100, 8, 0);
    } else {
        AddText(teamNameAgSystems, 30, 100, 8, 1);
    }
    if (selection->current == 2) {
        AddText(teamNameAuricom, 30, 110, 8, 0);
    } else {
        AddText(teamNameAuricom, 30, 110, 8, 1);
    }
    if (selection->current == 3) {
        AddText(teamNameQirex, 30, 120, 8, 0);
    } else {
        AddText(teamNameQirex, 30, 120, 8, 1);
    }
    if (selection->current == 4) {
        AddText(teamNamePiranha, 30, 130, 8, 0);
    } else if (*(s32 *) &gameData->rapierChallengeDone & 0xFF00FF00) {
        AddText(teamNamePiranha, 30, 130, 8, 1);
    }
    selectedTeam = (s16) selection->current;
    switch (selectedTeam) {
    case 0:
        Print(msgExcellentTurningAbilityAnd, 30, 182, 8);
        teamDescription = msgButNotTooQuickBeginner;
        break;
    case 1:
        Print(msgGoodAccelerationAndTurning, 30, 182, 8);
        teamDescription = msgWeakShieldAmateur;
        break;
    case 2:
        Print(msgGoodAllRounderButAverage, 30, 182, 8);
        teamDescription = msgIntermediate;
        break;
    case 3:
        Print(msgFastWithGoodShieldCapability, 30, 182, 8);
        teamDescription = msgHeavyAtTurningExpert;
        break;
    case 4:
        Print(msgVeryFastWithExcellentShield, 30, 182, 8);
        teamDescription = msgWeaponTechnologyMaster;
        break;
    default:
        goto ship_parameters;
    }
    Print(teamDescription, 30, 189, 8);
ship_parameters:
    ShipParameters(selection, gameData, shipShapes, 155, 142);
}


void ClassSelect(ConfigData *gameData, MenuSelection *selection)
{
    ClassOrder classOrder = *classOrderTable;
    char *trackName;
    char *trackDescription;
    char *className;
    int optionCount;
    short trackCount;
    u_char track;
    int original;

    trackCount = 2;
    if (gameData->class1Unlocked != 0 || gameData->phantomTrackCheatActive != 0) {
        trackCount = 6;
        if (selection->current == 3) {
            trackCount = 2;
        }
        if (gameData->phantomTrackCheatActive != 0) {
            PrintCX(msgPhantomTrackCheatActive, 210, 75);
        }
    }
    if (gameData->rapierComplete != 0 || gameData->trackCheatActive != 0) {
        trackCount = 8;
        if ((u_short)selection->current < 2U) {
            trackCount = 6;
        }
        if (gameData->trackCheatActive != 0) {
            PrintCX(msgTrackCheatActive, 160, 204);
        }
    }
    optionCount = 3;
    if (menu[2].options[3].enabled != 0) {
        optionCount = 4;
    }
    /* Port: the lines shown, WIPEOUT after the classes (portorig.c) */
    if (GetMenuKey(1)) {
        short moved = (short)PortClassMove(selection->current, 1, optionCount);

        if (moved != selection->current) {
            Sound(8);
        }
        selection->current = moved;
    }
    if (GetMenuKey(0)) {
        short moved = (short)PortClassMove(selection->current, -1, optionCount);

        if (moved != selection->current) {
            Sound(8);
        }
        selection->current = moved;
    }
    original = PortOriginalClassMenu(selection, &trackCount, 0); /* Port: the WIPEOUT line's tracks */
    if (GetMenuKey(3)) {
        selection->trackSelection++;
        if (selection->trackSelection < trackCount) {
            Sound(8);
        }
    }
    if (selection->trackSelection >= trackCount - 1) {
        selection->trackSelection = trackCount - 1;
    }
    if (GetMenuKey(2)) {
        selection->trackSelection--;
        if (selection->trackSelection < 0) {
            selection->trackSelection = 0;
        } else {
            Sound(8);
        }
    }
    if (gameData->class1Unlocked != 0 || gameData->rapierComplete != 0 || (*(s32 *)&gameData->entered.digits[12] & 0xFFFF0000)) {
        selection->actTrackNum = raceTracks[selection->trackSelection];
        selection->trackNum = selection->trackSelection;
    }
    if (selection->current == 0) {
        track = selection->trackSelection;
        if (track >= 8U) {
            track = 7;
        }
        if (gameData->class1Unlocked == 0 || gameData->rapierComplete == 0 || gameData->phantomTrackCheatActive == 0 || gameData->trackCheatActive == 0) {
            selection->actTrackNum = raceTracks[track];
            selection->trackNum = track;
        }
        selection->shipClass = 0;
    }
    if (selection->current == 1) {
        track = selection->trackSelection + 2;
        if (track >= 8U) {
            track = 7;
        }
        if (gameData->class1Unlocked == 0 && gameData->rapierComplete == 0 && !(*(s32 *)&gameData->entered.digits[12] & 0xFFFF0000)) {
            selection->actTrackNum = raceTracks[track];
            selection->trackNum = track;
        }
        selection->shipClass = 1;
    }
    if (selection->current == 2) {
        track = selection->trackSelection + 4;
        if (track >= 8U) {
            track = 7;
        }
        if (gameData->class1Unlocked == 0 && gameData->rapierComplete == 0 && !(*(s32 *)&gameData->entered.digits[12] & 0xFFFF0000)) {
            selection->actTrackNum = raceTracks[track];
            selection->trackNum = track;
        }
        selection->shipClass = 2;
    }
    if (selection->current == 3) {
        track = selection->trackSelection + 6;
        if (track >= 8U) {
            track = 7;
        }
        if (gameData->rapierComplete == 0 && gameData->trackCheatActive == 0) {
            selection->actTrackNum = raceTracks[track];
            selection->trackNum = track;
        }
        selection->shipClass = 3;
    }
    if (original) {
        /* Port: the original's track (left and right went through them above) */
        PortOriginalClassMenu(selection, &trackCount, 1);
        goto afterTrackDescription;
    }
    switch (selection->current) {
    case 0:
        className = msgNovice;
        break;
    case 1:
        className = msgIntermediate2;
        break;
    case 2:
        className = msgExpert;
        break;
    case 3:
        className = msgSpeedDemon;
        break;
    default:
        goto afterClassName;
    }
    PrintCX(className, 68, 186);
afterClassName:
    switch (selection->trackNum) {
    case 0:
        Print(msgAMajorCanadianIndustrial, 120, 182, 8);
        trackDescription = msgComplexEasy;
        break;
    case 1:
        Print(msgASnowyTibetanMountainCircuit, 120, 182, 8);
        trackDescription = msgEasy;
        break;
    case 2:
        Print(msgASouthAmericanJungleCircuit, 120, 182, 8);
        trackDescription = msgTricky;
        break;
    case 3:
        Print(msgANewlyConstructedGerman, 120, 182, 8);
        trackDescription = msgCommercialParkTricky;
        break;
    case 4:
        Print(msgADisusedFrenchMetroSystem, 120, 182, 8);
        trackDescription = msgHard;
        break;
    case 5:
        Print(msgAHugeSuspendedCircuitOver, 120, 182, 8);
        trackDescription = msgBlackSeaHard;
        break;
    case 6:
        Print(msgAnObscureVolcanicIslandLocated, 120, 182, 8);
        trackDescription = msgInTheSouthPacificVeryHard;
        break;
    case 7:
        Print(msgAnAmericanCityCircuitAffected, 120, 182, 8);
        trackDescription = msgBySeismicActivityVeryHard;
        break;
    default:
        goto afterTrackDescription;
    }
    Print(trackDescription, 120, 189, 8);
afterTrackDescription:
    if (selection->trackSelection < trackCount - 1 && classBlink % 30 < 21) {
        DoButton(280, 165, 5, numTex);
    }
    if (selection->trackSelection > 0 && classBlink % 30 < 21) {
        DoButton(125, 165, 4, numTex);
    }
    if (original) {
        /* Port: no model of the original's track; its name; no medals (its races keep no records) */
        PrintCX((u_char *)PortRaceTrackName(selection->actTrackNum), 160, 90);
        DrawTeamShips(-780, 40, 1400, heading, 0, menuModels[classOrder.order[selection->shipClass]]);
        classBlink += 1;
        return;
    }
    PortTwoSided(1); /* Port: the track model's surfaces shown from behind too */
    DrawTeamShips(400, -200, 1800, heading, 220, shipModels[trackModelOrder[selection->trackNum]]);
    PortTwoSided(0);
    PrintCX(GetTrackName(selection->actTrackNum, 90, 90, 0), 160, 90);
    switch (trackMedals[selection->trackNum]) {
    case 1:
        if (classBlink % 30 < 16) {
            PrintCX(msgGold, 260, 90);
            PrintCX(tempheapcopy, 260, 97);
        }
        envmapr = 255;
        envmapg = 128;
        envmapb = 0;
        break;
    case 2:
        if (classBlink % 30 < 16) {
            PrintCX(msgSilver, 260, 90);
            PrintCX(tempheapcopy, 260, 97);
        }
        envmapr = 224;
        envmapg = 224;
        envmapb = 224;
        break;
    case 3:
        if (classBlink % 30 < 16) {
            PrintCX(msgBronze, 260, 90);
            PrintCX(tempheapcopy, 260, 97);
        }
        envmapr = 192;
        envmapg = 32;
        envmapb = 0;
        break;
    }

    if (trackMedals[selection->trackNum] != 0) {
        DrawTeamShips(-780, 40, 1400, heading, -1, menuModels[classOrder.order[selection->current]]);
        goto afterClassObject;
    }
    DrawTeamShips(-780, 40, 1400, heading, 0, menuModels[classOrder.order[selection->current]]);
afterClassObject:
    classBlink += 1;
}


void MenuFunction(void *gameData, void *polys, void *keyConfig, void *selection, void *cardState, void *musicScreen)
{
    switch (currMenu) {
    case 5: case 6: case 7: case 8:
        ClassSelect(gameData, selection);
        break;
    case 4:
        TeamSelect(gameData, selection, polys);
        break;
    case 9:
        AudioConfig(gameData, polys, selection, musicScreen);
        break;
    case 21:
        NegConConfig(gameData, keyConfig, polys, selection);
        break;
    case 10: case 22:
        JoyPadConfig(gameData, keyConfig, polys, selection);
        break;
    case 15:
        DisplayConfig(gameData, selection, cardState, polys);
        break;
    case 16:
        ScreenConfig(gameData, polys, selection);
        break;
    case 12:
        LoadSave(gameData, polys, selection);
        break;
    case 23:
        EnterPassword(gameData, selection, polys);
        break;
    case 14:
        DisplayPassword(gameData);
        break;
    case 17:
        DispHiScores(gameData, selection, 5);
        break;
    case 18:
        DispHiScores(gameData, selection, 6);
        break;
    case 19:
        PortLinkScreen(gameData, selection); /* Port: the lobby (portlink.c); the console's cable link was EstablishLink() */
        break;
    }
}


void AddTextBits(ConfigData *gameData)
{
    char menuLabel[32];
    char formattedTrack[64];

    if ((linked != 0) && ((menuTitleBlink % 30) >= 0xF)) {
        Print(msgLinkedUp, 0xFF, 0x4B, 7);
    }
    menuTitleBlink += 1;
    switch (currMenu) {
    case 0:
        if ((u32) ownShip < 3U) {
            __builtin_memcpy(menuLabel, teamNameAgSystems, 11);
        } else if ((u32) (ownShip - 3) < 3U) {
            __builtin_memcpy(menuLabel, teamNameAuricom, 8);
        } else if ((u32) (ownShip - 6) < 3U) {
            __builtin_memcpy(menuLabel, teamNameQirex, 6);
        } else if ((u32) (ownShip - 9) < 3U) {
            __builtin_memcpy(menuLabel, teamNameFeisar, 7);
        } else if ((u32) (ownShip - 0xC) < 3U) {
            __builtin_memcpy(menuLabel, teamNamePiranha, 8);
        }
        PrintCX(menuLabel, 0xA0, 0xAC);
        switch (raceType) {
        case 0:
            __builtin_memcpy(menuLabel, msgArcade, 7);
            break;
        case 1:
            __builtin_memcpy(menuLabel, msgTimeTrial, 11);
            break;
        case 3:
            __builtin_memcpy(menuLabel, msgOneOnOne, 11);
            break;
        case 2:
            __builtin_memcpy(menuLabel, msgArcadeLink, 12);
            break;
        case 4:
            if (gameData->venomComplete != 0) {
                __builtin_memcpy(menuLabel, msgChallengeI, 12);
            } else if (gameData->rapierComplete != 0) {
                __builtin_memcpy(menuLabel, msgChallengeII, 13);
            }
            break;
        }
        PrintCX(menuLabel, 0x3F, 0xAC);
        strcpy(formattedTrack, PortRaceTrackName(gameData->actTrackNum)); /* Port: an original track's name */
        PrintCX(formattedTrack, 0x100, 0xAC);
        __builtin_memcpy(menuLabel, msgEssentialOptions, 18);
        break;
    case 2:
        __builtin_memcpy(menuLabel, msgRacingMode, 12);
        break;
    case 1:
        __builtin_memcpy(menuLabel, msgStructureMode, 15);
        break;
    case 3:
        __builtin_memcpy(menuLabel, msgOptionsMode, 13);
        break;
    case 5:
    case 6:
    case 7:
    case 8:
        __builtin_memcpy(menuLabel, msgTrackMode, 11);
        break;
    case 4:
        __builtin_memcpy(menuLabel, msgTeamMode, 10);
        break;
    case 9:
        __builtin_memcpy(menuLabel, msgPrecisionLevels, 17);
        break;
    case 10:
        __builtin_memcpy(menuLabel, msgInteractionMode, 17);
        break;
    case 20:
        __builtin_memcpy(menuLabel, msgNegconSelectionMode, 22);
        break;
    case 22:
        __builtin_memcpy(menuLabel, msgNegconMode, 12);
        break;
    case 21:
        __builtin_memcpy(menuLabel, msgNegconTwistMode, 18);
        break;
    case 14:
        __builtin_memcpy(menuLabel, msgPasswordMode, 14);
        break;
    case 23:
        __builtin_memcpy(menuLabel, msgProgressionMode, 17);
        break;
    case 12:
        __builtin_memcpy(menuLabel, msgDataMode, 10);
        break;
    case 15:
        __builtin_memcpy(menuLabel, msgPreferencesMode, 17);
        break;
    case 16:
        __builtin_memcpy(menuLabel, msgScreenMode, 12);
        break;
    case 13:
        __builtin_memcpy(menuLabel, msgTimesMode, 11);
        break;
    case 17:
        __builtin_memcpy(menuLabel, msgArcadeMode, 12);
        break;
    case 18:
        __builtin_memcpy(menuLabel, msgTimeTrialMode, 16);
        break;
    case 19:
        /* Port: the lobby says which of the two-player races it is for (the console's link screen said LINK UP twice) */
        if (raceType == 2) {
            __builtin_memcpy(menuLabel, msgArcadeLink, 12);
        } else {
            __builtin_memcpy(menuLabel, msgOneOnOne, 11);
        }
        break;

    default:
        break;
    }
    Print(menuLabel, 0x1E, 0x4B, 7);
}


void DisplayTexture(Texture *texture, s32 left, s32 top, POLY_FT4 *polys)
{
    POLY_FT4 *poly;
    s16 x, y;
    u16 height;
    s32 slot;

    slot = CurrentScreen;
    poly = &polys[slot];
    SetPolyFT4(poly);
    x = left;
    y = top;
    poly->r0 = 0x80;
    poly->g0 = 0x80;
    poly->b0 = 0x80;
    poly->x0 = x;
    poly->y0 = y;
    poly->x1 = x + texture->textureW * 4;
    poly->y1 = y;
    poly->x2 = x;
    height = texture->textureH;
    poly->y2 = y + height;
    poly->x3 = x + texture->textureW * 4;
    poly->y3 = y + texture->textureH;
    poly->u0 = texture->u0;
    poly->v0 = texture->v0;
    poly->u1 = texture->u1;
    poly->v1 = texture->v1;
    poly->u2 = texture->u2;
    poly->v2 = texture->v2 + 1;
    poly->u3 = texture->u3;
    poly->v3 = texture->v3 + 1;
    poly->tpage = GetTPage(0, 0, texture->textureX, texture->textureY);
    poly->clut = texture->cba;
    AddPrim(OT[slot], poly);
}


/* Draws the highlight bar over a box of the current menu. */
#define HighlightBox(index, line) \
    DrawHighLight(polys->options, (short)(menuLayout[currMenu].box[index].x0 + 1), (short)(menuLayout[currMenu].box[index].y0 + 1), \
                  (short)(menuLayout[currMenu].box[index].x1 - menuLayout[currMenu].box[index].x0 - 1), \
                  (short)(menuLayout[currMenu].box[index].y1 - menuLayout[currMenu].box[index].y0 - 1), line)

void ShowMenu(ConfigData *gameData, MenuBars *polys, MenuSelection *selection)
{
    ClassOrder classOrder;
    int gap;
    RaceOrder raceOrder;
    short i,j;
    unsigned char blocked;
    int index;
    classOrder=classOrderTable[0];
    raceOrder=raceOrderTable[0];
    blocked=0;
    for(i=0;i<menu[currMenu].numOptions;i++) {
        for(j=0;j<menu[currMenu].options[i].numText;j++) {
            if(currMenu==0) {
                CentreText2(menu[0].options[i].text[j].text,menu[0].options[i].x,menu[0].options[i].y+j*10,8,1);
            } else if(menu[currMenu].options[i].enabled) {
                if(currMenu==2) {
                    MenuOption *options=menu[2].options;
                    int row=(i==4 && !options[3].enabled)?3:i; /* Port: WIPEOUT in the fourth class's row when that is locked */
                    if(i==selection->current) CentreText2(options[i].text[j].text,68,menu[2].options[0].y+row*10,8,0);
                    else CentreText2(options[i].text[j].text,68,menu[2].options[0].y+row*10,8,1);
                } else if(currMenu==3) {
                    MenuOption *options=menu[3].options;
                    if(i==menu[3].selection) CentreText(options[i].text[j].text,68,menu[3].options[0].y+i*12,8,0);
                    else CentreText(options[i].text[j].text,68,menu[3].options[0].y+i*12,8,1);
                } else {
                    if(i==menu[currMenu].selection) AddText(menu[currMenu].options[i].text[j].text,menu[currMenu].options[0].x,menu[currMenu].options[0].y+i*10,8,0);
                    else AddText(menu[currMenu].options[i].text[j].text,menu[currMenu].options[0].x,menu[currMenu].options[0].y+i*10,8,1);
                }
            }
        }
        if(currMenu==0) {
            if(i==menu[currMenu].selection) {
                polys->options[i][CurrentScreen].r0=85;
                polys->options[i][CurrentScreen].g0=0;
                polys->options[i][CurrentScreen].b0=40;
            } else {
                polys->options[i][CurrentScreen].r0=0;
                polys->options[i][CurrentScreen].g0=0;
                polys->options[i][CurrentScreen].b0=46;
            }
            if((unsigned short)(i-3)<2) {
                if(i==menu[currMenu].selection) {
                    polys->options[i][CurrentScreen].r0=85;
                    polys->options[i][CurrentScreen].g0=0;
                    polys->options[i][CurrentScreen].b0=40;
                } else {
                    polys->options[i][CurrentScreen].r0=0;
                    polys->options[i][CurrentScreen].g0=0;
                    polys->options[i][CurrentScreen].b0=46;
                }
                HighlightBox(i, i);
            } else if(i==menu[currMenu].selection) {
                polys->options[i][CurrentScreen].r0=85;
                polys->options[i][CurrentScreen].g0=0;
                polys->options[i][CurrentScreen].b0=40;
                HighlightBox(i, i);
            }
        }
    }
    if(currMenu==9) {
        polys->options[0][CurrentScreen].r0=85;
        polys->options[0][CurrentScreen].g0=0;
        polys->options[0][CurrentScreen].b0=40;
        if(menuLayout[currMenu].box[1].active || menuLayout[currMenu].box[3].active) {
            if(selection->stereo==0) {
                HighlightBox(3, 0);
            } else {
                HighlightBox(1, 0);
            }
        }
    }
    if(currMenu==21) {
        polys->options[0][CurrentScreen].r0=85;
        polys->options[0][CurrentScreen].g0=0;
        polys->options[0][CurrentScreen].b0=40;
        if(menuLayout[currMenu].box[menu[currMenu].selection].active) {
            HighlightBox(menu[currMenu].selection, 0);
        }
    }
    if(currMenu!=19 && currMenu!=17 && currMenu!=18 && currMenu!=9 && currMenu!=10 && currMenu!=15 && currMenu!=23 && currMenu!=21 && currMenu!=22) DoButton(30,198,1,msgSelect2);
    if(currMenu==9 || currMenu==10 || currMenu==15 || currMenu==23 || currMenu==21 || currMenu==22) DoButton(30,198,1,msgAccept);
    if(currMenu!=0) DoButton(278,198,3,msgBack);
    if(currMenu==2) ClassSelect(gameData,selection);
    if(currMenu==1) {
        switch(menu[1].selection) {
        case 0: Print(msgOnePlayerSingleRaceAgainst,30,182,8);Print(msgGridOfCompetitors,30,189,8);break;
        case 1: Print(msgOnePlayerSingleRaceTimeTrial,30,182,8);Print(msgNoCompetitors,30,189,8);break;
        case 2: Print(msgTwoPlayerSingleRaceAgainst,30,182,8);Print(msgGridOfCompetitors,30,189,8);break;
        case 3: Print(msgTwoPlayerSingleRaceWithNo,30,182,8);Print(msgCompetitors,30,189,8);break;
        case 4:
            if(gameData->venomComplete) {Print(msgOnePlayerPhantomChallenge,30,182,8);Print(msgAtRapierClassWeaponsAreActivated,30,189,8);}
            else if(gameData->rapierComplete) {Print(msgOnePlayerPiranhaChallenge,30,182,8);Print(msgAtPhantomClassWeaponsAre,30,189,8);}
            break;
        }
    }
    if(!menuEditor) {
        for(i=0;i<menuLayout[currMenu].numBoxes;i++) {
            menuLayout[currMenu].box[i].edge[0][CurrentScreen].x0=menuLayout[currMenu].box[i].x0;
            menuLayout[currMenu].box[i].edge[0][CurrentScreen].y0=menuLayout[currMenu].box[i].y0;
            menuLayout[currMenu].box[i].edge[0][CurrentScreen].x1=menuLayout[currMenu].box[i].x1;
            menuLayout[currMenu].box[i].edge[0][CurrentScreen].y1=menuLayout[currMenu].box[i].y0;
            menuLayout[currMenu].box[i].edge[1][CurrentScreen].x0=menuLayout[currMenu].box[i].x1;
            menuLayout[currMenu].box[i].edge[1][CurrentScreen].y0=menuLayout[currMenu].box[i].y0;
            menuLayout[currMenu].box[i].edge[1][CurrentScreen].x1=menuLayout[currMenu].box[i].x1;
            menuLayout[currMenu].box[i].edge[1][CurrentScreen].y1=menuLayout[currMenu].box[i].y1;
            menuLayout[currMenu].box[i].edge[2][CurrentScreen].x0=menuLayout[currMenu].box[i].x1;
            menuLayout[currMenu].box[i].edge[2][CurrentScreen].y0=menuLayout[currMenu].box[i].y1;
            menuLayout[currMenu].box[i].edge[2][CurrentScreen].x1=menuLayout[currMenu].box[i].x0;
            menuLayout[currMenu].box[i].edge[2][CurrentScreen].y1=menuLayout[currMenu].box[i].y1;
            menuLayout[currMenu].box[i].edge[3][CurrentScreen].x0=menuLayout[currMenu].box[i].x0;
            menuLayout[currMenu].box[i].edge[3][CurrentScreen].y0=menuLayout[currMenu].box[i].y1;
            menuLayout[currMenu].box[i].edge[3][CurrentScreen].x1=menuLayout[currMenu].box[i].x0;
            menuLayout[currMenu].box[i].edge[3][CurrentScreen].y1=menuLayout[currMenu].box[i].y0;
            if((short)menuLayout[currMenu].box[i].x0!=160 && (short)menuLayout[currMenu].box[i].y0!=120 && (short)menuLayout[currMenu].box[i].x1!=160 && (short)menuLayout[currMenu].box[i].y1!=120) {
                for(j=0;j<4;j++) {
                    menuLayout[currMenu].box[i].edge[j][CurrentScreen].r0=255;
                    menuLayout[currMenu].box[i].edge[j][CurrentScreen].g0=255;
                    menuLayout[currMenu].box[i].edge[j][CurrentScreen].b0=255;
                    AddPrim(OT[CurrentScreen],&menuLayout[currMenu].box[i].edge[j][CurrentScreen]);
                }
            }
        }
    }
    if(currMenu==3) DrawTeamShips(0,-55,600,heading,0,menuModels[7]);
    if(currMenu==0) {
        DrawTeamShips(0,-120,1800,cardNum,173,shipShapeTable[highResTable[ownShip]]);
        switch(raceType) {
        case 1: DrawTeamShips(-670,-90,1200,cardNum,0,menuModels[raceOrder.order[1]]);break;
        case 3: DrawTeamShips(-670,-60,1200,cardNum,0,menuModels[raceOrder.order[3]]);break;
        case 2: DrawTeamShips(-550,-40,1000,cardNum,0,menuModels[raceOrder.order[2]]);break;
        case 4:
            if(gameData->venomComplete) DrawTeamShips(-670,-60,1200,cardNum,0,menuModels[raceOrder.order[4]]);
            else DrawTeamShips(-720,-60,1300,cardNum,0,menuModels[raceOrder.order[5]]);
            break;
        default: DrawTeamShips(-570,-40,1000,cardNum,0,menuModels[raceOrder.order[raceType]]);break;
        }
        DrawTeamShips(730,-60,1200,cardNum,0,menuModels[classOrder.order[gameData->shipClass]]);
    } else if(currMenu==1) {
        MenuEntry *menus=menu;
        if(menus[1].selection==4 && !gameData->venomComplete) {
            DrawTeamShips(440,-100,1000,cardNum,0,menuModels[raceOrder.order[menus[1].selection+1]]);
        } else {
            DrawTeamShips(440,-100,1000,cardNum,0,menuModels[raceOrder.order[menus[1].selection]]);
        }
    }
    if(currMenu==0) DisplayTexture(textureTable[6 + menuTextures],25,38,polys->title);
    else AddText(menu[currMenu].title.text,menu[currMenu].title.size,menu[currMenu].title.colour,12,1);
    AddTextBits(gameData);
    if(menuEditor) return;
    if(currMenu==0 && menu[currMenu].numOptions>0) {
        if(GetMenuKey(3)) {
            menu[currMenu].selection++;
            if(menu[currMenu].options[menu[currMenu].selection].enabled==0) menu[currMenu].selection++;
            if(menu[currMenu].selection>=menu[currMenu].numOptions) menu[currMenu].selection=menu[currMenu].numOptions-1;
            else PlayNote(8,255,15,15,256);
        }
        if(GetMenuKey(2)) {
            menu[currMenu].selection--;
            if(menu[currMenu].selection<0) {
                menu[currMenu].selection=0;
                if(!menu[currMenu].options[0].enabled) menu[currMenu].selection=-1;
            } else PlayNote(8,255,15,15,256);
        }
        if(currMenu!=0) return;
        if(GetMenuKey(0)) {
            if(menu[currMenu].selection>=menu[currMenu].numOptions-2) {
                menu[currMenu].selection--;
                if(menu[currMenu].selection==2) menu[currMenu].selection=lastTopOption;
                PlayNote(8,255,15,15,256);
            }
        }
        if(GetMenuKey(1)) {
            if((unsigned short)menu[currMenu].selection<3) {
                lastTopOption=menu[currMenu].selection;
                menu[currMenu].selection=3;
                PlayNote(8,255,15,15,256);
            } else {
                menu[currMenu].selection++;
                if(menu[currMenu].selection>=menu[currMenu].numOptions) menu[currMenu].selection=4;
                else PlayNote(8,255,15,15,256);
            }
        }
    } else if(menu[currMenu].numOptions>0) {
        if(GetMenuKey(1)) {
            short oldSelection=menu[currMenu].selection;
            menu[currMenu].selection++;
            if(!menu[currMenu].options[menu[currMenu].selection].enabled) {
                menu[currMenu].selection=oldSelection;
                blocked=1;
            }
            if(menu[currMenu].selection>=menu[currMenu].numOptions) menu[currMenu].selection=menu[currMenu].numOptions-1;
            else if(!blocked) PlayNote(8,255,15,15,256);
        }
        if(GetMenuKey(0)) {
            menu[currMenu].selection--;
            if(menu[currMenu].selection<0) menu[currMenu].selection=0;
            else PlayNote(8,255,15,15,256);
        }
    }
}

#undef HighlightBox


void LoadMenuLineData(void)
{
    char filePath[264];
    int fileLength;
    void *fileBuffer;

    fileLength = FileLength(pathMenuDat);
    if (fileLength <= 0) {
        printf("%s", errLoadMenuLineDataFileHasZero);
        return;
    }
    fileBuffer = DAlloc(reserveHeap, fileLength + 0x20);
    if (fileBuffer == 0) {
        sprintf(errorString, "%s", errLoadMenuLineDataFailedTo);
        Error(errorString, 2);
    }
    if (LoadFile(pathMenuDat, fileBuffer) != fileLength) {
        sprintf(errorString, "%s", errLoadMenuLineDataFailedTo2);
        Error(errorString, 2);
    }
    *(MenuFile *)menuLayout = *(MenuFile *)fileBuffer;
    DFree(reserveHeap, fileBuffer);
}


void InitLineEditor(void) {
}



/* Fixed-size copies of label text; the A types are halfword aligned. */
typedef union Chars4A { char bytes[4]; short align; } Chars4A;
typedef struct Chars5 { char bytes[5]; } Chars5;
typedef union Chars6A { char bytes[6]; short align; } Chars6A;
typedef union Chars7A { char bytes[7]; short align; } Chars7A;
typedef struct Chars8 { char bytes[8]; } Chars8;
typedef struct Chars9 { char bytes[9]; } Chars9;
typedef struct Chars10 { char bytes[10]; } Chars10;
typedef struct Chars11 { char bytes[11]; } Chars11;
typedef union Chars11A { char bytes[11]; short align; } Chars11A;
typedef struct Chars12 { char bytes[12]; } Chars12;
typedef struct Chars13 { char bytes[13]; } Chars13;
typedef struct Chars14 { char bytes[14]; } Chars14;
typedef union Chars14A { char bytes[14]; short align; } Chars14A;
typedef union Chars18A { char bytes[18]; short align; } Chars18A;
typedef union Chars22A { char bytes[22]; short align; } Chars22A;

extern Chars13 msgRacingClass;
extern Chars10 msgRaceType;
extern Chars13 msgAudioConfig;
extern Chars18A msgControllerConfig;
extern Chars12 msgPreferences;
extern Chars14A msgLoadAndSave;
extern Chars9 msgPassword;
extern Chars18A msgBestArcadeTimes;
extern Chars22A msgBestTimeTrialTimes;
extern Chars13 msgOptionsMenu;
extern Chars13 msgVectorClass;
extern Chars12 msgVenomClass;
extern Chars13 msgRapierClass;
extern Chars14 msgPhantomClass;
extern Chars10 msgTeamMenu;
extern Chars11 msgController;
extern Chars12 msgGameConfig;
extern Chars11 msgBestTimes;
extern Chars14A msgButtonConfig;
extern Chars13 msgTwistConfig;
extern Chars14 msgNegconConfig;
extern Chars5 msgType;
extern Chars6A msgClass;
extern Chars4A msgAnd;
extern Chars6A msgTrack;
extern Chars5 msgTeam;
extern Chars8 msgOptions;
extern Chars6A msgStart;
extern Chars8 msgWipeout;

void InitMenus(void)
{
    __builtin_memcpy(menu[0].options[0].text[0].text, hiScoreLabelTime, 5);
    menu[0].options[0].text[0].size = 8;
    menu[0].options[0].text[0].colour = 1;
    __builtin_memcpy(menu[0].options[0].text[1].text, &msgType, 5);
    menu[0].options[0].text[1].size = 8;
    menu[0].options[0].text[1].colour = 1;
    menu[0].options[0].x = 63;
    menu[0].options[0].y = 87;
    menu[0].options[0].numText = 2;
    menu[0].options[0].font = 9;
    menu[0].options[0].nextMenu = 1;
    menu[0].options[0].selection = 0;
    menu[0].options[0].enabled = 1;
    menu[0].options[0].flags = 0;
    __builtin_memcpy(menu[0].options[2].text[0].text, &msgClass, 6);
    menu[0].options[2].text[0].size = 8;
    menu[0].options[2].text[0].colour = 1;
    __builtin_memcpy(menu[0].options[2].text[1].text, &msgAnd, 4);
    menu[0].options[2].text[1].size = 8;
    menu[0].options[2].text[1].colour = 1;
    __builtin_memcpy(menu[0].options[2].text[2].text, &msgTrack, 6);
    menu[0].options[2].text[2].size = 8;
    menu[0].options[2].text[2].colour = 1;
    menu[0].options[2].x = 255;
    menu[0].options[2].y = 87;
    menu[0].options[2].numText = 3;
    menu[0].options[2].font = 9;
    menu[0].options[2].nextMenu = 2;
    menu[0].options[2].selection = 0;
    menu[0].options[2].enabled = 1;
    menu[0].options[2].flags = 0;
    __builtin_memcpy(menu[0].options[1].text[0].text, &msgTeam, 5);
    menu[0].options[1].text[0].size = 8;
    menu[0].options[1].text[0].colour = 1;
    menu[0].options[1].x = 161;
    menu[0].options[1].y = 87;
    menu[0].options[1].numText = 1;
    menu[0].options[1].font = 9;
    menu[0].options[1].nextMenu = 4;
    menu[0].options[1].selection = 0;
    menu[0].options[1].enabled = 1;
    menu[0].options[1].flags = 0;
    __builtin_memcpy(menu[0].options[4].text[0].text, &msgOptions, 8);
    menu[0].options[4].text[0].size = 8;
    menu[0].options[4].text[0].colour = 1;
    menu[0].options[4].x = 162;
    menu[0].options[4].y = 203;
    menu[0].options[4].numText = 1;
    menu[0].options[4].font = 9;
    menu[0].options[4].nextMenu = 3;
    menu[0].options[4].selection = 0;
    menu[0].options[4].enabled = 1;
    menu[0].options[4].flags = 0;
    __builtin_memcpy(menu[0].options[3].text[0].text, &msgStart, 6);
    menu[0].options[3].text[0].size = 8;
    menu[0].options[3].text[0].colour = 1;
    menu[0].options[3].x = 162;
    menu[0].options[3].y = 185;
    menu[0].options[3].numText = 1;
    menu[0].options[3].font = 9;
    menu[0].options[3].nextMenu = 1;
    menu[0].options[3].selection = 0;
    menu[0].options[3].enabled = 1;
    menu[0].options[3].flags = 0;
    __builtin_memcpy(menu[0].title.text, &msgWipeout, 8);
    menu[0].title.size = 40;
    menu[0].title.colour = 50;
    menu[0].numOptions = 5;
    menu[0].id = 0;
    menu[0].flags = 1;
    menu[0].prevMenu = 999;
    menu[0].selection = 0;
    menu[0].mode = 0;
    menu[0].updateCount = 0;
    __builtin_memcpy(menu[2].options[0].text[0].text, hiScoreRank0, 7);
    menu[2].options[0].text[0].size = 8;
    menu[2].options[0].text[0].colour = 1;
    menu[2].options[0].x = 30;
    menu[2].options[0].y = 90;
    menu[2].options[0].numText = 1;
    menu[2].options[0].font = 9;
    menu[2].options[0].nextMenu = 0;
    menu[2].options[0].selection = 0;
    menu[2].options[0].enabled = 1;
    menu[2].options[0].flags = 0;
    __builtin_memcpy(menu[2].options[1].text[0].text, hiScoreRank1, 6);
    menu[2].options[1].text[0].size = 8;
    menu[2].options[1].text[0].colour = 1;
    menu[2].options[1].x = 128;
    menu[2].options[1].y = 64;
    menu[2].options[1].numText = 1;
    menu[2].options[1].font = 9;
    menu[2].options[1].nextMenu = 0;
    menu[2].options[1].selection = 0;
    menu[2].options[1].enabled = 1;
    menu[2].options[1].flags = 0;
    __builtin_memcpy(menu[2].options[2].text[0].text, hiScoreRank2, 7);
    menu[2].options[2].text[0].size = 8;
    menu[2].options[2].text[0].colour = 1;
    menu[2].options[2].x = 205;
    menu[2].options[2].y = 64;
    menu[2].options[2].numText = 1;
    menu[2].options[2].font = 9;
    menu[2].options[2].nextMenu = 0;
    menu[2].options[2].selection = 0;
    menu[2].options[2].enabled = 1;
    menu[2].options[2].flags = 0;
    __builtin_memcpy(menu[2].options[3].text[0].text, hiScoreRank3, 8);
    menu[2].options[3].text[0].size = 8;
    menu[2].options[3].text[0].colour = 1;
    menu[2].options[3].x = 281;
    menu[2].options[3].y = 64;
    menu[2].options[3].numText = 1;
    menu[2].options[3].font = 9;
    menu[2].options[3].nextMenu = 0;
    menu[2].options[3].selection = 0;
    menu[2].options[3].enabled = 0;
    menu[2].options[3].flags = 0;
    /* Port: the original Wipeout's tracks (portorig.c), shown when the player's copy has been found */
    __builtin_memcpy(menu[2].options[4].text[0].text, "WIPEOUT", 8);
    menu[2].options[4].text[0].size = 8;
    menu[2].options[4].text[0].colour = 1;
    menu[2].options[4].x = menu[2].options[3].x;
    menu[2].options[4].y = 64;
    menu[2].options[4].numText = 1;
    menu[2].options[4].font = 9;
    menu[2].options[4].nextMenu = 0;
    menu[2].options[4].selection = 0;
    menu[2].options[4].enabled = 0;
    menu[2].options[4].flags = 0;
    __builtin_memcpy(menu[2].title.text, &msgRacingClass, 13);
    menu[2].title.size = 40;
    menu[2].title.colour = 50;
    menu[2].id = 0;
    menu[2].flags = 0;
    menu[2].numOptions = 5; /* Port: and WIPEOUT */
    menu[2].prevMenu = 0;
    menu[2].selection = 0;
    menu[2].mode = 0;
    menu[2].updateCount = 0;
    __builtin_memcpy(menu[1].options[0].text[0].text, msgArcade, 7);
    menu[1].options[0].text[0].size = 8;
    menu[1].options[0].text[0].colour = 1;
    menu[1].options[0].x = 30;
    menu[1].options[0].y = 90;
    menu[1].options[0].numText = 1;
    menu[1].options[0].font = 9;
    menu[1].options[0].nextMenu = 0;
    menu[1].options[0].selection = 0;
    menu[1].options[0].enabled = 1;
    menu[1].options[0].flags = 0;
    __builtin_memcpy(menu[1].options[1].text[0].text, (Chars11A *)msgTimeTrial, 11);
    menu[1].options[1].text[0].size = 8;
    menu[1].options[1].text[0].colour = 1;
    menu[1].options[1].x = 128;
    menu[1].options[1].y = 64;
    menu[1].options[1].numText = 1;
    menu[1].options[1].font = 9;
    menu[1].options[1].nextMenu = 0;
    menu[1].options[1].selection = 0;
    menu[1].options[1].enabled = 1;
    menu[1].options[1].flags = 0;
    __builtin_memcpy(menu[1].options[2].text[0].text, (Chars12 *)msgArcadeLink, 12);
    menu[1].options[2].text[0].size = 8;
    menu[1].options[2].text[0].colour = 1;
    menu[1].options[2].x = 205;
    menu[1].options[2].y = 64;
    menu[1].options[2].numText = 1;
    menu[1].options[2].font = 9;
    menu[1].options[2].nextMenu = 19;
    menu[1].options[2].selection = 0;
    menu[1].options[2].enabled = 1;
    menu[1].options[2].flags = 0;
    __builtin_memcpy(menu[1].options[3].text[0].text, (Chars11A *)msgOneOnOne, 11);
    menu[1].options[3].text[0].size = 8;
    menu[1].options[3].text[0].colour = 1;
    menu[1].options[3].x = 281;
    menu[1].options[3].y = 64;
    menu[1].options[3].numText = 1;
    menu[1].options[3].font = 9;
    menu[1].options[3].nextMenu = 19;
    menu[1].options[3].selection = 0;
    menu[1].options[3].enabled = 1;
    menu[1].options[3].flags = 0;
    __builtin_memcpy(menu[1].options[4].text[0].text, (Chars12 *)msgChallengeI, 12);
    menu[1].options[4].text[0].size = 8;
    menu[1].options[4].text[0].colour = 1;
    menu[1].options[4].x = 167;
    menu[1].options[4].y = 144;
    menu[1].options[4].numText = 1;
    menu[1].options[4].font = 9;
    menu[1].options[4].nextMenu = 0;
    menu[1].options[4].selection = 0;
    menu[1].options[4].enabled = 1;
    menu[1].options[4].flags = 0;
    __builtin_memcpy(menu[1].title.text, &msgRaceType, 10);
    menu[1].title.size = 40;
    menu[1].title.colour = 50;
    menu[1].id = 0;
    menu[1].flags = 0;
    menu[1].numOptions = 4;
    menu[1].prevMenu = 0;
    menu[1].selection = 0;
    menu[1].mode = 0;
    menu[1].updateCount = 0;
    *(Chars13 *)(menu[3].options[0].text[0].text)=msgAudioConfig;

    menu[3].options[0].text[0].size = 8;
    menu[3].options[0].text[0].colour = 1;
    menu[3].options[0].x = 30;
    menu[3].options[0].y = 90;
    menu[3].options[0].numText = 1;
    menu[3].options[0].font = 9;
    menu[3].options[0].nextMenu = 9;
    menu[3].options[0].selection = 0;
    menu[3].options[0].enabled = 1;
    menu[3].options[0].flags = 0;
    __builtin_memcpy(menu[3].options[1].text[0].text, &msgControllerConfig, 18);
    menu[3].options[1].text[0].size = 8;
    menu[3].options[1].text[0].colour = 1;
    menu[3].options[1].x = 128;
    menu[3].options[1].y = 64;
    menu[3].options[1].numText = 1;
    menu[3].options[1].font = 9;
    menu[3].options[1].nextMenu = 10;
    menu[3].options[1].selection = 0;
    menu[3].options[1].enabled = 1;
    menu[3].options[1].flags = 0;
    __builtin_memcpy(menu[3].options[2].text[0].text, &msgPreferences, 12);
    menu[3].options[2].text[0].size = 8;
    menu[3].options[2].text[0].colour = 1;
    menu[3].options[2].x = 205;
    menu[3].options[2].y = 64;
    menu[3].options[2].numText = 1;
    menu[3].options[2].font = 9;
    menu[3].options[2].nextMenu = 15;
    menu[3].options[2].selection = 0;
    menu[3].options[2].enabled = 1;
    menu[3].options[2].flags = 0;
    __builtin_memcpy(menu[3].options[3].text[0].text, &msgLoadAndSave, 14);
    menu[3].options[3].text[0].size = 8;
    menu[3].options[3].text[0].colour = 1;
    menu[3].options[3].x = 281;
    menu[3].options[3].y = 64;
    menu[3].options[3].numText = 1;
    menu[3].options[3].font = 9;
    menu[3].options[3].nextMenu = 12;
    menu[3].options[3].selection = 0;
    menu[3].options[3].enabled = 1;
    menu[3].options[3].flags = 0;
    __builtin_memcpy(menu[3].options[4].text[0].text, &msgPassword, 9);
    menu[3].options[4].text[0].size = 8;
    menu[3].options[4].text[0].colour = 1;
    menu[3].options[4].x = 205;
    menu[3].options[4].y = 144;
    menu[3].options[4].numText = 1;
    menu[3].options[4].font = 9;
    menu[3].options[4].nextMenu = 23;
    menu[3].options[4].selection = 0;
    menu[3].options[4].enabled = 1;
    menu[3].options[4].flags = 0;
    __builtin_memcpy(menu[3].options[5].text[0].text, &msgBestArcadeTimes, 18);
    menu[3].options[5].text[0].size = 8;
    menu[3].options[5].text[0].colour = 1;
    menu[3].options[5].x = 129;
    menu[3].options[5].y = 144;
    menu[3].options[5].numText = 1;
    menu[3].options[5].font = 9;
    menu[3].options[5].nextMenu = 17;
    menu[3].options[5].selection = 0;
    menu[3].options[5].enabled = 1;
    menu[3].options[5].flags = 0;
    __builtin_memcpy(menu[3].options[6].text[0].text, &msgBestTimeTrialTimes, 22);
    menu[3].options[6].text[0].size = 8;
    menu[3].options[6].text[0].colour = 1;
    menu[3].options[6].x = 129;
    menu[3].options[6].y = 144;
    menu[3].options[6].numText = 1;
    menu[3].options[6].font = 9;
    menu[3].options[6].nextMenu = 18;
    menu[3].options[6].selection = 0;
    menu[3].options[6].enabled = 1;
    menu[3].options[6].flags = 0;
    __builtin_memcpy(menu[3].title.text, &msgOptionsMenu, 13);
    menu[3].title.size = 40;
    menu[3].title.colour = 50;
    menu[3].id = 0;
    menu[3].flags = 0;
    menu[3].numOptions = 7;
    menu[3].prevMenu = 0;
    menu[3].selection = 0;
    menu[3].mode = 0;
    menu[3].updateCount = 0;
    __builtin_memcpy(menu[5].title.text, &msgVectorClass, 13);
    menu[5].title.size = 40;
    menu[5].title.colour = 50;
    menu[5].id = 0;
    menu[5].flags = 0;
    menu[5].numOptions = 0;
    menu[5].prevMenu = 2;
    menu[5].nextMenu = 0;
    menu[5].selection = 0;
    menu[5].mode = 0;
    menu[5].updateCount = 0;
    __builtin_memcpy(menu[6].title.text, &msgVenomClass, 12);
    menu[6].title.size = 40;
    menu[6].title.colour = 50;
    menu[6].id = 0;
    menu[6].flags = 0;
    menu[6].numOptions = 0;
    menu[6].prevMenu = 2;
    menu[6].nextMenu = 0;
    menu[6].selection = 0;
    menu[6].mode = 0;
    menu[6].updateCount = 0;
    __builtin_memcpy(menu[7].title.text, &msgRapierClass, 13);
    menu[7].title.size = 40;
    menu[7].title.colour = 50;
    menu[7].id = 0;
    menu[7].flags = 0;
    menu[7].numOptions = 0;
    menu[7].prevMenu = 2;
    menu[7].nextMenu = 0;
    menu[7].selection = 0;
    menu[7].mode = 0;
    menu[7].updateCount = 0;
    __builtin_memcpy(menu[8].title.text, &msgPhantomClass, 14);
    menu[8].title.size = 40;
    menu[8].title.colour = 50;
    menu[8].id = 0;
    menu[8].flags = 0;
    menu[8].numOptions = 0;
    menu[8].prevMenu = 2;
    menu[8].nextMenu = 0;
    menu[8].selection = 0;
    menu[8].mode = 0;
    menu[8].updateCount = 0;
    __builtin_memcpy(menu[4].title.text, &msgTeamMenu, 10);
    menu[4].title.size = 40;
    menu[4].title.colour = 50;
    menu[4].id = 0;
    menu[4].flags = 0;
    menu[4].numOptions = 0;
    menu[4].prevMenu = 0;
    menu[4].nextMenu = 0;
    menu[4].selection = 0;
    menu[4].mode = 0;
    menu[4].updateCount = 0;
    *(Chars13 *)(menu[9].title.text)=msgAudioConfig;
    menu[9].title.size = 40;
    menu[9].title.colour = 50;
    menu[9].id = 0;
    menu[9].flags = 0;
    menu[9].numOptions = 0;
    menu[9].prevMenu = 3;
    menu[9].nextMenu = 3;
    menu[9].selection = 0;
    menu[9].mode = 0;
    menu[9].updateCount = 0;
    __builtin_memcpy(menu[10].title.text, &msgController, 11);
    menu[10].title.size = 40;
    menu[10].title.colour = 50;
    menu[10].id = 0;
    menu[10].flags = 0;
    menu[10].numOptions = 0;
    menu[10].prevMenu = 3;
    menu[10].nextMenu = 3;
    menu[10].selection = 0;
    menu[10].mode = 0;
    menu[10].updateCount = 0;
    *(Chars14 *)(menu[12].title.text) = *(Chars14 *)&msgLoadAndSave;
    menu[12].title.size = 40;
    menu[12].title.colour = 50;
    menu[12].id = 0;
    menu[12].flags = 0;
    menu[12].numOptions = 0;
    menu[12].prevMenu = 3;
    menu[12].nextMenu = 3;
    menu[12].selection = 0;
    menu[12].mode = 0;
    menu[12].updateCount = 0;
    __builtin_memcpy(menu[15].title.text, &msgGameConfig, 12);
    menu[15].title.size = 40;
    menu[15].title.colour = 50;
    menu[15].id = 0;
    menu[15].flags = 0;
    menu[15].numOptions = 0;
    menu[15].prevMenu = 3;
    menu[15].nextMenu = 3;
    menu[15].selection = 0;
    menu[15].mode = 0;
    menu[15].updateCount = 0;
    __builtin_memcpy(menu[17].title.text, &msgBestTimes, 11);
    menu[17].title.size = 40;
    menu[17].title.colour = 50;
    menu[17].id = 0;
    menu[17].flags = 0;
    menu[17].numOptions = 0;
    menu[17].prevMenu = 3;
    menu[17].nextMenu = 3;
    menu[17].selection = 0;
    menu[17].mode = 0;
    menu[17].updateCount = 0;
    __builtin_memcpy(menu[18].title.text, &msgBestTimes, 11);
    menu[18].title.size = 40;
    menu[18].title.colour = 50;
    menu[18].id = 0;
    menu[18].flags = 0;
    menu[18].numOptions = 0;
    menu[18].prevMenu = 3;
    menu[18].nextMenu = 3;
    menu[18].selection = 0;
    menu[18].mode = 0;
    menu[18].updateCount = 0;
    __builtin_memcpy(menu[19].title.text, msgLink, 8);
    menu[19].title.size = 40;
    menu[19].title.colour = 50;
    menu[19].id = 0;
    menu[19].flags = 0;
    menu[19].numOptions = 0;
    menu[19].prevMenu = 0;
    menu[19].nextMenu = 0;
    menu[19].selection = 0;
    menu[19].updateCount = 0;
    __builtin_memcpy(menu[20].options[0].text[0].text, &msgButtonConfig, 14);
    menu[20].options[0].text[0].size = 8;
    menu[20].options[0].text[0].colour = 1;
    menu[20].options[0].x = 30;
    menu[20].options[0].y = 90;
    menu[20].options[0].numText = 1;
    menu[20].options[0].font = 9;
    menu[20].options[0].nextMenu = 22;
    menu[20].options[0].selection = 0;
    menu[20].options[0].enabled = 1;
    menu[20].options[0].flags = 0;
    __builtin_memcpy(menu[20].options[1].text[0].text, &msgTwistConfig, 13);
    menu[20].options[1].text[0].size = 8;
    menu[20].options[1].text[0].colour = 1;
    menu[20].options[1].x = 205;
    menu[20].options[1].y = 64;
    menu[20].options[1].numText = 1;
    menu[20].options[1].font = 9;
    menu[20].options[1].nextMenu = 21;
    menu[20].options[1].selection = 0;
    menu[20].options[1].enabled = 1;
    menu[20].options[1].flags = 0;
    __builtin_memcpy(menu[20].title.text, &msgNegconConfig, 14);
    menu[20].title.size = 40;
    menu[20].title.colour = 50;
    menu[20].id = 0;
    menu[20].flags = 0;
    menu[20].numOptions = 2;
    menu[20].prevMenu = 3;
    menu[20].selection = 0;
    menu[20].mode = 0;
    menu[20].updateCount = 0;
    __builtin_memcpy(menu[21].title.text, &msgTwistConfig, 13);
    menu[21].title.size = 40;
    menu[21].title.colour = 50;
    menu[21].id = 0;
    menu[21].flags = 0;
    menu[21].numOptions = 0;
    menu[21].prevMenu = 20;
    menu[21].nextMenu = 20;
    menu[21].selection = 0;
    menu[21].mode = 0;
    menu[21].updateCount = 0;
    __builtin_memcpy(menu[22].title.text, &msgController, 11);
    menu[22].title.size = 40;
    menu[22].title.colour = 50;
    menu[22].id = 0;
    menu[22].flags = 0;
    menu[22].numOptions = 0;
    menu[22].prevMenu = 20;
    menu[22].nextMenu = 20;
    menu[22].selection = 0;
    menu[22].mode = 0;
    menu[22].updateCount = 0;
    __builtin_memcpy(menu[23].title.text, &msgPassword, 9);
    menu[23].title.size = 40;
    menu[23].title.colour = 50;
    menu[23].id = 0;
    menu[23].flags = 0;
    menu[23].numOptions = 0;
    menu[23].prevMenu = 3;
    menu[23].nextMenu = 3;
    menu[23].selection = 0;
    menu[23].mode = 0;
    menu[23].updateCount = 0;
}


void InitMenuBox(MenuBars *bars)
{
    s16 i;
    s16 j;

    for (i = 0; i < 8; i++) {
        for (j = 0; j < 2; j++) {
            SetPolyF4(&bars->options[i][j]);
            bars->options[i][j].r0 = 0;
            bars->options[i][j].g0 = 0;
            bars->options[i][j].b0 = 0x2E;
        }
    }
}


/* Port: the byte offset into the polygons is added to uintptr_t, not int. */
void DrawHighLight(POLY_F4 (*prim)[2], int x, int y, int width, int height, short lineNum)
{
    POLY_F4 *quad;
    int screenOffset;
    int lineOffset;
    short left;
    short top;
    int right;
    int bottom;

    left = x;
    top = y;
    right = width + x;
    bottom = height + y;
    screenOffset = CurrentScreen * 0x18;
    lineOffset = lineNum * 0x30;
    quad = (POLY_F4 *)(screenOffset + (lineOffset + (uintptr_t)prim));
    quad->x0 = left;
    quad->y0 = top;
    quad->x1 = right;
    quad->y1 = top;
    quad->x2 = left;
    quad->y2 = bottom;
    quad->x3 = right;
    quad->y3 = bottom;
    AddPrim(&OT[CurrentScreen][4095], quad);
}


void EnterName(ConfigData *gameData, HiScoreData *data)
{
    short textY = 150;
    int textSize = 8;
    u_short input;
    short character;
    int select;

    if (data->numChars != 4 && data->userEntry != 0) {
        data->dispChar[1] = 0;
        data->dispChar[0] = data->currChar;
        if (data->userEntry != 0) {
            if (data->toggleDisplay % 3 == 0) {
                if (data->charVal == 26 || data->charVal == 27) {
                    AddDelEnd((unsigned char)(data->charVal - 26), data->charX, 150, 8, 1);
                } else {
                    AddText(data->dispChar, data->charX, 150, 8, 1);
                }
            } else {
                if (data->charVal == 26 || data->charVal == 27) {
                    AddDelEnd((unsigned char)(data->charVal - 26), data->charX, 150, 8, 0);
                } else {
                    AddText(data->dispChar, data->charX, 150, 8, 0);
                }
            }
        }
        character = data->currChar - 65;
        input = keysDown;
        data->charVal = character;
        if ((input & 0x20) && data->padDelay == 0) {
            data->padDelay = 5;
            data->charVal++;
            if (data->numChars != 3) {
                Sound(8);
            }
            if (data->charVal >= 27) {
                if (data->numChars != 3) {
                    data->charVal = 0;
                } else {
                    data->charVal = 26;
                }
            }
        }
        if (keysDown & 0x80) {
            if (data->padDelay == 0) {
                data->padDelay = 5;
                data->charVal--;
                if (data->numChars != 3) {
                    data->checkVal = 0;
                    Sound(8);
                } else {
                    data->checkVal = 26;
                }
                if (data->charVal < data->checkVal) {
                    data->charVal = 26;
                }
            }
        }
        if (data->padDelay > 0) {
            data->padDelay--;
        }
        select = 0;
        if (!(keysDown & 0xC01)) {
            select = keysPressed & 0x2000;
        }
        if (select != 0 && data->charVal == 26) {
            if (data->numChars > 0) {
                Sound(9);
                data->numChars--;
                character = data->nameEntered[data->numChars] - 65;
                data->charX -= fontGlyphs8[character].width;
                data->nameEntered[data->numChars] = 0;
                data->charVal = 0;
            }
        } else {
            data->currChar = data->charVal + 65;
            if (select != 0) {
                if (data->numChars != 4 && (u_short)data->charVal - 26 >= 2U) {
                    Sound(9);
                    data->nameEntered[data->numChars] = data->currChar;
                    data->nameEntered[data->numChars + 1] = 0;
                    data->numChars++;
                    data->charX += fontGlyphs8[data->charVal].width;
                    if (data->numChars == 3) {
                        data->charVal = 26;
                        data->currChar = 91;
                    }
                } else if (select != 0 && data->charVal == 27) {
                    Sound(9);
                    data->nameEntered[data->numChars + 1] = 0;
                    data->userEntry = 0;
                    keysDown = 0;
                }
            }
        }
    }
    if (data->userEntry != 0) {
        AddText(data->nameEntered, 180, textY, textSize, 1);
    }
    if (data->userEntry == 0) {
        AddText(data->nameEntered, 180, textY, textSize, 1);
    }
    data->toggleDisplay++;
}


char *GetTrackName(short type, short x, short y, short draw)
{
    switch (type) {
    case 1:
        if (draw) AddText(msgTalonSReach, x, y, 8, 1);
        else return msgTalonSReach;
        break;
    case 2:
        if (draw) AddText(msgGareDEuropa, x, y, 8, 1);
        else return msgGareDEuropa;
        break;
    case 6:
        if (draw) AddText(msgVostokIsland, x, y, 8, 1);
        else return msgVostokIsland;
        break;
    case 7:
        if (draw) AddText(msgSpilskinanke, x, y, 8, 1);
        else return msgSpilskinanke;
        break;
    case 8:
        if (draw) AddText(msgSagarmatha, x, y, 8, 1);
        else return msgSagarmatha;
        break;
    case 13:
        if (draw) AddText(msgValparaiso, x, y, 8, 1);
        else return msgValparaiso;
        break;
    case 17:
        if (draw) AddText(msgOdessaKeys, x, y, 8, 1);
        else return msgOdessaKeys;
        break;
    case 20:
        if (draw) AddText(msgPhenitiaPark, x, y, 8, 1);
        else return msgPhenitiaPark;
        break;
    }
    return 0; /* Port: the console fell off the end here (draw != 0, or a type outside the table) and returned a stale $v0; no caller uses it then */
}


void Print(u_char *text, int x, int y, int size)
{
    s32 glyphSize;
    s32 specialGlyph;
    s32 glyphRight;
    s32 glyphBottom;
    s16 glyphRow;
    s16 glyphCol;
    s32 code;
    s32 advance;
    s32 code2;
    s32 glyph;
    s32 ch;
    POLY_FT4 *quad;

    glyphSize = 6;
    specialGlyph = 8;


nextChar:
    ch = *text;
    code = ch & 0xFF;
    text += 1;
    if (code != 0x20) {
        if (code == 0) return;
        glyph = code - 0x41;
        if ((u32)(ch - 0x41) >= 26U) {
            if ((u32)(ch - 0x30) < 10U) glyph = code - 0x13;
            else if (code == 46) glyph = 26;
            else if (code == 44) glyph = 27;
            else if (code == 58) glyph = 28;
            else if (code == 91) glyph = 39;
            else if (code == 93) glyph = 40;
            else if (code == 47) glyph = 41;
            else if (code == 39) glyph = 42;
            else if (code == 63) glyph = 43;
            else {
                code2 = ch & 0xFF;
                if (code2 == 45) glyph = 44;
                else if (code2 == 43) glyph = 45;
                else if (code2 == 37) glyph = 46;
                else if (code2 == 33) glyph = 47;
            }
        }
        glyphRow = ((s32)glyph / 13) * glyphSize;
        glyphCol = ((s32)glyph % 13) * glyphSize;
        quad = &quadPool[quadIndex];
        if (glyph == specialGlyph) {
            x -= 2;
        }
        glyphRight = x + glyphSize;
        glyphBottom = y + glyphSize;
        quad->x0 = x;
        quad->y0 = y;
        quad->x1 = glyphRight;
        quad->y1 = y;
        quad->x2 = x;
        quad->y2 = glyphBottom;
        quad->x3 = glyphRight;
        quad->y3 = glyphBottom;
        quad->u0 = (s8) (fontTexture->u0 + glyphCol);
        quad->v0 = (s8) (fontTexture->v0 + glyphRow);
        quad->u1 = (s8) (fontTexture->u0 + glyphCol + glyphSize);
        quad->v1 = (s8) (fontTexture->v0 + glyphRow);
        quad->u2 = (s8) (fontTexture->u0 + glyphCol);
        quad->v2 = (s8) (fontTexture->v0 + glyphRow + glyphSize);
        quad->u3 = (s8) (fontTexture->u0 + glyphCol + glyphSize);
        quad->v3 = (s8) (fontTexture->v0 + glyphRow + glyphSize);
        quad->tpage = GetTPage(0, 0, fontTexture->textureX, fontTexture->textureY);
        quad->clut = (u16) fontTexture->cba;
        AddPrim(OT[CurrentScreen], quad);
        quadIndex += 1;
        if (glyph == specialGlyph) {
            advance = x - 2;
            x = advance + glyphSize;
        } else {
            x = glyphRight;
        }
        goto nextChar;
    } else {
        x += 3;
        goto nextChar;
    }
}


/* Port: the console's callers also passed a size that was never read; the quad address is
   built in uintptr_t, not int. */
void PrintCX(u_char *text, int x, int y)
{
    u8 *scan;
    s16 glyphRow;
    s16 glyphCol;
    s32 ch, code, code1, glyph;
    s32 glyphSize, specialGlyph, glyphRight, advance;
    s16 width;
    POLY_FT4 *quad;
    POLY_FT4 *pool;

    width = 0;
    scan = text;
    glyphSize = 6;
    while ((ch = *scan) != 0) {
        code1 = ch & 0xFF;
        if (code1 != 0x20 && code1 != 'I') width += glyphSize;
        else width += 3;
        scan++;
    }
    x -= width >> 1;
    specialGlyph = 8;
    pool = quadPool;
    for (;;) {
        ch = *text++;
        code = ch & 0xFF;
        if (code == 0x20) goto space;
        if (code == 0) return;
        glyph = code - 65;
        if ((u32)(ch - 48) < 10) glyph = code - 19;
        else if (code == '.') glyph = 26;
        else if (code == ',') glyph = 27;
        else if (code == ':') glyph = 28;
        else if (code == '[') glyph = 39;
        else if (code == ']') glyph = 40;
        else if (code == '/') glyph = 41;
        else if (code == '\'') glyph = 42;
        else if (code == '?') glyph = 43;
        else if ((ch & 255) == '-') glyph = 44;
        else if ((ch & 255) == '+') glyph = 45;
        else if ((ch & 255) == '%') glyph = 46;
        else if ((ch & 255) == '!') glyph = 47;
        glyphRow = glyph / 13 * glyphSize;
        glyphCol = glyph % 13 * glyphSize;
        if (glyph == specialGlyph) x -= 2;
        glyphRight = x + glyphSize;
        quad = (POLY_FT4 *)(quadIndex * sizeof(POLY_FT4) + (uintptr_t)pool);
        quad->x0 = x;
        quad->y0 = y;
        quad->x1 = glyphRight;
        quad->y1 = y;
        quad->x2 = x;
        quad->y2 = y + glyphSize;
        quad->x3 = glyphRight;
        quad->y3 = y + glyphSize;
        quad->u0 = fontTexture->u0 + glyphCol;
        pool[quadIndex].v0 = fontTexture->v0 + glyphRow;
        pool[quadIndex].u1 = fontTexture->u0 + glyphCol + glyphSize;
        pool[quadIndex].v1 = fontTexture->v0 + glyphRow;
        pool[quadIndex].u2 = fontTexture->u0 + glyphCol;
        pool[quadIndex].v2 = fontTexture->v0 + glyphRow + glyphSize;
        pool[quadIndex].u3 = fontTexture->u0 + glyphCol + glyphSize;
        pool[quadIndex].v3 = fontTexture->v0 + glyphRow + glyphSize;
        pool[quadIndex].tpage = GetTPage(0, 0, fontTexture->textureX, fontTexture->textureY);
        quadPool[quadIndex].clut = fontTexture->cba;
        AddPrim(OT[CurrentScreen], &pool[quadIndex]);
        quadIndex++;
        if (glyph == specialGlyph) {
            advance = x - 2;
            x = advance + glyphSize;
        } else x = glyphRight;
        continue;
space:
        x += 3;
    }
}


LoadedImage *Load16BitTexture(char *fileName, int loadBothBuffers)
{
    RECT rect;
    s32 fileLength;
    LoadedImage *imageInfo;
    u8 *imageData;
    TimImageBlock *timHeader;

    imageInfo = DAlloc(reserveHeap, 0x34);
    fileLength = FileLength(fileName);
    if (fileLength <= 0) {
        sprintf(errorString, "%s", errLoadTextureFileNotFound);
        Error(errorString, 0);
        return 0; /* Port: the console returned whatever Error() left in $v0 (see the port notes) */
    }
    imageData = DAlloc(reserveHeap, fileLength + 0x20);
    if (imageData == 0) {
        sprintf(errorString, "%s", errLoadTextureFailedToAllocate);
        Error(errorString, 2);
    }
    if (LoadFile(fileName, imageData) != fileLength) {
        sprintf(errorString, "%s", errLoadTextureFailedToLoadFile);
        Error(errorString, 2);
    }
    timHeader = (TimImageBlock *)imageData;
    imageInfo->x = timHeader->x;
    imageInfo->y = timHeader->y;
    imageInfo->width = timHeader->width;
    imageInfo->height = timHeader->height;
    imageData = imageData + 0x14;
    if (!(loadBothBuffers & 0xFF)) {
        rect.x = imageInfo->x;
        rect.y = imageInfo->y;
        rect.w = imageInfo->width;
        rect.h = imageInfo->height;
    } else {
        rect.w = imageInfo->width;
        rect.h = imageInfo->height;
        rect.x = 0;
        rect.y = 0;
        LoadImage(&rect, (u_long *)imageData);
        DrawSync(0);
        rect.y = 0x100;
    }
    LoadImage(&rect, (u_long *)imageData);
    DrawSync(0);
    DFree(reserveHeap, (char *)timHeader);
    return imageInfo;
}


void ShowMenuBackground(POLY_FT4 *menuPrim, POLY_FT4 *menu1Prim, LoadedImage *image)
{
    SetPolyFT4(&menuPrim[CurrentScreen]);
    menuPrim[CurrentScreen].code = 0x2D;
    menuPrim[CurrentScreen].x0 = 0;
    menuPrim[CurrentScreen].y0 = 0;
    menuPrim[CurrentScreen].x1 = (short)image->width >> 1;
    menuPrim[CurrentScreen].y1 = 0;
    menuPrim[CurrentScreen].x2 = 0;
    menuPrim[CurrentScreen].y2 = image->height;
    menuPrim[CurrentScreen].x3 = (short)image->width >> 1;
    menuPrim[CurrentScreen].y3 = image->height;
    menuPrim[CurrentScreen].u0 = 0;
    menuPrim[CurrentScreen].v0 = 0;
    menuPrim[CurrentScreen].u1 = image->width >> 1;
    menuPrim[CurrentScreen].v1 = 0;
    menuPrim[CurrentScreen].u2 = 0;
    menuPrim[CurrentScreen].v2 = image->height - 1;
    menuPrim[CurrentScreen].u3 = image->width >> 1;
    menuPrim[CurrentScreen].v3 = image->height - 1;
    menuPrim[CurrentScreen].tpage = GetTPage(2, 0, image->x, image->y);
    AddPrim(OT[CurrentScreen] + 4095, &menuPrim[CurrentScreen]);
    SetPolyFT4(&menu1Prim[CurrentScreen]);
    menu1Prim[CurrentScreen].code = 0x2D;
    menu1Prim[CurrentScreen].x0 = (short)image->width >> 1;
    menu1Prim[CurrentScreen].y0 = 0;
    menu1Prim[CurrentScreen].x1 = image->width;
    menu1Prim[CurrentScreen].y1 = 0;
    menu1Prim[CurrentScreen].x2 = (short)image->width >> 1;
    menu1Prim[CurrentScreen].y2 = image->height;
    menu1Prim[CurrentScreen].x3 = image->width;
    menu1Prim[CurrentScreen].y3 = image->height;
    menu1Prim[CurrentScreen].u0 = 32;
    menu1Prim[CurrentScreen].v0 = 0;
    menu1Prim[CurrentScreen].u1 = ((short)image->width >> 1) + 32;
    menu1Prim[CurrentScreen].v1 = 0;
    menu1Prim[CurrentScreen].u2 = 32;
    menu1Prim[CurrentScreen].v2 = image->height - 1;
    menu1Prim[CurrentScreen].u3 = ((short)image->width >> 1) + 32;
    menu1Prim[CurrentScreen].v3 = image->height - 1;
    menu1Prim[CurrentScreen].tpage = GetTPage(2, 0, image->x + 128, image->y);
    AddPrim(OT[CurrentScreen] + 4095, &menu1Prim[CurrentScreen]);
    PortWideBackdrop(&menuPrim[CurrentScreen]); /* Port: widescreen fills the sides with the backdrop */
    PortWideBackdrop(&menu1Prim[CurrentScreen]);
}


void ShowScreen(POLY_FT4 *menuPrim, POLY_FT4 *menu1Prim, LoadedImage *image, int x, int y)
{
    short left, top;

    SetPolyFT4(&menuPrim[CurrentScreen]);
    menuPrim[CurrentScreen].code = 0x2D;
    left = x;
    top = y;
    menuPrim[CurrentScreen].x0 = left;
    menuPrim[CurrentScreen].y0 = top;
    menuPrim[CurrentScreen].x1 = x + (short)image->width / 2;
    menuPrim[CurrentScreen].y1 = top;
    menuPrim[CurrentScreen].x2 = left;
    menuPrim[CurrentScreen].y2 = image->height + y;
    menuPrim[CurrentScreen].x3 = x + (short)image->width / 2;
    menuPrim[CurrentScreen].y3 = image->height + y;
    menuPrim[CurrentScreen].u0 = 0;
    menuPrim[CurrentScreen].v0 = 0;
    menuPrim[CurrentScreen].u1 = (short)image->width / 2;
    menuPrim[CurrentScreen].v1 = 0;
    menuPrim[CurrentScreen].u2 = 0;
    menuPrim[CurrentScreen].v2 = image->height - 1;
    menuPrim[CurrentScreen].u3 = (short)image->width / 2;
    menuPrim[CurrentScreen].v3 = image->height - 1;
    menuPrim[CurrentScreen].tpage = GetTPage(2, 0, image->x, image->y);
    AddPrim(OT[CurrentScreen], &menuPrim[CurrentScreen]);
    SetPolyFT4(&menu1Prim[CurrentScreen]);
    menu1Prim[CurrentScreen].code = 0x2D;
    menu1Prim[CurrentScreen].x0 = x + (short)image->width / 2;
    menu1Prim[CurrentScreen].y0 = top;
    menu1Prim[CurrentScreen].x1 = image->width + x;
    menu1Prim[CurrentScreen].y1 = top;
    menu1Prim[CurrentScreen].x2 = x + (short)image->width / 2;
    menu1Prim[CurrentScreen].y2 = image->height + y;
    menu1Prim[CurrentScreen].x3 = image->width + x;
    menu1Prim[CurrentScreen].y3 = image->height + y;
    menu1Prim[CurrentScreen].u0 = 32;
    menu1Prim[CurrentScreen].v0 = 0;
    menu1Prim[CurrentScreen].u1 = (short)image->width / 2 + 32;
    menu1Prim[CurrentScreen].v1 = 0;
    menu1Prim[CurrentScreen].u2 = 32;
    menu1Prim[CurrentScreen].v2 = image->height - 1;
    menu1Prim[CurrentScreen].u3 = (short)image->width / 2 + 32;
    menu1Prim[CurrentScreen].v3 = image->height - 1;
    menu1Prim[CurrentScreen].tpage = GetTPage(2, 0, image->x + 128, image->y);
    AddPrim(OT[CurrentScreen], &menu1Prim[CurrentScreen]);
}


void LoadModel(Object **modelShapes, char *fileName)
{
    Object *obj;
    int count = 0;
    char modelFile[256];

    *(Str20 *)modelFile = pathCommonDir;
    heap = reserveHeap;
    strcat(modelFile, fileName);
    strcat(modelFile, extPrm);
    modelShapes[0] = LoadPrm(modelFile, 0);
    obj = modelShapes[0];
    while (obj) {
        obj->skeleton = NewSkeleton();
        modelShapes[count] = obj;
        count++;
        obj = obj->next;
    }
    heap = mainheapcopy;
}


void DrawTeamShips(int x, int y, int z, int dir, int scale, Object *obj)
{
    SetSkeletonPosition(obj->skeleton, x, y, z);
    SetSkeletonDirection(obj->skeleton, (short)scale, (short)dir, 0);
    if (obj == teamShip[0]) {
        if (preCalcObject != obj) {
            PreCalcNormals(obj, VertexNormal);
            preCalcObject = obj;
        }
        SetSkeletonPosition(obj->skeleton, 0, 0, 0xAA);
        SetSkeletonDirection(obj->skeleton, (short)(scale - shipSpin), (short)dir, 0);
        if (gameData->shipClass == 0) {
            DrawEnvMap(obj, 0, VertexNormal, 0x70, 0x20, 0xC0);
        } else {
            DrawEnvMap(obj, 0, VertexNormal, 0x60, 0x20, 0xE0);
        }
        return;
    }
    if (scale == -1) {
        if (preCalcObject != obj) {
            PreCalcNormals(obj, VertexNormal);
            preCalcObject = obj;
        }
        SetSkeletonPosition(obj->skeleton, 0, 0, z);
        SetSkeletonDirection(obj->skeleton, (short)~shipSpin, (short)dir, 0);
        DrawEnvMap(obj, 0, VertexNormal, -0x6D, -0xF, 0x120);
        return;
    }
    AsmDrawObject(obj, 0, 0); /* Port: roll was never passed; 0 stands for the console's stale a2 */
}


void InitGameData(ConfigData *gameData)
{
    s16 i;
    s32 rand;

    if (gameData->initMagic != 0x25345327) {
        gameData->initMagic = 0x25345327;
        gameData->cdTrack = 1;
        *(s16 *)&gameData->menuState[5] = 0x96;
        gameData->musicVolume = 0x4B;
        gameData->effectVolume = 0x4B;
        gameData->otherMusicVolume = 0x4B;
        gameData->otherEffectVolume = 0x4B;
        gameData->soundMode = 1;
        gameData->shipClass = 0;
        gameData->negconCentre = 0xFF;
        turnDivisor = 0xFF;
        gameData->negconMin = 0x80;
        twist = 0x80;
        gameData->negconType = 1;
        currScreen = 1;
    }
    gameData->paused = 0;
    rand = newRand();
    gameData->menuState[4] = 1;
    gameData->initFlag1D = 1;
    gameData->weaponsOn = 1;
    gameData->ship2Num = 0;
    gameData->trackNum = 0;
    gameData->shipClass = 0;
    gameData->menuState[2] = 0;
    gameData->menuState[1] = 0;
    gameData->menuState[0] = 0;
    gameData->menuState[3] = 0;
    gameData->raceInfo = 0;
    gameData->complete = 0;
    gameData->initFlag16 = 0;
    gameData->highScore = 0;
    gameData->screenX = 0;
    gameData->screenY = 0;
    gameData->screenOffsetState = 0;
    gameData->moveScreen = 0;
    gameData->name[0] = 0;
    gameData->shipNum = (s8)(rand % 15);
    gameData->actTrackNum = (u8)raceTracks[0];
    gameData->venomComplete = 0;
    gameData->rapierComplete = 0;
    gameData->venomChallengeDone = 0;
    gameData->rapierChallengeDone = 0;
    gameData->teamCheatActive = 0;
    gameData->class1Unlocked = 0;
    gameData->class2Unlocked = 0;
    gameData->class1Race = 0;
    gameData->class2Race = 0;
    gameData->trackCheatActive = 0;
    gameData->phantomTrackCheatActive = 0;
    for (i = 0; i < 14; i++) {
        gameData->generated.digits[i] = 0;
        gameData->entered.digits[i] = 0;
    }
}
void LoadButtons(void)
{
    buttonTextures = TextureTableCount;
    LoadCmpFiles(WADLoadCompressedTextureSequence(textureWad));
}


void InitResidentMenu(void)
{
    LoadButtons();
    fontTexture = WADLoadTexture(textureWad, 1);
    screenFills[0] = BlockFill(0, 0, 0x140, 0xF0, 0, 0, 0);
    screenFills[1] = BlockFill(0, 0xF0, 0x140, 0xF0, 0, 0, 0);
}


void EstablishLink(void)
{
    s32 changedA;
    s32 changedB;
    CentreText(msgInitializingCombatLink, 1, 0x74, 8, 0);
    if (linkState > 0) {
        prevLinkInA = linkInA;
        prevLinkInB = linkInB;
        linkInA = (_comb_control(0, 0, 0) & 0x100) >> 7;
        linkInB = (_comb_control(0, 0, 0) & 0x80) >> 7;
        changedA = linkInA ^ prevLinkInA;
        changedB = linkInB ^ prevLinkInB;
    }
    switch (linkState) {
    case 0:
        _comb_control(1, 1, 0);
        linkToggleA = 0;
        linkToggleB = 0;
        linkInA = (_comb_control(0, 0, 0) & 0x100) >> 7;
        linkInB = (_comb_control(0, 0, 0) & 0x80) >> 7;
        menuSendKey = 0;
        linkState = 1;
        linkTickCount = 0;
        linkReady = 0;
        linkRetries = 0;
        linkWaitCount = 0;
        break;
    case 1:
        if (linkRetries < 4) {
            linkRetries++;
            if (changedA) {
                gameType = 1;
                linkState = 2;
            }
        } else {
            gameType = 0;
            linkState = 2;
        }
        break;
    case 2:
        if (linkWaitCount < 300) {
            linkWaitCount++;
            if (gameType == 0) {
                if (changedA) {
                    gameType = 1;
                } else {
                    if (linkToggleA == 0) {
                        _comb_control(1, 1, 2);
                    } else {
                        _comb_control(1, 1, 0);
                    }
                    linkToggleA ^= 1;
                    if (changedB) linkState = 3;
                }
            } else {
                if (linkToggleB == 0) {
                    _comb_control(1, 1, 1);
                } else {
                    _comb_control(1, 1, 0);
                }
                linkState = 3;
                linkToggleB ^= 1;
            }
        } else if (linkWaitCount < 420) {
            linkWaitCount++;
            CentreText(msgLinkFailed, 1, 150, 8, 0);
        } else {
            linkTimedOut = 1;
        }
        break;
    case 3:
        linkBlinkCount++;
        if (linkBlinkCount % 40 >= 20) {
            if (gameType == 0) {
                CentreText(msgMasterLinkEstablished, 1, 150, 8, 0);
            } else if (gameType == 1) {
                CentreText(msgSlaveLinkEstablished, 1, 150, 8, 0);
            }
        }
        if (++linkTickCount > 120) {
            linkState = 4;
            linkCloseCount = 0;
        }
        break;
    case 4:
        if (++linkCloseCount >= 3) {
            linkState = 5;
            linked = 1;
        }
        break;
    case 5:
        break;
    }
}


void killLink(void)
{
    menuSendKey = 0;
    menuRecvKey = 0;
    linked = 0;
    linkState = 0;
    gameType = 2;
}


void ShowLoadMessage(void)
{
    BLK_FILL *fills[2];
    short i;

    fills[0] = BlockFill(0, 0, 0x140, 0xF0, 0, 0, 0x2E);
    fills[1] = BlockFill(0, 0x100, 0x140, 0xF0, 0, 0, 0x2E);
    i = 0;
    do {
        ClearOTagR(OT[CurrentScreen], 0x2000);
        ClearOTagR(OT2[CurrentScreen], 0x640);
        quadIndex = CurrentScreen * 0x12C;
        CentreText(loadMessage, 0, 0x80, 0x10, 0);
        AddPrim(OT[CurrentScreen], fills[CurrentScreen]);
        PortWideLoadScreen(); /* Port: widescreen shows the loading screen wide */
        Swap();
        i++;
    } while (i < 2);
}


void InitRaceLoad(void)
{
    BLK_FILL *fills[2];
    short i;

    fills[0] = BlockFill(0, 0, 0x140, 0xF0, 0, 0, 0x2E);
    fills[1] = BlockFill(0, 0x100, 0x140, 0xF0, 0, 0, 0x2E);
    i = 0;
    do {
        ClearOTagR(OT[CurrentScreen], 0x2000);
        ClearOTagR(OT2[CurrentScreen], 0x640);
        quadIndex = CurrentScreen * 0x12C;
        DisplayTexture(textureTable[6 + menuTextures], 0x19, 0x26, loadPolys0);
        DisplayTexture(textureTable[6 + (menuTextures + 2)], 0x28, 0xBE, loadPolys1);
        AddPrim(OT[CurrentScreen], fills[CurrentScreen]);
        PortWideLoadScreen(); /* Port: widescreen shows the loading screen wide */
        Swap();
        i++;
    } while (i < 2);
}


void InitChallenges(void)
{
    Table10 table;
    s32 mode;
    s32 selected;
    s32 less;

    table = shipGroupTeam;
    if (gameData->venomComplete != 0) {
        menu[1].numOptions = 5;
        __builtin_memcpy(menu[1].options[4].text[0].text, msgChallengeI, 12);
        menu[1].options[4].text[0].size = 8;
        menu[1].options[4].text[0].colour = 1;
    } else if (gameData->rapierComplete != 0) {
        menu[1].numOptions = 5;
        __builtin_memcpy(menu[1].options[4].text[0].text, msgChallengeII, 13);
        menu[1].options[4].text[0].size = 8;
        menu[1].options[4].text[0].colour = 1;
    }
    if (gameData->class1Unlocked != 0 || (gameData->trackCheatActive != 0 || gameData->phantomTrackCheatActive != 0)) {
        menu[2].options[3].enabled = 1;
    } else {
        if (gameData->trackCheatActive == 0 && gameData->shipClass >= 3) {
            gameData->shipClass = 0;
        }
        if ((u8)gameData->trackNum >= 6U) {
            gameData->trackNum = 0U;
            gameData->actTrackNum = raceTracks[gameData->trackNum];
        }
    }
    if (gameData->rapierComplete != 0 || gameData->trackCheatActive != 0) {
        menu[2].options[3].enabled = 1;
    }
    if ((u8)gameData->trackNum >= 6U && gameData->class1Unlocked == 0 && gameData->trackCheatActive == 0) {
        gameData->trackNum = 0U;
        gameData->actTrackNum = 1U;
        menu[2].selection = 0;
        menu[2].mode = 0;
    }
    mode = (u8)gameData->shipClass;
    menu[2].selection = (s8)mode;
    selected = gameData->trackNum;
    less = ownShip < 0xC;
    menu[2].mode = selected;
    if (!less && !(*(s32 *)&tempGameData->rapierChallengeDone & 0xFF00FF00)) {
        ownShip = 9;
    }
    menu[4].selection = (u16)table.h[ownShip / 3];
}


void ReInitGameProgress(void)
{
    if (raceType == 4 && tempGameData->venomComplete != 0) {
        raceType = 0;
    }
    if (raceType == 4 && tempGameData->rapierComplete != 0) {
        raceType = 0;
    }
    menu[1].numOptions = 4;
    menu[2].options[3].enabled = 0;
    menu[2].selection = 0;
    menu[2].mode = 0;
    menu[1].selection = 0;
    tempGameData->phantomTrackCheatActive = 0;
    tempGameData->trackCheatActive = 0;
    tempGameData->teamCheatActive = 0;
    tempGameData->venomComplete = 0;
    tempGameData->rapierComplete = 0;
    tempGameData->class1Unlocked = 0;
    tempGameData->class2Unlocked = 0;
}


void InitMovies(void)
{
    sixXPlayed = 0;
    eightXPlayed = 0;
    sixTracks = 0;
    eightTracks = 0;
    if (gameData->venomComplete) {
        sixXPlayed = 1;
        sixTracks = 1;
    } else if (gameData->rapierComplete) {
        eightXPlayed = 1;
        eightTracks = 1;
        sixXPlayed = 1;
        sixTracks = 1;
    } else {
        if (gameData->teamCheatActive == 0 && ownShip >= 12) {
            ownShip = 9;
        }
    }
}


void InitSlider(void)
{
    screenCount = 1;
    slideCount = 0;
    topOffSet = 0;
    middleOffSet = 0;
    bottomOffSet = 0;
}


typedef struct SlideBox {
    u16      rect[6][4];
    POLY_FT4 poly[6][2];
} SlideBox;

extern u8 menuRecvFlags;

/* Slides the three bands of the menu screen and throws sparks along the seams. */
void SlideScreen(void *menu)
{
    SlideBox *box;
    s16 count, i, j;
    s32 step;
    s32 x, right, distance;

    box = menu;
    if (gameType != 1) count = menuSendFlags;
    else count = menuRecvFlags;
    count++;
    step = 8;
    while (count != 0) {
        if (slideCount < 45) {
            topOffSet += step;
            middleOffSet -= step;
            bottomOffSet += step;
        } else {
            topOffSet -= step;
            middleOffSet += step;
            bottomOffSet -= step;
        }
        x = box->rect[4][0] + bottomOffSet;
        right = middleOffSet + (box->rect[2][0] + box->rect[2][2]);
        distance = right - x;
        if (slideCount - 1 < 21 && (s16)distance > 0) {
            for (i = 0; i < 20; i++) {
                Insert2DParticle((s16)x, 82);
                Insert2DParticle((s16)x, 180);
                Insert2DParticle((s16)right, 82);
                Insert2DParticle((s16)right, 180);
            }
        }
        slideCount++;
        if (slideCount == 90) {
            slideCount = 0;
            screenCount = 0;
            PlayNote(5, 255, 15, 15, 256);
            SetShake(15);
            count = 1;
        }
        count--;
    }
    for (i = 0; i < 6; i++) {
        for (j = 0; j < 2; j++) {
            if ((u16)i < 2) {
                (box->poly[i] + j)->x0 = box->rect[i][0] + topOffSet;
                (box->poly[i] + j)->y0 = box->rect[i][1];
                (box->poly[i] + j)->x1 = topOffSet + (box->rect[i][0] + box->rect[i][2]);
                (box->poly[i] + j)->y1 = box->rect[i][1];
                (box->poly[i] + j)->x2 = box->rect[i][0] + topOffSet;
                (box->poly[i] + j)->y2 = box->rect[i][1] + box->rect[i][3];
                (box->poly[i] + j)->x3 = topOffSet + (box->rect[i][0] + box->rect[i][2]);
                (box->poly[i] + j)->y3 = box->rect[i][1] + box->rect[i][3];
            } else if ((u16)(i - 2) < 2) {
                (box->poly[i] + j)->x0 = box->rect[i][0] + middleOffSet;
                (box->poly[i] + j)->y0 = box->rect[i][1];
                (box->poly[i] + j)->x1 = middleOffSet + (box->rect[i][0] + box->rect[i][2]);
                (box->poly[i] + j)->y1 = box->rect[i][1];
                (box->poly[i] + j)->x2 = box->rect[i][0] + middleOffSet;
                (box->poly[i] + j)->y2 = box->rect[i][1] + box->rect[i][3];
                (box->poly[i] + j)->x3 = middleOffSet + (box->rect[i][0] + box->rect[i][2]);
                (box->poly[i] + j)->y3 = box->rect[i][1] + box->rect[i][3];
            } else {
                (box->poly[i] + j)->x0 = box->rect[i][0] + bottomOffSet;
                (box->poly[i] + j)->y0 = box->rect[i][1];
                (box->poly[i] + j)->x1 = bottomOffSet + (box->rect[i][0] + box->rect[i][2]);
                (box->poly[i] + j)->y1 = box->rect[i][1];
                (box->poly[i] + j)->x2 = box->rect[i][0] + bottomOffSet;
                (box->poly[i] + j)->y2 = box->rect[i][1] + box->rect[i][3];
                (box->poly[i] + j)->x3 = bottomOffSet + (box->rect[i][0] + box->rect[i][2]);
                (box->poly[i] + j)->y3 = box->rect[i][1] + box->rect[i][3];
            }
        }
    }
}
