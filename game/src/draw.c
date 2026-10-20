#include "functions.h"
#include "track.h"
#define TrackVersion 8
extern Object     **trackObjects;
extern int          raceType;
extern int         clipNearTable1[8];
extern int         clipFarTable1[8];
extern int         clipNearTable2[8];
extern int         clipFarTable2[8];
extern short        visibleObjects[128];

char TrackDirection(TrackCamera *camera)
{
    SVECTOR      reference;
    SVECTOR      toCentre;
    int         angle;
    int         forwardAngle;
    SVECTOR      forwards;
    TrackSection *nextSection;
    TrackSection *startSection;

    nextSection = camera->section->nextSection.ptr;

    reference.vx = (short)(nextSection->centre.vx - camera->section->centre.vx);
    reference.vy = (short)(nextSection->centre.vy - camera->section->centre.vy);
    reference.vz = (short)(nextSection->centre.vz - camera->section->centre.vz);

    angle = (-ratan2(reference.vx, reference.vz)) + camera->hdg;
    angle = ang(angle);

    if ((angle >= -512) && (angle < 512)) {
        return 1;
    } else if ((angle >= 512) && (angle < 1536)) {
        return 4;
    } else if ((angle >= 1536) || (angle < -1536)) {
        return 2;
    } else {
        return 8;
    }
}


void DrawLandscape(ShipData *playerShip, TrackCamera *camera)
{
    short         i;
    VECTOR        result;
    SVECTOR       centre;
    int          flag;
    TrackSection *section;
    Object       *object;
    int          extent;
    int          minDistance;
    int          maxDistance;
    int          roll = 0;
    int          farZ;
    int          nearZ;

    if (raceType != 2) {
        minDistance = clipNearTable1[clipLevel];
        maxDistance = clipFarTable1[clipLevel];
    } else {
        minDistance = clipNearTable2[clipLevel];
        maxDistance = clipFarTable2[clipLevel];
    }

    for (i = 0; i < 128; i++) {
        visibleObjects[i] = 0;
    }

    section = camera->section;
    if (section->objectCount > 0) {
        centre.vx = 0;
        centre.vy = 0;
        centre.vz = 0;
        if (playerShip->attr & VIEW_IN) {
            roll = playerShip->roll;
        }
        for (i = section->objectCount - 1; i >= 0; i--) {
            object = trackObjects[section->objects[i]];
            object->skeleton->update = 1;
            SetSkeletonTransform(object->skeleton);
            extent = object->extent;
            RotTrans(&centre, &result, &flag);
            farZ = result.vz + extent;
            nearZ = result.vz - extent;
            result.vz = farZ;
            if ((farZ > minDistance) && (nearZ < maxDistance)) {
                result.vx = (result.vx * 160) / result.vz;
                result.vy = (result.vy * 160) / result.vz;
                extent = (extent * 160) / result.vz;
                if ((result.vx >= -(extent + 160)) && (result.vx <= (extent + 160)) &&
                    (result.vy >= -(extent + 120)) && (result.vy <= (extent + 120))) {
                    AsmDrawObject(object, camera->camPos, roll);
                }
            }
        }
    }
    waveIndex++;
}
