/* Monkey's Audio (.ape) decoding, for files of version 3.99 and later with 16-bit samples in one or two channels:
   the part of Monkey's Audio's decoder (MACLib: APEDecompressCore, UnBitArray, NewPredictor, NNFilter, Prepare)
   that such files need, in C.

   Adapted from the Monkey's Audio SDK, under its license:

   Copyright 2000-2026 Matthew T. Ashland. All rights reserved.

   Redistribution and use in source and binary forms, with or without modification, are permitted provided that the
   following conditions are met:
   1) Redistributions of source code must retain the above copyright notice, this list of conditions and the
      following disclaimer.
   2) Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the
      following disclaimer in the documentation and/or other materials provided with the distribution.
   3) Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote
      products derived from this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
   INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
   DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
   LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
   CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   The file: "MAC ", the version (3990 and on), a descriptor (the sizes of what follows), a header (the compression
   level, the blocks per frame, the frames, the format), a table of where each frame starts, the source file's own
   header (a WAV's), then the frames. A frame is read as 32-bit little-endian words, the highest bit of each first:
   its CRC (and, with the top bit set, special codes: silence, a mono picture of stereo), then the residuals of both
   channels, range coded with an adaptive Rice-like parameter; each channel's residual goes through up to three
   adaptive FIR filters (the "NN filters", by compression level) and a predictor of order 4 and 5 (the second
   channel's from the first), then the two channels (X, Y) are made left and right. */
#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

#include "ape.h"

/* ---- the bit stream and the range decoder ---- */

#define CODE_BITS 32
#define TOP_VALUE (1u << (CODE_BITS - 1))
#define EXTRA_BITS ((CODE_BITS - 2) % 8 + 1)
#define BOTTOM_VALUE (TOP_VALUE >> 8)
#define MODEL_ELEMENTS 64
#define OVERFLOW_SIGNAL 1
#define OVERFLOW_PIVOT 32768

static const uint32_t rangeTotal[65] = {
    0,     19578, 36160, 48417, 56323, 60899, 63265, 64435, 64971, 65232, 65351, 65416, 65447, 65466, 65476, 65482,
    65485, 65488, 65490, 65491, 65492, 65493, 65494, 65495, 65496, 65497, 65498, 65499, 65500, 65501, 65502, 65503,
    65504, 65505, 65506, 65507, 65508, 65509, 65510, 65511, 65512, 65513, 65514, 65515, 65516, 65517, 65518, 65519,
    65520, 65521, 65522, 65523, 65524, 65525, 65526, 65527, 65528, 65529, 65530, 65531, 65532, 65533, 65534, 65535,
    65536};
static const uint32_t rangeWidth[64] = {19578, 16582, 12257, 7906, 4576, 2366, 1170, 536, 261, 119, 65, 31, 19,
                                        10,    6,     3,     3,    2,    1,    1,    1,   1,   1,   1,   1,  1,
                                        1,     1,     1,     1,    1,    1,    1,    1,   1,   1,   1,   1,  1,
                                        1,     1,     1,     1,    1,    1,    1,    1,   1,   1,   1,   1,  1,
                                        1,     1,     1,     1,    1,    1,    1,    1,   1,   1,   1,   1};
static const uint32_t kSumMinBoundary[32] = {
    0,       32,       64,       128,      256,       512,       1024,       2048,       4096, 8192, 16384,
    32768,   65536,    131072,   262144,   524288,    1048576,   2097152,    4194304,    8388608,
    16777216, 33554432, 67108864, 134217728, 268435456, 536870912, 1073741824, 2147483648u, 0, 0, 0, 0};

typedef struct {
    const uint8_t *data; /* from a 4-byte boundary of the frame data on */
    size_t size;
    uint32_t bit;        /* the next bit */
    uint32_t low, range, buffer;
    int bad;             /* read past the end, or a value the coder cannot have made */
} Bits;

typedef struct {
    uint32_t k, kSum;
} RiceState;

static uint32_t Word(const Bits *b, uint32_t index)
{
    size_t at = (size_t)index * 4;
    const uint8_t *p = b->data + at;

    if (at + 4 > b->size) {
        return 0;
    }
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static uint32_t Byte(Bits *b)
{
    uint32_t value = (Word(b, b->bit >> 5) >> (24 - (b->bit & 31))) & 0xFF;

    if (((size_t)b->bit >> 3) >= b->size) {
        b->bad = 1;
    }
    b->bit += 8;
    return value;
}

static uint32_t BitsOf(Bits *b, uint32_t count)
{
    uint32_t left = 32 - (b->bit & 31), index = b->bit >> 5, value;

    b->bit += count;
    if (left >= count) {
        value = (Word(b, index) & (left == 32 ? 0xFFFFFFFFu : (1u << left) - 1)) >> (left - count);
    } else {
        uint32_t right = count - left;

        value = ((Word(b, index) & ((1u << left) - 1)) << right) | (Word(b, index + 1) >> (32 - right));
    }
    return value;
}

static void Normalise(Bits *b)
{
    while (b->range <= BOTTOM_VALUE && !b->bad) {
        b->buffer = (b->buffer << 8) | Byte(b);
        b->low = (b->low << 8) | ((b->buffer >> 1) & 0xFF);
        b->range <<= 8;
        if (!b->range) {
            b->bad = 1;
        }
    }
}

static uint32_t DecodeFast(Bits *b, int shift)
{
    Normalise(b);
    b->range >>= shift;
    return b->range ? b->low / b->range : (b->bad = 1, 0);
}

static uint32_t DecodeFastWithUpdate(Bits *b, int shift)
{
    uint32_t result;

    Normalise(b);
    b->range >>= shift;
    if (!b->range) {
        b->bad = 1;
        return 0;
    }
    result = b->low / b->range;
    b->low %= b->range;
    return result;
}

static uint32_t DecodeOverflow(Bits *b, uint32_t *pivot)
{
    uint32_t total, overflow;

    for (;;) {
        total = DecodeFast(b, 16);
        if (total >= 65536 || b->bad) {
            b->bad = 1;
            return 0;
        }
        for (overflow = 0; rangeTotal[overflow + 1] <= total; overflow++) {
        }
        b->low -= b->range * rangeTotal[overflow];
        b->range *= rangeWidth[overflow];
        if (overflow != MODEL_ELEMENTS - 1) {
            return overflow;
        }
        overflow = DecodeFastWithUpdate(b, 16) << 16;
        overflow |= DecodeFastWithUpdate(b, 16);
        if (overflow != OVERFLOW_SIGNAL) {
            return overflow;
        }
        *pivot = OVERFLOW_PIVOT; /* (signalled: a larger pivot, and the overflow again) */
    }
}

static uint32_t DivideOut(Bits *b, uint32_t by)
{
    uint32_t value;

    Normalise(b);
    b->range /= by;
    if (!b->range) {
        b->bad = 1;
        return 0;
    }
    value = b->low / b->range;
    b->low %= b->range;
    return value;
}

static int64_t DecodeValue(Bits *b, RiceState *state)
{
    uint32_t pivot = state->kSum / 32 > 1 ? state->kSum / 32 : 1, overflow, base;
    int64_t value;

    overflow = DecodeOverflow(b, &pivot);
    if (pivot >= (1u << 16)) {
        uint32_t pivotBits = 0, shift, split;

        while ((pivot >> pivotBits) > 0) {
            pivotBits++;
        }
        shift = pivotBits >= 16 ? pivotBits - 16 : 0;
        split = 1u << shift;
        base = DivideOut(b, pivot / split + 1) * split;
        base += DivideOut(b, split);
    } else {
        base = DivideOut(b, pivot);
    }
    value = (int64_t)base + (int64_t)overflow * pivot;
    state->kSum += (uint32_t)((value + 1) / 2) - ((state->kSum + 16) >> 5);
    if (state->kSum < kSumMinBoundary[state->k]) {
        state->k--;
    } else if (kSumMinBoundary[state->k + 1] && state->kSum >= kSumMinBoundary[state->k + 1]) {
        state->k++;
    }
    return (value & 1) ? (value >> 1) + 1 : -(value >> 1);
}

/* ---- the filters ---- */

#define WINDOW 512         /* the NN filters' rolling buffers (their history is the filter's order) */
#define PREDICTOR_WINDOW 256
#define PREDICTOR_HISTORY 8

typedef struct {
    int order, shift;
    int32_t runningAverage;
    int16_t *m;                       /* the coefficients */
    int16_t *input, *delta;           /* rolling buffers of WINDOW + order */
    int at;                           /* the current element of both */
} NNFilter;

static int NNCreate(NNFilter *f, int order, int shift)
{
    memset(f, 0, sizeof(*f));
    f->order = order;
    f->shift = shift;
    f->m = (int16_t *)SDL_calloc((size_t)order, sizeof(int16_t));
    f->input = (int16_t *)SDL_calloc((size_t)(WINDOW + order), sizeof(int16_t));
    f->delta = (int16_t *)SDL_calloc((size_t)(WINDOW + order), sizeof(int16_t));
    return f->m && f->input && f->delta;
}

static void NNFree(NNFilter *f)
{
    SDL_free(f->m);
    SDL_free(f->input);
    SDL_free(f->delta);
}

static void NNFlush(NNFilter *f)
{
    memset(f->m, 0, (size_t)f->order * sizeof(int16_t));
    memset(f->input, 0, (size_t)(f->order + 1) * sizeof(int16_t));
    memset(f->delta, 0, (size_t)(f->order + 1) * sizeof(int16_t));
    f->at = f->order;
    f->runningAverage = 0;
}

static int16_t Saturate(int32_t value)
{
    int16_t s = (int16_t)value;

    return s != value ? (int16_t)((value >> 31) ^ 0x7FFF) : s;
}

static int32_t NNDecompress(NNFilter *f, int32_t in)
{
    const int16_t *input = f->input + f->at - f->order;
    int16_t *delta = f->delta + f->at;
    uint32_t dot = 0; /* (sums as the original's 32-bit int does: wrapping) */
    int32_t out, magnitude;
    int i;

    for (i = 0; i < f->order; i++) {
        dot += (uint32_t)((int32_t)input[i] * f->m[i]);
    }
    out = (int32_t)((uint32_t)in + (uint32_t)(((int32_t)dot + (1 << (f->shift - 1))) >> f->shift));
    if (in < 0) {
        for (i = 0; i < f->order; i++) {
            f->m[i] = (int16_t)(f->m[i] + delta[i - f->order]);
        }
    } else if (in > 0) {
        for (i = 0; i < f->order; i++) {
            f->m[i] = (int16_t)(f->m[i] - delta[i - f->order]);
        }
    }
    /* the adaptation's next step (versions 3.98 and on) */
    magnitude = out < 0 ? -out : out;
    if (magnitude > f->runningAverage * 3) {
        delta[0] = (int16_t)(((out >> 25) & 64) - 32);
    } else if (magnitude > (f->runningAverage * 4) / 3) {
        delta[0] = (int16_t)(((out >> 26) & 32) - 16);
    } else if (magnitude > 0) {
        delta[0] = (int16_t)(((out >> 27) & 16) - 8);
    } else {
        delta[0] = 0;
    }
    f->runningAverage += (magnitude - f->runningAverage) / 16;
    delta[-1] >>= 1;
    delta[-2] >>= 1;
    delta[-8] >>= 1;
    f->input[f->at] = Saturate(out);
    if (++f->at == WINDOW + f->order) {
        memmove(f->input, f->input + WINDOW, (size_t)f->order * sizeof(int16_t));
        memmove(f->delta, f->delta + WINDOW, (size_t)f->order * sizeof(int16_t));
        f->at = f->order;
    }
    return out;
}

typedef struct {
    NNFilter nn[3]; /* applied first to last */
    int nnCount;
    int32_t predictionA[PREDICTOR_WINDOW + PREDICTOR_HISTORY], predictionB[PREDICTOR_WINDOW + PREDICTOR_HISTORY];
    int32_t adaptA[PREDICTOR_WINDOW + PREDICTOR_HISTORY], adaptB[PREDICTOR_WINDOW + PREDICTOR_HISTORY];
    int at, index;
    int32_t ma[8], mb[8];
    int32_t lastA;             /* the stage 1 input of A last time */
    int32_t stage1A, stage1B;  /* the scaled first-order filters' last values */
} Predictor;

/* The NN filters of a compression level: order and shift, applied in this order. */
static int PredictorCreate(Predictor *p, int level)
{
    static const int filters[5][3][2] = {{{0, 0}},
                                         {{16, 11}},
                                         {{64, 11}},
                                         {{32, 10}, {256, 13}},
                                         {{16, 11}, {256, 13}, {1024 + 256, 15}}};
    int i, row = level / 1000 - 1;

    memset(p, 0, sizeof(*p));
    if (level % 1000 || row < 0 || row > 4) {
        return 0;
    }
    for (i = 0; i < 3 && filters[row][i][0]; i++) {
        if (!NNCreate(&p->nn[p->nnCount++], filters[row][i][0], filters[row][i][1])) {
            return 0;
        }
    }
    return 1;
}

static void PredictorFree(Predictor *p)
{
    int i;

    for (i = 0; i < p->nnCount; i++) {
        NNFree(&p->nn[i]);
    }
}

static void PredictorFlush(Predictor *p)
{
    int i;

    for (i = 0; i < p->nnCount; i++) {
        NNFlush(&p->nn[i]);
    }
    memset(p->ma, 0, sizeof(p->ma));
    memset(p->mb, 0, sizeof(p->mb));
    memset(p->predictionA, 0, (PREDICTOR_HISTORY + 1) * sizeof(int32_t));
    memset(p->predictionB, 0, (PREDICTOR_HISTORY + 1) * sizeof(int32_t));
    memset(p->adaptA, 0, (PREDICTOR_HISTORY + 1) * sizeof(int32_t));
    memset(p->adaptB, 0, (PREDICTOR_HISTORY + 1) * sizeof(int32_t));
    p->at = PREDICTOR_HISTORY;
    p->ma[0] = 360;
    p->ma[1] = 317;
    p->ma[2] = -109;
    p->ma[3] = 98;
    p->stage1A = p->stage1B = 0;
    p->lastA = 0;
    p->index = 0;
}

static int32_t Sign(int32_t value)
{
    return value ? ((value >> 30) & 2) - 1 : 0;
}

static int32_t PredictorDecompress(Predictor *p, int32_t a, int32_t b)
{
    int32_t *pa, *pb, *aa, *ab, current, direction, predictionA, predictionB, filteredB;
    int i;

    if (p->index == PREDICTOR_WINDOW) {
        memmove(p->predictionA, p->predictionA + p->at - PREDICTOR_HISTORY, PREDICTOR_HISTORY * sizeof(int32_t));
        memmove(p->predictionB, p->predictionB + p->at - PREDICTOR_HISTORY, PREDICTOR_HISTORY * sizeof(int32_t));
        memmove(p->adaptA, p->adaptA + p->at - PREDICTOR_HISTORY, PREDICTOR_HISTORY * sizeof(int32_t));
        memmove(p->adaptB, p->adaptB + p->at - PREDICTOR_HISTORY, PREDICTOR_HISTORY * sizeof(int32_t));
        p->at = PREDICTOR_HISTORY;
        p->index = 0;
    }
    for (i = 0; i < p->nnCount; i++) {
        a = NNDecompress(&p->nn[i], a); /* stage 2: the largest filter last */
    }
    pa = p->predictionA + p->at;
    pb = p->predictionB + p->at;
    aa = p->adaptA + p->at;
    ab = p->adaptB + p->at;
    /* stage 1: order 4 on A, order 5 on B (the other channel, through a first-order filter) */
    pa[0] = p->lastA;
    pa[-1] = pa[0] - pa[-1];
    filteredB = b - (int32_t)(((int64_t)p->stage1B * 31) >> 5);
    p->stage1B = b;
    pb[0] = filteredB;
    pb[-1] = pb[0] - pb[-1];
    predictionA = (int32_t)((uint32_t)pa[0] * (uint32_t)p->ma[0] + (uint32_t)pa[-1] * (uint32_t)p->ma[1] +
                            (uint32_t)pa[-2] * (uint32_t)p->ma[2] + (uint32_t)pa[-3] * (uint32_t)p->ma[3]);
    predictionB = (int32_t)((uint32_t)pb[0] * (uint32_t)p->mb[0] + (uint32_t)pb[-1] * (uint32_t)p->mb[1] +
                            (uint32_t)pb[-2] * (uint32_t)p->mb[2] + (uint32_t)pb[-3] * (uint32_t)p->mb[3] +
                            (uint32_t)pb[-4] * (uint32_t)p->mb[4]);
    current = (int32_t)((uint32_t)a + (uint32_t)((int32_t)((uint32_t)predictionA + (uint32_t)(predictionB >> 1)) >> 10));
    aa[0] = Sign(pa[0]);
    aa[-1] = Sign(pa[-1]);
    ab[0] = Sign(pb[0]);
    ab[-1] = Sign(pb[-1]);
    direction = (a < 0) - (a > 0);
    for (i = 0; i < 4; i++) {
        p->ma[i] += aa[-i] * direction;
    }
    for (i = 0; i < 5; i++) {
        p->mb[i] += ab[-i] * direction;
    }
    p->stage1A = (int32_t)((uint32_t)current + (uint32_t)(int32_t)(((int64_t)p->stage1A * 31) >> 5));
    p->lastA = current;
    p->at++;
    p->index++;
    return p->stage1A;
}

/* ---- the file ---- */

static uint32_t Little32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static uint16_t Little16(const uint8_t *p)
{
    return (uint16_t)(p[0] | p[1] << 8);
}

static uint32_t crcTable[256];

static void CrcTable(void)
{
    uint32_t i, k, c;

    for (i = 0; i < 256; i++) {
        for (c = i, k = 0; k < 8; k++) {
            c = (c >> 1) ^ (c & 1 ? 0xEDB88320u : 0);
        }
        crcTable[i] = c;
    }
}

int PortApeDecode(const char *in, const char *out, char *message, size_t messageSize)
{
    size_t size = 0, at = 0;
    uint8_t *file = (uint8_t *)SDL_LoadFile(in, &size);
    const uint8_t *header;
    uint32_t descriptorBytes, headerBytes, seekBytes, blocksPerFrame, finalBlocks, frames, frame, seekCount;
    int version, level, bitsPerSample, channels, ok = 0;
    Predictor predictor[2];
    int16_t *samples = NULL;
    FILE *dst = NULL;

    memset(predictor, 0, sizeof(predictor));
    if (!file) {
        SDL_snprintf(message, messageSize, "%s could not be read.", in);
        return 0;
    }
    if (size > 10 && !memcmp(file, "ID3", 3)) {
        /* an ID3v2 tag before it */
        at = 10 + ((size_t)(file[6] & 0x7F) << 21 | (size_t)(file[7] & 0x7F) << 14 | (size_t)(file[8] & 0x7F) << 7 |
                   (size_t)(file[9] & 0x7F));
    }
    if (at + 52 > size || (memcmp(file + at, "MAC ", 4) && memcmp(file + at, "MACF", 4))) {
        SDL_snprintf(message, messageSize, "%s is not a Monkey's Audio file.", in);
        goto done;
    }
    version = Little16(file + at + 4);
    if (version < 3990) {
        SDL_snprintf(message, messageSize,
                     "%s was made by an old Monkey's Audio (version %d.%02d); only 3.99 and later can be played. "
                     "Monkey's Audio can convert it.",
                     in, version / 1000, version / 10 % 100);
        goto done;
    }
    descriptorBytes = Little32(file + at + 8);
    headerBytes = Little32(file + at + 12);
    seekBytes = Little32(file + at + 16);
    if (at + descriptorBytes + headerBytes + seekBytes > size || headerBytes < 24) {
        goto damaged;
    }
    header = file + at + descriptorBytes;
    level = Little16(header);
    blocksPerFrame = Little32(header + 4);
    finalBlocks = Little32(header + 8);
    frames = Little32(header + 12);
    bitsPerSample = Little16(header + 16);
    channels = Little16(header + 18);
    seekCount = seekBytes / 4;
    if (bitsPerSample != 16 || channels < 1 || channels > 2) {
        SDL_snprintf(message, messageSize, "%s is not 16-bit mono or stereo audio, as a disc's track is.", in);
        goto done;
    }
    if (!frames || seekCount < frames || !blocksPerFrame || finalBlocks > blocksPerFrame ||
        !PredictorCreate(&predictor[0], level) || !PredictorCreate(&predictor[1], level)) {
        goto damaged;
    }
    samples = (int16_t *)SDL_malloc((size_t)blocksPerFrame * (size_t)channels * sizeof(int16_t));
    dst = fopen(out, "wb");
    if (!samples || !dst) {
        SDL_snprintf(message, messageSize, "%s could not be written.", out);
        goto done;
    }
    CrcTable();
    for (frame = 0; frame < frames; frame++) {
        const uint8_t *seek = file + at + descriptorBytes + headerBytes;
        uint32_t start = Little32(seek + frame * 4), first = Little32(seek), skip, blocks, crc = 0xFFFFFFFFu, stored,
                 special = 0, block, i;
        RiceState state[2];
        Bits bits;
        int32_t lastX = 0;

        blocks = frame + 1 == frames ? finalBlocks : blocksPerFrame;
        if (start < first || at + start >= size) {
            goto damaged;
        }
        /* the frame's bits from the 4-byte word of the frame data it starts in */
        skip = (start - first) % 4;
        memset(&bits, 0, sizeof(bits));
        bits.data = file + at + start - skip;
        bits.size = size - (at + start - skip);
        bits.bit = skip * 8;
        stored = BitsOf(&bits, 32);
        if (stored & 0x80000000u) {
            special = BitsOf(&bits, 32);
        }
        stored &= 0x7FFFFFFFu;
        for (i = 0; i < 2; i++) {
            PredictorFlush(&predictor[i]);
            state[i].k = 10;
            state[i].kSum = (1u << state[i].k) * 16;
        }
        bits.bit = (bits.bit + 7) & ~7u;
        (void)BitsOf(&bits, 8); /* (a byte the encoder leaves) */
        bits.buffer = BitsOf(&bits, 8);
        bits.low = bits.buffer >> (8 - EXTRA_BITS);
        bits.range = 1u << EXTRA_BITS;
        for (block = 0; block < blocks && !bits.bad; block++) {
            int32_t x = 0, y = 0;

            if (channels == 2) {
                if ((special & 1) && (special & 2)) {
                    /* silence */
                } else if (special & 4) {
                    x = PredictorDecompress(&predictor[0], (int32_t)DecodeValue(&bits, &state[0]), 0);
                } else {
                    int64_t valueY = DecodeValue(&bits, &state[1]), valueX = DecodeValue(&bits, &state[0]);

                    y = PredictorDecompress(&predictor[1], (int32_t)valueY, lastX);
                    x = PredictorDecompress(&predictor[0], (int32_t)valueX, y);
                    lastX = x;
                }
                {
                    /* (x, y) as left and right (the original's names for them the other way round) */
                    int32_t first = x - y / 2, second = first + y;

                    if (first < -32768 || first > 32767 || second < -32768 || second > 32767) {
                        bits.bad = 1;
                    }
                    samples[block * 2] = (int16_t)first;
                    samples[block * 2 + 1] = (int16_t)second;
                }
            } else {
                if (!(special & 1)) {
                    x = PredictorDecompress(&predictor[0], (int32_t)DecodeValue(&bits, &state[0]), 0);
                }
                if (x < -32768 || x > 32767) {
                    bits.bad = 1;
                }
                samples[block] = (int16_t)x;
            }
        }
        if (bits.bad) {
            goto damaged;
        }
        for (i = 0; i < blocks * (uint32_t)channels; i++) {
            uint8_t sample[2] = {(uint8_t)samples[i], (uint8_t)((uint16_t)samples[i] >> 8)};

            crc = crcTable[(crc ^ sample[0]) & 0xFF] ^ (crc >> 8);
            crc = crcTable[(crc ^ sample[1]) & 0xFF] ^ (crc >> 8);
            if (fwrite(sample, 1, 2, dst) != 2) {
                SDL_snprintf(message, messageSize, "%s could not be written (is the disk full?).", out);
                goto done;
            }
        }
        if (((crc ^ 0xFFFFFFFFu) >> 1) != stored) {
            goto damaged;
        }
    }
    ok = 1;
    goto done;
damaged:
    SDL_snprintf(message, messageSize, "%s is damaged, or of a kind of Monkey's Audio this cannot play.", in);
done:
    if (dst && fclose(dst) != 0 && ok) {
        SDL_snprintf(message, messageSize, "%s could not be written (is the disk full?).", out);
        ok = 0;
    }
    if (dst && !ok) {
        remove(out);
    }
    PredictorFree(&predictor[0]);
    PredictorFree(&predictor[1]);
    SDL_free(samples);
    SDL_free(file);
    return ok;
}
