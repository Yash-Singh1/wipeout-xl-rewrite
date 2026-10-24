/* The PC version's 2D drawing (d3dpoly.c, d3d.c) on the port's (pc.h).

   The PC builds PlayStation primitives and hands each to Direct3D at once, with a depth for its Z-buffer: smaller is
   nearer (its text at 0.05, the menus' boxes at 0.06 and 0.07, the backdrop's tiles at 0.99). Here each is copied as
   the call is made, onto the PC page its texture code names (a page of colour depth 3, gpu.h), its corners kept
   exact (the PC's 640x480 layout puts them on half pixels of the console's 320x240: each corner is bound to its
   precise position, gte.h); at the end of the frame (PcDrawEndFrame) they are linked into the second ordering table
   by their depth, in the order they were made for the same depth (the Z-buffer's: what comes later at the same
   depth is drawn over). The copies live in a pool for each of the two screens, as the ordering tables do.

   A backdrop the PC draws behind its 3D (the menus' tiles and panels, at depth 0.96 and deeper, behind their turning
   models) goes at the far end of the first ordering table instead, under the models drawn there: from the depth
   pcBackdropDepth on (set by the menus; none otherwise). */
#include <string.h>

#include "pc.h"
#include "../../include/port_platform.h"
#include "../../../platform/psx/gte.h"


#define OT_ENTRIES 0x2000
#define OT2_ENTRIES 0x640
#define BACKDROP_ENTRIES 64 /* the far end of OT[] the backdrop takes, by depth */
#define POOL_BYTES (256 * 1024)
#define MAX_PRIMS 8192

static char pool[2][POOL_BYTES];
static int poolUsed[2];
static struct {
    void *prim;
    int entry;
    int backdrop; /* (linked into OT[], entry counted from its far end) */
} made[MAX_PRIMS];
static int madeCount;
static int centredFrom = -1; /* PcCentred: where in the pool the packets to stay in place begin */
static int blinkFrame;       /* the PC's blinkFrame (display.c): sixtieths of a second */
float pcBackdropDepth = 2.0f;

u_short PcTPage(int page, int abr)
{
    int n = page + 1;

    /* the page's number: x / 64 in bits 0-3, the second row in bit 4, the second half of video memory in bit 11 */
    return (u_short)((n & 15) | (n >> 4 & 1) << 4 | (abr & 3) << 5 | 3 << 7 | (n >> 5 & 1) << 11);
}

int PcPageOfCode(int code)
{
    if (code > 15 && code < 100) {
        return code - 10;
    }
    return code < 16 ? code - 5 : code;
}

/* Room for a primitive in this frame's pool, and its place in the table by depth; NULL when the frame has too many. */
static void *Make(int bytes, float depth)
{
    char *p;
    int entry = (int)(depth * (OT2_ENTRIES - 1));

    if (madeCount >= MAX_PRIMS || poolUsed[CurrentScreen] + bytes > POOL_BYTES) {
        return NULL;
    }
    p = pool[CurrentScreen] + poolUsed[CurrentScreen];
    poolUsed[CurrentScreen] += (bytes + 7) & ~7;
    made[madeCount].prim = p;
    made[madeCount].backdrop = depth >= pcBackdropDepth;
    if (made[madeCount].backdrop) {
        entry = (int)((1.0f - depth) * (BACKDROP_ENTRIES - 1));
    }
    made[madeCount].entry = entry < 0 ? 0 : entry >= OT2_ENTRIES ? OT2_ENTRIES - 1 : entry;
    madeCount++;
    return p;
}

/* A corner at (x, y) of the space given: its packet coordinates the console's pixel, bound to the exact place. */
static void Corner(short *xy, int x, int y, int space)
{
    float px = space == PC_SPACE_640 ? x * 0.5f : (float)x, py = space == PC_SPACE_640 ? y * 0.5f : (float)y;
    int ix = space == PC_SPACE_640 ? x >> 1 : x, iy = space == PC_SPACE_640 ? y >> 1 : y;

    xy[0] = (short)ix;
    xy[1] = (short)iy;
    if (space == PC_SPACE_640) {
        GteBindPrecise(xy, GteRecordProjection((uint32_t)(u_short)ix | (uint32_t)(u_short)iy << 16, px, py, 0.0f, 1.0f));
    }
}

void PcDrawFT4(float depth, const POLY_FT4 *poly, int blendMode, int space)
{
    POLY_FT4 *p = Make(sizeof(POLY_FT4), depth);
    int page = PcPageOfCode(poly->tpage);

    if (!p || page < 0 || page > 62) {
        return;
    }
    *p = *poly;
    SetPolyFT4(p);
    SetSemiTrans(p, blendMode != 0);
    p->tpage = PcTPage(page, 1); /* (the translucent pass adds) */
    p->clut = 0;
    Corner(&p->x0, poly->x0, poly->y0, space);
    Corner(&p->x1, poly->x1, poly->y1, space);
    Corner(&p->x2, poly->x2, poly->y2, space);
    Corner(&p->x3, poly->x3, poly->y3, space);
}

void PcDrawF4(float depth, const POLY_F4 *poly, int space)
{
    POLY_F4 *p = Make(sizeof(POLY_F4), depth);

    if (!p) {
        return;
    }
    *p = *poly;
    SetPolyF4(p);
    Corner(&p->x0, poly->x0, poly->y0, space);
    Corner(&p->x1, poly->x1, poly->y1, space);
    Corner(&p->x2, poly->x2, poly->y2, space);
    Corner(&p->x3, poly->x3, poly->y3, space);
}

void PcDrawG4(float depth, const POLY_G4 *poly, int blendMode, int space)
{
    POLY_G4 *p = Make(sizeof(POLY_G4), depth);

    if (!p) {
        return;
    }
    *p = *poly;
    SetPolyG4(p);
    SetSemiTrans(p, blendMode != 0);
    Corner(&p->x0, poly->x0, poly->y0, space);
    Corner(&p->x1, poly->x1, poly->y1, space);
    Corner(&p->x2, poly->x2, poly->y2, space);
    Corner(&p->x3, poly->x3, poly->y3, space);
}

void PcDrawPacket(float depth, const void *packet, int bytes)
{
    void *p = Make(bytes, depth);

    if (p) {
        memcpy(p, packet, (size_t)bytes);
    }
}

void PcCentred(int on)
{
    if (on) {
        centredFrom = poolUsed[CurrentScreen];
    } else if (centredFrom >= 0) {
        PortHudCentred(pool[CurrentScreen] + centredFrom, pool[CurrentScreen] + poolUsed[CurrentScreen]);
        centredFrom = -1;
    }
}

int PcBlink(void)
{
    return blinkFrame % 30 < 20;
}

void PcDrawEndFrame(void)
{
    int i;

    /* each added at the front of its entry, the last made first: the first made ends up drawn first */
    for (i = madeCount - 1; i >= 0; i--) {
        if (made[i].backdrop) {
            AddPrim(OT[CurrentScreen] + OT_ENTRIES - 1 - made[i].entry, made[i].prim);
        } else {
            AddPrim(OT2[CurrentScreen] + made[i].entry, made[i].prim);
        }
    }
    madeCount = 0;
    blinkFrame += 2; /* (a frame is two fields) */
    poolUsed[CurrentScreen ^ 1] = 0; /* the next frame's (the other screen's, two frames old) */
}
