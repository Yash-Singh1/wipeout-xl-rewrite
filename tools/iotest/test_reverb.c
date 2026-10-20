/* The reverb unit with the game's preset (Studio C): an impulse sent into it comes back after
   the delay lines' times, decays smoothly to silence, stays inside the work area, and is silent
   when the unit is off or its depth is zero. The exact algorithm is checked sample for sample
   against the console emulator's recording in test_spu_oracle. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../platform/psx/internal.h"
#include "testenv.h"
#include "testutil.h"

typedef struct { int16_t left, right; } Volume;
typedef struct {
    uint32_t voice, mask;
    Volume volume, volmode, volumex;
    uint16_t pitch, note, sample_note;
    int16_t envx;
    uint32_t addr, loop_addr;
    int32_t a_mode, s_mode, r_mode;
    uint16_t ar, dr, sr, rr, sl, adsr1, adsr2;
} VoiceAttr;
typedef struct {
    uint32_t mask;
    int32_t mode;
    Volume depth;
    int32_t delay, feedback;
} ReverbAttr;
typedef struct { Volume volume; int32_t reverb, mix; } ExtAttr;
typedef struct {
    uint32_t mask;
    Volume mvol, mvolmode, mvolx;
    ExtAttr cd, ext;
} CommonAttr;

void SpuInit(void);
int SpuInitMalloc(int, void *);
int SpuMalloc(int);
uint32_t SpuSetTransferStartAddr(uint32_t);
uint32_t SpuWrite(const void *, uint32_t);
int SpuSetReverb(int);
uint32_t SpuSetReverbVoice(int, uint32_t);
int SpuSetReverbModeParam(const ReverbAttr *);
int SpuReserveReverbWorkArea(int);
int SpuClearReverbWorkArea(int);
void SpuSetVoiceAttr(const VoiceAttr *);
void SpuSetKey(int, uint32_t);
void SpuSetCommonAttr(const CommonAttr *);

typedef struct Stats {
    double rms[8];
    long firstNonzero;
    int peak;
} Stats;

/* Plays an impulse and measures the reverb return over `seconds`. */
static void measure(Stats *s, int depth, int reverbOn, int seconds)
{
    uint8_t block[16];
    int address, n, window;
    long total = (long)seconds * SPU_SAMPLE_RATE;
    double sum[8] = {0};
    long count[8] = {0};
    VoiceAttr a;
    ReverbAttr rev;

    memset(block, 0, sizeof(block));
    block[1] = 0x01;           /* one block, then the voice stops */
    block[2 + 2] = 0x77;       /* samples 4..7: 7 * 4096 = 28672 */
    block[2 + 3] = 0x77;
    SpuInit();
    SpuInitMalloc(128, NULL);
    SpuSetReverb(1);
    SpuSetReverbVoice(1, 1);
    memset(&rev, 0, sizeof(rev));
    rev.mask = 1;
    rev.mode = 4;
    SpuSetReverbModeParam(&rev);
    SpuReserveReverbWorkArea(1);
    SpuClearReverbWorkArea(4);
    SpuSetReverb(reverbOn);
    rev.mask = 6;
    rev.depth.left = (int16_t)depth;
    rev.depth.right = (int16_t)depth;
    SpuSetReverbModeParam(&rev);
    {
        CommonAttr c;

        memset(&c, 0, sizeof(c));
        c.mvol.left = c.mvol.right = 0x3FFF;
        SpuSetCommonAttr(&c);
    }
    address = SpuMalloc(16);
    SpuSetTransferStartAddr((uint32_t)address);
    SpuWrite(block, 16);
    memset(&a, 0, sizeof(a));
    a.voice = 1;
    a.mask = 0x93;
    a.volume.left = a.volume.right = 0x3FFF;
    a.pitch = 0x1000;
    a.addr = (uint32_t)address;
    SpuSetVoiceAttr(&a);
    SpuSetKey(1, 1);
    memset(s, 0, sizeof(*s));
    s->firstNonzero = -1;
    PsxLock();
    for (n = 0; n < total; n++) {
        int16_t out[2];
        int level = abs(g_io.spu->stems[24][0]) > abs(g_io.spu->stems[24][1]) ? abs(g_io.spu->stems[24][0])
                                                                              : abs(g_io.spu->stems[24][1]);

        SpuSample(g_io.spu, NULL, out);
        level = abs(g_io.spu->stems[24][0]) | abs(g_io.spu->stems[24][1]);
        if (level && s->firstNonzero < 0) {
            s->firstNonzero = n;
        }
        if (level > s->peak) {
            s->peak = level;
        }
        window = n / SPU_SAMPLE_RATE;
        if (window < 8) {
            sum[window] += (double)g_io.spu->stems[24][0] * g_io.spu->stems[24][0] +
                           (double)g_io.spu->stems[24][1] * g_io.spu->stems[24][1];
            count[window] += 2;
        }
    }
    PsxUnlock();
    for (n = 0; n < 8; n++) {
        s->rms[n] = count[n] ? sqrt(sum[n] / (double)count[n]) : 0;
    }
}

int main(void)
{
    Stats on, off, zero;
    static uint8_t before[0x79020], after[0x79020];
    int i;

    if (!test_io_start(1.0, NULL)) {
        printf("test_reverb: disc not found, skipped\n");
        return 0;
    }
    memcpy(before, g_io.spu->ram, sizeof(before));
    measure(&on, 0x4000, 1, 6);
    memcpy(after, g_io.spu->ram, sizeof(after));
    printf("first return after %ld samples (%.1f ms); peak %d; rms per second:", on.firstNonzero,
           on.firstNonzero * 1000.0 / SPU_SAMPLE_RATE, on.peak);
    for (i = 0; i < 6; i++) {
        printf(" %.1f", on.rms[i]);
    }
    printf("\n");
    CHECK(on.firstNonzero > 20 && on.firstNonzero < 5000);  /* a few ms: the first taps */
    CHECK(on.peak > 100);
    CHECK(on.peak < 32767);
    /* the tail decays: every second is quieter than the one before, and it nearly vanishes */
    for (i = 1; i < 4; i++) {
        CHECK(on.rms[i] < on.rms[i - 1]);
    }
    CHECK(on.rms[4] <= on.rms[3] && on.rms[5] <= on.rms[4]);
    CHECK(on.rms[5] < on.rms[0] * 0.05);
    CHECK(on.rms[0] > 5);
    /* the unit only touches its work area (0x79020 up); the rest of sound RAM below it stays as
       it was, except the parked-voice block and the sample this test uploaded (0x1000-0x101F)
       and the capture buffers (below 0x1000) */
    {
        long changed = 0;

        for (i = 0x1000; i < 0x79020; i++) {
            if (before[i] != after[i] && !(i >= 0x1000 && i < 0x1020)) {
                changed++;
            }
        }
        CHECK_EQ_INT(changed, 0);
    }
    measure(&off, 0x4000, 0, 2);
    CHECK_EQ_INT(off.peak, 0);
    measure(&zero, 0, 1, 2);
    CHECK_EQ_INT(zero.peak, 0);
    test_io_stop();
    return test_report("reverb");
}
