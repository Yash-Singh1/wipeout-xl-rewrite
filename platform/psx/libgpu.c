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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../trace.h"
#include "gpu.h"
#include "../link.h"
#include "gte.h"
#include "psyq.h"

uint16_t psxVram[VRAM_HEIGHT][VRAM_WIDTH];

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
        page = (u_short)(((tp & 3) << 7) | ((abr & 3) << 5) | ((y & 0x100) >> 4));
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
        page = (uint32_t)tpage & 0x1FF;
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
    state.texpage = (state.texpage & ~0x1FFu) | (value & 0x1FF);
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
static void Submit(const uint32_t *w, int words)
{
    PortTraceGp0(w, words);
    Packet(w, words);
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

DISPENV *PutDispEnv(DISPENV *env)
{
    DISPENV *given = env;

    if (!env) {
        env = &nullDispEnv;
    }
    PortTraceGp1(0x05000000u | (uint32_t)(env->disp.y & 0x1FF) << 10 | (uint32_t)(env->disp.x & 0x3FF));
    state.displayX = env->disp.x;
    state.displayY = env->disp.y;
    state.displayW = env->disp.w;
    state.displayH = env->disp.h;
    if (PortLinkRole() == LINK_SLAVE && PortLinkSplit()) {
        HandOverSplit(); /* the second player's game shows what it drew last (no fields pass in it) */
    }
    return given;
}

void SetDispMask(int mask)
{
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

/* The display: libetc's VSync calls this when a frame is shown. The GPU scans out 240 lines (NTSC)
   from the display start whatever height the environment gives (the menus say 180 and draw down to
   line 230); only a height above 256 is an interlaced picture of twice that. */
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
   SPLIT_TOP .. SPLIT_TOP + SPLIT_ROWS of its picture (the full width, half the height), the first in the
   upper half of the window and the second, which hands its band over, in the lower half. Each game's
   HUD (the last entry of its ordering table and the second table) is drawn at half size about the
   centre, so that it fits in the band. */
#define SPLIT_TOP 60
#define SPLIT_ROWS 120
static int drawnSinceRead;

/* The second player's game: the band of the picture on display goes to the first player's game, at
   each switch of the display (its Swap waits for no fields: the link paces it). */
static void HandOverSplit(void)
{
    if (drawnSinceRead && state.displayEnabled) {
        int w, h;
        uint8_t *pixels = GpuReadArea(state.displayX, state.displayY + SPLIT_TOP, 320, SPLIT_ROWS, &w, &h);

        PortLinkPutFrame(pixels, w, h);
        drawnSinceRead = 0;
    }
}

static void ShowSplit(void)
{
    if (PortLinkRole() == LINK_SLAVE) {
        return;
    }
    {
        static unsigned shownSequence;
        unsigned sequence = 0;
        int w = 0, h = 0;
        const uint8_t *lower = PortLinkGetFrame(&w, &h, &sequence);

        GpuPresentSplit(state.displayX, state.displayY + SPLIT_TOP, 320, SPLIT_ROWS, lower, w, h,
                        sequence != shownSequence);
        shownSequence = sequence;
    }
}

void PsxShowDisplay(void)
{
    GpuFlush();
    if (PortLinkSplit()) {
        ShowSplit();
        return;
    }
    if (holdPicture) {
        if (!HoldReleased()) {
            return;
        }
        holdPicture = 0;
    }
    if (state.displayEnabled && state.displayW > 0 && state.displayH > 0) {
        GpuPresent(state.displayX, state.displayY, state.displayW, state.displayH > 256 ? 480 : 240);
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

    TraceTransfer(0xA0000000u, rect, p);

    for (y = 0; y < rect->h; y++) {
        for (x = 0; x < rect->w; x++) {
            psxVram[(rect->y + y) & (VRAM_HEIGHT - 1)][(rect->x + x) & (VRAM_WIDTH - 1)] = *pixels++;
        }
    }
    GpuVramChanged(rect->x, rect->y, rect->w, rect->h);
    GpuWriteFramebuffer(rect->x, rect->y, rect->w, rect->h);
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
    GpuMoveFramebuffer(rect->x, rect->y, rect->w, rect->h, x, y);
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

/* A vertex at packet position xy, from its projection when it was found. */
static void Vertex(GpuVertex *v, uint32_t xy, const GtePrecise *precise)
{
    int x = (int32_t)(xy << 21) >> 21, y = (int32_t)(xy << 5) >> 21;

    if (precise) {
        v->x = precise->x + (float)state.offsetX;
        v->y = precise->y + (float)state.offsetY;
        v->w = precise->z;
        v->a = (uint8_t)(precise->fade * 255.0f + 0.5f);
    } else {
        v->x = (float)(x + state.offsetX);
        v->y = (float)(y + state.offsetY);
        v->w = 0.0f; /* no depth: affine */
    }
}

/* The vertices of one primitive. Perspective-correct texturing needs the
   depth of every vertex; when one is missing (a point the game placed in 2D)
   the whole primitive is drawn affine, as the console draws it, so that the
   two triangles of a quad agree. */
static void HudPoint(GpuVertex *v);
static int hudActive;

static unsigned Vertices(GpuVertex *v, const uint32_t *const *words, int count)
{
    GtePrecise precise[GTE_RESOLVE_MAX];
    uint32_t xy[GTE_RESOLVE_MAX];
    unsigned mask;
    int i, depth;

    for (i = 0; i < count; i++) {
        xy[i] = *words[i];
    }
    mask = GteResolvePrecise(xy, (const void *const *)words, count, precise);
    depth = mask == (1u << count) - 1;

    for (i = 0; i < count; i++) {
        Vertex(&v[i], xy[i], (mask >> i) & 1 ? &precise[i] : NULL);
        depth &= v[i].w > 0.0f;
    }
    for (i = 0; i < count && !depth; i++) {
        v[i].w = 0.0f;
    }
    if (hudActive && !mask) {
        for (i = 0; i < count; i++) {
            HudPoint(&v[i]); /* the HUD of a split window */
        }
    }
    return mask;
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
    draw->pageY = (int)((page >> 4) & 1) * 256;
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
   pixels (judged on the integer coordinates). */
static int TooLarge(const int32_t x[3], const int32_t y[3])
{
    int i;

    for (i = 0; i < 3; i++) {
        int j = (i + 1) % 3;
        if (abs(x[i] - x[j]) > 1023 || abs(y[i] - y[j]) > 511) {
            return 1;
        }
    }
    return 0;
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
    Vertices(v, xys, count);
    if (textured) {
        /* A textured polygon's page also becomes the GPU's current page. */
        SetTexpage(page);
    }
    StateFor(&draw, textured, semi, raw);
    if (textured) {
        TextureFromPage(&draw, page);
        draw.clutX = (int)(clut & 0x3F) * 16;
        draw.clutY = (int)((clut >> 6) & 0x1FF);
    }
    if (!gouraud && !textured) {
        for (i = 1; i < count; i++) {
            v[i].r = v[0].r;
            v[i].g = v[0].g;
            v[i].b = v[0].b;
        }
    }
    {
        int32_t tx[3] = {ix[0], ix[1], ix[2]}, ty[3] = {iy[0], iy[1], iy[2]};
        if (!TooLarge(tx, ty)) {
            GpuTriangles(&draw, v, 3);
        }
    }
    if (quad) {
        int32_t tx[3] = {ix[1], ix[2], ix[3]}, ty[3] = {iy[1], iy[2], iy[3]};
        if (!TooLarge(tx, ty)) {
            GpuTriangles(&draw, v + 1, 3);
        }
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
        draw.clutY = (int)((uv >> 22) & 0x1FF);
    }
    memset(v, 0, sizeof(v));
    for (k = 0; k < 4; k++) {
        Colour(&v[k], colour);
        v[k].x = (float)(x + state.offsetX + ((k & 1) ? width : 0));
        v[k].y = (float)(y + state.offsetY + ((k & 2) ? height : 0));
        v[k].u = (float)((uv & 0xFF) + ((k & 1) ? width : 0));
        v[k].v = (float)(((uv >> 8) & 0xFF) + ((k & 2) ? height : 0));
        if (hudActive) {
            HudPoint(&v[k]);
        }
    }
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
        GpuFill(x, y, width, height, (uint8_t)w[0], (uint8_t)(w[0] >> 8), (uint8_t)(w[0] >> 16));
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

static u_long *hudEntry, *hudTable;

void PsxHudTables(u_long *lastEntry, u_long *secondTable)
{
    hudEntry = lastEntry;
    hudTable = secondTable;
}

/* A HUD vertex, halved about the centre of the picture (the band of a split window). */
static void HudPoint(GpuVertex *v)
{
    float ox = (float)state.offsetX, oy = (float)state.offsetY;

    v->x = ox + 160.0f + (v->x - ox - 160.0f) * 0.5f;
    v->y = oy + 120.0f + (v->y - oy - 120.0f) * 0.5f;
}

void DrawOTag(u_long *p)
{
    u_long *node;
    int tracing = PortTracing() && pendingCount < PENDING_TABLES;

    if (holdPicture && holdAreaCount < HOLD_AREAS) {
        holdAreas[holdAreaCount].left = state.areaLeft;
        holdAreas[holdAreaCount].top = state.areaTop;
        holdAreas[holdAreaCount].right = state.areaRight;
        holdAreas[holdAreaCount].bottom = state.areaBottom;
        holdAreaCount++;
    }
    drawnSinceRead = 1;
    if (PortLinkSplit() && !tracing) {
        hudActive = p == hudTable;
        for (node = p; node; node = NodeAfter(node)) {
            if (node == hudEntry) {
                hudActive = 1;
            }
            if (*node >> 24) {
                Submit((const uint32_t *)(node + 1), (int)(*node >> 24));
            }
        }
        hudActive = 0;
        GteFrameDrawn();
        return;
    }

    if (tracing) {
        pending[pendingCount].table = p;
        pending[pendingCount].count = 0;
    }
    for (node = PacketFrom(p); node; node = PacketFrom(NodeAfter(node))) {
        Submit((const uint32_t *)(node + 1), (int)(*node >> 24));
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
