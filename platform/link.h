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
    LINK_SLAVE_LINKED    /* linked: in the menus or racing */
};

/* The largest picture the slave hands over: 320x120 console pixels (the band of a split window) at
   eight times the console's resolution. */
#define LINK_FRAME_BYTES (2560u * 960u * 4u)

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

/* Master */
int PortLinkStartSlave(int windowWidth, int windowHeight);
void PortLinkStopSlave(void);
int PortLinkSlaveState(void);
void PortLinkGo(int secondShip);
void PortLinkSetInput(const LinkInput *input);
void PortLinkSetSplit(int split);
const uint8_t *PortLinkGetFrame(int *width, int *height, unsigned *sequence);

/* Slave */
int PortLinkSlaveInit(const char *sharedName);
void PortLinkSetSlaveState(int state);
int PortLinkGoSignal(int *secondShip);
void PortLinkClearGo(void);
int PortLinkQuitRequested(void);
int PortLinkGetInput(LinkInput *input);
void PortLinkPutFrame(const uint8_t *pixels, int width, int height);

/* Both */
int PortLinkSplit(void);
int PortLinkTransfer(const uint8_t *send, uint8_t *recv, int count);
void PortLinkShutdown(void);

#endif
