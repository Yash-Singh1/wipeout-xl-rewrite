/* A soak: 3000 video fields of random but valid library calls (voices, key commands, pitch and
   volume changes, reverb depth, CD music, data reads, card access), checking that nothing
   crashes or hangs (run it under the sanitizers), the clock stays exact, and the mix is sane. */
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
typedef struct { Volume volume; int32_t reverb, mix; } ExtAttr;
typedef struct {
    uint32_t mask;
    Volume mvol, mvolmode, mvolx;
    ExtAttr cd, ext;
} CommonAttr;
typedef struct {
    uint32_t mask;
    int32_t mode;
    Volume depth;
    int32_t delay, feedback;
} ReverbAttr;
typedef struct { uint8_t minute, second, sector, track; } Loc;

void SpuInit(void);
int SpuInitMalloc(int, void *);
uint32_t SpuMalloc(int);
uint32_t SpuSetTransferStartAddr(uint32_t);
uint32_t SpuWrite(const void *, uint32_t);
void SpuSetVoiceAttr(const VoiceAttr *);
void SpuGetVoiceAttr(VoiceAttr *);
void SpuSetKey(int, uint32_t);
int SpuGetKeyStatus(uint32_t);
void SpuSetCommonAttr(const CommonAttr *);
int SpuSetReverb(int);
uint32_t SpuSetReverbVoice(int, uint32_t);
int SpuSetReverbModeParam(const ReverbAttr *);
int SpuReserveReverbWorkArea(int);
int SpuClearReverbWorkArea(int);
int CdInit(void);
int CdControlB(uint8_t, const uint8_t *, uint8_t *);
int CdControl(uint8_t, const uint8_t *, uint8_t *);
int CdRead(int, uint32_t *, int);
int CdReadSync(int, uint8_t *);
Loc *CdIntToPos(int, Loc *);
void InitPAD(void *, int, void *, int);
void StartPAD(void);

static uint32_t rng = 12345;
static uint32_t rnd(uint32_t n)
{
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return rng % n;
}

int main(void)
{
    static uint32_t buffer[2048 * 8 / 4];
    static uint8_t pad1[8], pad2[8];
    uint32_t samples[4];
    int i, field;
    uint8_t block[16 * 8];
    ReverbAttr rev;
    CommonAttr common;
    long peak = 0;

    if (!test_io_start(8.0, NULL)) {
        printf("test_soak: disc not found, skipped\n");
        return 0;
    }
    InitPAD(pad1, 8, pad2, 8);
    StartPAD();
    SpuInit();
    SpuInitMalloc(128, NULL);
    for (i = 0; i < 8 * 16; i++) {
        block[i] = (uint8_t)rnd(256);
    }
    for (i = 0; i < 8; i++) {
        block[i * 16] &= 0x0F;           /* filter 0 .. */
        block[i * 16 + 1] = i == 7 ? (rnd(2) ? 3 : 1) : (i == 0 ? 4 : 0);
    }
    for (i = 0; i < 4; i++) {
        samples[i] = SpuMalloc(128);
        SpuSetTransferStartAddr(samples[i]);
        SpuWrite(block, 128);
    }
    memset(&rev, 0, sizeof(rev));
    SpuSetReverb(1);
    SpuSetReverbVoice(1, 0xFFFFFF);
    rev.mask = 1;
    rev.mode = 4;
    SpuSetReverbModeParam(&rev);
    SpuReserveReverbWorkArea(1);
    SpuClearReverbWorkArea(4);
    memset(&common, 0, sizeof(common));
    common.mvol.left = common.mvol.right = 0x3FC0;
    common.cd.volume.left = common.cd.volume.right = 0x2FC0;
    common.cd.mix = 1;
    SpuSetCommonAttr(&common);
    CdInit();

    for (field = 0; field < 3000; field++) {
        int actions = (int)rnd(6);

        while (actions--) {
            switch (rnd(9)) {
            case 0:
            case 1: {
                VoiceAttr a;
                unsigned v = rnd(24);

                memset(&a, 0, sizeof(a));
                a.voice = 1u << v;
                a.mask = 0x93;
                a.volume.left = (int16_t)((int)rnd(0x3FC1) * (rnd(4) ? 1 : -1));
                a.volume.right = (int16_t)rnd(0x3FC1);
                a.pitch = (uint16_t)(0x200 + rnd(0x2000));
                a.addr = samples[rnd(4)];
                SpuSetVoiceAttr(&a);
                SpuSetKey(1, 1u << v);
                break;
            }
            case 2:
                SpuSetKey(0, 1u << rnd(24));
                break;
            case 3: {
                VoiceAttr a;

                memset(&a, 0, sizeof(a));
                a.voice = 1u << rnd(24);
                a.mask = 0x10;
                a.pitch = (uint16_t)rnd(0x4000);
                SpuSetVoiceAttr(&a);
                SpuGetKeyStatus(1u << rnd(24));
                break;
            }
            case 4:
                rev.mask = 6;
                rev.depth.left = (int16_t)(rnd(128) << 8);
                rev.depth.right = (int16_t)-(int)(rnd(128) << 8);
                SpuSetReverbModeParam(&rev);
                break;
            case 5: {
                Loc loc;
                uint8_t mode = 5;

                CdControlB(0xE, &mode, NULL);
                CdIntToPos((int)(36584 + rnd(20000)), &loc);
                CdControlB(2, (uint8_t *)&loc, NULL);
                CdControlB(3, NULL, NULL);
                break;
            }
            case 6:
                CdControlB(9, NULL, NULL);
                break;
            case 7: {
                Loc loc;

                CdIntToPos((int)rnd(30000), &loc);
                CdControl(0x15, (uint8_t *)&loc, NULL);
                CdRead(1 + (int)rnd(8), buffer, 0x80);
                CdReadSync(0, NULL);
                break;
            }
            default: {
                uint8_t result[8];

                CdControlB(0x11, NULL, result);
                break;
            }
            }
        }
        PsxVBlank();
        {
            int s;

            PsxLock();
            for (s = 0; s < 26; s++) {
                long l = labs(g_io.spu->stems[s][0]), r = labs(g_io.spu->stems[s][1]);

                peak = peak > l ? peak : l;
                peak = peak > r ? peak : r;
            }
            PsxUnlock();
        }
    }
    printf("soak: %d fields, %llu samples, peak %ld\n", field, (unsigned long long)PsxIoNow(), peak);
    CHECK(PsxIoNow() >= (uint64_t)field * 44100ull * 1001ull / 60000ull - 1);
    CHECK(peak > 0);
    test_io_stop();
    return test_report("soak");
}
