/* N64 signs select their own model faces and preserve the ROM's full transforms. */
#include "../../game/src/portorig.c"
#include <assert.h>
#include "guest_sign_placements.h"
int portPal;
static int guest64 = 1;
int PortOriginalTrack(void) { return guest64 ? 0 : -1; }
int PortOriginalTrackIs64(void) { return guest64; }
static Skeleton camera;
static TrackCamera view = {&camera};
TrackCamera *cameraPtr = &view;
static Object *drawn[2];
static int drawCount;
void SetSkeletonPosition(Skeleton *s, int x, int y, int z)
{ s->relative.t[0] = x; s->relative.t[1] = y; s->relative.t[2] = z; s->update = 1; }
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
    previous.centre.vx = 1000; previous.centre.vz = -1000;
    section.nextSection.ptr = &section;
    for (int i = 0; i < 8; i++) {
        parts[i].skeleton = &skeletons[i]; parts[i].primitives = (Primitive *)&panels[i];
        parts[i].vertices = bounds; parts[i].vertexCount = 2; signParts[i] = &parts[i];
        skeletons[i].super = &camera;
    }
    assert(PortOriginalDrawDroid(&section, 2, 0, 0));
    assert(drawn[0] == &parts[7] && drawn[1] == &parts[6]);
    assert(skeletons[7].relative.t[0] == 1000 && skeletons[7].relative.t[1] == -2300);
    assert(skeletons[6].super == skeletons[7].super && skeletons[6].relative.t[2] == -1200);
    assert(panels[6].colour.r == 128);
    drawCount = 0; assert(PortOriginalDrawDroid(&section, 2, 0, 10));
    assert(panels[6].colour.r == 64);
    drawCount = 0; assert(PortOriginalDrawDroid(&section, 0, 0, 0)); assert(drawn[1] == &parts[2]);
    drawCount = 0; assert(PortOriginalDrawDroid(&section, 1, 0, 0)); assert(drawn[1] == &parts[4]);
    drawCount = 0; assert(PortOriginalDrawDroid(&section, 3, 1, 0)); assert(drawn[1] == &parts[0]);
    {
        TrackSection next = {0};
        /* A low roof from the old fitting cache must not move or scale a sign. */
        boardSections[0] = &section; boardTops[0] = -1050; boardBottoms[0] = 1050;
        boardHeights[0] = -900; boardScales[0] = 2000; boardCount = 1;
        section.nextSection.ptr = &next; next.prevSection.ptr = &section;
        for (unsigned i = 0; i < sizeof(signPlacements) / sizeof(signPlacements[0]); i++) {
            const struct SignPlacement *p = &signPlacements[i];
            section.centre = (Vector){p->input[0], p->input[1], p->input[2]};
            next.centre = (Vector){p->input[3], p->input[4], p->input[5]};
            previous.centre = (Vector){p->input[6], p->input[7], p->input[8]};
            drawCount = 0;
            assert(PortOriginalDrawDroid(&section, 2, 0, 0));
            for (int row = 0; row < 3; row++) {
                assert(skeletons[7].relative.t[row] == p->body[row]);
                assert(skeletons[6].relative.t[row] == p->panel[row]);
                for (int col = 0; col < 3; col++) {
                    assert(skeletons[7].relative.m[row][col] == p->matrix[row * 3 + col]);
                    assert(skeletons[6].relative.m[row][col] == p->matrix[row * 3 + col]);
                }
            }
            assert(skeletons[6].super == skeletons[7].super);
            assert(skeletons[6].update && skeletons[7].update);
        }
    }
    guest64 = 0; drawCount = 0;
    assert(!PortOriginalDrawDroid(&section, 2, 0, 0) && drawCount == 0);
    puts("N64 sign faces/blinking and all 18 ROM checkpoint transforms: OK");
    return 0;
}
