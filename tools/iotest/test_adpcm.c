/* ADPCM decoding of real samples from the disc's sound bank (WIPEOUT2/SOUND/SAMPLES.WAD)
   against an independent decoder written from the psx-spx description with floating-point
   floor() arithmetic, and a hand-computed block. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../platform/psx/disc.h"
#include "../../platform/psx/spu.h"
#include "testenv.h"
#include "testutil.h"

/* Reference decoder: sample = floor(nibble * 4096 / 2^shift) + floor(older terms / 64). */
static void reference_block(const uint8_t *block, double *old, double *older, int out[28])
{
    static const double f0[5] = {0, 60, 115, 98, 122};
    static const double f1[5] = {0, 0, -52, -55, -60};
    int shift = block[0] & 15, filter = (block[0] >> 4) & 15, i;

    if (shift > 12) {
        shift = 9;
    }
    for (i = 0; i < 28; i++) {
        int nibble = i % 2 ? block[2 + i / 2] >> 4 : block[2 + i / 2] & 15;
        double s;

        if (nibble > 7) {
            nibble -= 16;
        }
        s = floor(nibble * 4096.0 / ldexp(1.0, shift));
        if (filter < 5) {
            s += floor(*old * f0[filter] / 64.0) + floor(*older * f1[filter] / 64.0);
        }
        if (s > 32767) s = 32767;
        if (s < -32768) s = -32768;
        out[i] = (int)s;
        *older = *old;
        *old = s;
    }
}

static uint32_t be32(const uint8_t *p)
{
    return (uint32_t)p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3];
}

int main(void)
{
    const char *cue = test_disc_path();
    char error[256];
    Disc *disc;
    int32_t lba;
    uint32_t size, offset;
    uint8_t *wad;
    unsigned count, i, samples = 0;
    long blocks = 0, mismatches = 0;

    /* A hand-computed block: filter 1 (history * 60/64), shift 12, nibbles 1, 2, -8, -1 */
    {
        uint8_t block[16] = {0x1C, 0, 0x21, 0xF8, 0x00};
        int16_t history[2] = {0, 0}, out[28];
        double old = 0, older = 0;
        int want[28], i2;

        SpuDecodeBlock(block, history, out);
        reference_block(block, &old, &older, want);
        for (i2 = 0; i2 < 28; i2++) {
            CHECK_EQ_INT(out[i2], want[i2]);
        }
        CHECK_EQ_INT(out[0], 1);   /* nibble 1, no history */
        CHECK_EQ_INT(out[1], 2 + 0); /* floor(1 * 60 / 64) = 0 */
        CHECK_EQ_INT(out[2], -8 + 1); /* floor(2 * 60 / 64) = 1 */
        CHECK_EQ_INT(out[3], -1 - 7); /* floor(-7 * 60 / 64) = -7: negative products round down */
    }

    if (!cue) {
        printf("test_adpcm: disc not found, real-sample part skipped\n");
        return test_report("adpcm");
    }
    disc = DiscOpen(cue, error, sizeof(error));
    CHECK(disc != NULL);
    if (!disc || !DiscFindFile(disc, "\\WIPEOUT2\\SOUND\\SAMPLES.WAD;1", &lba, &size)) {
        fprintf(stderr, "cannot find SAMPLES.WAD\n");
        return 1;
    }
    wad = malloc(((size + 2047) / 2048) * 2048);
    CHECK(DiscReadData(disc, lba, (int)((size + 2047) / 2048), wad));
    /* the wad: a 16-bit count, then count entries of 25 bytes (name, word, size, flag), then the files */
    count = wad[0] | wad[1] << 8;
    CHECK(count > 20 && count < 200);
    offset = 2 + count * 25;
    for (i = 0; i < count; i++) {
        uint32_t fsize = wad[2 + i * 25 + 20] | wad[2 + i * 25 + 21] << 8 | wad[2 + i * 25 + 22] << 16 |
                         (uint32_t)wad[2 + i * 25 + 23] << 24;
        const uint8_t *vag = wad + offset;
        uint32_t dataSize;
        int16_t history[2] = {0, 0};
        double old = 0, older = 0;
        uint32_t b;

        CHECK(!memcmp(vag, "VAGp", 4));
        dataSize = be32(vag + 12);
        CHECK(dataSize + 48 <= fsize + 16);
        for (b = 0; b + 16 <= dataSize; b += 16) {
            int16_t out[28];
            int want[28], k;

            SpuDecodeBlock(vag + 48 + b, history, out);
            reference_block(vag + 48 + b, &old, &older, want);
            for (k = 0; k < 28; k++) {
                if (out[k] != want[k]) {
                    mismatches++;
                }
            }
            blocks++;
        }
        samples++;
        offset += fsize;
    }
    printf("decoded %u samples, %ld blocks (%ld ADPCM sample frames)\n", samples, blocks, blocks * 28);
    CHECK(blocks > 20000);
    CHECK_EQ_INT(mismatches, 0);
    free(wad);
    DiscClose(disc);
    return test_report("adpcm");
}
