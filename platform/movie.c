/* The intro and ending movies.

   On the console the game loads a movie-player program (NTSCANIM.EXE for the intro, XTRO1-4.EXE
   for the endings) and runs it. The player streams its movie file from the disc (MAKE.AV,
   XTRO1-4.AV) at double speed, 150 sectors a second, shows each frame as its sectors arrive, and
   stops two frames before the count it was built with, at the end of the file, or when Start or
   Cross is newly pressed. The port plays the same files from the disc image, frame for frame.

   A movie file is a stream of sectors. One in eight is sound: XA ADPCM, which the drive plays as
   it reads it. The others carry the video: a 32-byte header (0x0160, 0x8001, the chunk's number,
   the frame's count of chunks, the frame's number, its size in bytes, its width and height) and
   2016 bytes of the frame. A frame is the picture as the console's MDEC decoder takes it,
   compressed (bitstream version 1 or 2, which are the same): a header of four halfwords (the
   count of MDEC codes, 0x3800, the quantisation scale, the version), then the macroblocks of
   16x16 pixels, by columns from the top left, each six 8x8 blocks (Cr, Cb, then the four of Y).
   A block is a 10-bit DC coefficient and then AC coefficients as MPEG-1 codes them (run of
   zeros and level, with a sign bit; 000001 escapes a 6-bit run and a 10-bit level; 10 ends the
   block). The bits are read from 16-bit little-endian words, highest bit first. */
#include <SDL3/SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pc.h"
#include "platform.h"
#include "region.h"
#include "saturn.h"
#include "settings.h"
#include "psx/disc.h"
#include "psx/gpu.h"
#include "psx/psx_io.h"

#define PAD_START 0x0008
#define PAD_CROSS 0x4000

#define STREAM_SECTORS_PER_SECOND 150 /* the drive at double speed */
#define CHUNK_HEADER 32
#define CHUNK_BYTES 2016
#define MAX_CHUNKS 32
#define XA_GROUPS 18
#define XA_GROUP_BYTES 128

/* The console's movie programs: the file each plays and the frame count it was built with (the US disc's at 30
   frames a second; WipEout 2097's, Europe's, at 25: the same movies, the European program PALANIM.EXE for the
   intro, region.c). */
static const struct {
    const char *program, *file;
    int frames, europeFrames;
} movies[] = {{"ntscanim.exe", "\\MAKE.AV", 2044, 1709}, {"xtro1.exe", "\\XTRO1.AV", 630, 540},
              {"xtro2.exe", "\\XTRO2.AV", 600, 500},      {"xtro3.exe", "\\XTRO3.AV", 780, 650},
              {"xtro4.exe", "\\XTRO4.AV", 960, 800}};

/* ---- the pictures ---- */

/* The AC coefficients' codes (without the sign bit): run of zeros, level. */
static const struct {
    const char *code;
    uint8_t run, level;
} acCodes[] = {
    {"11", 0, 1}, {"011", 1, 1}, {"0100", 0, 2}, {"0101", 2, 1}, {"00101", 0, 3}, {"00110", 4, 1},
    {"00111", 3, 1}, {"000100", 7, 1}, {"000101", 6, 1}, {"000110", 1, 2}, {"000111", 5, 1}, {"0000100", 2, 2},
    {"0000101", 9, 1}, {"0000110", 0, 4}, {"0000111", 8, 1}, {"00100000", 13, 1}, {"00100001", 0, 6},
    {"00100010", 12, 1}, {"00100011", 11, 1}, {"00100100", 3, 2}, {"00100101", 1, 3}, {"00100110", 0, 5},
    {"00100111", 10, 1}, {"0000001000", 16, 1}, {"0000001001", 5, 2}, {"0000001010", 0, 7},
    {"0000001011", 2, 3}, {"0000001100", 1, 4}, {"0000001101", 15, 1}, {"0000001110", 14, 1},
    {"0000001111", 4, 2}, {"000000010000", 0, 11}, {"000000010001", 8, 2}, {"000000010010", 4, 3},
    {"000000010011", 0, 10}, {"000000010100", 2, 4}, {"000000010101", 7, 2}, {"000000010110", 21, 1},
    {"000000010111", 20, 1}, {"000000011000", 0, 9}, {"000000011001", 19, 1}, {"000000011010", 18, 1},
    {"000000011011", 1, 5}, {"000000011100", 3, 3}, {"000000011101", 0, 8}, {"000000011110", 6, 2},
    {"000000011111", 17, 1}, {"0000000010000", 10, 2}, {"0000000010001", 9, 2}, {"0000000010010", 5, 3},
    {"0000000010011", 3, 4}, {"0000000010100", 2, 5}, {"0000000010101", 1, 7}, {"0000000010110", 1, 6},
    {"0000000010111", 0, 15}, {"0000000011000", 0, 14}, {"0000000011001", 0, 13}, {"0000000011010", 0, 12},
    {"0000000011011", 26, 1}, {"0000000011100", 25, 1}, {"0000000011101", 24, 1}, {"0000000011110", 23, 1},
    {"0000000011111", 22, 1}, {"00000000010000", 0, 31}, {"00000000010001", 0, 30}, {"00000000010010", 0, 29},
    {"00000000010011", 0, 28}, {"00000000010100", 0, 27}, {"00000000010101", 0, 26}, {"00000000010110", 0, 25},
    {"00000000010111", 0, 24}, {"00000000011000", 0, 23}, {"00000000011001", 0, 22}, {"00000000011010", 0, 21},
    {"00000000011011", 0, 20}, {"00000000011100", 0, 19}, {"00000000011101", 0, 18}, {"00000000011110", 0, 17},
    {"00000000011111", 0, 16}, {"000000000010000", 0, 40}, {"000000000010001", 0, 39},
    {"000000000010010", 0, 38}, {"000000000010011", 0, 37}, {"000000000010100", 0, 36},
    {"000000000010101", 0, 35}, {"000000000010110", 0, 34}, {"000000000010111", 0, 33},
    {"000000000011000", 0, 32}, {"000000000011001", 1, 14}, {"000000000011010", 1, 13},
    {"000000000011011", 1, 12}, {"000000000011100", 1, 11}, {"000000000011101", 1, 10},
    {"000000000011110", 1, 9}, {"000000000011111", 1, 8}, {"0000000000010000", 1, 18},
    {"0000000000010001", 1, 17}, {"0000000000010010", 1, 16}, {"0000000000010011", 1, 15},
    {"0000000000010100", 6, 3}, {"0000000000010101", 16, 2}, {"0000000000010110", 15, 2},
    {"0000000000010111", 14, 2}, {"0000000000011000", 13, 2}, {"0000000000011001", 12, 2},
    {"0000000000011010", 11, 2}, {"0000000000011011", 31, 1}, {"0000000000011100", 30, 1},
    {"0000000000011101", 29, 1}, {"0000000000011110", 28, 1}, {"0000000000011111", 27, 1},
};

/* The quantisation table the console's libraries give the MDEC, and the zigzag order. */
static const uint8_t quantTable[64] = {
    2,  16, 19, 22, 26, 27, 29, 34, 16, 16, 22, 24, 27, 29, 34, 37, 19, 22, 26, 27, 29, 34,
    34, 38, 22, 22, 26, 27, 29, 34, 37, 40, 22, 26, 27, 29, 32, 35, 40, 48, 26, 27, 29, 32,
    35, 40, 48, 58, 26, 27, 29, 34, 38, 46, 56, 69, 27, 29, 35, 38, 46, 56, 69, 83};
static const uint8_t zigzag[64] = {
    0,  1,  8,  16, 9,  2,  3,  10, 17, 24, 32, 25, 18, 11, 4,  5,  12, 19, 26, 33, 40, 48,
    41, 34, 27, 20, 13, 6,  7,  14, 21, 28, 35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23,
    30, 37, 44, 51, 58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63};

/* The codes by their next 16 bits: length (0: none), run, level. */
static struct {
    uint8_t length, run, level;
} acTable[1 << 16];
static float idctCos[8][8];

static void InitDecoder(void)
{
    size_t i;
    int u, x;

    if (acTable[0xFFFF].length) {
        return; /* done ("11" fills the top of the table) */
    }
    for (i = 0; i < SDL_arraysize(acCodes); i++) {
        int length = (int)strlen(acCodes[i].code), code = (int)strtol(acCodes[i].code, NULL, 2), k;

        for (k = code << (16 - length); k < (code + 1) << (16 - length); k++) {
            acTable[k].length = (uint8_t)length;
            acTable[k].run = acCodes[i].run;
            acTable[k].level = acCodes[i].level;
        }
    }
    for (u = 0; u < 8; u++) {
        for (x = 0; x < 8; x++) {
            idctCos[u][x] = (u ? 1.0f : 0.70710678f) * cosf((2 * x + 1) * u * 3.14159265f / 16) / 2;
        }
    }
}

typedef struct Bits {
    const uint8_t *data;
    size_t size, pos;
    uint32_t window; /* the next bits, highest first */
    int count;       /* how many of them are valid */
} Bits;

static void Refill(Bits *bits)
{
    while (bits->count <= 16) {
        uint32_t word = bits->pos + 1 < bits->size ? bits->data[bits->pos] | bits->data[bits->pos + 1] << 8 : 0;

        bits->pos += 2;
        bits->window |= word << (16 - bits->count);
        bits->count += 16;
    }
}

static uint32_t Peek(Bits *bits, int n)
{
    Refill(bits);
    return bits->window >> (32 - n);
}

static void Skip(Bits *bits, int n)
{
    bits->window <<= n;
    bits->count -= n;
}

static int Signed10(uint32_t v)
{
    return (int)(v & 0x3FF) - (v & 0x200 ? 0x400 : 0);
}

/* One 8x8 block, as the MDEC dequantises and transforms it. False on a broken stream. */
static bool DecodeBlock(Bits *bits, int scale, float out[64])
{
    float coeff[64] = {0}, rows[64];
    int k = 0, u, v, x, y;

    coeff[0] = (float)(Signed10(Peek(bits, 10)) * quantTable[0]);
    Skip(bits, 10);
    for (;;) {
        int run, level, value;

        if (Peek(bits, 2) == 2) {
            Skip(bits, 2); /* the end of the block */
            break;
        }
        if (Peek(bits, 6) == 1) {
            uint32_t escaped;

            Skip(bits, 6);
            escaped = Peek(bits, 16);
            Skip(bits, 16);
            run = (int)(escaped >> 10);
            level = Signed10(escaped);
        } else {
            uint32_t next = Peek(bits, 16);

            if (!acTable[next].length) {
                return false;
            }
            Skip(bits, acTable[next].length);
            run = acTable[next].run;
            level = Peek(bits, 1) ? -acTable[next].level : acTable[next].level;
            Skip(bits, 1);
        }
        k += run + 1;
        if (k > 63) {
            return false;
        }
        value = (level * quantTable[zigzag[k]] * scale + 4) / 8;
        coeff[zigzag[k]] = (float)SDL_clamp(value, -1024, 1023);
    }
    for (v = 0; v < 8; v++) {
        for (x = 0; x < 8; x++) {
            float sum = 0;

            for (u = 0; u < 8; u++) {
                sum += idctCos[u][x] * coeff[v * 8 + u];
            }
            rows[v * 8 + x] = sum;
        }
    }
    for (y = 0; y < 8; y++) {
        for (x = 0; x < 8; x++) {
            float sum = 0;

            for (v = 0; v < 8; v++) {
                sum += idctCos[v][y] * rows[v * 8 + x];
            }
            out[y * 8 + x] = sum;
        }
    }
    return true;
}

static uint8_t Clamp255(float v)
{
    return (uint8_t)(v < 0 ? 0 : v > 255 ? 255 : v + 0.5f);
}

/* A frame to 24-bit RGB (width x height). False when it is not a frame this decoder knows. */
static bool DecodePicture(const uint8_t *data, size_t size, int width, int height, uint8_t *rgb)
{
    Bits bits = {data + 8, size > 8 ? size - 8 : 0, 0, 0, 0};
    int scale, version, mx, my;

    if (size < 8 || (data[2] | data[3] << 8) != 0x3800) {
        return false;
    }
    scale = data[4] | data[5] << 8;
    version = data[6] | data[7] << 8;
    if (version != 1 && version != 2) {
        return false;
    }
    for (mx = 0; mx < width; mx += 16) {
        for (my = 0; my < height; my += 16) {
            float blocks[6][64];
            int b, x, y;

            for (b = 0; b < 6; b++) {
                if (!DecodeBlock(&bits, scale, blocks[b])) {
                    return false;
                }
            }
            for (y = 0; y < 16 && my + y < height; y++) {
                for (x = 0; x < 16 && mx + x < width; x++) {
                    float luma = blocks[2 + (y / 8) * 2 + x / 8][(y % 8) * 8 + x % 8] + 128;
                    float cr = blocks[0][(y / 2) * 8 + x / 2], cb = blocks[1][(y / 2) * 8 + x / 2];
                    uint8_t *p = rgb + ((size_t)(my + y) * width + mx + x) * 3;

                    p[0] = Clamp255(luma + 1.402f * cr);
                    p[1] = Clamp255(luma - 0.3437f * cb - 0.7143f * cr);
                    p[2] = Clamp255(luma + 1.772f * cb);
                }
            }
        }
    }
    return true;
}

/* ---- the sound ---- */

typedef struct Sound {
    int16_t *samples; /* interleaved by channel */
    size_t frames, capacity;
    int channels, rate;
    int previous[2][2]; /* the decoder's last two samples, per channel */
} Sound;

/* Appends an XA ADPCM sector's samples (4-bit; 8-bit sectors are not used by the movies). */
static void DecodeXa(Sound *sound, const uint8_t *sector)
{
    static const int positive[4] = {0, 60, 115, 98}, negative[4] = {0, 0, -52, -55};
    int coding = sector[19], stereo = coding & 1, group, unit, i;

    if (coding & 0x10) {
        return;
    }
    if (!sound->channels) {
        sound->channels = stereo ? 2 : 1;
        sound->rate = coding & 4 ? 18900 : 37800;
    }
    if (sound->frames + 4032 > sound->capacity) {
        sound->capacity = sound->capacity * 2 + 8064;
        sound->samples = SDL_realloc(sound->samples, sound->capacity * 2 * sizeof(int16_t));
    }
    for (group = 0; group < XA_GROUPS; group++) {
        const uint8_t *g = sector + 24 + group * XA_GROUP_BYTES;

        for (unit = 0; unit < 8; unit++) {
            int header = g[4 + unit], shift = header & 15, filter = (header >> 4) & 3;
            int channel = stereo ? unit & 1 : 0, *last = sound->previous[channel];
            /* stereo: units 0 and 1 are the left and right of 28 frames, then 2 and 3, ... */
            size_t first = sound->frames + group * (stereo ? 112 : 224) + (stereo ? unit / 2 : unit) * 28;

            if (shift > 12) {
                shift = 9;
            }
            for (i = 0; i < 28; i++) {
                int nibble = (g[16 + i * 4 + unit / 2] >> ((unit & 1) * 4)) & 15;
                int s = ((int16_t)(nibble << 12) >> shift) + ((last[0] * positive[filter] + last[1] * negative[filter] + 32) >> 6);

                s = SDL_clamp(s, -32768, 32767);
                last[1] = last[0];
                last[0] = s;
                sound->samples[(first + i) * sound->channels + channel] = (int16_t)s;
            }
        }
    }
    sound->frames += stereo ? 2016 : 4032;
}

/* ---- the stream ---- */

typedef struct Frame {
    int number, width, height, chunks;
    uint32_t size;
    int32_t sectors[MAX_CHUNKS];
    int arrived; /* the stream's sectors read when its last chunk is in */
} Frame;

/* Reads the movie's sectors once: the frames' chunks and the whole sound. */
static Frame *ScanMovie(const Disc *disc, int32_t lba, int sectors, int *frameCount, Sound *sound, int *soundStart)
{
    Frame *frames = NULL;
    int count = 0, capacity = 0, i;

    *soundStart = -1;
    for (i = 0; i < sectors; i++) {
        const uint8_t *sector = DiscRawSector(disc, lba + i);
        const uint8_t *d;
        int chunk, chunks, number;
        Frame *frame;

        if (!sector) {
            break;
        }
        if (sector[18] & 0x04) { /* the subheader's submode: audio */
            if (*soundStart < 0) {
                *soundStart = i;
            }
            DecodeXa(sound, sector);
            continue;
        }
        d = sector + 24;
        if ((d[0] | d[1] << 8) != 0x0160 || (d[2] | d[3] << 8) != 0x8001) {
            continue;
        }
        chunk = d[4] | d[5] << 8;
        chunks = d[6] | d[7] << 8;
        number = (int)(d[8] | d[9] << 8 | d[10] << 16 | (uint32_t)d[11] << 24);
        if (chunks > MAX_CHUNKS || chunk >= chunks) {
            continue;
        }
        if (!count || frames[count - 1].number != number) {
            if (count == capacity) {
                capacity = capacity * 2 + 256;
                frames = SDL_realloc(frames, capacity * sizeof(Frame));
            }
            frame = &frames[count++];
            memset(frame, 0, sizeof(*frame));
            frame->number = number;
            frame->chunks = chunks;
            frame->size = d[12] | d[13] << 8 | d[14] << 16 | (uint32_t)d[15] << 24;
            frame->width = d[16] | d[17] << 8;
            frame->height = d[18] | d[19] << 8;
        }
        frame = &frames[count - 1];
        frame->sectors[chunk] = lba + i;
        frame->arrived = i + 1;
    }
    *frameCount = count;
    return frames;
}

/* A frame's chunks joined and decoded. */
static bool ShowFrame(const Disc *disc, const Frame *frame, uint8_t *data, uint8_t *rgb)
{
    int i;

    if (frame->width <= 0 || frame->height <= 0 || frame->width > 640 || frame->height > 480) {
        return false;
    }
    for (i = 0; i < frame->chunks; i++) {
        const uint8_t *sector = frame->sectors[i] ? DiscRawSector(disc, frame->sectors[i]) : NULL;

        if (!sector) {
            return false;
        }
        memcpy(data + i * CHUNK_BYTES, sector + 24 + CHUNK_HEADER, CHUNK_BYTES);
    }
    if (!DecodePicture(data, SDL_min(frame->size, (uint32_t)(frame->chunks * CHUNK_BYTES)), frame->width, frame->height, rgb)) {
        return false;
    }
    GpuShowImage(rgb, frame->width, frame->height);
    return true;
}

void PsxPlayMovie(const char *program)
{
    const Disc *disc = PsxIoDisc();
    const char *name = strrchr(program, '\\');
    Sound sound = {0};
    SDL_AudioStream *audio = NULL;
    Frame *frames;
    uint8_t *data, *rgb;
    uint64_t start;
    uint16_t previous;
    int32_t lba;
    uint32_t size;
    int movie = -1, frameCount, soundStart, shown = 0, i;

    if (portConfig.secondPlayer) {
        return; /* the second player's game shows no movies */
    }
    if (portConfig.fixedStep && !SDL_getenv("WIPEOUT_MOVIES")) {
        return; /* tests: the shown frames' count (the captures' numbers) stays the game's */
    }
    name = name ? name + 1 : program;
    for (i = 0; i < (int)SDL_arraysize(movies); i++) {
        if (!SDL_strcasecmp(name, movies[i].program)) {
            movie = i;
        }
    }
    if (movie < 0 || (movie == 0 && portSettings.skipIntro)) {
        return; /* not a movie program, or the settings: no intro */
    }
    if (PortPcData()) {
        /* the PC's own (pc_movie.c): MPEG-1 files, its intro and endings */
        static const char *const pcMovies[] = {"MPEG\\WXLINTR.MPG", "MPEG\\XTRO1.MPG", "MPEG\\XTRO2.MPG",
                                               "MPEG\\XTRO3.MPG", "MPEG\\XTRO4.MPG"};

        if (!PortPcPlayMovie(pcMovies[movie])) {
            PortLog("movie %s: not on the PC CD; skipped\n", pcMovies[movie]);
        }
        return;
    }
    if (PortSaturnDisc()) {
        /* the Saturn's own (saturn_movie.c): its intro (Wipeout XL's; the European disc has none) and endings */
        static const char *const saturnMovies[] = {
            "\\makecd\\project\\movies\\makexl.pmm", "\\makecd\\project\\movies\\xtro1.pmm",
            "\\makecd\\project\\movies\\xtro2.pmm", "\\makecd\\project\\movies\\xtro3.pmm",
            "\\makecd\\project\\movies\\xtro4.pmm"};

        if (!SaturnPlayMovie(saturnMovies[movie])) {
            PortLog("movie %s: not on the Saturn's disc; skipped\n", saturnMovies[movie]);
        }
        return;
    }
    if (!disc || !DiscFindFile(disc, movies[movie].file, &lba, &size)) {
        PortLog("movie %s: %s not on the disc; skipped\n", program, movies[movie].file);
        return;
    }
    InitDecoder();
    frames = ScanMovie(disc, lba, (int)(size / 2048), &frameCount, &sound, &soundStart);
    data = SDL_malloc(MAX_CHUNKS * CHUNK_BYTES);
    rgb = SDL_malloc(640 * 480 * 3);
    if (!portConfig.muted && !portConfig.fixedStep && sound.frames) {
        SDL_AudioSpec spec;

        spec.format = SDL_AUDIO_S16;
        spec.channels = sound.channels;
        spec.freq = sound.rate;
        audio = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
        if (audio) {
            /* the sound starts where its first sector is in the stream */
            size_t lead = (size_t)soundStart * sound.rate / STREAM_SECTORS_PER_SECOND * sound.channels;
            int16_t *silence = SDL_calloc(lead + 1, sizeof(int16_t));

            SDL_PutAudioStreamData(audio, silence, (int)(lead * sizeof(int16_t)));
            SDL_PutAudioStreamData(audio, sound.samples, (int)(sound.frames * sound.channels * sizeof(int16_t)));
            SDL_FlushAudioStream(audio);
            SDL_free(silence);
            SDL_ResumeAudioStreamDevice(audio);
        }
    }

    previous = PsxPadButtons(0);
    PortLog("movie %s at field %d\n", movies[movie].file, (int)PsxFieldCount());
    start = SDL_GetTicksNS();
    while (shown < frameCount) {
        uint16_t buttons, pressed;
        int next;

        if (!PortPumpEvents()) {
            exit(0);
        }
        buttons = PsxPadButtons(0);
        pressed = buttons & (uint16_t)~previous;
        previous = buttons;
        if (pressed & (PAD_START | PAD_CROSS)) {
            break;
        }
        /* the latest frame whose sectors are in: as the drive reads (each frame is whole on its
           own, so frames that are late are left out), or one frame per step in the fixed-step test mode */
        next = shown;
        if (!portConfig.fixedStep) {
            double sectorsRead = (SDL_GetTicksNS() - start) / 1e9 * STREAM_SECTORS_PER_SECOND;

            while (next + 1 < frameCount && frames[next + 1].arrived <= sectorsRead) {
                next++;
            }
            if (frames[next].arrived > sectorsRead) {
                SDL_Delay(2);
                continue;
            }
        }
        if (next > 0 && frames[next].number < frames[next - 1].number) {
            break; /* the numbers start again: the end */
        }
        ShowFrame(disc, &frames[next], data, rgb);
        shown = next + 1;
        if (frames[next].number >= (PortRegionEurope() ? movies[movie].europeFrames : movies[movie].frames) - 2) {
            break; /* as the console's player, two frames before its count */
        }
    }
    if (audio) {
        SDL_DestroyAudioStream(audio);
    }
    SDL_free(sound.samples);
    SDL_free(rgb);
    SDL_free(data);
    SDL_free(frames);
}
