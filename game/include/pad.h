#ifndef PAD_H
#define PAD_H

#include "wipeout.h"
#include "control.h"

/* A controller packet as the pad driver reports it: the pad type (0x4100 digital,
   0x2300 analog), the buttons (active low), then the analog sticks. */
typedef struct PadPacket {
    u_short header;
    u_short buttons;
    u_char  steering;
    u_char  analog[3];
} PadPacket;

#endif
