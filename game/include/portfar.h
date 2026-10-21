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

/* The pass, after the game's scenery (racectrl.c). */
void PortFarDraw(TrackCamera *camera, ShipData *ships, Object **shipShapes);

/* For the renderers' far modes (trackdraw.c, objdraw.c). */
void PortFarProject(const PortView *view, double x, double y, double z, PortFarVertex *out);
/* A camera-space point in front of the near plane (PORT_FAR_NEAR), wherever it lands on the screen: the
   packet keeps its position clamped to the GPU's coordinates, the precise projection holds the true one. */
#define PORT_FAR_NEAR 16.0
void PortFarProjectCamera(double cx, double cy, double cz, PortFarVertex *out);
void PortFarDepthBias(double scale); /* the depth of what is projected next is scaled (drawn behind) */
u_long *PortFarSlot(int entry, double depth);
char *PortFarAlloc(int bytes);
void PortFarRelease(char *end); /* the memory from end on, of the last PortFarAlloc, is not used */
/* Memory for primitives the port adds to the game's own drawing (objdraw.c), this frame: NULL when full or
   the far view is off. */
char *PortNearAlloc(int bytes);
void PortTrackFar(TrackSection *section, const PortView *view);
void PortDrawObjectFar(Object *object, const PortView *view, const CVECTOR *colour);

#endif
