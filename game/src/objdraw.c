/* The object renderer.  On the console these were three hand-written assembly routines (AsmDrawObject,
   AsmDrawObjectLit and AsmDrawObjectFixed in asm/800.s of the decomp); this is their translation to C, running
   on the GTE of the platform.

   Each routine transforms the vertices of an object into the camera's view (the skeleton's transform), then
   walks its primitives and builds a packet for every visible one in the primitive buffer (nextPrim), linking
   it into the ordering table of the current screen.  A primitive whose vertices are all on screen is drawn;
   one with a vertex rejected by the GTE is skipped.  Primitives of a type the routines do not know end the
   object (the rest of its primitives are not drawn).

   The three differ in how they treat the primitives:
     AsmDrawObject       the plain renderer: gouraud textured polygons are drawn flat, textured with the colour
                         of their first vertex; single sided polygons facing away are dropped; translucent ones
                         get the semi-transparency bit; depth is the average of the vertices' depths; billboard
                         sprites (types 10 and 11) are scaled by distance and rotated by `roll`.
     AsmDrawObjectLit    the same, except that gouraud polygons keep their colours, flat textured polygons are
                         tinted by `colour`, and everything is linked 0x78 slots nearer.
     AsmDrawObjectFixed  for the sky: every polygon is drawn flat and linked into one fixed slot, with
                         the facing test always applied and no sprites. */
#include "functions.h"
#include "drawasm.h"
#include "../../platform/psx/gte.h"
#include "portfar.h"

extern short   CurrentScreen;
extern u_long *OT[2];
extern Texture *textureTable[];

/* An object vertex after the transform: the screen position, the depth and the GTE flag bits that matter
   (the upper half of the flag word, masked with 0x466).  8 bytes, as the console kept them in the scratchpad. */
typedef struct ProjectedVertex {
    int     screen;     /* x in the low half, y in the high half */
    u_short depth;
    u_short flag;
} ProjectedVertex;

/* Room for the vertices of the biggest object: the console kept them in the scratchpad up to 128 vertices
   and in a 3584-byte area of RAM (448 vertices) beyond. Port: room for more, as the guests' track models in the
   class menu have (the original Wipeout's Silverstream 476: they wrote over what followed). */
#define MAX_OBJECT_VERTICES 1024
static ProjectedVertex projected[MAX_OBJECT_VERTICES];

/* Port: the far view (portfar.h) draws objects through these routines with its own projection: the view,
   and each vertex's precise screen position and depth. */
static const PortView *farView;
static double farX[MAX_OBJECT_VERTICES], farY[MAX_OBJECT_VERTICES], farZ[MAX_OBJECT_VERTICES];

/* Port: the precise projection of each vertex (gte.h), bound to the packet vertices made from it. */
static uint32_t projectedHandle[MAX_OBJECT_VERTICES];

static void PutScreenXY(short *x, short *y, const ProjectedVertex *v)
{
    StoreScreenXY(x, y, v->screen);
    GteBindPrecise(x, projectedHandle[v - projected]);
}

typedef enum DrawKind {
    DRAW_PLAIN,
    DRAW_LIT,
    DRAW_FIXED
} DrawKind;

/* The ordering table slot is the depth (OTZ) of the polygon's vertices minus 0x78 in the lit renderer, or
   not less than 0. */
#define LIT_DEPTH_BIAS 0x78

_Static_assert(sizeof(F3) == 0x10 && sizeof(F4) == 0x10 && sizeof(FT3) == 0x1C && sizeof(FT4) == 0x20, "PRM primitive sizes");
_Static_assert(sizeof(G3) == 0x18 && sizeof(G4) == 0x1C && sizeof(GT3) == 0x24 && sizeof(GT4) == 0x2C && sizeof(SPR) == 0x10, "PRM primitive sizes");

/* Port: frame interpolation (gte.h): a vertex is known by the model's vertex and which drawing of the model it is
   this frame. The ships share models, and DrawShips says which ship it draws (PortInterpInstance); other models
   drawn more than once count their drawings in order. The far view draws what the game drew in part or not at
   all: its drawing is the game's. */
static int      nextInstance = -1;
static uint64_t currentDrawing; /* the key of the drawing being made */
static uint64_t nearPrimitive;  /* ... and of the primitive DrawNearPrimitive cuts */

void PortInterpInstance(int instance)
{
    nextInstance = instance;
}

static uint64_t DrawingKey(Object *object)
{
    static struct {
        Object  *object;
        uint32_t frame;
        int      count;
    } drawings[512];
    uint32_t frame = GteFrameNumber(), slot = (uint32_t)(((uintptr_t)object >> 4) * 2654435761u) & 511, probe;
    int      instance = nextInstance;

    nextInstance = -1;
    if (instance < 0 && !farView) {
        instance = 0;
        for (probe = 0; probe < 512; probe++, slot = (slot + 1) & 511) {
            if (drawings[slot].frame != frame) {
                drawings[slot].object = object;
                drawings[slot].frame = frame;
                drawings[slot].count = 0;
            }
            if (drawings[slot].object == object) {
                instance = drawings[slot].count++;
                break;
            }
        }
    }
    return GteKeyMix((uintptr_t)object, (uint64_t)(instance < 0 ? 0 : instance));
}

/* Transforms the object's vertices. */
static void ProjectVertices(Object *object)
{
    SVECTOR         *vertex = object->vertices;
    ProjectedVertex *out = projected;
    int              count = (u_short)object->vertexCount;
    uint64_t         drawing = currentDrawing = DrawingKey(object); /* Port: frame interpolation */

    if (farView) {
        int i;

        for (i = 0; i < count && i < MAX_OBJECT_VERTICES; i++) {
            PortFarVertex v;

            GteKeyNext(0, GteKeyMix(drawing, (uint64_t)i));
            PortFarProject(farView, vertex[i].vx, vertex[i].vy, vertex[i].vz, &v);
            projected[i].screen = v.screen;
            projected[i].depth = (u_short)(v.z < 0 ? 0 : v.z > 65535 ? 65535 : v.z);
            projected[i].flag = (u_short)(v.rejected || count > MAX_OBJECT_VERTICES);
            projectedHandle[i] = v.handle;
            farX[i] = v.x;
            farY[i] = v.y;
            farZ[i] = v.z;
        }
        return;
    }

    /* Port: the console also jumped out when "all vertices rejected", but tested the wrong register
       (the flag bits are masked with 0x466, so a bit-0 test never fires) and so never left early. */
    do {
        GteWriteData(0, (vertex->vx & 0xFFFF) | (int)((u_int)(int)vertex->vy << 16));
        GteWriteData(1, (vertex->vz & 0xFFFF) | (int)((u_int)(u_short)vertex->pad << 16));
        GteKeyNext(0, GteKeyMix(drawing, (uint64_t)(vertex - object->vertices))); /* Port: frame interpolation */
        GteExecute(GTE_RTPS);
        out->depth = (u_short)GteReadData(19);
        out->flag = (u_short)(((u_int)GteReadControl(31) >> 12) & 0x466);
        out->screen = GteReadData(14);
        if (out - projected < MAX_OBJECT_VERTICES) {
            projectedHandle[out - projected] = GtePreciseHandle(14);
        }
        vertex++;
        out++;
    } while (--count > 0);
}

/* A polygon is drawn only when none of its vertices was rejected. */
/* Port: the far view leaves out the primitives the game drew (forceReject), and learns which the game
   culled as facing away (culled). */
static int forceReject, culled;
static const ProjectedVertex *culledVertex[3]; /* Port: the culled polygon's first three vertices */

static int AnyRejected(const ProjectedVertex *a, const ProjectedVertex *b, const ProjectedVertex *c, const ProjectedVertex *d)
{
    if (forceReject) {
        return 1;
    }
    return (a->flag | b->flag | c->flag | (d ? d->flag : 0)) != 0;
}

/* The facing test: the winding of the first three vertices on screen. */
static int FacesAway(const ProjectedVertex *a, const ProjectedVertex *b, const ProjectedVertex *c)
{
    if (farView) {
        int i = (int)(a - projected), j = (int)(b - projected), k = (int)(c - projected);

        return (farX[j] - farX[i]) * (farY[k] - farY[i]) - (farY[j] - farY[i]) * (farX[k] - farX[i]) <= 0;
    }
    GteWriteData(12, a->screen);
    GteWriteData(13, b->screen);
    GteWriteData(14, c->screen);
    GteExecute(GTE_NCLIP);
    return (int)GteReadData(24) <= 0;
}

/* The ordering table entry for a polygon: the average depth of its three or four vertices. */
static u_long *DepthSlot(DrawKind kind, u_long *ordering, int fixedSlot,
                         const ProjectedVertex *a, const ProjectedVertex *b, const ProjectedVertex *c, const ProjectedVertex *d)
{
    int slot;

    if (kind == DRAW_FIXED) {
        return ordering + fixedSlot;
    }
    if (farView) {
        /* AVSZ3/AVSZ4 on the unsaturated depths */
        double sum = farZ[a - projected] + farZ[b - projected] + farZ[c - projected] + (d ? farZ[d - projected] : 0);
        int    zsf = GteReadControl(d ? 30 : 29);

        slot = (int)(((long long)zsf * (long long)sum) >> 12);
        if (kind == DRAW_LIT) {
            slot = slot < LIT_DEPTH_BIAS ? 0 : slot - LIT_DEPTH_BIAS;
        }
        return PortFarSlot(slot, sum / (d ? 4 : 3));
    }
    if (d) {
        GteWriteData(16, a->depth);
        GteWriteData(17, b->depth);
        GteWriteData(18, c->depth);
        GteWriteData(19, d->depth);
        GteExecute(GTE_AVSZ4);
    } else {
        GteWriteData(17, a->depth);
        GteWriteData(18, b->depth);
        GteWriteData(19, c->depth);
        GteExecute(GTE_AVSZ3);
    }
    slot = GteReadData(7);
    if (kind == DRAW_LIT) {
        slot -= LIT_DEPTH_BIAS;
        if (slot < 0) {
            slot = 0;
        }
    }
    return ordering + slot;
}

/* The packet's code: the polygon's own, with semi-transparency for translucent primitives (not in the sky). */
static u_char PolyCode(DrawKind kind, u_char code, short primFlag)
{
    if (kind != DRAW_FIXED && (primFlag & Translucent)) {
        code |= 2;
    }
    return code;
}

/* Whether a polygon that faces away is dropped: single sided ones are in the plain and lit renderers, all of
   them in the sky. */
static int PreciseFacing(const ProjectedVertex *a, const ProjectedVertex *b, const ProjectedVertex *c);

/* Port: the menus' track models are drawn two-sided (port_platform.h). */
static int twoSided;

void PortTwoSided(int on)
{
    twoSided = on;
}

/* Port: ... and the guests' (port_platform.h). */
static int exactDepth;

void PortExactDepth(int on)
{
    exactDepth = on;
}

static int Culled(DrawKind kind, short primFlag, const ProjectedVertex *a, const ProjectedVertex *b, const ProjectedVertex *c)
{
    int facing;

    if (kind != DRAW_FIXED && (!(primFlag & SingleSided) || (twoSided && !PortTracing()))) {
        return 0;
    }
    /* Port: judged on the precise positions the polygon is drawn from, when there are: on the corners rounded to
       whole pixels a polygon seen nearly edge on (a sliver of a turning ship) faced away in one frame and not in
       the next, and flickered. Not in the lockstep tests, which compare the console's drawing. */
    facing = farView || PortTracing() ? -1 : PreciseFacing(a, b, c);
    culled = facing >= 0 ? facing : FacesAway(a, b, c);
    culledVertex[0] = a;
    culledVertex[1] = b;
    culledVertex[2] = c;
    return culled;
}

/* Port: whether a polygon the game culled faces away for certain. The game judges on the screen positions
   rounded to whole pixels; a far polygon a few pixels across, seen nearly edge on, can come out facing away
   in one frame and not in the next. Such a polygon is left to the far view (portfar.h), which judges on the
   precise positions: a black speck of background flickered where it was missing. */
/* Port: 1 when a polygon faces away judged on its corners' precise positions, 0 when it faces the camera, -1
   when they are not all known. */
static int PreciseFacing(const ProjectedVertex *a, const ProjectedVertex *b, const ProjectedVertex *c)
{
    GtePrecise p[3];

    if (!GtePreciseOf(projectedHandle[a - projected], &p[0]) || !GtePreciseOf(projectedHandle[b - projected], &p[1]) ||
        !GtePreciseOf(projectedHandle[c - projected], &p[2])) {
        return -1;
    }
    return (p[1].x - p[0].x) * (p[2].y - p[0].y) - (p[1].y - p[0].y) * (p[2].x - p[0].x) <= 0;
}

static int CulledForCertain(const ProjectedVertex *a, const ProjectedVertex *b, const ProjectedVertex *c)
{
    GtePrecise p[3];

    if (farView || !GtePreciseOf(projectedHandle[a - projected], &p[0]) ||
        !GtePreciseOf(projectedHandle[b - projected], &p[1]) || !GtePreciseOf(projectedHandle[c - projected], &p[2])) {
        return 1;
    }
    return (p[1].x - p[0].x) * (p[2].y - p[0].y) - (p[1].y - p[0].y) * (p[2].x - p[0].x) <= 0;
}

static void SetRgb(u_char *r, u_char *g, u_char *b, const CVECTOR *colour)
{
    *r = colour->r;
    *g = colour->g;
    *b = colour->b;
}

/* A flat textured triangle, from a flat or a gouraud textured one (both start with the same fields; a gouraud
   one is drawn with the colour of its first vertex). */
static char *DrawTexturedTriangle(DrawKind kind, char *out, u_long *ordering, int fixedSlot, const FT3 *prim,
                                  const CVECTOR *colour)
{
    ProjectedVertex *v0 = &projected[prim->coords[0]], *v1 = &projected[prim->coords[1]], *v2 = &projected[prim->coords[2]];
    POLY_FT3        *poly = (POLY_FT3 *)out;

    if (AnyRejected(v0, v1, v2, 0) || Culled(kind, prim->flag, v0, v1, v2)) {
        return out;
    }
    PutScreenXY(&poly->x0, &poly->y0, v0);
    PutScreenXY(&poly->x1, &poly->y1, v1);
    PutScreenXY(&poly->x2, &poly->y2, v2);
    ((u_char *)poly)[3] = 7;
    poly->code = PolyCode(kind, 0x24, prim->flag);
    poly->clut = prim->cba;
    poly->tpage = prim->tsb;
    poly->u0 = prim->u0; poly->v0 = prim->v0;
    poly->u1 = prim->u1; poly->v1 = prim->v1;
    poly->u2 = prim->u2; poly->v2 = prim->v2;
    SetRgb(&poly->r0, &poly->g0, &poly->b0, colour);
    AddPrim(DepthSlot(kind, ordering, fixedSlot, v0, v1, v2, 0), poly);
    return out + sizeof(POLY_FT3);
}

static char *DrawTexturedQuad(DrawKind kind, char *out, u_long *ordering, int fixedSlot, const FT4 *prim,
                              const CVECTOR *colour)
{
    ProjectedVertex *v0 = &projected[prim->coords[0]], *v1 = &projected[prim->coords[1]];
    ProjectedVertex *v2 = &projected[prim->coords[2]], *v3 = &projected[prim->coords[3]];
    POLY_FT4        *poly = (POLY_FT4 *)out;

    if (AnyRejected(v0, v1, v2, v3) || Culled(kind, prim->flag, v0, v1, v2)) {
        return out;
    }
    PutScreenXY(&poly->x0, &poly->y0, v0);
    PutScreenXY(&poly->x1, &poly->y1, v1);
    PutScreenXY(&poly->x2, &poly->y2, v2);
    PutScreenXY(&poly->x3, &poly->y3, v3);
    ((u_char *)poly)[3] = 9;
    poly->code = PolyCode(kind, 0x2C, prim->flag);
    poly->clut = prim->cba;
    poly->tpage = prim->tsb;
    poly->u0 = prim->u0; poly->v0 = prim->v0;
    poly->u1 = prim->u1; poly->v1 = prim->v1;
    poly->u2 = prim->u2; poly->v2 = prim->v2;
    poly->u3 = prim->u3; poly->v3 = prim->v3;
    SetRgb(&poly->r0, &poly->g0, &poly->b0, colour);
    AddPrim(DepthSlot(kind, ordering, fixedSlot, v0, v1, v2, v3), poly);
    return out + sizeof(POLY_FT4);
}

/* Gouraud textured polygons keep their colours (the lit renderer). */
static char *DrawGouraudTexturedTriangle(DrawKind kind, char *out, u_long *ordering, const GT3 *prim)
{
    ProjectedVertex *v0 = &projected[prim->coords[0]], *v1 = &projected[prim->coords[1]], *v2 = &projected[prim->coords[2]];
    POLY_GT3        *poly = (POLY_GT3 *)out;

    if (AnyRejected(v0, v1, v2, 0) || Culled(kind, prim->flag, v0, v1, v2)) {
        return out;
    }
    PutScreenXY(&poly->x0, &poly->y0, v0);
    PutScreenXY(&poly->x1, &poly->y1, v1);
    PutScreenXY(&poly->x2, &poly->y2, v2);
    ((u_char *)poly)[3] = 9;
    poly->code = PolyCode(kind, 0x34, prim->flag);
    poly->clut = prim->cba;
    poly->tpage = prim->tsb;
    poly->u0 = prim->u0; poly->v0 = prim->v0;
    poly->u1 = prim->u1; poly->v1 = prim->v1;
    poly->u2 = prim->u2; poly->v2 = prim->v2;
    SetRgb(&poly->r0, &poly->g0, &poly->b0, &prim->colour[0]);
    poly->r1 = prim->colour[1].r; poly->g1 = prim->colour[1].g; poly->b1 = prim->colour[1].b; poly->p1 = prim->colour[1].cd;
    poly->r2 = prim->colour[2].r; poly->g2 = prim->colour[2].g; poly->b2 = prim->colour[2].b; poly->p2 = prim->colour[2].cd;
    AddPrim(DepthSlot(kind, ordering, 0, v0, v1, v2, 0), poly);
    return out + sizeof(POLY_GT3);
}

static char *DrawGouraudTexturedQuad(DrawKind kind, char *out, u_long *ordering, const GT4 *prim)
{
    ProjectedVertex *v0 = &projected[prim->coords[0]], *v1 = &projected[prim->coords[1]];
    ProjectedVertex *v2 = &projected[prim->coords[2]], *v3 = &projected[prim->coords[3]];
    POLY_GT4        *poly = (POLY_GT4 *)out;

    if (AnyRejected(v0, v1, v2, v3) || Culled(kind, prim->flag, v0, v1, v2)) {
        return out;
    }
    PutScreenXY(&poly->x0, &poly->y0, v0);
    PutScreenXY(&poly->x1, &poly->y1, v1);
    PutScreenXY(&poly->x2, &poly->y2, v2);
    PutScreenXY(&poly->x3, &poly->y3, v3);
    ((u_char *)poly)[3] = 12;
    poly->code = PolyCode(kind, 0x3C, prim->flag);
    poly->clut = prim->cba;
    poly->tpage = prim->tsb;
    poly->u0 = prim->u0; poly->v0 = prim->v0;
    poly->u1 = prim->u1; poly->v1 = prim->v1;
    poly->u2 = prim->u2; poly->v2 = prim->v2;
    poly->u3 = prim->u3; poly->v3 = prim->v3;
    SetRgb(&poly->r0, &poly->g0, &poly->b0, &prim->colour[0]);
    poly->r1 = prim->colour[1].r; poly->g1 = prim->colour[1].g; poly->b1 = prim->colour[1].b; poly->p1 = prim->colour[1].cd;
    poly->r2 = prim->colour[2].r; poly->g2 = prim->colour[2].g; poly->b2 = prim->colour[2].b; poly->p2 = prim->colour[2].cd;
    poly->r3 = prim->colour[3].r; poly->g3 = prim->colour[3].g; poly->b3 = prim->colour[3].b; poly->p3 = prim->colour[3].cd;
    AddPrim(DepthSlot(kind, ordering, 0, v0, v1, v2, v3), poly);
    return out + sizeof(POLY_GT4);
}

/* Gouraud polygons without texture. */
static char *DrawGouraudTriangle(DrawKind kind, char *out, u_long *ordering, int fixedSlot, const G3 *prim)
{
    ProjectedVertex *v0 = &projected[prim->coords[0]], *v1 = &projected[prim->coords[1]], *v2 = &projected[prim->coords[2]];
    POLY_G3         *poly = (POLY_G3 *)out;

    if (AnyRejected(v0, v1, v2, 0) || Culled(kind, prim->flag, v0, v1, v2)) {
        return out;
    }
    PutScreenXY(&poly->x0, &poly->y0, v0);
    PutScreenXY(&poly->x1, &poly->y1, v1);
    PutScreenXY(&poly->x2, &poly->y2, v2);
    ((u_char *)poly)[3] = 6;
    poly->code = PolyCode(kind, 0x30, prim->flag);
    SetRgb(&poly->r0, &poly->g0, &poly->b0, &prim->colour[0]);
    poly->r1 = prim->colour[1].r; poly->g1 = prim->colour[1].g; poly->b1 = prim->colour[1].b; poly->pad1 = prim->colour[1].cd;
    poly->r2 = prim->colour[2].r; poly->g2 = prim->colour[2].g; poly->b2 = prim->colour[2].b; poly->pad2 = prim->colour[2].cd;
    AddPrim(DepthSlot(kind, ordering, fixedSlot, v0, v1, v2, 0), poly);
    return out + sizeof(POLY_G3);
}

static char *DrawGouraudQuad(DrawKind kind, char *out, u_long *ordering, int fixedSlot, const G4 *prim)
{
    ProjectedVertex *v0 = &projected[prim->coords[0]], *v1 = &projected[prim->coords[1]];
    ProjectedVertex *v2 = &projected[prim->coords[2]], *v3 = &projected[prim->coords[3]];
    POLY_G4         *poly = (POLY_G4 *)out;

    if (AnyRejected(v0, v1, v2, v3) || Culled(kind, prim->flag, v0, v1, v2)) {
        return out;
    }
    PutScreenXY(&poly->x0, &poly->y0, v0);
    PutScreenXY(&poly->x1, &poly->y1, v1);
    PutScreenXY(&poly->x2, &poly->y2, v2);
    PutScreenXY(&poly->x3, &poly->y3, v3);
    ((u_char *)poly)[3] = 8;
    poly->code = PolyCode(kind, 0x38, prim->flag);
    SetRgb(&poly->r0, &poly->g0, &poly->b0, &prim->colour[0]);
    poly->r1 = prim->colour[1].r; poly->g1 = prim->colour[1].g; poly->b1 = prim->colour[1].b; poly->pad1 = prim->colour[1].cd;
    poly->r2 = prim->colour[2].r; poly->g2 = prim->colour[2].g; poly->b2 = prim->colour[2].b; poly->pad2 = prim->colour[2].cd;
    poly->r3 = prim->colour[3].r; poly->g3 = prim->colour[3].g; poly->b3 = prim->colour[3].b; poly->pad3 = prim->colour[3].cd;
    AddPrim(DepthSlot(kind, ordering, fixedSlot, v0, v1, v2, v3), poly);
    return out + sizeof(POLY_G4);
}

/* Flat untextured polygons. */
static char *DrawFlatTriangle(DrawKind kind, char *out, u_long *ordering, int fixedSlot, const F3 *prim)
{
    ProjectedVertex *v0 = &projected[prim->coords[0]], *v1 = &projected[prim->coords[1]], *v2 = &projected[prim->coords[2]];
    POLY_F3         *poly = (POLY_F3 *)out;

    if (AnyRejected(v0, v1, v2, 0) || Culled(kind, prim->flag, v0, v1, v2)) {
        return out;
    }
    PutScreenXY(&poly->x0, &poly->y0, v0);
    PutScreenXY(&poly->x1, &poly->y1, v1);
    PutScreenXY(&poly->x2, &poly->y2, v2);
    ((u_char *)poly)[3] = 4;
    poly->code = PolyCode(kind, 0x20, prim->flag);
    SetRgb(&poly->r0, &poly->g0, &poly->b0, &prim->colour);
    AddPrim(DepthSlot(kind, ordering, fixedSlot, v0, v1, v2, 0), poly);
    return out + sizeof(POLY_F3);
}

static char *DrawFlatQuad(DrawKind kind, char *out, u_long *ordering, int fixedSlot, const F4 *prim)
{
    ProjectedVertex *v0 = &projected[prim->coords[0]], *v1 = &projected[prim->coords[1]];
    ProjectedVertex *v2 = &projected[prim->coords[2]], *v3 = &projected[prim->coords[3]];
    POLY_F4         *poly = (POLY_F4 *)out;

    if (AnyRejected(v0, v1, v2, v3) || Culled(kind, prim->flag, v0, v1, v2)) {
        return out;
    }
    PutScreenXY(&poly->x0, &poly->y0, v0);
    PutScreenXY(&poly->x1, &poly->y1, v1);
    PutScreenXY(&poly->x2, &poly->y2, v2);
    PutScreenXY(&poly->x3, &poly->y3, v3);
    ((u_char *)poly)[3] = 5;
    poly->code = PolyCode(kind, 0x28, prim->flag);
    SetRgb(&poly->r0, &poly->g0, &poly->b0, &prim->colour);
    AddPrim(DepthSlot(kind, ordering, fixedSlot, v0, v1, v2, v3), poly);
    return out + sizeof(POLY_F4);
}

/* Packs a screen position from separate coordinates (x as the low 16 bits). */
static int PackScreen(int x, int y)
{
    return (x & 0xFFFF) | (int)((u_int)y << 16);
}

/* Port: a billboard's corner, placed in 2D about its vertex's projection, recorded where it stands in view space
   (beside the vertex, at its depth), so that the pictures between two frames move it with the vertex and the view
   (it stayed put, then jumped with each frame, while the scenery around it moved). Recorded without a depth: the
   billboard is drawn as before, affine and outside the depth buffer. */
static void BindSpriteCorner(short *corner, const GtePrecise *centre, int x, int y, int k)
{
    double scale = centre->view[2] / (double)(u_short)GteReadControl(26);
    /* (x and y are the packet's 16 bits, unsigned: a centre left of the screen or above it is 65536 off) */
    int    dx = (short)(corner[0] - x), dy = (short)(corner[1] - y);
    uint32_t handle;

    GteKeyNext(0, GteKeyMix(centre->key, (uint64_t)k + 1));
    handle = GteRecordProjection(*(u_int *)corner, centre->x + (float)dx, centre->y + (float)dy, 0.0f, 1.0f);
    GteRecordView(handle, centre->view[0] + dx * scale, centre->view[1] + dy * scale, centre->view[2]);
    GteBindPrecise(corner, handle);
}

/* A billboard: a textured quad about the vertex's screen position, its size scaled by the distance (sprites
   nearer than 0x800 are not drawn) and turned by `roll`.  `top` selects the two ways the corners are laid
   out (types TSPR and BSPR).  Linked by the vertex's depth. */
static char *DrawSprite(DrawKind kind, char *out, u_long *ordering, int roll, const SPR *prim, int top)
{
    ProjectedVertex *v = &projected[prim->coord];
    POLY_FT4        *poly = (POLY_FT4 *)out;
    u_int            depth = farView ? (u_int)(farZ[v - projected] > 0 ? farZ[v - projected] : 0) : v->depth;
    u_int            width, height;
    int              halfWidth, cosA, sinA, cosB, sinB;
    int              dx1, dy1, dx2, dy2;
    int              x, y;
    Texture         *texture;

    if (v->flag || forceReject) {
        return out;
    }
    width = (u_int)((int)prim->width * 0xA0);
    height = (u_int)((int)prim->height * 0xA0);
    if (depth < 0x800) {
        return out;
    }
    width /= depth;
    height /= depth;
    halfWidth = (int)width >> 1;
    cosA = rcos(roll);
    dx1 = (int)((u_int)cosA * (u_int)halfWidth) >> 12;
    sinA = rsin(roll);
    dy1 = (int)((u_int)sinA * (u_int)halfWidth) >> 12;
    cosB = rcos(0x400 - roll);
    dx2 = (int)((u_int)cosB * height) >> 12;
    sinB = rsin(0x400 - roll);
    dy2 = (int)((u_int)sinB * height) >> 12;

    x = v->screen & 0xFFFF;
    y = v->screen >> 16;
    if (top) {
        *(int *)&poly->x0 = PackScreen(x - dx1, y - dy1);
        *(int *)&poly->x2 = PackScreen(x - dx1 - dx2, y - dy1 + dy2);
        *(int *)&poly->x1 = PackScreen(x + dx1, y + dy1);
        *(int *)&poly->x3 = PackScreen(x + dx1 - dx2, y + dy1 + dy2);
    } else {
        *(int *)&poly->x2 = PackScreen(x - dx1, y - dy1);
        *(int *)&poly->x0 = PackScreen(x - dx1 + dx2, y - dy1 - dy2);
        *(int *)&poly->x3 = PackScreen(x + dx1, y + dy1);
        *(int *)&poly->x1 = PackScreen(x + dx1 + dx2, y + dy1 - dy2);
    }
    {
        GtePrecise centre;

        if (GtePreciseOf(projectedHandle[v - projected], &centre) && centre.key && centre.view[2] > 0.0f) {
            BindSpriteCorner(&poly->x0, &centre, x, y, 0); /* Port: frame interpolation */
            BindSpriteCorner(&poly->x1, &centre, x, y, 1);
            BindSpriteCorner(&poly->x2, &centre, x, y, 2);
            BindSpriteCorner(&poly->x3, &centre, x, y, 3);
        }
    }
    SetRgb(&poly->r0, &poly->g0, &poly->b0, &prim->colour);
    texture = textureTable[prim->texture];
    ((u_char *)poly)[3] = 9;
    poly->code = 0x2C;
    poly->tpage = texture->tsb;
    poly->clut = texture->cba;
    poly->u0 = (u_char)texture->u0; poly->v0 = (u_char)texture->v0;
    poly->u1 = (u_char)texture->u1; poly->v1 = (u_char)texture->v1;
    poly->u2 = (u_char)texture->u2; poly->v2 = (u_char)texture->v2;
    poly->u3 = (u_char)texture->u3; poly->v3 = (u_char)texture->v3;
    if (kind == DRAW_LIT) {
        /* Port: the lit renderer linked the sprite through a register that was zero here, so the packet's
           length byte was cleared and the GPU skipped it: sprites are never drawn in lit objects. */
        ((u_char *)poly)[3] = 0;
    }
    if (farView) {
        AddPrim(PortFarSlot((int)(depth >> 2), (double)depth), poly);
        return out + sizeof(POLY_FT4);
    }
    AddPrim((u_long *)((char *)ordering + (v->depth & 0xFFFC)), poly);
    return out + sizeof(POLY_FT4);
}

/* Port: a polygon of the scenery that the game leaves out for a vertex too near the camera, behind it, or
   beyond the GPU's coordinates: a wall the camera passes close by vanished, the black background showing
   through. Its part in front of the near plane is drawn as gouraud triangles (the corners' colours, all one
   in a flat polygon), as trackdraw.c's DrawNearFace does for the track: the corners the game projected at
   their precise positions, those it rejected and the cut ones at their true ones. Returns whether it drew. */
static void CameraSpace(const SVECTOR *v, double out[3])
{
    /* RTPS's sums (gte.c's Rtp) */
    int32_t r[9] = {
        (short)GteReadControl(0), (short)(GteReadControl(0) >> 16), (short)GteReadControl(1),
        (short)(GteReadControl(1) >> 16), (short)GteReadControl(2), (short)(GteReadControl(2) >> 16),
        (short)GteReadControl(3), (short)(GteReadControl(3) >> 16), (short)GteReadControl(4)
    };
    int i;

    for (i = 0; i < 3; i++) {
        int64_t mac = (int64_t)GteReadControl(5 + i) * 4096 + (int64_t)r[3 * i] * v->vx + (int64_t)r[3 * i + 1] * v->vy +
                      (int64_t)r[3 * i + 2] * v->vz;

        out[i] = (double)mac / 4096.0;
    }
}

typedef struct NearCorner {
    double   c[8];   /* camera x, y, z, u, v, r, g, b */
    int      vertex; /* the object's vertex, or -1 - n for the cut in edge n */
} NearCorner;

static void SetNearCorner(short *x, short *y, u_char *rgb, const NearCorner *corner, const ProjectedVertex *vertex)
{
    if (corner->vertex >= 0 && !vertex->flag) {
        PutScreenXY(x, y, vertex); /* as the game's polygons beside it have it */
    } else {
        PortFarVertex point;

        /* frame interpolation: the object's vertex as the game's projection of it, a cut by its edge */
        GteKeyNext(0, corner->vertex >= 0 ? GteKeyMix(currentDrawing, (uint64_t)corner->vertex)
                                          : GteKeyMix(nearPrimitive, (uint64_t)-corner->vertex));
        PortFarProjectCamera(corner->c[0], corner->c[1], corner->c[2], &point);
        StoreScreenXY(x, y, point.screen);
        GteBindPrecise(x, point.handle);
    }
    rgb[0] = (u_char)(corner->c[5] + 0.5);
    rgb[1] = (u_char)(corner->c[6] + 0.5);
    rgb[2] = (u_char)(corner->c[7] + 0.5);
}

static int DrawNearPrimitive(Object *object, int index, u_long *ordering, Poly prim)
{
    const short   *coords;
    const char    *uv = NULL; /* u0, v0, u1, v1... */
    const CVECTOR *colours;
    short          flag = prim.f3->flag; /* each primitive begins with its type and flags */
    short          tsb = 0, cba = 0;
    int            count, gouraud = 0, rejected = 0, i, k, n = 0, slot;
    NearCorner     corner[4], clipped[8];
    double         sum = 0.0, h = (double)(u_short)GteReadControl(26);
    int            margin = PortScreenMargin(); /* widescreen's sides */
    char          *start, *out;
    const double  *a, *b, *c;

    nearPrimitive = GteKeyMix(currentDrawing, 0x10000 + (uint64_t)index); /* frame interpolation */

    switch ((u_short)prim.prim->type) {
    case TypeFT3:
        coords = prim.ft3->coords, count = 3, uv = &prim.ft3->u0, colours = &prim.ft3->colour;
        tsb = prim.ft3->tsb, cba = prim.ft3->cba;
        break;
    case TypeGT3: /* the plain renderer draws it flat, in its first colour */
        coords = prim.gt3->coords, count = 3, uv = &prim.gt3->u0, colours = prim.gt3->colour;
        tsb = prim.gt3->tsb, cba = prim.gt3->cba;
        break;
    case TypeFT4:
        coords = prim.ft4->coords, count = 4, uv = &prim.ft4->u0, colours = &prim.ft4->colour;
        tsb = prim.ft4->tsb, cba = prim.ft4->cba;
        break;
    case TypeGT4:
        coords = prim.gt4->coords, count = 4, uv = &prim.gt4->u0, colours = prim.gt4->colour;
        tsb = prim.gt4->tsb, cba = prim.gt4->cba;
        break;
    case TypeG3: coords = prim.g3->coords, count = 3, colours = prim.g3->colour, gouraud = 1; break;
    case TypeG4: coords = prim.g4->coords, count = 4, colours = prim.g4->colour, gouraud = 1; break;
    case TypeF3: coords = prim.f3->coords, count = 3, colours = &prim.f3->colour; break;
    case TypeF4: coords = prim.f4->coords, count = 4, colours = &prim.f4->colour; break;
    default: return 0;
    }
    for (i = 0; i < count; i++) {
        if ((u_short)coords[i] >= MAX_OBJECT_VERTICES || (u_short)coords[i] >= (u_short)object->vertexCount) {
            return 0;
        }
        rejected |= projected[coords[i]].flag;
    }
    if (!rejected) {
        return 0; /* facing away */
    }
    /* the outline (a quad's vertices go 0, 1, 3, 2) */
    for (i = 0; i < count; i++) {
        int j = count == 4 && i >= 2 ? 5 - i : i;
        const CVECTOR *colour = &colours[gouraud ? j : 0];

        CameraSpace(&object->vertices[coords[j]], corner[i].c);
        if (corner[i].c[2] > 32767.0) {
            return 0; /* too far: the far view's (portfar.h) */
        }
        corner[i].vertex = coords[j];
        corner[i].c[3] = uv ? (u_char)uv[2 * j] : 0;
        corner[i].c[4] = uv ? (u_char)uv[2 * j + 1] : 0;
        corner[i].c[5] = colour->r;
        corner[i].c[6] = colour->g;
        corner[i].c[7] = colour->b;
    }
    /* the facing test of vertices 0, 1 and 2 in camera space: the sign of their screen winding, at any depth */
    a = corner[0].c, b = corner[1].c, c = corner[count == 4 ? 3 : 2].c;
    if ((flag & SingleSided) &&
        a[0] * (b[1] * c[2] - b[2] * c[1]) - a[1] * (b[0] * c[2] - b[2] * c[0]) + a[2] * (b[0] * c[1] - b[1] * c[0]) <= 0) {
        return 0;
    }
    for (i = 0; i < count; i++) {
        const NearCorner *p = &corner[i], *q = &corner[(i + 1) % count];
        int pIn = p->c[2] >= PORT_FAR_NEAR, qIn = q->c[2] >= PORT_FAR_NEAR;

        if (pIn) {
            clipped[n++] = *p;
        }
        if (pIn != qIn) {
            double t = (PORT_FAR_NEAR - p->c[2]) / (q->c[2] - p->c[2]);

            for (k = 0; k < 8; k++) {
                clipped[n].c[k] = p->c[k] + (q->c[k] - p->c[k]) * t;
            }
            clipped[n].c[2] = PORT_FAR_NEAR;
            clipped[n++].vertex = -1 - i;
        }
    }
    if (n < 3) {
        return 0; /* behind the camera */
    }
    for (i = 0, k = 0; i < n; i++) {
        double x = clipped[i].c[0] * h / clipped[i].c[2], y = clipped[i].c[1] * h / clipped[i].c[2];

        /* a generous screen (widescreen's sides included) */
        k |= (x < 200.0 + margin) | (x > -200.0 - margin) << 1 | (y < 160.0) << 2 | (y > -160.0) << 3;
        sum += clipped[i].c[2];
    }
    if (k != 15) {
        return 0; /* wholly off one side */
    }
    /* DepthSlot's: AVSZ3/AVSZ4 of the average depth, in the game's table */
    sum *= (double)count / n;
    slot = (int)(((long long)GteReadControl(count == 4 ? 30 : 29) * (long long)sum) >> 12);
    slot = slot < 0 ? 0 : slot > 0x1FFD ? 0x1FFD : slot;
    start = out = PortNearAlloc((n - 2) * (int)sizeof(POLY_GT3));
    if (!out) {
        return 0;
    }
    for (i = 1; i + 1 < n; i++) {
        const NearCorner *p0 = &clipped[0], *p1 = &clipped[i], *p2 = &clipped[i + 1];
        const ProjectedVertex *v0 = &projected[p0->vertex < 0 ? 0 : p0->vertex];
        const ProjectedVertex *v1 = &projected[p1->vertex < 0 ? 0 : p1->vertex];
        const ProjectedVertex *v2 = &projected[p2->vertex < 0 ? 0 : p2->vertex];

        if (uv) {
            POLY_GT3 *poly = (POLY_GT3 *)out;

            SetPolyGT3(poly);
            SetNearCorner(&poly->x0, &poly->y0, &poly->r0, p0, v0);
            SetNearCorner(&poly->x1, &poly->y1, &poly->r1, p1, v1);
            SetNearCorner(&poly->x2, &poly->y2, &poly->r2, p2, v2);
            poly->u0 = (u_char)(p0->c[3] + 0.5); poly->v0 = (u_char)(p0->c[4] + 0.5);
            poly->u1 = (u_char)(p1->c[3] + 0.5); poly->v1 = (u_char)(p1->c[4] + 0.5);
            poly->u2 = (u_char)(p2->c[3] + 0.5); poly->v2 = (u_char)(p2->c[4] + 0.5);
            poly->tpage = (u_short)tsb;
            poly->clut = (u_short)cba;
            poly->code = PolyCode(DRAW_PLAIN, 0x34, flag);
            AddPrim(ordering + slot, poly);
            out += sizeof(POLY_GT3);
        } else {
            POLY_G3 *poly = (POLY_G3 *)out;

            SetPolyG3(poly);
            SetNearCorner(&poly->x0, &poly->y0, &poly->r0, p0, v0);
            SetNearCorner(&poly->x1, &poly->y1, &poly->r1, p1, v1);
            SetNearCorner(&poly->x2, &poly->y2, &poly->r2, p2, v2);
            poly->code = PolyCode(DRAW_PLAIN, 0x30, flag);
            AddPrim(ordering + slot, poly);
            out += sizeof(POLY_G3);
        }
    }
    PortTagPrims(start, out, "near object", object->name, index); /* the frame recorder */
    return 1;
}

static void DrawObject(Object *object, DrawKind kind, int roll, const CVECTOR *colour, int fixedSlot)
{
    u_long *ordering;
    char   *out;
    char   *primitive;
    int     count, index = 0;
    unsigned char *drawnFlags = NULL;
    const unsigned char *skip = NULL;

    if (farView) {
        int complete;

        /* the primitives the game handled (scenery; a ship model can serve several ships) */
        skip = kind == DRAW_LIT ? NULL : PortFarObjectDrawn(object, &complete);
        /* the largest packet per primitive, in the far view's own memory */
        out = PortFarAlloc((u_short)object->primitiveCount * (int)sizeof(POLY_GT4));
        if (!out) {
            return;
        }
    } else {
        drawnFlags = PortFarObjectPrimitives(object); /* Port: the far view draws what the game did not */
        SetSkeletonTransform(object->skeleton);
        out = nextPrim.ptr;
    }
    primitive = (char *)object->primitives;
    count = (u_short)object->primitiveCount;
    ordering = OT[CurrentScreen];
    GteSetBackground(kind == DRAW_FIXED); /* Port: the sky is behind everything (the depth buffer leaves it out) */
    GteSetExactDepth(exactDepth);
    ProjectVertices(object);
    GteSetBackground(0);
    GteSetExactDepth(0);

    do {
        Poly  prim;
        char *before = out;

        prim.ptr = primitive;
        forceReject = skip && index < (u_short)object->primitiveCount && skip[index];
        culled = 0;
        switch ((u_short)prim.prim->type) {
        case TypeGT3:
            if (kind == DRAW_LIT) {
                out = DrawGouraudTexturedTriangle(kind, out, ordering, prim.gt3);
            } else {
                out = DrawTexturedTriangle(kind, out, ordering, fixedSlot, (const FT3 *)prim.gt3, &prim.gt3->colour[0]);
            }
            primitive += sizeof(GT3);
            break;
        case TypeGT4:
            if (kind == DRAW_LIT) {
                out = DrawGouraudTexturedQuad(kind, out, ordering, prim.gt4);
            } else {
                out = DrawTexturedQuad(kind, out, ordering, fixedSlot, (const FT4 *)prim.gt4, &prim.gt4->colour[0]);
            }
            primitive += sizeof(GT4);
            break;
        case TypeFT3:
            out = DrawTexturedTriangle(kind, out, ordering, fixedSlot, prim.ft3, kind == DRAW_LIT ? colour : &prim.ft3->colour);
            primitive += sizeof(FT3);
            break;
        case TypeFT4:
            out = DrawTexturedQuad(kind, out, ordering, fixedSlot, prim.ft4, kind == DRAW_LIT ? colour : &prim.ft4->colour);
            primitive += sizeof(FT4);
            break;
        case TypeG3:
            out = DrawGouraudTriangle(kind, out, ordering, fixedSlot, prim.g3);
            primitive += sizeof(G3);
            break;
        case TypeG4:
            out = DrawGouraudQuad(kind, out, ordering, fixedSlot, prim.g4);
            primitive += sizeof(G4);
            break;
        case TypeF3:
            out = DrawFlatTriangle(kind, out, ordering, fixedSlot, prim.f3);
            primitive += sizeof(F3);
            break;
        case TypeF4:
            out = DrawFlatQuad(kind, out, ordering, fixedSlot, prim.f4);
            primitive += sizeof(F4);
            break;
        case TypeTSPR:
        case TypeBSPR:
            if (kind == DRAW_FIXED) {
                goto finished;
            }
            out = DrawSprite(kind, out, ordering, roll, prim.spr, prim.prim->type == TypeTSPR);
            primitive += sizeof(SPR);
            break;
        default:
            goto finished;
        }
        if (drawnFlags && index < (u_short)object->primitiveCount) {
            drawnFlags[index] = out != before || (culled && CulledForCertain(culledVertex[0], culledVertex[1], culledVertex[2])) ||
                                (!culled && kind == DRAW_PLAIN && DrawNearPrimitive(object, index, ordering, prim));
        }
        PortTagPrims(before, out, farView ? "far object" : "object", object->name, index); /* Port: the frame recorder */
        index++;
    } while (--count > 0);
finished:
    forceReject = 0;
    if (farView) {
        PortFarRelease(out);
        return;
    }
    nextPrim.ptr = out;
}

void AsmDrawObject(Object *object, Skeleton *camera, int roll)
{
    DrawObject(object, DRAW_PLAIN, roll, 0, 0);
}

void AsmDrawObjectLit(Object *object, Skeleton *camera, int roll, CVECTOR *colour)
{
    DrawObject(object, DRAW_LIT, roll, colour, 0);
}

void AsmDrawObjectFixed(Object *object, Skeleton *camera, int slot)
{
    DrawObject(object, DRAW_FIXED, 0, 0, slot);
}

/* Port: an object in the far view (portfar.h): the plain renderer, or the lit one with `colour`. */
void PortDrawObjectFar(Object *object, const PortView *view, const CVECTOR *colour)
{
    farView = view;
    DrawObject(object, colour ? DRAW_LIT : DRAW_PLAIN, 0, colour, 0);
    farView = NULL;
}
