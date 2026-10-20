/* Global data of animover.c, generated from the executable by tools/gendata.py. */
#include "functions.h"
#include "droid.h"

u_char cheatTime = 0;
u_char cheatMinigun = 0;
short cheatfade = 0;
int droidFlyin = 1000;
/* Port: holds 0x80100000, the first byte of console RAM above the program; only LoadProg (dead code) used it */
Header *rambase = 0;
Header progHeader[1];
