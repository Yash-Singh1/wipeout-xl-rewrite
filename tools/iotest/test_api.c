/* The kernel-level behaviour around the libraries: BIOS events (status, handlers, critical
   sections), the drive's error event, deterministic time in fixed-step mode (a field is exactly
   44100 * 1001 / 60000 samples and two runs give identical audio), and the real-time mode with
   an audio device (SDL's dummy driver) where the audio thread runs while the game calls in. */
#include <SDL3/SDL.h>

#include <pthread.h>
#include <stdatomic.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../platform/psx/internal.h"
#include "testenv.h"
#include "testutil.h"

typedef struct { uint8_t minute, second, sector, track; } Loc;

int OpenEvent(uint32_t, int, int, void (*)(void));
int EnableEvent(int);
int DisableEvent(int);
int CloseEvent(int);
int TestEvent(int);
int WaitEvent(int);
int EnterCriticalSection(void);
int ExitCriticalSection(void);
int CdInit(void);
int CdControl(int, const uint8_t *, uint8_t *);
int CdControlB(int, const uint8_t *, uint8_t *);
void SpuInit(void);
void SpuSetKey(int, uint32_t);

static int handlerCalls;
static void handler(void)
{
    handlerCalls++;
}

/* Runs a noisy scenario for `fieldsToRun` fields, recording the output; returns a hash of it
   and the number of non-zero samples. */
static uint32_t record_scenario(int fieldsToRun, long *nonzero)
{
    const char *path = "/tmp/psxio-api.wav";
    uint32_t h = 2166136261u;
    FILE *f;
    int c, i;

    *nonzero = 0;
    PsxLock();
    g_io.wav = fopen(path, "wb");
    PsxUnlock();
    SpuInit();
    SpuWriteRegister(g_io.spu, SPU_MAIN_VOL_L, 0x3FFF);
    SpuWriteRegister(g_io.spu, SPU_MAIN_VOL_R, 0x3FFF);
    SpuWriteRegister(g_io.spu, SPU_NOISE_MODE, 1);
    SpuWriteRegister(g_io.spu, 0x0, 0x3FFF);
    SpuWriteRegister(g_io.spu, 0x2, 0x3FFF);
    SpuWriteRegister(g_io.spu, 0x4, 0x1000);
    SpuSetKey(1, 1);
    for (i = 0; i < fieldsToRun; i++) {
        PsxVBlank();
    }
    PsxLock();
    fclose(g_io.wav);
    g_io.wav = NULL;
    PsxUnlock();
    f = fopen(path, "rb");
    while ((c = fgetc(f)) != EOF) {
        h = (h ^ (uint32_t)c) * 16777619u;
        *nonzero += c != 0;
    }
    fclose(f);
    unlink(path);
    return h;
}

static void *render_thread(void *arg)
{
    atomic_int *stop = arg;
    int16_t block[2 * 256];

    while (!atomic_load(stop)) {
        PsxAudioRender(block, 256);
        SDL_Delay(1); /* about 256 samples per ms: faster than real time, enough for the test */
    }
    return NULL;
}

int main(void)
{
    int a, b, c;

    if (!test_io_start(1.0, NULL)) {
        printf("test_api: disc not found, skipped\n");
        return 0;
    }

    /* events */
    a = OpenEvent(0xF0000099u, 1, 0x2000, 0);          /* polled */
    b = OpenEvent(0xF0000099u, 1, 0x1000, handler);    /* handler */
    c = OpenEvent(0xF0000099u, 2, 0x2000, 0);
    PsxDeliverEvent(0xF0000099u, 1);
    CHECK_EQ_INT(TestEvent(a), 0);                     /* opened but not enabled: dropped */
    EnableEvent(a);
    EnableEvent(b);
    EnableEvent(c);
    PsxDeliverEvent(0xF0000099u, 1);
    CHECK_EQ_INT(handlerCalls, 1);
    CHECK_EQ_INT(TestEvent(c), 0);                     /* other spec */
    CHECK_EQ_INT(TestEvent(a), 1);
    CHECK_EQ_INT(TestEvent(a), 0);                     /* a test consumes the event */
    PsxDeliverEvent(0xF0000099u, 1);
    CHECK_EQ_INT(WaitEvent(a), 1);                     /* already there: returns at once */
    DisableEvent(a);
    PsxDeliverEvent(0xF0000099u, 1);
    CHECK_EQ_INT(TestEvent(a), 0);
    CHECK_EQ_INT(WaitEvent(a), 0);                     /* disabled: no wait */
    /* handlers wait for the end of a critical section */
    handlerCalls = 0;
    EnterCriticalSection();
    PsxDeliverEvent(0xF0000099u, 1);
    CHECK_EQ_INT(handlerCalls, 0);
    ExitCriticalSection();
    CHECK_EQ_INT(handlerCalls, 1);
    CloseEvent(b);
    PsxDeliverEvent(0xF0000099u, 1);
    CHECK_EQ_INT(handlerCalls, 1);

    /* the drive's error event reaches a handler (the game's my_error_handler) */
    {
        int e = OpenEvent(0xF0000003u, 0x8000, 0x1000, handler);
        uint8_t bad = 0x99, result[16];

        EnableEvent(e);
        handlerCalls = 0;
        CHECK_EQ_INT(CdInit(), 1);
        CHECK_EQ_INT(CdControlB(0x14, &bad, result), 0);
        PsxVBlank();
        CHECK_EQ_INT(handlerCalls, 1);
        CHECK_EQ_INT(CdControlB(0x13, NULL, result), 1);
        PsxVBlank();
        CHECK_EQ_INT(handlerCalls, 1);
    }

    /* fixed-step time: a field is 735.735... samples, exactly */
    {
        uint64_t f, start = test_fields();

        for (f = 1; f <= 600; f++) {
            uint64_t want;

            PsxVBlank();
            want = (start + f) * 44100ull * 1001ull / 60000ull;
            CHECK_EQ_INT((long)PsxIoNow(), (long)want);
        }
    }
    {
        /* two sessions doing the same thing hear the same thing */
        uint32_t first, second;

        long loud1, loud2;

        test_io_restart();
        first = record_scenario(120, &loud1);
        test_io_restart();
        second = record_scenario(120, &loud2);
        CHECK(first == second);
        CHECK(loud1 > 10000);
        CHECK_EQ_INT(loud1, loud2);
    }
    test_io_stop();

    /* real time with an audio device: the clock follows the device, the game's calls and the
       audio thread share the lock */
    SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
    if (SDL_Init(SDL_INIT_AUDIO)) {
        PsxIoConfig config;
        char error[200];
        uint64_t t0, t1;
        Uint64 w0, w1;
        uint8_t result[16];
        char saveDir[] = "/tmp/psxio-test-XXXXXX";

        if (!mkdtemp(saveDir)) {
            return 1;
        }
        memset(&config, 0, sizeof(config));
        config.discPath = test_disc_path();
        config.savePath = saveDir;
        config.fixedStep = 0;
        config.cdSpeed = 1.0;
        CHECK(PsxIoInit(&config, error, sizeof(error)));
        CHECK(g_io.deviceClock);
        SpuInit();
        t0 = PsxIoNow();
        w0 = SDL_GetTicks();
        {
            /* game calls while the audio thread runs: 200 key-ons and 3 CD commands */
            int i;
            uint8_t mode = 5;

            for (i = 0; i < 200; i++) {
                SpuSetKey(1, 1u << (i % 24));
                SpuSetKey(0, 1u << ((i + 5) % 24));
                SDL_Delay(2);
            }
            CHECK_EQ_INT(CdControlB(0xE, &mode, result), 1);
            CHECK_EQ_INT(CdControlB(0x11, NULL, result), 1);
        }
        t1 = PsxIoNow();
        w1 = SDL_GetTicks();
        printf("device clock: %.2f s of audio in %.2f s of wall time\n", (double)(t1 - t0) / 44100.0,
               (w1 - w0) / 1000.0);
        CHECK((double)(t1 - t0) / 44100.0 > (w1 - w0) / 1000.0 * 0.5);
        CHECK((double)(t1 - t0) / 44100.0 < (w1 - w0) / 1000.0 * 1.5);
        PsxIoShutdown();
        {
            char command[300];

            snprintf(command, sizeof(command), "rm -rf '%s'", saveDir);
            if (system(command) != 0) {
                return 1;
            }
        }
        SDL_Quit();
    } else {
        printf("(no SDL audio: real-time part skipped)\n");
    }

    /* an audio device that stops: the game does not wait forever, time follows the fields */
    if (test_io_start(1.0, NULL)) {
        uint8_t result[16], mode = 5;
        Uint64 w0 = SDL_GetTicks();

        g_io.deviceClock = true; /* a device clock that never advances */
        CHECK_EQ_INT(CdControlB(0xE, &mode, result), 1);
        CHECK(!g_io.deviceClock);
        CHECK((SDL_GetTicks() - w0) < 3000);
        test_io_stop();
    }

    /* lock stress: a thread renders as the audio device would, the game thread calls in */
    if (test_io_start(1.0, NULL)) {
        atomic_int stop = 0;
        pthread_t thread;
        int i;

        g_io.deviceClock = true; /* the waits sleep instead of rendering */
        SpuInit();
        pthread_create(&thread, NULL, render_thread, &stop);
        for (i = 0; i < 300; i++) {
            uint8_t result[16];
            uint8_t mode = 5;

            SpuSetKey(1, 1u << (i % 24));
            if (i % 50 == 0) {
                CHECK_EQ_INT(CdControlB(0xE, &mode, result), 1);
            }
        }
        atomic_store(&stop, 1);
        pthread_join(thread, NULL);
        g_io.deviceClock = false;
        test_io_stop();
    }
    return test_report("api");
}
