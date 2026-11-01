/* N64 signs select their own model faces and keep the panel attached to its housing. */
#include "../../game/src/portorig.c"
#include <assert.h>
int portPal;
static int guest64 = 1;
int PortOriginalTrack(void) { return guest64 ? 0 : -1; }
int PortOriginalTrackIs64(void) { return guest64; }
static Skeleton camera;
static TrackCamera view = {&camera};
TrackCamera *cameraPtr = &view;
static Object *drawn[2];
static int drawCount;
int ratan2(int x, int z) { (void)x; (void)z; return 0; }
void SetSkeletonDirection(Skeleton *s, short x, short y, short z)
{
    (void)x; (void)y; (void)z;
    memset(s->relative.m, 0, sizeof(s->relative.m));
    s->relative.m[0][0] = s->relative.m[1][1] = s->relative.m[2][2] = 4096;
}
void SetSkeletonDirectionHPR(Skeleton *s, short h, short p, short r) { SetSkeletonDirection(s, h, p, r); }
void SetSkeletonPosition(Skeleton *s, int x, int y, int z)
{ s->relative.t[0] = x; s->relative.t[1] = y; s->relative.t[2] = z; }
void AsmDrawObject(Object *o, Skeleton *cam, int roll)
{ (void)cam; (void)roll; assert(drawCount < 2); drawn[drawCount++] = o; }
void PortFarObject(Object *o) { (void)o; }
void PortInterpInstance(int instance) { (void)instance; }
int main(void)
{
    Object parts[8] = {0}; Skeleton skeletons[8] = {0}; FT4 panels[8] = {0};
    SVECTOR bounds[] = {{0,-1050,0},{0,1050,0}};
    TrackSection section = {0}, previous = {0};
    section.prevSection.ptr = &previous; section.centre.vx = 1000; section.centre.vy = 200;
    for (int i = 0; i < 8; i++) {
        parts[i].skeleton = &skeletons[i]; parts[i].primitives = (Primitive *)&panels[i];
        parts[i].vertices = bounds; parts[i].vertexCount = 2; signParts[i] = &parts[i];
    }
    assert(PortOriginalDrawDroid(&section, 2, 0, 0));
    assert(drawn[0] == &parts[7] && drawn[1] == &parts[6]);
    assert(skeletons[7].relative.t[0] == 1000 && skeletons[7].relative.t[1] == -2300);
    assert(skeletons[6].super == &skeletons[7] && skeletons[6].relative.t[2] == -1200);
    assert(panels[6].colour.r == 128);
    drawCount = 0; assert(PortOriginalDrawDroid(&section, 2, 0, 10));
    assert(panels[6].colour.r == 64);
    drawCount = 0; assert(PortOriginalDrawDroid(&section, 0, 0, 0)); assert(drawn[1] == &parts[2]);
    drawCount = 0; assert(PortOriginalDrawDroid(&section, 1, 0, 0)); assert(drawn[1] == &parts[4]);
    drawCount = 0; assert(PortOriginalDrawDroid(&section, 3, 1, 0)); assert(drawn[1] == &parts[0]);
    guest64 = 0; drawCount = 0;
    assert(!PortOriginalDrawDroid(&section, 2, 0, 0) && drawCount == 0);
    puts("N64 checkpoint housing, C/S/F faces, arrow direction and blinking: OK");
    return 0;
}
