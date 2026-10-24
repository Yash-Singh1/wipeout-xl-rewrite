/* The game's sounds with the Sega Saturn's disc (saturn.c): samples.wad made from the Saturn's sound\gamesnds.rbh.

   The Saturn's container: block 0 a table of 41 records of 20 bytes (big-endian: 0x10, 0xff, the sound RAM address in
   16 bytes, the length in samples, 0x7000, 0, 0x80, whether it loops (the whole sample), loop start 0 and end, the
   rate (11025), 0), blocks 1-41 the samples, 8-bit signed, mono. Every one of the PlayStation's 39 has its Saturn
   counterpart (30 the same recordings, the countdown, voices and wall hit others, found from the Saturn program's
   calls); the 7 that loop are the PlayStation's 7 looping ones. The game reads samples.wad's entries in its order (not
   by name): the PlayStation's names in its order, each a VAG (a 48-byte header, then 16-byte SPU ADPCM blocks, laid out
   as the PlayStation's: a one-shot ends with a block that stops the voice, a loop's last block jumps back to its first,
   the loop resampled to whole blocks, a pitch change below 0.2%). */
#include <SDL3/SDL.h>
#include <string.h>

#include "saturn.h"

/* The Saturn's sample for each of the PlayStation's, in samples.wad's order (shipship, weapnhit, electro, engines...). */
static const int saturnSample[39] = {28, 3,  34, 7,  38, 26, 0,  1,  10, 30, 32, 23, 5,  19, 6,  9,  8,  16, 17, 18,
                                     20, 12, 13, 14, 15, 27, 37, 36, 35, 29, 33, 25, 24, 31, 4,  2,  11, 21, 22};

static const int filterK0[5] = {0, 60, 115, 98, 122}, filterK1[5] = {0, 0, -52, -55, -60};

typedef struct Out {
    uint8_t *data;
    size_t size, capacity;
} Out;

static void Add(Out *o, const void *data, size_t size)
{
    if (o->size + size > o->capacity) {
        o->capacity = (o->size + size) * 2 + 4096;
        o->data = SDL_realloc(o->data, o->capacity);
    }
    memcpy(o->data + o->size, data, size);
    o->size += size;
}

static int Clamp16(int v)
{
    return v < -32768 ? -32768 : v > 32767 ? 32767 : v;
}

/* One block of 28 samples: of every filter (only filter 0 when `plain`) and shift, the one that decodes nearest (as
   the SPU decodes: spu.c), the history carried on as decoded. */
static void EncodeBlock(const int16_t *x, int flags, int plain, int history[2], uint8_t block[16])
{
    long long best = -1;
    int bestFilter = 0, bestShift = 0, bestHistory[2] = {0, 0}, f, shift, i;
    uint8_t bestNibbles[28] = {0};

    for (f = 0; f < (plain ? 1 : 5); f++) {
        for (shift = 0; shift <= 12; shift++) {
            int h1 = history[0], h2 = history[1], e = 12 - shift, half = (1 << e) >> 1;
            long long error = 0;
            uint8_t nibbles[28];

            for (i = 0; i < 28; i++) {
                int prediction = ((h1 * filterK0[f]) >> 6) + ((h2 * filterK1[f]) >> 6);
                int q = (x[i] - prediction + half) >> e, decoded;

                q = q < -8 ? -8 : q > 7 ? 7 : q;
                nibbles[i] = (uint8_t)(q & 15);
                decoded = Clamp16(((int16_t)(q << 12) >> shift) + prediction);
                error += (long long)(decoded - x[i]) * (decoded - x[i]);
                h2 = h1;
                h1 = decoded;
            }
            if (best < 0 || error < best) {
                best = error;
                bestFilter = f;
                bestShift = shift;
                bestHistory[0] = h1;
                bestHistory[1] = h2;
                memcpy(bestNibbles, nibbles, sizeof(nibbles));
            }
        }
    }
    block[0] = (uint8_t)(bestFilter << 4 | bestShift);
    block[1] = (uint8_t)flags;
    for (i = 0; i < 14; i++) {
        block[2 + i] = (uint8_t)(bestNibbles[i * 2] | bestNibbles[i * 2 + 1] << 4);
    }
    history[0] = bestHistory[0];
    history[1] = bestHistory[1];
}

static void PutBe32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24), p[1] = (uint8_t)(v >> 16), p[2] = (uint8_t)(v >> 8), p[3] = (uint8_t)v;
}

/* A VAG of n samples (s8 << 8) at `rate`, looping or not. */
static void MakeVag(Out *vag, const int8_t *pcm, int n, int loops, int rate, const char *name)
{
    uint8_t header[48] = {0}, block[16];
    int16_t *x;
    int count, i, history[2] = {0, 0};
    Out data = {0};

    if (loops) {
        /* resampled to whole blocks (cyclic linear interpolation) */
        count = SDL_max(28, (n + 14) / 28 * 28);
        x = SDL_calloc((size_t)count, sizeof(int16_t));
        for (i = 0; i < count; i++) {
            long long position = (long long)i * n * 65536 / count;
            int j = (int)(position >> 16), frac = (int)(position & 0xFFFF);
            int a = pcm[j % n] * 256, b = pcm[(j + 1) % n] * 256;

            x[i] = (int16_t)((a * (65536LL - frac) + (long long)b * frac) >> 16);
        }
    } else {
        count = (n + 27) / 28 * 28;
        x = SDL_calloc((size_t)count, sizeof(int16_t));
        for (i = 0; i < n; i++) {
            x[i] = (int16_t)(pcm[i] * 256);
        }
    }
    memset(block, 0, 16);
    Add(&data, block, 16);
    if (!loops) {
        block[0] = 0x0c; /* (a silent block, as the PlayStation's start) */
        Add(&data, block, 16);
    }
    for (i = 0; i < count; i += 28) {
        int first = i == 0, last = i + 28 >= count;
        int flags = loops ? (first ? 6 : last ? 3 : 2) : (first ? 4 : 0);

        EncodeBlock(x + i, flags, loops && first, history, block);
        Add(&data, block, 16);
    }
    if (!loops) {
        memset(block, 0, 16);
        block[0] = 0x0c;
        block[1] = 1; /* the end: the voice stops */
        Add(&data, block, 16);
    }
    memcpy(header, "VAGp", 4);
    PutBe32(header + 4, 2);
    PutBe32(header + 12, (uint32_t)data.size);
    PutBe32(header + 16, (uint32_t)rate);
    SDL_strlcpy((char *)header + 32, name, 16);
    Add(vag, header, 48);
    Add(vag, data.data, data.size);
    SDL_free(data.data);
    SDL_free(x);
}

uint8_t *SaturnMakeSamples(const uint8_t *const *blocks, const uint32_t *sizes, int blockCount,
                           const char *const *names, size_t *size)
{
    Out wad = {0}, files = {0};
    uint8_t head[2];
    int i;

    if (blockCount < 42 || sizes[0] < 41 * 20) {
        return NULL;
    }
    head[0] = 39, head[1] = 0;
    Add(&wad, head, 2);
    for (i = 0; i < 39; i++) {
        const uint8_t *record = blocks[0] + saturnSample[i] * 20;
        int length = record[4] << 8 | record[5], loops = record[11] != 0, rate = record[16] << 8 | record[17];
        const uint8_t *pcm = blocks[1 + saturnSample[i]];
        uint8_t entry[25] = {0};
        size_t before = files.size, vagSize;

        length = SDL_min(length, (int)sizes[1 + saturnSample[i]]);
        MakeVag(&files, (const int8_t *)pcm, SDL_max(length, 1), loops, rate ? rate : 11025, names[i]);
        vagSize = files.size - before;
        SDL_strlcpy((char *)entry, names[i], 16);
        entry[16] = (uint8_t)vagSize, entry[17] = (uint8_t)(vagSize >> 8), entry[18] = (uint8_t)(vagSize >> 16);
        entry[20] = (uint8_t)vagSize, entry[21] = (uint8_t)(vagSize >> 8), entry[22] = (uint8_t)(vagSize >> 16);
        Add(&wad, entry, 25);
    }
    Add(&wad, files.data, files.size);
    SDL_free(files.data);
    *size = wad.size;
    return wad.data;
}
