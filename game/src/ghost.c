#include "functions.h"
#include "ghost.h"
#include "hiscores.h"

/* No global here is $gp-relative, so all of them are extern. */
extern int          attractShip;
extern int          ghostShip;
extern int          ownShip;
extern ControlInput ghostControl[];
extern u16          ghostDrone;

void InitGhostShip(ConfigData *config, ShipData *ships)
{
    GhostReplay *record;
    GhostReplay *replay;
    s32          seed;
    s32          control;
    s32          index;

    record = recordBuffer;
    replay = replayBuffer;
    seed = attractShip;
    control = playerControl[0].head;
    ghostShip = 1;
    record->keyCount = 0;
    record->keyTotal = 0;
    record->disabled = 0;
    record->magic = GHOST_MAGIC;
    record->attractSeed = seed;
    record->control = control;
    record->trackNum = (u8)config->actTrackNum;
    record->shipClass = (u8)config->shipClass;
    memcpy(&record->ship, &ships[ownShip], sizeof(ShipData));
    if (replay->trackNum == (u8)config->actTrackNum && replay->shipClass == config->shipClass && replay->disabled == 0)
    {
        if (replay->magic == GHOST_MAGIC)
        {
            index = ghostShip;
            replay->keyCount = 0;
            attractShip = replay->attractSeed;
            control = (u16)replay->control;
            ghostControl[0].head = control;
            memcpy(&ships[index], &replay->ship, sizeof(ShipData));
            ghostDrone = 0;
            ships[ghostShip].combatAttr = ships[ghostShip].combatAttr | 1;
            ghostButtonsPressed = 0;
            return;
        }
    }
    ghostDrone = 1;
    ships[ghostShip].update = UpdateDroneShipStart;
    ships[ghostShip].updateCount = 200;
    ships[ghostShip].numToSpeed = 0;
}

void RecordKeypresses(ShipData *ships)
{
    GhostReplay *take;
    s32          count;

    take = recordBuffer;
    count = take->keyCount;
    if (playerControl[0].head == 1) {
        if (count < GHOST_KEYS - 4 && ships[ownShip].lap <= classLaps[shipClassIndex]) {
            take->keys[count] = playerControl[0].buttons & 0xEFF6;
            take->keyCount = take->keyCount + 1;
        }
    } else if (playerControl[0].head == 2) {
        if (count < GHOST_KEYS - 8 && ships[ownShip].lap <= classLaps[shipClassIndex]) {
            take->keys[count] = playerControl[0].buttons & 0xEFF6;
            take->keys[count + 1] = playerControl[0].steering | (playerControl[0].thrust << 8);
            take->keyCount = take->keyCount + 2;
        }
    } else {
        take->disabled = 1;
    }
    take->keyTotal = take->keyCount;
}

void buildGhostReplayData(ShipData *ships)
{
    GhostReplay *take;
    u16          buttons;
    u16          analogue;

    ghostButtonsPrev = ghostButtons;
    if (ghostDrone == 0) {
        take = replayBuffer;
        if (take->keyCount < take->keyTotal) {
            if (take->control == 1) {
                buttons = take->keys[take->keyCount];
                take->keyCount += 1;
                ghostButtons = buttons;
            } else {
                ghostButtons = take->keys[take->keyCount];
                analogue = take->keys[take->keyCount + 1];
                take->keyCount += 2;
                ghostStickX = analogue >> 8;
                ghostStickY = analogue;
            }
        } else {
            ghostDrone = 1;
            ships[ghostShip].numToSpeed = 0;
            ships[ghostShip].update = UpdateDroneShip;
            humanToDroneSpeed(ships, ghostShip);
        }
    }
    ghostButtonsPressed = ghostButtons & (ghostButtons ^ ghostButtonsPrev);
}

void storeGhostTrail(ConfigData *config, HiScoreData *score)
{
    if (recordBuffer->disabled == 0 && (score->flag & 2)) {
        replayBuffer[0] = recordBuffer[0];
    }
}
