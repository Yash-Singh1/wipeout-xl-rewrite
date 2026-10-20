#include "internal.h"

#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
static SRWLOCK ioLock = SRWLOCK_INIT;
void PsxLock(void) { AcquireSRWLockExclusive(&ioLock); }
void PsxUnlock(void) { ReleaseSRWLockExclusive(&ioLock); }
static void sleep_ms(int ms) { Sleep((DWORD)ms); }
#else
#include <pthread.h>
#include <time.h>
static pthread_mutex_t ioLock = PTHREAD_MUTEX_INITIALIZER;
void PsxLock(void) { pthread_mutex_lock(&ioLock); }
void PsxUnlock(void) { pthread_mutex_unlock(&ioLock); }
static void sleep_ms(int ms)
{
    struct timespec t;

    t.tv_sec = ms / 1000;
    t.tv_nsec = (long)(ms % 1000) * 1000000L;
    nanosleep(&t, NULL);
}
#endif

PsxIoState g_io;

/* ---- WAV capture --------------------------------------------------------------------- */

static void put32(FILE *f, uint32_t v)
{
    uint8_t b[4] = {(uint8_t)v, (uint8_t)(v >> 8), (uint8_t)(v >> 16), (uint8_t)(v >> 24)};

    fwrite(b, 1, 4, f);
}

static void wav_header(FILE *f, uint32_t frames)
{
    fwrite("RIFF", 1, 4, f);
    put32(f, frames * 4 + 36);
    fwrite("WAVEfmt ", 1, 8, f);
    put32(f, 16);
    put32(f, 0x00020001); /* PCM, stereo */
    put32(f, SPU_SAMPLE_RATE);
    put32(f, SPU_SAMPLE_RATE * 4);
    put32(f, 0x00100004); /* 4 bytes a frame, 16 bits */
    fwrite("data", 1, 4, f);
    put32(f, frames * 4);
}

/* ---- rendering ----------------------------------------------------------------------- */

void PsxAudioRenderLocked(int16_t *out, uint32_t frames)
{
    int16_t scratch[2];
    uint32_t i;

    for (i = 0; i < frames; i++) {
        int16_t cd[2];
        int16_t *dest = out ? out + (size_t)i * 2 : scratch;

        CdDriveSample(&g_io.cd, cd);
        SpuSample(g_io.spu, cd, dest);
        if (g_io.wav) {
            uint8_t b[4] = {(uint8_t)dest[0], (uint8_t)((uint16_t)dest[0] >> 8), (uint8_t)dest[1],
                            (uint8_t)((uint16_t)dest[1] >> 8)};

            fwrite(b, 1, 4, g_io.wav);
            g_io.wavFrames++;
        }
    }
}

void PsxAudioRender(int16_t *out, uint32_t frames)
{
    PsxLock();
    PsxAudioRenderLocked(out, frames);
    PsxUnlock();
}

uint64_t PsxIoNow(void)
{
    uint64_t now;

    PsxLock();
    now = g_io.cd.now;
    PsxUnlock();
    return now;
}

/* With an audio device the clock is the device's. If it stops (the device is stuck or was
   removed) the game would wait forever; after a second without progress the device is given up
   and time follows the video fields. */
static void watch_device(uint64_t now)
{
    static uint64_t lastNow;
    static unsigned stalledMs;

    if (now != lastNow) {
        lastNow = now;
        stalledMs = 0;
    } else if (++stalledMs >= 1000) {
        PortLog("audio: the output device stopped; time follows the video fields\n");
        stalledMs = 0;
        PsxAudioDeviceClose();
        g_io.deviceClock = false;
    }
}

void PsxIoWait(uint64_t deadline)
{
    uint64_t now = PsxIoNow();

    if (g_io.deviceClock) {
        watch_device(now);
    }
    if (deadline > now && deadline - now < SAMPLES_PER_FIELD) {
        if (g_io.deviceClock) {
            sleep_ms(1);
        } else {
            PsxAudioRender(NULL, (uint32_t)(deadline - now));
        }
        return;
    }
    PsxVBlank();
}

void PsxIoField(uint64_t field)
{
    if (!g_io.initialized) {
        return;
    }
    if (!g_io.deviceClock) {
        uint64_t target = field * SPU_SAMPLE_RATE * FIELD_RATE_DEN / FIELD_RATE_NUM;

        PsxLock();
        if (target > g_io.cd.now) {
            PsxAudioRenderLocked(NULL, (uint32_t)(target - g_io.cd.now));
        }
        PsxUnlock();
    }
    PsxIoService();
    PsxPadField(field);
}

void PsxIoService(void)
{
    CdEventKind kind;

    for (;;) {
        bool any;

        PsxLock();
        any = CdDrivePopEvent(&g_io.cd, &kind);
        PsxUnlock();
        if (!any) {
            break;
        }
        if (kind == CDE_COMPLETE) {
            PsxDeliverEvent(EVENT_CLASS_HWCD, EVENT_SPEC_COMPLETE);
        } else if (kind == CDE_ERROR) {
            PsxDeliverEvent(EVENT_CLASS_HWCD, EVENT_SPEC_ERROR);
        }
    }
    PsxCdService();
}

void PsxSetExecHook(PsxExecHook hook)
{
    g_io.exec = hook;
}

/* ---- start and stop -------------------------------------------------------------------- */

int PsxIoInit(const PsxIoConfig *config, char *error, int errorSize)
{
    if (g_io.initialized) {
        return 1;
    }
    memset(&g_io, 0, sizeof(g_io));
    g_io.disc = DiscOpen(config->discPath, error, errorSize);
    if (!g_io.disc) {
        return 0;
    }
    g_io.spu = calloc(1, sizeof(Spu));
    if (!g_io.spu) {
        DiscClose(g_io.disc);
        return 0;
    }
    SpuReset(g_io.spu);
    CdDriveInit(&g_io.cd, g_io.disc, config->cdSpeed);
    g_io.fixedStep = config->fixedStep != 0;
    snprintf(g_io.savePath, sizeof(g_io.savePath), "%s", config->savePath ? config->savePath : ".");
    if (config->wavPath) {
        g_io.wav = fopen(config->wavPath, "wb");
        if (g_io.wav) {
            wav_header(g_io.wav, 0);
        }
    }
    PsxPadInit(config->padScript);
    PsxCardInit(g_io.savePath);
    g_io.initialized = true;
    if (!g_io.fixedStep && !config->noDevice) {
        g_io.deviceClock = PsxAudioDeviceOpen();
        if (!g_io.deviceClock) {
            PortLog("audio: no output device; time follows the video fields\n");
        }
    }
    return 1;
}

void PsxIoShutdown(void)
{
    if (!g_io.initialized) {
        return;
    }
    PsxAudioDeviceClose();
    g_io.deviceClock = false;
    PsxCardShutdown();
    PsxPadShutdown();
    if (g_io.wav) {
        fseek(g_io.wav, 0, SEEK_SET);
        wav_header(g_io.wav, g_io.wavFrames);
        fclose(g_io.wav);
        g_io.wav = NULL;
    }
    free(g_io.spu);
    DiscClose(g_io.disc);
    memset(&g_io, 0, sizeof(g_io));
}
