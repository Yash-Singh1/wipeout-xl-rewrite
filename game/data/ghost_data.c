/* Global data of ghost.c, generated from the executable by tools/gendata.py. */
#include "functions.h"
#include "ghost.h"

u16 ghostDrone;
GhostReplay recordBuffer[1];
volatile s32 recordKeyCount;
s32 recordKeyTotal;
s16 recordControl;
short recordDisabled;
s16 recordMagic;
u8 recordTrackNum;
u8 recordShipClass;
GhostReplay replayBuffer[1];
s32 replayKeyCount;
s32 replayKeyTotal;
s16 replayControl;
s16 replayDisabled;
s16 replayMagic;
u8 replayTrackNum;
u8 replayShipClass;
ControlInput ghostControl[2];
u16 ghostButtons;
u16 ghostButtonsPressed;
u16 ghostButtonsPrev;
s8 ghostStickX;
s8 ghostStickY;
