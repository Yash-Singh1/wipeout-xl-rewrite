/* libapi: the BIOS event system, critical sections, and the small kernel calls the game
   makes at start-up (callbacks, memory size) plus the serial link port.

   Events are what the CD and card libraries report through: a class/spec pair, delivered
   either by setting the event's status (polled with TestEvent/WaitEvent) or by calling a
   handler. Handlers the console runs from interrupts run here on the game thread, at the
   next point where the libraries service events, never inside the audio thread. */
#include <string.h>

#include "internal.h"

#define MAX_EVENTS 64
#define EVENT_HANDLE_BASE 0xF1000000u

#define ST_WAIT 0x1000    /* opened, disabled */
#define ST_ACTIVE 0x2000  /* enabled */
#define ST_ALREADY 0x4000 /* has occurred */
#define MODE_HANDLER 0x1000 /* EvMdINTR */

typedef struct Event {
    bool used;
    uint32_t eventClass;
    uint32_t spec;
    int mode;
    void (*handler)(void);
    int status;
} Event;

static Event events[MAX_EVENTS];
static int criticalDepth;
static void (*pending[MAX_EVENTS * 2])(void);
static int pendingCount;

static Event *lookup(int handle)
{
    uint32_t index = (uint32_t)handle - EVENT_HANDLE_BASE;

    return index < MAX_EVENTS && events[index].used ? &events[index] : NULL;
}

int OpenEvent(uint32_t desc, int spec, int mode, void (*func)(void))
{
    int i;

    PsxLock();
    for (i = 0; i < MAX_EVENTS; i++) {
        if (!events[i].used) {
            events[i].used = true;
            events[i].eventClass = desc;
            events[i].spec = (uint32_t)spec;
            events[i].mode = mode;
            events[i].handler = func;
            events[i].status = ST_WAIT;
            PsxUnlock();
            return (int)(EVENT_HANDLE_BASE + (uint32_t)i);
        }
    }
    PsxUnlock();
    return -1;
}

int EnableEvent(int handle)
{
    Event *e;

    PsxLock();
    e = lookup(handle);
    if (e) {
        e->status = ST_ACTIVE;
    }
    PsxUnlock();
    return e != NULL;
}

int DisableEvent(int handle)
{
    Event *e;

    PsxLock();
    e = lookup(handle);
    if (e) {
        e->status = ST_WAIT;
    }
    PsxUnlock();
    return e != NULL;
}

int CloseEvent(int handle)
{
    Event *e;

    PsxLock();
    e = lookup(handle);
    if (e) {
        memset(e, 0, sizeof(*e));
    }
    PsxUnlock();
    return e != NULL;
}

static void run_pending(void)
{
    while (!criticalDepth) {
        void (*handler)(void);

        PsxLock();
        if (!pendingCount) {
            PsxUnlock();
            return;
        }
        handler = pending[0];
        memmove(pending, pending + 1, (size_t)(--pendingCount) * sizeof(pending[0]));
        PsxUnlock();
        handler();
    }
}

void PsxDeliverEvent(uint32_t eventClass, uint32_t spec)
{
    int i;

    PsxLock();
    for (i = 0; i < MAX_EVENTS; i++) {
        Event *e = &events[i];

        if (!e->used || e->eventClass != eventClass || e->spec != spec || e->status == ST_WAIT) {
            continue;
        }
        if ((e->mode & MODE_HANDLER) && e->handler) {
            if (pendingCount < (int)(sizeof(pending) / sizeof(pending[0]))) {
                pending[pendingCount++] = e->handler;
            }
        } else {
            e->status = ST_ALREADY;
        }
    }
    PsxUnlock();
    run_pending();
}

int TestEvent(int handle)
{
    static unsigned misses;
    Event *e;
    int result = 0;

    PsxIoService();
    PsxLock();
    e = lookup(handle);
    if (e && e->status == ST_ALREADY) {
        e->status = ST_ACTIVE;
        result = 1;
    }
    PsxUnlock();
    if (result) {
        misses = 0;
    } else if (++misses >= 256) {
        /* a polling loop: let a video field pass so the devices can make progress */
        misses = 0;
        PsxVBlank();
    }
    return result;
}

int WaitEvent(int handle)
{
    for (;;) {
        Event *e;
        uint64_t deadline;

        PsxIoService();
        PsxLock();
        e = lookup(handle);
        if (!e || e->status == ST_WAIT) {
            PsxUnlock();
            return 0; /* closed or not enabled: the BIOS does not wait */
        }
        if (e->status == ST_ALREADY) {
            e->status = ST_ACTIVE;
            PsxUnlock();
            return 1;
        }
        deadline = g_io.cd.completeAt;
        PsxUnlock();
        PsxIoWait(deadline);
    }
}

int EnterCriticalSection(void)
{
    PsxLock();
    criticalDepth++;
    PsxUnlock();
    return 1;
}

int ExitCriticalSection(void)
{
    PsxLock();
    if (criticalDepth > 0) {
        criticalDepth--;
    }
    PsxUnlock();
    run_pending();
    return 0;
}

bool PsxInCriticalSection(void)
{
    return criticalDepth > 0;
}

/* ---- start-up calls ---------------------------------------------------------------------- */

int ResetCallback(void)
{
    return 0;
}

void StopCallback(void) {}

int SetMem(int megabytes)
{
    (void)megabytes;
    return 0;
}

/* The movie programs are run by the platform (see PsxSetExecHook); without a hook nothing
   runs and the call returns at once, as if the program had finished. */
int Exec(void *block, int argc, char **argv)
{
    (void)argc;
    (void)argv;
    if (g_io.exec) {
        return g_io.exec(block);
    }
    return 1;
}

/* ---- the serial link ------------------------------------------------------------------------
   Nothing is connected to the link port: the control register reads back with no line
   asserted, so the game's handshake times out ("link broken") as it does with no cable. */

void AddCOMB(void) {}

int ChangeClearSIO(int mode)
{
    (void)mode;
    return 0;
}

int _comb_control(int operation, int reg, int value)
{
    (void)operation;
    (void)reg;
    (void)value;
    return 0;
}
