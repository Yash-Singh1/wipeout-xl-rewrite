/* Near scenery must retain the PS1 quad's corners, diagonal, UVs and ordering depth. */
#include "../../game/src/objdraw.c"
#include <assert.h>
#include <string.h>

static union { POLY_GT4 gt; POLY_G4 g; char bytes[1024]; } arena;
static u_long ordering[8192];
static int linked;
static void *packet;
static u_long *packetSlot;
static int originalLook;
int PortOriginalLooks(void) { return originalLook; }
int PortTracing(void) { return 0; }
int PortScreenMargin(void) { return 0; }
char *PortNearAlloc(int bytes) { assert(bytes <= sizeof(arena)); return arena.bytes; }
u_long *PortFarSlot(int entry, double depth) { (void)depth; return ordering + entry; }
void PortTagPrims(const void *first, const void *end, const char *kind, const char *name, int index)
{ (void)first; (void)end; (void)kind; (void)name; (void)index; }
void AddPrim(u_long *slot, void *poly) { linked++; packet = poly; packetSlot = slot; }
void SetPolyGT3(POLY_GT3 *p) { p->code = 0x34; }
void SetPolyGT4(POLY_GT4 *p) { p->code = 0x3c; }
void SetPolyG3(POLY_G3 *p) { p->code = 0x30; }
void SetPolyG4(POLY_G4 *p) { p->code = 0x38; }
/* Record the inputs unchanged. The production projection and GPU clipper run in gameplay checks. */
void PortFarProjectAny(double x, double y, double z, PortFarVertex *out)
{
    out->screen = 0;
    out->handle = GteRecordProjection(0, 0, 0, z, 1);
    GteRecordView(out->handle, x, y, z);
}
static void CheckView(const short *xy, const SVECTOR *expected)
{
    GtePrecise p;
    assert(GtePreciseOf(GteBoundHandle(xy, *(const uint32_t *)xy), &p));
    assert(p.view[0] == expected->vx && p.view[1] == expected->vy && p.view[2] == expected->vz);
}
int main(void)
{
    SVECTOR vertices[4] = {{-100,-100,200}, {100,-100,200}, {-100,100,200}, {100,100,-100}};
    FT4 face = {0};
    Object object = {0};
    Poly prim;
    GteWriteControl(0, 4096); GteWriteControl(2, 4096); GteWriteControl(4, 4096);
    GteWriteControl(26, 160); GteWriteControl(29, 341); GteWriteControl(30, 256);
    object.vertices = vertices; object.vertexCount = 4;
    face.type = TypeFT4; face.flag = Translucent;
    face.coords[0] = 0; face.coords[1] = 1; face.coords[2] = 2; face.coords[3] = 3;
    face.u0 = (char)130; face.v0 = (char)140; face.u1 = (char)250; face.v1 = (char)140;
    face.u2 = (char)130; face.v2 = (char)240; face.u3 = (char)250; face.v3 = (char)240;
    face.tsb = 17; face.cba = 42; face.colour.r = 64; face.colour.g = 96; face.colour.b = 128;
    prim.ft4 = &face;
    /* Reproject all four corners so their exact camera positions can be checked. */
    for (int i = 0; i < 4; i++) {
        projected[i].flag = 1;
        projected[i].depth = vertices[i].vz > 0 ? vertices[i].vz : 0;
    }
    assert(DrawNearPrimitive(&object, 0, ordering, prim));
    assert(linked == 1 && packet == &arena.gt);
    assert(arena.gt.code == 0x3e); /* a quad, not a fan on the other diagonal */
    assert(arena.gt.u0 == 130 && arena.gt.u1 == 250 && arena.gt.u2 == 130 && arena.gt.u3 == 250);
    assert(arena.gt.v0 == 140 && arena.gt.v1 == 140 && arena.gt.v2 == 240 && arena.gt.v3 == 240);
    assert(arena.gt.tpage == 17 && arena.gt.clut == 42);
    assert(arena.gt.r3 == 64 && arena.gt.g3 == 96 && arena.gt.b3 == 128);
    CheckView(&arena.gt.x0, &vertices[0]); CheckView(&arena.gt.x1, &vertices[1]);
    CheckView(&arena.gt.x2, &vertices[2]); CheckView(&arena.gt.x3, &vertices[3]);
    assert(packetSlot == ordering + 37); /* original AVSZ4, including the rejected corner's saturated depth */
    /* The plain renderer keeps nontextured Gouraud colours, but flattens textured Gouraud faces. */
    {
        G4 smooth = {0};
        smooth.type = TypeG4;
        memcpy(smooth.coords, face.coords, sizeof(smooth.coords));
        for (int i = 0; i < 4; i++) {
            smooth.colour[i].r = 10 + i * 20;
            smooth.colour[i].g = 20 + i * 20;
            smooth.colour[i].b = 30 + i * 20;
        }
        linked = 0; prim.g4 = &smooth;
        assert(DrawNearPrimitive(&object, 0, ordering, prim));
        assert(linked == 1 && arena.g.code == 0x38);
        assert(arena.g.r0 == 10 && arena.g.r1 == 30 && arena.g.r2 == 50 && arena.g.r3 == 70);
        CheckView(&arena.g.x3, &vertices[3]);
    }
    {
        GT3 smooth = {0};
        smooth.type = TypeGT3;
        memcpy(smooth.coords, face.coords, sizeof(smooth.coords));
        smooth.colour[0].r = 40; smooth.colour[1].r = 80; smooth.colour[2].r = 120;
        linked = 0; prim.gt3 = &smooth;
        assert(DrawNearPrimitive(&object, 0, ordering, prim));
        assert(linked == 1 && arena.gt.code == 0x34);
        assert(arena.gt.r0 == 40 && arena.gt.r1 == 40 && arena.gt.r2 == 40);
    }
    prim.ft4 = &face;
    /* Fully hidden polygons and invalid vertex references must still be rejected. */
    linked = 0;
    for (int i = 0; i < 4; i++) vertices[i].vz = -100;
    assert(!DrawNearPrimitive(&object, 0, ordering, prim) && linked == 0);
    face.coords[3] = 4;
    assert(!DrawNearPrimitive(&object, 0, ordering, prim));
    /* The 1995 sky keeps four independent colours at the fixed background slot.
       Flattening these to colour[0] turns the aurora into solid bands. */
    {
        GT4 sky = {0}; GT3 triangle = {0};
        originalLook = 1;
        assert(GouraudColours(DRAW_FIXED) && GouraudColours(DRAW_LIT));
        assert(!GouraudColours(DRAW_PLAIN));
        originalLook = 0;
        assert(!GouraudColours(DRAW_FIXED) && GouraudColours(DRAW_LIT));
        for (int i = 0; i < 4; i++) {
            sky.coords[i] = (short)i;
            sky.colour[i] = (CVECTOR){30 + i * 40, 50 + i * 30, 70 + i * 20, 0};
            projected[i] = (ProjectedVertex){(i % 2 * 100) | (i / 2 * 100 << 16), 100, 0};
        }
        linked = 0;
        assert(DrawGouraudTexturedQuad(DRAW_FIXED, arena.bytes, ordering, 8191, &sky) ==
               arena.bytes + sizeof(POLY_GT4));
        assert(linked == 1 && packetSlot == ordering + 8191 && arena.gt.code == 0x3c);
        assert(arena.gt.r0 == 30 && arena.gt.g0 == 50 && arena.gt.b0 == 70);
        assert(arena.gt.r1 == 70 && arena.gt.g1 == 80 && arena.gt.b1 == 90);
        assert(arena.gt.r2 == 110 && arena.gt.g2 == 110 && arena.gt.b2 == 110);
        assert(arena.gt.r3 == 150 && arena.gt.g3 == 140 && arena.gt.b3 == 130);
        for (int i = 0; i < 3; i++) {
            triangle.coords[i] = sky.coords[i]; triangle.colour[i] = sky.colour[i];
        }
        linked = 0;
        assert(DrawGouraudTexturedTriangle(DRAW_FIXED, arena.bytes, ordering, 8191, &triangle) ==
               arena.bytes + sizeof(POLY_GT3));
        assert(linked == 1 && packetSlot == ordering + 8191 && ((POLY_GT3 *)arena.bytes)->code == 0x34);
    }
    puts("near scenery keeps PS1 quad geometry and original vertex positions: OK");
    puts("1995 sky keeps Gouraud corner colours in the fixed background slot: OK");
    return 0;
}
