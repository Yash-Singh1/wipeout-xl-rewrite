#include "options.h"
#include "functions.h"
#include "ayrace.h"
#include "hud.h"
#include "include_asm_g8.h"


extern short CurrentScreen;
extern char msgAreYouSure[];
extern char msgWantToRestart[];
extern char msgWantToQuit[];
extern char labelSelect[];
extern char labelYes[];
extern char labelNo[];
extern char *cdTrackNames[];
extern char *cdTrackArtists[];


char *GetCdTrackName(int track)
{
    return cdTrackNames[track];
}


char *GetCdTrackArtist(int track)
{
    return cdTrackArtists[track];
}


/* $gp-relative globals are tentative definitions; the rest are extern. */
extern MenuText pauseTextContinue;
extern MenuText pauseTextReturnToMenu;
extern MenuText pauseTextAudioConfig;
extern const unsigned char pauseMenuGap;
extern s32 restartText[2];
extern unsigned char openSteps;
extern unsigned char volumeRepeat;
extern unsigned char closeSteps;
extern unsigned char prevChoice;
extern unsigned char boxClosing;
extern signed char   settingRow;
extern short         boxOpen;
extern short         boxClosed;
extern short         boxWidth;
extern short         boxHeight;
extern short         boxX;
extern short         boxY;

extern unsigned short keysDown;
extern unsigned char cdMusicIndex;
extern int cdMusicCount;
extern int cdMusicTrack;
extern int ambientVoice1;
extern int trackNameScroll;
extern char msgControllerRemoved[];
extern char msgPaused[];
extern char labelAccept[];
extern char labelCdTrack[];
extern char nozzleShip[];
extern char targang[];


void InGameReset(struct PauseMenu *pause, ConfigData *game, struct InGameMenu *polys)
{
    MenuText options[4];
    int      i;
    short    y = 95;

    SfxSetReverbDepth(0);
    MuteControlledSound();
    if (padType == 0) {
        if ((u_int)pause->frameCount % 30 < 21) {
            CentreText(msgControllerRemoved, 107, 70, 8, 0);
        }
    } else {
        if ((u_int)pause->frameCount % 30 < 21) {
            CentreText(msgPaused, 107, 70, 12, 0);
        }
    }
    if (gameType == 1) {
        VSync(0);
    }
    if (pause->selection != 3) {
        boxX = 160;
        boxWidth = 0;
        boxHeight = 0;
        boxY = 170;
        openSteps = 0;
        closeSteps = 0;
        boxClosing = 0;
        boxOpen = 0;
        boxClosed = 0;
    }
    if (pause->stage == 2 || pause->stage == 0) {
        __builtin_memcpy(options[0].text, pauseTextContinue.text, 9);
        __builtin_memcpy(options[1].text, restartText, 8);
        __builtin_memcpy(options[2].text, pauseTextReturnToMenu.text, 15);
        __builtin_memcpy(options[3].text, pauseTextAudioConfig.text, 13);
        if (game->paused) {
            DoButton(133, 208, 1, labelAccept);
        } else {
            DoButton(133, 170, 1, labelSelect);
        }
        for (i = 0; i < 4; i++) {
            if (i == pause->selection) {
                if (!(pause->frameCount & 1)) {
                    CentreText(options[i].text, 0, y + pauseMenuGap * i, 8, 0);
                } else {
                    CentreText(options[i].text, 0, y + pauseMenuGap * i, 8, 1);
                }
            } else {
                CentreText(options[i].text, 0, y + pauseMenuGap * i, 8, 1);
            }
        }

        if (pause->selection == 3) {
            if (!game->paused && boxOpen) {
                SetVolume(ambientVoice1, 0);
            } else {
                if (!game->paused) {
                    if (GetMenuKey(5)) {
                        boxX = 160;
                        settingRow = 0;
                        boxWidth = 0;
                        boxHeight = 0;
                        boxY = 170;
                        openSteps = 0;
                        closeSteps = 0;
                        if (!game->paused) {
                            CdMusicPlay(cdMusicTrack, 0);
                            pause->musicPaused = 1;
                        }
                        game->paused = 1;
                    }
                } else if (boxOpen && GetMenuKey(5)) {
                    boxWidth = 180;
                    boxHeight = 60;
                    boxX = 70;
                    boxY = 140;
                    openSteps = 0;
                    closeSteps = 0;
                    if (game->paused) {
                        pause->musicPaused = 0;
                        CdMusicStop();
                        SetSoundVolume((game->effectVolume * 255) / 100);
                    }
                    game->paused = 0;
                    boxClosing = 1;
                }
                if (game->paused) {
                    if (boxOpen) {
                        if (GetMenuKey(0)) {
                            settingRow--;
                            if (settingRow < 0) settingRow = 0;
                        } else if (GetMenuKey(1)) {
                            settingRow++;
                            if (settingRow >= 3) settingRow = 2;
                        }
                        if (settingRow == 0) {
                            if (GetMenuKey(2)) {
                                PlayNote(8, 255, 15, 15, 256);
                                game->cdTrack--;
                                trackNameScroll = 480;
                                if (game->cdTrack <= 0) game->cdTrack = cdMusicCount;
                                if (game->cdTrack != 1) {
                                    CdMusicPlay(game->cdTrack, 0);
                                    pause->musicPaused = 1;
                                } else {
                                    cdMusicIndex = (cdMusicIndex + 1) % (cdMusicCount - 1);
                                    trackNameScroll = 480;
                                    cdMusicTrack = cdMusicTable[cdMusicIndex];
                                    CdMusicPlay(cdMusicTrack, 0);
                                    pause->musicPaused = 1;
                                }
                            } else if (GetMenuKey(3)) {
                                PlayNote(8, 255, 15, 15, 256);
                                game->cdTrack++;
                                trackNameScroll = 480;
                                if (game->cdTrack > cdMusicCount) game->cdTrack = 1;
                                if (game->cdTrack != 1) {
                                    CdMusicPlay(game->cdTrack, 0);
                                    pause->musicPaused = 1;
                                } else {
                                    cdMusicIndex = (cdMusicIndex + 1) % (cdMusicCount - 1);
                                    trackNameScroll = 480;
                                    cdMusicTrack = cdMusicTable[cdMusicIndex];
                                    CdMusicPlay(cdMusicTrack, 0);
                                    pause->musicPaused = 1;
                                }
                            }
                            PrintCX(GetCdTrackName(game->cdTrack - 1), 160, y + pauseMenuGap * 5);
                            if ((u_int)pause->frameCount % 30 < 21) CentreText(labelCdTrack, 0, y + pauseMenuGap * 4, 8, 1);
                            AddText(nozzleShip, 85, 170, 8, 1);
                            AddText(targang, 85, 185, 8, 1);
                        }
                        if (settingRow == 1) {
                            if (keysDown & 128) {
                                volumeRepeat++;
                                if (volumeRepeat == 5) {
                                    game->otherMusicVolume -= 5;
                                    if (game->otherMusicVolume < 0) game->otherMusicVolume = 0;
                                    volumeRepeat = 0;
                                }
                            } else if (keysDown & 32) {
                                volumeRepeat++;
                                if (volumeRepeat == 5) {
                                    game->otherMusicVolume += 5;
                                    if (game->otherMusicVolume > 100) game->otherMusicVolume = 100;
                                    volumeRepeat = 0;
                                }
                            }
                            game->musicVolume = game->otherMusicVolume;
                            SetCdVolume((game->musicVolume * 255) / 100);
                            PrintCX(GetCdTrackName(game->cdTrack - 1), 160, y + pauseMenuGap * 5);
                            CentreText(labelCdTrack, 0, y + pauseMenuGap * 4, 8, 1);
                            if ((u_int)pause->frameCount % 30 < 21) AddText(nozzleShip, 85, 170, 8, 1);
                            AddText(targang, 85, 185, 8, 1);
                        }
                        if (settingRow == 2) {
                            if (keysDown & 128) {
                                volumeRepeat++;
                                if (volumeRepeat == 5) {
                                    game->otherEffectVolume -= 5;
                                    if (game->otherEffectVolume < 0) game->otherEffectVolume = 0;
                                    volumeRepeat = 0;
                                }
                            } else if (keysDown & 32) {
                                volumeRepeat++;
                                if (volumeRepeat == 5) {
                                    game->otherEffectVolume += 5;
                                    if (game->otherEffectVolume > 100) game->otherEffectVolume = 100;
                                    volumeRepeat = 0;
                                }
                            }
                            game->effectVolume = game->otherEffectVolume;
                            SetSoundVolume((game->effectVolume * 255) / 100);
                            SetVolume(ambientVoice1, 255);
                            SfxEffectLRPan(ambientVoice1, 15);
                            SfxEffectFBPan(ambientVoice1, 15);
                            CentreText(labelCdTrack, 0, y + pauseMenuGap * 4, 8, 1);
                            PrintCX(GetCdTrackName(game->cdTrack - 1), 160, y + pauseMenuGap * 5);
                            if ((u_int)pause->frameCount % 30 < 21) AddText(targang, 85, 185, 8, 1);
                            AddText(nozzleShip, 85, 170, 8, 1);
                        } else {
                            SetVolume(ambientVoice1, 0);
                        }
                        UpdateNotes(1);
                        UpdateMusicBar(polys->red, game);
                        UpdateSfxBar(polys->green, game);
                    }
                polys->footer[CurrentScreen].y0 = boxY;
                polys->footer[CurrentScreen].y1 = boxY;
                polys->footer[CurrentScreen].x0 = boxX;
                polys->footer[CurrentScreen].x1 = boxX + boxWidth;
                polys->footer[CurrentScreen].x2 = boxX;
                polys->footer[CurrentScreen].y2 = boxY + boxHeight;
                polys->footer[CurrentScreen].x3 = boxX + boxWidth;
                polys->footer[CurrentScreen].y3 = boxY + boxHeight;
                if (!boxClosing) {
                    AddPrim(OT2[CurrentScreen] + 1599, &polys->footer[CurrentScreen]);
                    AddPrim(OT2[CurrentScreen] + 1599, polys->textureWindow[CurrentScreen]);
                }
                if (game->paused) {
                    openSteps = 0;
                    if (boxWidth < 180) boxWidth += 18; else openSteps = 1;
                    if (boxHeight < 60) boxHeight += 6; else openSteps++;
                    if (boxX > 70) boxX -= 9; else openSteps++;
                    if (boxY > 140) boxY -= 3; else openSteps++;
                    if (openSteps == 4) {
                        boxOpen = 1;
                        boxClosed = 0;
                    }
                }
                } else {
                    SetVolume(ambientVoice1, 0);
                }
            }
            if (boxClosing == 1) {
                closeSteps = 0;
                if (boxWidth > 0) boxWidth -= 18; else closeSteps = 1;
                if (boxHeight > 0) boxHeight -= 6; else closeSteps++;
                if (boxX < 160) boxX += 9; else closeSteps++;
                if (boxY < 170) boxY += 3; else closeSteps++;
                if (closeSteps == 4) {
                    boxClosed = 1;
                    boxOpen = 0;
                    boxClosing = 0;
                }
                polys->footer[CurrentScreen].y0 = boxY;
                polys->footer[CurrentScreen].y1 = boxY;
                polys->footer[CurrentScreen].x0 = boxX;
                polys->footer[CurrentScreen].x1 = boxX + boxWidth;
                polys->footer[CurrentScreen].x2 = boxX;
                polys->footer[CurrentScreen].y2 = boxY + boxHeight;
                polys->footer[CurrentScreen].x3 = boxX + boxWidth;
                polys->footer[CurrentScreen].y3 = boxY + boxHeight;
                AddPrim(OT[CurrentScreen], &polys->footer[CurrentScreen]);
                AddPrim(OT[CurrentScreen], polys->textureWindow[CurrentScreen]);
                }
        } else {
            SetVolume(ambientVoice1, 0);
        }
    }
    pause->frameCount++;
    prevChoice = pause->selection;
}


void UpdateMusicBar(POLY_G4 bars[][2], ConfigData *game)
{
    int level;
    short i;
    int volume;
    int barX;

    level = 0;
    i = 0;
    do {
        volume = game->otherMusicVolume;
        if (volume > 0) {
            level = volume / 5;
        }
        if (!(level & 0xFF)) {
            level = 1;
        }
        if (i + 1 == (level & 0xFF)) {
            bars[i][CurrentScreen].r0 = 0;
            bars[i][CurrentScreen].g0 = 0;
            bars[i][CurrentScreen].b0 = 0x3F;
            bars[i][CurrentScreen].r1 = 0;
            bars[i][CurrentScreen].g1 = 0;
            bars[i][CurrentScreen].b1 = 0x7F;
            bars[i][CurrentScreen].r2 = 0;
            bars[i][CurrentScreen].g2 = 0;
            bars[i][CurrentScreen].b2 = 0xBF;
            bars[i][CurrentScreen].r3 = 0;
            bars[i][CurrentScreen].g3 = 0;
            bars[i][CurrentScreen].b3 = 0xFF;
        } else {
            bars[i][CurrentScreen].r0 = 0x3F;
            bars[i][CurrentScreen].g0 = 0;
            bars[i][CurrentScreen].b0 = 0;
            bars[i][CurrentScreen].r1 = 0x7F;
            bars[i][CurrentScreen].g1 = 0;
            bars[i][CurrentScreen].b1 = 0;
            bars[i][CurrentScreen].r2 = 0xBF;
            bars[i][CurrentScreen].g2 = 0;
            bars[i][CurrentScreen].b2 = 0;
            bars[i][CurrentScreen].r3 = 0xFF;
            bars[i][CurrentScreen].g3 = 0;
            bars[i][CurrentScreen].b3 = 0;
        }
        barX = i * 5;
        bars[i][CurrentScreen].x0 = barX + 0x8C;
        bars[i][CurrentScreen].y0 = 170;
        bars[i][CurrentScreen].y1 = 170;
        bars[i][CurrentScreen].x1 = barX + 0x90;
        bars[i][CurrentScreen].x2 = barX + 0x8C;
        bars[i][CurrentScreen].y2 = 178;
        bars[i][CurrentScreen].x3 = barX + 0x90;
        bars[i][CurrentScreen].y3 = 178;
        AddPrim(OT[CurrentScreen], &bars[i][CurrentScreen]);
        i++;
    } while (i < 20);
}


void UpdateSfxBar(POLY_G4 bars[][2], ConfigData *game)
{
    int level;
    short i;
    int volume;
    int barX;

    level = 0;
    i = 0;
    do {
        volume = game->otherEffectVolume;
        if (volume > 0) {
            level = volume / 5;
        }
        if (!(level & 0xFF)) {
            level = 1;
        }
        if (i + 1 == (level & 0xFF)) {
            bars[i][CurrentScreen].r0 = 0;
            bars[i][CurrentScreen].g0 = 0;
            bars[i][CurrentScreen].b0 = 0x3F;
            bars[i][CurrentScreen].r1 = 0;
            bars[i][CurrentScreen].g1 = 0;
            bars[i][CurrentScreen].b1 = 0x7F;
            bars[i][CurrentScreen].r2 = 0;
            bars[i][CurrentScreen].g2 = 0;
            bars[i][CurrentScreen].b2 = 0xBF;
            bars[i][CurrentScreen].r3 = 0;
            bars[i][CurrentScreen].g3 = 0;
            bars[i][CurrentScreen].b3 = 0xFF;
        } else {
            bars[i][CurrentScreen].r0 = 0;
            bars[i][CurrentScreen].g0 = 0x3F;
            bars[i][CurrentScreen].b0 = 0;
            bars[i][CurrentScreen].r1 = 0;
            bars[i][CurrentScreen].g1 = 0x7F;
            bars[i][CurrentScreen].b1 = 0;
            bars[i][CurrentScreen].r2 = 0;
            bars[i][CurrentScreen].g2 = 0xBF;
            bars[i][CurrentScreen].b2 = 0;
            bars[i][CurrentScreen].r3 = 0;
            bars[i][CurrentScreen].g3 = 0xFF;
            bars[i][CurrentScreen].b3 = 0;
        }
        barX = i * 5;
        bars[i][CurrentScreen].x0 = barX + 0x8C;
        bars[i][CurrentScreen].y0 = 185;
        bars[i][CurrentScreen].y1 = 185;
        bars[i][CurrentScreen].x1 = barX + 0x90;
        bars[i][CurrentScreen].x2 = barX + 0x8C;
        bars[i][CurrentScreen].y2 = 193;
        bars[i][CurrentScreen].x3 = barX + 0x90;
        bars[i][CurrentScreen].y3 = 193;
        AddPrim(OT[CurrentScreen], &bars[i][CurrentScreen]);
        i++;
    } while (i < 20);
}


void InGameConfirm(u_char *confirmed, int mode, int unused, u_char *selection, int *frameCount)
{
    int highlight;

    highlight = 1;
    if (mode == 1) {
        CentreText(msgAreYouSure, 0x4D, 0x5F, 8, highlight);
        CentreText(msgWantToRestart, 0x5A, 0x69, 8, highlight);
    } else {
        CentreText(msgAreYouSure, 0x4D, 0x5F, 8, highlight);
        CentreText(msgWantToQuit, 0x69, 0x69, 8, highlight);
    }
    DoButton(0x85, 0xAA, 1, labelSelect);
    if (GetMenuKey(2) != 0 || GetMenuKey(3) != 0) {
        *selection ^= 1;
        PlayNote(8, 0xFF, 0xF, 0xF, 0x100);
    }

    if (*selection != 0) {
        if (!(*frameCount & 1)) {
            AddText(labelYes, 0x4B, 0x78, 0x10, 0);
        } else {
            AddText(labelYes, 0x4B, 0x78, 0x10, 1);
        }
        AddText(labelNo, 0xCD, 0x78, 0x10, 1);
    } else {
        AddText(labelYes, 0x4B, 0x78, 0x10, 1);
        if (!(*frameCount & 1)) {
            AddText(labelNo, 0xCD, 0x78, 0x10, 0);
        } else {
            AddText(labelNo, 0xCD, 0x78, 0x10, 1);
        }
    }
    if (gameType == 1) {
        VSync(0);
    }
    if (GetMenuKey(5) != 0 || GetMenuKey(6) != 0) {
        *confirmed = *selection;
    }
    *frameCount += 1;
}


/* Sets the four corners of a POLY_F4 in order: top-left, top-right, bottom-left, bottom-right. */
#define setXY4(p, _x0, _y0, _x1, _y1, _x2, _y2, _x3, _y3) (p)->x0 = (_x0), (p)->y0 = (_y0), (p)->x1 = (_x1), (p)->y1 = (_y1), (p)->x2 = (_x2), (p)->y2 = (_y2), (p)->x3 = (_x3), (p)->y3 = (_y3)

void DeSelectScreen(struct SelectBox *sel)
{
    SetPolyF4(&sel->box[CurrentScreen]);
    sel->box[CurrentScreen].r0 = 6;
    sel->box[CurrentScreen].g0 = 6;
    sel->box[CurrentScreen].b0 = 6;
    setXY4(&sel->box[CurrentScreen], 0, 0, 320, 0, 0, SCREEN_HEIGHT, 320, SCREEN_HEIGHT);
    SetSemiTrans(&sel->box[CurrentScreen], 1);
    SetDrawMode(&sel->trans[CurrentScreen], 1, 1, GetTPage(0, 0, 0, 0), 0);
    AddPrim(OT[CurrentScreen], &sel->box[CurrentScreen]);
    AddPrim(OT[CurrentScreen], &sel->trans[CurrentScreen]);
}
