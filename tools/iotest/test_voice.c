/* Voices through the PsyQ calls the game makes: upload a sample, set pitch and address, key on,
   and check what the hardware does: the position advances by pitch / 4096 samples per output
   sample, loops follow the ADPCM flags, a one-shot stops itself (key status 3), key off releases
   (status 2, then 0), and a voice can be reused again and again without ever sticking. */
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

void SpuInit(void);
int SpuInitMalloc(int, void *);
int SpuMalloc(int);
void SpuFree(int);
uint32_t SpuSetTransferStartAddr(uint32_t);
uint32_t SpuWrite(const void *, uint32_t);
void SpuSetVoiceAttr(const VoiceAttr *);
void SpuGetVoiceAttr(VoiceAttr *);
void SpuSetKey(int, uint32_t);
int SpuGetKeyStatus(uint32_t);

/* A sample of `blocks` ADPCM blocks (shift 0, filter 0) whose nibbles ramp, with the given
   flags on the first and last block (loop start / end / repeat bits). */
static int upload(int blocks, uint8_t firstFlags, uint8_t lastFlags)
{
    uint8_t *data = calloc((size_t)blocks, 16);
    int address, b, i;

    for (b = 0; b < blocks; b++) {
        data[b * 16] = 0x00;
        for (i = 0; i < 14; i++) {
            data[b * 16 + 2 + i] = (uint8_t)(((i * 2 + 1) % 7) << 4 | ((i * 2) % 7));
        }
        data[b * 16 + 1] = b == 0 ? firstFlags : b == blocks - 1 ? lastFlags : 0;
        if (blocks == 1) {
            data[1] = (uint8_t)(firstFlags | lastFlags);
        }
    }
    address = SpuMalloc(blocks * 16);
    SpuSetTransferStartAddr((uint32_t)address);
    SpuWrite(data, (uint32_t)blocks * 16);
    free(data);
    return address;
}

/* Sets the voice up (volume, pitch, start address) as the game's SfxPlay does. */
static void setup(int voice, int address, int pitch, int volume)
{
    VoiceAttr a;

    memset(&a, 0, sizeof(a));
    a.voice = 1u << voice;
    a.mask = 0x93; /* volumes, pitch, address */
    a.volume.left = (int16_t)volume;
    a.volume.right = (int16_t)volume;
    a.pitch = (uint16_t)pitch;
    a.addr = (uint32_t)address;
    SpuSetVoiceAttr(&a);
}

static void play(int voice, int address, int pitch, int volume)
{
    setup(voice, address, pitch, volume);
    SpuSetKey(1, 1u << voice);
}

static void run(int samples)
{
    PsxAudioRender(NULL, (uint32_t)samples);
}

int main(void)
{
    const Spu *spu;
    int loop, oneShot, i, pass;

    if (!test_io_start(1.0, NULL)) {
        printf("test_voice: disc not found, skipped\n");
        return 0;
    }
    spu = g_io.spu;
    SpuInit();
    SpuInitMalloc(128, NULL);
    loop = upload(3, 0x04, 0x03);       /* loop start ... loop end + repeat */
    oneShot = upload(3, 0x00, 0x01);    /* loop end without repeat */

    /* the position advances pitch / 4096 samples per output sample */
    {
        static const int pitches[] = {0x1000, 0x0800, 0x0400, 0x1555, 0x2000, 0x3FFF};
        size_t p;

        for (p = 0; p < sizeof(pitches) / sizeof(pitches[0]); p++) {
            unsigned counter;
            int k = 200;

            play(0, loop, pitches[p], 0x3FFF);
            run(1);            /* the key-on takes effect after this sample */
            run(k);
            counter = spu->voice[0].counter;
            CHECK_EQ_INT((unsigned)(k * pitches[p]) % (28 * 4096), counter);
            SpuSetKey(0, 1);
            run(20);
        }
    }

    /* a looping sample keeps playing, sets ENDX when it wraps, and returns to the loop start */
    play(1, loop, 0x1000, 0x3FFF);
    run(2);                    /* the key-on, then the first sample: the envelope leaves zero */
    CHECK_EQ_INT(SpuGetKeyStatus(2), 1);
    CHECK((spu->endx & 2) == 0);
    run(28 * 3);               /* three blocks of 28 samples at 44.1 kHz */
    CHECK(spu->endx & 2);
    CHECK_EQ_INT(spu->voice[1].reg[7] * 8, loop);
    CHECK(spu->voice[1].address >= (uint32_t)loop && spu->voice[1].address < (uint32_t)loop + 48);
    run(28 * 3 * 4);
    CHECK_EQ_INT(SpuGetKeyStatus(2), 1);
    CHECK(spu->voice[1].phase != ADSR_OFF);

    /* a one-shot ends by itself and the library reports "keyed on but silent" */
    play(2, oneShot, 0x1000, 0x3FFF);
    run(2);
    CHECK_EQ_INT(SpuGetKeyStatus(4), 1);
    run(28 * 3 + 2);
    CHECK_EQ_INT(spu->voice[2].reg[6], 0);
    CHECK_EQ_INT(SpuGetKeyStatus(4), 3);
    SpuSetKey(0, 4);
    CHECK_EQ_INT(SpuGetKeyStatus(4), 0);

    /* key off: releasing, then off */
    SpuSetKey(0, 2);
    run(1);
    CHECK_EQ_INT(SpuGetKeyStatus(2), 2 * (spu->voice[1].reg[6] != 0));
    run(16);
    CHECK_EQ_INT(SpuGetKeyStatus(2), 0);

    /* every voice plays at once, independently */
    for (i = 0; i < 24; i++) {
        setup(i, loop, 0x1000 + i * 64, 0x1000);
    }
    SpuSetKey(1, 0xFFFFFF);
    run(2);
    for (i = 0; i < 24; i++) {
        CHECK_EQ_INT(SpuGetKeyStatus(1u << i), 1);
    }
    run(5000);
    for (i = 0; i < 24; i++) {
        CHECK(spu->voice[i].phase == ADSR_SUSTAIN || spu->voice[i].phase == ADSR_DECAY);
        CHECK(spu->voice[i].reg[6] != 0);
    }
    /* SfxKillAllVoices keys the voices off one call at a time, all within one sample period:
       every command takes effect */
    for (i = 0; i < 24; i++) {
        SpuSetKey(0, 1u << i);
    }
    run(50);
    for (i = 0; i < 24; i++) {
        CHECK_EQ_INT(SpuGetKeyStatus(1u << i), 0);
        CHECK(spu->voice[i].phase == ADSR_OFF);
    }
    /* and key-ons given one at a time the same way */
    for (i = 0; i < 24; i++) {
        setup(i, loop, 0x1000, 0x1000);
        SpuSetKey(1, 1u << i);
    }
    run(2);
    for (i = 0; i < 24; i++) {
        CHECK_EQ_INT(SpuGetKeyStatus(1u << i), 1);
    }
    SpuSetKey(0, 0xFFFFFF);
    run(50);

    /* reuse: play a one-shot on the same voice 300 times, each time waiting for the end the way
       the game does (key status 0 or 3) and playing again: it never sticks */
    for (pass = 0; pass < 300; pass++) {
        int waited = 0, status;

        play(5, oneShot, 0x1000 + (pass % 7) * 100, 0x3FFF);
        run(2);
        while ((status = SpuGetKeyStatus(1u << 5)) == 1 && waited < 400) {
            run(7);
            waited += 7;
        }
        CHECK_EQ_INT(status, 3);
    }

    /* the output of a constant-valued sample follows volume x envelope x 2^-15 per stage */
    {
        uint8_t block[16];
        int16_t out[2];
        int addr, n;

        memset(block, 0, sizeof(block));
        block[0] = 0x0C;       /* shift 12: a nibble of 7 is 7, so use shift 0 below */
        block[0] = 0x00;
        block[1] = 0x06;       /* loop start + repeat... */
        block[1] = 0x07;       /* start, end, repeat: one block loops forever */
        memset(block + 2, 0x44, 14); /* every nibble 4: constant 4 * 4096 = 16384 */
        addr = SpuMalloc(16);
        SpuSetTransferStartAddr((uint32_t)addr);
        SpuWrite(block, 16);
        play(7, addr, 0x1000, 0x2000);
        run(2000);
        {
            const int16_t flat[4] = {16384, 16384, 16384, 16384};
            int gauss, last, expected;

            PsxLock();
            for (n = 0; n < 10; n++) {
                SpuSample(g_io.spu, NULL, out);
            }
            PsxUnlock();
            /* voice 7 settled: the interpolated sample (the four Gaussian weights of equal samples
               sum to just under 1), times the envelope 0x7FFF, times the voice volume 0x4000 */
            gauss = SpuGaussian(flat, 0);
            CHECK(gauss > 16300 && gauss <= 16384);
            last = (gauss * 32767) >> 15;
            expected = (last * 0x4000) >> 15;
            CHECK_EQ_INT(g_io.spu->stems[7][0], expected);
            CHECK_EQ_INT(g_io.spu->stems[7][1], expected);
        }
        SpuSetKey(0, 1u << 7);
        run(50);
    }
    test_io_stop();
    return test_report("voice");
}
