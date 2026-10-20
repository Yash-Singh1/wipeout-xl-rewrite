#include "functions.h"

extern short CurrentScreen;

void Error(char *string, short errorLevel)
{
    void *screenPrim[2]; /* never assigned: the original links these uninitialised stack words into the table */
    short i;

    if (errorLevel != NonFatal && errorLevel >= Fatal) {
        if (errorLevel == Fatal) {
            i = 0;
            do {
                ClearOTagR(OT[CurrentScreen], 0x2000);
                ClearOTagR(OT2[CurrentScreen], 0x640);
                AddPrim(OT[CurrentScreen] + 0x1FFF, screenPrim[CurrentScreen]); /* Port: the last of the 0x2000 entries (was a byte offset) */
                Swap();
                i++;
            } while (i < 2);
            exit(0);
        }
    }
}
