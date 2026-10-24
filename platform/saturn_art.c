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
   - the menus' panel, text box, arrow, sound icons and logo (menutims.cmp): the front end's sprites (art\super\super.rbh
     block 2 and .vri) 24, 8, 9, 10, 11 and 0 (1 for 2097), the same drawings;
   - the ships' shadows (shadow1.tim: one ship's outline in each bit of a pixel, shadow2.tim the fifth): sprites
     52, 50, 53, 54 and 51, the same outlines. */
#include <SDL3/SDL.h>
#include <string.h>

#include "saturn.h"

#define VDP1_RAM 0x25C00000u
#define CELLS 0x25E20000u /* the HUD's VDP2 characters */
#define COLOUR_RAM 0x25F00000u

static int loaded;
static SaturnVideo *video, *menuVideo;
static unsigned char *pieces, *hudMap, *sprites, *menuSprites;
static size_t piecesSize, hudMapSize, spritesSize, menuSpritesSize;

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
        menuVideo = SaturnLoadVideo("art\\super\\super.vri");
        menuSprites = SaturnBlock("art\\super\\super.rbh", 2, &menuSpritesSize);
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

/* A VDP1 sprite of a table (its parts, each where the entry puts it: 4-bit with a colour table, 8-bit from a bank
   of colour RAM, or 15-bit) at (x, y). */
static void SpriteOf(Canvas *c, SaturnVideo *v, const unsigned char *table, size_t tableSize, int sprite, int x,
                     int y, unsigned alpha)
{
    unsigned at, mode, colours, part, parts;

    unsigned offsets;

    /* the table: where its offsets start (4), the offsets, then the sprites: as many as fit before the first */
    if (!v || !table || tableSize < 8 || sprite < 0 || (offsets = Be32(table)) + 4 > tableSize ||
        offsets + (unsigned)sprite * 4 + 4 > SDL_min(tableSize, (size_t)Be32(table + offsets))) {
        return;
    }
    at = Be32(table + offsets + sprite * 4);
    if (at + 16 > tableSize) {
        return;
    }
    parts = Be16(table + at);
    mode = (Be16(table + at + 4) >> 3) & 7;
    colours = Be16(table + at + 6);
    for (part = 0; part < parts && at + 16 + part * 8 <= tableSize; part++) {
        const unsigned char *p = table + at + 8 + part * 8;
        /* (the first part's place is the sprite's handle: the others' from it) */
        int px = x + (int16_t)Be16(p) - (int16_t)Be16(table + at + 8);
        int py = y + (int16_t)Be16(p + 2) - (int16_t)Be16(table + at + 10);
        unsigned source = Be16(p + 4) * 8u, w = (Be16(p + 6) >> 8) * 8, h = Be16(p + 6) & 0xff, i, k;

        for (k = 0; k < h; k++) {
            for (i = 0; i < w; i++) {
                const unsigned char *texel, *colour = NULL;

                if (mode == 1) {
                    texel = SaturnVideoAt(v, VDP1_RAM + source + (k * w + i) / 2, 1);
                    if (texel && (i & 1 ? *texel & 15 : *texel >> 4)) {
                        colour = SaturnVideoAt(v, VDP1_RAM + colours * 8u + (i & 1 ? *texel & 15 : *texel >> 4) * 2, 2);
                    }
                } else if (mode == 4) {
                    texel = SaturnVideoAt(v, VDP1_RAM + source + k * w + i, 1);
                    if (texel && *texel) {
                        colour = SaturnVideoAt(v, COLOUR_RAM + ((colours & 0xff00) | *texel) * 2, 2);
                    }
                } else if (mode == 5) {
                    texel = SaturnVideoAt(v, VDP1_RAM + source + (k * w + i) * 2, 2);
                    colour = texel && (Be16(texel) & 0x8000) ? texel : NULL;
                }
                if (colour) {
                    Plot(c, px + (int)i, py + (int)k, Rgb(Be16(colour), alpha));
                }
            }
        }
    }
}

/* The race art's sprites, the front end's. */
static void Sprite(Canvas *c, int sprite, int x, int y, unsigned alpha)
{
    SpriteOf(c, video, sprites, spritesSize, sprite, x, y, alpha);
}

static void MenuSprite(Canvas *c, int sprite, int x, int y)
{
    SpriteOf(c, menuVideo, menuSprites, menuSpritesSize, sprite, x, y, 255);
}

/* ---- the font ---- */

/* One of the PlayStation font's glyphs (A-Z, 0-9, ':', '.') as the Saturn's HUD letters draw it, white: its big ones
   (pieces 76-101 the letters, 24 x 16; 0-9 the digits 1-9 and 0, 16 x 16; 30 and 31 ':' and '.', 8 x 16) or its
   small ones (202-227, 16 x 8; 190-199; 200, 201: 8 x 8), into w x h. */
int SaturnFontGlyph(int glyph, int small, unsigned *rgba, int w, int h)
{
    Canvas c = {w, h, rgba};
    int piece;

    if (!Load() || glyph < 0 || glyph > 37) {
        return 0;
    }
    if (glyph < 26) {
        piece = (small ? 202 : 76) + glyph;
    } else if (glyph < 36) {
        piece = small ? 190 + glyph - 26 : glyph == 26 ? 9 : glyph - 27;
    } else {
        piece = (small ? 200 : 30) + glyph - 36;
    }
    memset(rgba, 0, (size_t)w * h * sizeof(unsigned));
    Piece(&c, piece, 0, 0, 255);
    return 1;
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

/* ---- the front end ---- */

/* The menus' panel (menutims.cmp's first): the Saturn's top and bottom, its sides down the open middle. */
static void MenuPanel(Canvas *c)
{
    int x, y, top = 51, bottom = 147;

    MenuSprite(c, 24, 0, 0);
    for (y = top; y < bottom && y < c->h; y++) {
        for (x = 0; x < c->w; x++) {
            unsigned above = c->rgba[(top - 1) * c->w + x];
            int bright = (above & 0xff) + ((above >> 8) & 0xff) + ((above >> 16) & 0xff) > 3 * 128;

            if ((x < 4 || x >= c->w - 4) && (above >> 24) && bright) {
                c->rgba[y * c->w + x] = above;
            }
        }
    }
}

/* A thin green ring (menutims.cmp's eighth). */
static void Ring(Canvas *c)
{
    int x, y;
    float r = c->w / 2.0f - 2;

    for (y = 0; y < c->h; y++) {
        for (x = 0; x < c->w; x++) {
            float d = SDL_fabsf(SDL_sqrtf((x + 0.5f - c->w / 2.0f) * (x + 0.5f - c->w / 2.0f) +
                                          (y + 0.5f - c->h / 2.0f) * (y + 0.5f - c->h / 2.0f)) - r);

            if (d < 1.0f) {
                Plot(c, x, y, 0xff009000u);
            }
        }
    }
}

/* The menus' backdrop (menutims.cmp's sixth, the whole screen): the title's picture, dim, in the menus' blue. */
static void MenuBackdrop(Canvas *c)
{
    unsigned short *picture = SDL_calloc(320 * 224, sizeof(unsigned short));
    int x, y;

    SaturnTitlePicture(picture);
    for (y = 0; y < c->h; y++) {
        for (x = 0; x < c->w; x++) {
            int sy = y - (c->h - 224) / 2, sx = x - (c->w - 320) / 2, light = 0;

            if (sy >= 0 && sy < 224 && sx >= 0 && sx < 320) {
                unsigned p = picture[sy * 320 + sx];

                light = ((p & 31) * 3 + ((p >> 5) & 31) * 6 + ((p >> 10) & 31)) * 8 / 10;
            }
            Plot(c, x, y, (unsigned)(light / 20) | (unsigned)(light / 10) << 8 | (unsigned)(24 + light / 5) << 16 |
                              0xff000000u);
        }
    }
    SDL_free(picture);
}

/* The small font (wofont.tim) the menus print with: 13 glyphs a row of 6 x 6 (A-Z, '.', ',', ':', 0-9, '[', ']',
   '/', '\'', '?', '-', '+', '%', '!'), each 5 x 5, a row a byte (its high 5 bits). */
static const unsigned char smallGlyphs[48][5] = {
    {0x70, 0x88, 0xf8, 0x88, 0x88}, {0xf0, 0x88, 0xf0, 0x88, 0xf0}, {0x78, 0x80, 0x80, 0x80, 0x78},
    {0xf0, 0x88, 0x88, 0x88, 0xf0}, {0xf8, 0x80, 0xf0, 0x80, 0xf8}, {0xf8, 0x80, 0xf0, 0x80, 0x80},
    {0x78, 0x80, 0x98, 0x88, 0x78}, {0x88, 0x88, 0xf8, 0x88, 0x88}, {0x20, 0x20, 0x20, 0x20, 0x20},
    {0x08, 0x08, 0x08, 0x88, 0x70}, {0x88, 0x90, 0xe0, 0x90, 0x88}, {0x80, 0x80, 0x80, 0x80, 0xf8},
    {0xd8, 0xa8, 0xa8, 0x88, 0x88}, {0x88, 0xc8, 0xa8, 0x98, 0x88}, {0x70, 0x88, 0x88, 0x88, 0x70},
    {0xf0, 0x88, 0xf0, 0x80, 0x80}, {0x70, 0x88, 0x88, 0x90, 0x68}, {0xf0, 0x88, 0xf0, 0x90, 0x88},
    {0x78, 0x80, 0x70, 0x08, 0xf0}, {0xf8, 0x20, 0x20, 0x20, 0x20}, {0x88, 0x88, 0x88, 0x88, 0x70},
    {0x88, 0x88, 0x88, 0x50, 0x20}, {0x88, 0x88, 0xa8, 0xa8, 0x50}, {0x88, 0x50, 0x20, 0x50, 0x88},
    {0x88, 0x50, 0x20, 0x20, 0x20}, {0xf8, 0x10, 0x20, 0x40, 0xf8}, {0x00, 0x00, 0x00, 0x00, 0x20},
    {0x00, 0x00, 0x00, 0x20, 0x40}, {0x00, 0x20, 0x00, 0x20, 0x00}, {0x70, 0x98, 0xa8, 0xc8, 0x70},
    {0x20, 0x60, 0x20, 0x20, 0x70}, {0xf0, 0x08, 0x70, 0x80, 0xf8}, {0xf0, 0x08, 0x70, 0x08, 0xf0},
    {0x90, 0x90, 0xf8, 0x10, 0x10}, {0xf8, 0x80, 0xf0, 0x08, 0xf0}, {0x70, 0x80, 0xf0, 0x88, 0x70},
    {0xf8, 0x08, 0x10, 0x20, 0x20}, {0x70, 0x88, 0x70, 0x88, 0x70}, {0x70, 0x88, 0x78, 0x08, 0x70},
    {0x60, 0x40, 0x40, 0x40, 0x60}, {0x30, 0x10, 0x10, 0x10, 0x30}, {0x08, 0x10, 0x20, 0x40, 0x80},
    {0x20, 0x20, 0x00, 0x00, 0x00}, {0x70, 0x08, 0x30, 0x00, 0x20}, {0x00, 0x00, 0x70, 0x00, 0x00},
    {0x00, 0x20, 0x70, 0x20, 0x00}, {0x88, 0x10, 0x20, 0x40, 0x88}, {0x20, 0x20, 0x20, 0x00, 0x20}};

static void SmallFont(Canvas *c)
{
    int glyph, x, y;

    for (glyph = 0; glyph < 48; glyph++) {
        for (y = 0; y < 5; y++) {
            for (x = 0; x < 5; x++) {
                if (smallGlyphs[glyph][y] & (0x80 >> x)) {
                    Plot(c, glyph % 13 * 6 + x, glyph / 13 * 6 + y, 0xffe8e8e8u);
                }
            }
        }
    }
}

/* ---- the tracks' animated boards and smoke ---- */

/* An image scaled into a rectangle of a canvas, keeping its shape, centred (each pixel the average of what it
   covers). */
static void Fit(Canvas *c, const Canvas *image, int x, int y, int w, int h)
{
    float scale = SDL_min((float)w / image->w, (float)h / image->h);
    int ox = x + (int)((w - image->w * scale) / 2), oy = y + (int)((h - image->h * scale) / 2), i, k;

    for (k = 0; k < (int)(image->h * scale); k++) {
        for (i = 0; i < (int)(image->w * scale); i++) {
            int x0 = (int)(i / scale), y0 = (int)(k / scale), x1 = SDL_max(x0 + 1, (int)((i + 1) / scale)),
                y1 = SDL_max(y0 + 1, (int)((k + 1) / scale)), sx, sy, n = 0;
            unsigned r = 0, g = 0, b = 0, a = 0;

            for (sy = y0; sy < y1 && sy < image->h; sy++) {
                for (sx = x0; sx < x1 && sx < image->w; sx++) {
                    unsigned p = image->rgba[sy * image->w + sx];

                    r += p & 0xff, g += (p >> 8) & 0xff, b += (p >> 16) & 0xff, a += p >> 24, n++;
                }
            }
            if (n && a / n >= 128) {
                Plot(c, ox + i, oy + k, r / n | g / n << 8 | b / n << 16 | 0xff000000u);
            }
        }
    }
}

/* A track's animated boards (the PlayStation's *red.cmp, two pictures it alternates): the game's logo on the
   menus' blue. */
static void Board(Canvas *c)
{
    unsigned logo[232 * 31] = {0};
    Canvas image = {232, 31, logo};
    int i;

    for (i = 0; i < c->w * c->h; i++) {
        c->rgba[i] = 0xff401808u;
    }
    MenuSprite(&image, PortSaturnEurope(), 0, 0);
    Fit(c, &image, 2, 2, c->w - 4, c->h - 4);
}

/* Talon's Reach's smoke (smoke.cmp, 25 frames of a column, 32 x 96, risen 4 lines a frame): billows of yellow
   and brown, semi-transparent. */
static float Noise(int x, int y)
{
    unsigned h = (unsigned)x * 374761393u + (unsigned)y * 668265263u;

    h = (h ^ (h >> 13)) * 1274126177u;
    return (float)((h ^ (h >> 16)) & 0xffff) / 65535.0f;
}

static float SmoothNoise(float x, float y)
{
    int ix = (int)SDL_floorf(x), iy = (int)SDL_floorf(y);
    float fx = x - ix, fy = y - iy, a = Noise(ix, iy), b = Noise(ix + 1, iy), c = Noise(ix, iy + 1),
          d = Noise(ix + 1, iy + 1);

    fx = fx * fx * (3 - 2 * fx);
    fy = fy * fy * (3 - 2 * fy);
    return (a + (b - a) * fx) + ((c + (d - c) * fx) - (a + (b - a) * fx)) * fy;
}

static void Smoke(Canvas *c, int frame)
{
    int x, y;

    for (y = 0; y < c->h; y++) {
        for (x = 0; x < c->w; x++) {
            float rise = (float)(y + frame * 4), across = SDL_fabsf(x + 0.5f - c->w / 2.0f) / (c->w / 2.0f);
            float density = SmoothNoise(x / 6.0f, rise / 8.0f) * 0.6f + SmoothNoise(x / 3.0f, rise / 4.0f) * 0.4f;
            float edge = density - across * 0.9f - (y < 12 ? (12 - y) / 12.0f : 0);

            if (edge > 0.05f) {
                float heat = SDL_min(1.0f, edge * 2.5f);
                unsigned r = (unsigned)(96 + 104 * heat), g = (unsigned)(56 + 124 * heat), b = (unsigned)(16 * (1 - heat));

                Plot(c, x, y, r | g << 8 | b << 16 | 128u << 24);
            }
        }
    }
}

/* ---- the pad's buttons ---- */

/* The console pad's button icons (ashbutt.cmp, the Saturn's own being its pad's): round grey buttons with circle,
   cross, square and triangle, left and right arrows (16 x 8), and the A, I, II and B of the other pad the game
   knows (blue and red buttons, black letters). */
static float Line(float x, float y, float x0, float y0, float x1, float y1)
{
    float dx = x1 - x0, dy = y1 - y0, t = ((x - x0) * dx + (y - y0) * dy) / (dx * dx + dy * dy + 1e-6f);

    t = SDL_clamp(t, 0.0f, 1.0f);
    return SDL_sqrtf((x - x0 - dx * t) * (x - x0 - dx * t) + (y - y0 - dy * t) * (y - y0 - dy * t));
}

/* The strokes of a button's symbol, as lines (x0 y0 x1 y1) on its 16 x 16 box. */
static const float buttonStrokes[10][8][4] = {
    {{0}},
    {{5.2f, 5.2f, 10.8f, 10.8f}, {10.8f, 5.2f, 5.2f, 10.8f}},
    {{5, 5, 11, 5}, {11, 5, 11, 11}, {11, 11, 5, 11}, {5, 11, 5, 5}},
    {{8, 4.5f, 11.5f, 10.5f}, {11.5f, 10.5f, 4.5f, 10.5f}, {4.5f, 10.5f, 8, 4.5f}},
    {{0}},
    {{0}},
    {{5.5f, 11.5f, 8, 4.5f}, {8, 4.5f, 10.5f, 11.5f}, {6.4f, 9, 9.6f, 9}},
    {{8, 4.5f, 8, 11.5f}},
    {{6.6f, 4.5f, 6.6f, 11.5f}, {9.4f, 4.5f, 9.4f, 11.5f}},
    {{6, 4.5f, 6, 11.5f}, {6, 4.5f, 9.2f, 4.5f}, {9.2f, 4.5f, 10.2f, 6.2f}, {10.2f, 6.2f, 9.2f, 8},
     {6, 8, 9.6f, 8}, {9.6f, 8, 10.6f, 9.8f}, {10.6f, 9.8f, 9.6f, 11.5f}, {9.6f, 11.5f, 6, 11.5f}}};

static unsigned ButtonAt(int icon, float x, float y)
{
    static const unsigned symbols[4] = {0xff5028e8u, 0xffe8a040u, 0xffd090e8u, 0xff80d840u};
    float r = SDL_sqrtf((x - 8) * (x - 8) + (y - 8) * (y - 8)), d = 1e9f;
    int k;

    if (icon == 4 || icon == 5) {
        float ax = icon == 4 ? 16 - x : x; /* (drawn pointing right, mirrored for the left) */

        return (ax >= 2 && ax <= 9 && y >= 2.5f && y <= 5.5f) || (ax > 9 && ax <= 14 && SDL_fabsf(y - 4) <= (14 - ax) * 0.75f)
                   ? 0xff9a9490u
                   : 0;
    }
    if (r > 7.6f) {
        return 0;
    }
    for (k = 0; k < 8 && (buttonStrokes[icon][k][0] || buttonStrokes[icon][k][2]); k++) {
        d = SDL_min(d, Line(x, y, buttonStrokes[icon][k][0], buttonStrokes[icon][k][1], buttonStrokes[icon][k][2],
                            buttonStrokes[icon][k][3]));
    }
    if (icon == 0) {
        d = SDL_fabsf(r - 3.4f);
    }
    if (d < 0.75f) {
        return icon < 4 ? symbols[icon] : 0xff101010u;
    }
    if (r > 6.6f) {
        return 0xff302c2cu;
    }
    return icon < 4 ? 0xff585050u : icon == 6 || icon == 9 ? 0xffd05028u : 0xff1818c0u;
}

static void Button(Canvas *c, int icon)
{
    int x, y, i, k;

    for (y = 0; y < c->h; y++) {
        for (x = 0; x < c->w; x++) {
            unsigned r = 0, g = 0, b = 0, n = 0;

            for (k = 0; k < 4; k++) {
                for (i = 0; i < 4; i++) {
                    unsigned p = ButtonAt(icon, x + (i + 0.5f) / 4, y + (k + 0.5f) / 4);

                    if (p) {
                        r += p & 0xff, g += (p >> 8) & 0xff, b += (p >> 16) & 0xff, n++;
                    }
                }
            }
            if (n >= 6) {
                Plot(c, x, y, r / n | g / n << 8 | b / n << 16 | 0xff000000u);
            }
        }
    }
}

/* ---- the rest ---- */

/* The arrow over the other player's ship in a linked race (link.tim): pointing down, red, edged yellow. */
static void LinkArrow(Canvas *c)
{
    int x, y;

    for (y = 0; y < c->h; y++) {
        for (x = 0; x < c->w; x++) {
            float fx = x + 0.5f - c->w / 2.0f, fy = y + 0.5f, w = fy < 16 ? 5 : 15 - (fy - 16) * 15 / 14;
            int inside = fy >= 2 && fy <= 30 && SDL_fabsf(fx) <= w;
            int core = fy >= 4 && fy <= 27 && SDL_fabsf(fx) <= w - 2.5f;

            if (inside) {
                Plot(c, x, y, core ? 0xff1818e0u : 0xff10d8f0u);
            }
        }
    }
}

/* The reflections of chrome (chrome.tim): grey clouds. */
static void Chrome(Canvas *c)
{
    int x, y;

    for (y = 0; y < c->h; y++) {
        for (x = 0; x < c->w; x++) {
            float v = SmoothNoise(x / 16.0f, y / 6.0f) * 0.6f + SmoothNoise(x / 6.0f, y / 3.0f) * 0.4f;
            unsigned g = (unsigned)(32 + 200 * v);

            Plot(c, x, y, g | g << 8 | g << 16 | 0xff000000u);
        }
    }
}

/* The memory card's icon (lucky.tim): the pilot. */
static void CardIcon(Canvas *c)
{
    unsigned pilot[32 * 32] = {0};
    Canvas image = {32, 32, pilot};

    Sprite(&image, 44, 0, 0, 255);
    Fit(c, &image, 0, 0, c->w, c->h);
}

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
    } else if (strstr(name, "red.cmp") && strlen(name) <= 12) {
        Board(c);
    } else if (!strcmp(name, "smoke.cmp")) {
        Smoke(c, index);
    } else if (!strcmp(name, "link.tim")) {
        LinkArrow(c);
    } else if (!strcmp(name, "chrome.tim")) {
        Chrome(c);
    } else if (!strcmp(name, "lucky.tim")) {
        CardIcon(c);
    } else if (!strcmp(name, "ashbutt.cmp") && index < 10) {
        Button(c, index);
    } else if (!strcmp(name, "wofont.tim")) {
        SmallFont(c);
    } else if (!strcmp(name, "menutims.cmp") && index < 8) {
        switch (index) {
        case 0:
            MenuPanel(c);
            break;
        case 1:
            MenuSprite(c, 8, 0, 0);
            break;
        case 2:
            MenuSprite(c, 9, (c->w - 16) / 2, (c->h - 18) / 2);
            break;
        case 3:
        case 4:
            MenuSprite(c, 7 + index, 0, 0);
            break;
        case 5:
            MenuBackdrop(c);
            break;
        case 6:
            MenuSprite(c, PortSaturnEurope(), PortSaturnEurope() ? 4 : 0, 0);
            break;
        default:
            Ring(c);
            break;
        }
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
