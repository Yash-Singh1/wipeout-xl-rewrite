/* Synthetic road, tunnel roof and board geometry. No disc data needed. */
#include "../../game/src/portorig.c"
#include <assert.h>
static int guest64;
int PortOriginalTrackIs64(void) { return guest64; }
int main(void)
{
    VECTOR points[4] = {{-1000, -2400, -1000}, {1000, -2400, -1000},
                        {1000, -2400, 1000}, {-1000, -2400, 1000}};
    SVECTOR roofVertices[4] = {{-1000, 0, -1000}, {1000, 0, -1000},
                              {1000, 0, 1000}, {-1000, 0, 1000}};
    SVECTOR boardVertices[2] = {{0, -400, 0}, {0, 400, 0}};
    F4 polygon = {TypeF4, 0, {0, 1, 2, 3}};
    Skeleton skeleton = {0};
    TrackSection section = {0};
    Object board = {0}, roof = {0};

    skeleton.relative.m[0][0] = skeleton.relative.m[1][1] = skeleton.relative.m[2][2] = 4096;
    skeleton.relative.t[1] = -2400;
    roof.vertices = roofVertices;
    roof.vertexCount = 4;
    roof.primitiveCount = 1;
    roof.primitives = (Primitive *)&polygon;
    roof.skeleton = &skeleton;
    guestScene = &roof;
    board.vertices = boardVertices;
    board.vertexCount = 2;
    assert(PortOriginalDroidY(&board, &section) == -3000);
    guest64 = 1;
    assert(PortOriginalDroidY(&board, &section) == -1872);
    assert(PortOriginalDroidY(&board, &section) == -1872); /* cached */
    section.centre.vx = 2000;
    assert(BoardBelowRoof(-3000, -400, &section, points, 4) == -3000);
    section.centre.vx = 0;
    assert(BoardBelowRoof(-3000, -400, &section, points, 4) == -1872);
    boardVertices[0].vy = -600;
    assert(PortOriginalDroidY(&board, &section) == -1672); /* a taller board */
    boardCount = 0;
    skeleton.relative.t[0] = 10000;
    assert(PortOriginalDroidY(&board, &section) == -3000); /* roof moved away */
    points[0].vy = 1000;
    assert(BoardBelowRoof(-3000, -400, &section, points, 4) == -3000);
    puts("checkpoint clearance: OK");
    return 0;
}
