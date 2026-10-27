#ifndef PAD_H
#define PAD_H

/* Physical pad buttons, before game-action bindings. */
#define PAD_SELECT           0x0001
#define PAD_START            0x0008
#define PAD_UP               0x0010
#define PAD_RIGHT            0x0020
#define PAD_DOWN             0x0040
#define PAD_LEFT             0x0080
#define PAD_L2               0x0100
#define PAD_R2               0x0200
#define PAD_L1               0x0400
#define PAD_R1               0x0800
#define PAD_TRIANGLE         0x1000
#define PAD_CIRCLE           0x2000
#define PAD_CROSS            0x4000
#define PAD_SQUARE           0x8000
#define PAD_CHEAT_MODIFIERS (PAD_SELECT | PAD_L1 | PAD_R1)

#define MENU_KEY_UP          0
#define MENU_KEY_DOWN        1
#define MENU_KEY_LEFT        2
#define MENU_KEY_RIGHT       3
#define MENU_KEY_BACK        4
#define MENU_KEY_SELECT      5
#define MENU_KEY_START       6
#define MENU_KEY_PAD_SELECT  7
#define MENU_KEY_CIRCLE      8
#define MENU_KEY_SQUARE      9

#include "wipeout.h"
#include "control.h"

extern u_short keysDown;
extern u_short keysPrev;
extern u_short keysPressed;
extern u_short padType;

/* A controller packet as the pad driver reports it: the pad type (0x4100 digital,
   0x2300 analog), the buttons (active low), then the analog sticks. */
typedef struct PadPacket {
    u_short header;
    u_short buttons;
    u_char  steering;
    u_char  analog[3];
} PadPacket;

/* The controller packets the pad driver fills (one per port). */
extern PadPacket padBuf1[];

#endif
