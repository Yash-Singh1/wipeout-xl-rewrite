#include "functions.h"
#include "../../platform/trace.h" /* Port: lockstep test hooks */
#include "ships.h"
#include "pad.h"
#include "menus.h"
#include "include_asm_g8.h"

/* $gp-relative globals are tentative definitions; the rest are extern. */
extern u_char cheatWeapon;
extern u_char cheatDamage;
extern u_char cheatTime;
extern u_char cheatMinigun;
extern short  cheatfade;

/* Pad state: the keys held now and last frame, and the pad type. */
extern u_short keysDown;
extern u_short keysPrev;

/* Button masks of the default pad layout (PlayStation pad bits). */
extern u_short buttonCross;
extern u_short buttonTriangle;
extern u_short buttonCircle;
extern u_short buttonSquare;
extern u_short buttonL2;
extern u_short buttonR2;
extern u_short buttonUp;
extern u_short buttonDown;
extern u_short buttonLeft;
extern u_short buttonRight;

/* Default bindings of each game action: [0] for a digital pad, [1] for an analog pad.
   A binding above 0x8FEF is an analog stick direction (0x8FF0 | axis) instead of a button. */
extern u_short keyDefineThrustPad[2];
extern u_short keyDefineFire[2];
extern u_short keyDefineView[2];
extern u_short keyDefineDitch[2];
extern u_short keyDefineLBrake[2];
extern u_short keyDefineRBrake[2];

/* The bindings in use, indexed by action: view, fire, ditch, left brake, right brake, thrust. */
extern u_short keyDef[6];

extern struct PadPacket padBuf1[];
extern struct PadPacket padBuf2[];
extern int     cheatHistory[];
extern int     ownShip;
extern ShipData *shipIndex;
extern int     raceType;
extern int     checkTimer;
extern MenuEntry *menu;

/* The cheat codes are a face button with L1, R1 and Select held (0xC01). */
#define CHEAT_TRIANGLE 0x1C01
#define CHEAT_CIRCLE   0x2C01
#define CHEAT_CROSS    0x4C01
#define CHEAT_SQUARE   0x8C01


void InitPadSettings(void)
{
    buttonCross = 0x4000;
    buttonTriangle = 0x1000;
    buttonCircle = 0x2000;
    buttonSquare = 0x8000;
    buttonR2 = 0x200;
    buttonL2 = 0x100;
    buttonUp = 0x10;
    buttonDown = 0x40;
    buttonLeft = 0x80;
    buttonRight = 0x20;
    keyDefineFire[1] = 0x8FF1;
    keyDefineLBrake[1] = 0x800;
    keyDefineView[0] = 0x1000;
    keyDefineFire[0] = 0x2000;
    keyDefineDitch[0] = 0x8000;
    keyDefineLBrake[0] = 0x200;
    keyDefineRBrake[0] = 0x100;
    keyDefineThrustPad[0] = 0x4000;
    keyDefineView[1] = 0x1000;
    keyDefineRBrake[1] = 0x8FF2;
    keyDefineDitch[1] = 0x2000;
    keyDefineThrustPad[1] = 0x8FF0;
}


/* Bindings above this are analog stick directions (0x8FF0 | axis), not buttons. */
#define STICK_LIMIT 0x8FEF

void buildGameKeyPad(struct PadPacket *pad, ControlInput *control)
{
    u_short held;
    u_short buttons;
    u_short prev;
    u_short idx;

    prev = control->buttons;
    control->buttons = 0;
    *(u_short *)control->pad6 = prev;
    if (pad->header == 0x2300 || pad->header == 0x4100) {
        held = ~pad->buttons;
        idx = pad->header == 0x2300;
        keyDef[0] = keyDefineView[idx];
        keyDef[1] = keyDefineFire[idx];
        keyDef[2] = keyDefineDitch[idx];
        keyDef[3] = keyDefineLBrake[idx];
        keyDef[4] = keyDefineRBrake[idx];
        keyDef[5] = keyDefineThrustPad[idx];
        if (held & 0x10) control->buttons |= 0x10;
        if (held & 0x40) control->buttons |= 0x40;
        if (held & 0x1) control->buttons |= 0x1;
        if (held & 0x8) control->buttons |= 0x8;
        if (held & 0x80) control->buttons |= 0x80;
        if (held & 0x20) control->buttons |= 0x20;
        if (pad->header == 0x2300) {
            control->head = 2;
            buttons = control->buttons;
            control->steering = pad->steering;
            control->buttons = buttons | 4;
            if (keyDef[0] <= STICK_LIMIT) {
                if (held & keyDef[0]) control->buttons = buttons | 0x1004;
            } else {
                switch (keyDef[0] & 3) {
                default:
                    if (pad->analog[0] >= 0x80U) control->buttons = buttons | 0x1004;
                    break;
                case 1:
                    if (pad->analog[1] >= 0x80U) control->buttons = buttons | 0x1004;
                    break;
                case 2:
                    if (pad->analog[2] >= 0x80U) control->buttons = buttons | 0x1004;
                    break;
                }
            }
            if (keyDef[2] <= STICK_LIMIT) {
                if (held & keyDef[2]) control->buttons |= 0x8000;
            } else {
                switch (keyDef[2] & 3) {
                default:
                    if (pad->analog[0] >= 0x80U) control->buttons |= 0x8000;
                    break;
                case 1:
                    if (pad->analog[1] >= 0x80U) control->buttons |= 0x8000;
                    break;
                case 2:
                    if (pad->analog[2] >= 0x80U) control->buttons |= 0x8000;
                    break;
                }
            }
            if (keyDef[3] <= STICK_LIMIT) {
                if (held & keyDef[3]) control->buttons |= 0x400;
            } else {
                switch (keyDef[3] & 3) {
                default:
                    if (pad->analog[0] >= 0x80U) control->buttons |= 0x400;
                    break;
                case 1:
                    if (pad->analog[1] >= 0x80U) control->buttons |= 0x400;
                    break;
                case 2:
                    if (pad->analog[2] >= 0x80U) control->buttons |= 0x400;
                    break;
                }
            }
            if (keyDef[4] <= STICK_LIMIT) {
                if (held & keyDef[4]) control->buttons |= 0x800;
            } else {
                switch (keyDef[4] & 3) {
                default:
                    if (pad->analog[0] >= 0x80U) control->buttons |= 0x800;
                    break;
                case 1:
                    if (pad->analog[1] >= 0x80U) control->buttons |= 0x800;
                    break;
                case 2:
                    if (pad->analog[2] >= 0x80U) control->buttons |= 0x800;
                    break;
                }
            }
            if (keyDef[1] <= STICK_LIMIT) {
                if (held & keyDef[1]) control->buttons |= 0x2000;
            } else {
                switch (keyDef[1] & 3) {
                default:
                    if (pad->analog[0] >= 0x80U) control->buttons |= 0x2000;
                    break;
                case 1:
                    if (pad->analog[1] >= 0x80U) control->buttons |= 0x2000;
                    break;
                case 2:
                    if (pad->analog[2] >= 0x80U) control->buttons |= 0x2000;
                    break;
                }
            }
            if (keyDef[5] <= STICK_LIMIT) {
                if (held & keyDef[5]) control->buttons |= 0x4000;
            } else {
                control->buttons |= 2;
                switch (keyDef[5] & 3) {
                default: control->thrust = pad->analog[0]; break;
                case 1: control->thrust = pad->analog[1]; break;
                case 2: control->thrust = pad->analog[2]; break;
                }
            }
        } else {
            if (held & keyDef[0]) control->buttons |= 0x1000;
            if (held & keyDef[1]) control->buttons |= 0x2000;
            if (held & keyDef[2]) control->buttons |= 0x8000;
            if (held & keyDef[3]) control->buttons |= 0x400;
            if (held & keyDef[4]) control->buttons |= 0x800;
            if (held & keyDef[5]) control->buttons |= 0x4000;
            control->head = 1;
        }
    } else {
        control->head = 0;
        *(u_short *)control->pad6 = 0;
    }
    control->pressed = control->buttons & (control->buttons ^ *(u_short *)control->pad6);
}
#undef STICK_LIMIT


void buildFixedPad(struct PadPacket *pad)
{
    u_short prevKeys;
    u_short buttons;
    int     held;
    int     header;

    PortScriptFixedPad((unsigned char *)pad); /* Port: scripted pad input for the lockstep tests (WIPEOUT_PADS) */
    prevKeys = keysDown;
    header = pad->header;
    keysDown = 0;
    keysPrev = prevKeys;
    if (header == 0x2300 || header == 0x4100) {
        held = ~pad->buttons;
        buttons = held;
        if (held & 0x10) {
            keysDown = 0x10;
        }
        if (held & 0x40) {
            keysDown |= 0x40;
        }
        if (held & 128) {
            keysDown |= 0x80;
        }
        if (held & 32) {
            keysDown |= 0x20;
        }
        if (held & 8) {
            keysDown |= 8;
        }
        if (held & 4096) {
            keysDown |= 0x1000;
        }
        if (held & 8192) {
            keysDown |= 0x2000;
        }
        if (pad->header == 0x2300) {
            if (pad->analog[0] >= 0x80U) {
                keysDown |= 0x4000;
            }
            if (pad->analog[1] >= 0x80U) {
                keysDown |= 0x8000;
            }
            if (pad->analog[2] >= 0x80U) {
                keysDown |= 0x400;
            }
            if (buttons & 2048) {
                keysDown |= 0x800;
            }
            padType = 2;
        } else {
            if (buttons & 0x4000) {
                keysDown |= 0x4000;
            }
            if (buttons & 32768) {
                keysDown |= 0x8000;
            }
            if (buttons & 1024) {
                keysDown |= 0x400;
            }
            if (buttons & 2048) {
                keysDown |= 0x800;
            }
            if (buttons & 256) {
                keysDown |= 0x100;
            }
            if (buttons & 512) {
                keysDown |= 0x200;
            }
            if (buttons & 1) {
                keysDown |= 1;
            }
            padType = 1;
        }
    } else {
        padType = 0;
        keysPrev = 0;
    }
    keysPressed = keysDown & (keysDown ^ keysPrev);
}


void SetBrake(ShipData *ship, ControlInput *control)
{
    if (control->buttons & 0x800) {
        ship->r_brake_rot += 0x20;
    } else if (ship->r_brake_rot > 0) {
        ship->r_brake_rot -= 0x20;
    }
    if (ship->r_brake_rot < 0) {
        ship->r_brake_rot = 0;
    }
    if (ship->r_brake_rot >= 0x101) {
        ship->r_brake_rot = 0x100;
    }
    if (control->buttons & 0x400) {
        ship->l_brake_rot += 0x20;
    } else if (ship->l_brake_rot > 0) {
        ship->l_brake_rot -= 0x20;
    }
    if (ship->l_brake_rot < 0) {
        ship->l_brake_rot = 0;
    }
    if (ship->l_brake_rot >= 0x101) {
        ship->l_brake_rot = 0x100;
    }
}


void SetView(ShipData *ship, ControlInput *control)
{
    if ((control->pressed & 0x1000) && !(ship->combatAttr & 2)) {
        if (!(ship->attr & VIEW_IN)) {
            CameraUpdate = UpdateCameraRaceInternal;
            ship->attr |= VIEW_IN;
        } else {
            CameraUpdate = UpdateCameraRaceExternal;
            ship->attr &= ~VIEW_IN;
        }
    }
}


void SetFire(ShipData *ship, WeaponData *weapons, int shipNo, ControlInput *control)
{
    if (((control->pressed & 0x2000) && (ship->weapon != 0)) || ((control->buttons & 0x2000) && (ship->weapon == 0xF))) {
        if (control->buttons & 0x40) {
            ship->rearFire = 1;
        } else {
            ship->rearFire = 0;
        }
        if (!(ship->attr & SHIELDED)) {
            FireWeapon(ship, weapons, shipNo, (int)ship->weapon);
            if (cheatWeapon != 0) {
                GetCheatWeapon(shipIndex, ownShip);
            }
        } else if (!(ship->combatAttr & 2)) {
            PlayNote(8, 0xFF, 0xF, 0xF, 0x100);
        }
    }
}


void SetDitch(ShipData *ship, WeaponData *weapons, int shipNo, ControlInput *control)
{
    if (control->pressed & 0x8000) {
        if (ship->weapon != 0) {
            if (!(ship->combatAttr & 2)) {
                PlayNote(0x2E, 0xFF, 0xF, 0xF, 0x100);
            }
            ship->weapon = 0U;
        }
        if (cheatWeapon != 0) {
            GetCheatWeapon(shipIndex, ownShip);
        }
    }
}


short GetMenuKey(short a)
{
    if ((keysDown & 0xC01) == 0xC01) {
        return 0;
    }
    switch (a) {
    case 0: return keysPressed & 0x10;
    case 1: return keysPressed & 0x40;
    case 2: return keysPressed & 0x80;
    case 3: return keysPressed & 0x20;
    case 4: return keysPressed & 0x1000;
    case 5: return keysPressed & 0x4000;
    case 6: return keysPressed & 0x8;
    case 7: return keysPressed & 0x1;
    case 8: return keysPressed & 0x2000;
    case 9: return keysPressed & 0x8000;
    }
    return 0;
}


void UpdateCheatSequence(int arg0)
{
    int i;
    u_char toggled;

    if (keysPressed != 0) {
        for (i = 15; i > 0; i--) {
            cheatHistory[i] = cheatHistory[i - 1];
        }
        cheatHistory[0] = (int)keysPressed;
        if ((keysDown & 0xC01) == 0xC01) {
            cheatHistory[0] = keysPressed | 0xC01;
        }
        if (!(arg0 & 0xFF) && (gameType == 2) && (raceType != 1)) {
            if ((cheatHistory[6] == CHEAT_CROSS) && (cheatHistory[5] == cheatHistory[6]) && (cheatHistory[4] == CHEAT_SQUARE) && (cheatHistory[3] == cheatHistory[4]) && (cheatHistory[2] == CHEAT_CIRCLE) && (cheatHistory[1] == cheatHistory[2]) && (cheatHistory[0] == CHEAT_TRIANGLE)) {
                toggled = cheatWeapon ^ 1;
                cheatWeapon = toggled;
                if (toggled != 0) {
                    GetNewWeapon(shipIndex, ownShip);
                }
                cheatfade = 0xFF;
            }
            if (cheatHistory[7] == CHEAT_TRIANGLE) {
                if ((cheatHistory[6] == CHEAT_CROSS) && (cheatHistory[5] == CHEAT_SQUARE) && (cheatHistory[4] == CHEAT_CIRCLE) && (cheatHistory[3] == cheatHistory[7]) && (cheatHistory[2] == cheatHistory[6]) && (cheatHistory[1] == cheatHistory[5]) && (cheatHistory[0] == cheatHistory[4])) {
                    cheatfade = 0xFF;
                    cheatDamage ^= 1;
                }
                if (cheatHistory[7] == CHEAT_TRIANGLE) {
                    if (cheatHistory[6] == CHEAT_SQUARE) {
                        if ((cheatHistory[5] == CHEAT_CIRCLE) && (cheatHistory[4] == CHEAT_CROSS) && (cheatHistory[3] == cheatHistory[7]) && (cheatHistory[2] == cheatHistory[6]) && (cheatHistory[1] == cheatHistory[5]) && (cheatHistory[0] == cheatHistory[4])) {
                            toggled = cheatTime ^ 1;
                            cheatTime = toggled;
                            if (toggled != 0) {
                                checkTimer = 0x3E7;
                            }
                            cheatfade = 0xFF;
                        }
                        goto checkMinigun;
                    }
                } else {
                    goto checkMinigun;
                }
            } else {
checkMinigun:
                if ((cheatHistory[6] == CHEAT_SQUARE) && (cheatHistory[5] == CHEAT_CIRCLE) && (cheatHistory[4] == CHEAT_CROSS) && (cheatHistory[3] == cheatHistory[6]) && (cheatHistory[2] == cheatHistory[5]) && (cheatHistory[1] == cheatHistory[4]) && (cheatHistory[0] == CHEAT_TRIANGLE)) {
                    cheatfade = 0xFF;
                    cheatMinigun ^= 1;
                }
            }
        } else {
            if ((cheatHistory[6] == CHEAT_CROSS) && (cheatHistory[5] == cheatHistory[6]) && (cheatHistory[4] == cheatHistory[5]) && (cheatHistory[3] == cheatHistory[4]) && (cheatHistory[2] == CHEAT_CIRCLE) && (cheatHistory[1] == CHEAT_TRIANGLE) && (cheatHistory[0] == CHEAT_SQUARE)) {
                tempGameData->teamCheatActive = 1;
            }
            if ((cheatHistory[5] == CHEAT_TRIANGLE) && (cheatHistory[4] == cheatHistory[5]) && (cheatHistory[3] == cheatHistory[4]) && (cheatHistory[2] == CHEAT_CIRCLE) && (cheatHistory[1] == cheatHistory[2]) && (cheatHistory[0] == cheatHistory[1])) {
                tempGameData->phantomTrackCheatActive = 1;
                menu[2].options[3].enabled = 1;
            }
            if ((cheatHistory[4] == CHEAT_SQUARE) && (cheatHistory[3] == CHEAT_CIRCLE) && (cheatHistory[2] == CHEAT_TRIANGLE) && (cheatHistory[1] == cheatHistory[3]) && (cheatHistory[0] == cheatHistory[4])) {
                tempGameData->trackCheatActive = 1;
                menu[2].options[3].enabled = 1;
            }
        }
    }
}


void InitController(void)
{
    InitPAD(padBuf1, 8, padBuf2, 8);
    StartPAD();
    ChangeClearPAD(0);
    VSync(0);
}
