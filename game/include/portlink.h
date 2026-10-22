/* Port: two players on one machine (portlink.c, portlobby.c; platform/link.c). */
#ifndef PORTLINK_H
#define PORTLINK_H

#include "wipeout.h"
#include "menus.h"

/* platform/link.h */
enum { PORT_LINK_NONE, PORT_LINK_MASTER, PORT_LINK_SLAVE };
enum { PORT_LINK_SLAVE_NONE, PORT_LINK_SLAVE_BOOTING, PORT_LINK_SLAVE_WAITING, PORT_LINK_SLAVE_LINKED,
       PORT_LINK_SLAVE_OTHER_BUILD };
int PortLinkRole(void);
int PortLinkTransfer(const unsigned char *send, unsigned char *recv, int count);
void PortLinkSetSlaveState(int state);
int PortLinkSlaveState(void);
int PortLinkGoSignal(int *secondShip);
void PortLinkClearGo(void);
void PortLinkGo(int secondShip);
void PortLinkRestart(void);   /* the second player's game, started (again) */
void PortLinkStop(void);      /* ... and closed */
void PortLinkSetSplit(int split);

/* The input devices (platform/link.h): the keyboard (id 0) or a gamepad; kind: the platform's PadDevice
   (0 none, 1 keyboard, 2 PlayStation, 3 Xbox, 4 Nintendo, 5 other); buttons: the console buttons held. */
typedef struct PortInputDevice {
    int id;
    int kind;
    unsigned short buttons;
} PortInputDevice;
int PortInputDevices(PortInputDevice *out, int max);
int PortInputLastDevice(void);
void PortInputAssign(int first, int second);

/* portlink.c */
int PortLinkActive(void);
void PortLinkScreen(ConfigData *gameData, MenuSelection *selection);
int PortLinkMenuSelect(int screenCount);
void PortLinkMenuSelected(int from);
void PortLinkMenuCancelled(int from, int to);
void PortLinkMenusStart(void);
void PortLinkRaceView(int racing);
void killLink(void);

/* portlobby.c: the lobby in place of the link screen; returns 1 when both players are ready and the
   second player's game is waiting (the go is given) */
int PortLobby(ConfigData *gameData, MenuSelection *selection);
int PortLobbyRaceSelection(void);
void PortLobbyLeave(void);

#endif
