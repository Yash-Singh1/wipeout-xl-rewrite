/* CHD version 5 CDs: their hunk map is Huffman coded; their sectors are 2352 bytes followed by 96 bytes of
   subchannel data. The latter is decoded and checked but not kept in the .bin. Each track is padded to four
   sectors in the CHD; that padding is left out, and audio's big-endian samples become the .bin's little-endian.
   CHTR and CHT2 metadata supply the tracks and their pregaps. A stored pregap (PGTYPE starts with V) becomes
   INDEX 00; a gap not stored becomes PREGAP. Postgaps are written as silence, since our cue reader has no POSTGAP.

   The CD codecs cdzl and cdlz split the sectors from their subchannels, compress each separately, and leave
   out sync and ECC where they can be made again. cdlz uses LZMA, not Huffman. cdfl holds headerless FLAC frames
   for the sectors, then deflated subchannels. The general zlib, lzma and huff codecs work too. A track of type MODE1
   (2048-byte sectors, as made from an .iso) is written as raw sectors with a made-up sync and header
   (DiscMakeRawSector).
   No parent image, older CHD, other cooked sector, GD-ROM, DVD or hard disk image is accepted. */
#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

#include "chd.h"
#include "psx/disc.h"
#include "third_party/lzma/LzmaDec.h"
#include "third_party/lzma/7zAlloc.h"
#ifdef PORT_HAVE_ZLIB
#include <zlib.h>
#endif

#define CD_FRAME 2448
#define CD_SECTOR 2352
#define TAG(a, b, c, d) ((uint32_t)(a) << 24 | (uint32_t)(b) << 16 | (uint32_t)(c) << 8 | (d))

typedef struct {
    const uint8_t *data;
    size_t size, bit;
    int bad, little;
} Bits;

static uint32_t ReadBits(Bits *b, unsigned count)
{
    uint32_t value = 0;
    unsigned i;

    if (count > 32 || b->bit > b->size * 8 || count > b->size * 8 - b->bit) {
        b->bad = 1;
        return 0;
    }
    for (i = 0; i < count; i++, b->bit++) {
        unsigned bit = (b->data[b->bit / 8] >> (b->little ? b->bit % 8 : 7 - b->bit % 8)) & 1;

        if (b->little) {
            value |= bit << i;
        } else {
            value = value * 2 + bit;
        }
    }
    return value;
}

static int64_t SignedBits(Bits *b, unsigned count)
{
    uint32_t value = ReadBits(b, count);

    return count && (value & (1u << (count - 1))) ? (int64_t)value - ((int64_t)1 << count) : value;
}

/* Canonical codes, in symbol order within each length. CHD numbers the longest codes first; DEFLATE numbers
   the shortest first. The bit reader's byte order is separate from the order of bits in a Huffman code. */
typedef struct {
    uint8_t length[288];
    uint16_t symbols[288], count[17], offset[17];
    uint32_t first[17];
    unsigned max;
} Huffman;

static int MakeTree(Huffman *h, unsigned symbols, unsigned max, int chd)
{
    uint32_t code = 0;
    unsigned i, length, total = 0, next[17];

    h->max = max;
    memset(h->count, 0, sizeof(h->count));
    for (i = 0; i < symbols; i++) {
        if (h->length[i] > max) {
            return 0;
        }
        if (h->length[i]) {
            h->count[h->length[i]]++;
        }
    }
    if (chd) {
        for (length = max; length; length--) {
            h->first[length] = code;
            code += h->count[length];
            if (length > 1 && (code & 1)) {
                return 0;
            }
            code >>= 1;
        }
    } else {
        h->count[0] = 0;
        for (length = 1; length <= max; length++) {
            code = (code + h->count[length - 1]) * 2;
            h->first[length] = code;
        }
    }
    for (length = 1; length <= max; length++) {
        if (h->first[length] + h->count[length] > (1u << length)) {
            return 0;
        }
        next[length] = h->offset[length] = (uint16_t)total;
        total += h->count[length];
    }
    for (i = 0; i < symbols; i++) {
        if (h->length[i]) {
            h->symbols[next[h->length[i]]++] = (uint16_t)i;
        }
    }
    return total != 0;
}

static unsigned Symbol(Bits *b, const Huffman *h)
{
    uint32_t code = 0;
    unsigned length;

    for (length = 1; length <= h->max && !b->bad; length++) {
        code = code * 2 + ReadBits(b, 1);
        if (code >= h->first[length] && code - h->first[length] < h->count[length]) {
            return h->symbols[h->offset[length] + code - h->first[length]];
        }
    }
    b->bad = 1;
    return 0;
}

static int MapTree(Bits *b, Huffman *h)
{
    unsigned at = 0;

    while (at < 16 && !b->bad) {
        unsigned length = ReadBits(b, 4), count = 1;

        if (length == 1) {
            length = ReadBits(b, 4);
            if (length != 1) {
                count = ReadBits(b, 4) + 3;
            }
        }
        if (count > 16 - at) {
            return 0;
        }
        while (count--) {
            h->length[at++] = (uint8_t)length;
        }
    }
    return !b->bad && MakeTree(h, 16, 8, 1);
}

static int HuffmanData(const uint8_t *src, size_t size, uint8_t *out, size_t bytes)
{
    Bits b = {src, size, 0, 0, 0};
    Huffman small = {0}, h = {0};
    unsigned start, count = 0, last = 0, at = 0, i;

    small.length[0] = (uint8_t)ReadBits(&b, 3);
    start = ReadBits(&b, 3) + 1;
    for (i = start; i < 24 && count != 7; i++) {
        count = ReadBits(&b, 3);
        small.length[i] = (uint8_t)(count == 7 ? 0 : count);
    }
    if (!MakeTree(&small, 24, 6, 1)) {
        return 0;
    }
    while (at < 256 && !b.bad) {
        unsigned value = Symbol(&b, &small);

        count = 1;
        if (value) {
            last = value - 1;
        } else {
            count = ReadBits(&b, 3) + 2;
            if (count == 9) {
                count += ReadBits(&b, 8);
            }
        }
        if (count > 256 - at) {
            return 0;
        }
        while (count--) {
            h.length[at++] = (uint8_t)last;
        }
    }
    if (b.bad || !MakeTree(&h, 256, 16, 1)) {
        return 0;
    }
    for (i = 0; i < bytes && !b.bad; i++) {
        out[i] = (uint8_t)Symbol(&b, &h);
    }
    return !b.bad && (b.bit + 7) / 8 == size;
}

#ifndef PORT_HAVE_ZLIB
/* Raw DEFLATE for builds without zlib (the Windows cross-build, for instance). Only the output hunk is kept;
   distances cannot reach into another hunk. Stored, fixed-code and dynamic-code blocks are all accepted. */
static int Inflate(const uint8_t *src, size_t size, uint8_t *out, size_t bytes)
{
    static const uint16_t lengths[29] = {3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258};
    static const uint8_t lengthBits[29] = {0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0};
    static const uint16_t distances[30] = {1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577};
    static const uint8_t distanceBits[30] = {0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13};
    static const uint8_t order[19] = {16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};
    Bits b = {src, size, 0, 0, 1};
    size_t at = 0;
    unsigned final = 0;

    while (!final && !b.bad) {
        Huffman h = {0}, d = {0};
        unsigned type, i;

        final = ReadBits(&b, 1);
        type = ReadBits(&b, 2);
        if (!type) {
            unsigned length, inverse;

            b.bit = (b.bit + 7) & ~(size_t)7;
            length = ReadBits(&b, 16);
            inverse = ReadBits(&b, 16);
            if ((length ^ inverse) != 65535 || length > bytes - at) {
                return 0;
            }
            while (length--) {
                out[at++] = (uint8_t)ReadBits(&b, 8);
            }
            continue;
        }
        if (type == 1) {
            for (i = 0; i < 288; i++) {
                h.length[i] = (uint8_t)(i < 144 ? 8 : i < 256 ? 9 : i < 280 ? 7 : 8);
            }
            memset(d.length, 5, 32);
        } else if (type == 2) {
            Huffman codes = {0};
            uint8_t all[320];
            unsigned literals = ReadBits(&b, 5) + 257, distance = ReadBits(&b, 5) + 1, n = ReadBits(&b, 4) + 4;

            if (literals > 286) {
                return 0;
            }
            for (i = 0; i < n; i++) {
                codes.length[order[i]] = (uint8_t)ReadBits(&b, 3);
            }
            if (!MakeTree(&codes, 19, 7, 0)) {
                return 0;
            }
            for (i = 0; i < literals + distance && !b.bad;) {
                unsigned value = Symbol(&b, &codes), count = 1;

                if (value == 16) {
                    if (!i) {
                        return 0;
                    }
                    value = all[i - 1];
                    count = ReadBits(&b, 2) + 3;
                } else if (value >= 17) {
                    count = ReadBits(&b, value == 17 ? 3 : 7) + (value == 17 ? 3 : 11);
                    value = 0;
                }
                if (count > literals + distance - i) {
                    return 0;
                }
                while (count--) {
                    all[i++] = (uint8_t)value;
                }
            }
            if (b.bad) {
                return 0;
            }
            memcpy(h.length, all, literals);
            memcpy(d.length, all + literals, distance);
        } else {
            return 0;
        }
        if (!h.length[256] || !MakeTree(&h, 288, 15, 0)) {
            return 0;
        }
        /* A block of literals may have no distance codes. Symbol refuses that tree if a match uses it. */
        if (!MakeTree(&d, 32, 15, 0)) {
            for (i = 0; i < 32; i++) {
                if (d.length[i]) {
                    return 0;
                }
            }
        }
        while (!b.bad) {
            unsigned value = Symbol(&b, &h), length, distance;

            if (value == 256) {
                break;
            }
            if (value < 256) {
                if (at == bytes) {
                    return 0;
                }
                out[at++] = (uint8_t)value;
                continue;
            }
            if (value > 285) {
                return 0;
            }
            value -= 257;
            length = lengths[value] + ReadBits(&b, lengthBits[value]);
            value = Symbol(&b, &d);
            if (value >= 30) {
                return 0;
            }
            distance = distances[value] + ReadBits(&b, distanceBits[value]);
            if (distance > at || length > bytes - at) {
                return 0;
            }
            while (length--) {
                out[at] = out[at - distance];
                at++;
            }
        }
    }
    return !b.bad && at == bytes && (b.bit + 7) / 8 == size;
}
#else
static int Inflate(const uint8_t *src, size_t size, uint8_t *out, size_t bytes)
{
    z_stream z = {0};
    int result, ok;

    z.next_in = (Bytef *)src;
    z.avail_in = (uInt)size;
    z.next_out = out;
    z.avail_out = (uInt)bytes;
    if (inflateInit2(&z, -MAX_WBITS) != Z_OK) {
        return 0;
    }
    result = inflate(&z, Z_FINISH);
    ok = result == Z_STREAM_END && z.total_in == size && z.total_out == bytes;
    inflateEnd(&z);
    return ok;
}
#endif

static int Lzma(const uint8_t *src, size_t size, uint8_t *out, size_t bytes)
{
    /* CHD's fixed properties: lc=3, lp=0, pb=2. The dictionary need only hold this one hunk. */
    uint8_t props[5] = {93, (uint8_t)bytes, (uint8_t)(bytes >> 8), (uint8_t)(bytes >> 16), (uint8_t)(bytes >> 24)};
    SizeT input = size, output = bytes;
    ELzmaStatus status;
    ISzAlloc alloc = {SzAlloc, SzFree};
    int result = LzmaDecode(out, &output, src, &input, props, 5, LZMA_FINISH_END, &status, &alloc);

    return result == SZ_OK && input == size && output == bytes &&
           (status == LZMA_STATUS_FINISHED_WITH_MARK || status == LZMA_STATUS_MAYBE_FINISHED_WITHOUT_MARK);
}

/* ---- the FLAC frames of cdfl: 16-bit stereo at 44100 Hz, including fixed and LPC subframes ---- */

static int FlacResidual(Bits *b, int32_t *samples, unsigned count, unsigned order)
{
    unsigned method = ReadBits(b, 2), partition = ReadBits(b, 4), part, i = order;

    if (method > 1 || (count & ((1u << partition) - 1)) || (count >> partition) < order) {
        return 0;
    }
    for (part = 0; part < (1u << partition) && !b->bad; part++) {
        unsigned rice = ReadBits(b, method ? 5 : 4), end = (part + 1) * (count >> partition);
        unsigned escape = rice == (method ? 31 : 15), raw = escape ? ReadBits(b, 5) : 0;

        for (; i < end && !b->bad; i++) {
            uint64_t value = 0;
            int64_t sample;

            if (escape) {
                sample = SignedBits(b, raw);
            } else {
                while (!ReadBits(b, 1) && !b->bad) {
                    if (++value > (UINT32_MAX >> rice)) {
                        return 0;
                    }
                }
                value = (value << rice) | ReadBits(b, rice);
                sample = value & 1 ? -(int64_t)(value / 2) - 1 : (int64_t)(value / 2);
            }
            if (sample < INT32_MIN || sample > INT32_MAX) {
                return 0;
            }
            samples[i] = (int32_t)sample;
        }
    }
    return !b->bad;
}

static int FlacSubframe(Bits *b, int32_t *samples, unsigned count, unsigned depth)
{
    static const int fixed[5][4] = {{0}, {1}, {2,-1}, {3,-3,1}, {4,-6,4,-1}};
    unsigned type, wasted, order, i, j;
    int shift = 0, coefficients[32];

    if (ReadBits(b, 1)) {
        return 0;
    }
    type = ReadBits(b, 6);
    wasted = ReadBits(b, 1);
    if (wasted) {
        while (!ReadBits(b, 1) && !b->bad && wasted < depth) {
            wasted++;
        }
    }
    if (b->bad || wasted >= depth) {
        return 0;
    }
    depth -= wasted;
    if (type < 2) {
        for (i = 0; i < count; i++) {
            samples[i] = (int32_t)(type == 0 && i ? samples[0] : SignedBits(b, depth));
        }
    } else {
        if ((type < 8 || type > 12) && type < 32) {
            return 0;
        }
        order = type < 32 ? type - 8 : type - 31;
        if (order > count) {
            return 0;
        }
        for (i = 0; i < order; i++) {
            samples[i] = (int32_t)SignedBits(b, depth);
        }
        if (type >= 32) {
            unsigned precision = ReadBits(b, 4) + 1;

            if (precision == 16) {
                return 0;
            }
            shift = (int)SignedBits(b, 5);
            for (i = 0; i < order; i++) {
                coefficients[i] = (int)SignedBits(b, precision);
            }
        } else {
            for (i = 0; i < order; i++) {
                coefficients[i] = fixed[order][i];
            }
        }
        if (!FlacResidual(b, samples, count, order)) {
            return 0;
        }
        for (i = order; i < count; i++) {
            int64_t sum = 0, value;

            for (j = 0; j < order; j++) {
                sum += (int64_t)coefficients[j] * samples[i - j - 1];
            }
            if (shift < 0 && (sum > INT64_MAX / ((int64_t)1 << -shift) ||
                              sum < INT64_MIN / ((int64_t)1 << -shift))) {
                return 0;
            }
            sum = shift < 0 ? sum * ((int64_t)1 << -shift) : sum >> shift;
            if (sum > (int64_t)INT32_MAX - samples[i] || sum < (int64_t)INT32_MIN - samples[i]) {
                return 0;
            }
            value = samples[i] + sum;
            samples[i] = (int32_t)value;
        }
    }
    for (i = 0; i < count; i++) {
        int64_t value = (int64_t)samples[i] * ((int64_t)1 << wasted);

        if (value < INT32_MIN || value > INT32_MAX) {
            return 0;
        }
        samples[i] = (int32_t)value;
    }
    return !b->bad;
}

static size_t Flac(const uint8_t *src, size_t size, uint8_t *out, size_t bytes)
{
    Bits b = {src, size, 0, 0, 0};
    size_t at = 0;
    int32_t *samples = SDL_malloc(2 * 65536 * sizeof(int32_t));

    if (!samples) {
        return 0;
    }
    while (at < bytes && !b.bad) {
        unsigned block, rate, channels, depth, code, i, count;

        if (ReadBits(&b, 14) != 0x3FFE || ReadBits(&b, 1)) {
            goto bad;
        }
        (void)ReadBits(&b, 1); /* fixed or variable block size */
        block = ReadBits(&b, 4);
        rate = ReadBits(&b, 4);
        channels = ReadBits(&b, 4);
        depth = ReadBits(&b, 3);
        if (ReadBits(&b, 1) || (channels != 1 && (channels < 8 || channels > 10)) || (depth != 0 && depth != 4)) {
            goto bad;
        }
        code = ReadBits(&b, 8); /* the UTF-8 frame or sample number */
        if (code >= 128) {
            unsigned mask = 0x80, extra = 0;

            while (code & mask) {
                extra++;
                mask >>= 1;
            }
            if (extra < 2 || extra > 7) {
                goto bad;
            }
            while (--extra) {
                if ((ReadBits(&b, 8) & 0xC0) != 0x80) {
                    goto bad;
                }
            }
        }
        if (!block) {
            goto bad;
        }
        count = block == 1 ? 192 : block <= 5 ? 576u << (block - 2) :
                block == 6 ? ReadBits(&b, 8) + 1 : block == 7 ? ReadBits(&b, 16) + 1 : 256u << (block - 8);
        if (rate >= 12) {
            if (rate == 15) {
                goto bad;
            }
            (void)ReadBits(&b, rate == 12 ? 8 : 16);
        }
        (void)ReadBits(&b, 8); /* header CRC; the CHD hunk's CRC checks the decoded bytes */
        if ((size_t)count * 4 > bytes - at ||
            !FlacSubframe(&b, samples, count, channels == 9 ? 17 : 16) ||
            !FlacSubframe(&b, samples + 65536, count, channels == 8 || channels == 10 ? 17 : 16)) {
            goto bad;
        }
        b.bit = (b.bit + 7) & ~(size_t)7;
        (void)ReadBits(&b, 16); /* frame CRC */
        for (i = 0; i < count; i++) {
            int64_t a = samples[i], c = samples[65536 + i];

            if (channels == 8) {
                c = a - c;
            } else if (channels == 9) {
                a += c;
            } else if (channels == 10) {
                a = a * 2 + (c & 1);
                c = (a - c) >> 1;
                a -= c;
            }
            if (a < -32768 || a > 32767 || c < -32768 || c > 32767) {
                goto bad;
            }
            out[at++] = (uint8_t)((uint16_t)a >> 8);
            out[at++] = (uint8_t)a;
            out[at++] = (uint8_t)((uint16_t)c >> 8);
            out[at++] = (uint8_t)c;
        }
    }
    SDL_free(samples);
    return b.bad ? 0 : b.bit / 8;
bad:
    SDL_free(samples);
    return 0;
}

/* ---- the file and its hunk map ---- */

static uint64_t Big(const uint8_t *p, unsigned bytes)
{
    uint64_t value = 0;

    while (bytes--) {
        value = value * 256 + *p++;
    }
    return value;
}

static void PutBig(uint8_t *p, uint64_t value, unsigned bytes)
{
    while (bytes) {
        p[--bytes] = (uint8_t)value;
        value >>= 8;
    }
}

static uint16_t Crc16(uint16_t crc, const uint8_t *p, size_t bytes)
{
    unsigned bit;

    while (bytes--) {
        crc ^= (uint16_t)*p++ << 8;
        for (bit = 0; bit < 8; bit++) {
            crc = (uint16_t)((crc << 1) ^ (crc & 0x8000 ? 0x1021 : 0));
        }
    }
    return crc;
}

typedef struct {
    SDL_IOStream *file;
    uint64_t size, logical, mapAt, metadata;
    uint32_t codecs[4], hunkBytes, hunks;
    uint8_t *map, *packed, *hunk, *split;
    uint32_t cached;
    int compressed;
} Chd;

static int ReadAt(Chd *c, uint64_t at, void *out, size_t bytes)
{
    return at <= c->size && bytes <= c->size - at && SDL_SeekIO(c->file, (Sint64)at, SDL_IO_SEEK_SET) >= 0 &&
           SDL_ReadIO(c->file, out, bytes) == bytes;
}

static int ReadMap(Chd *c)
{
    uint8_t header[16];
    uint8_t *packed = NULL;
    uint32_t bytes, i, repeat = 0, last = 0, self = 0;
    uint64_t at;
    Bits b;
    Huffman h = {0};
    int ok = 0;

    if (!c->compressed) {
        return ReadAt(c, c->mapAt, c->map, (size_t)c->hunks * 4);
    }
    if (!ReadAt(c, c->mapAt, header, 16)) {
        return 0;
    }
    bytes = (uint32_t)Big(header, 4);
    at = Big(header + 4, 6);
    if (!bytes || bytes > (uint64_t)c->hunks * 8 + 32 || header[12] > 24 || header[13] > 32 || header[14] > 48 || header[15]) {
        return 0;
    }
    packed = SDL_malloc(bytes);
    if (!packed || !ReadAt(c, c->mapAt + 16, packed, bytes)) {
        goto done;
    }
    b = (Bits){packed, bytes, 0, 0, 0};
    if (!MapTree(&b, &h)) {
        goto done;
    }
    for (i = 0; i < c->hunks && !b.bad; i++) {
        unsigned value;

        if (repeat) {
            repeat--;
        } else {
            value = Symbol(&b, &h);
            if (value == 7 || value == 8) {
                repeat = 2 + Symbol(&b, &h);
                if (value == 8) {
                    repeat = 18 + (repeat - 2) * 16 + Symbol(&b, &h);
                }
                if (repeat >= c->hunks - i) {
                    goto done;
                }
            } else {
                last = value;
            }
        }
        c->map[(size_t)i * 12] = (uint8_t)last;
    }
    for (i = 0; i < c->hunks && !b.bad; i++) {
        uint8_t *m = c->map + (size_t)i * 12;
        uint64_t offset = at;
        uint32_t length = 0, crc = 0;

        if (m[0] <= 4) {
            length = m[0] == 4 ? c->hunkBytes : ReadBits(&b, header[12]);
            crc = ReadBits(&b, 16);
            if (!length || length > c->hunkBytes || at > c->size || length > c->size - at) {
                goto done;
            }
            at += length;
        } else if (m[0] == 5 || m[0] == 9 || m[0] == 10) {
            if (m[0] == 5) {
                self = ReadBits(&b, header[13]);
            } else if (m[0] == 10) {
                self++;
            }
            m[0] = 5;
            offset = self;
            if (offset >= c->hunks) {
                goto done;
            }
        } else {
            goto done; /* parent references and reserved types */
        }
        PutBig(m + 1, length, 3);
        PutBig(m + 4, offset, 6);
        PutBig(m + 10, crc, 2);
    }
    ok = !b.bad && Crc16(0xFFFF, c->map, (size_t)c->hunks * 12) == Big(header + 10, 2);
done:
    SDL_free(packed);
    return ok;
}

/* The same P and Q parities as ECM's (ecm.c), over the header and payload. Mode 2's header is treated as zeros. */
static void RestoreEcc(uint8_t *sector)
{
    uint8_t forward[256], backward[256], header[4];
    unsigned i, pass;

    for (i = 0; i < 256; i++) {
        unsigned j = (i << 1) ^ (i & 128 ? 0x11D : 0);

        forward[i] = (uint8_t)j;
        backward[i ^ (j & 255)] = (uint8_t)i;
    }
    sector[0] = sector[11] = 0;
    memset(sector + 1, 255, 10);
    memcpy(header, sector + 12, 4);
    if (sector[15] == 2) {
        memset(sector + 12, 0, 4);
    }
    for (pass = 0; pass < 2; pass++) {
        unsigned major, minor, majors = pass ? 52 : 86, minors = pass ? 43 : 24, size = majors * minors;
        unsigned multiply = pass ? 86 : 2, increment = pass ? 88 : 86, out = pass ? 2248 : 2076;

        for (major = 0; major < majors; major++) {
            unsigned index = (major >> 1) * multiply + (major & 1);
            uint8_t a = 0, b = 0;

            for (minor = 0; minor < minors; minor++) {
                uint8_t value = sector[12 + index];

                index = (index + increment) % size;
                a = forward[a ^ value];
                b ^= value;
            }
            a = backward[forward[a] ^ b];
            sector[out + major] = a;
            sector[out + major + majors] = a ^ b;
        }
    }
    memcpy(sector + 12, header, 4);
}

static int DecodeHunk(Chd *c, uint32_t codec, size_t bytes)
{
    uint32_t frames = c->hunkBytes / CD_FRAME, i;
    size_t dataBytes = (size_t)frames * CD_SECTOR, subBytes = (size_t)frames * 96;

    if (codec == TAG('z','l','i','b')) {
        return Inflate(c->packed, bytes, c->hunk, c->hunkBytes);
    }
    if (codec == TAG('l','z','m','a')) {
        return Lzma(c->packed, bytes, c->hunk, c->hunkBytes);
    }
    if (codec == TAG('h','u','f','f')) {
        return HuffmanData(c->packed, bytes, c->hunk, c->hunkBytes);
    }
    if (codec == TAG('c','d','f','l')) {
        size_t consumed = Flac(c->packed, bytes, c->split, dataBytes);

        if (!consumed || consumed >= bytes || !Inflate(c->packed + consumed, bytes - consumed,
                                                      c->split + dataBytes, subBytes)) {
            return 0;
        }
    } else if (codec == TAG('c','d','z','l') || codec == TAG('c','d','l','z')) {
        size_t ecc = (frames + 7) / 8, lengthBytes = c->hunkBytes < 65536 ? 2 : 3, header = ecc + lengthBytes, length;

        if (bytes < header) {
            return 0;
        }
        length = (size_t)Big(c->packed + ecc, (unsigned)lengthBytes);
        if (length > bytes - header ||
            !(codec == TAG('c','d','z','l') ? Inflate(c->packed + header, length, c->split, dataBytes)
                                            : Lzma(c->packed + header, length, c->split, dataBytes)) ||
            !Inflate(c->packed + header + length, bytes - header - length, c->split + dataBytes, subBytes)) {
            return 0;
        }
    } else {
        return 0;
    }
    for (i = 0; i < frames; i++) {
        uint8_t *sector = c->hunk + (size_t)i * CD_FRAME;

        memcpy(sector, c->split + (size_t)i * CD_SECTOR, CD_SECTOR);
        memcpy(sector + CD_SECTOR, c->split + dataBytes + (size_t)i * 96, 96);
        if (codec != TAG('c','d','f','l') && (c->packed[i / 8] & (1u << (i % 8)))) {
            RestoreEcc(sector);
        }
    }
    return 1;
}

static int Hunk(Chd *c, uint32_t number)
{
    const uint8_t *m;
    uint32_t steps = 0;
    uint64_t offset;
    size_t length;

    if (c->cached == number) {
        return 1;
    }
    c->cached = UINT32_MAX;
    if (!c->compressed) {
        offset = Big(c->map + (size_t)number * 4, 4) * c->hunkBytes;
        if (!offset) {
            memset(c->hunk, 0, c->hunkBytes);
            return 1;
        }
        return ReadAt(c, offset, c->hunk, c->hunkBytes);
    }
    m = c->map + (size_t)number * 12;
    while (m[0] == 5) {
        if (++steps > c->hunks) {
            return 0; /* a loop in self references */
        }
        number = (uint32_t)Big(m + 4, 6);
        m = c->map + (size_t)number * 12;
    }
    offset = Big(m + 4, 6);
    length = (size_t)Big(m + 1, 3);
    if (!ReadAt(c, offset, c->packed, length)) {
        return 0;
    }
    if (m[0] == 4) {
        memcpy(c->hunk, c->packed, c->hunkBytes);
    } else if (!DecodeHunk(c, c->codecs[m[0]], length)) {
        return 0;
    }
    return Crc16(0xFFFF, c->hunk, c->hunkBytes) == Big(m + 10, 2);
}

/* ---- tracks, then the .bin and .cue ---- */

typedef struct {
    uint32_t frames, pregap, postgap;
    int audio, mode, stored;
    int cooked; /* 2048-byte sectors of user data */
} Track;

static int ReadTracks(Chd *c, Track *tracks, unsigned *count)
{
    uint64_t at = c->metadata, sectors = 0, discSectors = 0;
    unsigned entries = 0;

    *count = 0;
    while (at) {
        uint8_t header[16];
        uint32_t tag, length;
        uint64_t next;

        if (++entries > 1024 || !ReadAt(c, at, header, 16)) {
            return 0;
        }
        tag = (uint32_t)Big(header, 4);
        length = (uint32_t)Big(header + 5, 3);
        next = Big(header + 8, 8);
        if (tag == TAG('C','H','T','R') || tag == TAG('C','H','T','2')) {
            char text[512], type[32], sub[32], pgtype[32] = "", pgsub[32];
            int number, frames, pregap = 0, postgap = 0, fields;
            Track *t;

            if (*count == 99 || !length || length >= sizeof(text) || !ReadAt(c, at + 16, text, length)) {
                return 0;
            }
            text[length] = 0;
            fields = sscanf(text, "TRACK:%d TYPE:%31s SUBTYPE:%31s FRAMES:%d PREGAP:%d PGTYPE:%31s PGSUB:%31s POSTGAP:%d",
                            &number, type, sub, &frames, &pregap, pgtype, pgsub, &postgap);
            if (fields != (tag == TAG('C','H','T','2') ? 8 : 4) || number != (int)*count + 1 ||
                frames <= 0 || pregap < 0 || postgap < 0 || (pgtype[0] == 'V' && pregap >= frames)) {
                return 0;
            }
            t = &tracks[(*count)++];
            t->frames = (uint32_t)frames;
            t->pregap = (uint32_t)pregap;
            t->postgap = (uint32_t)postgap;
            t->stored = pgtype[0] == 'V';
            t->audio = !strcmp(type, "AUDIO");
            t->cooked = !strcmp(type, "MODE1");
            t->mode = t->cooked || !strcmp(type, "MODE1_RAW") ? 1 : !strcmp(type, "MODE2_RAW") ? 2 : 0;
            if ((!t->audio && !t->mode) || (t->pregap && t->stored && strcmp(pgtype + 1, type))) {
                return 0;
            }
            sectors += ((uint64_t)t->frames + 3) & ~(uint64_t)3;
            discSectors += (uint64_t)t->frames + t->postgap + (t->stored ? 0 : t->pregap);
        } else if (tag == TAG('C','H','G','T') || tag == TAG('C','H','G','D') || tag == TAG('C','H','C','D')) {
            return 0; /* GD-ROM or the older binary CD table */
        }
        at = next;
    }
    return *count && sectors * CD_FRAME == c->logical && discSectors <= 100 * 60 * 75;
}

static int Time(FILE *f, const char *name, uint64_t sector)
{
    return fprintf(f, "    %s %02u:%02u:%02u\n", name, (unsigned)(sector / 4500),
                   (unsigned)(sector / 75 % 60), (unsigned)(sector % 75)) >= 0;
}

int PortChdDecode(const char *in, const char *bin, const char *cue, void (*progress)(int),
                  char *message, size_t messageSize)
{
    Chd c = {0};
    uint8_t header[124], silence[CD_SECTOR] = {0};
    Track tracks[99];
    unsigned trackCount, track, i;
    uint64_t source = 0, output = 0;
    FILE *dst = NULL, *sheet = NULL;
    const char *name = strrchr(bin, '/');
    int ok = 0;

#ifdef _WIN32
    {
        const char *back = strrchr(bin, '\\');

        if (back && (!name || back > name)) {
            name = back;
        }
    }
#endif
    name = name ? name + 1 : bin;

    c.cached = UINT32_MAX;
    c.file = SDL_IOFromFile(in, "rb");
    if (!c.file) {
        SDL_snprintf(message, messageSize, "%s could not be read.", in);
        goto done;
    }
    {
        Sint64 size = SDL_GetIOSize(c.file);

        if (size < 0) {
            SDL_snprintf(message, messageSize, "%s could not be read.", in);
            goto done;
        }
        c.size = (uint64_t)size;
    }
    if (!ReadAt(&c, 0, header, sizeof(header)) || memcmp(header, "MComprHD", 8)) {
        goto damaged;
    }
    if (Big(header + 12, 4) != 5 || Big(header + 8, 4) != 124) {
        SDL_snprintf(message, messageSize, "%s is not a version 5 CHD; chdman can convert it.", in);
        goto done;
    }
    for (i = 104; i < 124; i++) {
        if (header[i]) {
            SDL_snprintf(message, messageSize, "%s needs a parent CHD; a complete CD image is needed.", in);
            goto done;
        }
    }
    c.logical = Big(header + 32, 8);
    c.mapAt = Big(header + 40, 8);
    c.metadata = Big(header + 48, 8);
    c.hunkBytes = (uint32_t)Big(header + 56, 4);
    if (Big(header + 60, 4) != CD_FRAME || !c.logical || c.logical % CD_FRAME ||
        !c.hunkBytes || c.hunkBytes % CD_FRAME || c.hunkBytes > 16 * 1024 * 1024 ||
        c.logical > (uint64_t)100 * 60 * 75 * CD_FRAME) {
        SDL_snprintf(message, messageSize, "%s is not a raw-sector CD CHD of a kind this can play.", in);
        goto done;
    }
    c.hunks = (uint32_t)((c.logical + c.hunkBytes - 1) / c.hunkBytes);
    for (i = 0; i < 4; i++) {
        uint32_t codec = c.codecs[i] = (uint32_t)Big(header + 16 + i * 4, 4);

        if (codec && codec != TAG('c','d','z','l') && codec != TAG('c','d','l','z') && codec != TAG('c','d','f','l') &&
            codec != TAG('z','l','i','b') && codec != TAG('l','z','m','a') && codec != TAG('h','u','f','f')) {
            SDL_snprintf(message, messageSize, "%s uses the CHD codec '%.4s', which this cannot decode.", in,
                         (const char *)header + 16 + i * 4);
            goto done;
        }
    }
    c.compressed = c.codecs[0] != 0;
    if (!c.compressed && (c.codecs[1] || c.codecs[2] || c.codecs[3])) {
        goto damaged;
    }
    c.map = SDL_calloc(c.hunks, c.compressed ? 12 : 4);
    c.packed = SDL_malloc(c.hunkBytes);
    c.hunk = SDL_malloc(c.hunkBytes);
    c.split = SDL_malloc(c.hunkBytes);
    if (!c.map || !c.packed || !c.hunk || !c.split) {
        SDL_snprintf(message, messageSize, "There is not enough memory to unpack %s.", in);
        goto done;
    }
    if (!ReadMap(&c) || !ReadTracks(&c, tracks, &trackCount)) {
        goto damaged;
    }
    dst = fopen(bin, "wb");
    sheet = fopen(cue, "wb");
    if (!dst || !sheet || fprintf(sheet, "FILE \"%s\" BINARY\n", name) < 0) {
        goto writeFailed;
    }
    for (track = 0; track < trackCount; track++) {
        Track *t = &tracks[track];
        char mode[32];
        uint32_t sector;

        SDL_snprintf(mode, sizeof(mode), "MODE%d/2352", t->mode);
        if (fprintf(sheet, "  TRACK %02u %s\n", track + 1, t->audio ? "AUDIO" : mode) < 0 ||
            (t->pregap && !(t->stored ? Time(sheet, "INDEX 00", output) : Time(sheet, "PREGAP", t->pregap))) ||
            !Time(sheet, "INDEX 01", output + (t->stored ? t->pregap : 0))) {
            goto writeFailed;
        }
        for (sector = 0; sector < t->frames; sector++, source++, output++) {
            uint32_t hunk = (uint32_t)(source / (c.hunkBytes / CD_FRAME));
            size_t offset = (size_t)(source % (c.hunkBytes / CD_FRAME)) * CD_FRAME;
            uint8_t raw[CD_SECTOR];

            if (c.cached != hunk) {
                if (!Hunk(&c, hunk)) {
                    SDL_snprintf(message, messageSize, "%s has a damaged CHD hunk (%u).", in, hunk);
                    goto done;
                }
                c.cached = hunk;
                if (progress) {
                    progress((int)((uint64_t)hunk * 1000 / c.hunks));
                }
            }
            if (t->cooked) {
                DiscMakeRawSector(raw, (int32_t)output, c.hunk + offset);
            } else {
                memcpy(raw, c.hunk + offset, CD_SECTOR);
            }
            if (t->audio) {
                for (i = 0; i < CD_SECTOR; i += 2) {
                    uint8_t byte = raw[i];

                    raw[i] = raw[i + 1];
                    raw[i + 1] = byte;
                }
            }
            if (fwrite(raw, 1, CD_SECTOR, dst) != CD_SECTOR) {
                goto writeFailed;
            }
        }
        for (sector = 0; sector < t->postgap; sector++, output++) {
            if (fwrite(silence, 1, CD_SECTOR, dst) != CD_SECTOR) {
                goto writeFailed;
            }
        }
        source = (source + 3) & ~(uint64_t)3;
    }
    ok = 1;
    goto done;
writeFailed:
    SDL_snprintf(message, messageSize, "%s could not be written (is the disk full?).", bin);
    goto done;
damaged:
    SDL_snprintf(message, messageSize, "%s is damaged, or has a CHD track layout this cannot play.", in);
done:
    if (dst && fclose(dst) != 0) {
        ok = 0;
        SDL_snprintf(message, messageSize, "%s could not be written (is the disk full?).", bin);
    }
    if (sheet && fclose(sheet) != 0) {
        ok = 0;
        SDL_snprintf(message, messageSize, "%s could not be written (is the disk full?).", cue);
    }
    if (!ok) {
        if (dst) {
            remove(bin);
        }
        if (sheet) {
            remove(cue);
        }
    }
    if (c.file) {
        SDL_CloseIO(c.file);
    }
    SDL_free(c.map);
    SDL_free(c.packed);
    SDL_free(c.hunk);
    SDL_free(c.split);
    return ok;
}
