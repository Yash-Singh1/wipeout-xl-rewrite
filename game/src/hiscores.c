#include "functions.h"
#include "hiscores.h"
#include "include_asm_g8.h"

/* $gp-relative globals are tentative definitions; the rest are extern. */
s32 scoreTabFrame;

extern int          raceType;
extern u16          keysDown;
extern int          ownShip;
extern int          savedLapTimes[];
extern s32          myBestLap;
extern s32          myBestRace;
/* Port: glyphWidth is part of fontGlyphs16; the uses read it directly. */
#include "hud.h"
extern FontGlyph fontGlyphs16[];
extern u8           msgSelect[];

static const HiScoreNames defaultHiScoreNames = {{
        {"ROB", "NIK", "ASH", "EVE"},
        {"POL", "AJS", "ASH", "DAV"},
        {"NIC", "JON", "PAT", "JEF"},
        {"MIC", "TIM", "JEN", "JIM"},
        {"STU", "NIK", "PAT", "CJR"},
        {"TWE", "SUE", "ROB", "DAR"},
        {"", "", "AJS", "NIK"},
        {"", "", "POL", "CJR"}
}};

static const HiScoreTimes defaultHiScoreRaceTimes = {{
        {0x1428, 0x1A04, 0x1E00, 0x2328},
        {0x1590, 0x1C20, 0x22B0, 0x27D8},
        {0x2058, 0x2DB4, 0x3660, 0x3714},
        {0x1F68, 0x28C8, 0x31B0, 0x34BC},
        {0x2058, 0x2C4C, 0x3480, 0x3E1C},
        {0x1D88, 0x2814, 0x31B0, 0x396C},
        {0, 0, 0x2C10, 0x3264},
        {0, 0, 0x2D00, 0x3390}
}};

static const HiScoreTimes defaultHiScoreLapTimes = {{
        {0x09D8, 0x0870, 0x0744, 0x06CC},
        {0x0A8C, 0x0924, 0x0870, 0x07BC},
        {0x0FF0, 0x0F00, 0x0D5C, 0x0AC8},
        {0x0F78, 0x0D5C, 0x0C30, 0x0A50},
        {0x0FF0, 0x0E88, 0x0CE4, 0x0C30},
        {0x0E88, 0x0D20, 0x0C30, 0x0B40},
        {0, 0, 0x0AC8, 0x09D8},
        {0, 0, 0x0B04, 0x0A14}
}};

void ScoreTabInit(ConfigData *config)
{
    HiScoreNames names;
    HiScoreTimes raceTimes;
    HiScoreTimes lapTimes;
    int track;
    int trackSlot;
    int cls;

    names = defaultHiScoreNames;
    raceTimes = defaultHiScoreRaceTimes;
    lapTimes = defaultHiScoreLapTimes;
    if (config->initMagic != 0x25345327) {
        for (track = 0; track < 16; track++) {
            trackSlot = track;
            if (track >= 8) trackSlot = track - 8;
            for (cls = 0; cls < 4; cls++) {
                strcpy(hiScoreTable[track][cls].name, names.name[trackSlot][cls]);
                hiScoreTable[track][cls].time = raceTimes.time[trackSlot][cls];
                strcpy(hiScoreTable[track][cls].lapName, names.name[trackSlot][cls]);
                hiScoreTable[track][cls].lapTime = lapTimes.time[trackSlot][cls];
                hiScoreTable[track][cls].lapShip = 15;
                hiScoreTable[track][cls].ship = 15;
            }
        }
    }
}

void InitScoreTabData(HiScoreData *score)
{
    score->currChar = 'A';
    score->userEntry = 1;
    score->enterHs = 1;
    score->charX = 40;
    score->nameEntered[0] = 0;
    score->charVal = 0;
    score->numChars = 0;
    score->dispChar[0] = 0;
    score->toggleDisplay = 0;
    score->checkVal = 0;
    score->count = 0;
    score->padDelay = 0;
    score->totalTime = 0;
    score->flag = 0;
    score->displayFlag = 0;
    score->highScore = 0;
    score->confirm = 2;
    score->lConfirm = 0;
}


void CheckForHiScore(ConfigData *config, HiScoreData *score, ShipData *ships, int shipNum, u8 lap)
{
    int trackNum;
    int lapNum;
    int i;
    int shipNo;
    int record;
    u8 flag;
    ShipData *ship;
    ShipData *own;
    int *times;

    if (raceType == 1) trackNum = config->trackNum + 8;
    else trackNum = config->trackNum;
    score->totalTime = 0;
    if (lap == 1) {
        ship = &ships[shipNum];
        lapNum = ship->lap;
        if (lapNum == 2 || !score->flag) {
            if (ship->lapTimes[lapNum - 2] < hiScoreTable[trackNum][config->shipClass].lapTime) {
                shipNo = ownShip;
                own = &ships[shipNo];
                record = *(volatile s32 *)&own->lapTimes[own->lap - 2];
                flag = *(volatile u8 *)&score->flag | 1;
                score->lapPilot = shipNo;
                score->flag = flag;
                score->displayFlag = flag;
                score->lapTime = record;
            }
        } else if (ship->lapTimes[lapNum - 2] < score->lapTime) {
            shipNo = ownShip;
            score->lapTime = ships[shipNo].lapTimes[ships[shipNo].lap - 2];
            score->lapPilot = shipNo;
            myBestLap = score->lapTime;
            flag = score->flag | 1;
            score->flag = flag;
            score->displayFlag = flag;
        }
    } else {
        i = 0;
        if ((int)classLaps[config->shipClass] > i) {
            times = savedLapTimes;
            do {
                score->totalTime += *times;
                i++;
                times++;
            } while (i < classLaps[config->shipClass]);
        }
        shipNo = score->totalTime;
        if (shipNo < hiScoreTable[trackNum][config->shipClass].time) {
            record = ownShip;
            myBestRace = shipNo;
            flag = score->flag | 2;
            score->raceTime = shipNo;
            score->racePilot = record;
            score->flag = flag;
            score->displayFlag = flag;
        }
    }
}


void DisplayScoreTab(ConfigData *config, HiScoreData *score)
{
    int track;
    int cls;

    if (raceType == 1) track = config->trackNum + 8;
    else track = config->trackNum;
    cls = config->shipClass;
    if (score->flag != 0) {
        EnterHighScore(config, score);
    }
    if (score->displayFlag != 0) {
        CentreText("ENTER PLAYER NAME", 0x28, 0x28, 8, 0);
        DoButton(0x85, 0xD2, 1, msgSelect);
    } else {
        CentreText("PLAYER TRACK RECORDS", 0x28, 0x28, 8, 0);
    }
    if (score->displayFlag & 2) {
        if ((scoreTabFrame % 30) < 0x15) {
            AddText("NEW RACE RECORD", 0x28, 0x3C, 8, 1);
        }
        AddTime(score->raceTime, 0xA0, 0x50, 8, 1, 1);
    } else {
        AddText("RACE RECORD", 0x28, 0x3C, 8, 1);
        AddText(hiScoreTable[track][cls].name, 0x28, 0x50, 8, 1);
        AddTime(hiScoreTable[track][cls].time, 0xA0, 0x50, 8, 1, 1);
    }
    if (score->displayFlag & 1) {
        if ((scoreTabFrame % 30) < 0x15) {
            AddText("NEW LAP RECORD", 0x28, 0x64, 8, 1);
        }
        AddTime(score->lapTime, 0xA0, 0x78, 8, 1, 1);
    } else {
        AddText("LAP RECORD", 0x28, 0x64, 8, 1);
        AddText(hiScoreTable[track][cls].lapName, 0x28, 0x78, 8, 1);
        AddTime(hiScoreTable[track][cls].lapTime, 0xA0, 0x78, 8, 1, 1);
    }
    scoreTabFrame += 1;
}


void EnterHighScore(ConfigData *gameData, HiScoreData *hiScoreData)
{
    short charY = 185;
    short select;
    int scoreTrack;

    if (hiScoreData->numChars != 4 && hiScoreData->userEntry) {
        hiScoreData->dispChar[1] = 0;
        hiScoreData->dispChar[0] = hiScoreData->currChar;
        if (hiScoreData->userEntry) {
            if (hiScoreData->toggleDisplay % 3 == 0) {
                if (hiScoreData->charVal == 26 || hiScoreData->charVal == 27) {
                    AddDelEnd((unsigned char)(hiScoreData->charVal - 26), hiScoreData->charX, 185, 16, 1);
                } else {
                    AddText(hiScoreData->dispChar, hiScoreData->charX, 185, 16, 1);
                }
            } else {
                if (hiScoreData->charVal == 26 || hiScoreData->charVal == 27) {
                    AddDelEnd((unsigned char)(hiScoreData->charVal - 26), hiScoreData->charX, 185, 16, 0);
                } else {
                    AddText(hiScoreData->dispChar, hiScoreData->charX, 185, 16, 0);
                }
            }
        }
        hiScoreData->charVal = hiScoreData->currChar - 65;
        if ((keysDown & 0x20) && hiScoreData->padDelay == 0) {
            PlayNote(8, 255, 15, 15, 256);
            hiScoreData->padDelay = 5;
            hiScoreData->charVal++;
            if (hiScoreData->charVal > 27) {
                if (hiScoreData->numChars != 3) hiScoreData->charVal = 0;
                else hiScoreData->charVal = 26;
            }
        }
        if ((keysDown & 0x80) && hiScoreData->padDelay == 0) {
            PlayNote(8, 255, 15, 15, 256);
            hiScoreData->padDelay = 5;
            hiScoreData->charVal--;
            if (hiScoreData->numChars != 3) hiScoreData->checkVal = 0;
            else hiScoreData->checkVal = 26;
            if (hiScoreData->charVal < hiScoreData->checkVal) hiScoreData->charVal = 27;
        }
        if (hiScoreData->padDelay > 0) hiScoreData->padDelay--;
        select = GetMenuKey(5);
        if (select && hiScoreData->charVal == 26) {
            if (hiScoreData->numChars >= 1) {
                PlayNote(9, 255, 15, 15, 256);
                hiScoreData->numChars--;
                hiScoreData->charX -= fontGlyphs16[hiScoreData->nameEntered[hiScoreData->numChars] - 65].width;
                hiScoreData->nameEntered[hiScoreData->numChars] = 0;
                hiScoreData->charVal = 0;
            }
        } else {
            hiScoreData->currChar = hiScoreData->charVal + 65;
            if (select && hiScoreData->numChars != 4 && hiScoreData->charVal != 26 && hiScoreData->charVal != 27) {
                PlayNote(9, 255, 15, 15, 256);
                hiScoreData->nameEntered[hiScoreData->numChars] = hiScoreData->currChar;
                hiScoreData->nameEntered[hiScoreData->numChars + 1] = 0;
                hiScoreData->numChars++;
                hiScoreData->charX += fontGlyphs16[hiScoreData->charVal].width;
                if (hiScoreData->numChars == 3) {
                    hiScoreData->charVal = 27;
                    hiScoreData->currChar = 92;
                }
            } else if (select && hiScoreData->charVal == 27) {
                PlayNote(9, 255, 15, 15, 256);
                hiScoreData->nameEntered[hiScoreData->numChars] = hiScoreData->currChar;
                hiScoreData->nameEntered[hiScoreData->numChars + 1] = 0;
                hiScoreData->userEntry = 0;
                keysDown = 0;
            }
        }
    } else if (hiScoreData->enterHs) {
        hiScoreData->userEntry = 0;
        hiScoreData->nameEntered[hiScoreData->numChars] = 0;
        if (raceType == 1) scoreTrack = gameData->trackNum + 8;
        else scoreTrack = gameData->trackNum;
        if (hiScoreData->flag & 2) {
            strcpy(hiScoreTable[scoreTrack][gameData->shipClass].name, hiScoreData->nameEntered);
            hiScoreTable[scoreTrack][gameData->shipClass].time = hiScoreData->raceTime;
            hiScoreTable[scoreTrack][gameData->shipClass].ship = hiScoreData->racePilot;
        }
        if (hiScoreData->flag & 1) {
            strcpy(hiScoreTable[scoreTrack][gameData->shipClass].lapName, hiScoreData->nameEntered);
            hiScoreTable[scoreTrack][gameData->shipClass].lapTime = hiScoreData->lapTime;
            hiScoreTable[scoreTrack][gameData->shipClass].lapShip = hiScoreData->lapPilot;
        }
        hiScoreData->displayFlag = 0;
        hiScoreData->numChars = 0;
        hiScoreData->nameEntered[0] = 0;
        hiScoreData->charX = 80;
        hiScoreData->charVal = 0;
        hiScoreData->enterHs = 0;
    }
    CentreText("HALL OF FAME", 107, 20, 16, 1);
    if (hiScoreData->userEntry) {
        AddText(hiScoreData->nameEntered, 40, charY, 16, 1);
        AddTime(hiScoreData->totalTime, 165, charY, 16, 0, 1);
    }
    hiScoreData->toggleDisplay++;
    if (!hiScoreData->userEntry) {
        if (hiScoreData->count > 2) {
            DoButton(127, 210, 1, "CONTINUE");
            if (GetMenuKey(5)) {
                hiScoreData->charX = 40;
                hiScoreData->highScore = 0;
                hiScoreData->count = 0;
                hiScoreData->totalTime = 0;
                hiScoreData->flag = 0;
                gameData->complete = 1;
                gameData->highScore = 0;
                raceCompleteFlag = 1;
                return;
            }
        }
        hiScoreData->count++;
    }
}
