#include "functions.h"

extern u_long *OT[];
extern u_long *OT2[];
extern short CurrentScreen;

void exit(int status);
void Error(char *string, short errorLevel)
{
    void *screenPrim[2];
    short i;

    if (errorLevel != NonFatal && errorLevel >= Fatal) {
        if (errorLevel == Fatal) {
            i = 0;
            do {
                ClearOTagR(OT[CurrentScreen], 0x2000);
                ClearOTagR(OT2[CurrentScreen], 0x640);
                AddPrim((char *)OT[CurrentScreen] + 0x7FFC, screenPrim[CurrentScreen]);
                Swap();
                i++;
            } while (i < 2);
            exit(0);
        }
    }
}
