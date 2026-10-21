/* Port: the lobby of the two-player modes, in place of the console's link screen (see portlink.c).

   The left half is the first player's: the controller they used last, and their team's ship, which
   LEFT and RIGHT change. The right half asks for a second player: START (or ENTER on the keyboard) on
   any other controller joins it, and that controller is the second player's from then on, choosing a
   team the same way. Each player confirms with their accept button (ENTER on the keyboard) and can
   take it back; the second player leaves with their back button, the first leaves the lobby with
   theirs (as from any menu). When both are ready, and the second player's game is up, the two games
   link and the first player goes on to choose the class and track. */
#include "functions.h"
#include "menus.h"
#include "portlink.h"

#define BUTTON_START    0x0008
#define BUTTON_RIGHT    0x0020
#define BUTTON_LEFT     0x0080
#define BUTTON_TRIANGLE 0x1000
#define BUTTON_CROSS    0x4000

#define PILOTS_PER_TEAM 3
#define DEVICE_KEYBOARD 0
#define DEVICE_NONE     (-1)
#define MAX_DEVICES     8

extern int       raceType;
extern short     CurrentScreen;
extern int       heading;
extern TeamOrder teamShipGroup[];
extern char      teamNameFeisar[], teamNameAgSystems[], teamNameAuricom[], teamNameQirex[], teamNamePiranha[];

typedef struct LobbyPlayer {
    int device;  /* the controller (PortInputDevice id), DEVICE_NONE: not joined */
    int kind;    /* its make (PortInputDevice kind) */
    int team;    /* index in the team order */
    int ready;
} LobbyPlayer;

static LobbyPlayer players[2];
static int raceSelection = 3;
static int lastFrame = -100;
static unsigned short held[MAX_DEVICES];
static int heldIds[MAX_DEVICES];
static int blink;
static int session; /* players and teams kept from the last time (after a race) */

int PortLobbyRaceSelection(void)
{
    return raceSelection;
}

static int TeamCount(ConfigData *gameData)
{
    return gameData->class2Unlocked || gameData->teamCheatActive ? 5 : 4;
}

static int TeamShip(int team)
{
    return PILOTS_PER_TEAM * teamShipGroup[0].order[team];
}

static char *TeamName(int team)
{
    switch (teamShipGroup[0].order[team]) {
    case 0: return teamNameFeisar;
    case 1: return teamNameAgSystems;
    case 2: return teamNameAuricom;
    case 3: return teamNameQirex;
    default: return teamNamePiranha;
    }
}

static int TeamOfShip(int ship, int count)
{
    int team;

    for (team = 0; team < count; team++) {
        if (TeamShip(team) == ship - ship % PILOTS_PER_TEAM) {
            return team;
        }
    }
    return 0;
}

static u_char *DeviceName(int kind)
{
    switch (kind) {
    case 1: return (u_char *)"KEYBOARD";
    case 2: return (u_char *)"PLAYSTATION CONTROLLER";
    case 3: return (u_char *)"XBOX CONTROLLER";
    case 4: return (u_char *)"NINTENDO CONTROLLER";
    default: return (u_char *)"CONTROLLER";
    }
}

/* The name of the accept button on a device, for the prompts. */
static const char *AcceptName(int kind)
{
    switch (kind) {
    case 1: return "ENTER";
    case 3: return "A";
    case 4: return "B";
    case 2: return "CROSS";
    default: return "A";
    }
}

static const char *BackName(int kind)
{
    switch (kind) {
    case 1: return "ESC";
    case 3: return "B";
    case 4: return "A";
    case 2: return "TRIANGLE";
    default: return "B";
    }
}

/* The buttons newly pressed on a device since the last frame. */
static unsigned short Pressed(const PortInputDevice *device)
{
    int i, free = -1;

    for (i = 0; i < MAX_DEVICES; i++) {
        if (heldIds[i] == device->id && held[i] != 0xFFFF) {
            unsigned short pressed = device->buttons & (unsigned short)~held[i];
            held[i] = device->buttons;
            return pressed;
        }
        if (free < 0 && held[i] == 0xFFFF) {
            free = i;
        }
    }
    if (free >= 0) {
        heldIds[free] = device->id;
        held[free] = device->buttons; /* held when first seen: not a press */
    }
    return 0;
}

static void Start(ConfigData *gameData, MenuSelection *selection)
{
    int i, first = PortInputLastDevice(), count = TeamCount(gameData);

    for (i = 0; i < MAX_DEVICES; i++) {
        held[i] = 0xFFFF;
        heldIds[i] = DEVICE_NONE;
    }
    players[0].ready = 0;
    players[1].ready = 0;
    if (session) {
        /* back from a race: the same players and teams, to confirm again */
        PortInputAssign(players[0].device, players[1].device);
    } else {
        raceSelection = raceType == 2 ? 2 : 3;
        players[0].device = first;
        players[0].kind = 1;
        players[0].team = TeamOfShip((unsigned char)selection->ship, count);
        players[1].device = DEVICE_NONE;
        PortInputAssign(first, DEVICE_NONE);
        session = 1;
    }
    if (PortLinkSlaveState() == PORT_LINK_SLAVE_NONE) {
        PortLinkRestart(); /* the second player's game starts now, to be up when they join */
    }
}

static void ChangeTeam(LobbyPlayer *player, unsigned short pressed, int count)
{
    if (player->ready) {
        return;
    }
    if ((pressed & BUTTON_RIGHT) && player->team < count - 1) {
        player->team++;
        PlayNote(8, 255, 15, 15, 256);
    }
    if ((pressed & BUTTON_LEFT) && player->team > 0) {
        player->team--;
        PlayNote(8, 255, 15, 15, 256);
    }
}

static void Line(char *buffer, const char *a, const char *b, const char *c)
{
    sprintf(buffer, "%s%s%s", a, b, c);
}

/* The layout, in the menu box: the content between rows 84 and 178, the status strip from 182. */
#define HEADING_Y 88
#define DEVICE_Y  100
#define TEAM_Y    146
#define HINT_Y    158
#define STATUS_Y  168
#define STRIP_Y   186

static void DrawPlayer(const LobbyPlayer *player, int index, int centre, int shipX)
{
    char text[48];

    CentreText2((unsigned char *)(index ? "PLAYER TWO" : "PLAYER ONE"), centre, HEADING_Y, 8, 0);
    PrintCX(DeviceName(player->kind), centre, DEVICE_Y);
    DrawTeamShips(shipX, -150, 1150, heading, 0, shipShapeTable[highResTable[TeamShip(player->team)]]);
    CentreText2((unsigned char *)TeamName(player->team), centre, TEAM_Y, 8, 1);
    if (player->ready) {
        if (blink % 30 < 20) {
            CentreText2((unsigned char *)"READY", centre, STATUS_Y, 8, 0);
        }
        Line(text, BackName(player->kind), " TO CHANGE", "");
        PrintCX((u_char *)text, centre, STRIP_Y);
    } else {
        PrintCX((u_char *)"LEFT OR RIGHT FOR TEAM", centre, HINT_Y);
        Line(text, "PRESS ", AcceptName(player->kind), " WHEN READY");
        PrintCX((u_char *)text, centre, STATUS_Y);
        if (index) {
            Line(text, BackName(player->kind), " TO LEAVE", "");
            PrintCX((u_char *)text, centre, STRIP_Y);
        }
    }
}

/* The line between the halves. */
static void DrawDivider(void)
{
    LINE_F2 *line = (LINE_F2 *)nextPrim.ptr;

    SetLineF2(line);
    line->r0 = 0x80;
    line->g0 = 0x80;
    line->b0 = 0x80;
    line->x0 = 160;
    line->y0 = 86;
    line->x1 = 160;
    line->y1 = 176;
    AddPrim(OT[CurrentScreen], line);
    nextPrim.ptr += sizeof(LINE_F2);
}

int PortLobby(ConfigData *gameData, MenuSelection *selection)
{
    PortInputDevice devices[MAX_DEVICES];
    int count = TeamCount(gameData), n, i, frame = VSync(-1);
    int state = PortLinkSlaveState();

    if (frame - lastFrame > 8) {
        Start(gameData, selection);
    }
    lastFrame = frame;
    blink++;
    n = PortInputDevices(devices, MAX_DEVICES);
    {
        /* a controller unplugged: the second player leaves, the first falls back to the keyboard */
        int firstFound = 0, secondFound = 0, changed = 0;

        for (i = 0; i < n; i++) {
            firstFound |= devices[i].id == players[0].device;
            secondFound |= devices[i].id == players[1].device;
        }
        if (players[1].device != DEVICE_NONE && !secondFound) {
            players[1].device = DEVICE_NONE;
            players[1].ready = 0;
            changed = 1;
        }
        if (!firstFound) {
            players[0].device = DEVICE_KEYBOARD;
            changed = 1;
        }
        if (changed) {
            PortInputAssign(players[0].device, players[1].device);
        }
    }
    for (i = 0; i < n; i++) {
        unsigned short pressed = Pressed(&devices[i]);

        if (devices[i].id == players[0].device) {
            players[0].kind = devices[i].kind;
            ChangeTeam(&players[0], pressed, count);
            if (pressed & BUTTON_CROSS) {
                players[0].ready = !players[0].ready;
                PlayNote(9, 255, 15, 15, 256);
            }
        } else if (devices[i].id == players[1].device) {
            players[1].kind = devices[i].kind;
            ChangeTeam(&players[1], pressed, count);
            if (pressed & BUTTON_CROSS) {
                players[1].ready = !players[1].ready;
                PlayNote(9, 255, 15, 15, 256);
            } else if (pressed & BUTTON_TRIANGLE) {
                PlayNote(49, 255, 15, 15, 256);
                if (players[1].ready) {
                    players[1].ready = 0;
                } else {
                    players[1].device = DEVICE_NONE;
                    PortInputAssign(players[0].device, DEVICE_NONE);
                }
            }
        } else if (players[1].device == DEVICE_NONE && (pressed & (BUTTON_START | BUTTON_CROSS))) {
            /* a second controller joins */
            players[1].device = devices[i].id;
            players[1].kind = devices[i].kind;
            players[1].team = (players[0].team + 1) % count;
            players[1].ready = 0;
            PortInputAssign(players[0].device, players[1].device);
            PlayNote(9, 255, 15, 15, 256);
        }
    }
    if (state == PORT_LINK_SLAVE_NONE) {
        PortLinkRestart(); /* it closed (or never started): again */
    }

    DrawDivider();
    DrawPlayer(&players[0], 0, 84, -410);
    if (players[1].device != DEVICE_NONE) {
        DrawPlayer(&players[1], 1, 236, 410);
    } else if (blink % 40 < 28) {
        CentreText2((unsigned char *)"PRESS START", 236, 104, 8, 1);
        CentreText2((unsigned char *)"OR ENTER", 236, 116, 8, 1);
        CentreText2((unsigned char *)"TO CONNECT", 236, 128, 8, 1);
        CentreText2((unsigned char *)"PLAYER TWO", 236, 140, 8, 0);
    }

    if (players[0].ready && players[1].device != DEVICE_NONE && players[1].ready) {
        if (state == PORT_LINK_SLAVE_WAITING) {
            selection->ship = (char)TeamShip(players[0].team);
            PortLinkGo(TeamShip(players[1].team));
            return 1;
        }
        PrintCX((u_char *)"CONNECTING", 160, STRIP_Y);
    }
    return 0;
}

/* The first player left the lobby: every controller is theirs again and the second player's game closes. */
void PortLobbyLeave(void)
{
    PortInputAssign(DEVICE_NONE, DEVICE_NONE);
    PortLinkStop();
    lastFrame = -100;
    session = 0;
}
