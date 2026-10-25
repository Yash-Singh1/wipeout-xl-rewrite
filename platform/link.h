/* Two players on one machine (link.c): the link cable between the game in the window and a second
   copy of it for the second player. */
#ifndef PORT_LINK_H
#define PORT_LINK_H

#include <stdint.h>

#include "psx/pad.h"

enum { LINK_NONE, LINK_MASTER, LINK_SLAVE };

/* What the slave is doing, as the master sees it. */
enum {
    LINK_SLAVE_NONE,     /* not running */
    LINK_SLAVE_BOOTING,  /* started, not yet at the link screen */
    LINK_SLAVE_WAITING,  /* at the link screen, waiting for the master's go */
    LINK_SLAVE_LINKED,   /* linked: in the menus or racing */
    LINK_SLAVE_OTHER_BUILD, /* refused: not the program the master runs (it was replaced since the master started) */
    LINK_SLAVE_HERE_NONE   /* none can run here: a web page or a phone's app starts no second program */
};

/* The largest picture the slave hands over: the lower half of a window up to 3840 pixels across. */
#define LINK_FRAME_BYTES (3840u * 1080u * 4u)

/* The second player's controller, as the master reads it. */
typedef struct LinkInput {
    int device;          /* PadDevice: the glyphs the slave shows */
    int keyboard;        /* the keyboard: keys[] holds the bound keys held; else pad is a gamepad's state */
    uint32_t keys;
    PadState pad;
} LinkInput;

/* An input device for the lobby: the keyboard (id 0) or a gamepad (its SDL id); kind is its PadDevice;
   buttons the console buttons it holds now, as the menus read them. */
typedef struct PortInputDevice {
    int id;
    int kind;
    unsigned short buttons;
} PortInputDevice;
int PortInputDevices(PortInputDevice *out, int max);
int PortInputLastDevice(void);
/* The lobby's assignment: the first player's device drives this game, the second player's goes over the
   link (-1: none; both -1: every device drives this game, as without a link). */
void PortInputAssign(int first, int second);

int PortLinkRole(void);
void PortLinkNoteProgram(const char *path); /* at start-up, before the program's file can change */
int PortLinkField(void);                    /* each field: alive; 1 while the second player's game starts up */

/* Master */
int PortLinkStartSlave(int windowWidth, int windowHeight);
void PortLinkStopSlave(void);
int PortLinkSlaveState(void);
void PortLinkGo(int secondShip);
void PortLinkSetInput(const LinkInput *input);
void PortLinkSetSplit(int split);
const uint8_t *PortLinkGetFrame(int *width, int *height, unsigned *sequence);
/* The split window's lower half, for the slave's band: the widescreen margin and its size in pixels. */
void PortLinkSetBand(int margin, int width, int height);
/* Whether this game draws pictures between the game's frames: the slave then hands such pictures over too, as
   often as this game shows the window (microseconds). */
void PortLinkSetInterpolate(int on);
void PortLinkSetPresentPeriod(unsigned microseconds);
/* What the slave's game asked of the second player's motors since the last call (pad.h's PAD_RUMBLE_*
   kinds, the game's units): 0 when it asked nothing of that kind. */
int PortLinkTakeRumble(int kind, int *level);

/* Slave */
int PortLinkSlaveInit(const char *sharedName);
void PortLinkSetSlaveState(int state);
int PortLinkGoSignal(int *secondShip);
void PortLinkClearGo(void);
int PortLinkQuitRequested(void);
int PortLinkGetInput(LinkInput *input);
void PortLinkPutFrame(const uint8_t *pixels, int width, int height);
int PortLinkBand(int *margin, int *width, int *height); /* 0 before the master has said */
int PortLinkInterpolate(void);  /* the master draws pictures between the game's frames */
unsigned PortLinkPresentPeriod(void); /* how often the master shows the window (microseconds; 0: not known) */
/* The second player's motors: what this game asks of them, for the master, whose window holds their
   controller, to run at its own setting. */
void PortLinkPutRumble(int kind, int level);

/* Both */
int PortLinkSplit(void);
int PortLinkTransfer(const uint8_t *send, uint8_t *recv, int count);
void PortLinkShutdown(void);

#endif
