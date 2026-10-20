#include "functions.h"
#include "object2.h"

extern HeapBlock *heap;
extern DRAWENV DrawEnv0;
extern DRAWENV DrawEnv1;
extern DISPENV DispEnv0;
extern DISPENV DispEnv1;
extern DB doubleBuffer[];
extern short CurrentScreen;
extern u_long *OT[];
extern u_long *OT2[];
extern u_long otBuffer0[];
extern u_long otBuffer1[];
extern u_long ot2Buffer0[];
extern u_long ot2Buffer1[];
extern char *primBuffers[];
extern char PrimitiveBuffer0[];
extern char PrimitiveBuffer1[];
extern PrimPtr nextPrim;
extern ConfigData *gameData;
extern HeapBlock *reserveHeap;
extern u_short dispOffX0;
extern u_short dispOffY0;
extern u_short dispOffX1;
extern u_short dispOffY1;
extern u_char activateOT2;
extern u_char gameType;
extern short fastFrame;
extern int primsDrawn;
extern char errorString[];

void SetGraphDebug(int level);
void InitGeom(void);
void SetVideoMode(int mode);
void SfxDoSfx(void);
void DrawPrim(void *prim);
void SetBlockFill(BLK_FILL *prim);

char  menuSendFlags;
short gpuWarning;
short cpuWarning;
int   max;
POLY_F4 *ClearScreen(short red, short green, short blue)
{
    POLY_F4 *quad;

    quad = DAlloc(heap, sizeof(POLY_F4));
    if (!quad) {
        return 0;
    }
    SetPolyF4(quad);
    quad->r0 = red;
    quad->g0 = green;
    quad->b0 = blue;
    quad->x0 = 0;
    quad->y0 = 0;
    quad->x1 = 255;
    quad->y1 = 0;
    quad->x2 = 0;
    quad->y2 = 239;
    quad->x3 = 255;
    quad->y3 = 239;
    return quad;
}


void InitSystem(void)
{
    ResetGraph(0);
    SetGraphDebug(0);
    InitGeom();
    SetVideoMode(0);
    SetDefDrawEnv(&DrawEnv0, 0, 256, 320, 240);
    SetDefDrawEnv(&DrawEnv1, 0, 0, 320, 240);
    InitGameDisplay(1);
    doubleBuffer[0].disp = &DispEnv0;
    doubleBuffer[0].draw = &DrawEnv0;
    doubleBuffer[1].disp = &DispEnv1;
    doubleBuffer[1].draw = &DrawEnv1;
    CurrentScreen = 0;
    PutDrawEnv(doubleBuffer[CurrentScreen].draw);
    PutDispEnv(doubleBuffer[CurrentScreen].disp);
    SetDispMask(1);
    OT[0] = otBuffer0;
    OT[1] = otBuffer1;
    OT2[0] = ot2Buffer0;
    OT2[1] = ot2Buffer1;
    primBuffers[0] = PrimitiveBuffer0;
    primBuffers[1] = PrimitiveBuffer1;
    nextPrim.ptr = primBuffers[CurrentScreen];
}


void InitMenuDisplay(int geomY, int keepOffsets)
{
    SetDefDispEnv(&DispEnv0, 0, 0, 0x140, 0xB4);
    SetDefDispEnv(&DispEnv1, 0, 0x100, 0x140, 0xB4);
    if (keepOffsets == 0) {
        dispOffX0 = *(u_short *)&gameData->screenX;
        dispOffY0 = *(u_short *)&gameData->screenY;
        dispOffX1 = *(u_short *)&gameData->screenX;
        dispOffY1 = *(u_short *)&gameData->screenY;
    }
    SetGeomOffset(0xA0, geomY);
    SetGeomScreen(0xA0);
    PutDispEnv(doubleBuffer[CurrentScreen].disp);
}


void InitGameDisplay(int keepOffsets)
{
    SetDefDispEnv(&DispEnv0, 0, 0, 0x140, 0xF0);
    SetDefDispEnv(&DispEnv1, 0, 0x100, 0x140, 0xF0);
    if (keepOffsets == 0) {
        dispOffX1 = *(u_short *)&gameData->screenX;
        dispOffX0 = dispOffX1;
        dispOffY1 = *(u_short *)&gameData->screenY;
        dispOffY0 = dispOffY1;
    }
    SetGeomOffset(0xA0, 0x78);
    SetGeomScreen(0xA0);
    PutDispEnv(doubleBuffer[CurrentScreen].disp);
    PutDispEnv(doubleBuffer[CurrentScreen == 0].disp);
}


void InitSonyDisplay(void)
{
    SetDefDispEnv(&DispEnv0, 0, 0, 0x280, 0x100);
    SetDefDispEnv(&DispEnv1, 0, 0x100, 0x280, 0x100);
    SetGeomOffset(0x140, 0x80);
    SetGeomScreen(0x140);
    PutDispEnv(doubleBuffer[CurrentScreen].disp);
}


void Swap(void)
{
    int   zero;
    int   drawTime;
    int   vcount;
    int   vcountNow;
    short nextScreen;
    DRAWENV *draw;

    zero = 0;
    drawTime = GsGetVcount();
    DrawSync(zero);
    drawTime = GsGetVcount() - drawTime;
    GsGetVcount();
    if ((gameType == 0) || (gameType == 2)) {
        VSync(0);
    }
    GsGetVcount();
    vcount = GsGetVcount();
    max = vcount;
    if (((gameType != 1) && (fastFrame == 0)) && (vcount < 0x20C)) {
        VSync(0);
    }
    vcountNow = GsGetVcount();
    max = vcountNow;
    menuSendFlags = vcountNow / 262;
    GsClearVcount(0);
    if (max >= 0x220) {
        if (drawTime >= 0xB) {
            gpuWarning = 2;
        } else {
            cpuWarning = 2;
        }
    }
    nextScreen = (CurrentScreen + 1) & 1;
    draw = doubleBuffer[nextScreen].draw;
    primsDrawn = 0;
    CurrentScreen = nextScreen;
    PutDrawEnv(draw);
    PutDispEnv(doubleBuffer[CurrentScreen].disp);
    DrawOTag((u_long *)((char *)OT[((CurrentScreen + 1) & 1)] + 0x7FFC));
    if (activateOT2 != 0) {
        DrawOTag((u_long *)((char *)OT2[((CurrentScreen + 1) & 1)] + 0x18FC));
    }
    nextPrim.ptr = primBuffers[CurrentScreen];
    SfxDoSfx();
}


void CallbackSwap(void)
{
    u_short nextScreen;

    DrawSync(0);
    nextScreen = (CurrentScreen + 1) & 1;
    CurrentScreen = nextScreen;
    PutDrawEnv(doubleBuffer[nextScreen].draw);
    PutDispEnv(doubleBuffer[CurrentScreen].disp);
    DrawOTag((u_long *)((char *)OT[((CurrentScreen + 1) & 1)] + 0x7FFC));
    DrawOTag((u_long *)((char *)OT2[((CurrentScreen + 1) & 1)] + 0x18FC));
    nextPrim.ptr = primBuffers[CurrentScreen];
}


void ClearVram(void)
{
    BLK_FILL *blk;

    blk = BlockFill(0, 0, 0x200, 0x100, 0, 0, 0);
    DrawPrim(blk);
    DrawSync(0);
    DFree(reserveHeap, (char *)blk);
    blk = BlockFill(0, 0x100, 0x200, 0x100, 0, 0, 0);
    DrawPrim(blk);
    DrawSync(0);
    DFree(reserveHeap, (char *)blk);
    blk = BlockFill(0x200, 0, 0x200, 0x100, 0, 0, 0);
    DrawPrim(blk);
    DrawSync(0);
    DFree(reserveHeap, (char *)blk);
    blk = BlockFill(0x200, 0x100, 0x200, 0x100, 0, 0, 0);
    DrawPrim(blk);
    DrawSync(0);
    DFree(reserveHeap, (char *)blk);
}


BLK_FILL *BlockFill(short x, short y, short w, short h, short red, short green, short blue)
{
    BLK_FILL *blk;

    blk = DAlloc(reserveHeap, sizeof(BLK_FILL));
    if (!blk) {
        sprintf(errorString, "Main.c:BlockFill(): Failed to allocate BlkFill\n");
        Error(errorString, 2);
    }
    SetBlockFill(blk);
    blk->x0 = x;
    blk->y0 = y;
    blk->w = w;
    blk->h = h;
    blk->r0 = red;
    blk->g0 = green;
    blk->b0 = blue;
    return blk;
}
