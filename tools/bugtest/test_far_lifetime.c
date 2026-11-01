/* The main far pass must preserve checkpoint packets allocated earlier in the frame. */
#include "../../game/src/portfar.c"
#include "../../platform/psx/libgpu.c"
#include <assert.h>
#include <stdio.h>
short CurrentScreen;
int PortFarViewEnabled(void) { return 1; }
static u_long mainOT[8192];
u_long *OT[2] = {mainOT, mainOT};
TrackData data;
TrackData *track = &data;
Object **trackObjects;
Shadow *shadowData;
ShipData *shipIndex;
int ownShip, remoteShip, ghostShip, raceType;
/* An empty scene exercises the real pass without drawing track or ship geometry. */
int PlaneLineCollision(VECTOR p, VECTOR s, SVector n) { abort(); }
int PortScreenMargin(void) { return 0; }
void PortDrawObjectFar(Object *o, const PortView *v, const CVECTOR *c) { abort(); }
void PortTrackFar(TrackSection *s, const PortView *v) { abort(); }
void PortInterpInstance(int i) { abort(); }
void InitTransShip(Object *o, short enabled) { abort(); }
int main(void)
{
    ShipData ships[15] = {0};
    Skeleton root = {0}, cam = {0};
    TrackCamera camera = {0};
    cam.super = &root; camera.camPos = &cam;
    ClearOTagR(mainOT, 8192);
    POLY_F3 *early = (POLY_F3 *)PortFarAlloc(64);
    SetPolyF3(early); early->r0 = 42;
    AddPrim(mainOT + 2, early);
    u_long *slot = PortFarSlot(8191, 40000);
    u_long link = *slot;
    /* Checkpoint board packets exist before the main far-view pass. */
    PortFarDraw(&camera, ships, NULL);
    POLY_F3 *late = (POLY_F3 *)PortFarAlloc(64);
    SetPolyF3(late); late->r0 = 99;
    AddPrim(mainOT + 2, late);
    assert(NodeAfter((u_long *)late) != (u_long *)late);
    assert((char *)late == (char *)early + 64);
    assert(NodeAfter((u_long *)late) == (u_long *)early);
    assert(NodeAfter((u_long *)early) == mainOT + 1);
    assert(early->r0 == 42 && *slot == link);
    CurrentScreen = 1;
    assert(PortFarAlloc(32) != (char *)early);
    CurrentScreen = 0;
    assert(PortFarAlloc(32) == (char *)late + 64);
    GteClearPrecise();
    assert(PortFarAlloc(64) == (char *)early);
    slot = PortFarSlot(8191, 40000);
    assert(NodeAfter(slot) == slot - 1);
    puts("checkpoint packets survive the main far pass; no memory reuse or self-linked list: OK");
    return 0;
}
