#include "functions.h"
#include "sfx.h"
#include "pad.h"
#include "racectrl.h"
#include "ayrace.h"
#include "weapon.h"
#include "hud.h"
#include "ghost.h"
#include "hiscores.h"
#include "options.h"
#include "raceinit.h"

/* $gp-relative globals are tentative definitions; the rest are extern. */
extern short continueAnimFrame;
extern short droneSwitchStep;
extern short            linkDropped;
extern short            linkMyState;
extern short            linkRemoteState;
extern short            linkTimer;
extern short            linked;
extern u_char           medalDisplay;
extern u_char           perfectLapCount;
extern u_char           plasmaShotBusy;
extern u_char           raceEndState;
extern u_char           linkRaceDone;
/* Port: hudDisabled is part of scoreTabData; the uses read it directly. */
extern HiScoreData scoreTabData;
extern u_char           sfxMode;
extern int              trackNameScroll;
extern int              playCredits;
extern int              ownShip;
extern int              raceType;
extern int              raceEndTimer;
extern int              attractLoopFlag;
extern int              checkTimer;
extern int              attractModeFlag;
extern int              attractDelayFrames;
extern int              oneMoreTime;
extern int              masterWait;
extern int              raceContext;
extern u_short          idleTimer;
extern u_short          keysDown;
extern short            sixTracks;
extern short            sixXPlayed;
extern short            eightTracks;
extern short            eightXPlayed;
extern short            phantomDone;
extern short            piranhaDone;
extern u_char           linkSyncB;
extern u_char           linkRaceEnd;
extern u_char           cdMusicIndex;
extern int              cdMusicTrack;
extern ConfigData      *gameData;
extern TrackData       *track;
extern ShipData        *shipIndex;
extern int              shipDamage[];
extern int              fracdamage[];
extern char            *crapArray[];
extern char             selectLabel[];

/* Starts the music track that the CD drive reports as playing, once per change. */
void SetInGameCDTrack(signed char *bcd, struct WipeCdStatus *position, ConfigData *trackInfo, int *state)
{
    int           selectedTrack;
    int           trackNumber;
    signed char   mode;

    *bcd = ((position->time >> 4) * 10) + (position->time & 0xF);
    CdControlB(0x11, 0, (u_char *)position);
    mode = trackInfo->cdTrack;
    if (mode == 1) {
        trackNumber = ((position->track >> 4) * 10) + (position->track & 0xF);
        if (trackNumber != cdMusicTrack) {
            if (*state == 0) {
                selectedTrack = cdMusicTable[cdMusicIndex];
                trackNameScroll = 0x1E0;
                cdMusicTrack = selectedTrack;
                CdMusicPlay(selectedTrack, 0);
                *state = 1;
            }
        } else if (*state == 1) {
            *state = 0;
        }
    } else {
        int tens;
        int units;

        tens = (position->track >> 4) * 10;
        units = position->track & 0xF;
        tens += units;
        if (tens != mode) {
            if (*state == 0) {
                cdMusicTrack = mode;
                CdMusicPlay(mode, 0);
                *state = 1;
            }
        } else if (*state == 1) {
            *state = 0;
        }
    }
}


void CheckShipDestroyed(ShipData *ships, ConfigData *game, int *attract, int idleLimit)
{
    void    (*update)();
    ShipData *own;

    if ((short)idleTimer >= idleLimit) {
        if (linkSendFlag != 0 && raceEndState == 2 && raceType != RACE_CHALLENGE) {
            linkSendFlag = 0;
            game->complete = 0;
            goto check;
        }
        if (!(ships[ownShip].attr & RACING)) goto check;
        if (ships[ownShip].combatAttr & DESTROYED) goto destroyed;
        ships[ownShip].update = UpdateDroneShip;
        CameraUpdate = UpdateCameraAttractMode;
        ships[ownShip].attr &= ~VIEW_IN;
        ships[ownShip].attr &= ~RACING;
        humanToDroneSpeed(ships, ownShip);
        attractDelayFrames = PAL_TIME(900);
        *attract = 1;
        idleTimer = 0;
    }
check:
    if (ships[ownShip].combatAttr & DESTROYED) {
destroyed:
        if (ships[ownShip].attr & RACING) {
            ships[ownShip].attr &= ~RACING;
            ships[ownShip].attr &= ~VIEW_IN;
            if (!(ships[ownShip].combatAttr & 4)) {
                CameraUpdate = UpdateCameraAttractDestroyed;
            }
            game->raceInfo = 1;
            linkSendFlag = 1;
            attractDelayFrames = 1;
            raceEndState = 2;
            finishPosition = 15;
            copyInTempLapTimes(ships);
        }
    }
    if (droneSwitchStep == 1) {
        own = &ships[ownShip];
        game->complete = 1;
        update = own->update;
        linkSendFlag = droneSwitchStep;
        attractDelayFrames = droneSwitchStep;
        droneSwitchStep++;
        raceEndState = 2;
        finishPosition = 15;
        if (update == UpdatePlayerShipStart) {
            own->update = UpdateDroneShipStart;
        } else {
            own->update = UpdateDroneShip;
        }
        CameraUpdate = UpdateCameraAttractMode;
        ships[ownShip].attr &= ~VIEW_IN;
        ships[ownShip].attr &= ~RACING;
        humanToDroneSpeed(ships, ownShip);
    }
}


void CheckForButtonPress(void)
{
    UpdateNotes(1);
    if (raceContext != 1) {
        if (padType != 2) {
            if (gameType == GAME_SINGLE_PLAYER) {
                if (keysDown != 0) {
                    idleTimer = 0;
                    return;
                }
                idleTimer++;
            }
        } else if (gameType == padType) {
            if (keysDown != 0 || (u_int)(playerControl[0].steering - 0x7A) >= 14 || playerControl[0].thrust >= 31 /* Port: were the separate names linkPadAux1 and linkPadAux0 for these two bytes of playerControl[0] */) {
                idleTimer = 0;
                return;
            }
            idleTimer++;
        }
    }
}


void PlayCredits(int state)
{
    if (state == 1) {
        if (attractModeFlag == state) {
            ShowCredits(attractDelayFrames);
            return;
        }
        if (!(shipIndex[ownShip].combatAttr & DESTROYED)) {
            CentreText("DEMO MODE", 60, REGION(20, 10), 8, 0);
        }
    }
}


void Restart(ResetData *reset)
{
    int    i;
    u_char unused;

    checkTimer = 0;
    for (i = 0; i < 6; i++) {
        reset->checkPoints[i].reached = -1;
    }
    TerminateSound();
    SfxSetMode(sfxMode);
    StartControlledSound();
    FlattenTrack();
    initShipData(track->sections, reset->ships, reset->config);
    InitRescueDroid(reset->rescue, reset->shipShapes, &reset->ships[ownShip]);
    if (raceType == RACE_TIME_TRIAL) {
        InitGhostShip(reset->config, reset->ships);
    }
    cameraPtr->section = reset->ships[ownShip].nearTrkSect;
    ResetStartBoom();
    if (reset->config->actTrackNum == 2) {
        ResetAnimatedObjects();
    }
    ResetLightStart();
    for (i = 0; i < 30; i++) {
        reset->weapons[i].count = 0;
        reset->weapons[i].type = 0;
    }
    for (i = 0; i < 31; i++) {
        reset->weaponShapes[i].used = 0;
    }
    InitCDAudio(gameData, &unused);
    for (i = 0; i < 15; i++) {
        shipDamage[i] = 0;
        fracdamage[i] = 0;
        shipTargetIndex[i] = -1;
    }
    InitElectric();
    CameraUpdate = UpdateCameraRaceStart;
    medalDisplay = 0;
    linkRaceDone = 0;
    playCredits = 0;
    raceCompleteFlag = 0;
    linkMyState = 0;
    linkRemoteState = 0;
    linkSendFlag = 0;
    raceEndTimer = 0;
    raceEndState = 0;
    medalDisplay = 0;
    scoreTabData.highScore = 0;
    plasmaShotBusy = 0;
    perfectLapCount = 0;
    perfectLap = 0;
    droneSwitchStep = 0;
    continueAnimFrame = 0;
    idleTimer = 0;
    linkDropped = 0;
    linkTimer = 0;
    trackNameScroll = 480;
}


void QuitGame(void)
{
    gameType = GAME_SINGLE_PLAYER;
    if (raceType != RACE_CHALLENGE) {
        raceType = 0x63;
    }
    if (attractModeFlag != 1) {
        ShowLoadMessage();
    }
    if (linked != 0) {
        killLink();
    }
    KillControlledSound();
    CdMusicStop();
}


int CheckAttractOver(ConfigData *config)
{
    if (keysPressed == 0 || (shipIndex[ownShip].combatAttr & DESTROYED)) {
        if (attractDelayFrames != 0) {
            return 0;
        }
    }
    config->menuState[3] = 99;
    raceType = 99;
    if (attractDelayFrames != 0) {
        attractLoopFlag = 1;
    }
    raceEndTimer = 1;
    return 1;
}


void RaceComplete(void *screen, void *unused, ConfigData *gameData, short *choice, HiScoreData *scoreData, ShipData *ships)
{
    char text[32];
    int  race;

    if (gameData->complete) {
        if (raceType == RACE_CHALLENGE && (gameData->venomComplete || gameData->rapierComplete)) {
            if (finishPosition == 1 || finishPosition >= 4) {
                if (finishPosition == 1) {
                    if (gameData->venomComplete == 1 && gameData->class1Race == 5) {
                        gameData->class1Unlocked = 1;
                        phantomDone = 1;
                        linkMyState = 3;
                        raceEndTimer = 2;
                        raceType = 99;
                        goto done;
                    }
                    if (gameData->rapierComplete == 1 && gameData->class2Race == 7) {
                        gameData->class2Unlocked = 1;
                        piranhaDone = 1;
                        linkMyState = 3;
                        raceEndTimer = 2;
                        raceType = 99;
                        goto done;
                    }
                    if (gameData->venomComplete == 1) {
                        CentreText("PHANTOM CHALLENGE", 60, 36, 8, 1);
                        race = gameData->class1Race;
                    } else if (gameData->rapierComplete == 1) {
                        CentreText("PIRANHA CHALLENGE", 60, 36, 8, 1);
                        race = gameData->class2Race;
                    }
                    strcpy(text, crapArray[race]);
                    if (race == 0) strcat(text, " CIRCUIT COMPLETE");
                    else strcat(text, " CIRCUITS COMPLETED");
                    CentreText(text, 60, 50, 8, 1);
                    if (GetMenuKey(MENU_KEY_UP)) {
                        PlayNote(8, 255, 15, 15, 256);
                        (*choice)--;
                        if (*choice<0) *choice = 0;
                    }
                    if (GetMenuKey(MENU_KEY_DOWN)) {
                        PlayNote(8, 255, 15, 15, 256);
                        (*choice)++;
                        if (*choice >= 2) *choice = 1;
                    }
                    if (*choice == 0) {
                        CentreText("NEXT CIRCUIT", 60, 110, 8, 0);
                        CentreText("END CHALLENGE", 60, 124, 8, 1);
                    } else if (*choice == 1) {
                        CentreText("NEXT CIRCUIT", 60, 110, 8, 1);
                        CentreText("END CHALLENGE", 60, 124, 8, 0);
                    }
                    DoButton(133, RESULT_BUTTON_Y, 1, selectLabel);
                    DeSelectScreen(screen);
                    if (GetMenuKey(MENU_KEY_SELECT)) {
                        PlayNote(9, 255, 15, 15, 256);
                        gameData->complete = 0;
                        linkSendFlag = 0;
                        if (*choice == 0) {
                            if (gameData->venomComplete == 1) gameData->class1Race++;
                            else gameData->class2Race++;
                            linkMyState = 3;
                            raceEndTimer = 2;
                        } else if (*choice == 1) {
                            EndChallenge(gameData);
                            linkMyState = 3;
                            raceEndTimer = 2;
                        }
                    }
                    goto done;
                } else {
                    CentreText("YOU DID NOT QUALIFY", 60, 36, 8, 1);
                    DoButton(133, RESULT_BUTTON_Y, 1, selectLabel);
                    if (continuesLeft) {
                        DisplayContinues(continuesTexture, 176, 168);
                        if (continueAnimFrame == 0) {
                            if (GetMenuKey(MENU_KEY_UP)) {
                                PlayNote(8, 255, 15, 15, 256);
                                (*choice)--;
                                if (*choice<0) *choice = 0;
                            }
                            if (GetMenuKey(MENU_KEY_DOWN)) {
                                PlayNote(8, 255, 15, 15, 256);
                                (*choice)++;
                                if (*choice >= 2) *choice = 1;
                            }
                        }
                        if (*choice == 0) {
                            CentreText("CONTINUE GAME", 60, 110, 8, 0);
                            CentreText("GO TO MENU", 60, 124, 8, 1);
                        } else if (*choice == 1) {
                            CentreText("CONTINUE GAME", 60, 110, 8, 1);
                            CentreText("GO TO MENU", 60, 124, 8, 0);
                        }
                    } else CentreText("GO TO MENU", 60, 124, 8, 0);
                    DeSelectScreen(screen);
                    if (continueAnimFrame == 0) {
                        if (GetMenuKey(MENU_KEY_SELECT)) {
                            PlayNote(9, 255, 15, 15, 256);
                            if (*choice == 0 && continuesLeft) {
                                continueAnimFrame = 1;
                                PlayNote(SFX_START_STALL, 480, 15, 15, 320);
                            } else if (*choice == 1 || !continuesLeft) {
                                linkMyState = 3;
                                gameData->complete = 0;
                                linkSendFlag = 0;
                                raceEndTimer = 2;
                                EndChallenge(gameData);
                            }
                        }
                    } else if (continueAnimFrame == 30) {
                        gameData->complete = 0;
                        linkSendFlag = 0;
                        linkRaceDone = 1;
                        continuesLeft--;
                    } else continueAnimFrame++;
                    goto done;
                }
            }
        }
        if (gameType == GAME_SINGLE_PLAYER || linkSyncB) {
            if (gameType != GAME_LINK_SLAVE) {
                if ((sixTracks == 1 && sixXPlayed == 0) || (eightTracks == 1 && eightXPlayed == 0)) {
                    raceEndTimer = 1;
                    linkSendFlag = 0;
                    goto done;
                }
                if (raceType == RACE_CHALLENGE) {
                    CentreText("MEDAL ACHIEVED", 60, 36, 8, 1);
                    CentreText("FREE CONTINUE AWARDED", 60, 50, 8, 1);
                }
                if (GetMenuKey(MENU_KEY_UP)) {
                    PlayNote(8, 255, 15, 15, 256);
                    (*choice)--;
                    if (*choice<0) *choice = 0;
                }
                if (GetMenuKey(MENU_KEY_DOWN)) {
                    PlayNote(8, 255, 15, 15, 256);
                    (*choice)++;
                    if (*choice >= 2) *choice = 1;
                }
                if (*choice == 0) {
                    CentreText("REPLAY GAME", 60, 110, 8, 0);
                    CentreText("GO TO MENU", 60, 124, 8, 1);
                } else if (*choice == 1) {
                    CentreText("REPLAY GAME", 60, 110, 8, 1);
                    CentreText("GO TO MENU", 60, 124, 8, 0);
                }
                DoButton(133, RESULT_BUTTON_Y, 1, selectLabel);
                DeSelectScreen(screen);
                if (GetMenuKey(MENU_KEY_SELECT)) {
                    PlayNote(9, 255, 15, 15, 256);
                    gameData->complete = 0;
                    linkSendFlag = 0;
                    if (*choice == 0) linkRaceDone = 1;
                    else if (*choice == 1) {
                        raceType = 99;
                        linkMyState = 3;
                        raceEndTimer = 2;
                    }
                }
            } else {
                gameData->complete = 0;
                linkSendFlag = 0;
            }
        }
    } else if (gameData->raceInfo) {
        if (gameType != GAME_SINGLE_PLAYER && !linkRaceEnd) {
            CentreText("WAITING FOR OTHER PLAYER", 25, 100, 8, 0);
            if (gameType == GAME_LINK_SLAVE) VSync(0);
        } else {
            DisplayRaceInfo(gameData, scoreData, ships);
            if (raceEndState != 2 || shipTargetIndex[ownShip]<5) DeSelectScreen(screen);
            if (GetMenuKey(MENU_KEY_SELECT)) {
                PlayNote(9, 255, 15, 15, 256);
                gameData->raceInfo = 0;
                if (raceEndState == 2) {
                    gameData->complete = 1;
                    raceCompleteFlag = 1;
                } else gameData->highScore = 1;
            }
        }
    } else {
        if (gameData->highScore) {
            CheckForHiScore(gameData, scoreData, ships, ownShip, 0);
            if (raceType == RACE_TIME_TRIAL) storeGhostTrail(gameData, scoreData);
            gameData->highScore = 0;
            InitDefaultName(gameData, scoreData, 0);
            if (!scoreData->flag) {
                raceCompleteFlag = 1;
                gameData->complete = 1;
            }
        }
        if (scoreData->flag) scoreData->highScore = 1;
        if (scoreData->highScore) {
            DisplayScoreTab(gameData, scoreData);
            DeSelectScreen(screen);
            gameData->highScore = 0;
        }
    }
    done: if (gameData->complete) InitScoreTabData(scoreData);
}


void CheckGamePause(ShipData *ships, int *pauseFlag, ConfigData *config, u_short *pauseChoice, int *pauseCounter)
{
    if ((ships[ownShip].attr & RACING) && (((GetMenuKey(MENU_KEY_START) << 0x10) != 0) || (padType == 0)) && (*pauseFlag == -1)) {
        *pauseCounter = 0;
        CdMusicStop();
        PlayNote(9, 0xFF, 0xF, 0xF, 0x100);
        if (raceType == RACE_TIME_TRIAL) {
            recordBuffer->disabled = 1;
        }
        *pauseFlag = 0;
        *pauseChoice = 0;
        oneMoreTime = 1;
    }
}


void Check2PlayerPause(u_char status, u_char *lastStatus, int *musicPaused)
{
    int current;

    current = status & 0xFF;
    if (current == 1) {
        if (*lastStatus == 2) {
            *musicPaused = CdControl(9, 0, 0);
        } else if (*musicPaused == 0 && CdControl(9, 0, 0) != 0) {
            *musicPaused = 1;
        }
    } else if (current == 2 && *lastStatus == 1) {
        CdControl(3, 0, 0);
    }
    *lastStatus = status;
}


void SelectPauseOption(int *selection, int canMove, ConfigData *config, struct PauseMenu *pause)
{
    int up;
    int down;

    if (config->paused == 0 && (gameType == GAME_SINGLE_PLAYER || pause->stage == 0) && padType != 0) {
        if (GetMenuKey(MENU_KEY_UP) && canMove) {
            up = *selection - 1;
            *selection = up;
            if (up < 0) {
                *selection = 3;
            }
            goto played;
        }
        if (GetMenuKey(MENU_KEY_DOWN) && canMove) {
            down = *selection + 1;
            *selection = down;
            if (down >= 4) {
                *selection = 0;
played:;
            }
            PlayNote(8, 255, 15, 15, 256);
        }
    }
}


int PerformPauseOperations(struct PauseMenu *pause, struct SelectBox *box, ConfigData *config)
{
    int selection;
    int stage;

    if (pause->selection >= 0) {
        if (linkDropped == 0) {
            UpdateNotes(0);
            SelectPauseOption(&pause->selection, pause->showOptions, config, pause);
            if (pause->showOptions) {
                /* Port: box is the in-game menu block, whose first bytes are the SelectBox that
                   DeSelectScreen takes; InGameReset uses the block's other polygons. */
                InGameReset(pause, config, (struct InGameMenu *)box);
                if (pause->musicPaused == 0 && CdControl(9, 0, 0) != 0) {
                    pause->musicPaused = 1;
                }
                DeSelectScreen(box);
                pause->confirmChoice = 0;
            } else {
                InGameConfirm(&pause->confirmed, pause->selection, keysPressed, &pause->confirmChoice, &pause->frameCount);
                DeSelectScreen(box);
            }
            if (pause->confirmed == 0) {
                pause->showOptions = 1;
                pause->confirmed = 2;
                pause->selection = 0;
                keysPressed = 0;
            }
            if (padType != 0) {
                if ((GetMenuKey(MENU_KEY_SELECT) || GetMenuKey(MENU_KEY_START)) && !pause->showOptions && pause->confirmed == 1 && pause->stage != 1) {
                    selection = pause->selection;
                    pause->showOptions = 1;
                    pause->confirmed = 2;
                    switch (selection) {
                    case 0:
                        pause->selection = -1;
                        pause->command = 0;
                        break;
                    case 1:
                        pause->selection = -1;
                        pause->command = selection;
                        if (raceType == RACE_CHALLENGE) {
                            droneSwitchStep = 1;
                            masterWait = selection;
                        } else {
                            linkRaceDone = 1;
                            masterWait = selection;
                        }
                        break;
                    case 2:
                        stage = pause->stage;
                        pause->selection = -1;
                        pause->command = selection;
                        if (stage == selection) {
                            config->menuState[3] = 99;
                            raceType = 99;
                            linkMyState = 3;
                            raceEndTimer = stage;
                            return 1;
                        }
                        break;
                    }
                }
                if (padType != 0 && (GetMenuKey(MENU_KEY_SELECT) || GetMenuKey(MENU_KEY_START)) && pause->showOptions) {
                    if (pause->stage != 1) {
                        PlayNote(9, 255, 15, 15, 256);
                        if ((u_int)(pause->selection - 1) < 2) {
                            pause->showOptions = 0;
                        } else if (pause->selection == 0) {
                            CdControl(3, 0, 0);
                            pause->command = 0;
                            pause->selection = -1;
                            masterWait = 1;
                        }
                    }
                }
            }
        } else {
            DeSelectScreen(box);
        }
    } else if (linkDropped != 0) {
        DeSelectScreen(box);
    }
    return 0;
}


void InitiateTwoPlayerPause(struct PauseMenu *pause)
{
    int command;

    if (gameType != GAME_SINGLE_PLAYER && pause->selection >= 0) {
        if (pause->active == 0) {
            pause->active = 1;
            pause->stage = 0;
            linkMyState = 1;
            goto block_4;
        }
        goto block_5;
    }
block_4:
    if (pause->active != 0) {
block_5:
        if (pause->stage == 0) {
            if (pause->command == 0) {
                linkMyState = 0;
                pause->stage = 2;
                pause->active = 0;
                pause->command = -1;
            }
            if (pause->command == 1) {
                linkMyState = 2;
                pause->stage = 2;
                pause->active = 0;
                pause->command = -1;
            }
            command = pause->command;
            if (command == 2) {
                linkMyState = 3;
                raceEndTimer = command;
            }
        }
    }
}


void Perform2PlayerPauseCommand(struct PauseMenu *pause)
{
    if ((linkRemoteState == 1) && (pause->active == 0)) {
        pause->stage = 1;
        pause->active = 1;
        pause->selection = 0;
    }
    if ((linkRemoteState == 3) || (linkMyState == 3)) {
        pause->stage = 2;
        pause->selection = -1;
        pause->active = 0;
        linkMyState = 3;
        pause->count = 0;
        raceEndTimer = 2;
    }
    if ((pause->active != 0) && (pause->stage == 1)) {
        if (linkRemoteState == 0) {
            pause->stage = 2;
            pause->active = 0;
            pause->selection = -1;
        }
        if (linkRemoteState == 2) {
            pause->stage = 2;
            pause->selection = -1;
            pause->active = 0;
            linkRaceDone = 1;
        }
        if (linkRemoteState == 3) {
            pause->stage = 2;
            pause->active = 0;
            pause->selection = -1;
        }
    }
    if ((pause->stage == 0) && (pause->command == 2)) {
        pause->stage = 2;
        pause->active = 0;
        pause->command = -1;
    }
}
