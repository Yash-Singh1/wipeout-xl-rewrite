#include "functions.h"
#include "../../platform/trace.h" /* Port: lockstep test hooks */
#include "include_asm_g8.h"

extern TrackData *track;
extern short      activeShipCount;
extern ShipRank   shipRank[];

/* A new drone is lifted to its flying height over its first 200 frames, then starts steering.
   Port: it is called through ShipData.update, so it takes the five arguments of every update function;
   the last two are not read. */
void UpdateDroneShipStart(ShipData *ships, Object **unused, int shipNo, WeaponData *unusedWeapons, ControlInput *unusedControl)
{
    ShipData *ship;
    int       remaining;

    ship = &ships[shipNo];
    ship->prevShipSect = ship->nearTrkSect;
    SectionSearch(ship); /* Port: the console also left track in the unused second argument register */
    if (ship->updateCount == 200) {
        ship->startY = ship->ppivot.vy;
    }
    ship->ppivot.vy = ship->startY + (rsin(ship->updateCount * ((shipNo * 3) + 80)) >> 7);
    remaining = ship->updateCount - 1;
    ship->updateCount = remaining;
    if (remaining == 0) {
        ship->update = UpdateDroneShip;
    }
}

extern int       ownShip;
extern int       remoteShip;
extern short     pitLaneSection1[];
extern short     pitLaneSection2[];
extern int       pitLaneDirection[];
extern int       shipDamage[];
extern u_short   classSpeedLimit[];
extern short     classSpeedMargin[];
extern u_short   classAccel[];
extern short     classSteer[];
extern RaceLine  raceLines[];

/* Speed gained on a speed-up pad, per ship class. */
typedef struct ClassBonus {
    short value[4];
} ClassBonus;

extern ClassBonus droneInitVector[];

/* Weapon types, and the samples played when a drone fires them. */
#define ROCKET         1
#define MISSILE        2
#define MINE           3
#define ELEC_BOLT      5
#define SHIELD         8
#define SFX_MINES      20
#define SFX_MISSILE    21
#define SFX_ROCKET     22
#define SFX_SHOCKWAVE  24

/* ShipData.combatAttr: the drone's temperament. */
#define DRONE_STEADY    0x10
#define DRONE_AMBUSHER  0x20
#define DRONE_HUNTER    0x40

/* TrackSection.flags */
#define SECTION_JUMP        0x1
#define SECTION_JUNCTION    0x10

#define TRACK_MAGNET 64
#define TARGETHEIGHT 256

/* Steers a computer-controlled ship round the track (or over it when it is flying), lets it
   speed up, fire at the human player and take junctions, then integrates its motion. */
void UpdateDroneShip(ShipData *ships, Object **shipShapes, int shipNo, WeaponData *weaponIndex, ControlInput *control)
{
    VECTOR       offset;
    VECTOR       targetVector;
    VECTOR       facePoint;
    VECTOR       bestPath;
    VECTOR       unused;
    Vector       path1;
    Vector       path2;
    ClassBonus   speedBonus;
    VECTOR       direction;
    /* Locals the original reads back from the stack (volatile) or hands to calcMyXPos by address. */
    short           lateral;
    short           along;
    WeaponData     *volatile weapons;
    volatile short  humanMoveDist;
    volatile short  boostStep;
    volatile short  humanNo;
    ShipData     *humanShip;
    TrackSection *section;
    TrackSection *nearSect;
    TrackSection *nextSection;
    short         i;
    short         decide;
    int           window;
    int           steerBias;
    short         rivalNo;
    ShipData     *rival;
    int           packDiff;
    int           packGap;
    u_short       attr;
    short         blocked;
    u_char        weapon;
    int           weaponCopy;
    int           sfx;
    int           sfxVolume;
    int           weaponType;
    int           flat;
    int           alpha;
    int           magnitude;
    int           speedZ;
    int           numToSpeed;
    u_char        fireType;
    WeaponData   *fireWeapons;
    short         steer;
    short         startCount;
    short         ahead;
    int           raceDiff;
    short         aheadDiff;
    short         aheadGap;
    int           sectDiff;
    short         zone;
    u_short       startTemper;
    u_short       curSpeed;
    u_short       raceDist;
    ShipData     *aheadShip;
    ShipData     *ship;
    Face         *face;
    ShipData     *self;

    /* The human ship this drone is measured against: the remote player when it is ahead of the local one
       (and gameType is not 2), else the local player. */
    weapons = weaponIndex;
    speedBonus = droneInitVector[0];
    blocked = 0;
    offset.vx = 0;
    offset.vy = 0;
    offset.vz = 0;
    /* Port: the console stored the low halfword of ownShip / remoteShip (read with lhu); the conversion to short is the same. */
    if ((gameType != 2) && (playerPosition >= remoteRacePos)) {
        humanNo = remoteShip;
    } else {
        humanNo = ownShip;
    }
    ship = ships + shipNo;
    startCount = ship->numToSpeed;
    humanShip = ships + humanNo;
    /* Launch acceleration while numToSpeed counts down, afterwards the speed creeps up to its limit. */
    if (startCount > 0) {
        startTemper = ship->combatAttr;
        ship->numToSpeed = startCount - 1;
        ship->updateCount = 0;
        if ((startTemper & DRONE_STEADY) || (shipNo == ownShip) || (shipNo == remoteShip)) {
            if ((ship->remoteMax + (classSpeedMargin[shipClassIndex] / 2)) >= ship->speed) {
                ship->speed = ship->speed + (ship->remoteMagnitude + classAccel[shipClassIndex]);
            }
        } else if (startTemper & DRONE_AMBUSHER) {
            if ((ship->remoteMax + ((classSpeedMargin[shipClassIndex] * 32) / 35)) >= ship->speed) {
                ship->speed = ship->speed + (ship->remoteMagnitude + classAccel[shipClassIndex]);
            }
        } else if (startTemper & DRONE_HUNTER) {
            if ((ship->remoteMax + ((classSpeedMargin[shipClassIndex] * 32) / 27)) >= ship->speed) {
                ship->speed = ship->speed + (ship->remoteMagnitude + classAccel[shipClassIndex]);
            }
        }
    } else if (ship->autopilotCount == 0) {
        if (ship->speed <= ship->remoteMax) {
            ship->speed = ship->speed + ship->remoteMagnitude;
        }
    } else {
        if ((ship->remoteMax * 2) >= ship->speed) {
            ship->speed = ship->speed + (ship->remoteMagnitude * 3);
        }
    }
    if (!(ship->attr & FLYING)) {
        /* Steer along the track base face; overtake a ship in front if it is close and slower. */
        face = track->faces + ship->nearTrkSect->faceStart;
        while (!(face->flags & TRACK_BASE)) {
            face++;
        }
        self = ships + shipNo;
        raceDist = (u_short)raceLines[self->nearSecNo].distance;
        raceDiff = raceDist - (u_short)raceLines[ships[humanNo].nearSecNo].distance;
        sectDiff = raceDiff;
        if ((short)raceDiff < 0) {
            sectDiff = numTrackSections + raceDiff;
        }
        ahead = getShipInfrontLAP(ships, shipNo);
        if (ahead != -1) {
            aheadShip = ships + ahead;
            aheadDiff = (u_short)raceLines[aheadShip->nearSecNo].distance - raceDist;
            aheadGap = aheadDiff;
            if (aheadDiff < 0) {
                aheadGap = numTrackSections + aheadDiff;
            }
            if ((aheadGap < 0x20) && (aheadShip->moveDist < self->moveDist)) {
                lookForWayPast(&offset, ships, shipNo, ahead);
            } else {
                followRaceLine(&offset, ships, face, shipNo);
            }
        } else {
            followRaceLine(&offset, ships, face, shipNo);
        }
        /* Zone: how far ahead (+) or behind (-) the human ship is, in sections; 100 means out of range. */
        if (ship->autopilotCount == 0) {
            window = sectDiff + 79;
            if (((short)numTrackSections / 2) < (short)sectDiff) {
                sectDiff -= numTrackSections;
                window = sectDiff + 79;
            }
            zone = 100;
            if ((u_short)window < 163) {
                if ((u_short)(sectDiff - 41) < 40) {
                    zone = 3;
                }
                if ((u_short)(sectDiff - 21) < 20) {
                    zone = 2;
                }
                if ((u_short)(sectDiff - 13) < 8) {
                    zone = 1;
                }
                if ((u_short)(sectDiff + 8) < 21) {
                    zone = 0;
                }
                if ((u_short)(sectDiff + 20) < 12) {
                    zone = -1;
                }
                if ((u_short)(sectDiff + 40) < 20) {
                    zone = -2;
                }
                if ((u_short)(sectDiff + 80) < 40) {
                    zone = -3;
                }
                steerBias = classSteer[shipClassIndex];
                rivalNo = humanNo;
                rival = ships + rivalNo;
                steer = steerBias;
                humanMoveDist = rival->moveDist;
                if (steerBias < 0) {
                    steerBias += 7;
                }
                numToSpeed = ship->numToSpeed;
                boostStep = (u_short)((u_int)steerBias >> 3);
                if ((numToSpeed == 0) && (getShipBehindLAP(ships, shipNo) == rivalNo) && (activeShipCount >= 4)) {
                    packDiff = (u_short)raceLines[ships[getShipInfrontLAP(ships, getShipInfrontLAP(ships, shipNo))].nearSecNo].distance - (u_short)raceLines[rival->nearSecNo].distance;
                    packGap = packDiff;
                    if ((short)packDiff < 0) {
                        packGap = numTrackSections + packDiff;
                    }
                    if ((u_short)(packGap - 1) < 39) {
                        blocked = 1;
                    }
                }
                if ((blocked == 0) && (ships[shipNo].moveDist < humanMoveDist)) {
                    if ((zone == 2) || (zone == -2)) {
                        if ((((short)classSpeedLimit[shipClassIndex] / 2) < ships[humanNo].speed) && (((attr = ship->combatAttr) & DRONE_AMBUSHER) || (attr & DRONE_HUNTER))) {
                            curSpeed = ship->speed;
                            if (curSpeed <= (ship->remoteMax + (steer / 4))) {
                                ship->speed = curSpeed + (ship->remoteMagnitude + (boostStep / 4));
                            }
                        }
                    }
                    if ((ships[shipNo].moveDist < humanMoveDist) && (((u_short)zone < 2U) || (zone == -1)) && (((short)classSpeedLimit[shipClassIndex] / 2) < ships[humanNo].speed) && (((attr = ship->combatAttr) & DRONE_AMBUSHER) || (attr & DRONE_HUNTER))) {
                        curSpeed = ship->speed;
                        if ((ship->remoteMax + (steer / 2)) >= curSpeed) {
                            ship->speed = curSpeed + (ship->remoteMagnitude + (boostStep / 2));
                        }
                    }
                }
            }
            /* Fire the drone's weapon according to its temperament and the zone. */
            weapon = ship->weapon;
            weaponType = weapon & 0xFF;
            if ((weaponType != 0) && (zone != 100) && !(ships[humanNo].combatAttr & DESTROYED)) {
                attr = ship->combatAttr;
                if (!(attr & DRONE_AMBUSHER)) goto hunterAttack;
                if ((zone == -3) && (((u_int)(weapon - 1) < 2U) || (weaponType == ELEC_BOLT))) {
                    if (weaponType == ROCKET) PlayNote(SFX_ROCKET, 0x1FE, 15, 15, 256);
                    if (weaponType == MISSILE) PlayNote(SFX_MISSILE, 0x1FE, 15, 15, 256);
                    if (weaponType == ELEC_BOLT) PlayNote(SFX_SHOCKWAVE, 0x1FE, 15, 15, 256);
                    fireType = ship->weapon;
                    fireWeapons = weapons;
                    ship->targetShip = -1;
                    FireWeapon(ship, fireWeapons, shipNo, fireType);
                    goto weaponDone;
                }
                weaponCopy = weapon & 255;
                if (zone != 1) goto weaponByteReady;
                if (weaponCopy != MINE) goto missileCheck;
                FireWeapon(ship, weapons, shipNo, *(volatile u_char *)&ship->weapon);
                sfx = weaponType + (SFX_MINES - MINE);
                sfxVolume = 470;
                goto playSound;
hunterAttack:
                if (!(attr & DRONE_HUNTER)) goto weaponDone;
                if (((u_short)(zone + 3) < 2U) && (((u_int)(weapon - 1) < 2U) || (weaponType == ELEC_BOLT))) {
                    fireType = *(volatile u_char *)&ship->weapon;
                    fireWeapons = weapons;
                    ship->targetShip = (u_char)humanNo;
                    FireWeapon(ship, fireWeapons, shipNo, fireType);
                    if (weaponType == ROCKET) PlayNote(SFX_ROCKET, 0x1FE, 15, 15, 256);
                    if (weaponType == MISSILE) PlayNote(SFX_MISSILE, 0x1FE, 15, 15, 256);
                    if (weaponType == ELEC_BOLT) PlayNote(SFX_SHOCKWAVE, 0x1FE, 15, 15, 256);
                }
                weaponCopy = weapon & 255;
                if (zone != 1) goto weaponByteReady;
                if (weaponCopy != MINE) goto missileCheck;
                FireWeapon(ship, weapons, shipNo, ship->weapon);
                sfx = SFX_MINES;
                sfxVolume = 470;
                goto playSound;
missileCheck:
                if (weaponCopy != MISSILE) goto weaponDone;
                weaponCopy = weapon & 255;
                if (gameType != weaponCopy) goto weaponDone;
                fireType = *(volatile u_char *)&ship->weapon;
                fireWeapons = weapons;
                ship->targetShip = (u_char)humanNo;
                FireWeapon(ship, fireWeapons, shipNo, fireType);
                sfx = SFX_MISSILE;
                sfxVolume = 510;
playSound:
                PlayNote(sfx, sfxVolume, 15, 15, 256);
weaponDone:
                weaponCopy = weapon & 255;
weaponByteReady:
                if ((weaponCopy == SHIELD) && ((u_short)((u_short)zone - 1) < 3U)) {
                    FireWeapon(ship, weapons, shipNo, ship->weapon);
                }
            }
        }
        /* Pick the branch to take at a junction: pit lane when due, otherwise at random. */
        section = ship->nearTrkSect->prevSection.ptr;
        for (i = 0; i < 4; i++) {
            section = section->nextSection.ptr;
        }
        if ((section->junction.ptr != (TrackSection *)-1) && (section->junction.ptr->flags & SECTION_JUNCTION)) {
            if ((section->secNo == pitLaneSection1[clipLevel]) || (section->secNo == pitLaneSection2[clipLevel])) {
                ship->attr &= ~JUNC_LEFT;
                if (shipDamage[shipNo] > 0) {
                    ship->attr |= pitLaneDirection[clipLevel];
                } else {
                    ship->attr |= pitLaneDirection[clipLevel] ^ JUNC_LEFT;
                }
            } else {
                decide = newRand() % 2;
                if (decide == 0) {
                    ship->attr |= JUNC_LEFT;
                } else {
                    ship->attr &= ~JUNC_LEFT;
                }
            }
        }
        section = ship->nearTrkSect->prevSection.ptr;
        for (i = 0; i < 4; i++) {
            if ((section->junction.ptr != (TrackSection *)-1) && (section->junction.ptr->flags & SECTION_JUNCTION)
                && (ship->attr & JUNC_LEFT)) {
                section = section->junction.ptr;
            } else {
                section = section->nextSection.ptr;
            }
        }
        /* Target velocity along the track, bled off by turning, then the pull towards the racing line. */
        nextSection = section->nextSection.ptr;
        nearSect = ship->nearTrkSect;
        ship->speed -= abs(ship->speed * ship->vhdg) >> 14;
        ship->speed -= abs(ship->speed * ship->vpitch) >> 14;
        section = ship->nearTrkSect;
        if ((nearSect->flags & SECTION_JUMP) || (nearSect->nextSection.ptr->flags & SECTION_JUMP)
            || (nearSect->prevSection.ptr->flags & SECTION_JUMP)
            || (nearSect->nextSection.ptr->nextSection.ptr->flags & SECTION_JUMP)
            || (nearSect->nextSection.ptr->nextSection.ptr->nextSection.ptr->flags & SECTION_JUMP)) {
            targetVector.vx = ship->nearTrkSect->centre.vx - ship->nearTrkSect->prevSection.ptr->centre.vx;
            targetVector.vy = ship->nearTrkSect->centre.vy - ship->nearTrkSect->prevSection.ptr->centre.vy;
            targetVector.vz = ship->nearTrkSect->centre.vz - ship->nearTrkSect->prevSection.ptr->centre.vz;
        } else {
            targetVector.vx = nextSection->centre.vx - section->centre.vx;
            targetVector.vy = nextSection->centre.vy - section->centre.vy;
            targetVector.vz = nextSection->centre.vz - section->centre.vz;
        }
        magnitude = GetMagnitude(targetVector);
        targetVector.vx = (targetVector.vx * ship->speed) / magnitude;
        targetVector.vy = (targetVector.vy * ship->speed) / magnitude;
        targetVector.vz = (targetVector.vz * ship->speed) / magnitude;
        path1.vx = section->centre.vx + offset.vx;
        path1.vy = section->centre.vy + offset.vy;
        path1.vz = section->centre.vz + offset.vz;
        path2.vx = nextSection->centre.vx + offset.vx;
        path2.vy = nextSection->centre.vy + offset.vy;
        path2.vz = nextSection->centre.vz + offset.vz;
        GetTargetPos(&bestPath, ship->ppivot, path2, path1);
        ship->apivot.vx = targetVector.vx + (bestPath.vx - ship->ppivot.vx);
        ship->apivot.vy = targetVector.vy + (bestPath.vy - ship->ppivot.vy);
        ship->apivot.vz = targetVector.vz + (bestPath.vz - ship->ppivot.vz);
        ship->vpivot.vx = ship->vpivot.vx + ship->apivot.vx;
        ship->vpivot.vy = ship->vpivot.vy + ship->apivot.vy;
        ship->vpivot.vz = ship->vpivot.vz + ship->apivot.vz;
        /* Speed-up pads boost the drone when it drives over them on the matching side. */
        calcMyXPos(ships, shipNo, &lateral, &along);
        if ((face->flags & SPEED_UP) && (lateral < 0)) {
            ship->speed = ship->speed + speedBonus.value[shipClassIndex];
        }
        face++;
        if ((face->flags & SPEED_UP) && (lateral > 0)) {
            ship->speed = ship->speed + speedBonus.value[shipClassIndex];
        }
        facePoint.vx = track->vertices[face->vertex[0]].vx;
        facePoint.vy = track->vertices[face->vertex[0]].vy;
        facePoint.vz = track->vertices[face->vertex[0]].vz;
        alpha = PlaneLineCollision(facePoint, ship->ppivot, face->normal);
        if (alpha < 50) {
            alpha = 50;
        }
        ship->apivot.vx = ((face->normal.vx * TARGETHEIGHT * TRACK_MAGNET) / alpha - (face->normal.vx * TRACK_MAGNET)) >> 8;
        ship->apivot.vy = ((face->normal.vy * TARGETHEIGHT * TRACK_MAGNET) / alpha - (face->normal.vy * TRACK_MAGNET)) >> 8;
        ship->apivot.vz = ((face->normal.vz * TARGETHEIGHT * TRACK_MAGNET) / alpha - (face->normal.vz * TRACK_MAGNET)) >> 8;
        ship->vpivot.vx += ship->apivot.vx;
        ship->vpivot.vy += ship->apivot.vy;
        ship->vpivot.vz += ship->apivot.vz;
        /* Heading, pitch and roll follow the target direction. */
        flat = SquareRoot0(targetVector.vx * targetVector.vx + targetVector.vz * targetVector.vz);
        ship->vhdg = -ratan2(targetVector.vx, targetVector.vz) - ship->hdg;
        ship->vhdg += ship->ahdg;
        ship->ahdg -= ship->ahdg >> 3;
        if (ship->vhdg > 2048) {
            ship->vhdg = ship->vhdg - 4096;
        } else if (ship->vhdg < -2048) {
            ship->vhdg = ship->vhdg + 4096;
        }
        ship->hdg += ship->vhdg >> 4;
        ship->vpitch = -ratan2(targetVector.vy, flat) - ship->pitch;
        ship->pitch += ship->vpitch >> 4;
        ship->vroll += sar(ship->vhdg, 2); /* Port: the console call left the old pitch in $a2, which sar never reads */
        ship->vroll -= ship->vroll >> 1;
        ship->roll += ship->vroll;
        ship->roll -= ship->roll >> 3;
        facePoint.vx = track->vertices[face->vertex[0]].vx;
        facePoint.vy = track->vertices[face->vertex[0]].vy;
        facePoint.vz = track->vertices[face->vertex[0]].vz;
        if (ship->autopilotCount > 0) {
            alpha = PlaneLineCollision(facePoint, ship->ppivot, face->normal);
            if (alpha < -300) {
                alpha = -300;
            }
            DroneShip2TrackBase(ship, face, alpha);
            section = ship->nearTrkSect;
            nextSection = section->nextSection.ptr;
            direction.vx = nextSection->centre.vx - section->centre.vx;
            direction.vy = nextSection->centre.vy - ship->nearTrkSect->centre.vy;
            direction.vz = nextSection->centre.vz - ship->nearTrkSect->centre.vz;
            if (((direction.vx * ship->unitVecNose.vx) + (direction.vy * ship->unitVecNose.vy) + (direction.vz * ship->unitVecNose.vz)) < 0) {
                ship->attr &= ~DIRECTION;
            } else {
                ship->attr |= DIRECTION;
            }
        }
    } else {
        section = ship->nearTrkSect->nextSection.ptr->nextSection.ptr;
        nextSection = section->nextSection.ptr;
        moveDownCentre(&offset, humanShip, face, shipNo);
        ship->speed -= abs(ship->speed * ship->vhdg) >> 14;
        targetVector.vx = nextSection->centre.vx - section->centre.vx;
        targetVector.vy = nextSection->centre.vy - section->centre.vy;
        targetVector.vz = nextSection->centre.vz - section->centre.vz;
        magnitude = GetMagnitude(targetVector);
        targetVector.vx = (targetVector.vx * ship->speed) / magnitude;
        speedZ = targetVector.vz * ship->speed;
        targetVector.vy = 500;
        targetVector.vz = speedZ / magnitude;
        GetTargetPos(&bestPath, ship->ppivot, nextSection->centre, ship->nearTrkSect->centre);
        ship->apivot.vx = targetVector.vx + ((bestPath.vx - ship->ppivot.vx) >> 1);
        ship->apivot.vy = targetVector.vy;
        ship->apivot.vz = targetVector.vz + ((bestPath.vz - ship->ppivot.vz) >> 1);
        ship->vpivot.vx += ship->apivot.vx;
        ship->vpivot.vy += ship->apivot.vy;
        ship->vpivot.vz += ship->apivot.vz;
        ship->vpitch = -200 - ship->pitch;
        ship->pitch += ship->vpitch >> 4;
        ship->vhdg = -ratan2(targetVector.vx, targetVector.vz) - ship->hdg;
        if (ship->vhdg > 2048) {
            ship->vhdg = ship->vhdg - 4096;
        } else if (ship->vhdg < -2048) {
            ship->vhdg = ship->vhdg + 4096;
        }
        ship->hdg += ship->vhdg >> 4;
    }
    ship->vpivot.vx -= ship->vpivot.vx >> 3;
    ship->vpivot.vy -= ship->vpivot.vy >> 3;
    ship->vpivot.vz -= ship->vpivot.vz >> 3;
    ship->ppivot.vx += ship->vpivot.vx >> 6;
    ship->ppivot.vy += ship->vpivot.vy >> 6;
    ship->ppivot.vz += ship->vpivot.vz >> 6;
    if (ship->attr & ELECTROED) {
        ship->ppivot.vx += (newRand() - 0x4000) % 20;
        ship->ppivot.vy += (newRand() - 0x4000) % 20;
        ship->ppivot.vz += (newRand() - 0x4000) % 20;
        ship->electroCount--;
        if (ship->electroCount <= 0) {
            ship->electroCount = 0;
            ship->attr &= ~ELECTROED;
        }
        if ((newRand() % 10) == 0) {
            ship->speed -= ship->speed >> 1;
        }
    }
    ship->hdg = ang(ship->hdg);
    ship->pitch = ang(ship->pitch);
    ship->roll = ang(ship->roll);
    if (ship->autopilotCount > 0) {
        if (control->pressed & 0x8000) {
            ship->update = UpdatePlayerShip;
            ship->vhdg = 0;
            ship->autopilotCount = 0;
            if (shipNo == ownShip) {
                PlayNote(0x2E, 0xFF, 15, 15, 0x100);
            }
        } else {
            if (!PortAutopilot()) { /* Port: the lockstep tests' infinite autopilot keeps its count */
                ship->autopilotCount--;
            }
            if (ship->autopilotCount == 0) {
                if ((u_short)(ship->roll + 49) < 99) {
                    ship->update = UpdatePlayerShip;
                    ship->vhdg = 0;
                    return;
                }
                ship->autopilotCount = 1;
            }
        }
    }
}

/* Clears the offset from the racing line. */
void moveDownCentre(VECTOR *relative, ShipData *ships, Face *segment, int shipNo)
{
    relative->vx = 0;
    relative->vy = 0;
    relative->vz = 0;
}

/* Offset from the racing-line point for the ship's section to the start vertex of its face. */
void followRaceLine(VECTOR *relative, ShipData *ships, Face *segment, int ship)
{
    int        index;
    TrackData *t;

    index = ships[ship].nearSecNo;
    t = track;
    /* Port: racingLine[n].vx, vy and vz were another name for raceLines[n].x, y and z. */
    relative->vx = raceLines[index].x - t->vertices[segment->vertex[0]].vx;
    relative->vy = raceLines[index].y - t->vertices[segment->vertex[0]].vy;
    relative->vz = raceLines[index].z - t->vertices[segment->vertex[0]].vz;
}

/* Steers past the ship ahead when the two are level sideways and there is room beside it. */
void lookForWayPast(VECTOR *relative, ShipData *ships, short shipNo, short otherShip)
{
    short  otherLateral;
    short  otherWidth;
    short  lateral;
    short  width;
    short  temp;
    int    gap;
    int    negWidth;
    int    clear;
    Face  *face;

    clear = 0;
    face = &track->faces[ships[shipNo].nearTrkSect->faceStart];
    while (!(face->flags & TRACK_BASE)) {
        face++;
    }
    calcMyXPos(ships, otherShip, &otherLateral, &otherWidth);
    calcMyXPos(ships, shipNo, &lateral, &width);
    if (otherWidth < otherLateral) {
        otherLateral = otherWidth;
    }
    negWidth = -otherWidth;
    if (otherLateral < negWidth) {
        otherLateral = -otherWidth;
    }
    if (width < lateral) {
        lateral = width;
    }
    if (lateral < -width) {
        lateral = -width;
    }
    gap = abs(otherLateral - lateral);
    if (gap < 20) {
        temp = otherLateral;
        gap = abs(negWidth - otherLateral);
        temp = otherWidth - temp;
        if ((short)gap >= 21) {
            clear |= 1;
        }
        if (temp >= 21) {
            clear |= 2;
        }
        if ((otherLateral < lateral) && (clear & 2)) {
            goRight(relative, ships, face, shipNo);
            return;
        }
        if ((lateral < otherLateral) && (clear & 1)) {
            goLeft(relative, ships, face, shipNo);
        }
    } else {
        followRaceLine(relative, ships, face, shipNo);
    }
}

/* Lateral offset of the ship across its track base edge (also stored in unkA8), and the
   position of the section centre along that edge. */
void calcMyXPos(ShipData *ships, short shipNo, short *lateral, short *along)
{
    ShipData *ship;
    Face     *face;
    VECTOR    edge;
    VECTOR    fromCentre;
    VECTOR    fromSecond;

    face = &track->faces[ships[shipNo].nearTrkSect->faceStart];
    while (!(face->flags & TRACK_BASE)) {
        face++;
    }
    edge.vx = track->vertices[face->vertex[0]].vx - track->vertices[face->vertex[1]].vx;
    edge.vy = track->vertices[face->vertex[0]].vy - track->vertices[face->vertex[1]].vy;
    edge.vz = track->vertices[face->vertex[0]].vz - track->vertices[face->vertex[1]].vz;
    ship = &ships[shipNo];
    fromCentre.vx = ship->nearTrkSect->centre.vx - ship->ppivot.vx;
    fromCentre.vy = ship->nearTrkSect->centre.vy - ship->ppivot.vy;
    fromCentre.vz = ship->nearTrkSect->centre.vz - ship->ppivot.vz;
    fromSecond.vx = ship->nearTrkSect->centre.vx - track->vertices[face->vertex[1]].vx;
    fromSecond.vy = ship->nearTrkSect->centre.vy - track->vertices[face->vertex[1]].vy;
    fromSecond.vz = ship->nearTrkSect->centre.vz - track->vertices[face->vertex[1]].vz;
    *lateral = (int)-((fromCentre.vx * edge.vx) + (fromCentre.vy * edge.vy) + (fromCentre.vz * edge.vz)) >> 16;
    *along = ((fromSecond.vx * edge.vx) + (fromSecond.vy * edge.vy) + (fromSecond.vz * edge.vz)) >> 16;
    if (*lateral > 0) {
        ship->attr |= LSIDE;
    } else {
        ship->attr &= ~LSIDE;
    }
    ships[shipNo].lateralOffset = (u_short)*lateral;
}

/* Half the base-edge vector of the face, from its second vertex to its first. */
void goRight(VECTOR *result, ShipData *shipIndex, Face *facePtr, int shipNo)
{
    VECTOR faceVert1;
    VECTOR faceVert2;

    faceVert1.vx = track->vertices[facePtr->vertex[0]].vx;
    faceVert1.vy = track->vertices[facePtr->vertex[0]].vy;
    faceVert1.vz = track->vertices[facePtr->vertex[0]].vz;
    faceVert2.vx = track->vertices[facePtr->vertex[1]].vx;
    faceVert2.vy = track->vertices[facePtr->vertex[1]].vy;
    faceVert2.vz = track->vertices[facePtr->vertex[1]].vz;
    result->vx = (faceVert1.vx - faceVert2.vx) >> 1;
    result->vy = (faceVert1.vy - faceVert2.vy) >> 1;
    result->vz = (faceVert1.vz - faceVert2.vz) >> 1;
}

/* Half the base-edge vector of the face, from its first vertex to its second. */
void goLeft(VECTOR *result, ShipData *shipIndex, Face *facePtr, int shipNo)
{
    VECTOR faceVert1;
    VECTOR faceVert2;

    faceVert1.vx = track->vertices[facePtr->vertex[1]].vx;
    faceVert1.vy = track->vertices[facePtr->vertex[1]].vy;
    faceVert1.vz = track->vertices[facePtr->vertex[1]].vz;
    faceVert2.vx = track->vertices[facePtr->vertex[0]].vx;
    faceVert2.vy = track->vertices[facePtr->vertex[0]].vy;
    faceVert2.vz = track->vertices[facePtr->vertex[0]].vz;
    result->vx = (faceVert1.vx - faceVert2.vx) >> 1;
    result->vy = (faceVert1.vy - faceVert2.vy) >> 1;
    result->vz = (faceVert1.vz - faceVert2.vz) >> 1;
}

/* The nearest ship ahead in race order that is still running, or -1. */
short getShipInfrontLAP(ShipData *ships, short ship)
{
    short position;
    short attempts;
    short candidate;

    /* Port: shipOrder[n][0] was another name for shipRank[n].shipNo. */
    for (position = 0; position < activeShipCount; position++) {
        if (shipRank[position].shipNo == ship) {
            break;
        }
    }
    for (attempts = 0; attempts < 15; attempts++) {
        if (position == 0) {
            position = activeShipCount;
        }
        position--;
        candidate = shipRank[position].shipNo;
        if (!(ships[candidate].combatAttr & DESTROYED)) {
            if (candidate != ship) {
                return candidate;
            }
        }
    }
    return -1;
}

/* The nearest running ship behind in race order, or -1. */
short getShipBehindLAP(ShipData *ships, short ship)
{
    short position;
    short attempts;
    short candidate;

    for (position = 0; position < activeShipCount; position++) {
        if (shipRank[position].shipNo == ship) {
            break;
        }
    }
    for (attempts = 0; attempts < 15; attempts++) {
        position++;
        if (position >= activeShipCount) {
            position = 0;
        }
        candidate = shipRank[position].shipNo;
        if (!(ships[candidate].combatAttr & DESTROYED)) {
            if (candidate != ship) {
                return candidate;
            }
        }
    }
    return -1;
}

/* Sets a drone's speed from its own speed limit, clamped to its top speed, and clears its turn. */
void humanToDroneSpeed(ShipData *ships, int shipNo)
{
    ShipData *ship;
    int       speed;

    ship = &ships[shipNo];
    speed = ship->moveDist * 9;
    if (ship->remoteMax < speed) {
        speed = ship->remoteMax;
    }
    ship->speed = speed;
    ship->numToSpeed = 0;
    ship->ahdg = 0;
}
