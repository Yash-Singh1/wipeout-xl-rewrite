/* Port: interventions of the console recorder's test scenarios, for the
   lockstep tests (platform/trace.c applies them at UpdateShips). Not part of
   the game. */
#include "functions.h"
#include "../../platform/trace.h"

extern TrackData *track;
extern ShipData *shipIndex;
extern int ownShip;

/* The player's ship record (native layout), for diagnostics. */
const void *PortTestPlayerShip(int *size)
{
    *size = sizeof(ShipData);
    return shipIndex && ownShip >= 0 && ownShip < 15 ? &shipIndex[ownShip] : NULL;
}

/* The player's ship for the trace's state event: position (x, y, z),
   section number and lap, as the console recorder reads them. */
int PortTestPlayerState(int *x, int *y, int *z)
{
    ShipData *ship;

    if (!shipIndex || ownShip < 0 || ownShip >= 15) {
        return 0;
    }
    ship = &shipIndex[ownShip];
    *x = ship->ppivot.vx;
    *y = ship->ppivot.vy;
    *z = ship->ppivot.vz;
    return 1;
}

/* "wall": move the player's ship value/4096 of its wing vector sideways and
   give it back to the player for one update, so it meets the wall. */
void PortTestWall(void *player, int value)
{
    ShipData *ship = player;

    ship->ppivot.vx += (int)(((int64_t)ship->unitVecWing.vx * value) / 4096);
    ship->ppivot.vy += (int)(((int64_t)ship->unitVecWing.vy * value) / 4096);
    ship->ppivot.vz += (int)(((int64_t)ship->unitVecWing.vz * value) / 4096);
    ship->autopilotCount = 1;
    ship->update = UpdatePlayerShip;
}

/* "pit": put the player's ship, stopped, over the first section whose first
   face is not a pit face and whose second is (the red recharge stage). */
int PortTestPit(void *player)
{
    ShipData *ship = player;
    int i;

    for (i = 0; i < track->sectionCount; i++) {
        TrackSection *section = &track->sections[i];
        Face *face = &track->faces[section->faceStart];

        if ((face[0].flags & 0x40) || !(face[1].flags & 0x40)) {
            continue;
        }
        ship->nearTrkSect = section;
        ship->prevShipSect = section;
        ship->ppivot.vx = section->centre.vx;
        ship->ppivot.vy = section->centre.vy - 200;
        ship->ppivot.vz = section->centre.vz;
        ship->vpivot.vx = 0;
        ship->vpivot.vy = 0;
        ship->vpivot.vz = 0;
        ship->r_brake_rot = 0;
        ship->nearSecNo = section->secNo;
        return 1;
    }
    return 0;
}
