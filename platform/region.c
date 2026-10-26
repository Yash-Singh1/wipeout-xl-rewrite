/* The disc's region: Wipeout XL (USA, SCUS-94351, or Japan, SIPS-60010, whose disc holds the same files) or WipEout
   2097 (Europe, SLES-00327), and the video standard the
   game runs to: NTSC's 60 fields a second and 240 lines, or PAL's 50 and 256 (version.h's portPal: the settings', by
   default the disc's). Both discs hold the same data, both releases' pieces among it: the program asks for its own
   (the title screen, the copyright and Red Bull screens, the menus' backdrop and logo, the screen before the game:
   Sony's logo or Europe's piracy warning). The port's game asks for its disc's (WipEout 2097's on the European
   disc); when the video standard is the other one, they are made to fit here (the game's file layer asks here
   first: PortRegionFileInfo): pictures made for 256 lines fitted to 240, or 240 to 256, across the same 4:3 screen
   (15-bit pictures blended between rows, palette ones by the nearest row), and the logo screen put where the
   program shows it. */
#include <SDL3/SDL.h>
#include <string.h>

#include "original.h"
#include "platform.h"
#include "settings.h"
#include "region.h"
#include "psx/disc.h"
#include "psx/psx_io.h"

#define VIRTUAL_LOC 0x50000000 /* the sectors the files are read at: beyond any disc, apart from original.c's */
#define VIRTUAL_SPAN 0x1000

enum { FIT_PICTURE, FIT_MENU, PLACE_LOGO };

/* A release's piece (the end of the path the game asks for), its file, whether it was made for PAL, and how it is
   made to fit the other standard. */
static const struct {
    const char *name, *path;
    int pal, how;
} pieces[] = {
    {"\\rstartxl.tim", "\\WIPEOUT2\\TEXTURES\\RSTARTXL.TIM", 0, FIT_PICTURE},
    {"\\rcopyxl.tim", "\\WIPEOUT2\\TEXTURES\\RCOPYXL.TIM", 0, FIT_PICTURE},
    {"\\redbntsc.tim", "\\WIPEOUT2\\TEXTURES\\REDBNTSC.TIM", 0, FIT_PICTURE},
    {"\\sce_logo.tim", "\\WIPEOUT2\\TEXTURES\\SCE_LOGO.TIM", 0, PLACE_LOGO},
    {"\\menutims.cmp", "\\WIPEOUT2\\TEXTURES\\MENUTIMS.CMP", 0, FIT_MENU},
    {"\\startpal.tim", "\\WIPEOUT2\\TEXTURES\\STARTPAL.TIM", 1, FIT_PICTURE},
    {"\\copy2097.tim", "\\WIPEOUT2\\TEXTURES\\COPY2097.TIM", 1, FIT_PICTURE},
    {"\\redbpal.tim", "\\WIPEOUT2\\TEXTURES\\REDBPAL.TIM", 1, FIT_PICTURE},
    {"\\warning.tim", "\\WIPEOUT2\\TEXTURES\\WARNING.TIM", 1, PLACE_LOGO},
    {"\\menutimp.cmp", "\\WIPEOUT2\\TEXTURES\\MENUTIMP.CMP", 1, FIT_MENU},
};
#define PIECES ((int)SDL_arraysize(pieces))

/* Where each program shows the logo screen from: the US program Sony's logo at (640, 0), the European one its
   warning at (128, 512), which the console's video memory wraps to row 0. */
#define LOGO_X (portPal ? 128 : 640)
#define LOGO_Y 0

static uint8_t *made[PIECES];
static size_t madeSize[PIECES];

int portPal;
int portEuropeDisc;
int portJapanDisc;
int portEuropeCard; /* (off: version.h) */

/* The switches the game's version.h reads, fixed for the run: the disc's region, and the video standard (the
   settings; by default the disc's). */
void PortRegionStart(void)
{
    portEuropeDisc = PortRegionEurope();
    portJapanDisc = PortRegionJapan();
    portPal = portSettings.videoStandard == VIDEO_PAL ||
              (portSettings.videoStandard == VIDEO_BY_DISC && portEuropeDisc);
}

/* Whether the disc holds a program: its name in the disc's root ("\\SLES_003.27"). */
static int Holds(const char *program)
{
    const Disc *disc = PsxIoDisc();
    int32_t lba;
    uint32_t size;

    return disc && DiscFindFile(disc, program, &lba, &size);
}

int PortRegionEurope(void)
{
    static int known = -1;

    if (known < 0 && PsxIoDisc()) {
        known = Holds("\\SLES_003.27");
    }
    return known > 0;
}

int PortRegionJapan(void)
{
    static int known = -1;

    if (known < 0 && PsxIoDisc()) {
        known = Holds("\\SIPS_600.10");
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

/* A TIM fitted to `height` rows (the same TIM when it has as many): 15-bit pixels blended between the two nearest
   source rows, palette indices taken from the nearest. Returns a new TIM (the caller frees), its size in *size. */
static uint8_t *Fit(const uint8_t *tim, size_t length, int height, size_t *size)
{
    size_t image = ImageBlock(tim), head = image + 12, out;
    int w = Le16(tim + image + 8), h = Le16(tim + image + 10), y, x;
    int direct = (tim[4] & 7) == 2;
    uint8_t *fitted;

    if (h == height || head + (size_t)w * h * 2 > length) {
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

/* The menus' textures: the backdrop (240 rows NTSC, 256 PAL) and the logo (30, 32) fitted to the video standard's. */
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

        fitted[i] = Fit(tim, timSize, h == 240 || h == 256 ? (portPal ? 256 : 240) : (x == 640 && y == 190 && (h == 30 || h == 32)) ? (portPal ? 32 : 30) : h, &sizes[i]);
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

/* Whether a piece needs making: made for the other standard, or the logo screen kept elsewhere than the program
   shows it from. */
static int Needed(int i, const uint8_t *data, uint32_t length)
{
    size_t image = ImageBlock(data);

    if (pieces[i].how != PLACE_LOGO) {
        return pieces[i].pal != portPal;
    }
    return image + 8 <= length && (Le16(data + image + 4) != LOGO_X || Le16(data + image + 6) != LOGO_Y);
}

/* A piece made to fit, once (0 when it needs nothing: the game reads the disc's). */
static int Make(int i)
{
    const Disc *disc = PsxIoDisc();
    int32_t lba;
    uint32_t length;
    uint8_t *data;

    if (made[i]) {
        return 1;
    }
    if (pieces[i].how != PLACE_LOGO && pieces[i].pal == portPal) {
        return 0;
    }
    if (!disc || !DiscFindFile(disc, pieces[i].path, &lba, &length)) {
        return 0;
    }
    data = SDL_malloc(((size_t)length + 2047) / 2048 * 2048);
    if (!DiscReadData(disc, lba, (int)((length + 2047) / 2048), data)) {
        SDL_free(data);
        return 0;
    }
    if (!Needed(i, data, length)) {
        SDL_free(data);
        return 0;
    }
    switch (pieces[i].how) {
    case FIT_PICTURE:
        made[i] = Fit(data, length, portPal ? 256 : 240, &madeSize[i]);
        break;
    case FIT_MENU:
        made[i] = FitMenu(data, length, &madeSize[i]);
        break;
    case PLACE_LOGO:
        /* where the program shows it from (the same size and depth: 640 x 256, 15-bit) */
        made[i] = SDL_malloc(length);
        memcpy(made[i], data, length);
        madeSize[i] = length;
        PutLe16(made[i] + ImageBlock(made[i]) + 4, LOGO_X);
        PutLe16(made[i] + ImageBlock(made[i]) + 6, LOGO_Y);
        break;
    }
    SDL_free(data);
    return made[i] != NULL;
}

int PortRegionFileInfo(const char *pathname, int *loc, int *size)
{
    size_t length = strlen(pathname);
    int i;

    for (i = 0; i < PIECES; i++) {
        size_t n = strlen(pieces[i].name);

        if (length >= n && !SDL_strcasecmp(pathname + length - n, pieces[i].name) && Make(i)) {
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
