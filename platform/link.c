/* Two players on one machine: the link cable between two copies of the game.

   On the console the two-player modes link two PlayStations with a cable; each runs the race for its
   own ship and the two exchange a packet every frame (doSerialTransfer). Here the game in the window
   (the master) starts the second copy of the game this program holds (the slave, platform/second/: on a
   thread of its own, with no window of its own) and the two exchange their packets through memory they
   share, in step as over the cable. The shared memory also carries what the slave cannot have by itself:
   the second player's controller (read by the master, which has the window and the input focus), the
   second player's choices made on the master's lobby screen, and the slave's picture, which the master
   shows in the lower half of the window during a race. */
#include <SDL3/SDL.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "link.h"
#include "platform.h"
#ifdef PORT_HAVE_SECOND_GAME
#include "second/second.h"
#endif

#define LINK_MAGIC 0x574C4B32u /* WLK2 */
#define PACKET_BYTES 128
/* A transfer waits for the other game as long as that game is alive and going on (its heartbeat): a
   track takes a while to load, and the two need not finish together. The link is broken when the other
   game has exited, has stopped (no heartbeat for LINK_STALLED_MS) or has not answered for LINK_UNANSWERED_MS. */
#define LINK_STALLED_MS 8000
#define LINK_UNANSWERED_MS 30000

typedef struct LinkShared {
    uint32_t magic;
    atomic_uint heartbeat[2];       /* bumped by each side every field, and while it waits for the other */
    atomic_uint sequence[2];        /* transfers each side has made */
    uint8_t packet[2][2][PACKET_BYTES];
    atomic_int slaveState;          /* LINK_SLAVE_* */
    atomic_int go;                  /* master: link now (the lobby is done) */
    atomic_int secondShip;          /* the second player's ship */
    atomic_int quit;                /* master: the slave is to exit */
    atomic_int split;               /* master: both are racing, the window is split */
    atomic_int bandMargin, bandWidth, bandHeight; /* master: the lower half (widescreen margin, pixels) */
    atomic_int interpolate;         /* master: pictures between the game's frames (the slave draws its own too) */
    LinkInput input;                /* the second player's controller, written by the master */
    atomic_uint inputSequence;
    atomic_int rumble[PAD_RUMBLE_KINDS]; /* slave: what its game asks of the second player's motors, level + 1
                                            (0: nothing since the master took it; a jolt keeps the strongest) */
    atomic_uint frameSequence;      /* the slave's pictures (three buffers) */
    atomic_uint presentPeriod;      /* master: how often it shows the window, in microseconds */
    int frameWidth[3], frameHeight[3];
    uint8_t frames[3][LINK_FRAME_BYTES];
} LinkShared;

static LinkShared *shared;
static int role = LINK_NONE;
static int slave; /* the master: the slave was started (and has not been waited for since) */

/* ---- shared memory ---- */

/* The master's (made when it first starts the slave), handed to the slave when it starts. */
static LinkShared *MapShared(int create)
{
    LinkShared *memory;

    if (!create) {
#ifdef PORT_HAVE_SECOND_GAME
        return PortSecondGameShared();
#else
        return NULL;
#endif
    }
    memory = SDL_calloc(1, sizeof(LinkShared));
    if (memory) {
        memory->magic = LINK_MAGIC;
    }
    return memory;
}

static void UnmapShared(void)
{
    if (shared && role == LINK_MASTER && !slave) {
        SDL_free(shared);
    }
    shared = NULL;
}

/* ---- the two sides ---- */

int PortLinkRole(void)
{
    return role;
}

#ifdef __APPLE__
#include <objc/message.h>
#include <objc/runtime.h>
/* macOS slows down programs it considers in the background (App Nap: their timers late by up to seconds); the
   link must keep both games in step. */
static void KeepAwake(void)
{
    static void *activity;
    void *info, *reason;

    if (activity) {
        return;
    }
    info = ((void *(*)(void *, SEL))objc_msgSend)((void *)objc_getClass("NSProcessInfo"), sel_registerName("processInfo"));
    reason = ((void *(*)(void *, SEL, const char *))objc_msgSend)((void *)objc_getClass("NSString"),
                                                                  sel_registerName("stringWithUTF8String:"),
                                                                  "two players: the link keeps both games in step");
    /* NSActivityUserInitiated | NSActivityLatencyCritical */
    activity = ((void *(*)(void *, SEL, unsigned long long, void *))objc_msgSend)(
        info, sel_registerName("beginActivityWithOptions:reason:"), 0x00FFFFFFULL | 0xFF00000000ULL, reason);
    ((void *(*)(void *, SEL))objc_msgSend)(activity, sel_registerName("retain"));
}
#else
static void KeepAwake(void) {}
#endif

int PortLinkSlaveInit(void)
{
    shared = MapShared(0);
    if (!shared || shared->magic != LINK_MAGIC) {
        fprintf(stderr, "link: the first player's game shares nothing with this one\n");
        return 0;
    }
    role = LINK_SLAVE;
    atomic_store(&shared->slaveState, LINK_SLAVE_BOOTING);
    return 1;
}

int PortLinkStartSlave(int windowWidth, int windowHeight)
{
#ifdef PORT_HAVE_SECOND_GAME
    const char *args[16];
    char window[32], saves[1024];
    int n = 0;

    if (slave && PortSecondGameRunning()) {
        return 1;
    }
    if (slave && !PortSecondGameStop(0)) {
        return 0; /* (not yet ended) */
    }
    slave = 0;
    if (!shared) {
        role = LINK_MASTER;
        shared = MapShared(1);
        if (!shared) {
            role = LINK_NONE;
            fprintf(stderr, "link: no memory to share with the second player's game\n");
            return 0;
        }
        KeepAwake();
    }
    atomic_store(&shared->quit, 0);
    atomic_store(&shared->go, 0);
    atomic_store(&shared->split, 0);
    atomic_store(&shared->slaveState, LINK_SLAVE_BOOTING);
    atomic_store(&shared->sequence[0], 0);
    atomic_store(&shared->sequence[1], 0);
    SDL_snprintf(window, sizeof(window), "%dx%d", windowWidth, windowHeight);
    /* the second player's memory card is a scratch one: the first player's card is the player's */
    SDL_snprintf(saves, sizeof(saves), "%slink-player2/", portConfig.savePath ? portConfig.savePath : "./");
    SDL_CreateDirectory(saves);
    args[n++] = "wipeoutxl";
    args[n++] = "--disc";
    args[n++] = portConfig.discPath;
    args[n++] = "--window";
    args[n++] = window;
    args[n++] = "--link-slave";
    args[n++] = "--save-dir";
    args[n++] = saves;
    args[n] = NULL;
    if (!PortSecondGameStart(shared, n, args)) {
        fprintf(stderr, "link: cannot start the second player's game: %s\n", SDL_GetError());
        atomic_store(&shared->slaveState, LINK_SLAVE_NONE);
        return 0;
    }
    slave = 1;
    PortLog("link: the second player's game started\n");
    return 1;
#else
    (void)windowWidth;
    (void)windowHeight;
    return 0; /* (no second game here) */
#endif
}

void PortLinkStopSlave(void)
{
    if (!slave) {
        return;
    }
    atomic_store(&shared->quit, 1);
#ifdef PORT_HAVE_SECOND_GAME
    if (!PortSecondGameStop(3000)) {
        fprintf(stderr, "link: the second player's game has not ended\n");
        return; /* (it still has the shared memory) */
    }
#endif
    slave = 0;
    atomic_store(&shared->slaveState, LINK_SLAVE_NONE);
    atomic_store(&shared->split, 0);
    PortLog("link: the second player's game ended\n");
}

void PortLinkShutdown(void)
{
    if (role == LINK_MASTER) {
        PortLinkStopSlave();
    }
    UnmapShared();
}

int PortLinkSlaveState(void)
{
#ifndef PORT_HAVE_SECOND_GAME
    return LINK_SLAVE_HERE_NONE;
#else
    if (!shared) {
        return LINK_SLAVE_NONE;
    }
    if (role == LINK_MASTER && slave && !PortSecondGameRunning()) {
        /* it has ended */
        PortSecondGameStop(0);
        slave = 0;
        atomic_store(&shared->slaveState, LINK_SLAVE_NONE);
    }
    return atomic_load(&shared->slaveState);
#endif
}

void PortLinkSetSlaveState(int state)
{
    if (shared) {
        atomic_store(&shared->slaveState, state);
    }
}

void PortLinkGo(int secondShip)
{
    if (shared) {
        atomic_store(&shared->secondShip, secondShip);
        atomic_store(&shared->sequence[0], 0);
        atomic_store(&shared->sequence[1], 0);
        atomic_store(&shared->go, 1);
    }
}

int PortLinkGoSignal(int *secondShip)
{
    if (!shared || !atomic_load(&shared->go)) {
        return 0;
    }
    *secondShip = atomic_load(&shared->secondShip);
    return 1;
}

void PortLinkClearGo(void)
{
    if (shared) {
        atomic_store(&shared->go, 0);
    }
}

int PortLinkQuitRequested(void)
{
    return shared && role == LINK_SLAVE && atomic_load(&shared->quit);
}

/* Each field (libetc.c): this game is alive. The second player's game hurries through its start-up (the
   legal screens, the loading) to the link screen, where it is wanted; it is not seen meanwhile. */
int PortLinkField(void)
{
    if (!shared) {
        return 0;
    }
    atomic_fetch_add(&shared->heartbeat[role == LINK_MASTER ? 0 : 1], 1);
    return role == LINK_SLAVE && atomic_load(&shared->slaveState) == LINK_SLAVE_BOOTING;
}

/* One packet each way, in step: each side's n-th transfer meets the other's n-th. Two slots per
   side, as a side can be one transfer ahead of the other's reading. While it waits, the first player's
   game keeps its window answering and fields pass in the second player's (as the console's vertical
   blanks went on during a transfer). */
int PortLinkTransfer(const uint8_t *send, uint8_t *recv, int count)
{
    int me = role == LINK_MASTER ? 0 : 1, other = 1 - me;
    unsigned n, beat;
    uint64_t start, beatAt, pumped;

    if (!shared || count > PACKET_BYTES) {
        return 1;
    }
    n = atomic_load(&shared->sequence[me]) + 1;
    memcpy(shared->packet[me][n & 1], send, (size_t)count);
    atomic_store(&shared->sequence[me], n);
    start = beatAt = pumped = SDL_GetTicks();
    beat = atomic_load(&shared->heartbeat[other]);
    if (role == LINK_SLAVE) {
        PsxPassFields();
    }
    while (atomic_load(&shared->sequence[other]) < n) {
        uint64_t now = SDL_GetTicks();
        const char *broken = NULL;

        if (role == LINK_SLAVE) {
            PsxPassFields();
            PsxLinkWaiting();
        } else if (now - pumped >= 50) {
            pumped = now;
            atomic_fetch_add(&shared->heartbeat[me], 1);
            if (!PortPumpEvents()) {
                exit(0); /* the window was closed */
            }
        }
        if (atomic_load(&shared->heartbeat[other]) != beat) {
            beat = atomic_load(&shared->heartbeat[other]);
            beatAt = now;
        }
        if (atomic_load(&shared->quit) || PortLinkQuitRequested()) {
            broken = "the first player's game closed the link";
        } else if (role == LINK_MASTER && PortLinkSlaveState() == LINK_SLAVE_NONE) {
            broken = "the second player's game has exited";
        } else if (now - beatAt > LINK_STALLED_MS) {
            broken = "the other game has stopped";
        } else if (now - start > LINK_UNANSWERED_MS) {
            broken = "the other game no longer answers";
        }
        if (broken) {
            fprintf(stderr, "link: broken at transfer %u: %s\n", n, broken);
            return 1; /* the game reports a broken link */
        }
        SDL_DelayNS(100000);
    }
    if (SDL_GetTicks() - start > 100) {
        PortLog("link: %s waited %llu ms for transfer %u\n", role == LINK_MASTER ? "first" : "second",
                (unsigned long long)(SDL_GetTicks() - start), n);
    }
    memcpy(recv, shared->packet[other][n & 1], (size_t)count);
    return 0;
}

/* ---- the second player's controller ---- */

void PortLinkSetInput(const LinkInput *input)
{
    if (shared) {
        shared->input = *input;
        atomic_fetch_add(&shared->inputSequence, 1);
    }
}

int PortLinkGetInput(LinkInput *input)
{
    if (!shared) {
        return 0;
    }
    *input = shared->input;
    return 1;
}

/* ---- the second player's motors ---- */

void PortLinkPutRumble(int kind, int level)
{
    int before;

    if (!shared || kind < 0 || kind >= PAD_RUMBLE_KINDS) {
        return;
    }
    if (kind != PAD_RUMBLE_JOLT) {
        atomic_store(&shared->rumble[kind], level + 1);
        return;
    }
    /* two hits in one field (a nose on the wall says two): the master sees the stronger */
    before = atomic_load(&shared->rumble[kind]);
    while (before < level + 1 && !atomic_compare_exchange_weak(&shared->rumble[kind], &before, level + 1)) {
    }
}

int PortLinkTakeRumble(int kind, int *level)
{
    int value;

    if (!shared || kind < 0 || kind >= PAD_RUMBLE_KINDS) {
        return 0;
    }
    value = atomic_exchange(&shared->rumble[kind], 0);
    *level = value - 1;
    return value > 0;
}

/* ---- the split window ---- */

void PortLinkSetSplit(int split)
{
    if (shared) {
        atomic_store(&shared->split, split);
    }
}

int PortLinkSplit(void)
{
    return shared && atomic_load(&shared->split);
}

void PortLinkSetBand(int margin, int width, int height)
{
    if (shared) {
        atomic_store(&shared->bandMargin, margin);
        atomic_store(&shared->bandWidth, width);
        atomic_store(&shared->bandHeight, height);
    }
}

int PortLinkBand(int *margin, int *width, int *height)
{
    if (!shared || atomic_load(&shared->bandWidth) <= 0 || atomic_load(&shared->bandHeight) <= 0) {
        return 0;
    }
    *margin = atomic_load(&shared->bandMargin);
    *width = atomic_load(&shared->bandWidth);
    *height = atomic_load(&shared->bandHeight);
    return 1;
}

void PortLinkPutFrame(const uint8_t *pixels, int width, int height)
{
    unsigned next;

    if (!shared || (size_t)width * height * 4 > LINK_FRAME_BYTES) {
        return;
    }
    next = (atomic_load(&shared->frameSequence) + 1) % 3;
    memcpy(shared->frames[next], pixels, (size_t)width * height * 4);
    shared->frameWidth[next] = width;
    shared->frameHeight[next] = height;
    atomic_fetch_add(&shared->frameSequence, 1);
}

void PortLinkSetPresentPeriod(unsigned microseconds)
{
    if (shared) {
        atomic_store(&shared->presentPeriod, microseconds);
    }
}

unsigned PortLinkPresentPeriod(void)
{
    return shared ? atomic_load(&shared->presentPeriod) : 0;
}

void PortLinkSetInterpolate(int on)
{
    if (shared) {
        atomic_store(&shared->interpolate, on);
    }
}

int PortLinkInterpolate(void)
{
    return shared && atomic_load(&shared->interpolate);
}

const uint8_t *PortLinkGetFrame(int *width, int *height, unsigned *sequence)
{
    unsigned current;

    if (!shared || !atomic_load(&shared->frameSequence)) {
        return NULL;
    }
    current = atomic_load(&shared->frameSequence);
    *sequence = current;
    current %= 3;
    *width = shared->frameWidth[current];
    *height = shared->frameHeight[current];
    return shared->frames[current];
}

/* ---- for the game (game/include/portlink.h) ---- */

void PortLinkRestart(void)
{
    int width, height;

    PortLinkStopSlave();
    PortWindowSize(&width, &height); /* in the window system's points, as --window takes it */
    PortLinkStartSlave(width, height);
}

void PortLinkStop(void)
{
    PortLinkStopSlave();
}
