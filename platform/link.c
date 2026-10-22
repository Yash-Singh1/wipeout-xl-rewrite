/* Two players on one machine: the link cable between two copies of the game.

   On the console the two-player modes link two PlayStations with a cable; each runs the race for its
   own ship and the two exchange a packet every frame (doSerialTransfer). Here the game in the window
   (the master) starts a second copy of itself for the second player (the slave, no window of its
   own) and the two exchange their packets through shared memory, in step as over the cable. The
   shared memory also carries what the slave cannot have by itself: the second player's controller
   (read by the master, which has the window and the input focus), the second player's choices made
   on the master's lobby screen, and the slave's picture, which the master shows in the lower half of
   the window during a race. */
#include <SDL3/SDL.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

#include "link.h"
#include "platform.h"

#ifdef __EMSCRIPTEN__
/* The web: a page runs one copy of the game and SDL has no processes there, so the second player's game
   never starts (PortLinkStartSlave fails as when the program cannot be run) and the two-player modes find no
   partner. */
#define SDL_CreateProcess(args, pipeStdio) (SDL_SetError("the web has no second program"), (SDL_Process *)NULL)
#define SDL_WaitProcess(process, block, exitCode) true
#define SDL_KillProcess(process, force) true
#define SDL_DestroyProcess(process) ((void)(process))
#endif

#define LINK_MAGIC 0x574C4B31u /* WLK1 */
#define PACKET_BYTES 128
#define TRANSFER_TIMEOUT_MS 5000

typedef struct LinkShared {
    uint32_t magic;
    atomic_uint heartbeat[2];       /* bumped by each side every field */
    atomic_uint sequence[2];        /* transfers each side has made */
    uint8_t packet[2][2][PACKET_BYTES];
    atomic_int slaveState;          /* LINK_SLAVE_* */
    atomic_int go;                  /* master: link now (the lobby is done) */
    atomic_int secondShip;          /* the second player's ship */
    atomic_int quit;                /* master: the slave is to exit */
    atomic_int split;               /* master: both are racing, the window is split */
    LinkInput input;                /* the second player's controller, written by the master */
    atomic_uint inputSequence;
    atomic_uint frameSequence;      /* the slave's pictures (three buffers) */
    int frameWidth[3], frameHeight[3];
    uint8_t frames[3][LINK_FRAME_BYTES];
} LinkShared;

static LinkShared *shared;
static int role = LINK_NONE;
static char name[64];
static SDL_Process *slave;
#ifdef _WIN32
static HANDLE mapping;
#endif

/* ---- shared memory ---- */

static LinkShared *MapShared(int create)
{
    size_t size = sizeof(LinkShared);
    void *memory;

#ifdef _WIN32
    mapping = create ? CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, (DWORD)((uint64_t)size >> 32),
                                          (DWORD)size, name)
                     : OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, name);
    if (!mapping) {
        return NULL;
    }
    memory = MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, size);
#else
    int fd = shm_open(name, create ? (O_CREAT | O_EXCL | O_RDWR) : O_RDWR, 0600);

    if (fd < 0) {
        return NULL;
    }
    if (create && ftruncate(fd, (off_t)size) != 0) {
        close(fd);
        shm_unlink(name);
        return NULL;
    }
    memory = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if (memory == MAP_FAILED) {
        memory = NULL;
    }
#endif
    if (memory && create) {
        memset(memory, 0, size);
        ((LinkShared *)memory)->magic = LINK_MAGIC;
    }
    return memory;
}

static void UnmapShared(void)
{
    if (!shared) {
        return;
    }
#ifdef _WIN32
    UnmapViewOfFile(shared);
    CloseHandle(mapping);
#else
    munmap(shared, sizeof(LinkShared));
    if (role == LINK_MASTER) {
        shm_unlink(name);
    }
#endif
    shared = NULL;
}

/* ---- the two sides ---- */

int PortLinkRole(void)
{
    return role;
}

int PortLinkSlaveInit(const char *sharedName)
{
    SDL_strlcpy(name, sharedName, sizeof(name));
    shared = MapShared(0);
    if (!shared || shared->magic != LINK_MAGIC) {
        fprintf(stderr, "link: cannot open %s\n", name);
        return 0;
    }
    role = LINK_SLAVE;
    atomic_store(&shared->slaveState, LINK_SLAVE_BOOTING);
    return 1;
}

int PortLinkStartSlave(int windowWidth, int windowHeight)
{
    const char *args[16];
    char window[32], saves[1024];
    int n = 0;

    if (slave) {
        return 1;
    }
    if (!shared) {
#ifdef _WIN32
        SDL_snprintf(name, sizeof(name), "wipeoutxl-%lu", (unsigned long)GetCurrentProcessId());
#else
        SDL_snprintf(name, sizeof(name), "/wipeoutxl-%ld", (long)getpid());
#endif
        role = LINK_MASTER;
        shared = MapShared(1);
        if (!shared) {
            role = LINK_NONE;
            PortLog("link: no shared memory for the second player\n");
            return 0;
        }
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
    args[n++] = portConfig.executable;
    args[n++] = "--disc";
    args[n++] = portConfig.discPath;
    args[n++] = "--window";
    args[n++] = window;
    args[n++] = "--link-slave";
    args[n++] = name;
    args[n++] = "--save-dir";
    args[n++] = saves;
    args[n] = NULL;
    slave = SDL_CreateProcess(args, false);
    if (!slave) {
        PortLog("link: cannot start the second player's game: %s\n", SDL_GetError());
        return 0;
    }
    return 1;
}

void PortLinkStopSlave(void)
{
    if (!slave) {
        return;
    }
    atomic_store(&shared->quit, 1);
    {
        int waited = 0;
        while (waited < 3000 && !SDL_WaitProcess(slave, false, NULL)) {
            SDL_Delay(10);
            waited += 10;
        }
        if (waited >= 3000) {
            SDL_KillProcess(slave, true);
            SDL_WaitProcess(slave, true, NULL);
        }
    }
    SDL_DestroyProcess(slave);
    slave = NULL;
    atomic_store(&shared->slaveState, LINK_SLAVE_NONE);
    atomic_store(&shared->split, 0);
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
    if (!shared) {
        return LINK_SLAVE_NONE;
    }
    if (role == LINK_MASTER && slave && SDL_WaitProcess(slave, false, NULL)) {
        /* it has exited */
        SDL_DestroyProcess(slave);
        slave = NULL;
        atomic_store(&shared->slaveState, LINK_SLAVE_NONE);
    }
    return atomic_load(&shared->slaveState);
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
#ifndef _WIN32
    if (role == LINK_SLAVE && getppid() == 1) {
        return 1; /* the first player's game is gone */
    }
#endif
    return shared && role == LINK_SLAVE && atomic_load(&shared->quit);
}

/* One packet each way, in step: each side's n-th transfer meets the other's n-th. Two slots per
   side, as a side can be one transfer ahead of the other's reading. */
int PortLinkTransfer(const uint8_t *send, uint8_t *recv, int count)
{
    int me = role == LINK_MASTER ? 0 : 1, other = 1 - me;
    unsigned n;
    uint64_t start;

    if (!shared || count > PACKET_BYTES) {
        return 1;
    }
    n = atomic_load(&shared->sequence[me]) + 1;
    memcpy(shared->packet[me][n & 1], send, (size_t)count);
    atomic_store(&shared->sequence[me], n);
    start = SDL_GetTicks();
    if (role == LINK_SLAVE) {
        PsxPassFields();
    }
    while (atomic_load(&shared->sequence[other]) < n) {
        if (role == LINK_SLAVE) {
            PsxPassFields();
        }
        if (atomic_load(&shared->quit) || SDL_GetTicks() - start > TRANSFER_TIMEOUT_MS ||
            (role == LINK_MASTER && PortLinkSlaveState() == LINK_SLAVE_NONE)) {
            return 1; /* the other side is gone: the game reports a broken link */
        }
        SDL_DelayNS(50000);
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
    PortDrawableSize(&width, &height);
    PortLinkStartSlave(width, height);
}

void PortLinkStop(void)
{
    PortLinkStopSlave();
}
