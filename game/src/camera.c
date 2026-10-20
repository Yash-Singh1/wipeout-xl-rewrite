#include "functions.h"

/* $gp-relative globals are tentative definitions; the rest are extern. */
short cameraTarget;
int   raceExternalVel;
int   droidViewVel;
extern u_char           medalDisplay;
extern int              raceType;
extern int              ownShip;
extern ShipData        *shipIndex;
extern VECTOR           staticFollowPos;
void InitCamera(TrackCamera *camera, TrackSection *section)
{
    int    i;
    VECTOR pos;

    camera->section = section;
    for (i = 0; i < 2; i++) {
        camera->section = camera->section->prevSection.ptr;
    }
    pos.vx = camera->section->centre.vx;
    pos.vy = camera->section->centre.vy;
    pos.vz = camera->section->centre.vz;
    SetSkeletonPosition(camera->camPos, -pos.vx, -pos.vy, -pos.vz);
    camera->hdg = 0;
    camera->pitch = 0;
    camera->roll = 0;
    camera->vel.vx = 0;
    camera->vel.vy = 0;
    camera->vel.vz = 0;
}


void UpdateCameraRaceExternal(TrackCamera *camera, ShipData *camShip, RescueData *rescueDroid)
{
    VECTOR        pos;
    VECTOR        bestPath;
    VECTOR        distance;
    int           mag;
    short         hdg;
    short         pitch;
    TrackSection *nextSection;

    pos.vx = camShip->ppivot.vx - (camShip->unitVecNose.vx >> 2);
    pos.vy = camShip->ppivot.vy - (camShip->unitVecNose.vy >> 2);
    pos.vz = camShip->ppivot.vz - (camShip->unitVecNose.vz >> 2);
    pos.vy -= 200;

    camera->section = FindNearestSection(camera->section, pos);
    nextSection = camera->section->nextSection.ptr;

    GetTargetPos(&bestPath, pos, nextSection->centre, camera->section->centre);

    distance.vx = pos.vx - bestPath.vx;
    distance.vy = pos.vy - bestPath.vy;
    distance.vz = pos.vz - bestPath.vz;

    camera->vel.vx -= distance.vx >> 6;
    camera->vel.vy -= distance.vy >> 6;
    camera->vel.vz -= distance.vz >> 6;

    camera->vel.vx -= sar(camera->vel.vx, 3);
    camera->vel.vy -= sar(camera->vel.vy, 3);
    camera->vel.vz -= sar(camera->vel.vz, 3);

    pos.vx += camera->vel.vx;
    pos.vy += camera->vel.vy;
    pos.vz += camera->vel.vz;

    mag = GetMagnitude(distance);

    raceExternalVel += mag >> 7;
    raceExternalVel -= raceExternalVel >> 4;
    pos.vy -= raceExternalVel;

    SetSkeletonPosition(camera->camPos, -pos.vx, -pos.vy, -pos.vz);

    camera->pitch = -camShip->pitch;
    camera->roll = 0;
    camera->hdg = -camShip->hdg;
    SetSkeletonDirection(camera->camPos->super, camera->pitch, camera->hdg, 0);

    camera->camPos->super->update = 1;
}


void UpdateCameraRaceInternal(TrackCamera *camera, ShipData *camShip, RescueData *rescueDroid)
{
    VECTOR unitFinVector;

    camera->section = camShip->nearTrkSect;
    SetUnitFinVector(&unitFinVector, camShip);
    SetSkeletonPosition(camera->camPos,
                        (unitFinVector.vx >> 5) - camShip->ppivot.vx,
                        (unitFinVector.vy >> 5) - camShip->ppivot.vy,
                        (unitFinVector.vz >> 5) - camShip->ppivot.vz);
    camera->pitch = -camShip->pitch;
    camera->hdg = -camShip->hdg;
    camera->roll = -camShip->roll;
    SetSkeletonDirectionRPH(camera->camPos->super, -camShip->roll, -camShip->pitch, -camShip->hdg);
    camera->camPos->super->update = 1;
}


/* Fly-in from a few sections ahead of the grid to behind the player's ship. */
void UpdateCameraRaceStart(TrackCamera *camera, ShipData *camShip, RescueData *rescueDroid)
{
    VECTOR        pos;
    VECTOR        startPos;
    VECTOR        unused;
    VECTOR        targetVector;
    short         pitch;
    int           endY;
    int           raisedEndY;
    int           endX;
    int           startY;
    int           xSquared;
    int           horizontalDistance;
    int           endZ;
    int           startX;
    int           startZ;
    int           elapsed;
    int           acceleration;
    int           speed;
    int           distanceX;
    int           distanceZ;
    int           interpolation;
    int           elapsedSquared;
    int           linearTerm;
    int           sectionsAhead;
    int           sectionsAheadLong;
    int           attr;
    TrackSection *section;

    section = camShip->nearTrkSect->nextSection.ptr;
    speed = 2;
    acceleration = -5;
    if (raceType == 1) {
        sectionsAhead = 2;
        do {
            section = section->nextSection.ptr;
        } while (--sectionsAhead >= 0);
    } else {
        sectionsAheadLong = 8;
        do {
            section = section->nextSection.ptr;
            sectionsAheadLong -= 1;
        } while (sectionsAheadLong >= 0);
    }
    endX = section->centre.vx;
    pos.vx = endX;
    endY = section->centre.vy;
    pos.vy = endY;
    endZ = section->centre.vz;
    raisedEndY = endY - 800;
    pos.vy = raisedEndY;
    pos.vz = endZ;
    startX = camShip->nearTrkSect->centre.vx;
    startPos.vx = startX;
    startY = camShip->ppivot.vy - 400;
    startPos.vy = startY;
    startZ = camShip->nearTrkSect->centre.vz;
    startPos.vz = startZ;

    elapsed = 200 - camShip->updateCount;
    interpolation = (speed * elapsed << 8) + ((elapsed * elapsed * acceleration) >> 1);
    interpolation = 0x6400 - interpolation;
    pos.vx = startX - ((((startX - endX) * interpolation) / 100) >> 8);
    pos.vy = startY - ((((startY - raisedEndY) * interpolation) / 100) >> 8);
    pos.vz = startZ - ((((startZ - endZ) * interpolation) / 100) >> 8);

    camera->section = FindNearestSection(camera->section, pos);

    distanceX = camShip->ppivot.vx - pos.vx;
    xSquared = distanceX * distanceX;
    targetVector.vx = distanceX;
    targetVector.vy = camShip->ppivot.vy - pos.vy;
    distanceZ = camShip->ppivot.vz - pos.vz;
    targetVector.vz = distanceZ;
    horizontalDistance = SquareRoot0(xSquared + (distanceZ * distanceZ));

    SetSkeletonPosition(camera->camPos, -pos.vx, -pos.vy, -pos.vz);
    camera->hdg = ratan2(targetVector.vx, targetVector.vz);
    pitch = ratan2(targetVector.vy, horizontalDistance);
    camera->pitch = pitch;
    SetSkeletonDirection(camera->camPos->super, pitch, camera->hdg, 0);
    camera->camPos->super->update = 1;

    if (camShip->updateCount == 100) {
        if (tempGameData->internalView != 0) {
            CameraUpdate = UpdateCameraRaceInternal;
            camShip->attr |= VIEW_IN;
        } else {
            CameraUpdate = UpdateCameraRaceExternal;
            camShip->attr &= ~VIEW_IN;
        }
    }
}


void UpdateCameraAttractCircle(TrackCamera *camera, ShipData *camShip, RescueData *rescueDroid)
{
    VECTOR pos;
    int    hdg, pitch, temp;
    VECTOR targetVector;
    short  sx, sy, sz;
    short  cx, cy, cz;
    VECTOR unitFinVector;

    camera->section = camShip->nearTrkSect;

    if (cameraTarget == 0) {
        cameraTarget = 150;
    }
    cameraTarget--;
    if (medalDisplay) {
        cameraTarget = 0;
    }
    if (cameraTarget == 0) {
        CameraUpdate = UpdateCameraAttractMode;
    }

    SetUnitFinVector(&unitFinVector, camShip);

    pos.vx = camShip->ppivot.vx + (rsin(camShip->hdg) / 6);
    pos.vy = camShip->ppivot.vy + ((camShip->pitch >> 2) - 200);
    pos.vz = camShip->ppivot.vz - (rcos(camShip->hdg) / 6);

    pos.vx += rsin(cameraTarget << 3) >> 3;
    pos.vy -= 200;
    pos.vz += rcos(cameraTarget << 3) >> 3;

    pos.vx -= unitFinVector.vx >> 4;
    pos.vy -= unitFinVector.vy >> 4;
    pos.vz -= unitFinVector.vz >> 4;

    targetVector.vx = camShip->ppivot.vx - pos.vx;
    targetVector.vy = camShip->ppivot.vy - pos.vy;
    targetVector.vz = camShip->ppivot.vz - pos.vz;

    temp = SquareRoot0(targetVector.vx * targetVector.vx + targetVector.vz * targetVector.vz);

    SetSkeletonPosition(camera->camPos, -pos.vx, -pos.vy, -pos.vz);

    camera->hdg = ratan2(targetVector.vx, targetVector.vz);
    camera->pitch = ratan2(targetVector.vy, temp);
    SetSkeletonDirection(camera->camPos->super, camera->pitch, camera->hdg, 0);

    camera->camPos->super->update = 1;
}


void UpdateCameraAttractMedal(TrackCamera *camera, ShipData *camShip, RescueData *rescueDroid)
{
    VECTOR pos;
    int    hdg, pitch, temp;
    VECTOR targetVector;
    short  sx, sy, sz;
    short  cx, cy, cz;
    VECTOR unitFinVector;

    camera->section = camShip->nearTrkSect;

    if (cameraTarget == 0) {
        cameraTarget = 150;
    }
    cameraTarget--;
    if (cameraTarget == 0) {
        CameraUpdate = UpdateCameraAttractMode;
    }

    SetUnitFinVector(&unitFinVector, camShip);

    pos.vx = camShip->ppivot.vx + (rsin(camShip->hdg) >> 5);
    pos.vy = camShip->ppivot.vy + 0;
    pos.vz = camShip->ppivot.vz - (rcos(camShip->hdg) >> 5);

    pos.vx += rsin(cameraTarget << 3) >> 5;
    pos.vy -= 1024;
    pos.vz += rcos(cameraTarget << 3) >> 5;

    targetVector.vx = camShip->ppivot.vx - pos.vx;
    targetVector.vy = camShip->ppivot.vy - pos.vy;
    targetVector.vz = camShip->ppivot.vz - pos.vz;

    temp = SquareRoot0(targetVector.vx * targetVector.vx + targetVector.vz * targetVector.vz);

    SetSkeletonPosition(camera->camPos, -pos.vx, -pos.vy, -pos.vz);

    camera->hdg = ratan2(targetVector.vx, targetVector.vz);
    camera->pitch = ratan2(targetVector.vy, temp);
    SetSkeletonDirection(camera->camPos->super, camera->pitch, camera->hdg, 0);

    camera->camPos->super->update = 1;
}


void UpdateCameraAttractDestroyed(TrackCamera *camera, ShipData *camShip, RescueData *rescueDroid)
{
    VECTOR pos;
    int    hdg, pitch, temp;
    VECTOR targetVector;
    short  sx, sy, sz;
    short  cx, cy, cz;
    VECTOR unitFinVector;

    shipIndex[ownShip].attr &= ~VIEW_IN;
    camera->section = camShip->nearTrkSect;

    if (cameraTarget == 0) {
        cameraTarget = 32000;
    }
    cameraTarget -= 2;
    if (medalDisplay) {
        cameraTarget = 0;
    }
    if (cameraTarget <= 0) {
        cameraTarget = 32000;
    }

    pos.vx = camShip->ppivot.vx + (rsin(camShip->hdg) / 6);
    pos.vy = camShip->ppivot.vy + 0;
    pos.vz = camShip->ppivot.vz - (rcos(camShip->hdg) / 6);

    pos.vx += rsin(cameraTarget << 3) >> 2;
    pos.vy -= 800;
    pos.vz += rcos(cameraTarget << 3) >> 2;

    targetVector.vx = camShip->ppivot.vx - pos.vx;
    targetVector.vy = camShip->ppivot.vy - pos.vy;
    targetVector.vz = camShip->ppivot.vz - pos.vz;

    temp = SquareRoot0(targetVector.vx * targetVector.vx + targetVector.vz * targetVector.vz);

    SetSkeletonPosition(camera->camPos, -pos.vx, -pos.vy, -pos.vz);

    camera->hdg = ratan2(targetVector.vx, targetVector.vz);
    camera->pitch = ratan2(targetVector.vy, temp);
    SetSkeletonDirection(camera->camPos->super, camera->pitch, camera->hdg, 0);

    camera->camPos->super->update = 1;
}


/* Looks back at the ship from in front of it (used while the rescue droid carries it). */
void UpdateCameraDroidView(TrackCamera *camera, ShipData *camShip, RescueData *rescueDroid)
{
    VECTOR        pos;
    VECTOR        bestPath;
    VECTOR        distance;
    int           mag;
    short         hdg;
    short         pitch;
    TrackSection *prevSection;

    pos.vx = camShip->ppivot.vx + (camShip->unitVecNose.vx >> 2);
    pos.vy = camShip->ppivot.vy + (camShip->unitVecNose.vy >> 2);
    pos.vz = camShip->ppivot.vz + (camShip->unitVecNose.vz >> 2);
    pos.vy -= 200;

    camera->section = FindNearestSection(camera->section, pos);
    prevSection = camera->section->prevSection.ptr;

    GetTargetPos(&bestPath, pos, prevSection->centre, camera->section->centre);

    distance.vx = pos.vx - bestPath.vx;
    distance.vy = pos.vy - bestPath.vy;
    distance.vz = pos.vz - bestPath.vz;

    camera->vel.vx -= distance.vx >> 6;
    camera->vel.vy -= distance.vy >> 6;
    camera->vel.vz -= distance.vz >> 6;

    camera->vel.vx -= sar(camera->vel.vx, 3);
    camera->vel.vy -= sar(camera->vel.vy, 3);
    camera->vel.vz -= sar(camera->vel.vz, 3);

    pos.vx += camera->vel.vx;
    pos.vy += camera->vel.vy;
    pos.vz += camera->vel.vz;

    mag = GetMagnitude(distance);

    droidViewVel += mag >> 7;
    droidViewVel -= droidViewVel >> 4;
    pos.vy -= droidViewVel;

    SetSkeletonPosition(camera->camPos, -pos.vx, -pos.vy, -pos.vz);

    camera->pitch = -camShip->pitch;
    camera->roll = 0;
    camera->hdg = -(camShip->hdg + 2048);
    SetSkeletonDirection(camera->camPos->super, camera->pitch, camera->hdg, 0);

    camera->camPos->super->update = 1;
}


void UpdateCameraAttractInternal(TrackCamera *camera, ShipData *camShip, RescueData *rescueDroid)
{
    VECTOR unitFinVector;

    if (cameraTarget == 0) {
        cameraTarget = 150;
    }
    cameraTarget--;
    if (medalDisplay != 0) {
        cameraTarget = 0;
    }
    if (cameraTarget == 0) {
        CameraUpdate = UpdateCameraAttractMode;
    }
    camera->section = camShip->nearTrkSect;
    SetUnitFinVector(&unitFinVector, camShip);
    SetSkeletonPosition(camera->camPos,
                        (unitFinVector.vx >> 5) - camShip->ppivot.vx,
                        (unitFinVector.vy >> 5) - camShip->ppivot.vy,
                        (unitFinVector.vz >> 5) - camShip->ppivot.vz);
    camera->pitch = -camShip->pitch;
    camera->hdg = -camShip->hdg;
    SetSkeletonDirectionRPH(camera->camPos->super, -camShip->roll, -camShip->pitch, -camShip->hdg);
    camera->camPos->super->update = 1;
}


/* Fixed camera ten sections ahead that watches the ship go past. */
void UpdateCameraStaticFollow(TrackCamera *camera, ShipData *camShip, RescueData *rescueDroid)
{
    int           temp;
    VECTOR        targetVector;
    TrackSection *section;
    int           i;

    if (cameraTarget == 0) {
        section = camShip->nearTrkSect->nextSection.ptr;
        for (i = 0; i < 10; i++) {
            section = section->nextSection.ptr;
        }
        camera->section = section;
        staticFollowPos.vx = section->centre.vx;
        staticFollowPos.vy = section->centre.vy - 500;
        staticFollowPos.vz = section->centre.vz;
        cameraTarget = 150;
    }
    cameraTarget--;
    if (medalDisplay != 0) {
        cameraTarget = 0;
    }
    if (cameraTarget == 0) {
        CameraUpdate = UpdateCameraAttractMode;
    }
    targetVector.vx = camShip->ppivot.vx - staticFollowPos.vx;
    targetVector.vy = camShip->ppivot.vy - staticFollowPos.vy;
    targetVector.vz = camShip->ppivot.vz - staticFollowPos.vz;
    temp = SquareRoot0(targetVector.vx * targetVector.vx + targetVector.vz * targetVector.vz);
    SetSkeletonPosition(camera->camPos, -staticFollowPos.vx, -staticFollowPos.vy, -staticFollowPos.vz);
    camera->hdg = ratan2(targetVector.vx, targetVector.vz);
    camera->pitch = ratan2(targetVector.vy, temp);
    SetSkeletonDirection(camera->camPos->super, camera->pitch, camera->hdg, 0);
    camera->camPos->super->update = 1;
}


/* Picks the next attract-mode camera at random. */
void UpdateCameraAttractMode(TrackCamera *camera, ShipData *camShip, RescueData *rescueDroid)
{
    cameraTarget = 0;
    if (shipIndex[ownShip].combatAttr & DESTROYED) {
        CameraUpdate = UpdateCameraAttractDestroyed;
        camShip->attr &= ~VIEW_IN;
    } else {
        switch (newRand() % 2) {
        case 0:
            CameraUpdate = UpdateCameraAttractCircle;
            camShip->attr &= ~VIEW_IN;
            break;
        case 1:
            CameraUpdate = UpdateCameraStaticFollow;
            camShip->attr &= ~VIEW_IN;
            break;
        }
    }
    if (medalDisplay != 0) {
        camShip->attr &= ~VIEW_IN;
        CameraUpdate = UpdateCameraAttractMedal;
        medalDisplay = 0;
    }
    CameraUpdate(camera, camShip, rescueDroid);
}


/* Unit vector along the ship's fin (its local up axis). */
void SetUnitFinVector(VECTOR *unitFinVector, ShipData *camShip)
{
    short sx, sy, sz;
    short cx, cy, cz;

    sy = rsin(camShip->hdg);
    sx = rsin(camShip->pitch);
    sz = rsin(camShip->roll);
    cy = rcos(camShip->hdg);
    cx = rcos(camShip->pitch);
    cz = rcos(camShip->roll);

    unitFinVector->vx = ((cy * sz) >> 12) - ((((sy * sx) >> 12) * cz) >> 12);
    unitFinVector->vy = (cx * cz) >> 12;
    unitFinVector->vz = ((sy * sz) >> 12) + ((((cy * sx) >> 12) * cz) >> 12);
}
