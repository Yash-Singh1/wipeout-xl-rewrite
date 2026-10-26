#include "functions.h"
#include "sfx.h"
#include "racectrl.h"
#include "portorig.h" /* Port: the original Wipeout's tracks */
#include "../../platform/trace.h" /* Port: lockstep test hooks */
#include "ships.h"
#include "control.h"

/* $gp-relative globals are tentative definitions; the rest are extern. */
extern int startCountdown;

extern int           ownShip;
extern ConfigData   *gameData;
extern int           ghostShip;
extern int           remoteShip;
extern int           raceType;
extern int           checkTimer;
extern short         activeShipCount;
extern ControlInput  ghostControl[];
extern ControlInput  remoteControl[];
extern RaceLine      raceLines[];
extern ShipRank      shipRank[];
extern TrackData    *track;
extern int           classCheckTime[];
extern short         startLinePos[];
extern CheckCell     checkCells[][4][6];
extern BrakeModes    brakeModes[];
extern unsigned char turnDivisor;
extern unsigned char twist;
extern int           currScreen;
extern char          countdownActive;
void UpdateShips(ShipData *shipIndex, WeaponShape *unused, Object **shipShapes, WeaponData *weaponIndex)
{
    short i;
    short activeCount;
    short shipNo;
    short shipNum;
    VECTOR lastPos;
    VECTOR direction;
    VECTOR trackPos;
    SVECTOR position;
    ShipData *playerShip;

    PortScriptWrites(&shipIndex[ownShip], gameData, ownShip); /* Port: scripted writes for the lockstep tests (WIPEOUT_WRITES) */
    activeCount = 0;
    for (i = 0; i < 15; i++) {
        if (!(shipIndex[i].combatAttr & DESTROYED)) {
            activeCount++;
        }
    }

    if (activeCount > 0) {
        SortAllShips(shipIndex);
        for (i = 0; i < activeShipCount; i++) {
            shipNo = shipRank[i].shipNo;
            playerShip = &shipIndex[shipNo];
            lastPos.vx = playerShip->ppivot.vx;
            lastPos.vy = playerShip->ppivot.vy;
            lastPos.vz = playerShip->ppivot.vz;
            shipNum = shipNo;
            switch (raceType) {
            case RACE_TIME_TRIAL:
                if (shipNo == ownShip) {
                    playerShip->update(shipIndex, shipShapes, shipNo, weaponIndex, playerControl);
                } else if (shipNo == ghostShip) {
                    buildGhostReplayData(shipIndex);
                    playerShip->update(shipIndex, shipShapes, shipNo, weaponIndex, ghostControl);
                }
                break;

            case RACE_ONE_ON_ONE:
                if (shipNo == ownShip) {
                    playerShip->update(shipIndex, shipShapes, shipNo, weaponIndex, playerControl);
                    doShipShipCollision(shipNo, shipIndex, shipShapes);
                } else if (shipNo == remoteShip) {
                    playerShip->update(shipIndex, shipShapes, shipNo, weaponIndex, remoteControl);
                    doShipShipCollision(shipNo, shipIndex, shipShapes);
                }
                break;

            case RACE_ARCADE_LINK:
                if (shipNo == ownShip) {
                    playerShip->update(shipIndex, shipShapes, shipNo, weaponIndex, playerControl);
                    doShipShipCollision(shipNo, shipIndex, shipShapes);
                } else if (shipNo == remoteShip) {
                    playerShip->update(shipIndex, shipShapes, shipNo, weaponIndex, remoteControl);
                    doShipShipCollision(shipNo, shipIndex, shipShapes);
                } else {
                    playerShip->update(shipIndex, shipShapes, shipNo, weaponIndex, playerControl);
                    doShipShipCollision(shipNo, shipIndex, shipShapes);
                }
                break;

            default:
                shipNo = shipNum;
                shipIndex[shipNo].update(shipIndex, shipShapes, shipNo, weaponIndex, playerControl);
                doShipShipCollision(shipNo, shipIndex, shipShapes);
            }

            lastPos.vx = shipIndex[shipNum].ppivot.vx - lastPos.vx;
            lastPos.vy = shipIndex[shipNum].ppivot.vy - lastPos.vy;
            lastPos.vz = shipIndex[shipNum].ppivot.vz - lastPos.vz;
            shipIndex[shipNum].moveDist = GetMagnitude(lastPos);
        }

        SetUnitVectors(shipIndex);
    }
}


void SortAllShips(ShipData *shipIndex)
{
    short i, count;
    u_short swapSection, swapShip;
    short firstSection;
    short swapped;
    VECTOR distanceA, distanceB, direction;
    int angleA, angleB;
    int projectedA, projectedB;
    ShipData *playerShip;
    ShipRank *rank, *nextRank, *current, *next;
    short currentShip;
    short savedShip;
    u_short sectionNumber;

    count = 0;
    for (i = 0; i < activeShipCount; i++) {
        currentShip = shipRank[i].shipNo;
        playerShip = &shipIndex[currentShip];
        if (!(playerShip->combatAttr & DESTROYED)) {
            savedShip = currentShip;
            sectionNumber = raceLines[playerShip->nearSecNo].distance;
            shipRank[count].shipNo = savedShip;
            shipRank[count].position = sectionNumber;
            count++;
        }
    }
    activeShipCount = count;
    if (count >= 2) {
        rank = shipRank;
        nextRank = rank + 1;
        do {
            swapped = 0;
            for (i = 0; i < activeShipCount - 1; i++) {
                current = &rank[i];
                next = &nextRank[i];
                firstSection = current->position;
                if (firstSection < shipRank[i + 1].position) {
                    short savedSection;
                    savedSection = firstSection;
                    swapSection = next->position;
                    next->position = savedSection;
                    current->position = swapSection;
                    swapShip = current->shipNo;
                    swapSection = next->shipNo;
                    next->shipNo = swapShip;
                    current->shipNo = swapSection;
                    swapped = 1;
                } else if (firstSection == next->position) {
                    playerShip = &shipIndex[current->shipNo];
                    distanceA.vx = playerShip->ppivot.vx - playerShip->nearTrkSect->centre.vx;
                    distanceA.vy = playerShip->ppivot.vy - playerShip->nearTrkSect->centre.vy;
                    distanceA.vz = playerShip->ppivot.vz - playerShip->nearTrkSect->centre.vz;
                    distanceB.vx = playerShip->ppivot.vx - playerShip->nearTrkSect->centre.vx;
                    distanceB.vy = playerShip->ppivot.vy - playerShip->nearTrkSect->centre.vy;
                    distanceB.vz = playerShip->ppivot.vz - playerShip->nearTrkSect->centre.vz;
                    direction.vx = playerShip->nearTrkSect->nextSection.ptr->centre.vx - playerShip->nearTrkSect->centre.vx;
                    direction.vy = playerShip->nearTrkSect->nextSection.ptr->centre.vy - playerShip->nearTrkSect->centre.vy;
                    direction.vz = playerShip->nearTrkSect->nextSection.ptr->centre.vz - playerShip->nearTrkSect->centre.vz;
                    angleA = GetAng(distanceA, direction);
                    angleB = GetAng(distanceB, direction);
                    projectedA = (rcos(angleA >> 3) * GetMagnitude(distanceA)) >> 12;
                    projectedB = (rcos(angleB >> 3) * GetMagnitude(distanceB)) >> 12;
                    if (projectedA < projectedB) {
                        swapSection = next->position;
                        next->position = current->position;
                        current->position = swapSection;
                        swapShip = current->shipNo;
                        swapSection = next->shipNo;
                        next->shipNo = swapShip;
                        current->shipNo = swapSection;
                    }
                }
            }
        } while (swapped);
    }
}


void UpdatePlayerShipStart(ShipData *shipIndex, Object **shipShapes, int shipNo, WeaponData *weaponIndex, ControlInput *controller)
{
    ShipData *playerShip;
    int thrust;
    int targetThrust;
    int thrustDelta;
    short thrustVal;
    int boostAt, stallAt, rumbleLevel; /* Port: the pads' motors at the launch (below) */

    if (!(shipIndex[ownShip].combatAttr & DESTROYED)) {
        playerShip = &shipIndex[shipNo];
        GeneralStartProcs(playerShip);
        if (playerShip->updateCount == PAL_TIME(199)) {
            playerShip->startY = playerShip->ppivot.vy;
        }
        playerShip->ppivot.vy = playerShip->startY + (rsin(playerShip->updateCount * 80) >> 7);
        if (playerShip->updateCount == 0) {
            if (playerShip->attr & RACING) {
                playerShip->updateCount = PAL_TIME(90);
                playerShip->update = UpdatePlayerShip;
            } else {
                playerShip->update = UpdateDroneShip;
            }
        }
        if (playerShip->attr & RACING) {
            SetBrake(playerShip, controller);
            SetView(playerShip, controller);
        }
        if (controller->buttons & CONTROL_ANALOG_THRUST) {
            thrustVal = (controller->thrust * 5) >> 2;
            if (thrustVal >= 256) {
                thrustVal = 255;
            }
            targetThrust = (playerShip->maxThrust * thrustVal) / 200;
            if (targetThrust > playerShip->maxThrust) {
                targetThrust = playerShip->maxThrust;
            }
            if (thrustVal > 0) {
                if (targetThrust >= playerShip->thrust) {
                    thrustDelta = targetThrust - playerShip->thrust;
                    if (thrustDelta < PAL_SPEED(16)) {
                        thrust = thrustDelta;
                    } else {
                        thrust = PAL_SPEED(16);
                    }
                    playerShip->thrust += thrust;
                } else {
                    playerShip->thrust -= PAL_SPEED(16);
                }
            } else if (playerShip->thrust >= PAL_SPEED(16)) {
                playerShip->thrust -= PAL_SPEED(16);
            }
        } else {
            if (controller->buttons & CONTROL_THRUST) {
                playerShip->thrust += PAL_SPEED(16);
            } else if (playerShip->thrust >= PAL_SPEED(16)) {
                playerShip->thrust -= PAL_SPEED(16);
            }
            if (playerShip->thrust > playerShip->maxThrust) {
                playerShip->thrust = playerShip->maxThrust;
            }
        }
        /* Port: the pads' motors buzz only in the window the start boosts in - the throttle's amount, for
           the digital face buttons the original had an analogue throttle to judge it with - rising across
           it as it nears the edge where the start stalls, and silent everywhere else */
        if (shipNo == ownShip) {
            boostAt = (playerShip->maxThrust >> 3) * 5;
            stallAt = (playerShip->maxThrust >> 3) * 6;
            rumbleLevel = 0;
            if (playerShip->thrust >= boostAt && playerShip->thrust < stallAt) {
                rumbleLevel = 4 + (playerShip->thrust - boostAt) * 4 / (stallAt - boostAt);
            }
            PortPadRumbleLevel(rumbleLevel);
        }
    }
}


void SetUpFirstCheckPoint(void)
{
    TrackSection *sect;
    Face         *e;
    short         i;

    checkTimer = PAL_TIME(classCheckTime[tempGameData->shipClass]); /* the European table holds the same times in 50 Hz frames */
    sect = track->sections;
    while (sect->secNo != startLinePos[tempGameData->actTrackNum]) {
        sect = sect->nextSection.ptr;
    }
    e = track->faces + sect->faceStart;
    while ((e->flags & TRACK_BASE) == 0) {
        e++;
    }
    if (e->flags & CHECKPOINT) {
        i = 0;
        do {
            if (checkCells[tempGameData->trackNum][tempGameData->shipClass][i].section == sect->secNo) {
                checkCells[tempGameData->trackNum][tempGameData->shipClass][i].done = 1;
                checkTimer = checkTimer + checkCells[tempGameData->trackNum][tempGameData->shipClass][i].bonusSeconds * FRAMES_PER_SECOND;
            }
            i++;
        } while (i < 6);
    }
}


void UpdatePlayerShip(ShipData *ships, Object **shapes, int shipNo, WeaponData *weapons, ControlInput *control)
{
    int        pad[4];
    BrakeModes  modes;
    ShipData *ship;
    short       targetTurn, maxThrust, thrustVal, launchThrust;
    int         thrust, targetThrust;
    short       lower, upper;
    short       destroyed;

    if (PortAutopilot() && shipNo == ownShip && ships[shipNo].autopilotCount == 0) {
        /* Port: the lockstep tests' infinite autopilot, as the console recorder patches it in */
        ships[shipNo].autopilotCount = 120;
        ships[shipNo].weapon = 0;
        ships[shipNo].update = UpdateDroneShip;
        humanToDroneSpeed(ships, shipNo);
        return;
    }
    destroyed = 0;
    modes = brakeModes[0];
    ship = &ships[shipNo];
    if (shipTargetIndex[shipNo] != -1) {
        ShipTrkReaction(ship, shapes);
        return;
    }
    if (ship->combatAttr & DESTROYED) {
        return;
    }
    if (ship->electroCount > 0) {
        ship->electroCount--;
    } else if (ship->electroCount == 0) {
        ship->attr &= ~ELECTROED;
    }
    if (control->buttons & CONTROL_ANALOG_STEERING) {
        if (control->steering - twist + modes.value[currScreen] >= 127) {
            control->steering = 127 - modes.value[currScreen];
        } else if (control->steering - twist - modes.value[currScreen] < -126) {
            control->steering = modes.value[currScreen] - 127;
        } else {
            control->steering = control->steering - twist;
        }
        control->steering += 128;
        if (control->steering < 128 + modes.value[currScreen] && control->steering > 128 - modes.value[currScreen]) {
            control->steering = 128;
        } else if (control->steering >= 128 + modes.value[currScreen]) {
            control->steering -= modes.value[currScreen];
        } else {
            control->steering += modes.value[currScreen];
        }
        if (turnDivisor == 0) {
            turnDivisor = 1;
        }
        targetTurn = ((128 - control->steering) * (ship->maxHeading + 400) << 1) / turnDivisor;
        if (targetTurn > ship->vhdg) {
            if (targetTurn - ship->vhdg <= ship->headingInc) {
                ship->vhdg += targetTurn - ship->vhdg;
            } else if (ship->vhdg >= 0) {
                ship->vhdg += ship->headingInc;
            } else {
                ship->vhdg += ship->headingInc << 1;
            }
        } else if (targetTurn < ship->vhdg) {
            if (targetTurn - ship->vhdg >= -ship->headingInc) {
                ship->vhdg += targetTurn - ship->vhdg;
            }
            if (ship->vhdg <= 0) {
                ship->vhdg -= ship->headingInc;
            } else {
                ship->vhdg -= ship->headingInc << 1;
            }
        }
    } else if (control->buttons & CONTROL_LEFT) {
        if (ship->vhdg < 0) {
            ship->vhdg += ship->headingInc << 1;
        } else {
            ship->vhdg += ship->headingInc;
        }
    } else if (control->buttons & CONTROL_RIGHT) {
        if (ship->vhdg <= 0) {
            ship->vhdg -= ship->headingInc;
        } else {
            ship->vhdg -= ship->headingInc << 1;
        }
    } else {
        if (ship->vhdg > 0) {
            ship->vhdg -= ship->headingInc;
        } else if (ship->vhdg < 0) {
            ship->vhdg += ship->headingInc;
        }
    }
    if (ship->vhdg < -ship->maxHeading) {
        ship->vhdg = -ship->maxHeading;
    }
    if (ship->vhdg > ship->maxHeading) {
        ship->vhdg = ship->maxHeading;
    }
    if (ship->attr & ELECTROED) {
        if (shipNo == ownShip && (ship->attr & VIEW_IN)) {
            SetShake(2);
        }
        ship->vhdg += newRand() % 600 - 300;
    }
    if (control->buttons & CONTROL_DOWN) {
        ship->vpitch += PAL_SPEED(30);
    } else if (control->buttons & CONTROL_UP) {
        ship->vpitch -= PAL_SPEED(30);
    }
    if (ship->updateCount == PAL_TIME(90)) {
        launchThrust = ship->maxThrust >> 3;
        lower = launchThrust * 5;
        upper = launchThrust * 3;
        if (ship->thrust >= lower) {
            if (ship->thrust < upper << 1) {
                ship->thrustCount = 9;
                ship->thrust = ship->maxThrust;
                if (shipNo == ownShip) {
                    PlayNote(SFX_TURBO, 255, 15, 15, 256);
                }
            } else {
                ship->stallCount = 15;
                if (shipNo == ownShip) {
                    PlayNote(SFX_START_STALL, 255, 15, 15, 256);
                }
            }
        }
    }
    if (ship->thrustCount > 0) {
        ship->thrustCount--;
    }
    if (ship->updateCount > 0) {
        ship->updateCount--;
    }
    if (!(ship->combatAttr & 8)) {
        if (raceType == RACE_ONE_ON_ONE) {
            if (shipNo == ownShip) {
                if (playerPosition == 1) {
                    maxThrust = (unsigned int)(ship->maxThrust * 13) >> 4;
                } else {
                    maxThrust = ship->maxThrust;
                }
            } else if (shipNo == remoteShip) {
                if (remoteRacePos == 1) {
                    maxThrust = (unsigned int)(ship->maxThrust * 13) >> 4;
                } else {
                    maxThrust = ship->maxThrust;
                }
            }
        } else {
            maxThrust = ship->maxThrust;
        }
        if (control->buttons & CONTROL_ANALOG_THRUST) {
            thrustVal = control->thrust;
            targetThrust = maxThrust * thrustVal / 200;
            if (targetThrust > maxThrust) {
                targetThrust = maxThrust;
            }
            if (thrustVal > 0) {
                if (targetThrust >= ship->thrust) {
                    if (targetThrust - ship->thrust < PAL_SPEED(16)) {
                        thrust = targetThrust - ship->thrust;
                    } else {
                        thrust = PAL_SPEED(16);
                    }
                    ship->thrust += thrust;
                } else {
                    ship->thrust -= PAL_SPEED(16);
                }
            } else if (ship->thrust >= PAL_SPEED(16)) {
                ship->thrust -= PAL_SPEED(16);
            }
        } else {
            if (control->buttons & CONTROL_THRUST) {
                ship->thrust += PAL_SPEED(16);
            } else if (ship->thrust >= PAL_SPEED(8)) {
                ship->thrust -= PAL_SPEED(8);
            }
            if (ship->thrust > maxThrust) {
                ship->thrust = maxThrust;
            }
        }
    } else {
        ship->thrust -= PAL_SPEED(64);
        if (ship->thrust < 0) {
            ship->thrust = 0;
            if (ship->speed < 5000U) {
                destroyed = 1;
            }
        }
    }
    if ((ship->attr & ELECTROED) && newRand() % 80 == 0) {
        ship->thrust -= ship->thrust >> 2;
    }
    if (ship->stallCount > 0) {
        ship->stallCount--;
        ship->thrust -= ship->thrust >> 2;
    }
    SetBrake(ship, control);
    SetView(ship, control);
    SetFire(ship, weapons, shipNo, control);
    SetDitch(ship, weapons, shipNo, control);
    ShipTrkReaction(ship, shapes);
    if (destroyed) {
        shipTargetIndex[shipNo] = 100;
        ship->combatAttr |= 4;
        if (ship->attr & VIEW_IN) {
            ship->attr &= ~VIEW_IN;
            CameraUpdate = UpdateCameraRaceExternal;
        }
    }
}


void GeneralStartProcs(ShipData *s)
{
    if (s->updateCount == START_COUNTDOWN_FRAMES && !(s->combatAttr & REMOTE_SHIP)) {
        startCountdown = s->updateCount;
        countdownActive = 1;
    }
    s->updateCount--;
    if (!(s->combatAttr & REMOTE_SHIP)) {
        startCountdown--;
    }
    if (s->updateCount == 0) {
        PlayNote(0x1D, 0x1E0, 0xF, 0xF, 200);
        countdownActive = 0;
    }
    PortOriginalCount(s->updateCount); /* Port: an original Wipeout track's start gantries (portorig.c) */
    if (s->updateCount == COUNTDOWN_STEP_1) {
        PlayNote(0x1C, 0x1E0, 0xF, 0xF, 220);
    } else if (s->updateCount == COUNTDOWN_STEP_2) {
        PlayNote(0x1B, 0x1E0, 0xF, 0xF, 220);
    } else if (s->updateCount == COUNTDOWN_STEP_3) {
        PlayNote(0x1A, 0x1E0, 0xF, 0xF, 220);
    }
}
