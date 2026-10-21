/* Port: the whole track in view (see portfar.h). */
#include <math.h>

#include "functions.h"
#include "portfar.h"
#include "port_platform.h"
#include "../../platform/psx/gte.h"

extern TrackData *track;
extern Object   **trackObjects;
extern short      CurrentScreen;
extern int        ownShip;
extern int        raceType;

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

static u_long   farTable[2][FAR_BUCKETS];
static char     arena[2][ARENA_BYTES];
static int      arenaUsed;
static uint32_t frame = 1;
static uint32_t *faceMarks;      /* the frame each track face was last drawn by the game */
static int      faceMarkCount;
static uint32_t shipMarks[15];
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

static int Enabled(void)
{
    if (enabled < 0) {
        enabled = PortFarViewEnabled();
        if (enabled) {
            PsxLinkRegion(arena, sizeof(arena));
            PsxLinkRegion(farTable, sizeof(farTable));
        }
    }
    return enabled;
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
}

u_long *PortFarSlot(int entry, double depth)
{
    int bucket;

    if (entry >= 0 && entry < FAR_ENTRY) {
        return OT[CurrentScreen] + entry;
    }
    bucket = (int)((depth - FAR_START) / FAR_STEP);
    bucket = bucket < 0 ? 0 : bucket >= FAR_BUCKETS ? FAR_BUCKETS - 1 : bucket;
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

void PortFarRelease(char *end)
{
    arenaUsed = (int)(end - arena[CurrentScreen]);
}

/* ---- the pass ---- */

/* Whether a sphere about camera-space centre c might be on screen. */
static int MaybeVisible(const double c[3], double radius)
{
    if (GteFade(sqrt(c[0] * c[0] + c[1] * c[1] + c[2] * c[2]) - radius) <= 0.0f) {
        return 0; /* faded out entirely */
    }
    double h = (double)(u_short)GteReadControl(26);
    double z = c[2] + radius;

    if (z < h * 0.5) {
        return 0;
    }
    /* the screen is 320x240 about the centre: a generous cone */
    return fabs(c[0]) - radius < z * (200.0 / h) && fabs(c[1]) - radius < z * (160.0 / h);
}

static void DrawFarTrack(TrackCamera *camera)
{
    PortView view;
    Skeleton *camPos = camera->camPos;
    int i, k;

    /* the track renderers: SetSkeletonTransform(camera->super), corners + camera->relative.t */
    SkeletonView(camPos->super, &view);
    for (i = 0; i < 3; i++) {
        view.t[i] += view.m[i][0] * camPos->relative.t[0] + view.m[i][1] * camPos->relative.t[1] +
                     view.m[i][2] * camPos->relative.t[2];
    }
    for (i = 0; i < track->sectionCount; i++) {
        TrackSection *section = track->sections + i;
        double c[3];

        for (k = 0; k < 3; k++) {
            c[k] = view.m[k][0] * section->centre.vx + view.m[k][1] * section->centre.vy + view.m[k][2] * section->centre.vz +
                   view.t[k];
        }
        if (MaybeVisible(c, 4096.0 + section->globalRadius)) {
            PortTrackFar(section, &view);
        }
    }
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
        if (MaybeVisible(view.t, (double)object->extent)) {
            PortDrawObjectFar(object, &view, NULL);
        }
    }
}

/* The ships the game left out for their distance, drawn as DrawShips draws the near ones. */
static void DrawFarShips(ShipData *ships, Object **shipShapes)
{
    int i;

    if (raceType == 1 || raceType == 3) {
        return; /* time trial and the two-player race draw only their own ships, always near */
    }
    for (i = 0; i < 15; i++) {
        ShipData *ship = &ships[i];
        Object   *object;
        Skeleton  skeleton;
        PortView  view;
        Face     *face;
        u_char    flags;

        if (shipMarks[i] == frame || (ship->combatAttr & DESTROYED) || !ship->nearTrkSect ||
            (ship->attr & (REMOTE_VIEW | VIEW_IN)) == VIEW_IN) {
            continue;
        }
        object = shipShapes[ship->highResTableIndex];
        skeleton = *object->skeleton;
        SetSkeletonPosition(&skeleton, ship->ppivot.vx, ship->ppivot.vy, ship->ppivot.vz);
        SetSkeletonDirectionHPR(&skeleton, ship->hdg, ship->pitch, ship->roll);
        SkeletonView(&skeleton, &view);
        if (!MaybeVisible(view.t, (double)object->extent + 1024.0)) {
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
        PortDrawObjectFar(object, &view, (const CVECTOR *)&face->red);
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
    DrawFarTrack(camera);
    DrawFarScenery();
    DrawFarShips(ships, shipShapes);
    AddPrims(OT[CurrentScreen] + FAR_ENTRY, farTable[CurrentScreen] + FAR_BUCKETS - 1, farTable[CurrentScreen]);
    frame++;
}

/* The distance fade: from WIPEOUT_FADE ("near,far" in world units, or 0 for none), 64000 to 96000 by
   default; none without the far view (the console's own distance). */
void PortFade(int on)
{
    static int read;
    static double nearDistance = 64000.0, farDistance = 96000.0;
    static int disabled;

    if (!read) {
        const char *value = getenv("WIPEOUT_FADE");

        read = 1;
        if (value) {
            double a = 0, b = 0;

            if (sscanf(value, "%lf,%lf", &a, &b) == 2 && b > a && a > 0) {
                nearDistance = a;
                farDistance = b;
            } else {
                disabled = 1;
            }
        }
    }
    GteSetFade(on && !disabled && Enabled(), nearDistance, farDistance);
}
