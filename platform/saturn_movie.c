/* The movies of the Sega Saturn's disc (saturn.c), in place of the PlayStation's (movie.c): its intro and endings
   (\makecd\project\movies\*.pmm in its archive), its own format, read from its program's player (hand-written
   assembly there):

   "BIFF" size "PXMT" "PXMH" u32 64, a header of 64 bytes (big-endian: +2 flags, 0x10 the 256-colour kind; +4 and +6
   the width and height in 8x8 cells; +0xc frames a second x 256; +0xe the frame count; +0x34 the sound's rate, +0x36
   its kind (3: 4-bit ADPCM, 2: 8-bit through a table), +0x37 its channels), then chunks of a tag and a size, not
   padded: SYNC, then sound (AUDO) and pictures (BODY).

   A picture updates one frame buffer of the Saturn's 15-bit colours, cell by cell (8x8, by rows): a bit stream (32-bit
   big-endian words, bits from the lowest) says which cells change and how they split (8x8, four 4x4, four 2x2,
   pixels), and a stream of bytes gives the colours. In the 256-colour kind a byte is a colour of the picture's
   palette (0: the last again); in the 15-bit kind a 3-bit code per cell says which of red, green and blue change,
   and a byte sets one of them (or, 0x80 on, is a whole colour with the next). The Saturn's table for the code that
   changes red and green also sets blue (blue specks where red and green change: the encoder meant them alone); the
   picture is shown as meant. The sound: in turn per channel, blocks of 16 bytes (a shift, then 30 4-bit steps, the
   low nibble first, added to a running sum), or bytes through a table. A picture is shown at its time in the
   sound: frame n at sample n x rate / fps. */
#include <SDL3/SDL.h>
#include <stdlib.h>
#include <string.h>

#include "platform.h"
#include "saturn.h"
#include "settings.h"
#include "psx/gpu.h"
#include "psx/psx_io.h"

#define PAD_START 0x0008
#define PAD_CROSS 0x4000

static const int16_t pcm8Table[256] = {
    -32767, -30191, -27818, -25631, -23617, -21760, -20050, -18474, -17021, -15683, -14451, -13315, -12268, -11304,
    -10415, -9596,  -8842,  -8147,  -7506,  -6916,  -6373,  -5872,  -5410,  -4985,  -4593,  -4232,  -3899,  -3593,
    -3310,  -3050,  -2810,  -2589,  -2386,  -2198,  -2025,  -1866,  -1719,  -1584,  -1460,  -1345,  -1239,  -1142,
    -1052,  -969,   -893,   -823,   -758,   -698,   -643,   -593,   -546,   -503,   -464,   -427,   -393,   -363,
    -334,   -308,   -283,   -261,   -241,   -222,   -204,   -188,   -173,   -160,   -147,   -135,   -125,   -115,
    -106,   -97,    -90,    -83,    -76,    -70,    -65,    -59,    -55,    -50,    -46,    -43,    -39,    -36,
    -33,    -31,    -28,    -26,    -24,    -22,    -20,    -19,    -17,    -16,    -14,    -13,    -12,    -11,
    -10,    -9,     -9,     -8,     -7,     -7,     -6,     -6,     -5,     -5,     -4,     -4,     -4,     -3,
    -3,     -3,     -2,     -2,     -2,     -2,     -2,     -1,     -1,     -1,     -1,     -1,     -1,     -1,
    -1,     0,      1,      1,      1,      1,      1,      1,      1,      1,      2,      2,      2,      2,
    2,      3,      3,      3,      4,      4,      4,      5,      5,      6,      6,      7,      7,      8,
    9,      9,      10,     11,     12,     13,     14,     16,     17,     19,     20,     22,     24,     26,
    28,     31,     33,     36,     39,     43,     46,     50,     55,     59,     65,     70,     76,     83,
    90,     97,     106,    115,    125,    135,    147,    160,    173,    188,    204,    222,    241,    261,
    283,    308,    334,    363,    393,    427,    464,    503,    546,    593,    643,    698,    758,    823,
    893,    969,    1052,   1142,   1239,   1345,   1460,   1584,   1719,   1866,   2025,   2198,   2386,   2589,
    2810,   3050,   3310,   3593,   3899,   4232,   4593,   4985,   5410,   5872,   6373,   6916,   7506,   8147,
    8842,   9596,   10415,  11304,  12268,  13315,  14451,  15683,  17021,  18474,  20050,  21760,  23617,  25631,
    27818,  30191,  32767,  32767};

/* The 15-bit kind's colour updates: per block code, a byte below 97 sets blue (0-31), green (32-63) or red (64-95)
   (96: none); codes 3 (red and green), 5 (red and blue) and 6 (green and blue) set two alike. */
static uint16_t colourOr[8][97], colourAnd[8][97];
static const uint16_t keepMask[8] = {0xffff, 0xffe0, 0xfc1f, 0xfc00, 0x83ff, 0x83e0, 0x801f, 0x8000};

static void Tables(void)
{
    int code, v;

    for (code = 0; code < 8; code++) {
        for (v = 0; v < 97; v++) {
            uint16_t o, a;

            if (v < 32) {
                o = (uint16_t)(0x8000 | v << 10), a = 0x83ff;
            } else if (v < 64) {
                o = (uint16_t)(0x8000 | (v - 32) << 5), a = 0xfc1f;
            } else if (v < 96) {
                o = (uint16_t)(0x8000 | (v - 64)), a = 0xffe0;
            } else {
                o = 0x8000, a = 0xffff;
            }
            if (code == 3 && v < 32) {
                o = (uint16_t)(0x8000 | v * 0x21), a = 0xfc00; /* red and green (as meant: see above) */
            } else if (code == 5 && v >= 32 && v < 64) {
                o = (uint16_t)(0x8000 | (v - 32) * 0x401), a = 0x83e0;
            } else if (code == 6 && v >= 64 && v < 96) {
                o = (uint16_t)(0x8000 | (v - 64) * 0x420), a = 0x801f;
            }
            colourOr[code][v] = o;
            colourAnd[code][v] = a;
        }
    }
}

typedef struct Reader {
    const uint8_t *data;
    size_t size, at;
    uint32_t word;
    int left;
} Reader;

static int Bit(Reader *r)
{
    int v;

    if (!r->left) {
        r->word = r->at + 4 <= r->size ? (uint32_t)r->data[r->at] << 24 | (uint32_t)r->data[r->at + 1] << 16 |
                                             (uint32_t)r->data[r->at + 2] << 8 | r->data[r->at + 3]
                                       : 0;
        r->at += 4;
        r->left = 32;
    }
    v = (int)(r->word & 1);
    r->word >>= 1;
    r->left--;
    return v;
}

typedef struct Picture {
    const uint8_t *bytes;
    size_t size, at;
    const uint8_t *palette; /* the 256-colour kind's (big-endian words, at the picture's +4) */
    int code;
    uint16_t colour;
} Picture;

static uint16_t Byte(Picture *p)
{
    return p->at < p->size ? p->bytes[p->at++] : 0;
}

static uint16_t NextColour(Picture *p)
{
    int v = Byte(p);

    if (p->palette) {
        if (v) {
            p->colour = (uint16_t)(p->palette[v * 2] << 8 | p->palette[v * 2 + 1]); /* (0: the last again) */
        }
    } else if (v & 0x80) {
        p->colour = (uint16_t)(v << 8 | Byte(p));
    } else {
        v = v > 96 ? 96 : v;
        p->colour = (uint16_t)((p->colour & colourAnd[p->code][v]) | colourOr[p->code][v]);
    }
    return p->colour;
}

static void Put(uint16_t *buffer, int at, Picture *p, uint16_t c)
{
    buffer[at] = p->palette || p->code == 7 ? c : (uint16_t)((buffer[at] & keepMask[p->code]) | c);
}

static void Fill(uint16_t *buffer, int width, int at, int size, Picture *p)
{
    uint16_t c = NextColour(p);
    int x, y;

    for (y = 0; y < size; y++) {
        for (x = 0; x < size; x++) {
            Put(buffer, at + y * width + x, p, c);
        }
    }
}

static void DecodePicture(const uint8_t *body, size_t size, int palettised, int cellsW, int cellsH, uint16_t *buffer)
{
    int width = cellsW * 8, bx, by;
    Reader bits = {0};
    Picture p = {0};
    size_t base = palettised ? 0x204 : 0;

    if (size < base + 4) {
        return;
    }
    bits.data = body;
    bits.size = size;
    bits.at = base + (size_t)(body[base] << 8 | body[base + 1]);
    p.bytes = body;
    p.size = size;
    p.at = base + (size_t)(body[base + 2] << 8 | body[base + 3]);
    p.palette = palettised ? body + 4 : NULL;
    for (by = 0; by < cellsH; by++) {
        for (bx = 0; bx < cellsW; bx++) {
            int at = by * 8 * width + bx * 8, q, t, k;
            static const int quarter[4][2] = {{0, 0}, {4, 0}, {0, 4}, {4, 4}}, half[4][2] = {{0, 0}, {2, 0}, {0, 2}, {2, 2}};

            if (palettised) {
                if (!Bit(&bits)) {
                    continue;
                }
                p.code = 7;
            } else {
                p.code = Bit(&bits) << 2;
                p.code |= Bit(&bits) << 1;
                p.code |= Bit(&bits);
                if (!p.code) {
                    continue;
                }
            }
            p.colour = 0;
            if (!Bit(&bits)) {
                Fill(buffer, width, at, 8, &p);
                continue;
            }
            for (q = 0; q < 4; q++) {
                int aq = at + quarter[q][1] * width + quarter[q][0];

                if (!Bit(&bits)) {
                    Fill(buffer, width, aq, 4, &p);
                    continue;
                }
                for (t = 0; t < 4; t++) {
                    int a2 = aq + half[t][1] * width + half[t][0];

                    if (Bit(&bits)) {
                        for (k = 0; k < 4; k++) {
                            Put(buffer, a2 + half[k][1] / 2 * width + half[k][0] / 2, &p, NextColour(&p));
                        }
                    } else {
                        Fill(buffer, width, a2, 2, &p);
                    }
                }
            }
        }
    }
}

/* The sound, all its chunks one stream; interleaved 16-bit. */
static int16_t *DecodeSound(const uint8_t *movie, size_t size, size_t chunksAt, int kind, int channels, size_t *frames)
{
    int16_t *out = NULL;
    size_t count = 0, capacity = 0, at = chunksAt;
    int sum[2] = {0, 0};

    while (at + 8 <= size) {
        size_t length = (size_t)movie[at + 4] << 24 | (size_t)movie[at + 5] << 16 | (size_t)movie[at + 6] << 8 | movie[at + 7];
        const uint8_t *a = movie + at + 8;

        if (at + 8 + length > size) {
            break;
        }
        if (!memcmp(movie + at, "AUDO", 4)) {
            size_t blocks = kind == 3 ? length / (16 * (size_t)channels) : length / (size_t)channels, b;
            size_t add = kind == 3 ? blocks * 30 : blocks;

            if (count + add > capacity) {
                capacity = (count + add) * 2;
                out = SDL_realloc(out, capacity * (size_t)channels * sizeof(int16_t));
            }
            for (b = 0; b < blocks; b++) {
                int c;

                for (c = 0; c < channels; c++) {
                    if (kind == 3) {
                        const uint8_t *block = a + (b * (size_t)channels + (size_t)c) * 16;
                        int shift = block[0], i;

                        for (i = 0; i < 30; i++) {
                            int n = (block[1 + i / 2] >> ((i & 1) * 4)) & 15;

                            sum[c] += shift < 32 ? (n < 8 ? n : n - 16) * (1 << shift) : 0;
                            sum[c] = SDL_clamp(sum[c], -32768, 32767);
                            out[(count + b * 30 + (size_t)i) * (size_t)channels + (size_t)c] = (int16_t)sum[c];
                        }
                    } else {
                        out[(count + b) * (size_t)channels + (size_t)c] = pcm8Table[a[b * (size_t)channels + (size_t)c]];
                    }
                }
            }
            count += add;
        }
        at += 8 + length;
    }
    *frames = count;
    return out;
}

int SaturnPlayMovie(const char *path)
{
    size_t size, chunksAt, at, soundFrames = 0;
    uint8_t *movie = PortSaturnEntry(path, &size);
    int flags, cellsW, cellsH, frameCount, rate, kind, channels, shown = 0, palettised;
    double fps;
    uint16_t *buffer, previous;
    uint8_t *rgb;
    int16_t *sound;
    SDL_AudioStream *audio = NULL;
    uint64_t start;

    if (!movie || size < 84 || memcmp(movie, "BIFF", 4) || memcmp(movie + 8, "PXMTPXMH", 8)) {
        SDL_free(movie);
        return 0;
    }
    Tables();
    {
        const uint8_t *h = movie + 20;

        flags = h[2] << 8 | h[3];
        cellsW = h[4] << 8 | h[5];
        cellsH = h[6] << 8 | h[7];
        fps = (h[0xc] << 8 | h[0xd]) / 256.0;
        frameCount = h[0xe] << 8 | h[0xf];
        rate = h[0x34] << 8 | h[0x35];
        kind = h[0x36];
        channels = SDL_clamp(h[0x37], 1, 2);
        chunksAt = 20 + ((size_t)movie[16] << 24 | (size_t)movie[17] << 16 | (size_t)movie[18] << 8 | movie[19]);
    }
    if (cellsW <= 0 || cellsH <= 0 || cellsW > 80 || cellsH > 60 || fps <= 0) {
        SDL_free(movie);
        return 0;
    }
    palettised = (flags & 0x10) != 0;
    sound = DecodeSound(movie, size, chunksAt, kind, channels, &soundFrames);
    if (!portConfig.muted && !portConfig.fixedStep && sound && soundFrames) {
        SDL_AudioSpec spec;

        spec.format = SDL_AUDIO_S16;
        spec.channels = channels;
        spec.freq = rate;
        audio = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
        if (audio) {
            SDL_PutAudioStreamData(audio, sound, (int)(soundFrames * (size_t)channels * sizeof(int16_t)));
            SDL_FlushAudioStream(audio);
            SDL_ResumeAudioStreamDevice(audio);
        }
    }
    buffer = SDL_calloc((size_t)cellsW * 8 * cellsH * 8, sizeof(uint16_t));
    rgb = SDL_malloc((size_t)cellsW * 8 * cellsH * 8 * 3);
    previous = PsxPadButtons(0);
    PortLog("movie %s (Saturn) at field %d\n", path, (int)PsxFieldCount());
    start = SDL_GetTicksNS();
    at = chunksAt;
    while (at + 8 <= size && shown < frameCount) {
        size_t length = (size_t)movie[at + 4] << 24 | (size_t)movie[at + 5] << 16 | (size_t)movie[at + 6] << 8 | movie[at + 7];
        uint16_t buttons, pressed;
        int i;

        if (at + 8 + length > size) {
            break;
        }
        if (memcmp(movie + at, "BODY", 4)) {
            at += 8 + length;
            continue;
        }
        if (!PortPumpEvents()) {
            exit(0);
        }
        buttons = PsxPadButtons(0);
        pressed = buttons & (uint16_t)~previous;
        previous = buttons;
        if (pressed & (PAD_START | PAD_CROSS)) {
            break;
        }
        DecodePicture(movie + at + 8, length, palettised, cellsW, cellsH, buffer);
        /* its time in the sound */
        while (!portConfig.fixedStep && (SDL_GetTicksNS() - start) / 1e9 < shown / fps) {
            SDL_Delay(2);
        }
        for (i = 0; i < cellsW * 8 * cellsH * 8; i++) {
            uint16_t c = buffer[i];

            rgb[i * 3] = (uint8_t)(((c & 31) * 527 + 23) >> 6);
            rgb[i * 3 + 1] = (uint8_t)((((c >> 5) & 31) * 527 + 23) >> 6);
            rgb[i * 3 + 2] = (uint8_t)((((c >> 10) & 31) * 527 + 23) >> 6);
        }
        GpuShowImage(rgb, cellsW * 8, cellsH * 8);
        shown++;
        at += 8 + length;
    }
    if (audio) {
        SDL_DestroyAudioStream(audio);
    }
    SDL_free(sound);
    SDL_free(rgb);
    SDL_free(buffer);
    SDL_free(movie);
    return 1;
}
