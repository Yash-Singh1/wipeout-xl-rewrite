#include "functions.h"
#include "ghost.h"

/* No global here is $gp-relative, so all of them are extern. */
extern int          attractShip;
extern int          ghostShip;
extern int          ownShip;
extern ControlInput playerControl[];
extern ControlInput ghostControl[];
extern u8           classLaps[];
extern u16          shipClassIndex;
extern u16          ghostDrone;
extern u16          ghostButtons;
extern u16          ghostButtonsPressed;
extern u16          ghostButtonsPrev;
extern s8           ghostStickX;
extern s8           ghostStickY;
extern volatile s32 recordKeyCount;
extern s32          recordKeyTotal;
extern s16          recordControl;
extern s16          recordDisabled;
extern s16          recordMagic;
extern u8           recordTrackNum;
extern u8           recordShipClass;
extern s32          replayKeyCount;
extern s32          replayKeyTotal;
extern s16          replayControl;
extern s16          replayDisabled;
extern s16          replayMagic;
extern u8           replayTrackNum;
extern u8           replayShipClass;

typedef struct { s32 data[4]; } GhostChunk;
typedef struct { s32 data[60]; } GhostShip;

void InitGhostShip(ConfigData *config, ShipData *ships)
{
    GhostShip *dstShip;
    s32        srcEnd;
    void      *attrWord;
    void      *shipAddr;
    void      *srcAddr;
    s32        seed;
    s32        control;
    s32        ownNo;
    s32        recordAddr;
    s32        index;
    s32       *source, *destination, *end;

    seed = attractShip;
    control = playerControl[0].head;
    recordAddr = (s32) recordBuffer;
    ghostShip = 1;
    recordKeyCount = 0;
    recordKeyTotal = 0;
    recordDisabled = 0;
    recordMagic = GHOST_MAGIC;
    *((s32 *) recordAddr) = seed;
    recordControl = control;
    recordTrackNum = (u8)config->actTrackNum;
    ownNo = ownShip;
    dstShip = (GhostShip *) (((u8 *) recordAddr) + 0x14);
    shipAddr = (void *) ((s32) (u8)config->shipClass);
    srcEnd = (s32)ships + (ownNo * 0xF0);
    srcAddr = (void *) srcEnd;
    srcEnd = ((s32) srcAddr) + 0xF0;
    dstShip = (GhostShip *) (((u8 *) recordAddr) + 0x14);
    recordShipClass = (s32) shipAddr;
    *dstShip = *((GhostShip *) srcAddr);
    seed = (s32) replayBuffer;
    if (replayTrackNum == (u8)config->actTrackNum && replayShipClass == config->shipClass && replayDisabled == 0)
    {
        if (replayMagic == GHOST_MAGIC)
        {
            index = ghostShip;
            source = (s32 *)(seed + 20);
            end = (s32 *)(seed + 260);
            replayKeyCount = 0;
            seed = *((s32 *) seed);
            control = (u16)replayControl;
            srcAddr = (void *)((s32)ships + (index * 0xF0));
            attractShip = seed;
            ghostControl[0].head = control;
            destination = (s32 *)srcAddr;
            do {
                *(GhostChunk *)destination = *(GhostChunk *)source;
                source += 4; destination += 4;
            } while (source != end);
            shipAddr = (void *)((ghostShip * 0xF0) + (s32)ships);
            attrWord = (void *) ((s32) (*((u16 *) (((s8 *) shipAddr) + 0xAC))));
            ghostDrone = 0;
            *((u16 *) (((s8 *) shipAddr) + 0xAC)) = ((s32) attrWord) | 1;
            ghostButtonsPressed = 0;
            return;
        }
    }
    ghostDrone = 1;
    ((ShipData *)((ghostShip * 0xF0) + (s32)ships))->update = UpdateDroneShipStart;
    ((ShipData *)((ghostShip * 0xF0) + (s32)ships))->updateCount = 200;
    ((ShipData *)((ghostShip * 0xF0) + (s32)ships))->numToSpeed = 0;
}

void RecordKeypresses(ShipData *ships)
{
    GhostReplay *take;
    s32          count;

    take = recordBuffer;
    count = recordKeyCount;
    if (playerControl[0].head == 1) {
        if (count < GHOST_KEYS - 4 && ships[ownShip].lap <= classLaps[shipClassIndex]) {
            GHOST_KEY(take, count) = playerControl[0].buttons & 0xEFF6;
            recordKeyCount = recordKeyCount + 1;
        }
    } else if (playerControl[0].head == 2) {
        if (count < GHOST_KEYS - 8 && ships[ownShip].lap <= classLaps[shipClassIndex]) {
            GHOST_KEY(take, count) = playerControl[0].buttons & 0xEFF6;
            GHOST_KEY(take, count + 1) = playerControl[0].steering | (playerControl[0].thrust << 8);
            recordKeyCount = recordKeyCount + 2;
        }
    } else {
        recordDisabled = 1;
    }
    recordKeyTotal = recordKeyCount;
}

void buildGhostReplayData(ShipData *ships)
{
    GhostReplay *take;
    u16          buttons;
    u16          analogue;

    ghostButtonsPrev = ghostButtons;
    if (ghostDrone == 0) {
        take = replayBuffer;
        if (replayKeyCount < replayKeyTotal) {
            if (replayControl == 1) {
                buttons = GHOST_KEY(take, replayKeyCount);
                replayKeyCount += 1;
                ghostButtons = buttons;
            } else {
                ghostButtons = GHOST_KEY(take, replayKeyCount);
                analogue = GHOST_KEY(take, replayKeyCount + 1);
                replayKeyCount += 2;
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

void storeGhostTrail(void *unused, ConfigData *config)
{
    if (recordDisabled == 0 && (config->savedTrackNum & 2)) {
        replayBuffer[0] = recordBuffer[0];
    }
}
