/* Global data of ghost.c, generated from the executable by tools/gendata.py. */
#include "functions.h"
#include "ghost.h"

u16 ghostDrone;
/* Port: the take header holds words (recordKeyCount...); the executable has it word-aligned */
GhostReplay recordBuffer[1] __attribute__((aligned(4)));
/* Port: the take header holds words (replayKeyCount...); the executable has it word-aligned */
GhostReplay replayBuffer[1] __attribute__((aligned(4)));
ControlInput ghostControl[1];
