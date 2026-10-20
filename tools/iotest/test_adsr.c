/* Envelope curves against the psx-spx formulas, written here independently of the model:
   a counter that counts whole samples between steps, instead of the model's 0x8000 accumulator.

     rate -> shift = rate >> 2, step = 7 - (rate & 3) for an increase, -8 + (rate & 3) for a decrease
     a step of (step << max(0, 11 - shift)) is applied every (1 << max(0, shift - 11)) samples
     exponential increase from 0x6000 up behaves like rate + 8; exponential decrease scales the step
     by level / 0x8000.
   Then the ADSR phases of a voice: attack to full, decay to the sustain level, sustain, release. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../platform/psx/spu.h"
#include "testutil.h"

typedef struct Ref {
    int level;
    long counter;
    int rate;
    int decrease, exponential;
} Ref;

static int ref_clamp(int v)
{
    return v > 32767 ? 32767 : v < 0 ? 0 : v;
}

static void ref_tick(Ref *r)
{
    int rate = r->rate, shift, step;
    long cycles;

    if (!r->decrease && r->exponential && r->level >= 0x6000) {
        rate += 8;
    }
    shift = rate >> 2;
    step = r->decrease ? -8 + (rate & 3) : 7 - (rate & 3);
    if (shift < 11) {
        step *= 1 << (11 - shift);
    }
    cycles = shift > 11 ? 1L << (shift - 11) : 1;
    if (r->decrease && r->exponential) {
        step = (step * r->level) >> 15;
    }
    if (++r->counter >= cycles) {
        r->counter = 0;
        r->level = ref_clamp(r->level + step);
    }
}

static void compare(unsigned rate, bool decrease, bool exponential, int start, long samples)
{
    SpuEnvelope e;
    Ref ref = {start, 0, (int)rate, decrease, exponential};
    int16_t level = (int16_t)start;
    long i, bad = 0;

    SpuEnvelopeInit(&e, rate, 127, decrease, exponential, false);
    for (i = 0; i < samples; i++) {
        ref_tick(&ref);
        SpuEnvelopeTick(&e, &level);
        if (level != ref.level && bad++ < 3) {
            fprintf(stderr, "rate %u %s %s: sample %ld model %d reference %d\n", rate,
                    decrease ? "decrease" : "increase", exponential ? "exp" : "lin", i, level, ref.level);
        }
    }
    CHECK_EQ_INT(bad, 0);
}

int main(void)
{
    static const unsigned rates[] = {0, 1, 2, 3, 4, 7, 20, 31, 39, 40, 41, 42, 43, 44, 45, 46, 47,
                                     48, 52, 60, 70, 80, 90, 100, 107};
    size_t i;

    for (i = 0; i < sizeof(rates) / sizeof(rates[0]); i++) {
        unsigned rate = rates[i];
        long span = rate < 60 ? 300 : 70000;

        compare(rate, false, false, 0, span);
        compare(rate, false, true, 0, span);
        compare(rate, true, false, 32767, span);
        compare(rate, true, true, 32767, span);
    }
    /* A rate of 127 never ticks. */
    {
        SpuEnvelope e;
        int16_t level = 100;
        int n;

        SpuEnvelopeInit(&e, 127, 127, false, false, false);
        for (n = 0; n < 200000; n++) {
            SpuEnvelopeTick(&e, &level);
        }
        CHECK_EQ_INT(level, 100);
    }
    /* Closed form: a linear attack with rate 0 adds 7 << 11 a sample. */
    {
        SpuEnvelope e;
        int16_t level = 0;

        SpuEnvelopeInit(&e, 0, 127, false, false, false);
        SpuEnvelopeTick(&e, &level);
        CHECK_EQ_INT(level, 7 << 11);
        SpuEnvelopeTick(&e, &level);
        CHECK_EQ_INT(level, 14 << 11);
        SpuEnvelopeTick(&e, &level);
        CHECK_EQ_INT(level, 32767); /* 21 << 11 saturates */
    }

    /* A voice with the game's envelope (ADSR1 = ADSR2 = 0): attack, decay to the sustain level
       0x800, then the "sustain increase" climbs back; check the phases of a looping voice. */
    {
        static Spu spu;
        uint8_t block[16];
        int n, maxLevel = 0;
        bool sawDecay = false;

        SpuReset(&spu);
        memset(block, 0, sizeof(block));
        block[0] = 0x0C;      /* shift 12, filter 0 */
        block[1] = 0x03;      /* loop end + repeat */
        memset(block + 2, 0x11, 14);
        SpuWriteRam(&spu, 0x1000, block, 16);
        SpuWriteRegister(&spu, SPU_CONTROL, 0xC000);
        SpuWriteRegister(&spu, 0x0, 0x3FFF);
        SpuWriteRegister(&spu, 0x2, 0x3FFF);
        SpuWriteRegister(&spu, 0x4, 0x1000);
        SpuWriteRegister(&spu, 0x6, 0x200);
        SpuWriteRegister(&spu, SPU_KEY_ON, 1);
        for (n = 0; n < 40; n++) {
            int16_t out[2];

            SpuSample(&spu, NULL, out);
            if (spu.voice[0].reg[6] > maxLevel) {
                maxLevel = spu.voice[0].reg[6];
            }
            if (spu.voice[0].phase == ADSR_DECAY) {
                sawDecay = true;
            }
        }
        CHECK_EQ_INT(maxLevel, 32767);
        CHECK(sawDecay);
        /* the sustain phase raises the level again at the sustain rate */
        CHECK(spu.voice[0].phase == ADSR_SUSTAIN);
        /* key off: the release (rate 0, linear) empties it within a few samples and stops the voice */
        SpuWriteRegister(&spu, SPU_KEY_OFF, 1);
        for (n = 0; n < 8; n++) {
            int16_t out[2];

            SpuSample(&spu, NULL, out);
        }
        CHECK_EQ_INT(spu.voice[0].reg[6], 0);
        CHECK(spu.voice[0].phase == ADSR_OFF);
    }
    return test_report("adsr");
}
