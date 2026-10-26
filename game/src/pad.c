#include "functions.h"
#include "sfx.h"
#include "weapon.h"
#include "racectrl.h"
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

/* The cheat codes are a face button with L1, R1 and Select held. */
#define CHEAT_TRIANGLE (PAD_TRIANGLE | PAD_CHEAT_MODIFIERS)
#define CHEAT_CIRCLE   (PAD_CIRCLE | PAD_CHEAT_MODIFIERS)
#define CHEAT_CROSS    (PAD_CROSS | PAD_CHEAT_MODIFIERS)
#define CHEAT_SQUARE   (PAD_SQUARE | PAD_CHEAT_MODIFIERS)


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

    PortScriptGamePad((unsigned char *)pad); /* Port: scripted pad input for the lockstep tests (WIPEOUT_PADS) */
    prev = control->buttons;
    control->buttons = 0;
    control->previousButtons = prev;
    if (pad->header == 0x2300 || pad->header == 0x4100) {
        held = ~pad->buttons;
        idx = pad->header == 0x2300;
        keyDef[0] = keyDefineView[idx];
        keyDef[1] = keyDefineFire[idx];
        keyDef[2] = keyDefineDitch[idx];
        keyDef[3] = keyDefineLBrake[idx];
        keyDef[4] = keyDefineRBrake[idx];
        keyDef[5] = keyDefineThrustPad[idx];
        if (held & PAD_UP) control->buttons |= CONTROL_UP;
        if (held & PAD_DOWN) control->buttons |= CONTROL_DOWN;
        if (held & PAD_SELECT) control->buttons |= CONTROL_SELECT;
        if (held & PAD_START) control->buttons |= CONTROL_START;
        if (held & PAD_LEFT) control->buttons |= CONTROL_LEFT;
        if (held & PAD_RIGHT) control->buttons |= CONTROL_RIGHT;
        if (pad->header == 0x2300) {
            control->head = 2;
            buttons = control->buttons;
            control->steering = pad->steering;
            control->buttons = buttons | CONTROL_ANALOG_STEERING;
            if (keyDef[0] <= STICK_LIMIT) {
                if (held & keyDef[0]) control->buttons = buttons | (CONTROL_ANALOG_STEERING | CONTROL_VIEW);
            } else {
                switch (keyDef[0] & 3) {
                default:
                    if (pad->analog[0] >= 0x80U) control->buttons = buttons | (CONTROL_ANALOG_STEERING | CONTROL_VIEW);
                    break;
                case 1:
                    if (pad->analog[1] >= 0x80U) control->buttons = buttons | (CONTROL_ANALOG_STEERING | CONTROL_VIEW);
                    break;
                case 2:
                    if (pad->analog[2] >= 0x80U) control->buttons = buttons | (CONTROL_ANALOG_STEERING | CONTROL_VIEW);
                    break;
                }
            }
            if (keyDef[2] <= STICK_LIMIT) {
                if (held & keyDef[2]) control->buttons |= CONTROL_DITCH;
            } else {
                switch (keyDef[2] & 3) {
                default:
                    if (pad->analog[0] >= 0x80U) control->buttons |= CONTROL_DITCH;
                    break;
                case 1:
                    if (pad->analog[1] >= 0x80U) control->buttons |= CONTROL_DITCH;
                    break;
                case 2:
                    if (pad->analog[2] >= 0x80U) control->buttons |= CONTROL_DITCH;
                    break;
                }
            }
            if (keyDef[3] <= STICK_LIMIT) {
                if (held & keyDef[3]) control->buttons |= CONTROL_LEFT_BRAKE;
            } else {
                switch (keyDef[3] & 3) {
                default:
                    if (pad->analog[0] >= 0x80U) control->buttons |= CONTROL_LEFT_BRAKE;
                    break;
                case 1:
                    if (pad->analog[1] >= 0x80U) control->buttons |= CONTROL_LEFT_BRAKE;
                    break;
                case 2:
                    if (pad->analog[2] >= 0x80U) control->buttons |= CONTROL_LEFT_BRAKE;
                    break;
                }
            }
            if (keyDef[4] <= STICK_LIMIT) {
                if (held & keyDef[4]) control->buttons |= CONTROL_RIGHT_BRAKE;
            } else {
                switch (keyDef[4] & 3) {
                default:
                    if (pad->analog[0] >= 0x80U) control->buttons |= CONTROL_RIGHT_BRAKE;
                    break;
                case 1:
                    if (pad->analog[1] >= 0x80U) control->buttons |= CONTROL_RIGHT_BRAKE;
                    break;
                case 2:
                    if (pad->analog[2] >= 0x80U) control->buttons |= CONTROL_RIGHT_BRAKE;
                    break;
                }
            }
            if (keyDef[1] <= STICK_LIMIT) {
                if (held & keyDef[1]) control->buttons |= CONTROL_FIRE;
            } else {
                switch (keyDef[1] & 3) {
                default:
                    if (pad->analog[0] >= 0x80U) control->buttons |= CONTROL_FIRE;
                    break;
                case 1:
                    if (pad->analog[1] >= 0x80U) control->buttons |= CONTROL_FIRE;
                    break;
                case 2:
                    if (pad->analog[2] >= 0x80U) control->buttons |= CONTROL_FIRE;
                    break;
                }
            }
            if (keyDef[5] <= STICK_LIMIT) {
                if (held & keyDef[5]) control->buttons |= CONTROL_THRUST;
            } else {
                control->buttons |= CONTROL_ANALOG_THRUST;
                switch (keyDef[5] & 3) {
                default: control->thrust = pad->analog[0]; break;
                case 1: control->thrust = pad->analog[1]; break;
                case 2: control->thrust = pad->analog[2]; break;
                }
            }
        } else {
            if (held & keyDef[0]) control->buttons |= CONTROL_VIEW;
            if (held & keyDef[1]) control->buttons |= CONTROL_FIRE;
            if (held & keyDef[2]) control->buttons |= CONTROL_DITCH;
            if (held & keyDef[3]) control->buttons |= CONTROL_LEFT_BRAKE;
            if (held & keyDef[4]) control->buttons |= CONTROL_RIGHT_BRAKE;
            if (held & keyDef[5]) control->buttons |= CONTROL_THRUST;
            control->head = 1;
        }
    } else {
        control->head = 0;
        control->previousButtons = 0;
    }
    control->pressed = control->buttons & (control->buttons ^ control->previousButtons);
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
        if (held & PAD_UP) {
            keysDown = PAD_UP;
        }
        if (held & PAD_DOWN) {
            keysDown |= PAD_DOWN;
        }
        if (held & PAD_LEFT) {
            keysDown |= PAD_LEFT;
        }
        if (held & PAD_RIGHT) {
            keysDown |= PAD_RIGHT;
        }
        if (held & PAD_START) {
            keysDown |= PAD_START;
        }
        if (held & PAD_TRIANGLE) {
            keysDown |= PAD_TRIANGLE;
        }
        if (held & PAD_CIRCLE) {
            keysDown |= PAD_CIRCLE;
        }
        if (pad->header == 0x2300) {
            if (pad->analog[0] >= 0x80U) {
                keysDown |= PAD_CROSS;
            }
            if (pad->analog[1] >= 0x80U) {
                keysDown |= PAD_SQUARE;
            }
            if (pad->analog[2] >= 0x80U) {
                keysDown |= PAD_L1;
            }
            if (buttons & PAD_R1) {
                keysDown |= PAD_R1;
            }
            padType = 2;
        } else {
            if (buttons & PAD_CROSS) {
                keysDown |= PAD_CROSS;
            }
            if (buttons & PAD_SQUARE) {
                keysDown |= PAD_SQUARE;
            }
            if (buttons & PAD_L1) {
                keysDown |= PAD_L1;
            }
            if (buttons & PAD_R1) {
                keysDown |= PAD_R1;
            }
            if (buttons & PAD_L2) {
                keysDown |= PAD_L2;
            }
            if (buttons & PAD_R2) {
                keysDown |= PAD_R2;
            }
            if (buttons & PAD_SELECT) {
                keysDown |= PAD_SELECT;
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
    if (control->buttons & CONTROL_RIGHT_BRAKE) {
        ship->r_brake_rot += PAL_SPEED(0x20);
    } else if (ship->r_brake_rot > 0) {
        ship->r_brake_rot -= PAL_SPEED(0x20);
    }
    if (ship->r_brake_rot < 0) {
        ship->r_brake_rot = 0;
    }
    if (ship->r_brake_rot >= 0x101) {
        ship->r_brake_rot = 0x100;
    }
    if (control->buttons & CONTROL_LEFT_BRAKE) {
        ship->l_brake_rot += PAL_SPEED(0x20);
    } else if (ship->l_brake_rot > 0) {
        ship->l_brake_rot -= PAL_SPEED(0x20);
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
    if ((control->pressed & CONTROL_VIEW) && !(ship->combatAttr & REMOTE_SHIP)) {
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
    if (((control->pressed & CONTROL_FIRE) && (ship->weapon != WEAPON_NONE)) || ((control->buttons & CONTROL_FIRE) && (ship->weapon == WEAPON_MINIGUN))) {
        if (control->buttons & CONTROL_DOWN) {
            ship->rearFire = 1;
        } else {
            ship->rearFire = 0;
        }
        if (!(ship->attr & SHIELDED)) {
            FireWeapon(ship, weapons, shipNo, (int)ship->weapon);
            if (cheatWeapon != 0) {
                GetCheatWeapon(shipIndex, ownShip);
            }
        } else if (!(ship->combatAttr & REMOTE_SHIP)) {
            PlayNote(8, 0xFF, 0xF, 0xF, 0x100);
        }
    }
}


void SetDitch(ShipData *ship, WeaponData *weapons, int shipNo, ControlInput *control)
{
    if (control->pressed & CONTROL_DITCH) {
        if (ship->weapon != WEAPON_NONE) {
            if (!(ship->combatAttr & REMOTE_SHIP)) {
                PlayNote(SFX_WEAPON_DITCH, 0xFF, 0xF, 0xF, 0x100);
            }
            ship->weapon = WEAPON_NONE;
        }
        if (cheatWeapon != 0) {
            GetCheatWeapon(shipIndex, ownShip);
        }
    }
}


short GetMenuKey(short action)
{
    if ((keysDown & PAD_CHEAT_MODIFIERS) == PAD_CHEAT_MODIFIERS) {
        return 0;
    }
    switch (action) {
    case MENU_KEY_UP: return keysPressed & PAD_UP;
    case MENU_KEY_DOWN: return keysPressed & PAD_DOWN;
    case MENU_KEY_LEFT: return keysPressed & PAD_LEFT;
    case MENU_KEY_RIGHT: return keysPressed & PAD_RIGHT;
    case MENU_KEY_BACK: return keysPressed & PAD_TRIANGLE;
    case MENU_KEY_SELECT: return keysPressed & PAD_CROSS;
    case MENU_KEY_START: return keysPressed & PAD_START;
    case MENU_KEY_PAD_SELECT: return keysPressed & PAD_SELECT;
    case MENU_KEY_CIRCLE: return keysPressed & PAD_CIRCLE;
    case MENU_KEY_SQUARE: return keysPressed & PAD_SQUARE;
    }
    return 0;
}


void UpdateCheatSequence(int inMenu)
{
    int i;
    u_char toggled;

    if (keysPressed != 0) {
        for (i = 15; i > 0; i--) {
            cheatHistory[i] = cheatHistory[i - 1];
        }
        cheatHistory[0] = (int)keysPressed;
        if ((keysDown & PAD_CHEAT_MODIFIERS) == PAD_CHEAT_MODIFIERS) {
            cheatHistory[0] = keysPressed | PAD_CHEAT_MODIFIERS;
        }
        if (!(inMenu & 0xFF) && (gameType == GAME_SINGLE_PLAYER) && (raceType != RACE_TIME_TRIAL)) {
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
                if ((cheatHistory[6] == CHEAT_SQUARE) && (cheatHistory[5] == CHEAT_CIRCLE) && (cheatHistory[4] == CHEAT_CROSS) && (cheatHistory[3] == cheatHistory[7]) && (cheatHistory[2] == cheatHistory[6]) && (cheatHistory[1] == cheatHistory[5]) && (cheatHistory[0] == cheatHistory[4])) {
                    toggled = cheatTime ^ 1;
                    cheatTime = toggled;
                    if (toggled != 0) {
                        checkTimer = 0x3E7;
                    }
                    cheatfade = 0xFF;
                }
            }
            if ((cheatHistory[6] == CHEAT_SQUARE) && (cheatHistory[5] == CHEAT_CIRCLE) && (cheatHistory[4] == CHEAT_CROSS) && (cheatHistory[3] == cheatHistory[6]) && (cheatHistory[2] == cheatHistory[5]) && (cheatHistory[1] == cheatHistory[4]) && (cheatHistory[0] == CHEAT_TRIANGLE)) {
                cheatfade = 0xFF;
                cheatMinigun ^= 1;
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
