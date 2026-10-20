#include "functions.h"

/* Finds the section nearest the ship, stores it in nearTrkSect and sets FLYING
   when the ship is more than 3700 units above the track. Returns the distance. */
int SectionSearch(ShipData *playerShip)
{
    TrackSection *trk;
    TrackSection *nearestSection;
    TrackSection *junc;
    VECTOR        centreToShip;
    char          workspace[40];
    int           smallestMag;
    int           mag;
    int           i;
    short         juncChecked = 0;
    short         sectionFlags;

    trk = playerShip->nearTrkSect;
    for (i = 0; i < 3; i++) {
        trk = trk->prevSection.ptr;
    }
    sectionFlags = trk->flags;
    centreToShip.vx = playerShip->ppivot.vx - trk->centre.vx;
    centreToShip.vy = (playerShip->ppivot.vy - trk->centre.vy) >> 2;
    centreToShip.vz = playerShip->ppivot.vz - trk->centre.vz;
    smallestMag = centreToShip.vx * centreToShip.vx + centreToShip.vy * centreToShip.vy + centreToShip.vz * centreToShip.vz;
    if (trk->junction.index != -1) {
        junc = trk->junction.ptr;
        juncChecked = 1;
    }
    nearestSection = trk;
    for (i = 0; i < 6; i++) {
        trk = trk->nextSection.ptr;
        if (trk->junction.index != -1) {
            junc = trk->junction.ptr;
            juncChecked = 1;
        }
        centreToShip.vx = playerShip->ppivot.vx - trk->centre.vx;
        centreToShip.vy = (playerShip->ppivot.vy - trk->centre.vy) >> 2;
        centreToShip.vz = playerShip->ppivot.vz - trk->centre.vz;
        mag = centreToShip.vx * centreToShip.vx + centreToShip.vy * centreToShip.vy + centreToShip.vz * centreToShip.vz;
        if (mag < smallestMag) {
            smallestMag = mag;
            nearestSection = trk;
        }
    }
    if (juncChecked == 1) {
        if (junc->flags & 0x10) {
            trk = junc;
        } else {
            trk = junc;
            if (sectionFlags == 0x20) {
                trk = trk->nextSection.ptr;
            }
        }
        for (i = 0; i < 6; i++) {
            centreToShip.vx = playerShip->ppivot.vx - trk->centre.vx;
            centreToShip.vy = (playerShip->ppivot.vy - trk->centre.vy) >> 2;
            centreToShip.vz = playerShip->ppivot.vz - trk->centre.vz;
            mag = centreToShip.vx * centreToShip.vx + centreToShip.vy * centreToShip.vy + centreToShip.vz * centreToShip.vz;
            if (mag < smallestMag) {
                smallestMag = mag;
                nearestSection = trk;
            }
            if (junc->flags & 0x8) {
                trk = trk->prevSection.ptr;
            } else {
                trk = trk->nextSection.ptr;
            }
        }
    }
    smallestMag = SquareRoot0(smallestMag);
    if (smallestMag > 3700) {
        playerShip->attr |= FLYING;
    } else {
        playerShip->attr &= ~FLYING;
    }
    playerShip->nearTrkSect = nearestSection;
    return smallestMag;
}


TrackSection *FindNearestSection(TrackSection *trk, VECTOR pos)
{
    TrackSection *nearestSection;
    TrackSection *junc;
    VECTOR        centreToPos;
    char          workspace[40];
    int           smallestMag;
    int           mag;
    int           i;
    short         juncChecked = 0;
    short         sectionFlags;

    for (i = 0; i < 3; i++) {
        trk = trk->prevSection.ptr;
    }
    sectionFlags = trk->flags;
    centreToPos.vx = pos.vx - trk->centre.vx;
    centreToPos.vy = pos.vy - trk->centre.vy;
    centreToPos.vz = pos.vz - trk->centre.vz;
    smallestMag = centreToPos.vx * centreToPos.vx + centreToPos.vy * centreToPos.vy + centreToPos.vz * centreToPos.vz;
    nearestSection = trk;
    if (trk->junction.index != -1) {
        junc = trk->junction.ptr;
        juncChecked = 1;
    }
    for (i = 0; i < 6; i++) {
        trk = trk->nextSection.ptr;
        if (trk->junction.index != -1) {
            junc = trk->junction.ptr;
            juncChecked = 1;
        }
        centreToPos.vx = pos.vx - trk->centre.vx;
        centreToPos.vy = pos.vy - trk->centre.vy;
        centreToPos.vz = pos.vz - trk->centre.vz;
        mag = centreToPos.vx * centreToPos.vx + centreToPos.vy * centreToPos.vy + centreToPos.vz * centreToPos.vz;
        if (mag < smallestMag) {
            smallestMag = mag;
            nearestSection = trk;
        }
    }
    if (juncChecked == 1) {
        if (junc->flags & 0x10) {
            trk = junc;
        } else {
            trk = junc;
            if (sectionFlags == 0x20) {
                trk = trk->nextSection.ptr;
            }
        }
        for (i = 0; i < 6; i++) {
            centreToPos.vx = pos.vx - trk->centre.vx;
            centreToPos.vy = pos.vy - trk->centre.vy;
            centreToPos.vz = pos.vz - trk->centre.vz;
            mag = centreToPos.vx * centreToPos.vx + centreToPos.vy * centreToPos.vy + centreToPos.vz * centreToPos.vz;
            if (mag < smallestMag) {
                smallestMag = mag;
                nearestSection = trk;
            }
            if (junc->flags & 0x8) {
                trk = trk->prevSection.ptr;
            } else {
                trk = trk->nextSection.ptr;
            }
        }
    }
    return nearestSection;
}
