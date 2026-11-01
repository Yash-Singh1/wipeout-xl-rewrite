/* Exercise the second game's actual nonlocal exit and callback cleanup on its thread. */
#include "../../platform/second/entry.c"
#include <assert.h>
#include <stdio.h>

static int callbacks;

static void Last(void) { assert(callbacks++ == 2); }
static void Middle(void) { assert(callbacks++ == 1); PortSecondExit(7); }
static void First(void) { assert(callbacks++ == 0); }

static void LeaveNested(int depth)
{
    if (depth) {
        LeaveNested(depth - 1);
        abort();
    }
    PortSecondExit(3);
    abort();
}

int PortSecondGameMainEntry(int argc, char **argv)
{
    void *allocation = PortSecondMalloc(1024);
    (void)argc; (void)argv;
    assert(allocation);
    assert(!PortSecondAtExit(Last));
    assert(!PortSecondAtExit(Middle));
    assert(!PortSecondAtExit(First));
    LeaveNested(8);
    return 99;
}

static int Run(void *unused)
{
    int i;
    (void)unused;
    for (i = 0; i < 100; i++) {
        callbacks = 0;
        assert(PortSecondGameMain(0, NULL) == 7);
        assert(callbacks == 3 && !blocks && !blockSlots && !blockUsed);
    }
    return 0;
}

int main(void)
{
    int status = -1;
    SDL_SetLogPriorities(SDL_LOG_PRIORITY_ERROR);
    SDL_Thread *thread = SDL_CreateThread(Run, "second-exit-test", NULL);
    assert(thread);
    SDL_WaitThread(thread, &status);
    assert(status == 0);
    SDL_DestroyMutex(memoryLock);
    puts("second game exits and restarts on its own thread; callbacks and allocations cleaned: OK");
    return 0;
}
