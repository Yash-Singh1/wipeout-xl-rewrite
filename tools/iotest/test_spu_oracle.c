/* Replays a console emulator's recording of the retail game through the SPU model and
   compares every voice, the reverb and the master mix sample for sample.

   The recording (not part of the repository) is a directory with
     spu.bin               time-stamped SPU register and RAM writes
     voices/voiceNN.wav    each voice after its volume, 44.1 kHz stereo
     voices/reverb.wav     the reverb return
     voices/cd.wav         the CD input after the SPU's CD volume
     mix.wav               the final output
   Set PSX_SPU_ORACLE to that directory; without it the test is skipped. */
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../platform/psx/spu.h"
#include "testutil.h"

static uint32_t rd32(const uint8_t *p)
{
    return p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static FILE *open_wav(const char *dir, const char *name)
{
    char path[1024];
    uint8_t header[44];
    FILE *f;

    snprintf(path, sizeof(path), "%s/%s", dir, name);
    f = fopen(path, "rb");
    if (!f || fread(header, 1, 44, f) != 44 || memcmp(header, "RIFF", 4)) {
        fprintf(stderr, "cannot open %s\n", path);
        exit(1);
    }
    return f;
}

typedef struct Compare {
    FILE *file;
    uint64_t mismatches;
    uint64_t first;
    int max_error;
} Compare;

static void compare_sample(Compare *c, const int16_t got[2], uint64_t index)
{
    uint8_t bytes[4];
    int16_t want[2];
    int ch;

    if (fread(bytes, 1, 4, c->file) != 4) {
        fprintf(stderr, "reference ended early\n");
        exit(1);
    }
    want[0] = (int16_t)(bytes[0] | bytes[1] << 8);
    want[1] = (int16_t)(bytes[2] | bytes[3] << 8);
    for (ch = 0; ch < 2; ch++) {
        if (got[ch] != want[ch]) {
            int error = abs(got[ch] - want[ch]);

            if (!c->mismatches) {
                c->first = index;
            }
            c->mismatches++;
            if (error > c->max_error) {
                c->max_error = error;
            }
        }
    }
}

static int16_t clip(int32_t v)
{
    return (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
}

int main(void)
{
    const char *dir = getenv("PSX_SPU_ORACLE");
    static Compare voices[24];
    Compare reverb = {0}, mix = {0};
    FILE *cd, *trace;
    char path[1024];
    Spu *spu;
    uint8_t header[16], payload[8];
    uint64_t events = 0, total;
    int i;

    if (!dir) {
        printf("test_spu_oracle: skipped (set PSX_SPU_ORACLE)\n");
        return 0;
    }
    spu = calloc(1, sizeof(*spu));
    SpuReset(spu);
    for (i = 0; i < 24; i++) {
        char name[32];

        snprintf(name, sizeof(name), "voices/voice%02d.wav", i);
        voices[i].file = open_wav(dir, name);
    }
    reverb.file = open_wav(dir, "voices/reverb.wav");
    mix.file = open_wav(dir, "mix.wav");
    cd = open_wav(dir, "voices/cd.wav");
    snprintf(path, sizeof(path), "%s/spu.bin", dir);
    trace = fopen(path, "rb");
    if (!trace) {
        perror(path);
        return 1;
    }
    {
        /* the length of the recording, from the mix file */
        long here = ftell(mix.file);

        fseek(mix.file, 0, SEEK_END);
        total = (uint64_t)(ftell(mix.file) - 44) / 4;
        fseek(mix.file, here, SEEK_SET);
    }
#define RENDER_UNTIL(until)                                                                    \
    while (spu->samples < (until)) {                                                           \
        int16_t out[2], ref[2], cdin[2];                                                       \
        uint8_t cdbytes[4];                                                                    \
        int32_t sum;                                                                           \
        int c;                                                                                 \
        uint64_t n = spu->samples;                                                             \
        SpuSample(spu, NULL, out);                                                             \
        for (i = 0; i < 24; i++) compare_sample(&voices[i], spu->stems[i], n);                 \
        compare_sample(&reverb, spu->stems[24], n);                                            \
        if (fread(cdbytes, 1, 4, cd) != 4) return 1;                                           \
        cdin[0] = (int16_t)(cdbytes[0] | cdbytes[1] << 8);                                     \
        cdin[1] = (int16_t)(cdbytes[2] | cdbytes[3] << 8);                                     \
        for (c = 0; c < 2; c++) {                                                              \
            sum = cdin[c];                                                                     \
            for (i = 0; i < 25; i++) sum += spu->stems[i][c];                                  \
            ref[c] = clip((int32_t)(((int64_t)clip(sum) * spu->main[c].level) >> 15));         \
        }                                                                                      \
        compare_sample(&mix, ref, n);                                                          \
    }
    while (fread(header, 1, 16, trace) == 16) {
        uint64_t sample = rd32(header) | (uint64_t)rd32(header + 4) << 32;
        uint32_t kind = rd32(header + 8);
        uint32_t size = rd32(header + 12);

        if (size != 8 || fread(payload, 1, 8, trace) != 8 || sample < spu->samples || sample > total) {
            fprintf(stderr, "bad event %" PRIu64 "\n", events);
            return 1;
        }
        RENDER_UNTIL(sample)
        if (kind == 1) {
            SpuWriteRegister(spu, rd32(payload), (uint16_t)rd32(payload + 4));
        } else {
            uint8_t b[2] = {payload[4], payload[5]};

            SpuWriteRam(spu, rd32(payload), b, 2);
        }
        events++;
    }
    RENDER_UNTIL(total)
    printf("replayed %" PRIu64 " events, %" PRIu64 " samples\n", events, total);
    for (i = 0; i < 24; i++) {
        CHECK_EQ_INT((long)voices[i].mismatches, 0);
    }
    CHECK_EQ_INT((long)reverb.mismatches, 0);
    CHECK_EQ_INT((long)mix.mismatches, 0);
    return test_report("spu_oracle");
}
