/* The second player's game (platform/second/): its own copy of the game in this program, run on a thread of its
   own (host.c). Where the game would end or change the whole program, this copy ends or changes only itself:
   exit() ends its thread, atexit() keeps its own list, what it allocated and has not freed is freed when it ends
   (as a program's memory is at its exit), and the test settings in the environment are the first game's, not its
   own. (Compiled once, with the C library's own names: the second game's sources call these in
   their place.) */
#include <SDL3/SDL.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "second/second.h"

/* ---- its memory ---- */

/* What it allocated with the C library's malloc() and the others and has not freed: a set of the blocks, open
   addressing (a freed block leaves a tombstone until the set is made again). Its own threads (sound) may allocate
   too. */
#define TOMBSTONE ((void *)1)
static SDL_Mutex *memoryLock;
static void **blocks;
static size_t blockSlots, blockUsed; /* (used: blocks and tombstones) */

static size_t Slot(void *block)
{
    return (size_t)(((uintptr_t)block >> 4) * 0x9E3779B97F4A7C15ull) & (blockSlots - 1);
}

static void Remember(void *block)
{
    size_t i;

    if (blockUsed + 1 > blockSlots / 2) {
        void **old = blocks;
        size_t oldSlots = blockSlots, k;

        blockSlots = blockSlots ? blockSlots * 2 : 4096;
        blocks = calloc(blockSlots, sizeof(*blocks));
        blockUsed = 0;
        if (!blocks) {
            abort(); /* (as running out of memory anywhere) */
        }
        for (k = 0; k < oldSlots; k++) {
            if (old[k] && old[k] != TOMBSTONE) {
                for (i = Slot(old[k]); blocks[i]; i = (i + 1) & (blockSlots - 1)) {
                }
                blocks[i] = old[k];
                blockUsed++;
            }
        }
        free(old);
    }
    for (i = Slot(block); blocks[i] && blocks[i] != TOMBSTONE; i = (i + 1) & (blockSlots - 1)) {
    }
    if (!blocks[i]) {
        blockUsed++;
    }
    blocks[i] = block;
}

/* 1 when it was one of the second game's (now forgotten) */
static int Forget(void *block)
{
    size_t i;

    if (!blockSlots) {
        return 0;
    }
    for (i = Slot(block); blocks[i]; i = (i + 1) & (blockSlots - 1)) {
        if (blocks[i] == block) {
            blocks[i] = TOMBSTONE;
            return 1;
        }
    }
    return 0;
}

void *PortSecondMalloc(size_t size)
{
    void *block = malloc(size);

    if (block) {
        SDL_LockMutex(memoryLock);
        Remember(block);
        SDL_UnlockMutex(memoryLock);
    }
    return block;
}

void *PortSecondCalloc(size_t count, size_t size)
{
    void *block = calloc(count, size);

    if (block) {
        SDL_LockMutex(memoryLock);
        Remember(block);
        SDL_UnlockMutex(memoryLock);
    }
    return block;
}

void *PortSecondRealloc(void *block, size_t size)
{
    void *moved;

    SDL_LockMutex(memoryLock);
    moved = realloc(block, size);
    if (moved || size == 0) {
        if (block) {
            Forget(block);
        }
        if (moved) {
            Remember(moved);
        }
    }
    SDL_UnlockMutex(memoryLock);
    return moved;
}

/* (one it did not allocate itself, the C library's strdup()'s, is freed all the same) */
void PortSecondFree(void *block)
{
    if (!block) {
        return;
    }
    SDL_LockMutex(memoryLock);
    Forget(block);
    SDL_UnlockMutex(memoryLock);
    free(block);
}

static void FreeAll(void)
{
    size_t i, count = 0;

    for (i = 0; i < blockSlots; i++) {
        if (blocks[i] && blocks[i] != TOMBSTONE) {
            free(blocks[i]);
            count++;
        }
    }
    free(blocks);
    blocks = NULL;
    blockSlots = blockUsed = 0;
    if (count) {
        SDL_Log("second player: %zu blocks it left were freed", count);
    }
}

/* ---- its end ---- */

static jmp_buf leave;
static int status;
static void (*atExit[16])(void);
static int atExitCount;

void PortSecondExit(int code)
{
    status = code;
    longjmp(leave, 1);
}

int PortSecondAtExit(void (*function)(void))
{
    if (atExitCount == (int)SDL_arraysize(atExit)) {
        return -1;
    }
    atExit[atExitCount++] = function;
    return 0;
}

/* The first game's tests drive it alone. */
static int TestSetting(const char *name)
{
    static const char *const testSettings[] = {
        "WIPEOUT_KEYS", "WIPEOUT_CAPTURE_DIR", "WIPEOUT_FRAMES", "WIPEOUT_TRACE", "WIPEOUT_PADS",
        "WIPEOUT_VCOUNTS", "WIPEOUT_WRITES", "WIPEOUT_LOADS", "WIPEOUT_VRAM_DUMP",
        "WIPEOUT_SAVE_DIR", "WIPEOUT_FIXED_STEP", "WIPEOUT_HIDDEN", "WIPEOUT_RUMBLE_LOG", "WIPEOUT_DEBUG_UI",
        "WIPEOUT_FRAMECAP", "WIPEOUT_FRAMECAP_DIR", "WIPEOUT_FRAMECAP_FREEZE_AT", "WIPEOUT_FRAMECAP_TAB_AT",
        "WIPEOUT_FRAMECAP_PICK", "WIPEOUT_FRAMECAP_HOVER", "WIPEOUT_WAV", "WIPEOUT_AUTOSAVE", "WIPEOUT_XL_DISC"};
    size_t i;

    for (i = 0; i < SDL_arraysize(testSettings); i++) {
        if (!strcmp(name, testSettings[i])) {
            return 1;
        }
    }
    return 0;
}

char *PortSecondGetenv(const char *name)
{
    return TestSetting(name) ? NULL : getenv(name);
}

const char *PortSecondSDLGetenv(const char *name)
{
    return TestSetting(name) ? NULL : SDL_getenv(name);
}

int PortSecondGameMain(int argc, char **argv)
{
    int i;

    atExitCount = 0;
    status = 0;
    if (!memoryLock && !(memoryLock = SDL_CreateMutex())) {
        return 1;
    }
    if (!setjmp(leave)) {
        status = PortSecondGameMainEntry(argc, argv);
    }
    /* (as exit() would: the last registered first; one that exits ends the rest) */
    while (atExitCount > 0) {
        i = --atExitCount;
        if (!setjmp(leave)) {
            atExit[i]();
        }
    }
    FreeAll();
    return status;
}
