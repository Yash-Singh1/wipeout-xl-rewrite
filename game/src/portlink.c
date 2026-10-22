/* Port: two players on one machine (see platform/link.c).

   The console's two-player modes (ARCADE LINK and ONE ON ONE) link two PlayStations with a cable.
   The port runs a second copy of the game for the second player and links the two through shared
   memory: the game's own link code runs as it did, each copy racing its own ship and drawing its
   own view. In place of the console's link screen (menu 19, EstablishLink) the first player's game
   shows the lobby (portlobby.c), where the second player joins with their own controller and both
   choose their ships. When both are ready the two copies link; the first player then chooses the
   class and track and the race starts, the window split between the two views. The second player's
   game is driven from here: it starts on the link screen, waits for the go, and presses START. */
#include "functions.h"
#include "menus.h"
#include "portlink.h"

extern int    currMenu;
extern MenuEntry *menu;
extern s16    linked;
extern s16    linkState;
extern int    raceType;
extern int    ownShip;

/* The first player's way through the menus once linked: LINK_CHOOSING, in the class and track menus (the
   lobby leads straight there); LINK_TO_START, a track was chosen: START on the main menu. */
enum { LINK_OFF, LINK_CHOOSING, LINK_TO_START, LINK_STARTED };
static int stage;

int PortLinkActive(void)
{
    return PortLinkRole() != 0;
}

void PortLinkScreen(ConfigData *gameData, MenuSelection *selection)
{
    int secondShip;

    if (PortLinkRole() == PORT_LINK_SLAVE) {
        /* the second player's game: wait at the link screen for the lobby's go */
        if (!linked) {
            PortLinkSetSlaveState(PORT_LINK_SLAVE_WAITING);
            if (PortLinkGoSignal(&secondShip)) {
                PortLinkClearGo();
                selection->ship = (char)secondShip;
                ownShip = secondShip;
                gameType = 1;
                linkState = 5;
                linked = 1;
                PortLinkSetSlaveState(PORT_LINK_SLAVE_LINKED);
            }
        }
        return;
    }
    if (!linked && PortLobby(gameData, selection)) {
        gameType = 0;
        linkState = 5;
        linked = 1;
        /* straight on to the class and track (one change of screen, not two through the main menu) */
        menu[19].nextMenu = 2;
        stage = LINK_CHOOSING;
    }
}

int PortLinkStartsOnLinkScreen(void)
{
    return PortLinkRole() == PORT_LINK_SLAVE;
}

/* Called every menu frame before the menus read the select key: whether to act as if it was
   pressed (choosing the entry set here). */
int PortLinkMenuSelect(int screenCount)
{
    if (!linked || screenCount) {
        return 0;
    }
    if (PortLinkRole() == PORT_LINK_SLAVE) {
        if (currMenu == 0) {
            menu[0].selection = 3; /* START: the second player waits for the first */
            return 1;
        }
        return 0;
    }
    switch (stage) {
    case LINK_TO_START:
        if (currMenu == 0) {
            menu[0].selection = 3; /* START */
            stage = LINK_STARTED;
            return 1;
        }
        return 0;
    default:
        return 0;
    }
}

/* The menus have moved on (select) from menu `from`. */
void PortLinkMenuSelected(int from)
{
    if (stage == LINK_CHOOSING && (from == 2 || (from >= 5 && from <= 8))) {
        stage = LINK_TO_START; /* the racing class screen (with its track) or a track menu: chosen */
    }
}

/* The player backed out of menu `from` to `to`: out of the class and track menus to the main menu is
   back to the lobby (the link is closed; the second player's game starts again). */
void PortLinkMenuCancelled(int from, int to)
{
    if (stage == LINK_CHOOSING && to == 0 && from == 2) {
        stage = LINK_OFF;
        killLink();
        PortLinkRestart();
        currMenu = 19;
    }
}

/* A linked race is over or the menus start afresh: back to the lobby. */
void PortLinkMenusStart(void)
{
    if (PortLinkRole() == PORT_LINK_SLAVE) {
        currMenu = 19;
        menu[1].selection = 3;
        raceType = 3;
        return;
    }
    if (stage != LINK_OFF) {
        stage = LINK_OFF;
        killLink();
        PortLinkRestart();
        currMenu = 19;
        menu[1].selection = PortLobbyRaceSelection();
        raceType = menu[1].selection;
    }
}

/* A race starts (1) or ends (0): a linked race splits the window between the two players. */
void PortLinkRaceView(int racing)
{
    if (PortLinkRole() == PORT_LINK_MASTER) {
        PortLinkSetSplit(racing && linked);
    }
}
