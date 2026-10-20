#include "functions.h"
#include "ghost.h"
#include "dynam.h"

extern TrackData   *track;
extern int          raceType;

extern int droidFlyin;

void ShipTrkReaction(ShipData *playerShip, Object **shipShapes)
{
    VECTOR reaction, targetVelocity, nosePos, facePoint, targetPos, toTarget, direction, speedVector, nextDirection;
    short boost, brake;
    int speed, clearance, altitudeDifference, friction, xDrag, yDrag, zDrag;
    int xDragC, yDragC, zDragC;
    int mass;
    int alpha;
    Face *face;
    TrackSection *current, *neighbour, *next;

    /* boost is not assigned for a negative thrustCount: the original reads it uninitialised */
    if (playerShip->thrustCount == 0) boost = 1;
    else if ((unsigned short)(playerShip->thrustCount - 1) < 2) boost = 3;
    else if (playerShip->thrustCount >= 3) boost = 6;
    playerShip->thrustVector.vx = (playerShip->thrust * (playerShip->unitVecNose.vx * boost)) >> 6;
    playerShip->thrustVector.vy = (playerShip->thrust * (playerShip->unitVecNose.vy * boost)) >> 6;
    playerShip->thrustVector.vz = (playerShip->thrust * (playerShip->unitVecNose.vz * boost)) >> 6;
    speedVector.vx = playerShip->vpivot.vx >> 1;
    speedVector.vy = playerShip->vpivot.vy >> 1;
    speedVector.vz = playerShip->vpivot.vz >> 1;
    speed = GetMagnitude(speedVector) << 1;
    if (playerShip->autopilotCount == 0) playerShip->speed = speed >> 1;
    targetVelocity.vx = sar(speed * playerShip->unitVecNose.vx, 12);
    targetVelocity.vy = sar(speed * playerShip->unitVecNose.vy, 12);
    targetVelocity.vz = sar(speed * playerShip->unitVecNose.vz, 12);
    if (playerShip->nearTrkSect->flags & 1) {
        face = track->faces + playerShip->nearTrkSect->faceStart;
        while (!(face->flags & 1)) {
            face++;
        }
        facePoint.vx = track->vertices[face->vertex[0]].vx;
        facePoint.vy = track->vertices[face->vertex[0]].vy;
        facePoint.vz = track->vertices[face->vertex[0]].vz;
        alpha = PlaneLineCollision(facePoint, playerShip->ppivot, face->normal);
        if (CheckPolygonCollision(face, playerShip->ppivot, alpha, 2) == 0) playerShip->attr |= 16;
    }
    if (!(playerShip->attr & 16)) {
        face = track->faces + playerShip->nearTrkSect->faceStart;
        while (!(face->flags & 1)) {
            face++;
        }
        Ship2Track(playerShip, face, shipShapes);
        if ((face->flags & 32) && (playerShip->attr & 32)) {
            playerShip->vpivot.vx += playerShip->nearTrkSect->nextSection.ptr->centre.vx - playerShip->nearTrkSect->centre.vx;
            playerShip->vpivot.vy += playerShip->nearTrkSect->nextSection.ptr->centre.vy - playerShip->nearTrkSect->centre.vy;
            playerShip->vpivot.vz += playerShip->nearTrkSect->nextSection.ptr->centre.vz - playerShip->nearTrkSect->centre.vz;
        }
        face++;
        if ((face->flags & 32) && !(playerShip->attr & 32)) {
            playerShip->vpivot.vx += playerShip->nearTrkSect->nextSection.ptr->centre.vx - playerShip->nearTrkSect->centre.vx;
            playerShip->vpivot.vy += playerShip->nearTrkSect->nextSection.ptr->centre.vy - playerShip->nearTrkSect->centre.vy;
            playerShip->vpivot.vz += playerShip->nearTrkSect->nextSection.ptr->centre.vz - playerShip->nearTrkSect->centre.vz;
        }
        face--;
        if (!(playerShip->attr & 32)) face++;
        facePoint.vx = track->vertices[face->vertex[0]].vx;
        facePoint.vy = track->vertices[face->vertex[0]].vy;
        facePoint.vz = track->vertices[face->vertex[0]].vz;
        alpha = PlaneLineCollision(facePoint, playerShip->ppivot, face->normal);
        if (alpha < -300) alpha = -300;
        clearance = alpha;
        Ship2TrackBase(playerShip, face, clearance);
        brake = playerShip->r_brake_rot + playerShip->l_brake_rot;
        playerShip->apivot.vx = (targetVelocity.vx - playerShip->vpivot.vx) / (playerShip->skid + (brake >> 2));
        playerShip->apivot.vy = (targetVelocity.vy - playerShip->vpivot.vy) / (playerShip->skid + (brake >> 2));
        playerShip->apivot.vz = (targetVelocity.vz - playerShip->vpivot.vz) / (playerShip->skid + (brake >> 2));
        if (clearance < 75) alpha = 75;
        reaction.vx = (face->normal.vx << 14) / alpha;
        reaction.vy = (face->normal.vy << 14) / alpha;
        reaction.vz = (face->normal.vz << 14) / alpha;
        reaction.vx -= face->normal.vx << 6;
        reaction.vy -= face->normal.vy << 6;
        reaction.vz -= face->normal.vz << 6;
        reaction.vy += 30000;
        mass = playerShip->mass;
        altitudeDifference = playerShip->nearTrkSect->centre.vy - playerShip->ppivot.vy;
        if (altitudeDifference > 704) {
            if (mass == 110) playerShip->ppivot.vy += 80;
            if (playerShip->vpivot.vy < 0) playerShip->vpivot.vy >>= 1;
            playerShip->ppivot.vy += 16;
        }
        reaction.vy += altitudeDifference << 6;
        playerShip->apivot.vx += (reaction.vx + playerShip->thrustVector.vx) / mass;
        playerShip->apivot.vy += (reaction.vy + playerShip->thrustVector.vy) / mass;
        playerShip->apivot.vz += (reaction.vz + playerShip->thrustVector.vz) / mass;
        friction = (playerShip->resistance * (74 - (brake >> 3))) >> 7;
        playerShip->vpivot.vx += playerShip->apivot.vx;
        xDrag = ((volatile VECTOR *)&playerShip->vpivot)->vx; playerShip->vpivot.vx = xDrag - xDrag / friction;
        playerShip->vpivot.vy += playerShip->apivot.vy;
        yDrag = ((volatile VECTOR *)&playerShip->vpivot)->vy; playerShip->vpivot.vy = yDrag - yDrag / friction;
        playerShip->vpivot.vz += ((volatile VECTOR *)&playerShip->apivot)->vz;
        zDrag = ((volatile VECTOR *)&playerShip->vpivot)->vz; playerShip->vpivot.vz = zDrag - zDrag / friction;
        playerShip->ppivot.vx += sar(playerShip->vpivot.vx, 6);
        playerShip->ppivot.vy += sar(playerShip->vpivot.vy, 6);
        playerShip->ppivot.vz += sar(playerShip->vpivot.vz, 6);
        nosePos.vx = (playerShip->unitVecNose.vx >> 5) + playerShip->ppivot.vx;
        nosePos.vy = (playerShip->unitVecNose.vy >> 5) + playerShip->ppivot.vy;
        nosePos.vz = (playerShip->unitVecNose.vz >> 5) + playerShip->ppivot.vz;
        alpha = PlaneLineCollision(facePoint, nosePos, face->normal);
        if (playerShip->rotationCorrectionDisabled == 0) {
            clearance -= alpha;
            if (alpha < 600) { short angle = playerShip->vpitch + 5; playerShip->vpitch = angle + clearance; }
            else playerShip->vpitch -= 50;
            playerShip->vpitch -= playerShip->vpitch >> 2;
            playerShip->hdg += sar((short)sar(playerShip->r_brake_rot - playerShip->l_brake_rot, 3) * playerShip->speed, 15);
            playerShip->vroll += sar(playerShip->vhdg, 5);
            playerShip->vroll -= (playerShip->vroll >> 1);
        }
    } else {
        if (playerShip->vpivot.vy < playerShip->nearTrkSect->centre.vy) {
            face = track->faces + playerShip->nearTrkSect->faceStart;
            while (!(face->flags & 1)) {
                face++;
            }
            Ship2Track(playerShip, face, shipShapes);
        }
        neighbour = playerShip->nearTrkSect->nextSection.ptr;
        GetTargetPos(&targetPos, playerShip->ppivot, neighbour->centre, playerShip->nearTrkSect->centre);
        toTarget.vx = targetPos.vx - playerShip->ppivot.vx;
        toTarget.vy = targetPos.vy - playerShip->ppivot.vy;
        toTarget.vz = targetPos.vz - playerShip->ppivot.vz;
        if (toTarget.vy > 0) toTarget.vy >>= 7;
        else {
            toTarget.vx <<= 3;
            toTarget.vy <<= 3;
            toTarget.vz <<= 3;
        }
        if (GetMagnitude(toTarget) > 32000) {
            playerShip->update = UpdateRescueDroid; playerShip->updateCount = 500; droidFlyin = 1000;
            playerShip->thrust = 0; playerShip->attr |= 0x401;
            if (raceType == 1) recordBuffer->disabled = 1;
        }
        brake = playerShip->r_brake_rot + playerShip->l_brake_rot;
        playerShip->apivot.vx = (targetVelocity.vx - playerShip->vpivot.vx) / ((brake << 2) + 20);
        playerShip->apivot.vy = (targetVelocity.vy - playerShip->vpivot.vy) / ((brake << 2) + 20);
        playerShip->apivot.vz = (targetVelocity.vz - playerShip->vpivot.vz) / ((brake << 2) + 20);
        altitudeDifference = playerShip->nearTrkSect->centre.vy - playerShip->ppivot.vy;
        if (altitudeDifference > 704) {
            /* mass is only assigned in the branch above: the original reads it uninitialised here */
            if (mass == 110) playerShip->ppivot.vy += 80;
            if (playerShip->vpivot.vy < 0) playerShip->vpivot.vy >>= 1;
            playerShip->ppivot.vy += 16;
        }
        playerShip->apivot.vx += playerShip->thrustVector.vx / playerShip->mass;
        playerShip->apivot.vy += (playerShip->thrustVector.vy + 80000) / playerShip->mass;
        playerShip->apivot.vz += playerShip->thrustVector.vz / playerShip->mass;
        friction = (playerShip->resistance * (74 - (brake >> 3))) >> 7;
        playerShip->vpivot.vx += playerShip->apivot.vx;
        xDragC = ((volatile VECTOR *)&playerShip->vpivot)->vx; playerShip->vpivot.vx = xDragC - xDragC / friction;
        playerShip->vpivot.vy += ((volatile VECTOR *)&playerShip->apivot)->vy;
        yDragC = ((volatile VECTOR *)&playerShip->vpivot)->vy; playerShip->vpivot.vy = yDragC - yDragC / friction;
        playerShip->vpivot.vz += ((volatile VECTOR *)&playerShip->apivot)->vz;
        zDragC = ((volatile VECTOR *)&playerShip->vpivot)->vz; playerShip->vpivot.vz = zDragC - zDragC / friction;
        playerShip->ppivot.vx += sar(playerShip->vpivot.vx, 6);
        playerShip->ppivot.vy += sar(playerShip->vpivot.vy, 6);
        playerShip->ppivot.vz += sar(playerShip->vpivot.vz, 6);
        playerShip->vpitch -= 50;
        playerShip->vpitch -= playerShip->vpitch >> 2;
        playerShip->hdg += sar((short)sar(playerShip->r_brake_rot - playerShip->l_brake_rot, 3) * playerShip->speed, 15);
        playerShip->vroll += sar(playerShip->vhdg, 5);
        playerShip->vroll -= sar(playerShip->vroll, 1);
    }
    playerShip->hdg += sar(playerShip->vhdg, 6);
    playerShip->pitch += playerShip->vpitch >> 4;
    playerShip->roll += playerShip->vroll;
    playerShip->roll -= playerShip->roll >> 3;
    playerShip->hdg = ang(playerShip->hdg);
    playerShip->pitch = ang(playerShip->pitch);
    playerShip->roll = ang(playerShip->roll);
    neighbour = playerShip->nearTrkSect->prevSection.ptr;
    current = playerShip->nearTrkSect;
    if (!(playerShip->attr & 8) && !(current->flags & 1) && (neighbour->flags & 1)) {
        playerShip->pitch -= playerShip->vpitch >> 4;
        playerShip->ppivot.vx -= sar(playerShip->vpivot.vx, 6);
        playerShip->ppivot.vy -= sar(playerShip->vpivot.vy, 6);
        playerShip->ppivot.vz -= sar(playerShip->vpivot.vz, 6);
        direction.vx = playerShip->nearTrkSect->nextSection.ptr->centre.vx - playerShip->nearTrkSect->centre.vx;
        direction.vy = playerShip->nearTrkSect->nextSection.ptr->centre.vy - playerShip->nearTrkSect->centre.vy;
        direction.vz = playerShip->nearTrkSect->nextSection.ptr->centre.vz - playerShip->nearTrkSect->centre.vz;
        playerShip->vpivot.vx = direction.vx << 2;
        playerShip->vpivot.vy = direction.vy << 2;
        playerShip->vpivot.vz = direction.vz << 2;
        playerShip->ppivot.vx += sar(playerShip->vpivot.vx, 6);
        playerShip->ppivot.vy += sar(playerShip->vpivot.vy, 6);
        playerShip->ppivot.vz += sar(playerShip->vpivot.vz, 6);
    }
    next = playerShip->nearTrkSect->nextSection.ptr;
    nextDirection.vx = next->centre.vx - playerShip->nearTrkSect->centre.vx;
    nextDirection.vy = next->centre.vy - playerShip->nearTrkSect->centre.vy;
    nextDirection.vz = next->centre.vz - playerShip->nearTrkSect->centre.vz;
    if (nextDirection.vx * playerShip->unitVecNose.vx + nextDirection.vy * playerShip->unitVecNose.vy + nextDirection.vz * playerShip->unitVecNose.vz < 0 && playerShip->ppivot.vy >= playerShip->nearTrkSect->centre.vy + 1000 && (playerShip->attr & 16) && (current->flags & 1) && !(current->prevSection.ptr->flags & 1)) {
        playerShip->update = UpdateRescueDroid; playerShip->updateCount = 500; playerShip->thrust = 0; droidFlyin = 1000; playerShip->attr |= 0x401;
    }
}




void getWorldPoint(VECTOR *output, Object *object, Object *objectCollide, int pointNo)
{
    VECTOR cameraPosition;
    VECTOR worldPosition;
    int   flag;

    cameraPosition.vx = raceCamera.camPos->relative.t[0];
    cameraPosition.vy = raceCamera.camPos->relative.t[1];
    cameraPosition.vz = raceCamera.camPos->relative.t[2];
    worldPosition.vx = upperSkeleton->relative.t[0];
    worldPosition.vy = upperSkeleton->relative.t[1];
    worldPosition.vz = upperSkeleton->relative.t[2];
    SetSkeletonPosition(raceCamera.camPos, 0, 0, 0);
    SetSkeletonDirection(raceCamera.camPos, 0, 0, 0);
    SetSkeletonPosition(upperSkeleton, 0, 0, 0);
    SetSkeletonDirection(upperSkeleton, 0, 0, 0);
    SetSkeletonTransform(object->skeleton);
    raceCamera.camPos->relative.t[0] = cameraPosition.vx;
    raceCamera.camPos->relative.t[1] = cameraPosition.vy;
    raceCamera.camPos->relative.t[2] = cameraPosition.vz;
    upperSkeleton->relative.t[0] = worldPosition.vx;
    upperSkeleton->relative.t[1] = worldPosition.vy;
    upperSkeleton->relative.t[2] = worldPosition.vz;
    RotTrans(objectCollide->vertices + pointNo, output, &flag);
}


void GetTargetPos(VECTOR *result, VECTOR pos, Vector cent1, Vector cent2)
{
    register int a_mag;
    register int b_mag;
    register int c_mag;
    VECTOR       a;
    VECTOR       b;
    VECTOR       c;
    int          a_sq;
    int          b_sq;
    int          c_sq;
    int          cos_alpha;
    int          angle;

    a.vx = pos.vx - cent1.vx;
    a.vy = pos.vy - cent1.vy;
    a.vz = pos.vz - cent1.vz;
    b.vx = cent2.vx - cent1.vx;
    b.vy = cent2.vy - cent1.vy;
    b.vz = cent2.vz - cent1.vz;
    c.vx = pos.vx - cent2.vx;
    c.vy = pos.vy - cent2.vy;
    c.vz = pos.vz - cent2.vz;
    a_mag = GetMagnitude(a);
    b_mag = GetMagnitude(b);
    c_mag = GetMagnitude(c);
    a_sq = a_mag * a_mag;
    b_sq = b_mag * b_mag;
    c_sq = c_mag * c_mag;
    if ((a_mag != 0) && (b_mag != 0)) {
        cos_alpha = (((-b_sq) - (a_sq + c_sq)) << 12) / ((a_mag * b_mag) << 1);
        angle = arccos(cos_alpha) >> 2;
        c_mag = (rcos(angle) * a_mag) >> 12;
        result->vx = cent1.vx - ((b.vx * c_mag) / b_mag);
        result->vy = cent1.vy - ((b.vy * c_mag) / b_mag);
        result->vz = cent1.vz - ((b.vz * c_mag) / b_mag);
    }
}


void SetUnitVectors(ShipData *shipIndex)
{
    short sx;
    short sy;
    short sz;
    short cx;
    short cy;
    short cz;
    int            i;
    VECTOR         direction;
    VECTOR         toFaceVector;
    int            toFace;
    VECTOR         facePoint; /* unused: keeps the stack frame size */
    Face          *facePtr;

    for (i = 0; i < 15; i++) {
        sy = rsin(shipIndex[i].hdg);
        cy = rcos(shipIndex[i].hdg);
        sx = rsin(shipIndex[i].pitch);
        cx = rcos(shipIndex[i].pitch);
        sz = rsin(shipIndex[i].roll);
        cz = rcos(shipIndex[i].roll);
        shipIndex[i].unitVecNose.vx = -((sy * cx) >> 12);
        shipIndex[i].unitVecNose.vy = -sx;
        shipIndex[i].unitVecNose.vz = (cy * cx) >> 12;
        shipIndex[i].unitVecWing.vx = ((cy * cz) >> 12) + ((((sy * sz) >> 12) * sx) >> 12);
        shipIndex[i].unitVecWing.vy = (-(sz * cx)) >> 12;
        shipIndex[i].unitVecWing.vz = ((sy * cz) >> 12) - ((((cy * sx) >> 12) * sz) >> 12);
        OuterProduct12(&shipIndex[i].unitVecNose, &shipIndex[i].unitVecWing, &shipIndex[i].unitVecFin);
        VectorNormal(&shipIndex[i].unitVecFin, &shipIndex[i].unitVecFin);
        shipIndex[i].prevShipSect = shipIndex[i].nearTrkSect;
        SectionSearch(&shipIndex[i]);
        shipIndex[i].prevSecNo = shipIndex[i].prevShipSect->secNo;
        shipIndex[i].nearSecNo = shipIndex[i].nearTrkSect->secNo;

        facePtr = track->faces + shipIndex[i].nearTrkSect->faceStart;
        while ((facePtr->flags & 1) == 0) {
            facePtr++;
        }

        toFaceVector.vx = track->vertices[facePtr->vertex[0]].vx - track->vertices[facePtr->vertex[1]].vx;
        toFaceVector.vy = track->vertices[facePtr->vertex[0]].vy - track->vertices[facePtr->vertex[1]].vy;
        toFaceVector.vz = track->vertices[facePtr->vertex[0]].vz - track->vertices[facePtr->vertex[1]].vz;
        direction.vx = shipIndex[i].nearTrkSect->centre.vx - shipIndex[i].ppivot.vx;
        direction.vy = shipIndex[i].nearTrkSect->centre.vy - shipIndex[i].ppivot.vy;
        direction.vz = shipIndex[i].nearTrkSect->centre.vz - shipIndex[i].ppivot.vz;
        toFace = ((direction.vx * toFaceVector.vx) + (direction.vy * toFaceVector.vy)) + (direction.vz * toFaceVector.vz);
        if (toFace > 0) {
            shipIndex[i].attr |= 0x20;
        } else {
            shipIndex[i].attr &= ~0x20;
        }
    }
}


int SetTrackAttributes(TrackSection *section, short sectionNo, WeaponGrid *weaponGrid, int noWeaponGrids, short *specialCount, SpecialGrid *specialGrid)
{
    Face *facePtr;

    facePtr = track->faces + section->faceStart;
    while (!(facePtr->flags & 1)) {
        facePtr++;
    }

    if (facePtr->flags & 2) {
        if (raceType != 1) {
            if (!tempGameData->weaponsOn) {
                goto test_left;
            }
            goto color_left;
        }
        if (tempGameData->weaponsOn) {
            goto skip_left;
        }
test_left:
        if (raceType != 4) {
            goto skip_left;
        }

color_left:
        {
            facePtr->red = 255;
            facePtr->green = 35;
            facePtr->blue = 117;
        }

skip_left:
        weaponGrid[noWeaponGrids].section = section;
        weaponGrid[noWeaponGrids].count = 0;
        weaponGrid[noWeaponGrids].attr = 2;
        noWeaponGrids++;
    }
    if (facePtr->flags & 32) {
        facePtr->red = 35;
        facePtr->green = 35;
        facePtr->blue = 255;
    }
    if (facePtr->flags & 64) {
        facePtr->red = 128;
        facePtr->green = 0;
        facePtr->blue = 0;
    }
    if (facePtr->flags & 128) {
        specialGrid[*specialCount].count = 10;
        specialGrid[*specialCount].section = section->secNo;
        specialGrid[*specialCount].attr = 0;
        facePtr->red = 255;
        facePtr->green = 255;
        facePtr->blue = 255;
        (*specialCount)++;
    }
    facePtr++;
    if (facePtr->flags & 8) {
        if (raceType != 1) {
            if (!tempGameData->weaponsOn) {
                goto test_right;
            }
            goto color_right;
        }
        if (tempGameData->weaponsOn) {
            goto skip_right;
        }
test_right:
        if (raceType != 4) {
            goto skip_right;
        }

color_right:
        {
            facePtr->red = 84;
            facePtr->green = 238;
            facePtr->blue = 117;
        }

skip_right:
        weaponGrid[noWeaponGrids].section = section;
        weaponGrid[noWeaponGrids].count = 0;
        weaponGrid[noWeaponGrids].attr = 8;
        noWeaponGrids++;
    } else if (facePtr->flags & 32) {
        facePtr->red = 35;
        facePtr->green = 35;
        facePtr->blue = 255;
    }
    if (facePtr->flags & 64) {
        facePtr->red = 128;
        facePtr->green = 0;
        facePtr->blue = 0;
    }
    if (facePtr->flags & 128) {
        facePtr->red = 255;
        facePtr->green = 255;
        facePtr->blue = 255;
    }
    return noWeaponGrids;
}
