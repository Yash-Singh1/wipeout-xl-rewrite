/* Port: the whole track in view.

   The console drew only what its budget allowed: the track sections in the camera section's view lists,
   the scenery objects listed for the camera's section within 38200 units, and the other ships within
   9600. The rest popped in as the camera approached. The port draws everything else too, in a pass of
   its own after the game has drawn (portfar.c): the game's drawing and everything it decides are left
   as they are, the pass only adds primitives. They are projected in full precision (no 16-bit limits),
   linked into the game's ordering table at the slot the game would give them, or, beyond its range,
   into a table of their own drawn right after the sky. Off in the lockstep tests (WIPEOUT_TRACE) and
   with WIPEOUT_FAR_VIEW=0. */
#ifndef PORTFAR_H
#define PORTFAR_H

#include "wipeout.h"

/* A view transform in full precision: camera space = m * p + t. */
typedef struct PortView {
    double m[3][3];
    double t[3];
} PortView;

/* A vertex projected by the pass: the integer screen position as the game would pack it, its depth,
   its precise screen position and whether it is rejected (behind the near plane or off the GPU's range). */
typedef struct PortFarVertex {
    int    screen;
    double z;
    double x, y;
    int    rejected;
    unsigned int handle; /* the precise projection (gte.h) */
} PortFarVertex;

/* The distance fade (as wipeout-rewrite's), on during a race's 3D drawing. */
void PortFade(int on);

/* The game marks what it drew this frame. */
void PortFarMarkFace(Face *face);     /* a track face the game drew (some of) */
int PortFarFaceDrawn(Face *face);
/* The game draws an object: the flags of its primitives, to be set for each it handles (NULL: no far
   view). The far view draws the rest (PortFarObjectDrawn: NULL when the game did not draw it). */
unsigned char *PortFarObjectPrimitives(Object *object);
const unsigned char *PortFarObjectDrawn(Object *object, int *complete);
void PortFarMarkShip(int ship);
/* DrawShadow gave the ship a shadow this frame (the far view gives the other ships drawn theirs). */
void PortShadowDrawn(int ship);
/* Whether a point of a ship's shadow (world space, on the plane of the road face under the ship) lies over the road of
   the sections about the ship: the plane goes on past the end of a ramp, where the shadow hung in the air. */
int PortShadowOnRoad(const ShipData *ship, double x, double y, double z);
/* DrawShadow's grid of 5 x 5 points (x[], y[]: as the game halved its way between the corners on the screen), each
   recorded where it lies in the world between the shadow's corners (corner[]: world positions, the nose's left, its
   right, the tail's left, its right), with its depth: the depth buffer keeps the shadow under the ship and on the
   road, and libgpu cuts it at the camera. handles[]: the projections, bound to the packets by DrawShadow. */
void PortShadowGrid(TrackCamera *camera, const ShipData *ship, const VECTOR corner[4], const int *x, const int *y,
                    uint32_t handles[25]);

/* The pass, after the game's scenery (racectrl.c). */
void PortFarDraw(TrackCamera *camera, ShipData *ships, Object **shipShapes);
/* An object the game draws apart from the scene's, after PortFarDraw (a checkpoint droid's board): what of it the
   game's drawing left out for its depth, drawn by the far view, so that it does not pop in. */
void PortFarObject(Object *object);

/* The engine flares of the ships the far view drew (after DrawShipTrails, which draws the near ones'): DrawFlare
   says which it drew (PortFarFlared) and links them where PortFlareSlot says. */
void PortFarFlares(TrackCamera *camera);
void PortFarFlared(int ship);
u_long *PortFlareSlot(int distance);

/* For the renderers' far modes (trackdraw.c, objdraw.c). */
void PortFarProject(const PortView *view, double x, double y, double z, PortFarVertex *out);
/* A camera-space point in front of the near plane (PORT_FAR_NEAR), wherever it lands on the screen: the
   packet keeps its position clamped to the GPU's coordinates, the precise projection holds the true one. */
#define PORT_FAR_NEAR 16.0
void PortFarProjectCamera(double cx, double cy, double cz, PortFarVertex *out);
/* ... any camera-space point, also behind the camera: libgpu places a polygon with such a corner from the corners'
   view-space positions in each picture, cut just in front of the camera (the pictures between two frames see what
   the camera has just passed). The track's faces are drawn this far behind the camera for them. */
void PortFarProjectAny(double cx, double cy, double cz, PortFarVertex *out);
#define PORT_FAR_BEHIND 4096.0
/* ... and this many pixels around the screen (where the camera looked a frame before, turning) */
#define PORT_FAR_AROUND 64
void PortFarDepthBias(double scale); /* the depth of what is projected next is scaled (drawn behind) */
u_long *PortFarSlot(int entry, double depth);
char *PortFarAlloc(int bytes);
void PortFarRelease(char *end); /* the memory from end on, of the last PortFarAlloc, is not used */
/* Memory for primitives the port adds to the game's own drawing (objdraw.c), this frame: NULL when full or
   the far view is off. */
char *PortNearAlloc(int bytes);
void PortTrackFar(TrackSection *section, const PortView *view);
void PortDrawObjectFar(Object *object, const PortView *view, const CVECTOR *colour);

/* Frame interpolation (objdraw.c, gte.h): the next model the game draws is drawing number `instance` of it this
   frame (the ships share models: their ship number). */
void PortInterpInstance(int instance);

#endif
