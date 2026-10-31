/* Preserve steering angles when a time-trial take switches from digital input to the stick. */
#include "../../game/src/ghost.c"
#include <assert.h>
GhostReplay recordBuffer[1], replayBuffer[1];
ControlInput playerControl[1], ghostControl[4];
u_char classLaps[4] = {2, 3, 4, 5};
u_short shipClassIndex;
int ownShip, ghostShip = 1;
u16 ghostDrone;
void UpdateDroneShip(ShipData *ships, Object **shapes, int shipNo, WeaponData *weapons, ControlInput *control) { assert(0); }
void humanToDroneSpeed(ShipData *ships, int shipNo) { assert(0); }
int main(void)
{
    ShipData ships[2] = {0};
    recordBuffer->control = 1;
    playerControl->head = 1;
    playerControl->buttons = CONTROL_THRUST;
    RecordKeypresses(ships);
    playerControl->buttons = CONTROL_THRUST | CONTROL_LEFT;
    RecordKeypresses(ships);
    playerControl->buttons = CONTROL_THRUST | CONTROL_ANALOG_STEERING;
    playerControl->steering = 170;
    RecordKeypresses(ships);
    assert(recordBuffer->control == 3 && recordBuffer->keyCount == 6);
    assert(recordBuffer->keys[0] == CONTROL_THRUST && recordBuffer->keys[1] == 128);
    assert(recordBuffer->keys[2] == (CONTROL_THRUST | CONTROL_LEFT) && recordBuffer->keys[3] == 128);
    assert(recordBuffer->keys[5] == 170);
    playerControl->steering = 220;
    RecordKeypresses(ships);
    *replayBuffer = *recordBuffer;
    replayBuffer->keyCount = 0;
    ghostControl->head = replayBuffer->control;
    buildGhostReplayData(ships); assert(ghostControl->buttons == CONTROL_THRUST);
    buildGhostReplayData(ships); assert(ghostControl->buttons & CONTROL_LEFT);
    buildGhostReplayData(ships); assert(ghostControl->steering == 170);
    buildGhostReplayData(ships); assert(ghostControl->steering == 220);
    assert(ghostControl->buttons & CONTROL_ANALOG_STEERING);
    recordBuffer->control = 1;
    recordBuffer->keyCount = GHOST_KEYS / 2;
    RecordKeypresses(ships);
    assert(recordBuffer->disabled); /* never convert beyond the fixed recording buffer */
    puts("ghost recording preserves mixed digital and proportional steering: OK");
    return 0;
}
