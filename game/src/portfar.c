/* Port: the whole track in view (see portfar.h). */
#include <math.h>
#include <stdlib.h>

#include "functions.h"
#include "portfar.h"
#include "trails.h"
#include "effects.h"
#include "port_platform.h"
#include "../../platform/psx/gte.h"

extern TrackData *track;
extern Object   **trackObjects;
extern short      CurrentScreen;
extern int        ownShip;
extern int        remoteShip;
extern int        ghostShip;
extern int        raceType;
extern ShipData  *shipIndex;
extern Shadow    *shadowData;

/* The game's ordering table has 0x2000 entries; the sky is linked into the last. Primitives the game
   would put at or beyond FAR_ENTRY go into the far table, linked in at FAR_ENTRY: drawn after the sky,
   before everything the game linked nearer. */
#define OT_ENTRIES   0x2000
#define FAR_ENTRY    (OT_ENTRIES - 2)
#define FAR_BUCKETS  16384
#define FAR_START    ((double)FAR_ENTRY * 4) /* the depth of FAR_ENTRY for the track's slots */
#define FAR_STEP     32.0                    /* depth units per far bucket */
#define ARENA_BYTES  (6 << 20)               /* primitives per screen */
#define MARK_SLOTS   1024                    /* objects marked per frame */
#define NEAR_BYTES   (256 << 10)             /* PortNearAlloc's, per screen */

static u_long   farTable[2][FAR_BUCKETS];
static char     arena[2][ARENA_BYTES];
static int      arenaUsed;
static char     nearArena[2][NEAR_BYTES];
static int      nearUsed[2];
static uint32_t nearFrame[2];
static uint32_t frame = 1;
static uint32_t *faceMarks;      /* the frame each track face was last drawn by the game */
static int      faceMarkCount;
static uint32_t shipMarks[15];
static int      farShips;        /* the ships the far view drew this frame, until their engine flares are drawn */
static int      shadowShips;     /* the ships DrawShadow gave a shadow this frame */
static uint32_t shadowFrame;     /* ... the frame that was */
static u_long  *flareSlot;       /* where DrawFlare links the flare it draws for the far view (PortFlareSlot) */
/* The objects the game drew this frame, and which of their primitives it handled (drew, or culled as
   facing away): the GTE rejects every polygon with a vertex deeper than 32767 (IR3 saturates) while the
   game draws objects to 38200, so objects in between are "drawn" with nothing on screen. */
static struct {
    Object        *object;
    uint32_t       frame;
    unsigned char *drawn;
    int            count, capacity;
} objectMarks[MARK_SLOTS];
static int enabled = -1;

/* Live: the debug console turns it on and off. */
static int Enabled(void)
{
    int on = PortFarViewEnabled();

    if (on && enabled < 0) {
        enabled = 1;
        PsxLinkRegion(arena, sizeof(arena));
        PsxLinkRegion(farTable, sizeof(farTable));
        PsxLinkRegion(nearArena, sizeof(nearArena));
    }
    return on;
}

void PortFarMarkFace(Face *face)
{
    int index;

    if (!track || !Enabled()) {
        return;
    }
    if (faceMarkCount < track->faceCount) {
        faceMarks = realloc(faceMarks, (size_t)track->faceCount * sizeof(*faceMarks));
        memset(faceMarks + faceMarkCount, 0, (size_t)(track->faceCount - faceMarkCount) * sizeof(*faceMarks));
        faceMarkCount = track->faceCount;
    }
    index = (int)(face - track->faces);
    if (index >= 0 && index < faceMarkCount) {
        faceMarks[index] = frame;
    }
}

int PortFarFaceDrawn(Face *face)
{
    int index = track ? (int)(face - track->faces) : -1;

    return index >= 0 && index < faceMarkCount && faceMarks[index] == frame;
}

static int ObjectSlot(Object *object)
{
    uintptr_t key = (uintptr_t)object;

    return (int)((key >> 4) ^ (key >> 13)) & (MARK_SLOTS - 1);
}

/* The mark slot of an object this frame (created when `create`), or -1. */
static int MarkSlot(Object *object, int create)
{
    int slot = ObjectSlot(object), probe;

    for (probe = 0; probe < MARK_SLOTS; probe++, slot = (slot + 1) & (MARK_SLOTS - 1)) {
        if (objectMarks[slot].frame != frame) {
            if (!create) {
                return -1;
            }
            objectMarks[slot].object = object;
            objectMarks[slot].frame = frame;
            objectMarks[slot].count = 0;
            return slot;
        }
        if (objectMarks[slot].object == object) {
            return slot;
        }
    }
    return -1;
}

unsigned char *PortFarObjectPrimitives(Object *object)
{
    int slot, count = (u_short)object->primitiveCount;

    if (!Enabled() || (slot = MarkSlot(object, 1)) < 0) {
        return NULL;
    }
    if (objectMarks[slot].capacity < count) {
        objectMarks[slot].drawn = realloc(objectMarks[slot].drawn, (size_t)count);
        objectMarks[slot].capacity = count;
    }
    memset(objectMarks[slot].drawn, 0, (size_t)count);
    objectMarks[slot].count = count;
    return objectMarks[slot].drawn;
}

const unsigned char *PortFarObjectDrawn(Object *object, int *complete)
{
    int slot = MarkSlot(object, 0), i;

    *complete = 0;
    if (slot < 0) {
        return NULL;
    }
    *complete = 1;
    for (i = 0; i < objectMarks[slot].count; i++) {
        if (!objectMarks[slot].drawn[i]) {
            *complete = 0;
            break;
        }
    }
    return objectMarks[slot].drawn;
}

void PortFarMarkShip(int ship)
{
    if (ship >= 0 && ship < 15) {
        shipMarks[ship] = frame;
    }
}

/* ---- transforms ---- */

/* out = view composed with the 4096-scaled matrix and translation of `relative`. */
static void Compose(const PortView *view, const MATRIX *relative, PortView *out)
{
    PortView r;
    int i, j;

    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) {
            r.m[i][j] = (view->m[i][0] * relative->m[0][j] + view->m[i][1] * relative->m[1][j] +
                         view->m[i][2] * relative->m[2][j]) / 4096.0;
        }
        r.t[i] = view->t[i] + view->m[i][0] * relative->t[0] + view->m[i][1] * relative->t[1] +
                 view->m[i][2] * relative->t[2];
    }
    *out = r;
}

/* The transform of a skeleton (its own and its supers', as GetSkeletonMatrix composes them), read only. */
static void SkeletonView(const Skeleton *skeleton, PortView *out)
{
    if (!skeleton->super) {
        PortView identity;
        int i, j;

        for (i = 0; i < 3; i++) {
            for (j = 0; j < 3; j++) {
                identity.m[i][j] = i == j;
            }
            identity.t[i] = 0;
        }
        Compose(&identity, &skeleton->relative, out);
        return;
    }
    SkeletonView(skeleton->super, out);
    Compose(out, &skeleton->relative, out);
}

static double depthBias = 1.0;

void PortFarDepthBias(double scale)
{
    depthBias = scale;
}

void PortFarProject(const PortView *view, double x, double y, double z, PortFarVertex *out)
{
    double h = (double)(u_short)GteReadControl(26);
    double cx = view->m[0][0] * x + view->m[0][1] * y + view->m[0][2] * z + view->t[0];
    double cy = view->m[1][0] * x + view->m[1][1] * y + view->m[1][2] * z + view->t[1];
    double cz = view->m[2][0] * x + view->m[2][1] * y + view->m[2][2] * z + view->t[2];
    int sx, sy;

    out->z = cz;
    out->rejected = 1;
    out->screen = 0;
    out->handle = 0;
    if (cz < h * 0.5 || cz < 1.0) {
        return; /* the GTE's divide overflow: too near */
    }
    out->x = GteReadControl(24) / 65536.0 + cx * h / cz;
    out->y = GteReadControl(25) / 65536.0 + cy * h / cz;
    if (out->x < -1024.0 || out->x >= 1024.0 || out->y < -1024.0 || out->y >= 1024.0) {
        return; /* beyond the GPU's coordinates */
    }
    sx = (int)floor(out->x);
    sy = (int)floor(out->y);
    out->screen = (sx & 0xFFFF) | (int)((u_int)sy << 16);
    out->rejected = 0;
    out->handle = GteRecordProjection((uint32_t)out->screen, (float)out->x, (float)out->y, (float)(cz * depthBias),
                                      GteFade(sqrt(cx * cx + cy * cy + cz * cz)));
    GteRecordView(out->handle, cx, cy, cz); /* frame interpolation (gte.h) */
}

void PortFarProjectCamera(double cx, double cy, double cz, PortFarVertex *out)
{
    double h = (double)(u_short)GteReadControl(26);
    int sx, sy;

    out->z = cz;
    out->x = GteReadControl(24) / 65536.0 + cx * h / cz;
    out->y = GteReadControl(25) / 65536.0 + cy * h / cz;
    sx = out->x < -1024.0 ? -1024 : out->x > 1023.0 ? 1023 : (int)floor(out->x);
    sy = out->y < -1024.0 ? -1024 : out->y > 1023.0 ? 1023 : (int)floor(out->y);
    out->screen = (sx & 0xFFFF) | (int)((u_int)sy << 16);
    out->rejected = 0;
    out->handle = GteRecordProjection((uint32_t)out->screen, (float)out->x, (float)out->y, (float)(cz * depthBias),
                                      GteFade(sqrt(cx * cx + cy * cy + cz * cz)));
    GteRecordView(out->handle, cx, cy, cz); /* frame interpolation (gte.h) */
}

void PortFarProjectAny(double cx, double cy, double cz, PortFarVertex *out)
{
    double nearZ = cz > PORT_FAR_NEAR ? cz : PORT_FAR_NEAR;
    double h = (double)(u_short)GteReadControl(26);
    int sx, sy;

    if (cz >= PORT_FAR_NEAR) {
        PortFarProjectCamera(cx, cy, cz, out);
        return;
    }
    /* (its place is libgpu's to work out, from the view-space position: this one stands in) */
    out->z = cz;
    out->x = GteReadControl(24) / 65536.0 + cx * h / nearZ;
    out->y = GteReadControl(25) / 65536.0 + cy * h / nearZ;
    sx = out->x < -1024.0 ? -1024 : out->x > 1023.0 ? 1023 : (int)floor(out->x);
    sy = out->y < -1024.0 ? -1024 : out->y > 1023.0 ? 1023 : (int)floor(out->y);
    out->screen = (sx & 0xFFFF) | (int)((u_int)sy << 16);
    out->rejected = 0;
    out->handle = GteRecordProjection((uint32_t)out->screen, (float)out->x, (float)out->y, (float)(cz * depthBias),
                                      GteFade(sqrt(cx * cx + cy * cy + cz * cz)));
    GteRecordView(out->handle, cx, cy, cz);
}

u_long *PortFarSlot(int entry, double depth)
{
    int bucket;

    if (entry >= 0 && entry < FAR_ENTRY) {
        return OT[CurrentScreen] + entry;
    }
    /* bucket 0 is the table's tail, which PortFarDraw links back into the game's table (AddPrims rewrites
       its link): whatever was put there would be cut off */
    bucket = 1 + (int)((depth - FAR_START) / FAR_STEP);
    bucket = bucket < 1 ? 1 : bucket >= FAR_BUCKETS ? FAR_BUCKETS - 1 : bucket;
    return farTable[CurrentScreen] + bucket;
}

char *PortFarAlloc(int bytes)
{
    char *p;

    if (arenaUsed + bytes > ARENA_BYTES) {
        return NULL;
    }
    p = arena[CurrentScreen] + arenaUsed;
    arenaUsed += bytes;
    return p;
}

/* Apart from the far view's arena: the game draws before PortFarDraw, which starts that afresh. It starts
   afresh with the game's next frame (GteFrameNumber), not with PortFarDraw: the game goes on drawing objects
   after PortFarDraw (explosions, rings, birds), and starting over there took packets already linked into the
   frame's table for new ones, which could link the table into a loop that never ends. */
char *PortNearAlloc(int bytes)
{
    int      screen = CurrentScreen & 1;
    uint32_t now = GteFrameNumber();
    char    *p;

    if (!Enabled()) {
        return NULL;
    }
    if (nearFrame[screen] != now) {
        nearFrame[screen] = now;
        nearUsed[screen] = 0;
    }
    if (nearUsed[screen] + bytes > NEAR_BYTES) {
        return NULL;
    }
    p = nearArena[screen] + nearUsed[screen];
    nearUsed[screen] += bytes;
    return p;
}

void PortFarRelease(char *end)
{
    arenaUsed = (int)(end - arena[CurrentScreen]);
}

/* ---- the pass ---- */

/* Whether a sphere about camera-space centre c might be on screen; with `behind`, also when it is that far behind the
   camera (seen from where the camera was a frame before: the pictures between two frames). */
static int MaybeVisible(const double c[3], double radius, double behind)
{
    if (GteFade(sqrt(c[0] * c[0] + c[1] * c[1] + c[2] * c[2]) - radius) <= 0.0f) {
        return 0; /* faded out entirely */
    }
    double h = (double)(u_short)GteReadControl(26);
    double z = c[2] + radius;

    if (behind > 0.0 && c[2] + radius + behind >= h * 0.5 && c[2] < radius + behind) {
        return 1; /* about the camera: the cone below says nothing there */
    }
    if (z < h * 0.5) {
        return 0;
    }
    /* the screen is 320x240 about the centre, and as much wider as widescreen adds: a cone twice as wide and high, as
       the pictures between two frames show the view a little before (an object leaving the screen at a turn popped
       out at its edge, with a cone a quarter wider) */
    return fabs(c[0]) - radius < z * ((320.0 + 2 * PortScreenMargin()) / h) && fabs(c[1]) - radius < z * (256.0 / h);
}

/* The view of world positions, as the track renderers have it: SetSkeletonTransform(camera->super), points +
   camera->relative.t. */
static void CameraView(TrackCamera *camera, PortView *view)
{
    Skeleton *camPos = camera->camPos;
    int i;

    SkeletonView(camPos->super, view);
    for (i = 0; i < 3; i++) {
        view->t[i] += view->m[i][0] * camPos->relative.t[0] + view->m[i][1] * camPos->relative.t[1] +
                      view->m[i][2] * camPos->relative.t[2];
    }
}

static void DrawFarTrack(TrackCamera *camera)
{
    PortView view;
    char *start;
    int i, k;

    CameraView(camera, &view);
    start = arena[CurrentScreen] + arenaUsed; /* (the track's packets, for the shadows: libgpu.c) */
    for (i = 0; i < track->sectionCount; i++) {
        TrackSection *section = track->sections + i;
        double c[3];

        for (k = 0; k < 3; k++) {
            c[k] = view.m[k][0] * section->centre.vx + view.m[k][1] * section->centre.vy + view.m[k][2] * section->centre.vz +
                   view.t[k];
        }
        if (MaybeVisible(c, 4096.0 + section->globalRadius, PORT_FAR_BEHIND)) {
            PortTrackFar(section, &view);
        }
    }
    PortTrackPackets(start, arena[CurrentScreen] + arenaUsed);
}

static void DrawFarScenery(void)
{
    Object *object;

    if (!trackObjects) {
        return;
    }
    for (object = trackObjects[0]; object; object = object->next) {
        PortView view;

        {
            int complete;

            PortFarObjectDrawn(object, &complete);
            if (complete) {
                continue; /* the game drew all of it */
            }
        }
        SkeletonView(object->skeleton, &view);
        if (MaybeVisible(view.t, (double)object->extent, PORT_FAR_BEHIND)) { /* (beside the camera: as the track) */
            PortDrawObjectFar(object, &view, NULL);
        }
    }
}

/* The ships the game left out for their distance, drawn as DrawShips draws the near ones. */
static void DrawFarShips(ShipData *ships, Object **shipShapes)
{
    int i;

    for (i = 0; i < 15; i++) {
        ShipData *ship = &ships[i];
        Object   *object;
        Skeleton  skeleton;
        PortView  view;
        Face     *face;
        u_char    flags;

        /* a time trial draws its own ship and the ghost, the two-player race the two players' ships (the other may be
           far ahead or behind) */
        if ((raceType == 1 && i != ownShip && i != ghostShip) || (raceType == 3 && i != ownShip && i != remoteShip)) {
            continue;
        }
        if (shipMarks[i] == frame || (ship->combatAttr & DESTROYED) || !ship->nearTrkSect ||
            (ship->attr & (REMOTE_VIEW | VIEW_IN)) == VIEW_IN) {
            continue;
        }
        object = shipShapes[ship->highResTableIndex];
        skeleton = *object->skeleton;
        SetSkeletonPosition(&skeleton, ship->ppivot.vx, ship->ppivot.vy, ship->ppivot.vz);
        SetSkeletonDirectionHPR(&skeleton, ship->hdg, ship->pitch, ship->roll);
        SkeletonView(&skeleton, &view);
        if (!MaybeVisible(view.t, (double)object->extent + 1024.0, 0.0)) {
            continue;
        }
        /* lit by the colour of the track face beside it */
        face = track->faces + ship->nearTrkSect->faceStart;
        do {
            flags = face->flags;
            face++;
        } while (!(flags & 1));
        face--;
        if (!(ship->attr & LSIDE)) {
            face++;
        }
        farShips |= 1 << i;
        PortInterpInstance(i); /* frame interpolation: the ship's drawing, as DrawShips gives it */
        if (raceType == 1 && i == ghostShip) {
            InitTransShip(object, 1); /* the ghost is see-through, as DrawShips draws it */
            PortDrawObjectFar(object, &view, (const CVECTOR *)&face->red);
            InitTransShip(object, 0);
        } else {
            PortDrawObjectFar(object, &view, (const CVECTOR *)&face->red);
        }
        PortInterpInstance(-1);
    }
}

/* Whether (u, v) is inside the triangle a, b, c (either winding). */
static int InTriangle(const double a[2], const double b[2], const double c[2], double u, double v)
{
    double d1 = (b[0] - a[0]) * (v - a[1]) - (b[1] - a[1]) * (u - a[0]);
    double d2 = (c[0] - b[0]) * (v - b[1]) - (c[1] - b[1]) * (u - b[0]);
    double d3 = (a[0] - c[0]) * (v - c[1]) - (a[1] - c[1]) * (u - c[0]);

    return (d1 >= 0 && d2 >= 0 && d3 >= 0) || (d1 <= 0 && d2 <= 0 && d3 <= 0);
}

/* Whether the point is over a road face (flags & 1) of the section: within its corners seen along the face's normal,
   and near its plane (not a road crossing far above or below). */
static int OverRoad(const TrackSection *section, double x, double y, double z)
{
    int i, k;

    for (i = 0; i < section->faceCount; i++) {
        const Face *face = track->faces + section->faceStart + i;
        double corner[4][2], n[3] = {face->normal.vx, face->normal.vy, face->normal.vz}, u, v, height;
        int axis = fabs(n[0]) > fabs(n[1]) ? (fabs(n[0]) > fabs(n[2]) ? 0 : 2) : (fabs(n[1]) > fabs(n[2]) ? 1 : 2);
        const VECTOR *first = &track->vertices[face->vertex[0]];

        if (!(face->flags & 1)) {
            continue;
        }
        height = ((x - first->vx) * n[0] + (y - first->vy) * n[1] + (z - first->vz) * n[2]) / 4096.0;
        if (fabs(height) > 1000.0) {
            continue;
        }
        for (k = 0; k < 4; k++) {
            const VECTOR *p = &track->vertices[face->vertex[k]];
            double w[3] = {p->vx, p->vy, p->vz};

            corner[k][0] = w[axis == 0 ? 1 : 0];
            corner[k][1] = w[axis == 2 ? 1 : 2];
        }
        u = axis == 0 ? y : x;
        v = axis == 2 ? y : z;
        /* (the quad's corners in either order: its four triangles cover it) */
        if (InTriangle(corner[0], corner[1], corner[2], u, v) || InTriangle(corner[0], corner[2], corner[3], u, v) ||
            InTriangle(corner[0], corner[1], corner[3], u, v) || InTriangle(corner[1], corner[2], corner[3], u, v)) {
            return 1;
        }
    }
    return 0;
}

int PortShadowOnRoad(const ShipData *ship, double x, double y, double z)
{
    const TrackSection *section = ship->nearTrkSect;
    int i;

    if (!track || !section) {
        return 1;
    }
    if (OverRoad(section, x, y, z) || OverRoad(section->prevSection.ptr, x, y, z) ||
        OverRoad(section->nextSection.ptr, x, y, z)) {
        return 1;
    }
    if (section->junction.index != -1 && OverRoad(section->junction.ptr, x, y, z)) {
        return 1;
    }
    /* two sections on: a ship at the end of one, its shadow reaching into the next but one */
    for (i = 0; i < 2; i++) {
        section = i ? ship->nearTrkSect->prevSection.ptr->prevSection.ptr : ship->nearTrkSect->nextSection.ptr->nextSection.ptr;
        if (OverRoad(section, x, y, z)) {
            return 1;
        }
    }
    return 0;
}

void PortShadowDrawn(int ship)
{
    if (shadowFrame != frame) {
        shadowFrame = frame;
        shadowShips = 0;
    }
    shadowShips |= 1 << ship;
}

/* The shadows of the ships drawn this frame that DrawShadow gave none: it gives one only to a ship DrawShips drew
   itself the frame before (within 9600 of the camera and ahead of it), and the far view draws the rest. Laid as
   DrawShadow lays them, on the track face under the ship, in its quads (unused this frame), the grid placed in the
   world rather than halved on the screen. */
static void DrawFarShadows(TrackCamera *camera, ShipData *ships)
{
    PortView view;
    int      i;

    if (!shadowData) {
        return;
    }
    CameraView(camera, &view);
    for (i = 0; i < 15; i++) {
        ShipData      *ship = &ships[i];
        Shadow        *shadow = &shadowData[i];
        VECTOR         corner[4], facePoint;
        PortFarVertex  grid[25];
        Face          *face;
        u_char         flags;
        int            k, row, col, entry[3];

        if (!(farShips & (1 << i)) && shipMarks[i] != frame) {
            continue; /* not drawn */
        }
        if ((shadowFrame == frame && (shadowShips & (1 << i))) || (ship->attr & FLYING) ||
            (ship->combatAttr & DESTROYED) || !ship->nearTrkSect || (raceType == 1 && i != ownShip) ||
            (raceType == 3 && i != ownShip && i != remoteShip)) {
            continue; /* (as the game's: none for a ship in the air, nor for a time trial's ghost) */
        }
        face = track->faces + ship->nearTrkSect->faceStart;
        do {
            flags = face->flags;
            face++;
        } while (!(flags & 1));
        face--;
        for (k = 0; k < 4; k++) {
            int nose = k < 2 ? 1 : -1, wing = k & 1 ? 1 : -1;

            corner[k].vx = ship->ppivot.vx + nose * ((ship->unitVecNose.vx * 3) >> 5) + wing * (ship->unitVecWing.vx >> 4);
            corner[k].vy = ship->ppivot.vy + nose * ((ship->unitVecNose.vy * 3) >> 5) + wing * (ship->unitVecWing.vy >> 4);
            corner[k].vz = ship->ppivot.vz + nose * ((ship->unitVecNose.vz * 3) >> 5) + wing * (ship->unitVecWing.vz >> 4);
        }
        facePoint.vx = track->vertices[face->vertex[0]].vx;
        facePoint.vy = track->vertices[face->vertex[0]].vy;
        facePoint.vz = track->vertices[face->vertex[0]].vz;
        for (k = 0; k < 4; k++) {
            int alpha = PlaneLineCollision(facePoint, corner[k], face->normal);

            corner[k].vx -= (alpha * face->normal.vx) >> 12;
            corner[k].vy -= (alpha * face->normal.vy) >> 12;
            corner[k].vz -= (alpha * face->normal.vz) >> 12;
        }
        for (k = 0; k < 25; k++) {
            double s = (k % 5) / 4.0, t = (k / 5) / 4.0;
            double x = (1 - t) * ((1 - s) * corner[0].vx + s * corner[1].vx) + t * ((1 - s) * corner[2].vx + s * corner[3].vx);
            double y = (1 - t) * ((1 - s) * corner[0].vy + s * corner[1].vy) + t * ((1 - s) * corner[2].vy + s * corner[3].vy);
            double z = (1 - t) * ((1 - s) * corner[0].vz + s * corner[1].vz) + t * ((1 - s) * corner[2].vz + s * corner[3].vz);

            GteKeyNext(0, GteKeyMix(GteKeyMix((uint64_t)(uintptr_t)ship, 0x534841444FFull), (uint64_t)k) | GTE_KEY_MOVING);
            PortFarProject(&view, x, y, z, &grid[k]);
            if (grid[k].rejected) {
                break;
            }
        }
        if (k < 25) {
            continue;
        }
        /* sorted as DrawShadow sorts them: its two rows nearest the nose by the nose's left corner, the rest by the
           tail's (RotTransPers's depth: a quarter of the view depth) */
        entry[0] = (int)(grid[0].z / 4.0) - 175;
        entry[0] = entry[0] < 0 ? 0 : entry[0];
        entry[1] = entry[0] - 20 < 0 ? 0 : entry[0] - 20;
        entry[2] = (int)(grid[20].z / 4.0) - 100;
        entry[2] = entry[2] < 0 ? 0 : entry[2];
        for (row = 0; row < 4; row++) {
            for (col = 0; col < 4; col++) {
                POLY_FT4 *poly = &shadow->quad[row * 4 + col][CurrentScreen];
                int       g = row * 5 + col, e = entry[row < 2 ? row : 2];
                double    s = (col + 0.5) / 4.0, t = (row + 0.5) / 4.0;

                if (!PortShadowOnRoad(ship,
                                      (1 - t) * ((1 - s) * corner[0].vx + s * corner[1].vx) + t * ((1 - s) * corner[2].vx + s * corner[3].vx),
                                      (1 - t) * ((1 - s) * corner[0].vy + s * corner[1].vy) + t * ((1 - s) * corner[2].vy + s * corner[3].vy),
                                      (1 - t) * ((1 - s) * corner[0].vz + s * corner[1].vz) + t * ((1 - s) * corner[2].vz + s * corner[3].vz))) {
                    continue; /* (as DrawShadow: off the road) */
                }

                *(int *)&poly->x0 = grid[g].screen;
                *(int *)&poly->x1 = grid[g + 1].screen;
                *(int *)&poly->x2 = grid[g + 5].screen;
                *(int *)&poly->x3 = grid[g + 6].screen;
                GteBindPrecise(&poly->x0, grid[g].handle);
                GteBindPrecise(&poly->x1, grid[g + 1].handle);
                GteBindPrecise(&poly->x2, grid[g + 5].handle);
                GteBindPrecise(&poly->x3, grid[g + 6].handle);
                AddPrim(PortFarSlot(e, e * 4.0), poly);
            }
        }
    }
}

void PortFarDraw(TrackCamera *camera, ShipData *ships, Object **shipShapes)
{
    if (!Enabled() || !track) {
        frame++;
        return;
    }
    ClearOTagR(farTable[CurrentScreen], FAR_BUCKETS);
    arenaUsed = 0;
    farShips = 0;
    DrawFarTrack(camera);
    DrawFarScenery();
    DrawFarShips(ships, shipShapes);
    DrawFarShadows(camera, ships);
    AddPrims(OT[CurrentScreen] + FAR_ENTRY, farTable[CurrentScreen] + FAR_BUCKETS - 1, farTable[CurrentScreen]);
    frame++;
}

void PortFarFlared(int ship)
{
    if (ship >= 0 && ship < 15) {
        farShips &= ~(1 << ship);
    }
}

u_long *PortFlareSlot(int distance)
{
    return flareSlot ? flareSlot : OT[CurrentScreen] + (distance >> 1);
}

/* The engine flares of the ships the far view drew, where DrawTrail puts a near ship's: where its trail's newest
   segment would be, the size and the table entry from that point's depth less 160 (RotTransPers3's average). */
void PortFarFlares(TrackCamera *camera)
{
    PortView view;
    int      i;

    if (!farShips || sillyShips || !PortFarFlaresEnabled()) {
        farShips = 0;
        return;
    }
    CameraView(camera, &view);
    for (i = 0; i < 15; i++) {
        ShipData      *at = &shipIndex[i];
        PortFarVertex  centre;
        int            distance;

        if (!(farShips & (1 << i))) {
            continue;
        }
        /* where its trail's newest segment would be (UpdateShipTrails): the game keeps no trail for a ship out of
           its view, and the trail's last segment was left where the ship went out of it (a flare stood there) */
        GteKeyNext(0, GteKeyMix((uint64_t)(uintptr_t)at, 0x464152464Cull) | GTE_KEY_MOVING); /* frame interpolation */
        PortFarProject(&view, at->ppivot.vx - ((at->unitVecNose.vx * 420) >> 12),
                       at->ppivot.vy - ((at->unitVecNose.vy * 420) >> 12) - 15,
                       at->ppivot.vz - ((at->unitVecNose.vz * 420) >> 12), &centre);
        if (centre.rejected) {
            continue;
        }
        distance = (int)(centre.z * 3.0 * (short)GteReadControl(29) / 4096.0) - 160; /* ZSF3: RotTransPers3's OTZ */
        if (distance < 0) {
            distance = 0;
        }
        flareSlot = PortFarSlot(distance >> 1, (distance >> 1) * 4.0);
        {
            /* its size: speed * 8 / distance (DrawFlare), not below the 2 of a flare at the edge of the game's range
               (beyond it the flare would round away to nothing) */
            ShipData *ship = &shipIndex[i];
            int speed = (i != ownShip || !(ship->attr & RACING)) ? ship->speed >> 1 : ship->thrust >> 5;
            int sizeDistance = speed * 4 > 0 && distance > speed * 4 ? speed * 4 : distance;

            PortFlareCentre(centre.handle);
            DrawFlare((short)(centre.screen & 0xFFFF), (short)(centre.screen >> 16), sizeDistance, i);
        }
        flareSlot = NULL;
    }
    farShips = 0;
}

/* The distance fade: the settings' range (WIPEOUT_FADE "near,far" in world units, or 0 for none),
   64000 to 96000 by default; none without the far view (the console's own distance). */
void PortFade(int on)
{
    int nearDistance, farDistance, fade = PortFadeRange(&nearDistance, &farDistance);

    GteSetFade(on && fade && Enabled(), nearDistance, farDistance);
}
