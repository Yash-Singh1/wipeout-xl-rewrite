/* Projection failures at one point must not hide the arc's other valid segments. */
#include "../../game/src/sparks.c"
#include <assert.h>
#include <string.h>

Electric electrics[32];
int rockflareat;
short CurrentScreen;
static u_long ordering[8192];
u_long *OT[2] = {ordering, ordering};
static Skeleton parent, camera;
static TrackCamera testCamera;
TrackCamera *cameraPtr = &testCamera;
static int flags[4], projection, tracing, count;
static void *linked[3];
int PortTracing(void) { return tracing; }
int newRand(void) { return 128; }
void SetSkeletonTransform(Skeleton *skeleton) { (void)skeleton; }
int RotTransPers(SVECTOR *v, int *sxy, int *p, int *flag)
{
    (void)v;
    *sxy = (40 + projection * 20) | (80 << 16);
    *p = 0;
    *flag = flags[projection++];
    return 400;
}
void AddPrim(u_long *slot, void *poly)
{
    assert(slot == ordering + 110);
    assert(count < 3);
    linked[count++] = poly;
}
static void Run(int start, int end, int bend, int last, int trace)
{
    VECTOR position = {0, 0, 4000};
    flags[0] = start; flags[1] = end; flags[2] = bend; flags[3] = last;
    projection = count = rockflareat = 0;
    tracing = trace;
    AddElectric(&position, 0);
}
int main(void)
{
    camera.super = &parent;
    testCamera.camPos = &camera;
    Run(0, 0, 0, 0, 0);
    assert(count == 3);
    assert(linked[0] == &electrics[1].line1[0]);
    assert(linked[1] == &electrics[1].line2[0]);
    assert(linked[2] == &electrics[1].line3[0]);
    Run(0, 0x20000, 0, 0, 0);
    assert(count == 2);
    assert(linked[0] == &electrics[1].line1[0]);
    assert(linked[1] == &electrics[1].line2[0]);
    Run(0, 0, 0x400000, 0, 0);
    assert(count == 1 && linked[0] == &electrics[1].line3[0]);
    Run(0, 0x20000, 0x400000, 0, 0);
    assert(count == 0);
    /* Keep the original start/last-bend tests, and exact original packets in tracing mode. */
    Run(0x20000, 0, 0, 0, 0); assert(count == 0);
    Run(0, 0, 0, 0x20000, 0); assert(count == 0);
    Run(0, 0x20000, 0x400000, 0, 1); assert(count == 3);
    puts("electric arcs retain every segment with valid endpoints: OK");
    return 0;
}
