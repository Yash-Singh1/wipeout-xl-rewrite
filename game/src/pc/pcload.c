/* The PC version's loading screen (wadfile.c's VsyncFunc): the text of the PlayStation's, in the PC's font, with the
   bars and lines of the track's colour. progress.c's VsyncFunc draws it into each frame the load shows. */
#include <stdio.h>

#include "pc.h"
#include "../../include/portorig.h"

extern ConfigData *tempGameData;
extern int ownShip;
extern int scanPhase;
extern int waveIndex;
extern u_char barHeight[];

/* The track's name, place, length and height, and the colour of its bars. */
typedef struct TrackInfo {
    short track;
    const char *place, *length, *height;
    short red, green, blue;
} TrackInfo;

static const TrackInfo trackInfo[] = {
    {1, "CANADA", "3.2 KM", "74 M", 255, 0, 0},
    {2, "FRANCE", "3.5 KM", "179 M", 0, 255, 0},
    {6, "SOUTH PACIFIC", "5.1 KM", "97 M", 0, 0, 255},
    {7, "NORTH AMERICA", "4.0 KM", "82 M", 255, 255, 0},
    {8, "NEPAL", "4.3 KM", "158 M", 255, 0, 255},
    {13, "CHILE", "3.9 KM", "220 M", 0, 255, 255},
    {17, "BLACK SEA", "4.4 KM", "121 M", 128, 32, 255},
    {20, "GERMANY", "3.8 KM", "137 M", 0, 64, 0},
};

static const char *const className[] = {"VECTOR", "VENOM", "RAPIER", "PHANTOM"};
static const char *const teamName[] = {"AG SYSTEMS", "AURICOM", "QIREX", "FEISAR", "PIRANHA"};

/* A line of the screen's text, at the PC's size 8. */
static void Say(const char *text, int x, int y)
{
    PcPrint(text, x, y, 0, 0);
}

/* A row of random digits from firstX, as long as a random number of them to endBase (the PC's scanner). */
static void DigitRow(int y, int firstX, int endBase, int endMask)
{
    char digit[2] = {0, 0};
    int x;

    for (x = firstX; x < (newRand() & endMask) * 6 + endBase; x += 6) {
        digit[0] = (char)('0' + newRand() % 10);
        Say(digit, x, y);
    }
}

void PcLoadScreen(int frame, int percent)
{
    const TrackInfo *info = NULL;
    int tintRed = 0, tintGreen = 0, tintBlue = 0;
    int phase, x, i;

    PcAddText("LOADING", 0xA0, 0x56, 8, 0);
    PcAddJustifyNumber(percent / 100 % 10, 0x30, 0x62, 8, 0);
    PcAddJustifyNumber(percent / 10 % 10, 0x3E, 0x62, 8, 0);
    PcAddJustifyNumber(percent % 10, 0x4C, 0x62, 8, 0);
    Say("  LOAD          33    33    33", 0x20, 0x70);
    if (frame > 0x46) {
        Say("VENUE", 0x20, 0x78);
    }
    if (frame > 0x1E) {
        Say("LOCATION", 0x20, 0x7F);
    }
    if (frame > 0x32) {
        Say("LENGTH", 0x20, 0x86);
    }
    if (frame > 0x14) {
        Say("HEIGHT", 0x20, 0x8D);
    }
    if (frame > 0x28) {
        Say("CLASS", 0x20, 0x94);
    }
    if (frame > 0x3C) {
        Say("TEAM", 0x20, 0x9B);
    }
    if (tempGameData->shipClass >= 0 && tempGameData->shipClass < 4) {
        Say(className[tempGameData->shipClass], 0x50, 0x94);
    }
    if (ownShip >= 0 && ownShip <= 14) {
        Say(teamName[ownShip / 3], 0x50, 0x9B);
    }
    Say(PortRaceTrackName(tempGameData->actTrackNum), 0x50, 0x78);
    for (i = 0; i < (int)(sizeof(trackInfo) / sizeof(trackInfo[0])); i++) {
        if (trackInfo[i].track == tempGameData->actTrackNum) {
            info = &trackInfo[i];
        }
    }
    if (info) {
        Say(info->place, 0x50, 0x7F);
        Say(info->length, 0x50, 0x86);
        Say(info->height, 0x50, 0x8D);
        tintRed = info->red;
        tintGreen = info->green;
        tintBlue = info->blue;
    }
    DigitRow(0x62, 0xA0, 0xB4, 0xF);
    DigitRow(0x68, 0xA0, 0xB4, 0xF);
    DigitRow(0x70, 0xA0, 0xB4, 0xF);
    DigitRow(0x78, 0xA2, 0xB4, 0xF);
    DigitRow(0xA8, 0x102, 0x120, 5);
    Say("TM", 0xEE, 0x56);
    DrawBox(0x9E, 0x54, 0x60, 0xB, 0xFF, 0xFF, 0xFF);
    DrawBox(0xA0, 0x76, 0x80, 0x30, 0xFF, 0xFF, 0xFF);
    DrawBox(0x100, 0xA6, 0x40, 8, 0xFF, 0xFF, 0xFF);
    DrawBox(0xA0, 0xA2, 0xA0, 4, 0xFF, 0xFF, 0xFF);
    DrawLine(0xA0, 0x82, 0x120, 0x82, 0xC0, 0xC0, 0xC0);
    DrawLine(0xA0, 0x7E, 0x120, 0x7E, 0xC0, 0xC0, 0xC0);
    DrawLine(0xA0, 0x9E, 0x120, 0x9E, 0xC0, 0xC0, 0xC0);
    for (x = 0x86; x < 0x9E; x += 8) {
        DrawLine(0x100, x, 0x120, x, 0xC0, 0xC0, 0xC0);
    }
    for (x = 0xB0; x < 0x120; x += 0x10) {
        DrawLine(x, 0xA6, x, 0x7E, 0xC0, 0xC0, 0xC0);
    }
    for (x = 0xA8; x < 0x120; x += 0x10) {
        DrawLine(x, 0xA2, x, 0xA6, 0xC0, 0xC0, 0xC0);
    }
    DrawLine(0x20, 0x6E, 0x110, 0x6E, 0xFF, 0xFF, 0xFF);
    DrawLine(0x110, 0x76, 0x110, 0x7E, 0xFF, 0xFF, 0xFF);
    DrawLine(0x20, 0x76, 0xA0, 0x76, 0xFF, 0xFF, 0xFF);
    DrawLine(0x20, 0x70, 0x20, 0x70, 0xFF, 0xFF, 0xFF);
    DrawLine(0x24, 0x70, 0x20, 0x74, 0xFF, 0xFF, 0xFF);
    DrawLine(0x24, 0x74, 0x24, 0x74, 0xFF, 0xFF, 0xFF);
    DrawLine(0xE0, 0x86, 0x100, 0x86, 0xC0, 0xC0, 0xC0);
    for (x = 0xE8; x < 0x120; x += 0x10) {
        DrawLine(x, 0x86, x, 0x82, 0xC0, 0xC0, 0xC0);
    }
    /* the scanning bars */
    waveIndex = 0;
    phase = scanPhase;
    scanPhase = (scanPhase - 10) & 0xFF;
    for (x = 0xA2; x < 0x100; x += 2) {
        phase = (phase + 5) & 0xFF;
        DrawLine(x, 0xA1, x, barHeight[waveIndex] + 0x78, (u_char)(tintRed * phase >> 8),
                 (u_char)(phase * tintGreen >> 8), (u_char)(tintBlue * phase >> 8));
        switch (newRand() & 3) {
        case 1:
            if (barHeight[waveIndex] > 8) {
                barHeight[waveIndex] -= 2;
            }
            break;
        case 2:
            if (barHeight[waveIndex] < 0x28) {
                barHeight[waveIndex] += 2;
            }
            break;
        }
        waveIndex++;
    }
    PcDrawEndFrame(); /* (the load's frames are not Swap's, which would link the 2D) */
}
