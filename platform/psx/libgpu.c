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

#include "gpu.h"
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

static uint32_t LinkId(void *pointer)
{
    uint32_t slot = HashPointer(pointer);

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
    if (id == 0 || id >= linkCount) {
        fprintf(stderr, "libgpu: broken ordering-table link %06x\n", id);
        abort();
    }
    return linkPointers[id];
}

void AddPrim(u_long *ot, u_long *p)
{
    *p = (*p & 0xFF000000u) | (*ot & 0x00FFFFFFu);
    *ot = (*ot & 0xFF000000u) | (LinkId(p) & 0x00FFFFFFu);
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

static void SetTexpage(uint32_t value)
{
    state.texpage = (state.texpage & ~0x7FFu) | (value & 0x7FF);
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

DRAWENV *PutDrawEnv(DRAWENV *env)
{
    int right = env->clip.x + env->clip.w - 1, bottom = env->clip.y + env->clip.h - 1;

    Environment(DrawModeWord(env->dfe, env->dtd, env->tpage));
    Environment(TextureWindowWord(&env->tw));
    state.areaLeft = env->clip.x < 0 ? 0 : env->clip.x;
    state.areaTop = env->clip.y < 0 ? 0 : env->clip.y;
    state.areaRight = right > 1023 ? 1023 : right;
    state.areaBottom = bottom > 511 ? 511 : bottom;
    state.offsetX = env->ofs[0];
    state.offsetY = env->ofs[1];
    ApplyArea();
    if (env->isbg) {
        GpuFill(env->clip.x, env->clip.y, env->clip.w, env->clip.h, env->r0, env->g0, env->b0);
    }
    return env;
}

DISPENV *PutDispEnv(DISPENV *env)
{
    state.displayX = env->disp.x;
    state.displayY = env->disp.y;
    state.displayW = env->disp.w;
    state.displayH = env->disp.h;
    return env;
}

void SetDispMask(int mask)
{
    state.displayEnabled = mask != 0;
}

int ResetGraph(int mode)
{
    if (mode == 0 || mode == 3) {
        memset(&state, 0, sizeof(state));
        state.areaRight = 1023;
        state.areaBottom = 511;
        GpuInit();
    }
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
    return 0;
}

/* The display: libetc's VSync calls this when a frame is shown. */
void PsxShowDisplay(void)
{
    GpuFlush();
    if (state.displayEnabled && state.displayW > 0 && state.displayH > 0) {
        GpuPresent(state.displayX, state.displayY, state.displayW, state.displayH);
    } else {
        GpuPresent(-1, -1, 0, 0);
    }
}

/* ---- video memory transfers ---- */

int LoadImage(RECT *rect, u_long *p)
{
    const uint16_t *pixels = (const uint16_t *)p;
    int x, y;

    for (y = 0; y < rect->h; y++) {
        for (x = 0; x < rect->w; x++) {
            psxVram[(rect->y + y) & (VRAM_HEIGHT - 1)][(rect->x + x) & (VRAM_WIDTH - 1)] = *pixels++;
        }
    }
    GpuVramChanged(rect->x, rect->y, rect->w, rect->h);
    return 0;
}

int StoreImage(RECT *rect, u_long *p)
{
    uint16_t *pixels = (uint16_t *)p;
    int x, y;

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
    int i, j;

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
    return 0;
}

int ClearImage(RECT *rect, u_char r, u_char g, u_char b)
{
    GpuFill(rect->x, rect->y, rect->w, rect->h, r, g, b);
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

static void Vertex(GpuVertex *v, uint32_t xy)
{
    int x = (int32_t)(xy << 21) >> 21, y = (int32_t)(xy << 5) >> 21;
    GtePrecise precise;

    if (GteLookupPrecise((int16_t)(xy & 0xFFFF), (int16_t)(xy >> 16), &precise)) {
        v->x = precise.x + (float)state.offsetX;
        v->y = precise.y + (float)state.offsetY;
        v->w = precise.z;
    } else {
        v->x = (float)(x + state.offsetX);
        v->y = (float)(y + state.offsetY);
        v->w = 0.0f; /* no depth: affine */
    }
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
    uint32_t clut = 0, page = 0;
    GpuDrawState draw;

    memset(v, 0, sizeof(v));
    for (i = 0; i < count; i++) {
        uint32_t colour = (i == 0 || gouraud) ? w[k++] : w[0];
        uint32_t xy = w[k++];
        Colour(&v[i], colour);
        Vertex(&v[i], xy);
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
    Vertex(&a, w[k++]);
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
        Vertex(&b, w[k++]);
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

void DrawOTag(u_long *p)
{
    u_long *node = p;
    int guard = 0;

    for (;;) {
        uint32_t tag = *node;
        int words = (int)(tag >> 24);

        if (words) {
            Packet((const uint32_t *)(node + 1), words);
        }
        if ((tag & 0xFFFFFF) == LINK_END) {
            break;
        }
        node = LinkPointer(tag & 0xFFFFFF);
        if (++guard > 4000000) {
            fprintf(stderr, "libgpu: ordering table does not end\n");
            abort();
        }
    }
    GteFrameDrawn();
}

void DrawPrim(void *p)
{
    uint32_t tag = *(uint32_t *)p;

    Packet((const uint32_t *)p + 1, (int)(tag >> 24));
}
