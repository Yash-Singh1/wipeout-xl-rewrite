/* The PsyQ libgpu calls the game makes: primitives, ordering tables, the
   drawing and display environments and video-memory transfers. The setters
   are translations of the library's code in the retail executable; drawing
   decodes each GPU packet the way the console's GPU interprets it and hands
   the geometry to gpu_gl.c.

   Ordering tables link packets with 24-bit addresses in each packet's first
   word. On the console those are the low bits of the packet's address; here
   they are numbers from a table of the packets and table entries that have
   been linked, so the game's linking (all through AddPrim and ClearOTagR)
   behaves the same with 64-bit addresses. */
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../hdtex.h"
#include "../platform.h"

#include "../trace.h"
#include "framecap.h"
#include "gpu.h"
#include "../link.h"
#include "../settings.h"
#include "gte.h"
#include "psyq.h"
#include "../region.h"

uint16_t psxVram[VRAM_HEIGHT][VRAM_WIDTH];
uint16_t psxVramHigh[VRAM_HEIGHT][VRAM_WIDTH];

/* ---- link numbers ---- */

#define LINK_END 0xFFFFFFu
#define LINK_SLOTS (1 << 18)

static void **linkPointers;
static uint32_t linkCount = 1; /* 0 is unused */
static void **linkKeys;
static uint32_t *linkIds;

static uint32_t HashPointer(const void *pointer)
{
    uint64_t v = (uint64_t)(uintptr_t)pointer;

    v ^= v >> 33;
    v *= 0xFF51AFD7ED558CCDull;
    v ^= v >> 33;
    return (uint32_t)v & (LINK_SLOTS - 1);
}

/* Buffers of packets with a range of link numbers of their own, one per word (PsxLinkRegion). */
#define REGION_FIRST 0x200000u
static struct {
    char *start;
    size_t bytes;
    uint32_t first;
} regions[8];
static int regionCount;
static uint32_t regionNext = REGION_FIRST;

void PsxLinkRegion(void *start, size_t bytes)
{
    if (regionCount == (int)(sizeof(regions) / sizeof(regions[0])) || regionNext + bytes / 4 >= LINK_END) {
        fprintf(stderr, "libgpu: no link numbers left for a packet buffer\n");
        abort();
    }
    regions[regionCount].start = start;
    regions[regionCount].bytes = bytes;
    regions[regionCount].first = regionNext;
    regionNext += (uint32_t)(bytes / 4);
    regionCount++;
}

static uint32_t LinkId(void *pointer)
{
    uint32_t slot = HashPointer(pointer);
    int r;

    for (r = 0; r < regionCount; r++) {
        if ((char *)pointer >= regions[r].start && (char *)pointer < regions[r].start + regions[r].bytes) {
            return regions[r].first + (uint32_t)(((char *)pointer - regions[r].start) / 4);
        }
    }

    if (!linkKeys) {
        linkKeys = calloc(LINK_SLOTS, sizeof(*linkKeys));
        linkIds = calloc(LINK_SLOTS, sizeof(*linkIds));
        linkPointers = calloc(LINK_SLOTS, sizeof(*linkPointers));
    }
    for (;;) {
        if (linkKeys[slot] == pointer) {
            return linkIds[slot];
        }
        if (!linkKeys[slot]) {
            if (linkCount >= LINK_SLOTS / 2) {
                fprintf(stderr, "libgpu: too many linked packets\n");
                abort();
            }
            linkKeys[slot] = pointer;
            linkIds[slot] = linkCount;
            linkPointers[linkCount] = pointer;
            return linkCount++;
        }
        slot = (slot + 1) & (LINK_SLOTS - 1);
    }
}

static void *LinkPointer(uint32_t id)
{
    int r;

    for (r = 0; r < regionCount && id >= REGION_FIRST; r++) {
        if (id >= regions[r].first && id < regions[r].first + regions[r].bytes / 4) {
            return regions[r].start + (size_t)(id - regions[r].first) * 4;
        }
    }
    if (id == 0 || id >= linkCount) {
        fprintf(stderr, "libgpu: broken ordering-table link %06x\n", id);
        abort();
    }
    return linkPointers[id];
}

void AddPrim(u_long *ot, void *primitive)
{
    u_long *p = primitive; /* the game passes any POLY_*, LINE_*, TILE or DR_* packet (psyq.h) */

    *p = (*p & 0xFF000000u) | (*ot & 0x00FFFFFFu);
    *ot = (*ot & 0xFF000000u) | (LinkId(p) & 0x00FFFFFFu);
}

/* A chain of packets, first to last already linked, into one entry. */
void AddPrims(u_long *ot, void *first, void *last)
{
    u_long *tail = last;

    *tail = (*tail & 0xFF000000u) | (*ot & 0x00FFFFFFu);
    *ot = (*ot & 0xFF000000u) | (LinkId(first) & 0x00FFFFFFu);
}

/* The ordering-table DMA: each entry links to the one before it, the first
   is the end of the list. */
u_long *ClearOTagR(u_long *ot, int n)
{
    int i;

    for (i = n - 1; i > 0; i--) {
        ot[i] = LinkId(&ot[i - 1]) & 0x00FFFFFFu;
    }
    if (n > 0) {
        ot[0] = LINK_END;
    }
    return ot;
}

/* ---- primitive setters (the library's own values) ---- */

static void SetHeader(void *p, int length, int code)
{
    ((u_char *)p)[3] = (u_char)length;
    ((u_char *)p)[7] = (u_char)code;
}

void SetPolyF3(POLY_F3 *p) { SetHeader(p, 4, 0x20); }
void SetPolyF4(POLY_F4 *p) { SetHeader(p, 5, 0x28); }
void SetPolyFT3(POLY_FT3 *p) { SetHeader(p, 7, 0x24); }
void SetPolyFT4(POLY_FT4 *p) { SetHeader(p, 9, 0x2C); }
void SetPolyG3(POLY_G3 *p) { SetHeader(p, 6, 0x30); }
void SetPolyG4(POLY_G4 *p) { SetHeader(p, 8, 0x38); }
void SetPolyGT3(POLY_GT3 *p) { SetHeader(p, 9, 0x34); }
void SetPolyGT4(POLY_GT4 *p) { SetHeader(p, 12, 0x3C); }
void SetLineF2(LINE_F2 *p) { SetHeader(p, 3, 0x40); }
void SetBlockFill(BLK_FILL *p) { SetHeader(p, 3, 0x02); }
void SetLineG2(LINE_G2 *p) { SetHeader(p, 4, 0x50); }
void SetTile(TILE *p) { SetHeader(p, 3, 0x60); }

void SetSemiTrans(void *p, int abe)
{
    u_char *code = (u_char *)p + 7;
    *code = abe ? (u_char)(*code | 2) : (u_char)(*code & 0xFD);
}

void SetShadeTex(void *p, int tge)
{
    u_char *code = (u_char *)p + 7;
    *code = tge ? (u_char)(*code | 1) : (u_char)(*code & 0xFE);
}

/* The graphics type ResetGraph detects; 0 is the retail console's GPU. */
static int graphType;

int GetGraphType(void)
{
    return graphType;
}

u_short GetTPage(int tp, int abr, int x, int y)
{
    u_short page;

    if (graphType) {
        page = (u_short)(((tp & 3) << 9) | ((abr & 3) << 7) | ((y & 0x300) >> 3));
    } else {
        page = (u_short)(((tp & 3) << 7) | ((abr & 3) << 5) | ((y & 0x100) >> 4) | ((y & 0x200) << 2)); /* (bit 11: gpu.h) */
    }
    return (u_short)(page | ((x & 0x3FF) >> 6));
}

u_short GetClut(int x, int y)
{
    return (u_short)(((y << 6) | ((x >> 4) & 0x3F)) & 0xFFFF);
}

static uint32_t DrawModeWord(int dfe, int dtd, int tpage)
{
    uint32_t mode, page;

    if (graphType) {
        mode = dtd ? 0xE1000800u : 0xE1000000u;
        page = (uint32_t)tpage & 0x27FF;
        if (dfe) {
            page |= 0x1000;
        }
    } else {
        mode = dtd ? 0xE1000200u : 0xE1000000u;
        page = (uint32_t)tpage & 0x9FF; /* (bit 11: the second half of video memory, gpu.h) */
        if (dfe) {
            page |= 0x400;
        }
    }
    return mode | page;
}

static uint32_t TextureWindowWord(RECT *tw)
{
    uint32_t x, y, w, h;

    if (!tw) {
        return 0;
    }
    x = (uint32_t)(u_char)tw->x >> 3;
    y = (uint32_t)(u_char)tw->y >> 3;
    w = (uint32_t)((-tw->w) & 0xFF) >> 3;
    h = (uint32_t)((-tw->h) & 0xFF) >> 3;
    return 0xE2000000u | (x << 10) | (y << 15) | (h << 5) | w;
}

void SetDrawMode(DR_MODE *p, int dfe, int dtd, int tpage, RECT *tw)
{
    ((u_char *)p)[3] = 2;
    p->code[0] = DrawModeWord(dfe, dtd, tpage & 0xFFFF);
    p->code[1] = TextureWindowWord(tw);
}

DRAWENV *SetDefDrawEnv(DRAWENV *env, int x, int y, int w, int h)
{
    env->clip.x = (short)x;
    env->ofs[0] = (short)x;
    env->clip.y = (short)y;
    env->ofs[1] = (short)y;
    env->clip.w = (short)w;
    env->clip.h = (short)h;
    env->tw.x = env->tw.y = env->tw.w = env->tw.h = 0;
    env->r0 = env->g0 = env->b0 = 0;
    env->dtd = 1;
    env->dfe = (u_char)(h != 0x1E0);
    env->tpage = GetTPage(0, 0, 640, 0);
    env->isbg = 0;
    return env;
}

DISPENV *SetDefDispEnv(DISPENV *env, int x, int y, int w, int h)
{
    env->disp.x = (short)x;
    env->disp.y = (short)y;
    env->disp.w = (short)w;
    env->disp.h = (short)h;
    env->screen.x = env->screen.y = env->screen.w = env->screen.h = 0;
    env->isinter = 0;
    env->isrgb24 = 0;
    env->pad0 = env->pad1 = 0;
    return env;
}

/* ---- GPU state ---- */

static struct {
    uint32_t texpage;            /* E1: bits 0-8 page and blend, 9 dither, 10 draw to display */
    int windowMaskX, windowMaskY, windowOffsetX, windowOffsetY;
    int areaLeft, areaTop, areaRight, areaBottom;
    int offsetX, offsetY;
    int displayX, displayY, displayW, displayH;
    int displayEnabled;
} state;

static void ApplyArea(void)
{
    GpuSetDrawArea(state.areaLeft, state.areaTop, state.areaRight - state.areaLeft + 1,
                   state.areaBottom - state.areaTop + 1, state.offsetX, state.offsetY);
}

/* A textured polygon's page attribute sets only the page, blend and colour
   depth (bits 0-8); dithering and drawing to the display stay as E1 set them. */
static void SetTexpage(uint32_t value)
{
    state.texpage = (state.texpage & ~0x9FFu) | (value & 0x9FF);
}

/* GP0 E1-E6 */
static void Environment(uint32_t word)
{
    switch (word >> 24) {
    case 0xE1:
        state.texpage = word & 0xFFFFFF;
        break;
    case 0xE2:
        state.windowMaskX = word & 0x1F;
        state.windowMaskY = (word >> 5) & 0x1F;
        state.windowOffsetX = (word >> 10) & 0x1F;
        state.windowOffsetY = (word >> 15) & 0x1F;
        break;
    case 0xE3:
        state.areaLeft = word & 0x3FF;
        state.areaTop = (word >> 10) & 0x1FF;
        ApplyArea();
        break;
    case 0xE4:
        state.areaRight = word & 0x3FF;
        state.areaBottom = (word >> 10) & 0x1FF;
        ApplyArea();
        break;
    case 0xE5:
        /* 11-bit signed offsets */
        state.offsetX = (int32_t)(word << 21) >> 21;
        state.offsetY = (int32_t)(word << 10) >> 21;
        ApplyArea();
        break;
    default:
        break;
    }
}

static void Packet(const uint32_t *w, int words);
static void ReadPendingTables(void);

/* A packet as the GPU receives it: recorded for tests, then drawn. */
static int interpolating; /* drawing a picture between two of the game's frames (PsxDrawInterpolated) */
static int recordingBetween; /* ... which the frame recorder keeps (FrameCapBetween) */
static float interpolationAlong;

/* Widescreen: the packets of the menus' backdrops (PortWideBackdrop), which fill the picture's sides. They are
   the same few packets every frame. */
#define BACKDROPS 8
static const void *backdrops[BACKDROPS];
static int backdropCount, backdropNext;

void PortWideBackdrop(const void *packet)
{
    int i;

    for (i = 0; i < backdropCount && backdrops[i] != packet; i++) {
    }
    if (i == backdropCount) {
        backdrops[backdropNext] = packet;
        backdropNext = (backdropNext + 1) % BACKDROPS;
        backdropCount = backdropCount < BACKDROPS ? backdropCount + 1 : BACKDROPS;
    }
    GpuWideMenu();
}

void PortWideLoadScreen(void)
{
    GpuWideLoadScreen(); /* (as a menu without a backdrop: only the fills across the screen reach the sides) */
}

static int Backdrop(const uint32_t *w)
{
    int i;

    for (i = 0; i < backdropCount; i++) {
        if ((const uint32_t *)backdrops[i] + 1 == w) {
            return 1;
        }
    }
    return 0;
}

static void Submit(const uint32_t *w, int words)
{
    int backdrop = PortScreenMargin() && Backdrop(w);

    if (!interpolating) {
        PortTraceGp0(w, words);
    }
    if (backdrop) {
        GpuBackdrop(1);
    }
    Packet(w, words);
    if (backdrop) {
        GpuBackdrop(0);
    }
}

/* The library's clamps of a drawing-area corner and a size to video memory. */
static int ClampX(int x)
{
    return x < 0 ? 0 : x > 0x3FF ? 0x3FF : x;
}

static int ClampY(int y)
{
    return y < 0 ? 0 : y > 0x1FF ? 0x1FF : y;
}

/* The drawing environment's packet as SetDrawEnv builds it: area, offset,
   mode, texture window, mask, then the background (a fill when the area is
   aligned to 64 pixels, else a rectangle). */
DRAWENV *PutDrawEnv(DRAWENV *env)
{
    uint32_t words[6], background[3];
    int x = env->clip.x, y = env->clip.y, w = ClampX(env->clip.w), h = ClampY(env->clip.h);

    words[0] = 0xE3000000u | (uint32_t)(ClampY(env->clip.y) & 0x3FF) << 10 | (uint32_t)ClampX(env->clip.x);
    words[1] = 0xE4000000u | (uint32_t)ClampY(env->clip.y + env->clip.h - 1) << 10
               | (uint32_t)ClampX(env->clip.x + env->clip.w - 1);
    words[2] = 0xE5000000u | (uint32_t)(env->ofs[1] & 0x7FF) << 11 | (uint32_t)(env->ofs[0] & 0x7FF);
    words[3] = DrawModeWord(env->dfe, env->dtd, env->tpage);
    words[4] = TextureWindowWord(&env->tw);
    words[5] = 0xE6000000u;
    Submit(words, 6);
    if (env->isbg) {
        if ((x & 0x3F) || (w & 0x3F)) {
            background[0] = 0x60000000u | (uint32_t)env->b0 << 16 | (uint32_t)env->g0 << 8 | env->r0;
            background[1] = (uint32_t)(uint16_t)(y - env->ofs[1]) << 16 | (uint16_t)(x - env->ofs[0]);
        } else {
            background[0] = 0x02000000u | (uint32_t)env->b0 << 16 | (uint32_t)env->g0 << 8 | env->r0;
            background[1] = (uint32_t)(uint16_t)y << 16 | (uint16_t)x;
        }
        background[2] = (uint32_t)h << 16 | (uint32_t)w;
        Submit(background, 3);
    }
    return env;
}

/* The game's first PutDispEnv (InitSystem -> InitGameDisplay) is given doubleBuffer[0].disp before the
   game has stored the environment there, i.e. a null pointer.  On the console that read the words at
   address 0 (kernel RAM); here it reads zeros. */
static DISPENV nullDispEnv;

static void HandOverSplit(void);

/* Frame interpolation: the frame just drawn (DrawOTag of the upcoming tables), shown in place of the console's
   picture until the game says what it draws next. The console shows each frame only once the next is drawn: the
   picture it shows meanwhile is the frame before, which interpolation had already gone past (the pictures
   shown come up to the frame just drawn), so a field that passes outside the game's Swap (pausing does), or
   with tables that cannot be interpolated, would step back to it. The next frame drawn takes its place;
   anything else the game does to video memory or the display ends it (the game shows what it put there). */
static struct {
    int pending, left, top, width, height;
    u_long *second; /* (the tables' second, drawn just after: part of the frame) */
} drawnFrame;

static void ForgetDrawnFrame(void)
{
    drawnFrame.pending = 0;
}

static int shakeX, shakeY; /* PortDisplayShake's, until the next PutDispEnv */

void PortDisplayShake(int x, int y)
{
    shakeX = x;
    shakeY = y;
}

DISPENV *PutDispEnv(DISPENV *env)
{
    DISPENV *given = env;

    if (!env) {
        env = &nullDispEnv;
    }
    PortTraceGp1(0x05000000u | (uint32_t)(env->disp.y & 0x1FF) << 10 | (uint32_t)(env->disp.x & 0x3FF));
    if (env->disp.x != state.displayX || env->disp.y != state.displayY) {
        ForgetDrawnFrame();
    }
    state.displayX = env->disp.x;
    state.displayY = env->disp.y;
    state.displayW = env->disp.w;
    state.displayH = env->disp.h;
    GpuDisplayShake(shakeX, shakeY);
    if (PortLinkRole() == LINK_SLAVE && PortLinkSplit()) {
        HandOverSplit(); /* the second player's game shows what it drew last (no fields pass in it) */
    }
    return given;
}

void SetDispMask(int mask)
{
    if (!mask) {
        ForgetDrawnFrame();
    }
    state.displayEnabled = mask != 0;
}

/* ResetGraph as the library's _reset: mode 0 resets the GPU (GP1 0), modes 1
   and 3 only cancel drawing and flush its command buffer (GP1 2, GP1 1); all
   then send a mode word built from the GPU status (the current texture page
   and mask bits). */
int ResetGraph(int mode)
{
    uint32_t word;

    ReadPendingTables();
    GpuInit();
    if ((mode & 3) == 0) {
        PortTraceGp1(0x00000000u);
        memset(&state, 0, sizeof(state));
        ApplyArea();
    } else {
        PortTraceGp1(0x02000000u);
        PortTraceGp1(0x01000000u);
    }
    word = 0xE1001000u | (state.texpage & 0x7FF);
    Submit(&word, 1);
    graphType = 0;
    return 0;
}

int SetGraphDebug(int level)
{
    (void)level;
    return 0;
}

int DrawSync(int mode)
{
    (void)mode;
    ReadPendingTables();
    return 0;
}

/* The display: libetc's VSync calls this when a frame is shown. The GPU scans out 240 lines (NTSC; 256 PAL)
   from the display start whatever height the environment gives (the menus say 180 and draw down to
   line 230); only a height above 256 is an interlaced picture of twice that. */
static int ShownLines(int height)
{
    int lines = portPal ? 256 : 240;

    return height > 256 ? 2 * lines : lines;
}
/* The window keeps its last picture until the area to be shown has been drawn again (PsxHoldPicture):
   the drawing areas of the tables drawn since. */
#define HOLD_AREAS 4
static int holdPicture, holdAreaCount;
static struct {
    int left, top, right, bottom;
} holdAreas[HOLD_AREAS];

void PsxHoldPicture(void)
{
    holdPicture = 1;
    holdAreaCount = 0;
}

static int HoldReleased(void)
{
    int i;

    for (i = 0; i < holdAreaCount; i++) {
        if (state.displayX >= holdAreas[i].left && state.displayX <= holdAreas[i].right &&
            state.displayY >= holdAreas[i].top && state.displayY <= holdAreas[i].bottom) {
            return 1;
        }
    }
    return 0;
}

/* Two players on one machine (link.c): during a linked race each game shows the band of console rows
   GPU_SPLIT_TOP .. GPU_SPLIT_TOP + GPU_SPLIT_ROWS of its picture (gpu.h: the full width, the part of the height
   with the ship), the first in the upper half of the window and the second, which hands its band over, in the
   lower half. With widescreen the band has the wide picture's sides, and fills the half of a wide window. Each
   game's HUD (the last entry of its ordering table and the second table) is drawn smaller, its elements at the
   band's edges and corners (DrawAnchored), so that it fits in the band. */
static int drawnSinceRead;

/* The second player's game: the size the first player's game shows the band of the picture at column x at (the
   half's height, the band's shape); 0 when it has not said. */
static int SplitSize(int x, int *w, int *h)
{
    int margin, halfW, halfH, across = 320 + 2 * GpuShownMargin(x, 320);

    if (!state.displayEnabled || !PortLinkBand(&margin, &halfW, &halfH)) {
        return 0;
    }
    /* (the console's pixels as wide as they are shown: PAL's 256 lines across the same 4:3 screen) */
    *h = halfH;
    *w = (int)((int64_t)halfH * across * GPU_LINES / (GPU_SPLIT_ROWS * 240));
    if (*w > halfW) {
        *w = halfW;
        *h = (int)((int64_t)halfW * GPU_SPLIT_ROWS * 240 / ((int64_t)across * GPU_LINES));
    }
    while ((size_t)*w * *h * 4 > LINK_FRAME_BYTES) {
        *w = *w * 3 / 4;
        *h = *h * 3 / 4;
    }
    return *w > 0 && *h > 0;
}

static void PutSplit(const uint8_t *pixels, int w, int h, void *user)
{
    (void)user;
    PortLinkPutFrame(pixels, w, h);
    GpuPictureHandedOver();
}

/* ... the band of the picture at (x, y) goes to the first player's game; 1 when it did. */
static int HandOverSplitOf(int x, int y)
{
    int w, h;

    if (!SplitSize(x, &w, &h)) {
        return 0;
    }
    PutSplit(GpuReadPicture(x, y + GPU_SPLIT_TOP, 320, GPU_SPLIT_ROWS, GpuShownMargin(x, 320), w, h), w, h, NULL);
    return 1;
}

/* ... the picture on display, at each switch of the display (its Swap waits for no fields: the link paces it),
   unless pictures between its frames went over since the frame was drawn (PsxLinkWaiting, while it waited). */
static void HandOverSplit(void)
{
    if (drawnSinceRead && HandOverSplitOf(state.displayX, state.displayY)) {
        drawnSinceRead = 0;
    }
}

/* The first player's game: the band of its picture at (x, y), and the second player's under it (which draws its
   pictures as often as this game shows them). */
static void ShowSplit(int x, int y)
{
    static unsigned shownSequence;
    static uint64_t lastShown, period = 16666667;
    uint64_t now = SDL_GetTicksNS();
    unsigned sequence = 0;
    int w = 0, h = 0;
    const uint8_t *lower = PortLinkGetFrame(&w, &h, &sequence);

    GpuPresentSplit(x, y + GPU_SPLIT_TOP, 320, GPU_SPLIT_ROWS, lower, w, h, sequence != shownSequence);
    shownSequence = sequence;
    if (lastShown && now - lastShown < 100000000ull) {
        period = (period * 7 + (now - lastShown)) / 8;
        PortLinkSetPresentPeriod((unsigned)(period / 1000));
    }
    lastShown = now;
}

void PsxShowDisplay(void)
{
    GpuFlush();
    if (PortLinkRole() == LINK_SLAVE && PortLinkSplit()) {
        PsxLinkWaiting(); /* (its pictures go to the first player's game) */
        return;
    }
    if (PsxCanInterpolate() && !(portConfig.fixedStep && PsxInterpolationTest())) {
        if (state.displayEnabled && state.displayW > 0 && state.displayH > 0) {
            FrameCapShown(state.displayX, state.displayY, state.displayW, ShownLines(state.displayH));
        }
        PsxDrawInterpolated();
        return;
    }
    if (drawnFrame.pending && PsxInterpolationOn() && !holdPicture && state.displayEnabled &&
        !(portConfig.fixedStep && PsxInterpolationTest())) {
        if (state.displayW > 0 && state.displayH > 0) {
            FrameCapShown(state.displayX, state.displayY, state.displayW, ShownLines(state.displayH));
        }
        if (PortLinkSplit()) {
            ShowSplit(drawnFrame.left, drawnFrame.top);
        } else {
            GpuPresent(drawnFrame.left, drawnFrame.top, drawnFrame.width, ShownLines(drawnFrame.height));
        }
        return;
    }
    if (PortLinkSplit()) {
        ShowSplit(state.displayX, state.displayY);
        return;
    }
    if (holdPicture) {
        if (!HoldReleased()) {
            return;
        }
        holdPicture = 0;
    }
    if (state.displayEnabled && state.displayW > 0 && state.displayH > 0) {
        FrameCapShown(state.displayX, state.displayY, state.displayW, ShownLines(state.displayH));
        GpuPresent(state.displayX, state.displayY, state.displayW, ShownLines(state.displayH));
    } else {
        GpuPresent(-1, -1, 0, 0);
    }
}

/* ---- video memory transfers ---- */

/* The transfer commands, recorded for tests. */
static void TraceTransfer(uint32_t command, RECT *rect, const void *data)
{
    static const uint32_t clearCache = 0x01000000u;
    uint32_t header[3];

    if (!PortTracing()) {
        return;
    }
    header[0] = command;
    header[1] = (uint32_t)(uint16_t)rect->y << 16 | (uint16_t)rect->x;
    header[2] = (uint32_t)(uint16_t)rect->h << 16 | (uint16_t)rect->w;
    PortTraceGp0(&clearCache, 1);
    if (data) {
        int count = 3 + (rect->w * rect->h + 1) / 2;
        uint32_t *words = malloc((size_t)count * 4);

        memcpy(words, header, sizeof(header));
        memcpy(words + 3, data, (size_t)rect->w * rect->h * 2);
        if ((rect->w * rect->h) & 1) {
            ((uint16_t *)(words + 3))[rect->w * rect->h] = 0;
        }
        PortTraceGp0(words, count);
        free(words);
    } else {
        PortTraceGp0(header, 3);
    }
}

int LoadImage(RECT *rect, u_long *p)
{
    const uint16_t *pixels = (const uint16_t *)p;
    int x, y;

    ForgetDrawnFrame();
    TraceTransfer(0xA0000000u, rect, p);

    if (rect->y >= VRAM_HEIGHT) {
        /* the second half (gpu.h): textures only */
        for (y = 0; y < rect->h; y++) {
            for (x = 0; x < rect->w; x++) {
                psxVramHigh[(rect->y + y) & (VRAM_HEIGHT - 1)][(rect->x + x) & (VRAM_WIDTH - 1)] = *pixels++;
            }
        }
        GpuVramHighChanged();
        return 0;
    }
    for (y = 0; y < rect->h; y++) {
        for (x = 0; x < rect->w; x++) {
            psxVram[(rect->y + y) & (VRAM_HEIGHT - 1)][(rect->x + x) & (VRAM_WIDTH - 1)] = *pixels++;
        }
    }
    GpuVramChanged(rect->x, rect->y, rect->w, rect->h);
    HdTexUploaded(rect->x, rect->y, rect->w, rect->h);
    GpuWriteFramebuffer(rect->x, rect->y, rect->w, rect->h);
    FrameCapVramChanged();
    return 0;
}

int StoreImage(RECT *rect, u_long *p)
{
    uint16_t *pixels = (uint16_t *)p;
    int x, y;

    TraceTransfer(0xC0000000u, rect, NULL);
    GpuReadBack(rect->x, rect->y, rect->w, rect->h);
    for (y = 0; y < rect->h; y++) {
        for (x = 0; x < rect->w; x++) {
            *pixels++ = psxVram[(rect->y + y) & (VRAM_HEIGHT - 1)][(rect->x + x) & (VRAM_WIDTH - 1)];
        }
    }
    return 0;
}

int MoveImage(RECT *rect, int x, int y)
{
    static uint16_t buffer[VRAM_HEIGHT][VRAM_WIDTH];
    uint32_t words[4];
    int i, j;

    ForgetDrawnFrame();
    words[0] = 0x80000000u;
    words[1] = (uint32_t)(uint16_t)rect->y << 16 | (uint16_t)rect->x;
    words[2] = (uint32_t)(uint16_t)y << 16 | (uint16_t)x;
    words[3] = (uint32_t)(uint16_t)rect->h << 16 | (uint16_t)rect->w;
    PortTraceGp0(words, 4);

    GpuReadBack(rect->x, rect->y, rect->w, rect->h);
    for (j = 0; j < rect->h; j++) {
        for (i = 0; i < rect->w; i++) {
            buffer[j][i] = psxVram[(rect->y + j) & (VRAM_HEIGHT - 1)][(rect->x + i) & (VRAM_WIDTH - 1)];
        }
    }
    for (j = 0; j < rect->h; j++) {
        for (i = 0; i < rect->w; i++) {
            psxVram[(y + j) & (VRAM_HEIGHT - 1)][(x + i) & (VRAM_WIDTH - 1)] = buffer[j][i];
        }
    }
    GpuVramChanged(x, y, rect->w, rect->h);
    HdTexOverwritten(x, y, rect->w, rect->h);
    GpuMoveFramebuffer(rect->x, rect->y, rect->w, rect->h, x, y);
    FrameCapVramChanged();
    return 0;
}

/* ClearImage as the library's _clr: a fill when the rectangle is aligned to
   64 pixels, otherwise a rectangle drawn over the whole of video memory with
   the drawing area and offset restored after it. */
int ClearImage(RECT *rect, u_char r, u_char g, u_char b)
{
    uint32_t colour = (uint32_t)b << 16 | (uint32_t)g << 8 | r;
    uint32_t xy = (uint32_t)(uint16_t)rect->y << 16 | (uint16_t)rect->x;
    uint32_t size = (uint32_t)ClampY(rect->h) << 16 | (uint32_t)ClampX(rect->w);
    uint32_t words[7];

    ForgetDrawnFrame();
    if ((rect->x & 0x3F) || (ClampX(rect->w) & 0x3F)) {
        words[0] = 0xE3000000u;
        words[1] = 0xE4FFFFFFu;
        words[2] = 0xE5000000u;
        words[3] = 0xE6000000u;
        words[4] = 0x60000000u | colour;
        words[5] = xy;
        words[6] = size;
        Submit(words, 4);
        Submit(words + 4, 3);
        words[0] = 0xE3000000u | (uint32_t)state.areaTop << 10 | (uint32_t)state.areaLeft;
        words[1] = 0xE4000000u | (uint32_t)state.areaBottom << 10 | (uint32_t)state.areaRight;
        words[2] = 0xE5000000u | (uint32_t)(state.offsetY & 0x7FF) << 11 | (uint32_t)(state.offsetX & 0x7FF);
        Submit(words, 3);
    } else {
        words[0] = 0xE6000000u;
        words[1] = 0x02000000u | colour;
        words[2] = xy;
        words[3] = size;
        Submit(words, 1);
        Submit(words + 1, 3);
    }
    return 0;
}

/* ---- the debug font (unused by the retail game's output) ---- */

void FntLoad(int tx, int ty)
{
    (void)tx;
    (void)ty;
}

int FntOpen(int x, int y, int w, int h, int isbg, int n)
{
    (void)x;
    (void)y;
    (void)w;
    (void)h;
    (void)isbg;
    (void)n;
    return 0;
}

void SetDumpFnt(int id)
{
    (void)id;
}

/* ---- drawing packets ---- */

static float hudShift; /* widescreen: the HUD element being drawn goes this far towards its edge (DrawAnchored) */

/* A vertex at packet position xy, from its projection when it was found. */
static void Vertex(GpuVertex *v, uint32_t xy, const GtePrecise *precise)
{
    int x = (int32_t)(xy << 21) >> 21, y = (int32_t)(xy << 5) >> 21;
    GtePrecise moved;

    if (precise && interpolating && GteInterpolate(precise, interpolationAlong, &moved)) {
        precise = &moved; /* between the frame before and this one */
    }
    if (precise) {
        v->x = precise->x + (float)state.offsetX;
        v->y = precise->y + (float)state.offsetY;
        v->w = precise->z;
        v->a = (uint8_t)(precise->fade * 255.0f + 0.5f);
        v->noDepth = precise->background;
    } else {
        v->x = (float)(x + state.offsetX) + hudShift;
        v->y = (float)(y + state.offsetY);
        v->w = 0.0f; /* no depth: affine */
        v->noDepth = 0;
    }
}

/* The vertices of one primitive. Perspective-correct texturing needs the
   depth of every vertex; when one is missing (a point the game placed in 2D)
   the whole primitive is drawn affine, as the console draws it, so that the
   two triangles of a quad agree. */
static void HudPoint(GpuVertex *v);
enum { HUD_PLAIN, HUD_SCALED, HUD_FILL };
static int hudActive;            /* the HUD element being drawn is moved by HudPoint: HUD_SCALED or HUD_FILL */
static int hudFillBand;          /* ... a box over the whole screen covers the band of a split window */
static float hudMoveX, hudMoveY; /* ... a scaled one goes this far from the band's middle */

/* The HUD's packets are placed in 2D: one whose corners only happen to land on projected pixels is drawn as given,
   not moved to those projections (a letter would be stretched over the texels beside it). The 3D renderers' own
   packets there (bound to their projections: a menu's model) keep theirs. */
static int hudPackets;

static GtePrecise resolved[GTE_RESOLVE_MAX]; /* the last primitive's projections (Vertices), for NearCut */
static unsigned resolvedMask;

static unsigned Vertices(GpuVertex *v, const uint32_t *const *words, int count)
{
    GtePrecise *precise = resolved;
    uint32_t xy[GTE_RESOLVE_MAX];
    unsigned mask;
    int i, depth, bound;

    for (i = 0; i < count; i++) {
        xy[i] = *words[i];
    }
    mask = GteResolvePrecise(xy, (const void *const *)words, count, precise);
    bound = GteResolvedBound();
    if (hudPackets && !bound) {
        mask = 0;
    }
    depth = mask == (1u << count) - 1;

    for (i = 0; i < count; i++) {
        Vertex(&v[i], xy[i], (mask >> i) & 1 ? &precise[i] : NULL);
        depth &= v[i].w > 0.0f;
    }
    for (i = 0; i < count && !depth; i++) {
        v[i].w = 0.0f;
    }
    for (i = 0; i < count && !bound; i++) {
        v[i].noDepth = 1; /* (the depth buffer: only what the 3D renderers placed) */
    }
    {
        /* seen from behind: its first three corners wind the other way on screen (the game's facing test) */
        int back = depth && count >= 3 &&
                   (v[1].x - v[0].x) * (v[2].y - v[0].y) - (v[1].y - v[0].y) * (v[2].x - v[0].x) <= 0.0f;

        for (i = 0; i < count; i++) {
            v[i].backFacing = back;
        }
    }
    if (hudActive && !mask) {
        for (i = 0; i < count; i++) {
            HudPoint(&v[i]); /* the HUD of a split window, or a box over the whole screen */
        }
    }
    resolvedMask = mask;
    return mask;
}

/* A polygon with a corner nearer than the GTE projects (the far view's faces beside and just behind the camera; in a
   picture between two frames, what the camera was nearer to or had not yet passed) is placed from its corners' view-
   space positions, cut at a plane just in front of the camera (the far view's, portfar.h) with its texture coordinates
   and colours, and the part in front drawn. A quad is cut as the two triangles the GPU draws of it (corners 0 1 2 and
   1 2 3): a face that is not flat, seen edge on beside the camera, is not convex on the screen, and its other diagonal
   left a hole in the wall. 1 when it was (or was wholly behind), 0 when it needs none of this or its corners have no
   view-space positions. */
#define CUT_NEAR 16.0
typedef struct {
    double view[3], depth; /* view space; the depth drawn per view-space depth (the far view's is a hair more) */
    GpuVertex v;
} CutCorner;

/* back: 1 when the triangle's corners as given wind as a polygon's seen from behind do */
static void CutTriangle(const GpuDrawState *draw, const CutCorner *in, int back)
{
    CutCorner out[4];
    const double plane = CUT_NEAR;
    int i, k, cut = 0, turn;

    for (i = 0; i < 3; i++) {
        const int j = (i + 1) % 3;
        const int aIn = in[i].view[2] >= plane, bIn = in[j].view[2] >= plane;

        if (aIn) {
            out[cut++] = in[i];
        }
        if (aIn != bIn) {
            double t = (plane - in[i].view[2]) / (in[j].view[2] - in[i].view[2]);

            out[cut] = in[i];
            for (k = 0; k < 3; k++) {
                out[cut].view[k] = in[i].view[k] + (in[j].view[k] - in[i].view[k]) * t;
            }
            out[cut].view[2] = plane;
            out[cut].depth = in[i].depth + (in[j].depth - in[i].depth) * t;
            out[cut].v.u = (float)(in[i].v.u + (in[j].v.u - in[i].v.u) * t);
            out[cut].v.v = (float)(in[i].v.v + (in[j].v.v - in[i].v.v) * t);
            out[cut].v.r = (uint8_t)(in[i].v.r + (in[j].v.r - in[i].v.r) * t + 0.5);
            out[cut].v.g = (uint8_t)(in[i].v.g + (in[j].v.g - in[i].v.g) * t + 0.5);
            out[cut].v.b = (uint8_t)(in[i].v.b + (in[j].v.b - in[i].v.b) * t + 0.5);
            out[cut].v.a = (uint8_t)(in[i].v.a + (in[j].v.a - in[i].v.a) * t + 0.5);
            cut++;
        }
    }
    if (cut < 3) {
        return; /* wholly behind the camera */
    }
    for (i = 0; i < cut; i++) {
        float x, y;

        GteProjectView(out[i].view, &x, &y);
        out[i].v.x = x + (float)state.offsetX;
        out[i].v.y = y + (float)state.offsetY;
        out[i].v.w = (float)(out[i].view[2] * out[i].depth);
    }
    /* seen from behind: the winding on screen (as Vertices judges a whole polygon by its first three corners) */
    turn = (out[1].v.x - out[0].v.x) * (out[2].v.y - out[0].v.y) -
               (out[1].v.y - out[0].v.y) * (out[2].v.x - out[0].v.x) <= 0.0f;
    for (i = 0; i < cut; i++) {
        out[i].v.backFacing = turn != back;
    }
    for (i = 1; i + 1 < cut; i++) {
        GpuVertex triangle[3] = {out[0].v, out[i].v, out[i + 1].v};

        GpuTriangles(draw, triangle, 3);
    }
}

static int NearCut(const GpuDrawState *draw, const GpuVertex *v, int count)
{
    CutCorner in[4];
    double near = GteNearPlane();
    float along = interpolating ? interpolationAlong : 1.0f;
    int i, behind = 0;

    if (count < 3 || resolvedMask != (1u << count) - 1) {
        return 0;
    }
    for (i = 0; i < count; i++) {
        if (!GteViewAt(&resolved[i], along, in[i].view)) {
            return 0;
        }
        in[i].depth = resolved[i].view[2] != 0.0f ? resolved[i].z / resolved[i].view[2] : 1.0;
        in[i].v = v[i];
        /* (what GteInterpolate cannot place: behind the camera in this frame, or nearer than it projects) */
        behind += resolved[i].view[2] < near || in[i].view[2] < near;
    }
    if (!behind) {
        return 0;
    }
    CutTriangle(draw, &in[0], 0);
    if (count == 4) {
        CutTriangle(draw, &in[1], 1); /* (corners 1 2 3 wind the other way round) */
    }
    return 1;
}

static void Colour(GpuVertex *v, uint32_t word)
{
    v->r = (uint8_t)word;
    v->g = (uint8_t)(word >> 8);
    v->b = (uint8_t)(word >> 16);
    v->a = 255;
}

static void TextureFromPage(GpuDrawState *draw, uint32_t page)
{
    draw->pageX = (int)(page & 0xF) * 64;
    draw->pageY = (int)((page >> 4) & 1) * 256 + (int)((page >> 11) & 1) * VRAM_HEIGHT; /* (the second half: gpu.h) */
    draw->depth = (int)((page >> 7) & 3);
}

static void StateFor(GpuDrawState *draw, int textured, int semi, int raw)
{
    memset(draw, 0, sizeof(*draw));
    draw->textured = textured;
    draw->raw = raw;
    draw->semi = semi ? (int)((state.texpage >> 5) & 3) : -1;
    draw->windowMaskX = state.windowMaskX;
    draw->windowMaskY = state.windowMaskY;
    draw->windowOffsetX = state.windowOffsetX;
    draw->windowOffsetY = state.windowOffsetY;
    draw->dither = (state.texpage >> 9) & 1;
    TextureFromPage(draw, state.texpage);
}

/* The console's GPU skips triangles wider than 1023 or taller than 511
   pixels (judged on the integer coordinates). A track face right beside the
   camera has a corner far off the screen, so it dropped out for a frame or two
   and left a hole in front of the ship. The port draws such a triangle when
   all its corners are projections it knows in 3D; one with a corner placed
   in 2D is still skipped. */
static int Skipped(const GpuVertex *v, const int32_t x[3], const int32_t y[3])
{
    int i;

    for (i = 0; i < 3; i++) {
        int j = (i + 1) % 3;
        if (abs(x[i] - x[j]) > 1023 || abs(y[i] - y[j]) > 511) {
            return v[0].w <= 0.0f || v[1].w <= 0.0f || v[2].w <= 0.0f;
        }
    }
    return 0;
}

/* The frame recorder's copy of a primitive as drawn (framecap.h); w: its words after the tag. */
static void Record(int kind, uint32_t code, int count, const int32_t *ix, const int32_t *iy, const GpuVertex *v,
                   const GpuDrawState *draw, int flags, const uint32_t *w, uint32_t page, uint32_t clut)
{
    FrameCapPrim *p = interpolating && !recordingBetween ? NULL : FrameCapAdd();
    int i, r;

    if (!p) {
        return;
    }
    p->kind = (uint8_t)kind;
    p->code = (uint8_t)code;
    p->count = (uint8_t)count;
    p->tpage = (uint16_t)page;
    p->clut = (uint16_t)clut;
    for (i = 0; i < count; i++) {
        p->ix[i] = (int16_t)(ix ? ix[i] : 0);
        p->iy[i] = (int16_t)(iy ? iy[i] : 0);
        p->v[i] = v[i];
    }
    if (draw) {
        p->draw = *draw;
    }
    p->address = (uint64_t)(uintptr_t)(w - 1);
    for (r = 0; r < regionCount; r++) {
        if ((const char *)w >= regions[r].start && (const char *)w < regions[r].start + regions[r].bytes) {
            flags |= FRAMECAP_FAR;
        }
    }
    p->flags = (uint8_t)flags;
    FrameCapTagRecord(p);
}

static void Polygon(const uint32_t *w, uint32_t code)
{
    int gouraud = (code & 0x10) != 0, quad = (code & 0x08) != 0, textured = (code & 0x04) != 0;
    int semi = (code & 0x02) != 0, raw = (code & 0x01) != 0;
    int count = quad ? 4 : 3, i, k = 0;
    GpuVertex v[4];
    int32_t ix[4], iy[4];
    const uint32_t *xys[4];
    uint32_t clut = 0, page = 0;
    GpuDrawState draw;
    int skip0, skip1 = 1, flags;

    memset(v, 0, sizeof(v));
    for (i = 0; i < count; i++) {
        uint32_t colour = (i == 0 || gouraud) ? w[k++] : w[0];
        uint32_t xy = w[k];
        Colour(&v[i], colour);
        xys[i] = &w[k++];
        ix[i] = (int32_t)(xy << 21) >> 21;
        iy[i] = (int32_t)(xy << 5) >> 21;
        if (textured) {
            uint32_t uv = w[k++];
            v[i].u = (float)(uv & 0xFF);
            v[i].v = (float)((uv >> 8) & 0xFF);
            if (i == 0) {
                clut = uv >> 16;
            } else if (i == 1) {
                page = uv >> 16;
            }
        }
        if (!textured || raw) {
            /* untextured colours are used as they are */
        }
    }
    flags = Vertices(v, xys, count) == (1u << count) - 1 && v[0].w > 0.0f ? FRAMECAP_PRECISE : 0;
    if (textured) {
        /* A textured polygon's page also becomes the GPU's current page. */
        SetTexpage(page);
    }
    StateFor(&draw, textured, semi, raw);
    if (textured) {
        TextureFromPage(&draw, page);
        draw.clutX = (int)(clut & 0x3F) * 16;
        draw.clutY = (int)((clut >> 6) & 0x3FF); /* (bit 15: a palette in the second half, gpu.h) */
    }
    if (!gouraud && !textured) {
        for (i = 1; i < count; i++) {
            v[i].r = v[0].r;
            v[i].g = v[0].g;
            v[i].b = v[0].b;
        }
    }
    if (NearCut(&draw, v, count)) {
        Record(FRAMECAP_POLYGON, code, count, ix, iy, v, &draw, flags, w, page, clut);
        return;
    }
    {
        int32_t tx[3] = {ix[0], ix[1], ix[2]}, ty[3] = {iy[0], iy[1], iy[2]};
        skip0 = Skipped(v, tx, ty);
    }
    if (quad) {
        int32_t tx[3] = {ix[1], ix[2], ix[3]}, ty[3] = {iy[1], iy[2], iy[3]};
        skip1 = Skipped(v + 1, tx, ty);
    }
    Record(FRAMECAP_POLYGON, code, count, ix, iy, v, &draw,
           flags | (skip0 ? FRAMECAP_SKIP0 : 0) | (quad && skip1 ? FRAMECAP_SKIP1 : 0), w, page, clut);
    if (!skip0) {
        GpuTriangles(&draw, v, 3);
    }
    if (quad && !skip1) {
        GpuTriangles(&draw, v + 1, 3);
    }
}

static void Line(const uint32_t *w, uint32_t code, int words)
{
    int gouraud = (code & 0x10) != 0, poly = (code & 0x08) != 0, semi = (code & 0x02) != 0;
    GpuDrawState draw;
    GpuVertex a, b;
    int k = 0;

    StateFor(&draw, 0, semi, 0);
    memset(&a, 0, sizeof(a));
    Colour(&a, w[k++]);
    {
        const uint32_t *word = &w[k++];
        Vertices(&a, &word, 1);
    }
    for (;;) {
        uint32_t colour = a.r | (a.g << 8) | ((uint32_t)a.b << 16);
        if (k >= words) {
            break;
        }
        if (gouraud) {
            if (poly && (w[k] & 0xF000F000u) == 0x50005000u) {
                break;
            }
            colour = w[k++];
        }
        if (k >= words || (poly && (w[k] & 0xF000F000u) == 0x50005000u)) {
            break;
        }
        memset(&b, 0, sizeof(b));
        Colour(&b, colour);
        {
            const uint32_t *word = &w[k++];
            Vertices(&b, &word, 1);
        }
        {
            GpuVertex segment[2] = {a, b};
            Record(FRAMECAP_LINE, code, 2, NULL, NULL, segment, &draw, 0, w, 0, 0);
        }
        GpuLine(&draw, &a, &b);
        a = b;
        if (!poly) {
            break;
        }
    }
}

static void Rectangle(const uint32_t *w, uint32_t code)
{
    int textured = (code & 0x04) != 0, semi = (code & 0x02) != 0, raw = (code & 0x01) != 0;
    int size = (code >> 3) & 3;
    int k = 0, width, height;
    GpuDrawState draw;
    GpuVertex v[4];
    uint32_t colour = w[k++], xy = w[k++], uv = 0;
    int x = (int32_t)(xy << 21) >> 21, y = (int32_t)(xy << 5) >> 21;

    if (textured) {
        uv = w[k++];
    }
    if (size == 0) {
        width = (int)(w[k] & 0x3FF);
        height = (int)((w[k] >> 16) & 0x1FF);
    } else {
        width = height = size == 1 ? 1 : size == 2 ? 8 : 16;
    }
    StateFor(&draw, textured, semi, raw);
    if (textured) {
        draw.clutX = (int)((uv >> 16) & 0x3F) * 16;
        draw.clutY = (int)((uv >> 22) & 0x3FF); /* (as a polygon's) */
    }
    memset(v, 0, sizeof(v));
    for (k = 0; k < 4; k++) {
        Colour(&v[k], colour);
        v[k].x = (float)(x + state.offsetX + ((k & 1) ? width : 0)) + hudShift;
        v[k].y = (float)(y + state.offsetY + ((k & 2) ? height : 0));
        v[k].u = (float)((uv & 0xFF) + ((k & 1) ? width : 0));
        v[k].v = (float)(((uv >> 8) & 0xFF) + ((k & 2) ? height : 0));
        if (hudActive) {
            HudPoint(&v[k]);
        }
    }
    Record(FRAMECAP_RECTANGLE, code, 4, NULL, NULL, v, &draw, 0, w, state.texpage, uv >> 16);
    GpuTriangles(&draw, v, 3);
    GpuTriangles(&draw, v + 1, 3);
}

static void Packet(const uint32_t *w, int words)
{
    uint32_t code = w[0] >> 24;

    if (words <= 0) {
        return;
    }
    if (code >= 0x20 && code < 0x40) {
        Polygon(w, code);
    } else if (code >= 0x40 && code < 0x60) {
        Line(w, code, words);
    } else if (code >= 0x60 && code < 0x80) {
        Rectangle(w, code);
    } else if (code == 0x02) {
        int x = (int)(w[1] & 0x3F0), y = (int)((w[1] >> 16) & 0x1FF);
        int width = (int)(((w[2] & 0x3FF) + 0xF) & ~0xF), height = (int)((w[2] >> 16) & 0x1FF);
        GpuVertex corners[2];

        memset(corners, 0, sizeof(corners));
        corners[0].x = (float)x;
        corners[0].y = (float)y;
        corners[1].x = (float)(x + width);
        corners[1].y = (float)(y + height);
        corners[0].r = (uint8_t)w[0];
        corners[0].g = (uint8_t)(w[0] >> 8);
        corners[0].b = (uint8_t)(w[0] >> 16);
        Record(FRAMECAP_FILL, code, 2, NULL, NULL, corners, NULL, 0, w, 0, 0);
        if (interpolating) {
            GpuClearArea(x, y, width, height, (uint8_t)w[0], (uint8_t)(w[0] >> 8), (uint8_t)(w[0] >> 16));
        } else {
            GpuFill(x, y, width, height, (uint8_t)w[0], (uint8_t)(w[0] >> 8), (uint8_t)(w[0] >> 16));
        }
    } else if (code >= 0xE1 && code <= 0xE6) {
        int i;
        for (i = 0; i < words; i++) {
            Environment(w[i]);
        }
    } else if (code == 0x00 || code == 0x01) {
        /* no operation / cache clear */
    } else {
        fprintf(stderr, "libgpu: packet %02x is not supported\n", (unsigned)code);
    }
}

/* ---- ordering tables ---- */

static u_long *NodeAfter(u_long *node)
{
    uint32_t link = *node & 0xFFFFFF;

    return link == LINK_END ? NULL : LinkPointer(link);
}

/* The first node from node on that carries a packet, or NULL. */
static u_long *PacketFrom(u_long *node)
{
    int guard = 0;

    while (node && !(*node >> 24)) {
        node = NodeAfter(node);
        if (++guard > 4000000) {
            fprintf(stderr, "libgpu: ordering table does not end\n");
            abort();
        }
    }
    return node;
}

static u_long *hudEntry, *hudTable;
static int SceneSprite(const u_long *node);

/* PacketFrom, noting whether the HUD's part of the table (from hudEntry on) was reached. */
static u_long *PacketFromNoting(u_long *node, int *inHud)
{
    while (node && !(*node >> 24)) {
        if (node == hudEntry) {
            *inHud = 1;
        }
        node = NodeAfter(node);
    }
    if (node == hudEntry) {
        *inHud = 1;
    }
    return node;
}

/* A packet of the table, the HUD's drawn as placed (hudPackets). */
static void SubmitNode(u_long *node, int inHud)
{
    hudPackets = inHud && !SceneSprite(node);
    Submit((const uint32_t *)(node + 1), (int)(*node >> 24));
    hudPackets = 0;
}

/* On the console DrawOTag returns at once and the DMA reads each packet only
   when the GPU is ready for it, while the game goes on building the next
   frame. A packet the game changes meanwhile is drawn as it was or as
   changed, and a packet the game links into the next frame's table leads the
   GPU on along the new links: some menus update both buffers' primitives,
   and the menu sparks are one set of primitives linked into whichever table
   is being built. The port draws a table when it is given. For the lockstep
   tests it records the table's packets in order, and when the game next
   waits for the GPU (DrawSync, ResetGraph) each packet and link as they are
   then, the latest the console could have read them; tools/trace/lockstep.py
   accepts any point at which the console's GPU went over from the one to the
   other. */
#define PENDING_TABLES 8
static struct {
    u_long *table;
    uint32_t *nodes;     /* link number and word count of each packet, in the order drawn */
    int count, capacity;
} pending[PENDING_TABLES];
static int pendingCount;

#define SEEN_SLOTS (1 << 18)
static u_long *seen[SEEN_SLOTS];

static int Seen(u_long *node)
{
    uint32_t slot = HashPointer(node) & (SEEN_SLOTS - 1);

    while (seen[slot]) {
        if (seen[slot] == node) {
            return 1;
        }
        slot = (slot + 1) & (SEEN_SLOTS - 1);
    }
    seen[slot] = node;
    return 0;
}

/* Records the packets from node on as they are now, until one already
   recorded: link number, the next packet's link number, the packet. */
static void RecordLateFrom(u_long *node)
{
    uint32_t words[2 + 255];

    for (node = PacketFrom(node); node && !Seen(node); node = PacketFrom(NodeAfter(node))) {
        u_long *next = PacketFrom(NodeAfter(node));
        int count = (int)(*node >> 24);

        words[0] = LinkId(node);
        words[1] = next ? LinkId(next) : LINK_END;
        memcpy(words + 2, node + 1, (size_t)count * 4);
        PortTraceEvent(TRACE_LATE_PACKET, words, 2 + count);
    }
}

static void ReadPendingTables(void)
{
    int i, j;

    if (!pendingCount) {
        return;
    }
    memset(seen, 0, sizeof(seen));
    for (i = 0; i < pendingCount; i++) {
        u_long *head = PacketFrom(pending[i].table);
        uint32_t first = head ? LinkId(head) : LINK_END;

        PortTraceEvent(TRACE_LATE_TABLE, &first, 1);
        RecordLateFrom(pending[i].table);
        for (j = 0; j < pending[i].count; j += 2) {
            RecordLateFrom(LinkPointer(pending[i].nodes[j]));
        }
    }
    pendingCount = 0;
}


void PsxHudTables(u_long *lastEntry, u_long *secondTable)
{
    hudEntry = lastEntry;
    hudTable = secondTable;
}

/* The HUD in the band of a split window, at this size of the console's (a little larger, against the band, than
   the console's HUD against its screen: the band is a wide strip of the window; its corner groups still fit). */
#define HUD_SPLIT_SCALE 0.65f

/* A HUD vertex (DrawAnchored): a box over the whole screen out to the sides of the wide picture (and the
   band's top and bottom); else scaled about the centre of the picture to the middle of the band, and moved
   (hudMoveX, hudMoveY). */
static void HudPoint(GpuVertex *v)
{
    float ox = (float)state.offsetX, oy = (float)state.offsetY;

    if (hudActive == HUD_FILL) {
        float sides = (float)PortScreenMargin() + 1.0f;

        v->x = v->x - ox < 160.0f ? ox - sides : ox + 320.0f + sides;
        if (hudFillBand) {
            v->y = v->y - oy < GPU_LINES / 2.0f ? oy + GPU_SPLIT_TOP - 1.0f : oy + GPU_SPLIT_TOP + GPU_SPLIT_ROWS + 1.0f;
        }
        return;
    }
    v->x = ox + 160.0f + (v->x - ox - 160.0f) * HUD_SPLIT_SCALE + hudMoveX;
    v->y = oy + GPU_SPLIT_TOP + GPU_SPLIT_ROWS / 2.0f + (v->y - oy - GPU_LINES / 2.0f) * HUD_SPLIT_SCALE + hudMoveY;
}

/* The rectangle a drawing packet covers, in drawing-area coordinates; 0 for packets that draw nothing. */
static int PacketBounds(const uint32_t *w, int words, int box[4])
{
    uint32_t code = w[0] >> 24;
    int i, k = 1, count = 0;
    int32_t xs[4], ys[4];

    if (code >= 0x20 && code < 0x40) {
        int gouraud = (code & 0x10) != 0, textured = (code & 0x04) != 0;

        for (i = 0; i < ((code & 0x08) ? 4 : 3) && k < words; i++) {
            if (i > 0 && gouraud) {
                k++;
            }
            xs[count] = (int32_t)(w[k] << 21) >> 21;
            ys[count++] = (int32_t)(w[k] << 5) >> 21;
            k += textured ? 2 : 1;
        }
    } else if (code >= 0x40 && code < 0x60) {
        xs[0] = (int32_t)(w[1] << 21) >> 21;
        ys[0] = (int32_t)(w[1] << 5) >> 21;
        k = (code & 0x10) ? 3 : 2;
        xs[1] = k < words ? (int32_t)(w[k] << 21) >> 21 : xs[0];
        ys[1] = k < words ? (int32_t)(w[k] << 5) >> 21 : ys[0];
        count = 2;
    } else if (code >= 0x60 && code < 0x80) {
        int size = (code >> 3) & 3, width, height;

        k = (code & 0x04) ? 3 : 2;
        if (size == 0) {
            width = k < words ? (int)(w[k] & 0x3FF) : 0;
            height = k < words ? (int)((w[k] >> 16) & 0x1FF) : 0;
        } else {
            width = height = size == 1 ? 1 : size == 2 ? 8 : 16;
        }
        xs[0] = (int32_t)(w[1] << 21) >> 21;
        ys[0] = (int32_t)(w[1] << 5) >> 21;
        xs[1] = xs[0] + width;
        ys[1] = ys[0] + height;
        count = 2;
    }
    if (!count) {
        return 0;
    }
    box[0] = box[2] = xs[0];
    box[1] = box[3] = ys[0];
    for (i = 1; i < count; i++) {
        box[0] = xs[i] < box[0] ? xs[i] : box[0];
        box[2] = xs[i] > box[2] ? xs[i] : box[2];
        box[1] = ys[i] < box[1] ? ys[i] : box[1];
        box[3] = ys[i] > box[3] ? ys[i] : box[3];
    }
    return 1;
}

/* Widescreen (gpu_gl.c): an ordering table drawn with the HUD's elements at the edges of the wide picture. The
   HUD's packets (from hudEntry on, or all of hudTable) are taken apart into elements, runs of packets that
   touch (the letters of a text, the parts of the speedometer), and an element wholly in the left or the right
   part of the console's picture goes to that edge; what reaches the middle (centred messages, the weapon icon,
   the pause menu) stays where the game put it. A box over the whole screen (the pause's shade) is an element of
   its own, stretched over the sides. A split window's band (half): the HUD smaller (HudPoint), the elements at
   the sides as far from the band's corners as they were from the screen's, the rest about the band's middle. */
#define ANCHOR_GAP 10    /* packets this close side by side are one element */
#define ANCHOR_LEFT 140  /* an element ending left of this goes to the left edge */
#define ANCHOR_RIGHT 180 /* ... beginning right of this to the right edge */
#define ANCHOR_OUTER 40  /* ... or within this of the screen's side and not across its middle (the speedometer's
                            number grows to the left as the speed does: three wide digits begin left of 180) */
#define ANCHOR_OFF 8     /* a packet reaching this far beyond the screen is the scene's (a trail at the camera, put
                            in the HUD's entry of the table at depth 0), not the HUD's */

/* The packets PortSceneSprite named (a few static primitives of the game's, so the set stays small). */
#define SCENE_SPRITES 96
static const void *sceneSprites[SCENE_SPRITES];
static int sceneSpriteCount;

void PortSceneSprite(const void *packet)
{
    int i;

    for (i = 0; i < sceneSpriteCount; i++) {
        if (sceneSprites[i] == packet) {
            return;
        }
    }
    if (sceneSpriteCount < SCENE_SPRITES) {
        sceneSprites[sceneSpriteCount++] = packet;
    }
}

/* ... and the memory of whole arrays of them (PortSceneRange). */
#define SCENE_RANGES 16
static struct {
    const char *start, *end;
} sceneRanges[SCENE_RANGES];
static int sceneRangeNext;

void PortSceneRange(const void *start, size_t bytes)
{
    int i;

    for (i = 0; i < SCENE_RANGES; i++) {
        if (sceneRanges[i].start == (const char *)start) {
            sceneRanges[i].end = (const char *)start + bytes;
            return;
        }
    }
    sceneRanges[sceneRangeNext].start = (const char *)start;
    sceneRanges[sceneRangeNext].end = (const char *)start + bytes;
    sceneRangeNext = (sceneRangeNext + 1) % SCENE_RANGES;
}

/* PortHudCentred's: those of the frame being built, and those of the tables drawn (PsxUpcomingTables). */
#define CENTRED_RANGES 16
static struct {
    const char *start, *end;
} centred[2][CENTRED_RANGES];
static int centredCount[2];
static int inPlace[2]; /* PortHudInPlace: the frame being built's, the tables drawn's */

void PortHudInPlace(void)
{
    inPlace[0] = 1;
}

void PortHudCentred(const void *start, const void *end)
{
    if (centredCount[0] < CENTRED_RANGES && end > start) {
        centred[0][centredCount[0]].start = (const char *)start;
        centred[0][centredCount[0]++].end = (const char *)end;
    }
}

static int HudCentred(const u_long *node)
{
    int i;

    if (inPlace[1]) {
        return 1;
    }
    for (i = 0; i < centredCount[1]; i++) {
        if ((const char *)node >= centred[1][i].start && (const char *)node < centred[1][i].end) {
            return 1;
        }
    }
    return 0;
}

static int SceneSprite(const u_long *node)
{
    int i;

    for (i = 0; i < sceneSpriteCount; i++) {
        if (sceneSprites[i] == (const void *)node) {
            return 1;
        }
    }
    for (i = 0; i < SCENE_RANGES; i++) {
        if ((const char *)node >= sceneRanges[i].start && (const char *)node < sceneRanges[i].end) {
            return 1;
        }
    }
    return 0;
}

static void DrawAnchored(u_long *p, int margin, int half)
{
    static u_long **hud;
    static int capacity;
    int count = 0, inHud = p == hudTable, i, first;
    u_long *node;

    for (node = p; node; node = NodeAfter(node)) {
        if (node == hudEntry) {
            inHud = 1;
        }
        if (!(*node >> 24)) {
            continue;
        }
        if (inHud && !SceneSprite(node)) {
            int box[4];

            if (!PacketBounds((const uint32_t *)(node + 1), (int)(*node >> 24), box) ||
                (box[0] >= -ANCHOR_OFF && box[1] >= -ANCHOR_OFF && box[2] <= 320 + ANCHOR_OFF &&
                 box[3] <= GPU_LINES + ANCHOR_OFF)) {
                if (count == capacity) {
                    capacity = capacity ? capacity * 2 : 1024;
                    hud = realloc(hud, (size_t)capacity * sizeof(*hud));
                }
                hud[count++] = node;
                continue;
            }
        }
        Submit((const uint32_t *)(node + 1), (int)(*node >> 24));
    }
    for (first = 0; first < count;) {
        int box[4], group[4], have = 0, last, side, whole;

        /* the element from `first`: packets that touch the ones before them; a box over the whole screen alone */
        whole = 0;
        for (last = first; last < count; last++) {
            int over;

            if (!PacketBounds((const uint32_t *)(hud[last] + 1), (int)(*hud[last] >> 24), box)) {
                continue;
            }
            over = box[0] <= 0 && box[1] <= 0 && box[2] >= 320 && box[3] >= GPU_LINES;
            if (have && (over || box[0] > group[2] + ANCHOR_GAP || box[2] < group[0] - ANCHOR_GAP ||
                         box[1] > group[3] || box[3] < group[1])) {
                break;
            }
            if (!have) {
                memcpy(group, box, sizeof(group));
                have = 1;
            } else {
                group[0] = box[0] < group[0] ? box[0] : group[0];
                group[1] = box[1] < group[1] ? box[1] : group[1];
                group[2] = box[2] > group[2] ? box[2] : group[2];
                group[3] = box[3] > group[3] ? box[3] : group[3];
            }
            if (over) {
                whole = 1;
                last++;
                break;
            }
        }
        for (i = first; i < last && have; i++) {
            if (HudCentred(hud[i])) {
                have = 0; /* (PortHudCentred: where the game put it) */
            }
        }
        side = !have ? 0
               : group[2] <= ANCHOR_LEFT || (group[2] < 160 && group[0] <= ANCHOR_OUTER) ? -1
               : group[0] >= ANCHOR_RIGHT || (group[0] > 160 && group[2] >= 320 - ANCHOR_OUTER) ? 1
               : 0;
        hudShift = 0.0f;
        hudMoveX = hudMoveY = 0.0f;
        if (whole) {
            hudActive = HUD_FILL;
            hudFillBand = half;
        } else if (half) {
            /* each corner's element as far from the band's corner as it was from the screen's, at the HUD's size */
            hudActive = HUD_SCALED;
            hudMoveX = (float)side * (160.0f * (1.0f - HUD_SPLIT_SCALE) + (float)margin);
            if (side && (group[3] <= GPU_LINES / 2 || group[1] >= GPU_LINES / 2)) {
                hudMoveY = (group[3] <= GPU_LINES / 2 ? -1.0f : 1.0f) * (GPU_SPLIT_ROWS / 2.0f - GPU_LINES / 2.0f * HUD_SPLIT_SCALE);
            }
        } else {
            hudActive = HUD_PLAIN;
            hudShift = (float)(side * margin);
        }
        hudPackets = 1;
        for (i = first; i < last; i++) {
            Submit((const uint32_t *)(hud[i] + 1), (int)(*hud[i] >> 24));
        }
        hudPackets = 0;
        hudActive = HUD_PLAIN;
        hudShift = 0.0f;
        first = last > first ? last : first + 1;
    }
}

/* ---- frame interpolation ----

   The game draws a frame every second field. Swap says which ordering tables it will draw once its fields have
   passed, and where (PsxUpcomingTables); until then every picture shown is those tables drawn with each
   projected vertex placed between the frame before and this one by the time passed (gte.h), into the area
   the frame is about to be drawn to afresh (the drawn picture only: the CPU's copy of video memory, the tests'
   traces and the frame recorder never see it). */
static struct {
    u_long *table, *second, *hudEntry;
    int left, top, width, height, offsetX, offsetY;
    int background;
    uint8_t r, g, b;
} upcoming;
static uint64_t frameDrawnTicks;
static uint64_t frameTicks = 33366667; /* between the game's frames: two fields */
static uint64_t frameDrawnField;       /* tests: the field the last frame was drawn in */

void PsxUpcomingTables(u_long *table, u_long *second, u_long *hudFirst, DRAWENV *env)
{
    memcpy(centred[1], centred[0], sizeof(centred[0])); /* (PortHudCentred: the frame's, for its tables) */
    centredCount[1] = centredCount[0];
    centredCount[0] = 0;
    inPlace[1] = inPlace[0];
    inPlace[0] = 0;
    upcoming.table = table;
    upcoming.second = second;
    upcoming.hudEntry = hudFirst;
    upcoming.left = env->clip.x;
    upcoming.top = env->clip.y;
    upcoming.width = env->clip.w;
    upcoming.height = env->clip.h;
    upcoming.offsetX = env->ofs[0];
    upcoming.offsetY = env->ofs[1];
    upcoming.background = env->isbg;
    upcoming.r = env->r0;
    upcoming.g = env->g0;
    upcoming.b = env->b0;
}

/* Tests: WIPEOUT_INTERPOLATE=n draws n pictures between the game's frames in each field of a fixed-step run
   (libetc.c). */
int PsxInterpolationTest(void)
{
    static int count = -1;

    if (count < 0) {
        const char *value = SDL_getenv("WIPEOUT_INTERPOLATE");
        count = value ? SDL_atoi(value) : 0;
    }
    return count;
}

int PsxInterpolationOn(void)
{
    int role = PortLinkRole(), on;

    if (role == LINK_SLAVE) {
        on = PortLinkSplit() && PortLinkInterpolate(); /* as the first player's game (whose window it is) does */
    } else {
        on = PsxInterpolationTest() > 0 ||
             (portSettings.frameRate != FRAME_RATE_CONSOLE && !portConfig.fixedStep && !portConfig.hidden);
        if (role == LINK_MASTER) {
            PortLinkSetInterpolate(on);
            on = on && PortLinkSplit(); /* a linked race only (the window split) */
        }
    }
    on = on && !PortTracing();
    GteKeepPast(on);
    return on;
}

int PsxCanInterpolate(void)
{
    return upcoming.table && upcoming.width > 0 && upcoming.height > 0 && !holdPicture && PsxInterpolationOn();
}

static void DrawTableAgain(u_long *p)
{
    u_long *node;
    int inHud = p == hudTable;

    if (PortLinkSplit()) {
        DrawAnchored(p, PortScreenMargin(), 1);
        return;
    }
    if (PortScreenMargin() && GpuRaceWide()) {
        DrawAnchored(p, PortScreenMargin(), 0);
        return;
    }
    for (node = PacketFromNoting(p, &inHud); node; node = PacketFromNoting(NodeAfter(node), &inHud)) {
        SubmitNode(node, inHud);
    }
}

void PsxDrawInterpolated(void)
{
    uint64_t now = SDL_GetTicksNS();

    PsxDrawInterpolatedAt(frameDrawnTicks ? (float)(now - frameDrawnTicks) / (float)frameTicks : 1.0f);
}

/* Tests: the picture the k-th of n of this field (two fields a frame). */
void PsxDrawInterpolatedTest(int k, int n)
{
    PsxDrawInterpolatedAt(((float)(PsxFieldCount() - frameDrawnField - 1) + (float)k / (float)n) / 2.0f);
}

static void DrawInterpolated(float along)
{
    u_long *savedHudEntry = hudEntry, *savedHudTable = hudTable;
    __typeof__(state) saved = state;

    interpolating = 1;
    interpolationAlong = along < 0.0f ? 0.0f : along > 1.0f ? 1.0f : along;
    GpuInterpolating(1);
    state.areaLeft = upcoming.left;
    state.areaTop = upcoming.top;
    state.areaRight = upcoming.left + upcoming.width - 1;
    state.areaBottom = upcoming.top + upcoming.height - 1;
    state.offsetX = upcoming.offsetX;
    state.offsetY = upcoming.offsetY;
    ApplyArea();
    GpuClearArea(upcoming.left, upcoming.top, upcoming.width, upcoming.height, upcoming.background ? upcoming.r : 0,
                 upcoming.background ? upcoming.g : 0, upcoming.background ? upcoming.b : 0);
    hudEntry = upcoming.hudEntry;
    hudTable = upcoming.second;
    DrawTableAgain(upcoming.table);
    if (upcoming.second) {
        DrawTableAgain(upcoming.second);
    }
    GpuFlush();
    hudEntry = savedHudEntry;
    hudTable = savedHudTable;
    state = saved;
    ApplyArea();
    interpolating = 0;
    GpuInterpolating(0);
}

void PsxDrawInterpolatedAt(float along)
{
    recordingBetween = FrameCapBetween(along < 0.0f ? 0.0f : along > 1.0f ? 1.0f : along, upcoming.offsetX,
                                       upcoming.offsetY, upcoming.left, upcoming.top, upcoming.width, upcoming.height);
    DrawInterpolated(along);
    if (recordingBetween) {
        /* (read back before it is shown: the window's picture is drawn over it) */
        FrameCapBetweenShown(upcoming.left, upcoming.top, 320, ShownLines(upcoming.height));
        recordingBetween = 0;
    }
    if (PortLinkSplit()) {
        ShowSplit(upcoming.left, upcoming.top);
    } else {
        GpuPresentInterpolated(upcoming.left, upcoming.top, upcoming.width, ShownLines(upcoming.height));
    }
}

/* The second player's game has no fields to wait in (the link paces it): it draws its frame's tables again
   while it waits for the first player's game, after the frame is drawn, each projected vertex as far from the
   frame before as the time since (the picture the console would show then, the frame before, moving to this
   one), and hands a picture over as often as the first player's game shows the window. Only while the frame is
   the picture (nothing else was drawn or loaded since: drawnFrame) and the projections are still the frame's. */
static __typeof__(upcoming) shownTables;
static uint32_t shownGteFrame;
static int shownComplete; /* the frame itself has been handed over (nothing moves until the next) */

void PsxLinkWaiting(void)
{
    static uint64_t lastPicture;
    __typeof__(upcoming) saved;
    uint64_t now = SDL_GetTicksNS(), period;
    float along;
    int w, h;

    if (PortLinkRole() != LINK_SLAVE || !PortLinkSplit()) {
        return;
    }
    GpuReadPictureDone(GPU_READ_LINK, 0, PutSplit, NULL); /* (read back while the game went on) */
    period = (uint64_t)PortLinkPresentPeriod() * 900; /* (a little sooner: a picture is ready when it is shown) */
    if (!shownTables.table || !drawnFrame.pending || shownComplete || GpuReadPictureBusy(GPU_READ_LINK) || !PsxInterpolationOn() ||
        GteFrameRecorded() != shownGteFrame || now - lastPicture < (period ? period : 8000000ull) ||
        !SplitSize(shownTables.left, &w, &h)) {
        return;
    }
    lastPicture = now;
    along = (float)(now - frameDrawnTicks) / (float)frameTicks;
    saved = upcoming;
    upcoming = shownTables;
    DrawInterpolated(along);
    upcoming = saved;
    GpuReadPictureStart(GPU_READ_LINK, shownTables.left, shownTables.top + GPU_SPLIT_TOP, 320, GPU_SPLIT_ROWS,
                        GpuShownMargin(shownTables.left, 320), w, h);
    drawnSinceRead = 0;
    shownComplete = along >= 1.0f;
}

void DrawOTag(u_long *p)
{
    u_long *node;
    int tracing = PortTracing() && pendingCount < PENDING_TABLES, inHud = p == hudTable;

    if (p == upcoming.table) {
        /* the frame interpolated towards is drawn: the next starts from here */
        uint64_t now = SDL_GetTicksNS();

        if (frameDrawnTicks && now - frameDrawnTicks > 8000000ull && now - frameDrawnTicks < 150000000ull) {
            frameTicks = (frameTicks * 3 + (now - frameDrawnTicks)) / 4;
        }
        frameDrawnTicks = now;
        frameDrawnField = PsxFieldCount();
        drawnFrame.pending = 1;
        drawnFrame.left = upcoming.left;
        drawnFrame.top = upcoming.top;
        drawnFrame.width = upcoming.width;
        drawnFrame.height = upcoming.height;
        drawnFrame.second = upcoming.second;
        if (PortLinkRole() == LINK_SLAVE) {
            shownTables = upcoming; /* (PsxLinkWaiting) */
            shownGteFrame = GteFrameRecorded();
            shownComplete = 0;
        }
        upcoming.table = NULL;
    } else if (p != drawnFrame.second) {
        ForgetDrawnFrame();
    }
    if (holdPicture && holdAreaCount < HOLD_AREAS) {
        holdAreas[holdAreaCount].left = state.areaLeft;
        holdAreas[holdAreaCount].top = state.areaTop;
        holdAreas[holdAreaCount].right = state.areaRight;
        holdAreas[holdAreaCount].bottom = state.areaBottom;
        holdAreaCount++;
    }
    FrameCapTable(state.offsetX, state.offsetY, state.areaLeft, state.areaTop, state.areaRight - state.areaLeft + 1,
                  state.areaBottom - state.areaTop + 1);
    /* each table drawn over what is there (the depth buffer: gpu_gl.c) */
    GpuClearDepth(state.areaLeft, state.areaTop, state.areaRight - state.areaLeft + 1, state.areaBottom - state.areaTop + 1);
    drawnSinceRead = 1;
    if (PortLinkSplit() && !tracing) {
        DrawAnchored(p, PortScreenMargin(), 1);
        GteFrameDrawn();
        return;
    }
    if (PortScreenMargin() && GpuRaceWide() && !tracing) {
        DrawAnchored(p, PortScreenMargin(), 0);
        GteFrameDrawn();
        return;
    }

    if (tracing) {
        pending[pendingCount].table = p;
        pending[pendingCount].count = 0;
    }
    for (node = PacketFromNoting(p, &inHud); node; node = PacketFromNoting(NodeAfter(node), &inHud)) {
        SubmitNode(node, inHud);
        if (tracing) {
            if (pending[pendingCount].count + 2 > pending[pendingCount].capacity) {
                pending[pendingCount].capacity = pending[pendingCount].capacity ? pending[pendingCount].capacity * 2 : 4096;
                pending[pendingCount].nodes = realloc(pending[pendingCount].nodes,
                                                      (size_t)pending[pendingCount].capacity * sizeof(uint32_t));
            }
            pending[pendingCount].nodes[pending[pendingCount].count++] = LinkId(node);
            pending[pendingCount].nodes[pending[pendingCount].count++] = *node >> 24;
        }
    }
    if (tracing) {
        PortTraceEvent(TRACE_TABLE, pending[pendingCount].nodes, pending[pendingCount].count);
        pendingCount++;
    }
    GteFrameDrawn();
}

void DrawPrim(void *p)
{
    uint32_t tag = *(uint32_t *)p;

    ForgetDrawnFrame();
    Submit((const uint32_t *)p + 1, (int)(tag >> 24));
}

/* The library keeps the video mode (0 NTSC, 1 PAL) in a word and returns the previous one. */
static int videoMode;

int SetVideoMode(int mode)
{
    int previous = videoMode;

    videoMode = mode;
    return previous;
}

