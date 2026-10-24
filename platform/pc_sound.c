/* The game's sounds with the PC version's files (pc.c): samples.wad made from its SOUND folder.

   The PC has the PlayStation's 39 sounds as WAVs (PCM, 16-bit or 8-bit, mono, 22050 Hz mostly), some under other
   names (SHP2SHP for shipship, NRG_LOW for shldlow, ...), and two more of its own (HITWALL and MENUSLID's
   alternatives are not used). The game reads samples.wad's entries in its order, not by name: the PlayStation's
   names in its order, each a VAG (a 48-byte header, then 16-byte SPU ADPCM blocks, laid out as the PlayStation's:
   a one-shot ends with a block that stops the voice, a loop's last block jumps back to its first, the loop
   resampled to whole blocks). The 7 that loop are the PlayStation's 7 looping ones. */
#include <SDL3/SDL.h>
#include <string.h>

#include "pc.h"

/* The PlayStation's sounds in samples.wad's order, and the PC's WAV for each; whether it loops. */
static const struct {
    const char *name, *wav;
    int loops;
} sounds[39] = {
    {"shipship.vag", "SHP2SHP"},  {"weapnhit.vag", "WEAPNHIT"},  {"electro.vag", "ELECTRO"},
    {"engines.vag", "ENGINES", 1}, {"wepnwall.vag", "WEPNWALL"}, {"hitwall.vag", "HITWALL2"},
    {"menumove.vag", "MENUMOVE"}, {"menusel.vag", "MENUSEL"},   {"recharge.vag", "RECHARGE", 1},
    {"mine.vag", "MINE"},         {"missile.vag", "MISSILE"},   {"pickup.vag", "PICKUP"},
    {"shield2.vag", "SHIELD", 1}, {"spautopl.vag", "AUTOPLOT"}, {"droid.vag", "DROID", 1},
    {"turblnce.vag", "TURBLNCE", 1}, {"crowd.vag", "CROWD", 1}, {"spmines.vag", "MINES"},
    {"spmissl.vag", "MISSILES"},  {"sprocket.vag", "ROCKETS"},  {"spelectr.vag", "ELECBOLT"},
    {"three.vag", "THREE"},       {"two.vag", "TWO"},           {"one.vag", "ONE"},
    {"go.vag", "GO"},             {"newturbo.vag", "NEWTURBO"}, {"thunder.vag", "THUNDER"},
    {"shpstall.vag", "SHPSTALL"}, {"locked.vag", "LOCKED"},     {"quake.vag", "QUAKE"},
    {"plasma.vag", "PLASMA"},     {"chkpoint.vag", "CHKPOINT"}, {"dtchwepn.vag", "DTCHWEPN"},
    {"epak.vag", "EPAK"},         {"warning.vag", "WARNING"},   {"menuslid.vag", "MENUSLID"},
    {"scrapelp.vag", "SCRAPE", 1}, {"eliminat.vag", "CONTELIM"}, {"shldlow.vag", "NRG_LOW"},
};

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

/* A VAG of n samples at `rate`, looping or not. */
static void MakeVag(Out *vag, const int16_t *pcm, int n, int loops, int rate, const char *name)
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
            int a = pcm[j % n], b = pcm[(j + 1) % n];

            x[i] = (int16_t)((a * (65536LL - frac) + (long long)b * frac) >> 16);
        }
    } else {
        count = (n + 27) / 28 * 28;
        x = SDL_calloc((size_t)count, sizeof(int16_t));
        memcpy(x, pcm, (size_t)n * sizeof(int16_t));
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

/* A WAV's samples as 16-bit mono (its first channel), its rate; NULL when it is not PCM. */
static int16_t *ReadWav(const uint8_t *wav, size_t size, int *count, int *rate)
{
    size_t pos = 12;
    int channels = 0, bits = 0;

    if (size < 12 || memcmp(wav, "RIFF", 4) || memcmp(wav + 8, "WAVE", 4)) {
        return NULL;
    }
    while (pos + 8 <= size) {
        uint32_t chunk = (uint32_t)wav[pos + 4] | (uint32_t)wav[pos + 5] << 8 | (uint32_t)wav[pos + 6] << 16 |
                         (uint32_t)wav[pos + 7] << 24;
        const uint8_t *body = wav + pos + 8;

        if (chunk > size - pos - 8) {
            chunk = (uint32_t)(size - pos - 8);
        }
        if (!memcmp(wav + pos, "fmt ", 4) && chunk >= 16) {
            if ((body[0] | body[1] << 8) != 1) {
                return NULL; /* not PCM */
            }
            channels = body[2] | body[3] << 8;
            *rate = body[4] | body[5] << 8 | body[6] << 16 | body[7] << 24;
            bits = body[14] | body[15] << 8;
        } else if (!memcmp(wav + pos, "data", 4) && channels > 0 && (bits == 8 || bits == 16)) {
            int frame = channels * bits / 8, n = (int)(chunk / (uint32_t)frame), i;
            int16_t *out = SDL_malloc((size_t)SDL_max(n, 1) * sizeof(int16_t));

            for (i = 0; i < n; i++) {
                const uint8_t *s = body + (size_t)i * frame;

                out[i] = bits == 16 ? (int16_t)(s[0] | s[1] << 8) : (int16_t)((s[0] - 128) * 256);
            }
            *count = n;
            return out;
        }
        pos += 8 + chunk + (chunk & 1);
    }
    return NULL;
}

unsigned char *PcMakeSamples(size_t *size)
{
    Out wad = {0}, files = {0};
    uint8_t head[2];
    int i;

    head[0] = 39, head[1] = 0;
    Add(&wad, head, 2);
    for (i = 0; i < 39; i++) {
        char path[64];
        size_t wavSize = 0, before = files.size, vagSize;
        uint8_t *wav, entry[25] = {0};
        int16_t *pcm;
        int n = 0, rate = 22050;

        SDL_snprintf(path, sizeof(path), "SOUND\\%s.WAV", sounds[i].wav);
        if (!(wav = PortPcLoadFile(path, &wavSize)) || !(pcm = ReadWav(wav, wavSize, &n, &rate)) || n <= 0) {
            SDL_free(wav);
            SDL_free(wad.data);
            SDL_free(files.data);
            return NULL;
        }
        MakeVag(&files, pcm, n, sounds[i].loops, rate, sounds[i].name);
        SDL_free(pcm);
        SDL_free(wav);
        vagSize = files.size - before;
        SDL_strlcpy((char *)entry, sounds[i].name, 16);
        entry[16] = (uint8_t)vagSize, entry[17] = (uint8_t)(vagSize >> 8), entry[18] = (uint8_t)(vagSize >> 16);
        entry[20] = (uint8_t)vagSize, entry[21] = (uint8_t)(vagSize >> 8), entry[22] = (uint8_t)(vagSize >> 16);
        Add(&wad, entry, 25);
    }
    Add(&wad, files.data, files.size);
    SDL_free(files.data);
    *size = wad.size;
    return wad.data;
}
