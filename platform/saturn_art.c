/* The PlayStation game's images with the Sega Saturn's disc (saturn.c): those of its TIMs the Saturn has art for,
   drawn from the Saturn's race art (art\common\objects.rbh and .vri) into the TIM's size and palette.

   objects.vri holds VDP1 sprites (4-bit, each with a 16-colour table in VDP1 memory) and VDP2 cells (8x8, 8-bit,
   palettes in colour RAM). objects.rbh's blocks: 0 the HUD pieces (u16 count, u16 -1, u32 offsets; a piece: width
   and height in cells, bytes a row, then its pattern names: 12 bits of character, 4 of palette), 1 the race HUD's
   VDP2 map (64 x 64 pattern names), 4 the sprite table (u32 count, u32 offsets; an entry: u16 parts, link, CMDPMOD,
   CMDCOLR, x, y, CMDSRCA, CMDSIZE).

   What the Saturn has, and the PlayStation's it stands for:
   - the speedometer's frame (spbile2.tim): the HUD map's cells 28-41 x 21-28, the same drawing; its bars (thrust,
     energy, revs.tim: the speed, the shield and the thrust) fill the frame's insides, in the PlayStation's colours;
   - the weapon icons (wepicon.cmp): pieces 16-29, the same 14, the PlayStation's first last;
   - the pilot icon (pilot.tim): sprite 44;
   - the explosions (explode.cmp: a fireball of 16 frames, then a flash of 16): sprites 0-15 and 17-32;
   - the ships' shadows (shadow1.tim: one ship's outline in each bit of a pixel, shadow2.tim the fifth): sprites
     52, 50, 53, 54 and 51, the same outlines. */
#include <SDL3/SDL.h>
#include <string.h>

#include "saturn.h"

#define VDP1_RAM 0x25C00000u
#define CELLS 0x25E20000u /* the HUD's VDP2 characters */
#define COLOUR_RAM 0x25F00000u

static int loaded;
static SaturnVideo *video;
static unsigned char *pieces, *hudMap, *sprites;
static size_t piecesSize, hudMapSize, spritesSize;

static unsigned Be16(const unsigned char *p)
{
    return (unsigned)(p[0] << 8 | p[1]);
}

static unsigned Be32(const unsigned char *p)
{
    return (unsigned)p[0] << 24 | (unsigned)p[1] << 16 | (unsigned)p[2] << 8 | p[3];
}

static int Load(void)
{
    if (!loaded) {
        loaded = 1;
        video = SaturnLoadVideo("art\\common\\objects.vri");
        pieces = SaturnBlock("art\\common\\objects.rbh", 0, &piecesSize);
        hudMap = SaturnBlock("art\\common\\objects.rbh", 1, &hudMapSize);
        sprites = SaturnBlock("art\\common\\objects.rbh", 4, &spritesSize);
    }
    return video && pieces && hudMap && sprites;
}

/* An image being drawn: RGBA, alpha 0 see-through, 255 opaque, 128 semi-transparent (the PlayStation's STP). */
typedef struct Canvas {
    int w, h;
    unsigned *rgba;
} Canvas;

static unsigned Rgb(unsigned saturnColour, unsigned alpha)
{
    return (saturnColour & 31) << 3 | ((saturnColour >> 5) & 31) << 11 | ((saturnColour >> 10) & 31) << 19 |
           alpha << 24;
}

static void Plot(Canvas *c, int x, int y, unsigned rgba)
{
    if (x >= 0 && y >= 0 && x < c->w && y < c->h) {
        c->rgba[y * c->w + x] = rgba;
    }
}

/* A VDP2 pattern name's cell at (x, y). */
static void Cell(Canvas *c, unsigned name, int x, int y, unsigned alpha)
{
    const unsigned char *cell = SaturnVideoAt(video, CELLS + (name & 0xfff) * 0x20, 64);
    int i;

    for (i = 0; cell && i < 64; i++) {
        unsigned index = ((name >> 12) & 15) * 256 + cell[i];
        const unsigned char *colour = SaturnVideoAt(video, COLOUR_RAM + index * 2, 2);

        if (cell[i] && colour) {
            Plot(c, x + i % 8, y + i / 8, Rgb(Be16(colour), alpha));
        }
    }
}

static void Piece(Canvas *c, int piece, int x, int y, unsigned alpha)
{
    unsigned at, w, h, stride, cx, cy;

    if (piecesSize < 4 || piece >= (int)Be16(pieces) || 4 + piece * 4 + 4 > (int)piecesSize) {
        return;
    }
    at = Be32(pieces + 4 + piece * 4);
    if (at + 6 > piecesSize) {
        return;
    }
    w = Be16(pieces + at);
    h = Be16(pieces + at + 2);
    stride = Be16(pieces + at + 4);
    for (cy = 0; cy < h; cy++) {
        for (cx = 0; cx < w && at + 6 + cy * stride + cx * 2 + 2 <= piecesSize; cx++) {
            Cell(c, Be16(pieces + at + 6 + cy * stride + cx * 2), x + (int)cx * 8, y + (int)cy * 8, alpha);
        }
    }
}

/* The HUD map's cells (cx, cy), cw x ch of them, at (x, y). */
static void MapCells(Canvas *c, int cx, int cy, int cw, int ch, int x, int y)
{
    int i, k;

    for (k = 0; k < ch; k++) {
        for (i = 0; i < cw; i++) {
            size_t at = ((size_t)(cy + k) * 64 + cx + i) * 2;

            if (at + 2 <= hudMapSize && (Be16(hudMap + at) & 0xfff)) {
                Cell(c, Be16(hudMap + at), x + i * 8, y + k * 8, 255);
            }
        }
    }
}

/* A VDP1 sprite's first part (4-bit with its colour table, or 15-bit) at (x, y). */
static void Sprite(Canvas *c, int sprite, int x, int y, unsigned alpha)
{
    unsigned at, mode, table, source, w, h, i, k;

    if (spritesSize < 8 || sprite >= (int)Be32(sprites) || 4 + sprite * 4 + 4 > (int)spritesSize) {
        return;
    }
    at = Be32(sprites + 4 + sprite * 4);
    if (at + 16 > spritesSize) {
        return;
    }
    mode = (Be16(sprites + at + 4) >> 3) & 7;
    table = Be16(sprites + at + 6) * 8u;
    source = Be16(sprites + at + 12) * 8u;
    w = (Be16(sprites + at + 14) >> 8) * 8;
    h = Be16(sprites + at + 14) & 0xff;
    for (k = 0; k < h; k++) {
        for (i = 0; i < w; i++) {
            const unsigned char *p;

            if (mode == 1) {
                p = SaturnVideoAt(video, VDP1_RAM + source + (k * w + i) / 2, 1);
                if (p) {
                    unsigned index = i & 1 ? *p & 15 : *p >> 4;
                    const unsigned char *colour = SaturnVideoAt(video, VDP1_RAM + table + index * 2, 2);

                    if (index && colour) {
                        Plot(c, x + (int)i, y + (int)k, Rgb(Be16(colour), alpha));
                    }
                }
            } else if (mode == 5) {
                p = SaturnVideoAt(video, VDP1_RAM + source + (k * w + i) * 2, 2);
                if (p && (Be16(p) & 0x8000)) {
                    Plot(c, x + (int)i, y + (int)k, Rgb(Be16(p), alpha));
                }
            }
        }
    }
}

/* ---- the speedometer ---- */

/* The frame as the PlayStation's spbile2.tim has it (the Saturn's is a pixel to the right and down). */
static void SpeedoFrame(Canvas *c)
{
    MapCells(c, 28, 21, 14, 8, -1, -1);
}

/* Where the frame's insides are: what cannot be reached from its edges without crossing it, but for its small
   holes (letters) and the speed's box (inside, above the bars). */
static void SpeedoInside(unsigned char *inside)
{
    enum { W = 112, H = 64 };
    unsigned *frame = SDL_calloc(W * H, sizeof(unsigned));
    unsigned char *seen = SDL_calloc(W * H, 1);
    int *stack = SDL_malloc(W * H * 5 * sizeof(int)), *component = SDL_malloc(W * H * sizeof(int));
    Canvas c = {W, H, frame};
    int top = 0, i;

    SpeedoFrame(&c);
    memset(inside, 0, W * H);
    for (i = 0; i < W * H; i++) {
        int x = i % W, y = i / W;

        if ((x == 0 || y == 0 || x == W - 1 || y == H - 1) && !(frame[i] >> 24)) {
            stack[top++] = i;
        }
    }
    while (top) {
        int p = stack[--top], x = p % W, y = p / W;

        if (seen[p] || (frame[p] >> 24)) {
            continue;
        }
        seen[p] = 1;
        if (x > 0) stack[top++] = p - 1;
        if (x < W - 1) stack[top++] = p + 1;
        if (y > 0) stack[top++] = p - W;
        if (y < H - 1) stack[top++] = p + W;
    }
    for (i = 0; i < W * H; i++) {
        int count = 0, lowest = 0, k;

        if (seen[i] || (frame[i] >> 24)) {
            continue;
        }
        stack[top++] = i;
        while (top) {
            int p = stack[--top], x = p % W, y = p / W;

            if (seen[p] || (frame[p] >> 24)) {
                continue;
            }
            seen[p] = 1;
            component[count++] = p;
            lowest = SDL_max(lowest, y);
            if (x > 0) stack[top++] = p - 1;
            if (x < W - 1) stack[top++] = p + 1;
            if (y > 0) stack[top++] = p - W;
            if (y < H - 1) stack[top++] = p + W;
        }
        if (count >= 8 && lowest >= 20) {
            for (k = 0; k < count; k++) {
                inside[component[k]] = 1;
            }
        }
    }
    SDL_free(component);
    SDL_free(stack);
    SDL_free(seen);
    SDL_free(frame);
}

/* A bar: the frame's insides from (x, y), coloured along its length from stops every 8 pixels. */
static void SpeedoBar(Canvas *c, int x, int y, const unsigned char (*stops)[3], int stopCount, unsigned alpha)
{
    unsigned char *inside = SDL_malloc(112 * 64);
    int i, k;

    SpeedoInside(inside);
    for (k = 0; k < c->h; k++) {
        for (i = 0; i < c->w; i++) {
            int fx = x + i, fy = y + k, s = SDL_min(i / 8, stopCount - 1), t = s + 1 < stopCount ? i % 8 : 0;
            const unsigned char *a = stops[s], *b = stops[SDL_min(s + 1, stopCount - 1)];

            if (fx < 112 && fy < 64 && inside[fy * 112 + fx]) {
                unsigned r = (unsigned)(a[0] * (8 - t) + b[0] * t) / 8, g = (unsigned)(a[1] * (8 - t) + b[1] * t) / 8,
                         bl = (unsigned)(a[2] * (8 - t) + b[2] * t) / 8;

                Plot(c, i, k, r | g << 8 | bl << 16 | alpha << 24);
            }
        }
    }
    SDL_free(inside);
}

static const unsigned char thrustColours[12][3] = {
    {0, 0, 32},     {72, 0, 104},  {152, 0, 136}, {192, 0, 96},   {232, 0, 32},    {248, 0, 0},
    {248, 24, 0},   {248, 96, 0},  {248, 168, 0}, {248, 248, 8},  {248, 248, 96},  {248, 248, 176}};
static const unsigned char energyColours[5][3] = {
    {232, 0, 32}, {200, 0, 96}, {152, 0, 136}, {80, 0, 112}, {56, 0, 96}};
static const unsigned char revsColours[1][3] = {{240, 8, 8}};

/* ---- the PlayStation's TIMs ---- */

/* The Saturn's art for a TIM (of a .cmp, its index) as RGBA: 0 when there is none. */
static int Draw(const char *name, int index, Canvas *c)
{
    if (!strcmp(name, "spbile2.tim")) {
        SpeedoFrame(c);
    } else if (!strcmp(name, "thrust.tim")) {
        SpeedoBar(c, 11, 10, thrustColours, 12, 255);
    } else if (!strcmp(name, "energy.tim")) {
        SpeedoBar(c, 3, 38, energyColours, 5, 128);
    } else if (!strcmp(name, "revs.tim")) {
        SpeedoBar(c, 11, 10, revsColours, 1, 128);
    } else if (!strcmp(name, "pilot.tim")) {
        Sprite(c, 44, 0, 0, 255);
    } else if (!strcmp(name, "wepicon.cmp") && index < 14) {
        Piece(c, index ? 15 + index : 29, 0, 0, 128);
    } else if (!strcmp(name, "explode.cmp") && index < 32) {
        Sprite(c, index < 16 ? index : index + 1, 0, (c->h - 64) / 2, 128);
    } else {
        return 0;
    }
    return 1;
}

/* The ships' outlines, one in each bit of a pixel (shadow1.tim; shadow2.tim's in its first). */
static int Shadows(const char *name, int w, int h, unsigned short *texels)
{
    static const int first[4] = {52, 50, 53, 54}, second[1] = {51};
    const int *outlines = !strcmp(name, "shadow1.tim") ? first : second;
    int count = outlines == first ? 4 : 1, bit, i;
    Canvas c = {w, h, SDL_calloc((size_t)w * h, sizeof(unsigned))};

    for (bit = 0; bit < count; bit++) {
        memset(c.rgba, 0, (size_t)w * h * sizeof(unsigned));
        Sprite(&c, outlines[bit], 0, 0, 255);
        for (i = 0; i < w * h; i++) {
            if (c.rgba[i] >> 24) {
                texels[i] |= (unsigned short)(1 << bit);
            }
        }
    }
    SDL_free(c.rgba);
    return 1;
}

static unsigned short Ps1Colour(unsigned rgba)
{
    unsigned alpha = rgba >> 24, colour = (rgba & 0xff) >> 3 | ((rgba >> 8) & 0xff) >> 3 << 5 |
                                          ((rgba >> 16) & 0xff) >> 3 << 10;

    if (!alpha) {
        return 0;
    }
    if (alpha < 255) {
        return (unsigned short)(colour | 0x8000);
    }
    return (unsigned short)(colour ? colour : 1); /* (0 is see-through) */
}

static int Distance(unsigned short a, unsigned short b)
{
    int dr = (int)(a & 31) - (int)(b & 31), dg = (int)((a >> 5) & 31) - (int)((b >> 5) & 31),
        db = (int)((a >> 10) & 31) - (int)((b >> 10) & 31);

    return dr * dr * 3 + dg * dg * 4 + db * db * 2 + ((a ^ b) & 0x8000 ? 10000 : 0);
}

/* The colours into a palette of `size` (its first see-through): those there are, or as many near the most used ones. */
static void Quantise(const unsigned *rgba, int pixels, int size, unsigned short *clut, unsigned short *texels)
{
    unsigned short *colours = SDL_malloc((size_t)pixels * sizeof(unsigned short));
    int *counts = SDL_calloc((size_t)pixels, sizeof(int)), distinct = 0, used, i, k, round;

    for (i = 0; i < pixels; i++) {
        unsigned short c = Ps1Colour(rgba[i]);

        if (!c) {
            continue;
        }
        for (k = 0; k < distinct && colours[k] != c; k++) {
        }
        if (k == distinct) {
            colours[distinct++] = c;
        }
        counts[k]++;
    }
    /* the most used first */
    for (i = 1; i < distinct; i++) {
        for (k = i; k > 0 && counts[k] > counts[k - 1]; k--) {
            int n = counts[k];
            unsigned short c = colours[k];

            counts[k] = counts[k - 1], colours[k] = colours[k - 1];
            counts[k - 1] = n, colours[k - 1] = c;
        }
    }
    memset(clut, 0, (size_t)size * sizeof(unsigned short));
    used = SDL_min(distinct, size - 1);
    for (i = 0; i < used; i++) {
        clut[1 + i] = colours[i];
    }
    for (round = 0; round < (distinct > used ? 6 : 1); round++) {
        long long sum[256][4];

        memset(sum, 0, sizeof(sum));
        for (i = 0; i < pixels; i++) {
            unsigned short c = Ps1Colour(rgba[i]);
            int best = 0, bestDistance = 0x7fffffff;

            if (c) {
                for (k = 1; k <= used; k++) {
                    int d = Distance(c, clut[k]);

                    if (d < bestDistance) {
                        bestDistance = d, best = k;
                    }
                }
                sum[best][0] += c & 31, sum[best][1] += (c >> 5) & 31, sum[best][2] += (c >> 10) & 31;
                sum[best][3]++;
            }
            texels[i] = (unsigned short)best;
        }
        if (distinct > used) {
            for (k = 1; k <= used; k++) {
                if (sum[k][3]) {
                    unsigned short c = (unsigned short)((clut[k] & 0x8000) | (sum[k][0] / sum[k][3]) |
                                                        (sum[k][1] / sum[k][3]) << 5 | (sum[k][2] / sum[k][3]) << 10);

                    clut[k] = c & 0x7fff ? c : (unsigned short)(c | (c & 0x8000 ? 0 : 1));
                }
            }
        }
    }
    SDL_free(counts);
    SDL_free(colours);
}

int SaturnArtTim(const char *name, int index, int depth, int w, int h, unsigned short *clut, int clutSize,
                 unsigned short *texels)
{
    Canvas c;
    int i;

    if (!name || !Load() || w <= 0 || h <= 0) {
        return 0;
    }
    memset(texels, 0, (size_t)w * h * sizeof(unsigned short));
    if (!strcmp(name, "shadow1.tim") || !strcmp(name, "shadow2.tim")) {
        for (i = 0; i < clutSize; i++) {
            clut[i] = (unsigned short)(i ? 0x8c63 : 0); /* (the game sets its own) */
        }
        return depth == 0 && Shadows(name, w, h, texels);
    }
    c.w = w;
    c.h = h;
    c.rgba = SDL_calloc((size_t)w * h, sizeof(unsigned));
    if (!Draw(name, index, &c)) {
        SDL_free(c.rgba);
        return 0;
    }
    if (depth == 2) {
        for (i = 0; i < w * h; i++) {
            texels[i] = Ps1Colour(c.rgba[i]);
        }
    } else {
        Quantise(c.rgba, w * h, SDL_min(clutSize, 256), clut, texels);
    }
    SDL_free(c.rgba);
    return 1;
}
