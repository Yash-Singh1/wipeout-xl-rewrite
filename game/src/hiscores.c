#include "functions.h"
#include "../../platform/controls.h" /* Port: a name typed on the keyboard */
#include "pad.h"
#include "racectrl.h"
#include "hiscores.h"
#include "pc/pc.h"
#include "include_asm_g8.h"

/* $gp-relative globals are tentative definitions; the rest are extern. */
extern s32 scoreTabFrame;

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

/* The European program's lap records, in 50 Hz frames: not a plain rescaling of the NTSC ones. */
static const HiScoreTimes defaultHiScoreLapTimesPal = {{
        {0x08FC, 0x07D0, 0x06D6, 0x0672},
        {0x0992, 0x0866, 0x07D0, 0x073A},
        {0x0E10, 0x0D48, 0x0BEA, 0x09C4},
        {0x0DAC, 0x0BEA, 0x0AF0, 0x0960},
        {0x0E10, 0x0CE4, 0x0B86, 0x0AF0},
        {0x0CE4, 0x0BB8, 0x0AF0, 0x0A28},
        {0, 0, 0x09C4, 0x08FC},
        {0, 0, 0x09F6, 0x092E}
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
    lapTimes = portPal ? defaultHiScoreLapTimesPal : defaultHiScoreLapTimes;
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
    score->pendingRecords = 0;
    score->displayedRecords = 0;
    score->nameEntryActive = 0;
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

    if (raceType == RACE_TIME_TRIAL) trackNum = config->trackNum + 8;
    else trackNum = config->trackNum;
    score->totalTime = 0;
    if (lap == 1) {
        ship = &ships[shipNum];
        lapNum = ship->lap;
        if (lapNum == 2 || !score->pendingRecords) {
            if (PortOriginalTrack() < 0 && ship->lapTimes[lapNum - 2] < hiScoreTable[trackNum][config->shipClass].lapTime) { /* Port: an original track's times are no records of Talon's Reach */
                shipNo = ownShip;
                own = &ships[shipNo];
                record = own->lapTimes[own->lap - 2];
                flag = score->pendingRecords | 1;
                score->lapPilot = shipNo;
                score->pendingRecords = flag;
                score->displayedRecords = flag;
                score->lapTime = record;
            }
        } else if (ship->lapTimes[lapNum - 2] < score->lapTime) {
            shipNo = ownShip;
            score->lapTime = ships[shipNo].lapTimes[ships[shipNo].lap - 2];
            score->lapPilot = shipNo;
            myBestLap = score->lapTime;
            flag = score->pendingRecords | 1;
            score->pendingRecords = flag;
            score->displayedRecords = flag;
        }
    } else {
        for (i = 0; i < classLaps[config->shipClass]; i++) {
            score->totalTime += savedLapTimes[i];
        }
        if (PortOriginalTrack() < 0 && score->totalTime < hiScoreTable[trackNum][config->shipClass].time) { /* Port: (as the lap's) */
            myBestRace = score->totalTime;
            flag = score->pendingRecords | 2;
            score->raceTime = score->totalTime;
            score->racePilot = ownShip;
            score->pendingRecords = flag;
            score->displayedRecords = flag;
        }
    }
}


void DisplayScoreTab(ConfigData *config, HiScoreData *score)
{
    int track;
    int cls;

    if (raceType == RACE_TIME_TRIAL) track = config->trackNum + 8;
    else track = config->trackNum;
    cls = config->shipClass;
    if (score->pendingRecords != 0) {
        EnterHighScore(config, score);
    }
    if (score->displayedRecords != 0) {
        CentreText("ENTER PLAYER NAME", 0x28, 0x28, 8, 0);
        DoButton(0x85, RESULT_BUTTON_Y, 1, msgSelect);
    } else {
        CentreText("PLAYER TRACK RECORDS", 0x28, 0x28, 8, 0);
    }
    if (score->displayedRecords & 2) {
        if ((scoreTabFrame % 30) < 0x15) {
            AddText("NEW RACE RECORD", 0x28, 0x3C, 8, 1);
        }
        AddTime(score->raceTime, 0xA0, 0x50, 8, 1, 1);
    } else {
        AddText("RACE RECORD", 0x28, 0x3C, 8, 1);
        AddText(hiScoreTable[track][cls].name, 0x28, 0x50, 8, 1);
        AddTime(hiScoreTable[track][cls].time, 0xA0, 0x50, 8, 1, 1);
    }
    if (score->displayedRecords & 1) {
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


/* Port: the keyboard types the name being entered (EnterHighScore, EnterName), as the PC version's did: a letter is
   chosen at once, Backspace deletes, and Enter ends the name typed (after letters picked with the arrows, Enter
   takes the cursor's, as the console's Cross does). What the key typed chooses of the letter picker's (0-25 a
   letter, 26 delete, 27 end), or -1: `letters` the name has, `picked` the picker moved this frame, `current` its
   choice. */
int PortNameTyped(int letters, int picked, int current)
{
    static int typing; /* (the name's last change was typed) */
    int key;

    PortNameEntry(PORT_NAME_LETTERS | PORT_NAME_ENTER);
    if (picked) {
        typing = 0;
    }
    key = PortNameKey();
    if (key >= 'A' && key <= 'Z') {
        if (letters >= 3) {
            return -1; /* (three letters at most) */
        }
        typing = 1;
        return key - 'A';
    }
    if (key == '\b') {
        typing = 1;
        return 26;
    }
    if (key == '\n') {
        return typing ? 27 : current;
    }
    return -1;
}

extern POLY_FT4 quadPool[];
extern int quadIndex;

void EnterHighScore(ConfigData *gameData, HiScoreData *hiScoreData)
{
    short charY = 185;
    short select;
    int scoreTrack;
    int charX = hiScoreData->charX;
    POLY_FT4 *inPlace = &quadPool[quadIndex]; /* Port: the name being entered stays under the heading in widescreen,
                                                 not at the screen's left edge (PortHudCentred) */

    if (PortPcData()) {
        PcCentred(1);
    }
    if (PortPcData()) {
        /* Port: the letter being chosen after the name as the PC's font spaces it */
        charX = 40 + PcTextWidth((const char *)hiScoreData->nameEntered, 8);
    }
    if (hiScoreData->numChars != 4 && hiScoreData->userEntry) {
        hiScoreData->dispChar[1] = 0;
        hiScoreData->dispChar[0] = hiScoreData->currChar;
        if (hiScoreData->userEntry) {
            if (hiScoreData->toggleDisplay % 3 == 0) {
                if (hiScoreData->charVal == 26 || hiScoreData->charVal == 27) {
                    AddDelEnd((unsigned char)(hiScoreData->charVal - 26), charX, 185, 16, 1);
                } else {
                    AddText(hiScoreData->dispChar, charX, 185, 16, 1);
                }
            } else {
                if (hiScoreData->charVal == 26 || hiScoreData->charVal == 27) {
                    AddDelEnd((unsigned char)(hiScoreData->charVal - 26), charX, 185, 16, 0);
                } else {
                    AddText(hiScoreData->dispChar, charX, 185, 16, 0);
                }
            }
        }
        hiScoreData->charVal = hiScoreData->currChar - 65;
        if ((keysDown & PAD_RIGHT) && hiScoreData->padDelay == 0) {
            PlayNote(8, 255, 15, 15, 256);
            hiScoreData->padDelay = 5;
            hiScoreData->charVal++;
            if (hiScoreData->charVal > 27) {
                if (hiScoreData->numChars != 3) hiScoreData->charVal = 0;
                else hiScoreData->charVal = 26;
            }
        }
        if ((keysDown & PAD_LEFT) && hiScoreData->padDelay == 0) {
            PlayNote(8, 255, 15, 15, 256);
            hiScoreData->padDelay = 5;
            hiScoreData->charVal--;
            if (hiScoreData->numChars != 3) hiScoreData->checkVal = 0;
            else hiScoreData->checkVal = 26;
            if (hiScoreData->charVal < hiScoreData->checkVal) hiScoreData->charVal = 27;
        }
        if (hiScoreData->padDelay > 0) hiScoreData->padDelay--;
        select = GetMenuKey(MENU_KEY_SELECT);
        {
            int typed = PortNameTyped(hiScoreData->numChars, (keysDown & (PAD_RIGHT | PAD_LEFT)) != 0, hiScoreData->charVal);

            if (typed >= 0) {
                hiScoreData->charVal = typed; /* Port: typed on the keyboard */
                select = 1;
            }
        }
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
        if (raceType == RACE_TIME_TRIAL) scoreTrack = gameData->trackNum + 8;
        else scoreTrack = gameData->trackNum;
        if (hiScoreData->pendingRecords & 2) {
            strcpy(hiScoreTable[scoreTrack][gameData->shipClass].name, hiScoreData->nameEntered);
            hiScoreTable[scoreTrack][gameData->shipClass].time = hiScoreData->raceTime;
            hiScoreTable[scoreTrack][gameData->shipClass].ship = hiScoreData->racePilot;
        }
        if (hiScoreData->pendingRecords & 1) {
            strcpy(hiScoreTable[scoreTrack][gameData->shipClass].lapName, hiScoreData->nameEntered);
            hiScoreTable[scoreTrack][gameData->shipClass].lapTime = hiScoreData->lapTime;
            hiScoreTable[scoreTrack][gameData->shipClass].lapShip = hiScoreData->lapPilot;
        }
        hiScoreData->displayedRecords = 0;
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
    PortHudCentred(inPlace, &quadPool[quadIndex]);
    if (PortPcData()) {
        PcCentred(0);
    }
    hiScoreData->toggleDisplay++;
    if (!hiScoreData->userEntry) {
        if (hiScoreData->count > 2) {
            DoButton(127, RESULT_BUTTON_Y, 1, "CONTINUE");
            if (GetMenuKey(MENU_KEY_SELECT)) {
                hiScoreData->charX = 40;
                hiScoreData->nameEntryActive = 0;
                hiScoreData->count = 0;
                hiScoreData->totalTime = 0;
                hiScoreData->pendingRecords = 0;
                gameData->complete = 1;
                gameData->highScore = 0;
                raceCompleteFlag = 1;
                return;
            }
        }
        hiScoreData->count++;
    }
}
