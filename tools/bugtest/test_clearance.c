/* Synthetic road, tunnel roof and board geometry. No disc data needed. */
#include "../../game/src/portorig.c"
#include <assert.h>
int portPal;
static int guest64, guestTrack = -1;
int PortOriginalTrack(void) { return guestTrack; }
int PortOriginalTrackIs64(void) { return guest64; }
int main(void)
{
    VECTOR points[4] = {{-1000, -2400, -1000}, {1000, -2400, -1000},
                        {1000, -2400, 1000}, {-1000, -2400, 1000}};
    SVECTOR roofVertices[4] = {{-1000, 0, -1000}, {1000, 0, -1000},
                              {-1000, 0, 1000}, {1000, 0, 1000}};
    SVECTOR boardVertices[2] = {{0, -400, 0}, {0, 400, 0}};
    F4 polygon = {TypeF4, 0, {0, 1, 2, 3}};
    Skeleton skeleton = {0};
    TrackSection section = {0};
    Object board = {0}, roof = {0};
    Skeleton boardSkeleton = {0};

    skeleton.relative.m[0][0] = skeleton.relative.m[1][1] = skeleton.relative.m[2][2] = 4096;
    skeleton.relative.t[1] = -2400;
    roof.vertices = roofVertices;
    roof.vertexCount = 4;
    roof.primitiveCount = 1;
    roof.primitives = (Primitive *)&polygon;
    roof.skeleton = &skeleton;
    guestScene = &roof;
    board.skeleton = &boardSkeleton;
    board.vertices = boardVertices;
    board.vertexCount = 2;
    assert(PortOriginalFitDroid(&board, &section) == -3000);
    guest64 = 1;
    assert(PortOriginalFitDroid(&board, &section) == -1872);
    assert(PortOriginalFitDroid(&board, &section) == -1872); /* cached */
    section.centre.vx = 2000;
    assert(BoardBelowRoof(-3000, -400, &section, points, 4) == -3000);
    section.centre.vx = 0;
    assert(BoardBelowRoof(-3000, -400, &section, points, 4) == -1872);
    boardVertices[0].vy = -600;
    assert(PortOriginalFitDroid(&board, &section) == -1672); /* a taller board */
    boardCount = 0;
    skeleton.relative.t[0] = 10000;
    assert(PortOriginalFitDroid(&board, &section) == -3000); /* roof moved away */
    points[0].vy = 1000;
    assert(BoardBelowRoof(-3000, -400, &section, points, 4) == -3000);
    /* A low roof must not push the board through the track. */
    boardCount = 0;
    skeleton.relative.t[0] = 0;
    skeleton.relative.t[1] = -1500;
    boardVertices[0].vy = -1200;
    boardVertices[1].vy = 1200;
    boardSkeleton.relative.m[0][0] = boardSkeleton.relative.m[1][1] = boardSkeleton.relative.m[2][2] = 4096;
    {
        int y = PortOriginalFitDroid(&board, &section);
        int scale = boardSkeleton.relative.m[1][1];
        assert(scale > 0 && scale < 4096);
        assert(y - 1200 * scale / 4096 >= -1500 + 128);
        assert(y + 1200 * scale / 4096 <= -512);
        /* Reset rotation as DrawDroid does. The cached fit must be identical. */
        boardSkeleton.relative.m[0][0] = boardSkeleton.relative.m[1][1] = boardSkeleton.relative.m[2][2] = 4096;
        assert(PortOriginalFitDroid(&board, &section) == y);
        assert(boardSkeleton.relative.m[1][1] == scale);
    }
    {
        ShipData ship = {0};
        ship.maxThrust = 1750;
        PortOriginalShipPhysics(&ship, 6, 2);
        assert(ship.maxThrust == 1750); /* XL */
        guestTrack = 1;
        PortOriginalShipPhysics(&ship, 6, 2);
        assert(ship.maxThrust == 1750); /* W64 */
        guest64 = 0;
        PortOriginalShipPhysics(&ship, 6, 2);
        assert(ship.maxThrust == 1400);
        assert(ship.mass == 150 && ship.resistance == 130 && ship.skid == 16);
        assert(ship.headingInc == 140 && ship.maxHeading == 1920);
        ship.maxThrust = 1750; portPal = 1;
        PortOriginalShipPhysics(&ship, 8, 2);
        assert(ship.headingInc == 168 && ship.maxHeading == 2304);
        portPal = 0;
        ship.maxThrust = 1750;
        PortOriginalShipPhysics(&ship, 6, 1);
        assert(ship.maxThrust == 1750); /* Venom */
        PortOriginalShipPhysics(&ship, 3, 2);
        assert(ship.maxThrust == 1750); /* Auricom */
    }
    puts("checkpoint clearance and guest thrust: OK");
    return 0;
}
