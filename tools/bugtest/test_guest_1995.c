/* Original scenery timing, object selection and sky-edge colours. No disc required. */
#include "../../game/src/portorig.c"
#include <assert.h>
#include "guest_1995_frames.h"

int portPal;
static int guest = 11, guestClass = 2;
static const char *guestModel = "silverstream";
int PortOriginalTrack(void) { return guest; }
int PortOriginalTrackIs64(void) { return 0; }
int PortOriginalTrackClass(int i) { (void)i; return guestClass; }
const char *PortOriginalTrackModel(int i) { (void)i; return guestModel; }
void PortOriginalAnimate(int tick) { (void)tick; assert(0); }
void InitStartBoom(Object **prm, short count) { (void)prm; (void)count; }
void ResetStartBoom(void) {}
unsigned short TextureTableCount;
TrackCamera *cameraPtr;
void LoadCmpFiles(TIMlist *list) { (void)list; }
TIMlist *LoadCompressedTextureSequence(char *name) { (void)name; return NULL; }
Object *LoadPrm(char *name, short base) { (void)name; (void)base; return NULL; }

int main(void)
{
    Object scenery[5] = {0}, sky = {0};
    Object *prm[] = {&scenery[0], &sky};
    Skeleton skeletons[5] = {0};
    GT4 lights[3] = {0};
    SVECTOR vertices[16] = {0};
    struct { GT3 triangle; GT4 quads[81]; } polygons = {0};
    const short heights[] = {-6500, -8000, -12000, -5000};
    for (int i = 0; i < 5; i++) {
        strcpy(scenery[i].name, i < 3 ? "redl" : "donkey");
        scenery[i].skeleton = &skeletons[i];
        skeletons[i].relative.t[0] = 100 + i;
        if (i < 3) scenery[i].primitives = (Primitive *)&lights[i];
        if (i < 4) scenery[i].next = &scenery[i + 1];
    }
    sky.vertices = vertices; sky.vertexCount = 16;
    sky.primitives = (Primitive *)&polygons; sky.primitiveCount = 82;
    polygons.triangle.type = TypeGT3;
    for (int i = 0; i < 16; i++) vertices[i].vy = heights[i / 4];
    for (int i = 0; i < 81; i++) {
        GT4 *p = &polygons.quads[i];
        p->type = TypeGT4;
        for (int k = 0; k < 4; k++) {
            p->coords[k] = (short)((i < 80 ? i % 3 : 3) * 4 + k);
            p->colour[k] = (CVECTOR){10 + k, 20 + k, 30 + k, 0};
        }
    }
    PortOriginalSceneInit(prm, 2, &sky);
    assert(beaconCount == 1 && beacons[0] == &scenery[2]);
    assert(pumpCount == 2 && auroraCount == 80);
    assert(aurora[0].corners == 3 && aurora[1].corners == 15 && aurora[2].corners == 12);
    for (unsigned i = 0; i < sizeof(sceneryFrames) / sizeof(sceneryFrames[0]); i++) {
        const struct SceneryFrame *f = &sceneryFrames[i];
        MoveOriginalScenery(f->tick);
        for (int k = 0; k < 4; k++) {
            assert(lights[0].colour[k].r == 0 && lights[1].colour[k].r == 0);
            assert(lights[2].colour[k].r == f->red);
            assert(lights[2].colour[k].g == 0 && lights[2].colour[k].b == 0);
        }
        for (int pump = 3; pump < 5; pump++) {
            assert(skeletons[pump].relative.t[0] == 100 + pump);
            for (int row = 0; row < 3; row++) for (int col = 0; col < 3; col++)
                assert(skeletons[pump].relative.m[row][col] == f->matrix[row * 3 + col]);
        }
        for (int q = 0; q < 80; q++) for (int k = 0; k < 4; k++) {
            CVECTOR c = polygons.quads[q].colour[k];
            int at = q % 3 * 12 + k * 3;
            assert(c.r == f->aurora[at] && c.g == f->aurora[at + 1] && c.b == f->aurora[at + 2]);
        }
        assert(polygons.quads[80].colour[0].r == 10);
    }
    /* Equal elapsed time produces identical original ticks in NTSC and PAL. */
    for (portPal = 0; portPal <= 1; portPal++) {
        sceneryFrame = 0;
        for (int i = 0; i < (portPal ? 25 : 30); i++) PortOriginalSceneUpdate(0);
        assert(lights[2].colour[0].r == sceneryFrames[3].red); /* original tick 30 */
        for (int row = 0; row < 3; row++) for (int col = 0; col < 3; col++)
            assert(skeletons[3].relative.m[row][col] == sceneryFrames[3].matrix[row * 3 + col]);
        for (int q = 0; q < 3; q++) for (int k = 0; k < 4; k++) {
            CVECTOR c = polygons.quads[q].colour[k];
            int at = q * 12 + k * 3;
            assert(c.r == sceneryFrames[3].aurora[at] && c.g == sceneryFrames[3].aurora[at + 1] &&
                   c.b == sceneryFrames[3].aurora[at + 2]);
        }
        GT4 frozen = polygons.quads[1];
        MATRIX pump = skeletons[3].relative;
        for (int i = 0; i < 100; i++) PortOriginalSceneUpdate(1);
        assert(!memcmp(&frozen, &polygons.quads[1], sizeof(frozen)));
        assert(!memcmp(&pump, &skeletons[3].relative, sizeof(pump)));
    }
    portPal = 0;
    guestClass = 1; PortOriginalSceneInit(prm, 2, &sky); assert(!auroraCount);
    guestClass = 2; guestModel = "altima";
    PortOriginalSceneInit(prm, 2, &sky); assert(!auroraCount);
    guestModel = "silverstream"; PortOriginalSceneInit(prm, 2, &sky); assert(auroraCount == 80);
    guest = -1; PortOriginalSceneInit(prm, 2, &sky);
    assert(!auroraCount && !beaconCount && !pumpCount);
    puts("1995 pump matrices, selected beacon, Silverstream aurora, PAL timing and pause: OK");
    return 0;
}
