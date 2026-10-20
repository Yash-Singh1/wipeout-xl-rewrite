/* Shared by the modules of platform/psx that implement the sound, disc, controller and
   memory card libraries. Not included by game code. */
#ifndef PSX_INTERNAL_H
#define PSX_INTERNAL_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "cdrom.h"
#include "psx_io.h"
#include "spu.h"

#define FIELD_RATE_NUM 60000ull /* 59.94 fields a second = 60000 / 1001 */
#define FIELD_RATE_DEN 1001ull
/* Samples in one video field, rounded up (for deciding whether a wait is shorter than one). */
#define SAMPLES_PER_FIELD 736

/* Provided by the platform's video timing (platform/psx/libetc.c): one field passes. */
void PsxVBlank(void);
/* Diagnostics (platform/video.c). */
void PortLog(const char *format, ...);

/* ---- the lock ---------------------------------------------------------------------- */

void PsxLock(void);
void PsxUnlock(void);

/* ---- state ------------------------------------------------------------------------- */

typedef struct PsxIoState {
    bool initialized;
    bool fixedStep;
    bool deviceClock;     /* an audio device thread advances time */
    Spu *spu;
    Disc *disc;
    CdDrive cd;
    PsxExecHook exec;
    FILE *wav;
    uint32_t wavFrames;
    char savePath[1024];
} PsxIoState;

extern PsxIoState g_io;

/* ---- time -------------------------------------------------------------------------- */

/* Renders `frames` samples of output (NULL discards) and advances the clock. Takes the lock. */
void PsxAudioRender(int16_t *out, uint32_t frames);
/* As above, with the lock already held. */
void PsxAudioRenderLocked(int16_t *out, uint32_t frames);
uint64_t PsxIoNow(void);
/* Lets time pass while the game waits for something due at `deadline` (device sample
   time; 0 if unknown): fields go by through PsxVBlank, or a short wait is slept/advanced. */
void PsxIoWait(uint64_t deadline);
/* Delivers queued CD and card events and runs event handlers. Call on the game thread
   with the lock NOT held. */
void PsxIoService(void);

/* audio_sdl.c */
bool PsxAudioDeviceOpen(void);
void PsxAudioDeviceClose(void);

/* ---- BIOS events (libapi.c) -------------------------------------------------------- */

#define EVENT_CLASS_HWCD 0xF0000003u
#define EVENT_SPEC_COMPLETE 0x0020u
#define EVENT_SPEC_ERROR 0x8000u

/* Posts an event. Takes the lock; runs interrupt-mode handlers on the calling thread. */
void PsxDeliverEvent(uint32_t eventClass, uint32_t eventSpec);
bool PsxInCriticalSection(void);

/* ---- CD file I/O (libcd.c) ---------------------------------------------------------- */

/* Completes a finished read job into the game's buffer; call on the game thread. */
void PsxCdService(void);

/* ---- pad (pad.c) ---------------------------------------------------------------------- */

void PsxPadField(uint64_t field);
void PsxPadInit(const char *script);
void PsxPadShutdown(void);

/* ---- memory card (libcard.c) ----------------------------------------------------------- */

void PsxCardInit(const char *directory);
void PsxCardShutdown(void);

#endif
