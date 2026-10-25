/* The PlayStation's geometry coprocessor (GTE), as the PsyQ libgte routines
   the game calls use it, and the full-precision vertex cache the renderer
   draws from. */
#ifndef PSX_GTE_H
#define PSX_GTE_H

#include <stdint.h>

/* Registers, numbered as the hardware numbers them (data 0-31, control 0-31). */
typedef struct Gte {
    int32_t data[32];
    int32_t control[32];
} Gte;

extern Gte gte;

/* Coprocessor instructions, by their 25-bit command word. */
void GteExecute(uint32_t command);
/* MTC2/MFC2/CTC2/CFC2 with the register side effects the hardware has. */
void GteWriteData(int reg, int32_t value);
int32_t GteReadData(int reg);
void GteWriteControl(int reg, int32_t value);
int32_t GteReadControl(int reg);

/* The library's read-only tables (gte_tables.c, from the retail executable). */
extern const unsigned int gteTableBase, gteTableEnd;
extern const unsigned char gteTables[];
int GteTable16(unsigned int address);

/* Full-precision projections of this frame. Every RTPS/RTPT stores the exact
   screen position (before the hardware's rounding to whole pixels) and the
   view depth of each vertex, under the integer screen position the game
   received. The renderer resolves a primitive's vertices here together (see
   gte.c); a vertex not found did not come from a projection (2D arithmetic)
   and is drawn as given. */
typedef struct GtePrecise {
    float x, y;  /* screen position, in the game's 320x240 units */
    float z;     /* view-space depth (for perspective-correct texturing) */
    float fade;  /* the port's distance fade: 1 opaque .. 0 gone */
    int background; /* projected while GteSetBackground was on: behind everything (the depth buffer leaves it out) */
    int exact;      /* ... while GteSetExactDepth was on: depth tested without the slack */
    uint64_t key;   /* frame interpolation: what was projected (0: not known), see GteKeyNext */
    float view[3];  /* ... and where it was in view space */
    uint64_t anchor;       /* what moves with something: the key it goes with (GteRecordAnchor), or 0 */
    float anchorOffset[3]; /* ... and how far from that it is in view space */
} GtePrecise;

#define GTE_RESOLVE_MAX 4
/* The vertices xy[0..count-1] (GPU packet words) of one primitive: fills
   out[i] for each vertex found and returns the mask of those. */
unsigned GteResolvePrecise(const uint32_t *xy, const void *const *addresses, int count, GtePrecise *out);
/* Whether every vertex of the last primitive resolved was bound to its projection by the renderer that built it
   (not found by its pixel alone, which a 2D shape can match by chance). */
int GteResolvedBound(void);

/* Exact links. Each projection has a handle (0: none was recorded); GtePreciseHandle gives the handle
   of the projection in SXY0, SXY1 or SXY2 (registers 12-14, 15 for SXY2). A renderer that writes a
   projection's screen position into a packet binds the packet's xy word to the handle; the vertex is
   then drawn from that projection (while the word still holds its position), whatever else rounds to
   the same pixel. addresses[] of GteResolvePrecise: the xy words of the packet, or NULL. */
uint32_t GtePreciseHandle(int reg);
void GteBindPrecise(const void *address, uint32_t handle);
/* A packet's xy word the renderer placed in 2D (about a projection, say): the primitive is drawn as given, not
   matched to whatever projections land on the same pixels. */
void GteBindFlat(const void *address);
int GtePreciseOf(uint32_t handle, GtePrecise *out);
/* The handle bound to an address that still holds the screen position xy (libgte's projections bind the
   words they store to), or 0. */
uint32_t GteBoundHandle(const void *address, uint32_t xy);
/* A projection made outside the GTE (the port's far view): packet word sxy,
   precise screen position and depth. */
uint32_t GteRecordProjection(uint32_t sxy, float x, float y, float z, float fade);
/* A size x size grid of points the game placed on screen by halving between
   four projected corners (corner[]: the screen positions of grid points 0,
   size-1, size*(size-1) and size*size-1, cornerAddress[]: where the projection
   stored them; x[], y[]: the grid row by row, as the game computed it).
   Records each point at the matching place between the corners' precise
   positions, without a depth (drawn affine, as the console draws it), and
   gives each point's handle (0 where there is none). */
void GteRecordGrid(const uint32_t corner[4], const void *const cornerAddress[4], const int *x, const int *y, int size,
                   uint32_t *handles);
void GteClearPrecise(void);
/* While on, what is projected is the background (the sky), drawn behind everything else whatever its depth. */
void GteSetBackground(int on);
/* While on, what is projected is depth tested exactly, without the slack the game's own models have (a decal on
   what is under it is drawn over it in the game's order): a model made for a depth buffer, as Wipeout 64's. */
void GteSetExactDepth(int on);

/* Frame interpolation (the display drawn more often than the game's 30 frames a second): a projection made
   with a key is found again in the next frame by that key, and a vertex drawn between the two frames is placed
   between its two view-space positions. The renderers give the key of what they project (a model's vertex and
   which drawing of the model, a track vertex, a particle) just before the projection: GteKeyNext(slot, key)
   for the GTE's vertex register slot (0-2: the next RTPS uses 0, RTPT 0, 1 and 2; GteRecordProjection 0).
   GteRecordView gives a projection made outside the GTE its view-space position. */
void GteKeyNext(int slot, uint64_t key);
/* The number of the frame whose projections are being made (it changes when a drawn frame's next projection
   starts the next one). */
uint32_t GteFrameNumber(void);
/* The number of the frame whose projections are recorded, without starting the next (a frame drawn again). */
uint32_t GteFrameRecorded(void);
uint64_t GteKeyMix(uint64_t a, uint64_t b);
/* Set in the keys of what stands still in the world (the track): the view's motion between two frames, which moves
   what one frame projects and the other did not, is fitted to these. */
#define GTE_KEY_WORLD 0x8000000000000000ull
/* Set in the keys of what moves with a ship (its trail, its engine flare). Not projected under its key in the frame
   before (new, or jumped), such a vertex is not moved by the view's motion as if it stood still in the world: it goes
   as its anchor (GteRecordAnchor) went, or without one stays where the frame has it. */
#define GTE_KEY_MOVING 0x4000000000000000ull
/* The anchor of a projection made with a GTE_KEY_MOVING key: the projection under key `anchor`, which it is
   (dx, dy, dz) from in view space. */
void GteRecordAnchor(uint32_t handle, uint64_t anchor, double dx, double dy, double dz);
void GteRecordView(uint32_t handle, double x, double y, double z);
/* On while the display interpolates: the projections of the frame before are kept. */
void GteKeepPast(int on);
/* A vertex of the frame being drawn as it is a fraction `along` (0..1) of the way from the frame before:
   1 when *out was made, 0 when there is nothing to go by (no key, not in the frame before, too near, or the
   view jumped: a camera cut). */
int GteInterpolate(const GtePrecise *p, float along, GtePrecise *out);
/* A vertex's view-space position in the picture drawn: the frame's own (along >= 1) or `along` of the way from the
   frame before, as GteInterpolate places it but at any depth, also behind the camera (a polygon there is clipped at
   GteNearPlane, libgpu.c). 0 when the vertex has no view-space position. */
int GteViewAt(const GtePrecise *p, float along, double view[3]);
/* The nearest depth the GTE projects (half the projection distance), and a view-space point there or beyond
   projected: its screen position. */
double GteNearPlane(void);
void GteProjectView(const double view[3], float *x, float *y);
/* libgpu has drawn a frame's ordering table: the next projection starts a
   new frame's cache. */
void GteFrameDrawn(void);
/* The port's distance fade (as wipeout-rewrite's): while on, projections between nearDistance and
   farDistance from the camera fade out smoothly (GtePrecise.fade), and beyond it are gone. */
void GteSetFade(int on, double nearDistance, double farDistance);
float GteFade(double distance);
/* When false, no precise positions are recorded (the original look). */
extern int gtePrecision;

#endif
