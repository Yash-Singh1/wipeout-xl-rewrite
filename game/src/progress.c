#include "functions.h"
#include "ghost.h"
#include "include_asm_g8.h"

/* $gp-relative globals are tentative definitions; the rest are extern. */
s32            wadHandle;
u8            *wadCache;
volatile s32   wadRemaining;
s32            wadSize;
volatile s32   bufat;
s32            loadPercent;
u_short        loadFrame;
s32            scanPhase;
u8             vsyncBusy;
s32            lastWadCurrent;
s32            loadTicks;
s32            waveIndex;
s32           *particleLife;
s32           *particleX;
s32           *particleY;
s32           *particleVelX;
s32           *particleVelY;
POLY_F3       *particlePolys;
LINE_F2       *linePrims;
s32            lineat;
s32            loopat;
WADFILE       *currentWad;
BLK_FILL      *screenFillTop __asm__("screenFills");

extern u8              barHeight[];
extern BLK_FILL       *screenFills[];
extern BLK_FILL       *screenFill1;
extern char            numberText[];
extern char            singleDigitFormat[];
extern u8              loadScreenFooter[];
extern Skeleton       *upperSkeleton;
extern Skeleton       *residentSkeletons[];
extern Object         *shipShapeTable[];
extern u_long         *OT[];
extern u_long         *OT2[];
extern short           CurrentScreen;
extern int            quadIndex;
extern ConfigData     *tempGameData;
extern int             ownShip;
extern void          *reserveHeap;

/* Menu strings, drawn with Print/AddText. */
extern u_char loadInstructionLine0[];
extern u_char loadInstructionLine1[];
extern u_char shipTeamNames0[];
extern u_char track6DescriptionLine0[];
extern u_char track7DescriptionLine0[];
extern u_char track17DescriptionLine0[];
extern u_char loadScreenTitle[];
extern u_char loadInstructionLine2[];
extern u_char loadInstructionLine3[];
extern u_char loadInstructionLine4[];
extern u_char loadInstructionLine5[];
extern u_char loadInstructionLine6[];
extern u_char classVectorLabel[];
extern u_char classVenomLabel[];
extern u_char classRapierLabel[];
extern u_char classPhantomLabel[];
extern u_char shipTeamNames1[];
extern u_char shipTeamNames2[];
extern u_char shipTeamNames3[];
extern u_char shipTeamNames4[];
extern u_char track1DescriptionLine0[];
extern u_char track1DescriptionLine1[];
extern u_char track1DescriptionLine2[];
extern u_char track2DescriptionLine0[];
extern u_char track2DescriptionLine1[];
extern u_char track2DescriptionLine2[];
extern u_char track6DescriptionLine1[];
extern u_char track6DescriptionLine2[];
extern u_char track7DescriptionLine1[];
extern u_char track7DescriptionLine2[];
extern u_char track8DescriptionLine0[];
extern u_char track8DescriptionLine1[];
extern u_char track8DescriptionLine2[];
extern u_char track13DescriptionLine0[];
extern u_char track13DescriptionLine1[];
extern u_char track13DescriptionLine2[];
extern u_char track17DescriptionLine1[];
extern u_char track17DescriptionLine2[];
extern u_char track20DescriptionLine0[];
extern u_char track20DescriptionLine1[];
extern u_char track20DescriptionLine2[];

void RefreshCache(void)
{
    iso_read(wadCache, 0xC, wadHandle);
}


int WDopen(int fileNo, int unused1, int unused2)
{
    s32 size;

    wadCache = (u8 *)recordBuffer;
    wadHandle = iso_open((char *)fileNo, 0);
    bufat = 0;
    size = iso_filesize(wadHandle);
    wadSize = size;
    wadRemaining = size;
    RefreshCache();
    return wadHandle;
}


void WDcloseFile() __asm__("iso_close");

void WDclose(void)
{
    WDcloseFile();
}


void WDread(int unused, u_char *dest, int count)
{
    int index;
    int remaining;
    int total;
    int progress;
    u_char *source;

    while (count > 0) {
        index = bufat;
        if (index < 0x6000) {
            source = &wadCache[index];
        } else {
            RefreshCache();
            source = wadCache;
            bufat = 0;
        }
        *dest = *source;
        bufat += 1;
        remaining = wadRemaining;
        total = wadSize;
        progress = (remaining * 100) / total;
        wadRemaining = remaining - 1;
        loadPercent = 100 - progress;
        count--;
        do { dest++; } while (0);
    }
}


void Insert2DParticle(int x, int y)
{
    particleX[loopat] = x << 7;
    particleY[loopat] = y << 7;
    particleVelX[loopat] = newRand() % 512 - 256;
    particleVelY[loopat] = 32 - newRand() % 128;
    particleLife[loopat] = newRand() % 128;
    loopat = (loopat + 1) % 128;
}


void Update2DParticle(void)
{
    int i;

    for (i = 0; i < 128; i++) {
        if (particleLife[i] > 0) {
            particleLife[i] -= 3;
            particlePolys[i].r0 = particleLife[i] * 2;
            particlePolys[i].g0 = particleLife[i] * 2;
            particlePolys[i].b0 = 0;
            particleX[i] += particleVelX[i];
            particleY[i] += particleVelY[i];
            particleVelY[i] += 1;
            particlePolys[i].x0 = particleX[i] >> 7;
            particlePolys[i].y0 = particleY[i] >> 7;
            particlePolys[i].x1 = (particleX[i] >> 7) + 1;
            particlePolys[i].y1 = particleY[i] >> 7;
            particlePolys[i].x2 = particleX[i] >> 7;
            particlePolys[i].y2 = (particleY[i] >> 7) + 1;
            AddPrim(OT[CurrentScreen], &particlePolys[i]);
        }
    }
}


void Init2DParticle(void)
{
    int i;

    particlePolys = DAlloc(reserveHeap, 0xA00);
    particleX = DAlloc(reserveHeap, 0x200);
    particleY = DAlloc(reserveHeap, 0x200);
    particleVelX = DAlloc(reserveHeap, 0x200);
    i = 0;
    particleVelY = DAlloc(reserveHeap, 0x200);
    particleLife = DAlloc(reserveHeap, 0x200);
    do {
        SetPolyF3(&particlePolys[i]);
        particleLife[i] = 0;
        particleX[i] = (newRand() % 320) << 7;
        particleY[i] = (newRand() % 240) << 7;
        particleVelX[i] = (newRand() % 9) - 4;
        particleVelX[i] = -(newRand() % 4);
        i += 1;
    } while (i < 0x80);
}


void Close2DParticle(void)
{
    DFree(reserveHeap, (char *)particlePolys);
    DFree(reserveHeap, (char *)particleX);
    DFree(reserveHeap, (char *)particleY);
    DFree(reserveHeap, (char *)particleVelX);
    DFree(reserveHeap, (char *)particleVelY);
    DFree(reserveHeap, (char *)particleLife);
}


void InitPop(void)
{
    int      i;
    s32      dirty;
    Object **shape;

    loadFrame = 0;
    linePrims = DAlloc(reserveHeap, 0x2000);
    i = 0;
    dirty = 1;
    shape = shipShapeTable;
    SetSkeletonPosition(residentSkeletons[0], 0, 0, 0);
    SetSkeletonDirection(residentSkeletons[0], 0, 0, 0);
    SetSkeletonPosition(upperSkeleton, 0, 0, 0);
    SetSkeletonDirection(upperSkeleton, 0, 0, 0);
    vsyncBusy = 0;
    do {
        i += 1;
        InitMatrix(&(*shape)->skeleton->relative);
        InitMatrix(&(*shape)->skeleton->absolute);
        (*shape)->skeleton->update = dirty;
        shape += 1;
    } while (i < 0xF);
    i = 0;
    Init2DParticle();
    do {
        barHeight[i] = ((s32)(rsin(i * 0x10) * 0xC) >> 0xC) + 0x14;
        i += 1;
    } while (i < 0x80);
    scanPhase = 0;
    loadPercent = 0;
    screenFillTop = BlockFill(0, 0x50, 0x140, 0x64, 0, 0, 0x2E);
    screenFill1 = BlockFill(0, 0x150, 0x140, 0x64, 0, 0, 0x2E);
}


void DrawLine(int x0, int y0, int x1, int y1, u_char r, u_char g, u_char b)
{
    LINE_F2 *line;

    if (lineat <= 0x100) {
        SetLineF2(&linePrims[lineat]);
        line = (LINE_F2 *)((lineat * sizeof(LINE_F2)) + (s32)linePrims);
        line->r0 = r;
        line->x0 = x0;
        line->y0 = y0;
        line->x1 = x1;
        line->y1 = y1;
        linePrims[lineat].g0 = g;
        linePrims[lineat].b0 = b;
        AddPrim(OT[CurrentScreen], &linePrims[lineat]);
        lineat++;
        if (lineat >= 0x100) {
            lineat = 0;
        }
    }
}

void DrawBox(int x, int y, int w, int h, u_char r, u_char g, u_char b)
{
    int right;
    int bottom;

    right = x + w;
    DrawLine(x, y, right, y, r, g, b);
    bottom = y + h;
    DrawLine(right, y, right, bottom, r, g, b);
    DrawLine(right, bottom, x, bottom, r, g, b);
    DrawLine(x, bottom, x, y, r, g, b);
}

void VsyncFunc(void)
{
    u_char      *shipName;
    short        x;
    short        tintR;
    short        tintG;
    short        tintB;
    short        nextY;
    short        nextX;
    short        phase;
    int         digitX1;
    int         digitX2;
    int         digitX3;
    int         digitX4;
    int         digitX5;
    int         digit1;
    int         digit2;
    int         digit3;
    int         digit4;
    int         digit5;
    int         bufferIndex;
    u_long      *orderTable;
    u_short      startCount;
    u_char      *heightPtrUp;
    u_char      *heightPtrDown;
    u_char       trackType;
    u_char       heightUp;
    u_char       heightDown;
    int         action;
    WADFILE     *wad;

    if (vsyncBusy == 0) {
        bufferIndex = CurrentScreen;
        startCount = loadFrame;
        wad = currentWad;
        orderTable = OT[bufferIndex];
        vsyncBusy = 1;
        loadFrame = startCount + 1;
        ClearOTagR(orderTable, 0x2000);
        ClearOTagR(OT2[CurrentScreen], 0x640);
        DrawSync(0);
        SfxDoSfx();
        quadIndex = CurrentScreen * 0x12C;
        AddText(loadScreenTitle, 0xA0, 0x56, 8, 1);
        if (lastWadCurrent != wad->current) {
            loadTicks = 0;
            lastWadCurrent = wad->current;
        }
        AddJustifyNumber((loadPercent / 100) % 10, 0x30, 0x61, 0xC, 1);
        AddJustifyNumber((loadPercent / 10) % 10, 0x42, 0x61, 0xC, 1);
        AddJustifyNumber(loadPercent % 10, 0x54, 0x61, 0xC, 1);
        Print(loadInstructionLine0, 0x20, 0x70, 8);
        if ((short)loadFrame >= 0x47) {
            Print(loadInstructionLine2, 0x20, 0x78, 8);
        }
        if ((short)loadFrame >= 0x1F) {
            Print(loadInstructionLine1, 0x20, 0x7F, 8);
        }
        if ((short)loadFrame >= 0x33) {
            Print(loadInstructionLine3, 0x20, 0x86, 8);
        }
        if ((short)loadFrame >= 0x15) {
            Print(loadInstructionLine4, 0x20, 0x8D, 8);
        }
        if ((short)loadFrame >= 0x29) {
            Print(loadInstructionLine5, 0x20, 0x94, 8);
        }
        if ((short)loadFrame >= 0x3D) {
            Print(loadInstructionLine6, 0x20, 0x9B, 8);
        }
        if (tempGameData->shipClass == 0) {
            Print(classVectorLabel, 0x50, 0x94, 8);
        }
        if (tempGameData->shipClass == 1) {
            Print(classVenomLabel, 0x50, 0x94, 8);
        }
        if (tempGameData->shipClass == 2) {
            Print(classRapierLabel, 0x50, 0x94, 8);
        }
        if (tempGameData->shipClass == 3) {
            Print(classPhantomLabel, 0x50, 0x94, 8);
        }
        if ((u_int)ownShip < 3U) {
            shipName = shipTeamNames0;
            goto printShipName;
        }
        if ((u_int)(ownShip - 3) < 3U) {
            shipName = shipTeamNames1;
            goto printShipName;
        }
        if ((u_int)(ownShip - 6) < 3U) {
            shipName = shipTeamNames2;
            goto printShipName;
        }
        if ((u_int)(ownShip - 9) < 3U) {
            shipName = shipTeamNames3;
            goto printShipName;
        }
        if ((u_int)(ownShip - 0xC) < 3U) {
            shipName = shipTeamNames4;
printShipName:
            Print(shipName, 0x50, 0x9B, 8);
        }
        Print((u_char *)GetTrackName(tempGameData->actTrackNum, 0, 0, 0), 0x50, 0x78, 8);
        trackType = tempGameData->actTrackNum;
        switch (trackType) {
        case 1:
            Print(track1DescriptionLine0, 0x50, 0x7F, 8);
            Print(track1DescriptionLine1, 0x50, 0x86, 8);
            Print(track1DescriptionLine2, 0x50, 0x8D, 8);
            tintR = 0xFF;
            tintG = 0;
            tintB = 0;
            break;
        case 2:
            Print(track2DescriptionLine0, 0x50, 0x7F, 8);
            Print(track2DescriptionLine1, 0x50, 0x86, 8);
            Print(track2DescriptionLine2, 0x50, 0x8D, 8);
            tintR = 0;
            tintG = 0xFF;
            tintB = 0;
            break;
        case 6:
            Print(track6DescriptionLine0, 0x50, 0x7F, 8);
            Print(track6DescriptionLine1, 0x50, 0x86, 8);
            Print(track6DescriptionLine2, 0x50, 0x8D, 8);
            tintR = 0;
            tintG = 0;
            tintB = 0xFF;
            break;
        case 7:
            Print(track7DescriptionLine0, 0x50, 0x7F, 8);
            Print(track7DescriptionLine1, 0x50, 0x86, 8);
            Print(track7DescriptionLine2, 0x50, 0x8D, 8);
            tintR = 0xFF;
            tintG = 0xFF;
            tintB = 0;
            break;
        case 8:
            Print(track8DescriptionLine0, 0x50, 0x7F, 8);
            Print(track8DescriptionLine1, 0x50, 0x86, 8);
            Print(track8DescriptionLine2, 0x50, 0x8D, 8);
            tintR = 0xFF;
            tintG = 0;
            tintB = 0xFF;
            break;
        case 13:
            Print(track13DescriptionLine0, 0x50, 0x7F, 8);
            Print(track13DescriptionLine1, 0x50, 0x86, 8);
            Print(track13DescriptionLine2, 0x50, 0x8D, 8);
            tintR = 0;
            tintG = 0xFF;
            tintB = 0xFF;
            break;
        case 17:
            Print(track17DescriptionLine0, 0x50, 0x7F, 8);
            Print(track17DescriptionLine1, 0x50, 0x86, 8);
            Print(track17DescriptionLine2, 0x50, 0x8D, 8);
            tintR = 0x80;
            tintG = 0x20;
            tintB = 0xFF;
            break;
        case 20:
            Print(track20DescriptionLine0, 0x50, 0x7F, 8);
            Print(track20DescriptionLine1, 0x50, 0x86, 8);
            Print(track20DescriptionLine2, 0x50, 0x8D, 8);
            tintR = 0;
            tintG = 0x40;
            tintB = 0;
            break;
        }
        x = 0xA0;
        while ((digitX1 = x), digitX1 < ((((newRand() & 0xF) * 6) + 0xB4))) {
            x += 6;
            digit1 = newRand();
            sprintf(numberText, singleDigitFormat, digit1 % 10);
            Print((u_char *)numberText, digitX1, 0x62, 8);
        }
        x = 0xA0;
        while ((digitX2 = x), digitX2 < ((((newRand() & 0xF) * 6) + 0xB4))) {
            x += 6;
            digit2 = newRand();
            sprintf(numberText, singleDigitFormat, digit2 % 10);
            Print((u_char *)numberText, digitX2, 0x68, 8);
        }
        x = 0xA0;
        while ((digitX3 = x), digitX3 < ((((newRand() & 0xF) * 6) + 0xB4))) {
            x += 6;
            digit3 = newRand();
            sprintf(numberText, singleDigitFormat, digit3 % 10);
            Print((u_char *)numberText, digitX3, 0x70, 8);
        }
        x = 0xA2;
        while ((digitX4 = x), digitX4 < ((((newRand() & 0xF) * 6) + 0xB4))) {
            x += 6;
            digit4 = newRand();
            sprintf(numberText, singleDigitFormat, digit4 % 10);
            Print((u_char *)numberText, digitX4, 0x78, 8);
        }
        x = 0x102;
        while ((digitX5 = x), digitX5 < (((((newRand() & 5) + 5) * 6) + 0xEA))) {
            x += 6;
            digit5 = newRand();
            sprintf(numberText, singleDigitFormat, digit5 % 10);
            Print((u_char *)numberText, digitX5, 0xA8, 8);
        }
        Print(loadScreenFooter, 0xEE, 0x56, 8);
        DrawBox(0x9E, 0x54, 0x60, 0xB, 0xFF, 0xFF, 0xFF);
        DrawBox(0xA0, 0x76, 0x80, 0x30, 0xFF, 0xFF, 0xFF);
        DrawBox(0x100, 0xA6, 0x27, 8, 0xFF, 0xFF, 0xFF);
        DrawBox(0xA0, 0xA2, 0x87, 4, 0xFF, 0xFF, 0xFF);
        DrawLine(0xA0, 0x82, 0x120, 0x82, 0xC0, 0xC0, 0xC0);
        DrawLine(0xA0, 0x7E, 0x120, 0x7E, 0xC0, 0xC0, 0xC0);
        DrawLine(0xA0, 0x9E, 0x120, 0x9E, 0xC0, 0xC0, 0xC0);
        x = 0x86;
        do {
            DrawLine(0x100, x, 0x120, x, 0xC0, 0xC0, 0xC0);
            nextY = x + 8;
            x = nextY;
        } while (nextY < 0x9E);
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
        x = 0xA2;
        waveIndex = 0;
        phase = scanPhase;
        scanPhase = (scanPhase - 0xA) & 0xFF;
        do {
            phase = (phase + 5) & 0xFF;
            DrawLine((int)x, 0xA1, (int)x, barHeight[waveIndex] + 0x78, ((int)(phase * tintR) >> 8) & 0xFF, ((int)(phase * tintG) >> 8) & 0xFF, ((int)(phase * tintB) >> 8) & 0xFF);
            action = newRand() & 3;
            switch (action) {
            case 1:
                heightPtrUp = &barHeight[waveIndex];
                heightUp = *heightPtrUp;
                if (heightUp >= 9U) {
                    *heightPtrUp = heightUp - 2;
                }
                break;
            case 2:
                heightPtrDown = &barHeight[waveIndex];
                heightDown = *heightPtrDown;
                if (heightDown < 0x28U) {
                    *heightPtrDown = heightDown + 2;
                }
                break;
            }
            nextX = x + 2;
            x = nextX;
            waveIndex += 1;
        } while (nextX < 0x100);
        AddPrim(OT[CurrentScreen] + 0x1FFF, screenFills[CurrentScreen]);
        CallbackSwap();
        vsyncBusy = 0;
    }
}
