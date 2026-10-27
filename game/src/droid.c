#include "functions.h"
#include "track.h"
#include "droid.h"

extern TrackData    *track;
extern VECTOR        droidTarget;

extern int droidFlyin;


void UpdateRescueDroid(ShipData *ships, Object **unused1, int shipNo, WeaponData *unused3, ControlInput *control)
{
    ShipData     *ship;
    TrackSection *section;
    TrackSection *next;
    VECTOR        nose;
    VECTOR        distance;
    short         maxThrust;
    short         throttle;
    int           target;
    short         heading;
    int           i;

    ship = &ships[shipNo];
    ship->attr |= FLYING;
    if (control->buttons & CONTROL_ANALOG_THRUST) {
        maxThrust = ship->maxThrust;
        throttle = control->thrust;
        target = (maxThrust * throttle) / 200;
        if (maxThrust < target) {
            target = maxThrust;
        }
        if (throttle > 0) {
            short thrust = ship->thrust;
            if (target >= thrust) {
                int diff = target - thrust;
                int step = PAL_SPEED(16);
                if (diff < PAL_SPEED(16)) {
                    step = diff;
                }
                ship->thrust = thrust + step;
            } else {
                ship->thrust = thrust - PAL_SPEED(16);
            }
        } else {
            short thrust = ship->thrust;
            if (thrust >= PAL_SPEED(16)) {
                ship->thrust = thrust - PAL_SPEED(16);
            }
        }
    } else {
        if (control->buttons & CONTROL_THRUST) {
            ship->thrust += PAL_SPEED(16);
        } else if (ship->thrust >= PAL_SPEED(16)) {
            ship->thrust -= PAL_SPEED(16);
        }
        if (ship->maxThrust < ship->thrust) {
            ship->thrust = ship->maxThrust;
        }
    }

    if (ship->updateCount == 0x1F4) {
        section = ship->nearTrkSect->prevSection.ptr;
        section = section->prevSection.ptr;
        section = section->prevSection.ptr;
        section = section->prevSection.ptr;
        i = 0;
        do {
            section = section->nextSection.ptr;
            i++;
        } while (!(section->flags & TRACK_JUMP) && i < 10);
        ship->nearTrkSect = section;
        next = section->nextSection.ptr;
        droidTarget.vx = (section->centre.vx + next->centre.vx) >> 1;
        droidTarget.vy = ((section->centre.vy + next->centre.vy) >> 1) - 0x7D0;
        droidTarget.vz = (section->centre.vz + next->centre.vz) >> 1;
        ship->vpivot.vx = 0;
        ship->vpivot.vy = 0;
        ship->vpivot.vz = 0;
        ship->updateCount = ship->updateCount - 1;
    }

    next = ship->nearTrkSect->nextSection.ptr;
    if (ship->attr & IN_TOW) {
        droidTarget.vx = droidTarget.vx + ((next->centre.vx - droidTarget.vx) >> 6);
        droidTarget.vy = droidTarget.vy + ((next->centre.vy - droidTarget.vy) >> 6);
        droidTarget.vz = droidTarget.vz + ((next->centre.vz - droidTarget.vz) >> 6);
        ship->vpivot.vx = PAL_SPEED(droidTarget.vx - ship->ppivot.vx);
        ship->vpivot.vy = PAL_SPEED(droidTarget.vy - ship->ppivot.vy);
        ship->vpivot.vz = PAL_SPEED(droidTarget.vz - ship->ppivot.vz);
        nose.vx = next->centre.vx - ship->nearTrkSect->centre.vx;
        nose.vy = next->centre.vy - ship->nearTrkSect->centre.vy;
        nose.vz = next->centre.vz - ship->nearTrkSect->centre.vz;
        heading = -ratan2(nose.vx, nose.vz) - (u_short)ship->hdg;
        ship->vhdg = heading;
        if (heading >= 0x801) {
            ship->vhdg = heading - 0x1000;
        } else if (heading < -0x800) {
            ship->vhdg = heading + 0x1000;
        }
        heading = (u_short)ship->hdg + (ship->vhdg >> 6);
        ship->hdg = heading;
        ship->hdg = ang(heading);
    }

    ship->vpivot.vx = ship->vpivot.vx - (ship->vpivot.vx >> 4);
    ship->vpivot.vy = ship->vpivot.vy - (ship->vpivot.vy >> 4);
    ship->vpivot.vz = ship->vpivot.vz - (ship->vpivot.vz >> 4);
    ship->ppivot.vx = ship->ppivot.vx + (ship->vpivot.vx >> 5);
    ship->ppivot.vy = ship->ppivot.vy + (ship->vpivot.vy >> 5);
    ship->ppivot.vz = ship->ppivot.vz + (ship->vpivot.vz >> 5);
    distance.vx = ship->ppivot.vx - ship->nearTrkSect->centre.vx;
    distance.vy = ship->ppivot.vy - ship->nearTrkSect->centre.vy;
    distance.vz = ship->ppivot.vz - ship->nearTrkSect->centre.vz;
    ship->pitch = ship->pitch - ((s16)ship->pitch >> 3);
    ship->roll = ship->roll - ((s16)ship->roll >> 5);
    if ((ship->attr & IN_TOW) && (GetMagnitude(distance) < 0x640)) {
        ship->update = UpdatePlayerShip;
        ship->updateCount = PAL_TIME(90);
        ship->attr &= ~RESCUE;
    }
}


void UpdateDroidStart(RescueData *droid, ShipData *ships, Object **shapes)
{
    u_short count;

    count = droid->updateCount - 1;
    droid->updateCount = count;
    if ((s16) count < 0x190) {
        s32 sinHdg;
        s32 cosHdg;
        s32 force;

        sinHdg = rsin(droid->hdg);
        force = -sinHdg * rcos(droid->pitch);
        droid->acc.vy = 0;
        droid->acc.vx = force >> 14;
        cosHdg = rcos(droid->hdg);
        force = cosHdg * rcos(droid->pitch);
        droid->vhdg = 0;
        droid->acc.vz = force >> 14;
    } else if ((s16) count < 0x2C6) {
        s32 sinHdg;
        s32 cosHdg;
        s32 force;

        sinHdg = rsin(droid->hdg);
        force = -sinHdg * rcos(droid->pitch);
        droid->acc.vy = -0x8C;
        droid->acc.vx = force >> 15;
        cosHdg = rcos(droid->hdg);
        force = cosHdg * rcos(droid->pitch);
        droid->vhdg = -8;
        droid->acc.vz = force >> 15;
    } else if ((s16) count == 0x302) {
        droid->acc.vy -= 0x5A;
    } else {
        droid->hdg = ships->hdg;
    }
    SetDroidVectors(droid, shapes);
    if ((s16) droid->updateCount < 0x1F4) {
        droid->updateCount = 0x320;
        droid->droidUpdate = UpdateDroidCircle;
        droid->pos.vx = droid->section->centre.vx;
        droid->pos.vy = droid->section->centre.vy - 0x1388;
        droid->pos.vz = droid->section->centre.vz;
    }
}


void UpdateDroidCircle(RescueData *d, ShipData *ship, Object **shapes)
{
    TrackSection *next;
    VECTOR        distance;
    VECTOR        target;
    short         hdg;
    short         quick;
    short         turn;

    next = d->section->nextSection.ptr;
    target.vx = (d->section->centre.vx + next->centre.vx) >> 1;
    target.vy = d->section->centre.vy - 3000;
    target.vz = (d->section->centre.vz + next->centre.vz) >> 1;
    distance.vx = target.vx - d->pos.vx;
    distance.vy = target.vy - d->pos.vy;
    distance.vz = target.vz - d->pos.vz;
    hdg = -ratan2(distance.vx, distance.vz);
    quick = hdg - d->hdg;
    if (d->hdg < 0) {
        turn = hdg - (d->hdg + 4096);
    } else {
        turn = hdg - (d->hdg - 4096);
    }
    if (__builtin_abs(turn) < __builtin_abs(quick)) {
        d->vhdg = turn >> 6;
    } else {
        d->vhdg = quick >> 6;
    }
    d->acc.vx = (-rsin(d->hdg) * rcos(d->pitch)) >> 15;
    d->acc.vy = distance.vy >> 6;
    d->acc.vz = (rcos(d->hdg) * rcos(d->pitch)) >> 15;
    if (ship->attr & RESCUE) {
        d->droidUpdate = UpdateDroidRescue;
        d->updateCount = 800;
        SetVolume(controlledSfxId, 255);
        CameraUpdate = UpdateCameraDroidView;
        ship->attr |= REMOTE_VIEW;
        if (ship->nearTrkSect->flags & TRACK_JUMP) {
            cameraPtr->section = ship->nearTrkSect->nextSection.ptr;
        } else {
            cameraPtr->section = ship->nearTrkSect;
        }
    }
    SetDroidVectors(d, shapes);
}


void UpdateDroidRescue(RescueData *rescueDroid, ShipData *ship, Object **shapes)
{
    char         workspace[32];
    TrackSection *section;
    TrackSection *prevSection;
    TrackData    *trackData;
    short         count;
    short         newCount;
    int          attr;
    int          verticalOffset;

    rescueDroid->acc.vx = 0;
    rescueDroid->acc.vy = 0;
    rescueDroid->acc.vz = 0;
    rescueDroid->vel.vx = 0;
    rescueDroid->vel.vy = 0;
    rescueDroid->vel.vz = 0;
    verticalOffset = droidFlyin;
    rescueDroid->pos.vx = ship->ppivot.vx;
    rescueDroid->pos.vy = ship->ppivot.vy - verticalOffset;
    rescueDroid->pos.vz = ship->ppivot.vz;
    rescueDroid->vhdg = 0;
    rescueDroid->hdg = (u_short)ship->hdg;
    count = 0;
    if (droidFlyin >= 0x15F) {
        droidFlyin -= 0x32;
    }
    if (!(ship->attr & RESCUE)) {
        SetVolume(controlledSfxId, 0);
        rescueDroid->droidUpdate = UpdateDroidCircle;
        rescueDroid->updateCount = 0x320;
        attr = ship->attr;
        ship->attr = attr & ~REMOTE_VIEW;
        if (attr & VIEW_IN) {
            CameraUpdate = UpdateCameraRaceInternal;
        } else {
            CameraUpdate = UpdateCameraRaceExternal;
        }
        section = rescueDroid->section;
        if (!(section->flags & TRACK_JUMP)) {
            trackData = track;
            while (1) {
                newCount = count + 1;
                prevSection = section->prevSection.ptr;
                rescueDroid->section = prevSection;
                count = newCount;
                if (newCount == trackData->sectionCount) {
                    break;
                }
                if (prevSection->flags & TRACK_JUMP) {
                    break;
                }
                section = prevSection;
            }
        }
    }
    SetDroidVectors(rescueDroid, shapes);
}


void SetDroidVectors(RescueData *d, Object **shapes)
{
    Face *face;

    d->vel.vx += d->acc.vx;
    d->vel.vy += d->acc.vy;
    d->vel.vz += d->acc.vz;
    d->vel.vx -= d->vel.vx >> 3;
    d->vel.vy -= d->vel.vy >> 3;
    d->vel.vz -= d->vel.vz >> 3;
    d->pos.vx += d->vel.vx >> 6;
    d->pos.vy += d->vel.vy >> 6;
    d->pos.vz += d->vel.vz >> 6;
    d->hdg += d->vhdg;
    d->hdg = ang(d->hdg);
    SetSkeletonPosition(shapes[RescueDroidModel]->skeleton, d->pos.vx, d->pos.vy, d->pos.vz);
    SetSkeletonDirectionHPR(shapes[RescueDroidModel]->skeleton, d->hdg, d->pitch, d->roll);
    shapes[RescueDroidModel]->skeleton->update = 1;
    face = track->faces + d->section->faceStart;
    while (!(face->flags & TRACK_BASE)) {
        face++;
    }
    AsmDrawObjectLit(shapes[RescueDroidModel], cameraPtr->camPos, 0, (CVECTOR *)&face->red);
}


void InitRescueDroid(RescueData *rescueDroid, Object **shipShapes, ShipData *shipIndex)
{
    int i;

    rescueDroid->section = track->sections;
    i = 0;
    while (1) {
        rescueDroid->section = rescueDroid->section->nextSection.ptr;
        if (rescueDroid->section->flags & TRACK_JUMP) {
            break;
        }
        i++;
        if (i > track->sectionCount) {
            break;
        }
    }
    rescueDroid->pos.vx = shipIndex->ppivot.vx;
    rescueDroid->pos.vy = shipIndex->ppivot.vy - 200;
    rescueDroid->pos.vz = shipIndex->ppivot.vz;
    rescueDroid->vel.vx = 0;
    rescueDroid->vel.vy = 0;
    rescueDroid->vel.vz = 0;
    rescueDroid->acc.vx = 0;
    rescueDroid->acc.vy = 0;
    rescueDroid->acc.vz = 0;
    rescueDroid->hdg = shipIndex->hdg;
    rescueDroid->pitch = shipIndex->pitch;
    rescueDroid->roll = shipIndex->roll;
    rescueDroid->vhdg = 0;
    rescueDroid->vpitch = 0;
    rescueDroid->vroll = 0;
    rescueDroid->updateCount = 800;
    rescueDroid->modelID = RescueDroidModel;
    rescueDroid->hdg = 0;
    rescueDroid->pitch = 0;
    rescueDroid->roll = 0;
    rescueDroid->droidUpdate = UpdateDroidStart;
}
