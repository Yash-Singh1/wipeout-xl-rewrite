/* The disc's region: Wipeout XL (USA, SCUS-94351) or WipEout 2097 (Europe, SLES-00327). The two discs hold the same
   game data, file for file, but for the program and the pieces the program chooses by region: the title screen,
   the copyright and Red Bull screens, the menus' backdrop and logo (WipEout 2097's), the screen before the game
   (Sony's logo; Europe's piracy warning) and the movies (at 25 frames a second). The port's game is the US
   program; with the European disc it is given the European pieces in place of the US ones (the game's file layer
   asks here first: PortRegionFileInfo), so that WipEout 2097 looks as it did. Their pictures were made for the
   European television's 256 lines where the US ones have 240 across the same 4:3 screen: each is fitted to 240
   lines, keeping its shape (15-bit pictures blended between rows, palette ones by the nearest row). */
#include <SDL3/SDL.h>
#include <string.h>

#include "original.h"
#include "platform.h"
#include "region.h"
#include "psx/disc.h"
#include "psx/psx_io.h"

#define VIRTUAL_LOC 0x50000000 /* the sectors the files are read at: beyond any disc, apart from original.c's */
#define VIRTUAL_SPAN 0x1000

enum { FIT_PICTURE, FIT_MENU, MOVE_WARNING };

/* The US program's file (the end of its path), the European one in its place, and how it is made to fit. */
static const struct {
    const char *us, *europe;
    int how;
} pieces[] = {
    {"\\rstartxl.tim", "\\WIPEOUT2\\TEXTURES\\STARTPAL.TIM", FIT_PICTURE},
    {"\\rcopyxl.tim", "\\WIPEOUT2\\TEXTURES\\COPY2097.TIM", FIT_PICTURE},
    {"\\redbntsc.tim", "\\WIPEOUT2\\TEXTURES\\REDBPAL.TIM", FIT_PICTURE},
    {"\\sce_logo.tim", "\\WIPEOUT2\\TEXTURES\\WARNING.TIM", MOVE_WARNING},
    {"\\menutims.cmp", "\\WIPEOUT2\\TEXTURES\\MENUTIMP.CMP", FIT_MENU},
};
#define PIECES ((int)SDL_arraysize(pieces))

static uint8_t *made[PIECES];
static size_t madeSize[PIECES];

int PortRegionEurope(void)
{
    static int known = -1;
    const Disc *disc = PsxIoDisc();
    int32_t lba;
    uint32_t size;

    if (known < 0 && disc) {
        known = DiscFindFile(disc, "\\SLES_003.27", &lba, &size);
    }
    return known > 0;
}

static uint16_t Le16(const uint8_t *p)
{
    return (uint16_t)(p[0] | p[1] << 8);
}

static uint32_t Le32(const uint8_t *p)
{
    return (uint32_t)Le16(p) | (uint32_t)Le16(p + 2) << 16;
}

static void PutLe16(uint8_t *p, int v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

static void PutLe32(uint8_t *p, uint32_t v)
{
    PutLe16(p, (int)(v & 0xFFFF));
    PutLe16(p + 2, (int)(v >> 16));
}

/* A TIM's image block (after its palette's, if any). */
static size_t ImageBlock(const uint8_t *tim)
{
    return 8 + ((tim[4] & 8) ? Le32(tim + 8) : 0);
}

/* A TIM fitted to `height` rows (the same TIM when it has no more): 15-bit pixels blended between the two nearest
   source rows, palette indices taken from the nearest. Returns a new TIM (the caller frees), its size in *size. */
static uint8_t *Fit(const uint8_t *tim, size_t length, int height, size_t *size)
{
    size_t image = ImageBlock(tim), head = image + 12, out;
    int w = Le16(tim + image + 8), h = Le16(tim + image + 10), y, x;
    int direct = (tim[4] & 7) == 2;
    uint8_t *fitted;

    if (h <= height || head + (size_t)w * h * 2 > length) {
        fitted = SDL_malloc(length);
        memcpy(fitted, tim, length);
        *size = length;
        return fitted;
    }
    out = head + (size_t)w * height * 2;
    fitted = SDL_malloc(out);
    memcpy(fitted, tim, head);
    PutLe32(fitted + image, (uint32_t)(12 + w * height * 2));
    PutLe16(fitted + image + 10, height);
    for (y = 0; y < height; y++) {
        double at = (y + 0.5) * h / height - 0.5;
        int top = SDL_clamp((int)SDL_floor(at), 0, h - 1), bottom = SDL_min(top + 1, h - 1);
        double t = SDL_clamp(at - top, 0.0, 1.0);
        const uint8_t *a = tim + head + (size_t)top * w * 2, *b = tim + head + (size_t)bottom * w * 2;
        uint8_t *o = fitted + head + (size_t)y * w * 2;

        if (!direct) {
            memcpy(o, t < 0.5 ? a : b, (size_t)w * 2); /* (palette indices: the nearest row) */
            continue;
        }
        for (x = 0; x < w; x++) {
            uint16_t p = Le16(a + x * 2), q = Le16(b + x * 2), r = 0;
            int c;

            for (c = 0; c < 15; c += 5) {
                double v = ((p >> c) & 31) * (1 - t) + ((q >> c) & 31) * t;
                r |= (uint16_t)((int)(v + 0.5) << c);
            }
            r |= (t < 0.5 ? p : q) & 0x8000;
            if (!r && (p || q)) {
                r = 0x0400; /* (a blend that came out black stays drawn: 0 is transparent) */
            }
            PutLe16(o + x * 2, r);
        }
    }
    *size = out;
    return fitted;
}

/* The menus' textures: the backdrop (256 rows) and WipEout 2097's logo (32) fitted to the US program's 240 and 30. */
static uint8_t *FitMenu(const uint8_t *cmp, size_t length, size_t *size)
{
    int count = (int)Le32(cmp), i;
    size_t header = 4 + (size_t)count * 4, total = 0, at = 0, fittedTotal = 0, packedSize;
    uint8_t *tims, **fitted, *all, *packed, *out;
    size_t *sizes;

    if (count <= 0 || count > 64 || header > length) {
        return NULL;
    }
    for (i = 0; i < count; i++) {
        total += Le32(cmp + 4 + i * 4);
    }
    tims = SDL_malloc(total + 1);
    if (PortLzssDecompress(cmp + header, length - header, tims, total) != total) {
        SDL_free(tims);
        return NULL;
    }
    fitted = SDL_calloc((size_t)count, sizeof(uint8_t *));
    sizes = SDL_calloc((size_t)count, sizeof(size_t));
    for (i = 0; i < count; i++) {
        size_t timSize = Le32(cmp + 4 + i * 4);
        const uint8_t *tim = tims + at;
        size_t image = ImageBlock(tim);
        int h = Le16(tim + image + 10), x = Le16(tim + image + 4), y = Le16(tim + image + 6);

        fitted[i] = Fit(tim, timSize, h == 256 ? 240 : (x == 640 && y == 190 && h == 32) ? 30 : h, &sizes[i]);
        fittedTotal += sizes[i];
        at += timSize;
    }
    all = SDL_malloc(fittedTotal);
    for (i = 0, at = 0; i < count; i++) {
        memcpy(all + at, fitted[i], sizes[i]);
        at += sizes[i];
        SDL_free(fitted[i]);
    }
    packed = PortLzssStore(all, fittedTotal, &packedSize);
    out = SDL_malloc(header + packedSize);
    PutLe32(out, (uint32_t)count);
    for (i = 0; i < count; i++) {
        PutLe32(out + 4 + i * 4, (uint32_t)sizes[i]);
    }
    memcpy(out + header, packed, packedSize);
    *size = header + packedSize;
    SDL_free(packed);
    SDL_free(all);
    SDL_free(sizes);
    SDL_free(fitted);
    SDL_free(tims);
    return out;
}

/* The European piece in place of a US one: read from the disc and made to fit, once. */
static int Make(int i)
{
    const Disc *disc = PsxIoDisc();
    int32_t lba;
    uint32_t length;
    uint8_t *data;

    if (made[i]) {
        return 1;
    }
    if (!disc || !DiscFindFile(disc, pieces[i].europe, &lba, &length)) {
        return 0;
    }
    data = SDL_malloc(((size_t)length + 2047) / 2048 * 2048);
    if (!DiscReadData(disc, lba, (int)((length + 2047) / 2048), data)) {
        SDL_free(data);
        return 0;
    }
    switch (pieces[i].how) {
    case FIT_PICTURE:
        made[i] = Fit(data, length, 240, &madeSize[i]);
        break;
    case FIT_MENU:
        made[i] = FitMenu(data, length, &madeSize[i]);
        break;
    case MOVE_WARNING:
        /* where the US program shows Sony's logo (the European file keeps another place) */
        made[i] = SDL_malloc(length);
        memcpy(made[i], data, length);
        madeSize[i] = length;
        PutLe16(made[i] + ImageBlock(made[i]) + 4, 640);
        PutLe16(made[i] + ImageBlock(made[i]) + 6, 0);
        break;
    }
    SDL_free(data);
    return made[i] != NULL;
}

int PortRegionFileInfo(const char *pathname, int *loc, int *size)
{
    size_t length = strlen(pathname);
    int i;

    if (!PortRegionEurope()) {
        return 0;
    }
    for (i = 0; i < PIECES; i++) {
        size_t n = strlen(pieces[i].us);

        if (length >= n && !SDL_strcasecmp(pathname + length - n, pieces[i].us) && Make(i)) {
            *loc = VIRTUAL_LOC + i * VIRTUAL_SPAN;
            *size = (int)madeSize[i];
            return 1;
        }
    }
    return 0;
}

int PortRegionRead(int loc, int sectors, void *buffer)
{
    int i = (loc - VIRTUAL_LOC) / VIRTUAL_SPAN, sector = (loc - VIRTUAL_LOC) % VIRTUAL_SPAN;
    size_t from = (size_t)sector * 2048, length = (size_t)sectors * 2048;

    if (loc < VIRTUAL_LOC || i >= PIECES || !made[i]) {
        return 0;
    }
    memset(buffer, 0, length);
    if (from < madeSize[i]) {
        memcpy(buffer, made[i] + from, SDL_min(length, madeSize[i] - from));
    }
    return 1;
}
