/* The Saturn's models as the PlayStation's .prm (models) and .cmp (their textures): the common objects (ships, the
   rescue droid, weapons, the start droid and lights, the front end's models) and a track's scenery and sky.

   A model (objects.rbh / objects2.rbh block 3, vertices block 5; super.rbh block 1, vertices block 5; a course's
   block 2, vertices in the largest block loaded with flags 0 after the first) is a header of 32 bytes: +0 its
   polygon groups (an offset in the same block), +4 its vertices (a byte offset in the vertex block), +8 its own
   offset, +12 run-time scratch, +16 the vertex count, +18 the count of billboard vertices (a model with only those
   is turned to the camera, its quads in its x-y plane, its origin the foot), +20 the polygon count. A vertex is 3 x
   32 bits, the PlayStation's / 8 (as are a course's positions). Polygon groups: a count, flags, a size (6 + the
   records), the size / 2 - 1, the VDP1 command (2 a distorted sprite, 4 a polygon), 1, then the records; the next
   group 6 + size on; flags 0xffff end them. Flags: 0x0001 textured, 0x0008 lit at run time (4 more bytes a record),
   0x0200 double-sided, 0x4000 SPD (texel 0 drawn). A record: 4 vertex byte offsets (/ 12), VDP1's corners A B C D
   (a triangle repeats one), then textured CMDSRCA, CMDSIZE, CMDGRDA, CMDCOLR (the address / 8 of a 16-colour LUT:
   every polygon has its own, its lighting in it), untextured CMDGRDA and an RGB555 colour. The texture (4-bit)
   lies on A B C D as A (0,0) B (w-1,0) C (w-1,h-1) D (0,h-1); VDP1's A B C D go round the other way from the
   PlayStation's quad (0 1 3 2), so a quad is A D B C. objects.rbh holds the start light (0, its lamps 1-12), the
   rescue droid (13), mine (14), start droid (15, its screens 16-25), wreck (26), low-detail ships (27-31: AG,
   Auricom, Qirex, Feisar, Piranha), shields (32, 33), class logos (34-37) and the ships (38-42 lit, 43-47 not: AG,
   Auricom, Qirex, Feisar, Piranha); objects2.rbh the same with the silly ships (38 snail, 39 slime, 40 pig, 41 bug,
   42 shark). A course's instance table (at block 2 + block 0's +0x14): a model's offset and its position, until an
   entry whose header is not at its own offset; in the PlayStation's scene order, some objects split.

   Made here: each LUT is a palette (shared by the LUTs of a texture: the brightest, each LUT a per-channel scale of
   it) times a PlayStation colour (128 x the scale); textures of a palette are packed into TIMs (4-bit, at most 256 x
   236) at fixed places in the second half of video memory, beside the track's tiles; quads become FT4 (GT4 for
   scenery), triangles (VDP1 squeezes the texture's collapsed side into the corner) are resampled into an FT3/GT3's
   texture, untextured polygons F3/F4 (G3/G4), billboards a BSPR standing on the origin. The Saturn has no rocket,
   missile or electro bolt models (it draws sprites): plain cones stand in. Its sky is not a model but a VDP2 scroll
   screen (skyNN.rbh: 1-word pattern names of 8-bit 8x8 cells, 4 pages of 64x64 cells side by side, a 2048 x 512
   panorama, its colours in colour RAM): its picture's band, at a quarter of the size, goes round a 16-sided
   cylinder, twice round (the Saturn scrolls it a pixel per unit of heading, 4096 a turn), the horizon 60% down the band. */
#include <SDL3/SDL.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "original.h"
#include "platform.h"
#include "saturn.h"

/* the same arithmetic on every machine: no fused multiply-adds */
#pragma STDC FP_CONTRACT OFF

#define VDP1_RAM 0x25C00000u
#define VDP2_RAM 0x25E00000u
#define COLOUR_RAM 0x25F00000u
#define SCALE 8 /* the Saturn's units: the PlayStation's / 8 */

#define TEXTURED 0x0001
#define DOUBLE_SIDED 0x0200
#define SPD 0x4000

#define SINGLE_SIDED 0x0001 /* the PlayStation's primitive flag */
#define F3 1
#define FT3 2
#define F4 3
#define FT4 4
#define G3 5
#define GT3 6
#define G4 7
#define GT4 8
#define BSPR 11

#define MAX_W 256 /* a TIM's texels across, rows */
#define MAX_H 236

/* ---- reading and writing ---- */

static uint32_t Be32(const uint8_t *p)
{
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}

static uint16_t Be16(const uint8_t *p)
{
    return (uint16_t)(p[0] << 8 | p[1]);
}

typedef struct Buffer {
    uint8_t *data;
    size_t size, capacity;
} Buffer;

static void Put(Buffer *b, const void *data, size_t size)
{
    if (b->size + size > b->capacity) {
        size_t c = SDL_max(b->capacity * 2, b->size + size + 256);
        uint8_t *d = SDL_realloc(b->data, c);

        if (!d) {
            return;
        }
        b->data = d;
        b->capacity = c;
    }
    memcpy(b->data + b->size, data, size);
    b->size += size;
}

static void PutBe16(Buffer *b, int v)
{
    uint8_t p[2] = {(uint8_t)(v >> 8), (uint8_t)v};

    Put(b, p, 2);
}

static void PutBe32(Buffer *b, uint32_t v)
{
    uint8_t p[4] = {(uint8_t)(v >> 24), (uint8_t)(v >> 16), (uint8_t)(v >> 8), (uint8_t)v};

    Put(b, p, 4);
}

static void PutLe16(Buffer *b, int v)
{
    uint8_t p[2] = {(uint8_t)v, (uint8_t)(v >> 8)};

    Put(b, p, 2);
}

static void PutLe32(Buffer *b, uint32_t v)
{
    uint8_t p[4] = {(uint8_t)v, (uint8_t)(v >> 8), (uint8_t)(v >> 16), (uint8_t)(v >> 24)};

    Put(b, p, 4);
}

static int Clamp(int v, int lo, int hi)
{
    return v < lo ? lo : v > hi ? hi : v;
}

/* Round half to even, as the tools that laid this out did. */
static int Round(double v)
{
    return (int)nearbyint(v);
}

/* ---- the Saturn's models ---- */

typedef struct Poly {
    int corner[4];
    int flags;
    uint16_t srca, size, colr; /* (untextured: colr the colour) */
} Poly;

typedef struct Model {
    int (*vertex)[3];
    int vertexCount;
    Poly *poly;
    int polyCount;
    int billboard;
    int position[3];
} Model;

static void FreeModel(Model *m)
{
    SDL_free(m->vertex);
    SDL_free(m->poly);
    memset(m, 0, sizeof(*m));
}

/* The model whose header is at `at` in `blk` (its vertices in `verts`); 0 when it does not fit its blocks. */
static int ReadModel(const uint8_t *blk, size_t blkSize, const uint8_t *verts, size_t vertsSize, uint32_t at,
                     const int position[3], Model *m)
{
    uint32_t groups, first, p;
    int nv, i, k;

    memset(m, 0, sizeof(*m));
    if ((size_t)at + 32 > blkSize) {
        return 0;
    }
    groups = Be32(blk + at);
    first = Be32(blk + at + 4);
    nv = Be16(blk + at + 16) + Be16(blk + at + 18);
    if ((size_t)first + (size_t)nv * 12 > vertsSize) {
        return 0;
    }
    m->vertexCount = nv;
    m->vertex = SDL_calloc((size_t)nv + 1, sizeof(*m->vertex));
    for (i = 0; i < nv; i++) {
        for (k = 0; k < 3; k++) {
            m->vertex[i][k] = SCALE * (int32_t)Be32(verts + first + i * 12 + k * 4);
        }
    }
    m->billboard = Be16(blk + at + 16) == 0 && Be16(blk + at + 18) > 0;
    for (k = 0; k < 3; k++) {
        m->position[k] = SCALE * position[k];
    }
    /* the groups: count them, then read them */
    for (p = groups; (size_t)p + 12 <= blkSize && Be16(blk + p + 2) != 0xffff; p += 6 + Be16(blk + p + 4)) {
        m->polyCount += Be16(blk + p);
    }
    m->poly = SDL_calloc((size_t)m->polyCount + 1, sizeof(Poly));
    m->polyCount = 0;
    for (p = groups; (size_t)p + 12 <= blkSize && Be16(blk + p + 2) != 0xffff; p += 6 + Be16(blk + p + 4)) {
        int count = Be16(blk + p), flags = Be16(blk + p + 2), size = Be16(blk + p + 4);
        int record = count ? (size - 6) / count : 0;

        for (i = 0; i < count && (size_t)p + 12 + (size_t)(i + 1) * (size_t)record <= blkSize; i++) {
            const uint8_t *r = blk + p + 12 + i * record;
            Poly *q = &m->poly[m->polyCount++];

            for (k = 0; k < 4; k++) {
                q->corner[k] = Clamp(Be16(r + k * 2) / 12, 0, nv ? nv - 1 : 0);
            }
            /* a billboard's texel 0 is see-through (the trees: their groups ask for it drawn, black) */
            q->flags = m->billboard ? flags & ~SPD : flags;
            if (flags & TEXTURED) {
                q->srca = Be16(r + 8);
                q->size = Be16(r + 10);
                q->colr = Be16(r + 14);
            } else {
                q->colr = Be16(r + 10);
            }
        }
    }
    return 1;
}

/* A merged model: a's polygons, then b's. */
static void MergeModels(Model *out, const Model *a, const Model *b)
{
    int i, k;

    memset(out, 0, sizeof(*out));
    out->vertexCount = a->vertexCount + b->vertexCount;
    out->vertex = SDL_calloc((size_t)out->vertexCount + 1, sizeof(*out->vertex));
    memcpy(out->vertex, a->vertex, (size_t)a->vertexCount * sizeof(*a->vertex));
    memcpy(out->vertex + a->vertexCount, b->vertex, (size_t)b->vertexCount * sizeof(*b->vertex));
    out->polyCount = a->polyCount + b->polyCount;
    out->poly = SDL_calloc((size_t)out->polyCount + 1, sizeof(Poly));
    memcpy(out->poly, a->poly, (size_t)a->polyCount * sizeof(Poly));
    for (i = 0; i < b->polyCount; i++) {
        out->poly[a->polyCount + i] = b->poly[i];
        for (k = 0; k < 4; k++) {
            out->poly[a->polyCount + i].corner[k] += a->vertexCount;
        }
    }
}

/* A container's models: objects (a table of header offsets, as many as fit before the first one's polygons) or the
   front end's (each header found at its own offset, from 0x10 on, 4-aligned). */
typedef struct Models {
    Model *model;
    int count;
} Models;

static void FreeModels(Models *ms)
{
    int i;

    for (i = 0; i < ms->count; i++) {
        FreeModel(&ms->model[i]);
    }
    SDL_free(ms->model);
    memset(ms, 0, sizeof(*ms));
}

static int ReadModels(const char *path, int modelBlock, int scan, Models *ms)
{
    static const int zero[3] = {0, 0, 0};
    size_t size, vertsSize;
    uint8_t *blk = SaturnBlock(path, modelBlock, &size), *verts = SaturnBlock(path, 5, &vertsSize);
    uint32_t o;
    int n, i;

    memset(ms, 0, sizeof(*ms));
    if (!blk || !verts || size < 8) {
        SDL_free(blk);
        SDL_free(verts);
        return 0;
    }
    if (!scan) {
        uint32_t first = Be32(blk);

        n = first + 4 <= size ? (int)(Be32(blk + first) / 4) : 0;
        ms->model = SDL_calloc((size_t)n + 1, sizeof(Model));
        for (i = 0; i < n && (size_t)i * 4 + 4 <= size; i++) {
            ReadModel(blk, size, verts, vertsSize, Be32(blk + i * 4), zero, &ms->model[ms->count++]);
        }
    } else {
        ms->model = SDL_calloc(size / 32 + 1, sizeof(Model));
        for (o = 0x10; (size_t)o + 32 <= size; o += 4) {
            if (Be32(blk + o + 8) == o && Be32(blk + o) < o &&
                (size_t)Be32(blk + o + 4) + (size_t)(Be16(blk + o + 16) + Be16(blk + o + 18)) * 12 <= vertsSize) {
                ReadModel(blk, size, verts, vertsSize, o, zero, &ms->model[ms->count++]);
            }
        }
    }
    SDL_free(blk);
    SDL_free(verts);
    return ms->count;
}

/* A container's blocks' load flags, from its header (each block: Saturn address, size, flags). */
static int BlockFlags(const char *path, uint32_t *flags, int max)
{
    size_t length;
    uint8_t *file = PortSaturnEntry(path, &length);
    int n = 0;

    if (file && length >= 20 && !memcmp(file, "BIFF", 4)) {
        uint32_t header = Be32(file + 16), i;

        for (i = 0; i < header / 12 && n < max && 20 + i * 12 + 12 <= length; i++) {
            flags[n++] = Be32(file + 20 + i * 12 + 8);
        }
    }
    SDL_free(file);
    return n;
}

/* A course's scenery: its instances' models, in the instance table's order. */
static int ReadCourse(int track, Models *ms)
{
    char path[64];
    uint32_t flags[64], table;
    size_t size, verticesSize = 0, sectionsSize;
    uint8_t *sections, *blk, *vertices = NULL;
    int count, i, zeros = 0;

    memset(ms, 0, sizeof(*ms));
    SDL_snprintf(path, sizeof(path), "art\\track%02d\\course%02d.rbh", track, track);
    count = BlockFlags(path, flags, 64);
    /* the scenery's vertices: the largest block loaded with flags 0 after the first (the track's) */
    for (i = 0; i < count; i++) {
        if (flags[i] == 0 && zeros++ > 0) {
            size_t s;
            uint8_t *d = SaturnBlock(path, i, &s);

            if (d && s > verticesSize) {
                SDL_free(vertices);
                vertices = d;
                verticesSize = s;
            } else {
                SDL_free(d);
            }
        }
    }
    sections = SaturnBlock(path, 0, &sectionsSize);
    blk = SaturnBlock(path, 2, &size);
    if (!sections || !blk || !vertices || sectionsSize < 0x18) {
        SDL_free(sections);
        SDL_free(blk);
        SDL_free(vertices);
        return 0;
    }
    table = Be32(sections + 0x14);
    ms->model = SDL_calloc(size / 16 + 1, sizeof(Model));
    for (; (size_t)table + 16 <= size; table += 16) {
        uint32_t at = Be32(blk + table);
        int position[3];

        if ((size_t)at + 32 > size || Be32(blk + at + 8) != at) {
            break;
        }
        for (i = 0; i < 3; i++) {
            position[i] = (int32_t)Be32(blk + table + 4 + i * 4);
        }
        if (!ReadModel(blk, size, vertices, verticesSize, at, position, &ms->model[ms->count])) {
            break;
        }
        ms->count++;
    }
    SDL_free(sections);
    SDL_free(blk);
    SDL_free(vertices);
    return ms->count;
}

/* A texture (4-bit, w = CMDSIZE's high byte x 8, h its low byte) as indices; the caller frees. */
static uint8_t *Texture(SaturnVideo *v, const Poly *p, int *w, int *h)
{
    int i;
    const uint8_t *raw;
    uint8_t *px;

    *w = (p->size >> 8) * 8;
    *h = p->size & 255;
    raw = SaturnVideoAt(v, VDP1_RAM + (uint32_t)p->srca * 8, (size_t)(*w * *h / 2));
    px = SDL_calloc((size_t)(*w * *h) + 1, 1);
    for (i = 0; raw && i < *w * *h / 2; i++) {
        px[i * 2] = raw[i] >> 4;
        px[i * 2 + 1] = raw[i] & 15;
    }
    return px;
}

static void Lut(SaturnVideo *v, uint16_t colr, uint16_t words[16])
{
    const uint8_t *raw = SaturnVideoAt(v, VDP1_RAM + (uint32_t)colr * 8, 32);
    int k;

    for (k = 0; k < 16; k++) {
        words[k] = raw ? Be16(raw + k * 2) : 0;
    }
}

static void Rgb5(uint16_t w, int rgb[3])
{
    rgb[0] = w & 31;
    rgb[1] = (w >> 5) & 31;
    rgb[2] = (w >> 10) & 31;
}

static int Brightness(const uint16_t words[16])
{
    int k, s = 0;

    for (k = 0; k < 16; k++) {
        s += (words[k] & 31) + ((words[k] >> 5) & 31) + ((words[k] >> 10) & 31);
    }
    return s;
}

/* A Saturn colour as the PlayStation's: 0 is see-through on both; the Saturn's opaque black stays drawn. */
static uint16_t ClutWord(uint16_t c, int transparent)
{
    if (transparent || c == 0) {
        return 0;
    }
    return (c & 0x7fff) ? (uint16_t)(c & 0x7fff) : 0x8000;
}

/* ---- palettes: a LUT as a palette times a per-channel light ---- */

typedef struct Palette {
    uint16_t words[16];
    int see; /* texel 0 see-through */
} Palette;

typedef struct Book {
    Palette *palette;
    int count, capacity;
    int uniform; /* a palette's light the same on the three channels: for models the game lights itself */
} Book;

/* The per-channel scales s that make `base` (16 colours) closest to `lut` (with `uniform`, one scale for all three);
   the largest difference; 0 when a channel has light where the base has none. */
static int Fit(const uint16_t base[16], const uint16_t lut[16], int skip0, int uniform, double s[3], double *err)
{
    int c, k, b[16][3], l[16][3];

    for (k = 0; k < 16; k++) {
        Rgb5(base[k], b[k]);
        Rgb5(lut[k], l[k]);
    }
    for (c = 0; c < 3; c++) {
        long num = 0, den = 0;
        int any = 0;

        for (k = 0; k < 16; k++) {
            any |= l[k][c];
            if (skip0 && k == 0) {
                continue;
            }
            num += (long)b[k][c] * l[k][c];
            den += (long)b[k][c] * b[k][c];
        }
        if (den) {
            s[c] = (double)num / (double)den;
        } else if (!any) {
            s[c] = 0;
        } else {
            *err = 99;
            return 0;
        }
    }
    if (uniform) {
        long num = 0, den = 0;

        for (k = skip0 ? 1 : 0; k < 16; k++) {
            for (c = 0; c < 3; c++) {
                num += (long)b[k][c] * l[k][c];
                den += (long)b[k][c] * b[k][c];
            }
        }
        s[0] = s[1] = s[2] = den ? (double)num / (double)den : 0;
    }
    *err = 0;
    for (k = skip0 ? 1 : 0; k < 16; k++) {
        for (c = 0; c < 3; c++) {
            double v = SDL_min(31.0, b[k][c] * s[c]), d = fabs(v - l[k][c]);

            if (d > *err) {
                *err = d;
            }
        }
    }
    return 1;
}

static int CompareLuts(const void *a, const void *b)
{
    const uint16_t *x = a, *y = b;
    int bx = Brightness(x), by = Brightness(y), k;

    if (bx != by) {
        return by - bx;
    }
    for (k = 0; k < 16; k++) {
        if (x[k] != y[k]) {
            return (int)x[k] - (int)y[k];
        }
    }
    return 0;
}

/* The palette for a texture drawn with these LUTs: one that fits them all (each within 3 of 31), else a new one,
   the brightest of them. */
static int Assign(Book *book, uint16_t (*luts)[16], int count, int see)
{
    int i, j;
    double s[3], err;

    SDL_qsort(luts, (size_t)count, sizeof(*luts), CompareLuts);
    for (i = 0; i < book->count; i++) {
        if (book->palette[i].see != see) {
            continue;
        }
        for (j = 0; j < count; j++) {
            if (!Fit(book->palette[i].words, luts[j], see, book->uniform, s, &err) || err > 3.0) {
                break;
            }
        }
        if (j == count) {
            return i;
        }
    }
    if (book->count == book->capacity) {
        book->capacity = book->capacity * 2 + 16;
        book->palette = SDL_realloc(book->palette, (size_t)book->capacity * sizeof(Palette));
    }
    memcpy(book->palette[book->count].words, luts[0], sizeof(luts[0]));
    book->palette[book->count].see = see;
    return book->count++;
}

/* The PlayStation colour (128: as it is) that turns palette i into a LUT. */
static void Light(const Book *book, int i, const uint16_t lut[16], int see, uint8_t rgb[3])
{
    double s[3], err;
    int c;

    if (!Fit(book->palette[i].words, lut, see, book->uniform, s, &err)) {
        rgb[0] = rgb[1] = rgb[2] = 128;
        return;
    }
    for (c = 0; c < 3; c++) {
        rgb[c] = (uint8_t)Clamp(Round(128 * s[c]), 0, 255);
    }
}

/* ---- video memory: texture pages and palette slots for one file's TIMs ---- */

typedef struct Shelf {
    int x, y, h;
} Shelf;

typedef struct Page {
    int x, y, h, shelves;
    Shelf shelf[256];
} Page;

typedef struct Region {
    Page page[24];
    int pages;
    int clut[1024][2];
    int cluts;
} Region;

static void AddPage(Region *r, int x, int y, int h)
{
    Page *p = &r->page[r->pages++];

    memset(p, 0, sizeof(*p));
    p->x = x;
    p->y = y;
    p->h = h;
}

static void AddCluts(Region *r, int x0, int x1, int y)
{
    int x;

    for (x = x0; x < x1; x += 16) {
        r->clut[r->cluts][0] = x;
        r->clut[r->cluts][1] = y;
        r->cluts++;
    }
}

/* Video memory plan (the second half: rows 512 on; the track's tiles take x 0-183 to row 959 and palettes 1019-1023;
   page rows 512-767 and 768-1003, palettes 1004-1018). */
enum { R_TERRY, R_RESCU, R_WRECK, R_WEAPONS, R_SROID, R_LIGHT, R_SKY, R_SCENE };

static void MakeRegion(Region *r, int which)
{
    int x;

    memset(r, 0, sizeof(*r));
    switch (which) {
    /* the models kept for the whole game (the ships, droids, weapons...) and their palettes all lie in x 192-383:
       the guest tracks' textures leave that alone (original.c: the original's are right of x 640, Wipeout 64's
       pages skip it), so racing one does not spoil them; their palettes below their pages and in rows 1004-1009 */
    case R_TERRY: /* (and the silly ships, in its place) */
        AddPage(r, 192, 512, 256);
        AddPage(r, 192, 768, 220);
        AddPage(r, 256, 512, 256);
        AddCluts(r, 192, 384, 1004);
        AddCluts(r, 192, 384, 1005);
        AddCluts(r, 192, 384, 1006);
        AddCluts(r, 192, 384, 1007);
        for (x = 988; x < 1004; x++) {
            AddCluts(r, 192, 256, x);
        }
        break;
    case R_RESCU:
        AddPage(r, 256, 768, 120);
        AddCluts(r, 192, 288, 1008);
        break;
    case R_WRECK:
        AddPage(r, 256, 888, 100);
        for (x = 988; x < 1004; x++) {
            AddCluts(r, 256, 320, x);
        }
        break;
    case R_WEAPONS:
        AddPage(r, 320, 512, 64);
        AddCluts(r, 288, 384, 1008);
        break;
    case R_SROID:
        AddPage(r, 320, 576, 192);
        AddCluts(r, 192, 384, 1009);
        break;
    case R_LIGHT:
        AddPage(r, 320, 768, 220);
        for (x = 988; x < 1004; x++) {
            AddCluts(r, 320, 384, x);
        }
        break;
    case R_SKY:
        AddPage(r, 384, 512, 256);
        AddPage(r, 384, 768, 236);
        AddCluts(r, 0, 1024, 1010);
        break;
    case R_SCENE:
        for (x = 448; x < 1024; x += 64) {
            AddPage(r, x, 512, 256);
            AddPage(r, x, 768, 236);
        }
        for (x = 1011; x < 1019; x++) {
            AddCluts(r, 0, 1024, x);
        }
        break;
    }
}

/* A place for w x h (words, rows): on a page's shelf, never across a page. */
static int PlaceTim(Region *r, int w, int h, int *x, int *y)
{
    int i, j;

    for (i = 0; i < r->pages; i++) {
        Page *p = &r->page[i];
        int used = 0;

        for (j = 0; j < p->shelves; j++) {
            Shelf *s = &p->shelf[j];

            if (h <= s->h && s->x + w <= 64) {
                *x = p->x + s->x;
                *y = p->y + s->y;
                s->x += w;
                return 1;
            }
            used = s->y + s->h;
        }
        if (used + h <= p->h && w <= 64 && p->shelves < 256) {
            p->shelf[p->shelves].y = used;
            p->shelf[p->shelves].h = h;
            p->shelf[p->shelves].x = w;
            p->shelves++;
            *x = p->x;
            *y = p->y + used;
            return 1;
        }
    }
    *x = *y = 0;
    return 0;
}

/* A palette slot (16 colours), or 16 in a row from a multiple of 256 (256 colours). */
static int PlaceClut(Region *r, int colours, int *x, int *y)
{
    int i, k;

    for (i = 0; i + (colours / 16) <= r->cluts; i++) {
        int cx = r->clut[i][0], cy = r->clut[i][1], ok = colours == 16 || cx % 256 == 0;

        for (k = 1; ok && k < colours / 16; k++) {
            ok = r->clut[i + k][0] == cx + 16 * k && r->clut[i + k][1] == cy;
        }
        if (ok) {
            *x = cx;
            *y = cy;
            memmove(&r->clut[i], &r->clut[i + colours / 16], (size_t)(r->cluts - i - colours / 16) * sizeof(r->clut[0]));
            r->cluts -= colours / 16;
            return 1;
        }
    }
    *x = *y = 0;
    return 0;
}

static void PutTim4(Buffer *b, const uint8_t *px, int w, int h, const uint16_t clut[16], int x, int y, int cx, int cy)
{
    int i;

    PutLe32(b, 0x10);
    PutLe32(b, 8);
    PutLe32(b, 12 + 32);
    PutLe16(b, cx);
    PutLe16(b, cy);
    PutLe16(b, 16);
    PutLe16(b, 1);
    for (i = 0; i < 16; i++) {
        PutLe16(b, clut[i]);
    }
    PutLe32(b, (uint32_t)(12 + w * h / 2));
    PutLe16(b, x);
    PutLe16(b, y);
    PutLe16(b, w / 4);
    PutLe16(b, h);
    for (i = 0; i < w * h; i += 2) {
        uint8_t v = (uint8_t)((px[i] & 15) | (px[i + 1] & 15) << 4);

        Put(b, &v, 1);
    }
}

static void PutTim8(Buffer *b, const uint8_t *px, int w, int h, const uint16_t clut[256], int x, int y, int cx, int cy)
{
    int i;

    PutLe32(b, 0x10);
    PutLe32(b, 9);
    PutLe32(b, 12 + 512);
    PutLe16(b, cx);
    PutLe16(b, cy);
    PutLe16(b, 256);
    PutLe16(b, 1);
    for (i = 0; i < 256; i++) {
        PutLe16(b, clut[i]);
    }
    PutLe32(b, (uint32_t)(12 + w * h));
    PutLe16(b, x);
    PutLe16(b, y);
    PutLe16(b, w / 2);
    PutLe16(b, h);
    Put(b, px, (size_t)(w * h));
}

/* A texture sequence: the count, the TIMs' sizes, the TIMs (packed). */
static uint8_t *MakeCmp(const Buffer *tims, const size_t *sizes, int count, size_t *size)
{
    Buffer out = {0};
    size_t packedSize = 0;
    uint8_t *packed;
    int i;

    PutLe32(&out, (uint32_t)count);
    for (i = 0; i < count; i++) {
        PutLe32(&out, (uint32_t)sizes[i]);
    }
    packed = PortLzssStore(tims->data ? tims->data : (const uint8_t *)"", tims->size, &packedSize);
    Put(&out, packed, packedSize);
    SDL_free(packed);
    *size = out.size;
    return out.data;
}

/* ---- the textures of one file: images of palettes, packed into TIMs ---- */

typedef struct Image {
    int palette, w, h, own; /* own: a TIM of its own (a sprite's: the whole TIM is drawn) */
    uint8_t *px;
    int tim, u, v;          /* where it went */
} Image;

typedef struct Atlas {
    Image *image;
    int count, capacity;
} Atlas;

static int AddImage(Atlas *a, uint8_t *px, int w, int h, int palette, int own)
{
    int i;

    for (i = 0; i < a->count; i++) {
        Image *m = &a->image[i];

        if (m->own == own && m->palette == palette && m->w == w && m->h == h && !memcmp(m->px, px, (size_t)(w * h))) {
            SDL_free(px);
            return i;
        }
    }
    if (a->count == a->capacity) {
        a->capacity = a->capacity * 2 + 64;
        a->image = SDL_realloc(a->image, (size_t)a->capacity * sizeof(Image));
    }
    a->image[a->count].palette = palette;
    a->image[a->count].w = w;
    a->image[a->count].h = h;
    a->image[a->count].own = own;
    a->image[a->count].px = px;
    return a->count++;
}

typedef struct Bin {
    int palette, w, h, first, items; /* items: indices in a list, from first */
    int shelves, y;
    Shelf shelf[MAX_H / 2 + 1];
} Bin;

static int ByHeight(const Atlas *a, int i, int j)
{
    const Image *x = &a->image[i], *y = &a->image[j];

    return x->h != y->h ? y->h - x->h : y->w - x->w;
}

/* Packs the images: a palette's into bins (shelves, at most MAX_W x MAX_H), each image of its own alone; the bins,
   tallest first, become the TIMs, placed in the region. */
static void BuildAtlas(Atlas *a, const Book *book, Region *r, Buffer *tims, size_t **sizes, int *timCount)
{
    int *list = SDL_calloc((size_t)a->count + 1, sizeof(int)), *at = SDL_calloc((size_t)a->count * 2 + 2, sizeof(int));
    Bin *bin = SDL_calloc((size_t)a->count + 1, sizeof(Bin));
    int *order = SDL_calloc((size_t)a->count + 1, sizeof(int));
    int *palettes = SDL_calloc((size_t)a->count + 1, sizeof(int));
    int bins = 0, listed = 0, paletteCount = 0, i, j, k;

    /* images of their own first, in order; then each palette's (in the order they first appear) */
    for (i = 0; i < a->count; i++) {
        if (a->image[i].own) {
            Bin *b = &bin[bins++];

            b->palette = a->image[i].palette;
            b->w = a->image[i].w;
            b->h = a->image[i].h;
            b->first = listed;
            b->items = 1;
            at[i * 2] = at[i * 2 + 1] = 0;
            list[listed++] = i;
        } else {
            for (j = 0; j < paletteCount && palettes[j] != a->image[i].palette; j++) {
            }
            if (j == paletteCount) {
                palettes[paletteCount++] = a->image[i].palette;
            }
        }
    }
    for (j = 0; j < paletteCount; j++) {
        int start = listed, n, cur = -1;

        for (i = 0; i < a->count; i++) {
            if (!a->image[i].own && a->image[i].palette == palettes[j]) {
                list[listed++] = i;
            }
        }
        n = listed - start;
        /* tallest (then widest) first, keeping their order otherwise */
        for (i = 1; i < n; i++) {
            int v = list[start + i];

            for (k = i; k > 0 && ByHeight(a, list[start + k - 1], v) > 0; k--) {
                list[start + k] = list[start + k - 1];
            }
            list[start + k] = v;
        }
        for (i = 0; i < n; i++) {
            int m = list[start + i], w = a->image[m].w, h = a->image[m].h, placed = 0;

            if (cur >= 0) {
                Bin *b = &bin[cur];

                for (k = 0; k < b->shelves && !placed; k++) {
                    if (h <= b->shelf[k].h && b->shelf[k].x + w <= MAX_W) {
                        at[m * 2] = b->shelf[k].x;
                        at[m * 2 + 1] = b->shelf[k].y;
                        b->shelf[k].x += w;
                        placed = 1;
                    }
                }
                if (!placed && b->y + h <= MAX_H && b->shelves < MAX_H / 2 + 1) {
                    b->shelf[b->shelves].x = w;
                    b->shelf[b->shelves].y = b->y;
                    b->shelf[b->shelves].h = h;
                    b->shelves++;
                    at[m * 2] = 0;
                    at[m * 2 + 1] = b->y;
                    b->y += h;
                    placed = 1;
                }
                if (placed) {
                    b->items++;
                }
            }
            if (!placed) {
                Bin *b = &bin[bins];

                cur = bins++;
                b->palette = palettes[j];
                b->first = start + i;
                b->items = 1;
                b->shelves = 1;
                b->shelf[0].x = w;
                b->shelf[0].y = 0;
                b->shelf[0].h = h;
                b->y = h;
                at[m * 2] = at[m * 2 + 1] = 0;
            }
        }
    }
    for (i = 0; i < bins; i++) {
        if (bin[i].shelves) {
            bin[i].w = 0;
            for (k = 0; k < bin[i].shelves; k++) {
                bin[i].w = SDL_max(bin[i].w, bin[i].shelf[k].x);
            }
            bin[i].h = bin[i].y;
        }
        order[i] = i;
    }
    /* tallest first (stable) */
    for (i = 1; i < bins; i++) {
        int v = order[i];

        for (k = i; k > 0 && bin[order[k - 1]].h < bin[v].h; k--) {
            order[k] = order[k - 1];
        }
        order[k] = v;
    }
    *sizes = SDL_calloc((size_t)bins + 1, sizeof(size_t));
    *timCount = bins;
    for (i = 0; i < bins; i++) {
        Bin *b = &bin[order[i]];
        int w4 = (b->w + 3) / 4 * 4, x, y, cx, cy, n;
        uint8_t *canvas = SDL_calloc((size_t)(w4 * b->h) + 2, 1);
        uint16_t clut[16];
        size_t before = tims->size;

        for (n = 0; n < b->items; n++) {
            Image *m = &a->image[list[b->first + n]];
            int mx = at[list[b->first + n] * 2], my = at[list[b->first + n] * 2 + 1];

            for (y = 0; y < m->h; y++) {
                memcpy(canvas + (my + y) * w4 + mx, m->px + y * m->w, (size_t)m->w);
            }
            m->tim = i;
            m->u = mx;
            m->v = my;
        }
        n = PlaceTim(r, w4 / 4, b->h, &x, &y);
        if (!PlaceClut(r, 16, &cx, &cy) || !n) {
            printf("saturn_models.c: no room in video memory for a %d x %d texture\n", w4, b->h);
        }
        for (n = 0; n < 16; n++) {
            const Palette *p = &book->palette[b->palette];

            clut[n] = ClutWord(p->words[n], p->see && n == 0);
        }
        PutTim4(tims, canvas, w4, b->h, clut, x, y, cx, cy);
        (*sizes)[i] = tims->size - before;
        SDL_free(canvas);
    }
    SDL_free(list);
    SDL_free(at);
    SDL_free(bin);
    SDL_free(order);
    SDL_free(palettes);
}

/* ---- converting ---- */

enum { SPEC_TEXTURED, SPEC_FLAT, SPEC_SPRITE };

typedef struct Spec {
    int kind, corners, corner[4], flag, image, gouraud;
    int uv[4][2];
    uint8_t colour[3];
    int width, height; /* a sprite's */
} Spec;

typedef struct Object {
    char name[16];
    int (*vertex)[3];
    int vertexCount;
    int position[3];
    Spec *spec;
    int specCount;
} Object;

/* A texture's palette: the key it was assigned under. */
typedef struct TexturePalette {
    SaturnVideo *video;
    uint16_t srca, size;
    int see, palette;
    uint16_t (*luts)[16];
    int lutCount, lutCapacity, brightest;
} TexturePalette;

typedef struct Converter {
    Book book;
    Atlas atlas;
    Region region;
    int gouraud;
    Object *object;
    int objectCount, objectCapacity;
    TexturePalette *tex;
    int texCount, texCapacity;
} Converter;

static void StartConverter(Converter *cv, int region, int gouraud)
{
    memset(cv, 0, sizeof(*cv));
    MakeRegion(&cv->region, region);
    cv->gouraud = gouraud;
}

static void FreeConverter(Converter *cv)
{
    int i;

    for (i = 0; i < cv->objectCount; i++) {
        SDL_free(cv->object[i].vertex);
        SDL_free(cv->object[i].spec);
    }
    for (i = 0; i < cv->atlas.count; i++) {
        SDL_free(cv->atlas.image[i].px);
    }
    for (i = 0; i < cv->texCount; i++) {
        SDL_free(cv->tex[i].luts);
    }
    SDL_free(cv->object);
    SDL_free(cv->atlas.image);
    SDL_free(cv->book.palette);
    SDL_free(cv->tex);
}

/* Each texture (with its see-through choice) gets one palette fitting all the LUTs it is drawn with; brightest
   textures first. */
static void PrepareModel(Converter *cv, const Model *m, SaturnVideo *v)
{
    int i, j;

    for (i = 0; i < m->polyCount; i++) {
        const Poly *p = &m->poly[i];
        int see = !(p->flags & SPD);
        TexturePalette *t;

        if (!(p->flags & TEXTURED)) {
            continue;
        }
        for (j = 0; j < cv->texCount; j++) {
            t = &cv->tex[j];
            if (t->video == v && t->srca == p->srca && t->size == p->size && t->see == see) {
                break;
            }
        }
        if (j == cv->texCount) {
            if (cv->texCount == cv->texCapacity) {
                cv->texCapacity = cv->texCapacity * 2 + 64;
                cv->tex = SDL_realloc(cv->tex, (size_t)cv->texCapacity * sizeof(TexturePalette));
            }
            t = &cv->tex[cv->texCount++];
            memset(t, 0, sizeof(*t));
            t->video = v;
            t->srca = p->srca;
            t->size = p->size;
            t->see = see;
            t->palette = -1;
        }
        t = &cv->tex[j];
        if (t->lutCount == t->lutCapacity) {
            t->lutCapacity = t->lutCapacity * 2 + 8;
            t->luts = SDL_realloc(t->luts, (size_t)t->lutCapacity * sizeof(*t->luts));
        }
        Lut(v, p->colr, t->luts[t->lutCount]);
        t->brightest = SDL_max(t->brightest, Brightness(t->luts[t->lutCount]));
        t->lutCount++;
    }
}

static void AssignPalettes(Converter *cv)
{
    int *order = SDL_calloc((size_t)cv->texCount + 1, sizeof(int)), i, k;

    if (cv->book.uniform) {
        /* the game lights these models itself, in place of a polygon's colour: a palette per colour scheme (a
           texture can be drawn in several), each the brightest of its LUTs */
        uint16_t (*all)[16] = NULL;
        int *see = NULL, n = 0, j;

        for (i = 0; i < cv->texCount; i++) {
            for (j = 0; j < cv->tex[i].lutCount; j++) {
                all = SDL_realloc(all, (size_t)(n + 1) * sizeof(*all));
                see = SDL_realloc(see, (size_t)(n + 1) * sizeof(*see));
                memcpy(all[n], cv->tex[i].luts[j], sizeof(all[0]));
                see[n++] = cv->tex[i].see;
            }
        }
        for (i = 0; i < n; i++) { /* brightest first (a selection: keeps the pairs together) */
            int best = i;

            for (j = i + 1; j < n; j++) {
                if (CompareLuts(all[j], all[best]) < 0) {
                    best = j;
                }
            }
            if (best != i) {
                uint16_t t[16];
                int st = see[i];

                memcpy(t, all[i], sizeof(t));
                memcpy(all[i], all[best], sizeof(t));
                memcpy(all[best], t, sizeof(t));
                see[i] = see[best];
                see[best] = st;
            }
            Assign(&cv->book, &all[i], 1, see[i]);
        }
        SDL_free(all);
        SDL_free(see);
    }

    for (i = 0; i < cv->texCount; i++) {
        int v = i;

        for (k = i; k > 0 && cv->tex[order[k - 1]].brightest < cv->tex[v].brightest; k--) {
            order[k] = order[k - 1];
        }
        order[k] = v;
    }
    for (i = 0; i < cv->texCount; i++) {
        TexturePalette *t = &cv->tex[order[i]];
        int n = 0, j;

        /* its distinct LUTs */
        for (j = 0; j < t->lutCount; j++) {
            for (k = 0; k < n && memcmp(t->luts[k], t->luts[j], sizeof(t->luts[0])); k++) {
            }
            if (k == n) {
                memmove(t->luts[n++], t->luts[j], sizeof(t->luts[0]));
            }
        }
        if (cv->book.uniform) { /* its brightest scheme's (the others' polygons find theirs: PaletteOf) */
            SDL_qsort(t->luts, (size_t)n, sizeof(*t->luts), CompareLuts);
            n = 1;
        }
        t->palette = Assign(&cv->book, t->luts, n, t->see);
    }
    SDL_free(order);
}

/* A polygon's palette and light. */
static int PaletteOf(Converter *cv, SaturnVideo *v, const Poly *p, uint8_t rgb[3])
{
    int see = !(p->flags & SPD), i;
    uint16_t lut[16];

    Lut(v, p->colr, lut);
    for (i = 0; i < cv->texCount; i++) {
        const TexturePalette *t = &cv->tex[i];

        if (t->video == v && t->srca == p->srca && t->size == p->size && t->see == see && t->palette >= 0) {
            double s[3], err;

            if (cv->book.uniform && (!Fit(cv->book.palette[t->palette].words, lut, see, 1, s, &err) || err > 3.0)) {
                break; /* drawn in another colour scheme: its own palette */
            }
            Light(&cv->book, t->palette, lut, see, rgb);
            return t->palette;
        }
    }
    i = Assign(&cv->book, &lut, 1, see);
    Light(&cv->book, i, lut, see, rgb);
    return i;
}

/* VDP1 lays a texture's (u, v) in [0,1]^2 bilinearly on A B C D; with two corners the same the quad is a triangle.
   For weights on the triangle's three corners (in the Saturn's order), the (u, v) there. */
static void SaturnUv(int collapsed, double wa, double wb, double wc, double *u, double *v)
{
    const double eps = 1e-6;

    if (collapsed == 2) { /* C = D */
        *v = wc;
        *u = *v < 1 - eps ? wb / (1 - *v) : 0.5;
    } else if (collapsed == 3) { /* D = A */
        *u = 1 - wa;
        *v = *u > eps ? wc / *u : 0.5;
    } else if (collapsed == 1) { /* B = C: A B D */
        *u = wb;
        *v = *u < 1 - eps ? wc / (1 - *u) : 0.5;
    } else { /* A = B: A C D */
        *v = 1 - wa;
        *u = *v > eps ? wb / *v : 0.5;
    }
    *u = SDL_min(1.0, SDL_max(0.0, *u));
    *v = SDL_min(1.0, SDL_max(0.0, *v));
}

/* A VDP1 triangle's texture resampled for the PlayStation's affine triangle: UVs P0 (0,0), P1 (0,H-1), P2 (W-1,0)
   at its corners X Z Y (X Y Z in the Saturn's order). */
static uint8_t *TriangleTexture(const uint8_t *tex, int w, int h, int collapsed, int *W, int *H)
{
    int x, y;
    uint8_t *out;

    *W = SDL_max(4, (w + 3) / 4 * 4);
    *H = SDL_max(2, h);
    out = SDL_calloc((size_t)(*W * *H) + 1, 1);
    for (y = 0; y < *H; y++) {
        for (x = 0; x < *W; x++) {
            double b2 = (x + 0.5) / *W, b1 = (y + 0.5) / *H, s = b1 + b2, b0, u, v;

            if (s > 1) {
                b1 = b1 / s;
                b2 = b2 / s;
            }
            b0 = 1 - b1 - b2;
            SaturnUv(collapsed, b0, b2, b1, &u, &v);
            out[y * *W + x] = tex[SDL_min(h - 1, (int)(v * h)) * w + SDL_min(w - 1, (int)(u * w))];
        }
    }
    return out;
}

static Object *NewObject(Converter *cv, const char *name, const Model *m, const int position[3])
{
    Object *o;

    if (cv->objectCount == cv->objectCapacity) {
        cv->objectCapacity = cv->objectCapacity * 2 + 16;
        cv->object = SDL_realloc(cv->object, (size_t)cv->objectCapacity * sizeof(Object));
    }
    o = &cv->object[cv->objectCount++];
    memset(o, 0, sizeof(*o));
    SDL_strlcpy(o->name, name, sizeof(o->name));
    if (m) {
        o->vertexCount = m->vertexCount;
        o->vertex = SDL_calloc((size_t)m->vertexCount + 1, sizeof(*o->vertex));
        memcpy(o->vertex, m->vertex, (size_t)m->vertexCount * sizeof(*o->vertex));
        o->spec = SDL_calloc((size_t)m->polyCount + 1, sizeof(Spec));
    }
    memcpy(o->position, position, sizeof(o->position));
    return o;
}

/* A billboard: one BSPR on its origin, its quads' textures painted into one picture (64 across, rows by its
   shape in steps of 16, so the same picture at other sizes is stored once). */
static void AddBillboard(Converter *cv, const char *name, const Model *m, SaturnVideo *v, const int position[3])
{
    int x0 = m->vertex[0][0], x1 = x0, y0 = m->vertex[0][1], y1 = y0, i, first = -1, W = 64, H;
    uint8_t *img, rgb[3];
    int palette;
    Object *o;
    double aspect;

    for (i = 0; i < m->vertexCount; i++) {
        x0 = SDL_min(x0, m->vertex[i][0]);
        x1 = SDL_max(x1, m->vertex[i][0]);
        y0 = SDL_min(y0, m->vertex[i][1]);
        y1 = SDL_max(y1, m->vertex[i][1]);
    }
    for (i = 0; i < m->polyCount && first < 0; i++) {
        if (m->poly[i].flags & TEXTURED) {
            first = i;
        }
    }
    if (first < 0 || x1 <= x0 || y1 <= y0) {
        return;
    }
    aspect = (double)(y1 - y0) / (double)(x1 - x0);
    H = SDL_max(16, SDL_min(128, (int)Round(64 * aspect / 16) * 16));
    img = SDL_calloc((size_t)(W * H) + 1, 1);
    palette = PaletteOf(cv, v, &m->poly[first], rgb);
    for (i = first; i < m->polyCount; i++) {
        const Poly *p = &m->poly[i];
        int tw, th, n, j, k;
        uint8_t *tex;
        const int *A, *B, *C, *D;

        if (!(p->flags & TEXTURED)) {
            continue;
        }
        tex = Texture(v, p, &tw, &th);
        A = m->vertex[p->corner[0]];
        B = m->vertex[p->corner[1]];
        C = m->vertex[p->corner[2]];
        D = m->vertex[p->corner[3]];
        n = 2 * SDL_max(tw, th);
        for (j = 0; j < n; j++) {
            for (k = 0; k < n; k++) {
                double u = (k + 0.5) / n, vv = (j + 0.5) / n;
                double X = (1 - u) * (1 - vv) * A[0] + u * (1 - vv) * B[0] + u * vv * C[0] + (1 - u) * vv * D[0];
                double Y = (1 - u) * (1 - vv) * A[1] + u * (1 - vv) * B[1] + u * vv * C[1] + (1 - u) * vv * D[1];
                int px = (int)((X - x0) / (x1 - x0) * W), py = (int)((Y - y0) / (y1 - y0) * H);

                if (px >= 0 && px < W && py >= 0 && py < H && tw > 0 && th > 0) {
                    img[py * W + px] = tex[SDL_min(th - 1, (int)(vv * th)) * tw + SDL_min(tw - 1, (int)(u * tw))];
                }
            }
        }
        SDL_free(tex);
    }
    o = NewObject(cv, name, NULL, position);
    o->vertexCount = 1;
    o->vertex = SDL_calloc(2, sizeof(*o->vertex));
    o->vertex[0][0] = (int)((x0 + x1) / 2.0);
    o->vertex[0][1] = y1;
    o->spec = SDL_calloc(1, sizeof(Spec));
    o->specCount = 1;
    o->spec[0].kind = SPEC_SPRITE;
    o->spec[0].image = AddImage(&cv->atlas, img, W, H, palette, 1);
    o->spec[0].width = x1 - x0;
    o->spec[0].height = y1 - y0;
    memcpy(o->spec[0].colour, rgb, 3);
}

/* A model's polygons as an object's primitives; those from `split` on drawn G* (gouraud2 1) or F* (0). */
static void AddModelSplit(Converter *cv, const char *name, const Model *m, SaturnVideo *v, const int position[3],
                          int split, int gouraud2)
{
    Object *o;
    int i, k;

    if (m->billboard) {
        AddBillboard(cv, name, m, v, position);
        return;
    }
    o = NewObject(cv, name, m, position);
    for (i = 0; i < m->polyCount; i++) {
        const Poly *p = &m->poly[i];
        int distinct[4], n = 0, flag = (p->flags & DOUBLE_SIDED) ? 0 : SINGLE_SIDED;
        Spec *s;

        for (k = 0; k < 4; k++) {
            int j;

            for (j = 0; j < n && distinct[j] != p->corner[k]; j++) {
            }
            if (j == n) {
                distinct[n++] = p->corner[k];
            }
        }
        if (n < 3) {
            continue;
        }
        s = &o->spec[o->specCount++];
        s->flag = flag;
        s->gouraud = i >= split ? gouraud2 : cv->gouraud;
        if (p->flags & TEXTURED) {
            int w, h, palette = PaletteOf(cv, v, p, s->colour);
            uint8_t *tex = Texture(v, p, &w, &h);

            s->kind = SPEC_TEXTURED;
            if (n == 4) {
                int w4 = (w + 3) / 4 * 4, y, x;
                uint8_t *px = SDL_calloc((size_t)(w4 * h) + 1, 1);

                for (y = 0; y < h; y++) {
                    for (x = 0; x < w4; x++) {
                        px[y * w4 + x] = tex[y * w + SDL_min(x, w - 1)];
                    }
                }
                s->image = AddImage(&cv->atlas, px, w4, h, palette, 0);
                s->corners = 4;
                s->corner[0] = p->corner[0];
                s->corner[1] = p->corner[3];
                s->corner[2] = p->corner[1];
                s->corner[3] = p->corner[2];
                s->uv[0][0] = 0, s->uv[0][1] = 0;
                s->uv[1][0] = 0, s->uv[1][1] = h - 1;
                s->uv[2][0] = w - 1, s->uv[2][1] = 0;
                s->uv[3][0] = w - 1, s->uv[3][1] = h - 1;
            } else {
                int collapsed = 0, W, H;
                uint8_t *px;

                while (collapsed < 4 && p->corner[collapsed] != p->corner[(collapsed + 1) % 4]) {
                    collapsed++;
                }
                px = TriangleTexture(tex, w, h, collapsed, &W, &H);
                s->image = AddImage(&cv->atlas, px, W, H, palette, 0);
                s->corners = 3;
                s->corner[0] = distinct[0];
                s->corner[1] = distinct[2];
                s->corner[2] = distinct[1];
                s->uv[0][0] = 0, s->uv[0][1] = 0;
                s->uv[1][0] = 0, s->uv[1][1] = H - 1;
                s->uv[2][0] = W - 1, s->uv[2][1] = 0;
            }
            SDL_free(tex);
        } else {
            int rgb[3];

            Rgb5(p->colr, rgb);
            s->kind = SPEC_FLAT;
            for (k = 0; k < 3; k++) {
                s->colour[k] = (uint8_t)SDL_min(255, rgb[k] * 8);
            }
            s->corners = n;
            if (n == 4) {
                s->corner[0] = distinct[0];
                s->corner[1] = distinct[3];
                s->corner[2] = distinct[1];
                s->corner[3] = distinct[2];
            } else {
                s->corner[0] = distinct[0];
                s->corner[1] = distinct[2];
                s->corner[2] = distinct[1];
            }
        }
    }
}

static void AddModel(Converter *cv, const char *name, const Model *m, SaturnVideo *v, const int position[3])
{
    AddModelSplit(cv, name, m, v, position, m->polyCount, cv->gouraud);
}

static void PutColour(Buffer *b, const uint8_t rgb[3])
{
    uint8_t c[4] = {rgb[0], rgb[1], rgb[2], 0};

    Put(b, c, 4);
}

/* An object: Object (64 bytes) and Skeleton (80, identity, at its position), vertices, primitives; big-endian. */
static void PutObject(Buffer *b, const char *name, int (*vertex)[3], int vertexCount, const int position[3],
                      const Buffer *prims, int primCount)
{
    uint8_t head[16] = {0};
    int extent = 0, i, k, t;

    for (i = 0; i < vertexCount; i++) {
        double d = (double)vertex[i][0] * vertex[i][0] + (double)vertex[i][1] * vertex[i][1] +
                   (double)vertex[i][2] * vertex[i][2];

        extent = SDL_max(extent, (int)sqrt(d) + 1);
    }
    SDL_strlcpy((char *)head, name, 16);
    Put(b, head, 16);
    PutBe16(b, vertexCount);
    PutBe16(b, 0);
    PutBe32(b, 0);
    PutBe16(b, 0); /* normals */
    PutBe16(b, 0);
    PutBe32(b, 0);
    PutBe16(b, primCount);
    PutBe16(b, 0);
    PutBe32(b, 0);
    PutBe32(b, 0); /* scratch, BSP tree, skeleton */
    PutBe32(b, 0);
    PutBe32(b, 0);
    PutBe32(b, (uint32_t)extent);
    PutBe16(b, 0); /* flags, index */
    PutBe16(b, 0);
    PutBe32(b, 0); /* next */
    for (t = 0; t < 2; t++) {
        for (i = 0; i < 3; i++) {
            for (k = 0; k < 3; k++) {
                PutBe16(b, i == k ? 4096 : 0);
            }
        }
        PutBe16(b, 0);
        for (i = 0; i < 3; i++) {
            PutBe32(b, (uint32_t)position[i]);
        }
    }
    PutBe16(b, 1); /* update */
    PutBe16(b, 0);
    PutBe32(b, 0);
    PutBe32(b, 0);
    PutBe32(b, 0);
    for (i = 0; i < vertexCount; i++) {
        for (k = 0; k < 3; k++) {
            PutBe16(b, Clamp(vertex[i][k], -32768, 32767));
        }
        PutBe16(b, 0);
    }
    Put(b, prims->data ? prims->data : (const uint8_t *)"", prims->size);
}

/* An untextured triangle or quad: F3/F4, or G3/G4 (each corner the colour). */
static void PutFlat(Buffer *b, const int *corner, int corners, int flag, const uint8_t rgb[3], int gouraud)
{
    int k;

    PutBe16(b, corners == 3 ? (gouraud ? G3 : F3) : (gouraud ? G4 : F4));
    PutBe16(b, flag);
    for (k = 0; k < corners; k++) {
        PutBe16(b, corner[k]);
    }
    if (corners == 3) {
        PutBe16(b, 0);
    }
    for (k = 0; k < (gouraud ? corners : 1); k++) {
        PutColour(b, rgb);
    }
}

/* The objects as a .prm, and their textures (the caller packs them). */
static uint8_t *BuildPrm(Converter *cv, size_t *size, Buffer *tims, size_t **timSizes, int *timCount)
{
    Buffer out = {0};
    int i, j, k;

    BuildAtlas(&cv->atlas, &cv->book, &cv->region, tims, timSizes, timCount);
    for (i = 0; i < cv->objectCount; i++) {
        Object *o = &cv->object[i];
        Buffer prims = {0};

        for (j = 0; j < o->specCount; j++) {
            Spec *s = &o->spec[j];

            if (s->kind == SPEC_TEXTURED) {
                const Image *m = &cv->atlas.image[s->image];

                PutBe16(&prims, s->corners == 3 ? (s->gouraud ? GT3 : FT3) : (s->gouraud ? GT4 : FT4));
                PutBe16(&prims, s->flag);
                for (k = 0; k < s->corners; k++) {
                    PutBe16(&prims, s->corner[k]);
                }
                PutBe16(&prims, m->tim);
                PutBe16(&prims, 0); /* the palette and page: the game's */
                PutBe16(&prims, 0);
                for (k = 0; k < s->corners; k++) {
                    uint8_t uv[2] = {(uint8_t)(s->uv[k][0] + m->u), (uint8_t)(s->uv[k][1] + m->v)};

                    Put(&prims, uv, 2);
                }
                PutBe16(&prims, 0);
                for (k = 0; k < (s->gouraud ? s->corners : 1); k++) {
                    PutColour(&prims, s->colour);
                }
            } else if (s->kind == SPEC_FLAT) {
                PutFlat(&prims, s->corner, s->corners, s->flag, s->colour, s->gouraud);
            } else {
                PutBe16(&prims, BSPR);
                PutBe16(&prims, 0);
                PutBe16(&prims, 0); /* standing on vertex 0 */
                PutBe16(&prims, s->width);
                PutBe16(&prims, s->height);
                PutBe16(&prims, cv->atlas.image[s->image].tim);
                PutColour(&prims, s->colour);
            }
        }
        PutObject(&out, o->name, o->vertex, o->vertexCount, o->position, &prims, o->specCount);
        SDL_free(prims.data);
    }
    *size = out.size;
    return out.data;
}

/* ---- the common objects ---- */

typedef struct Part {
    const char *name;
    int model, merged; /* merged: a second model after it, -1 none */
    int splitGouraud;  /* the second model's polygons as G* (1), F* (0), or as the file's (-1) */
} Part;

/* The models of one file from objects.rbh (or objects2.rbh): its .prm or .cmp. */
static uint8_t *MakeObjects(const char *container, const char *video, int region, int gouraud, const Part *parts,
                            int count, int wantCmp, size_t *size)
{
    static const int zero[3] = {0, 0, 0};
    Models ms;
    SaturnVideo *v = SaturnLoadVideo(video);
    Converter cv;
    Buffer tims = {0};
    size_t *timSizes = NULL, prmSize = 0;
    int timCount = 0, i;
    uint8_t *prm, *out;

    if (!v || !ReadModels(container, 3, 0, &ms)) {
        SDL_free(v);
        return NULL;
    }
    StartConverter(&cv, region, gouraud);
    cv.book.uniform = region == R_TERRY || region == R_RESCU; /* the ships and rescue droid: lit by the game */
    for (i = 0; i < count; i++) {
        if (parts[i].model < ms.count) {
            PrepareModel(&cv, &ms.model[parts[i].model], v);
        }
        if (parts[i].merged >= 0 && parts[i].merged < ms.count) {
            PrepareModel(&cv, &ms.model[parts[i].merged], v);
        }
    }
    AssignPalettes(&cv);
    for (i = 0; i < count; i++) {
        if (parts[i].model >= ms.count) {
            continue;
        }
        if (parts[i].merged >= 0 && parts[i].merged < ms.count) {
            Model m;
            const Model *a = &ms.model[parts[i].model];

            MergeModels(&m, a, &ms.model[parts[i].merged]);
            AddModelSplit(&cv, parts[i].name, &m, v, zero, a->polyCount,
                          parts[i].splitGouraud < 0 ? gouraud : parts[i].splitGouraud);
            FreeModel(&m);
        } else {
            AddModel(&cv, parts[i].name, &ms.model[parts[i].model], v, zero);
        }
    }
    prm = BuildPrm(&cv, &prmSize, &tims, &timSizes, &timCount);
    if (wantCmp) {
        out = MakeCmp(&tims, timSizes, timCount, size);
        SDL_free(prm);
    } else {
        out = prm;
        *size = prmSize;
    }
    SDL_free(tims.data);
    SDL_free(timSizes);
    FreeConverter(&cv);
    FreeModels(&ms);
    SDL_free(v);
    return out;
}

/* A plain untextured double cone along z, for a weapon the Saturn draws as a sprite. */
static uint8_t *StandIn(const char *name, int length, int radius, int r, int g, int b, int sides, size_t *size)
{
    static const int zero[3] = {0, 0, 0};
    int vertex[8][3] = {{0, 0, length}, {0, 0, -length}}, k;
    uint8_t rgb[3] = {(uint8_t)r, (uint8_t)g, (uint8_t)b};
    Buffer prims = {0}, out = {0};

    for (k = 0; k < sides; k++) {
        double a = 2 * M_PI * k / sides;

        vertex[2 + k][0] = (int)(radius * cos(a));
        vertex[2 + k][1] = (int)(radius * sin(a));
        vertex[2 + k][2] = 0;
    }
    for (k = 0; k < sides; k++) {
        int up[3] = {0, 2 + k, 2 + (k + 1) % sides}, down[3] = {1, 2 + (k + 1) % sides, 2 + k};

        PutFlat(&prims, up, 3, SINGLE_SIDED, rgb, 1);
        PutFlat(&prims, down, 3, SINGLE_SIDED, rgb, 1);
    }
    PutObject(&out, name, vertex, 2 + sides, zero, &prims, 2 * sides);
    SDL_free(prims.data);
    *size = out.size;
    return out.data;
}

/* The front end's models (super.rbh: block 1, found by their own offsets, in block order). */
enum {
    SUPER_QUESTION = 1, SUPER_ARCADE, SUPER_TIME_TRIAL, SUPER_PIRANHA, SUPER_PHANTOM_CLASS, SUPER_VECTOR, SUPER_VENOM,
    SUPER_RAPIER, SUPER_PHANTOM, SUPER_ALPHA = 24, SUPER_SAGARMATHA, SUPER_VALPARAISO, SUPER_UPSILON, SUPER_POLTRACK,
    SUPER_ODESSA, SUPER_VOSTOK, SUPER_SPILSKINANKE
};

static const Part harryParts[] = {
    {"ven", SUPER_VENOM, -1, -1}, {"vect", SUPER_VECTOR, -1, -1}, {"timetrial1", SUPER_TIME_TRIAL, -1, -1},
    {"rap", SUPER_RAPIER, -1, -1}, {"pirhana", SUPER_PIRANHA, -1, -1}, {"phant", SUPER_PHANTOM, -1, -1},
    {"phantm", SUPER_PHANTOM_CLASS, -1, -1}, {"question", SUPER_QUESTION, -1, -1}, {"arcade2", SUPER_ARCADE, -1, -1},
    {"arcade", SUPER_ARCADE, -1, -1},
    {"3d2pllink", SUPER_QUESTION, -1, -1}, /* (no two-player link on the Saturn: the question mark) */
};
static const Part juneParts[] = {
    {"vostok", SUPER_VOSTOK, -1, -1}, {"valparisso", SUPER_VALPARAISO, -1, -1}, {"upsildev5", SUPER_UPSILON, -1, -1},
    {"spilskinanke", SUPER_SPILSKINANKE, -1, -1}, {"odessakeys", SUPER_ODESSA, -1, -1},
    {"alphadev3", SUPER_ALPHA, -1, -1}, {"poltrack", SUPER_POLTRACK, -1, -1}, {"sagamathra", SUPER_SAGARMATHA, -1, -1},
};

/* harry.prm, june.prm, or a class logo (vecto, venom, rapie, phant: the front end's at half size); untextured. */
static uint8_t *MakeMenu(const Part *parts, int count, int half, size_t *size)
{
    static const int zero[3] = {0, 0, 0};
    Models ms;
    SaturnVideo *v = SaturnLoadVideo("art\\super\\super.vri");
    Converter cv;
    Buffer tims = {0};
    size_t *timSizes = NULL;
    int timCount = 0, i, k;
    uint8_t *prm;

    if (!v || !ReadModels("art\\super\\super.rbh", 1, 1, &ms)) {
        SDL_free(v);
        return NULL;
    }
    StartConverter(&cv, R_WEAPONS, 0);
    for (i = 0; i < count; i++) {
        if (parts[i].model < ms.count) {
            Model *m = &ms.model[parts[i].model];

            if (half) {
                for (k = 0; k < m->vertexCount * 3; k++) {
                    m->vertex[k / 3][k % 3] /= 2;
                }
            }
            AddModel(&cv, parts[i].name, m, v, zero);
        }
    }
    prm = BuildPrm(&cv, size, &tims, &timSizes, &timCount);
    SDL_free(tims.data);
    SDL_free(timSizes);
    FreeConverter(&cv);
    FreeModels(&ms);
    SDL_free(v);
    return prm;
}

unsigned char *SaturnMakeModel(const char *name, size_t *size)
{
    static const Part terry[] = {{"quirex1", 40, -1, -1}, {"fiesar1", 41, -1, -1}, {"auricom2", 39, -1, -1},
                                 {"ag1", 38, -1, -1}, {"piranha2", 42, -1, -1}};
    static const Part wierd[] = {{"snail", 38, -1, -1}, {"slime", 39, -1, -1}, {"shark", 42, -1, -1},
                                 {"pigg", 40, -1, -1}, {"bug", 41, -1, -1}};
    /* the low-detail ships by team: AG 27, Auricom 28, Qirex 29, Feisar 30, Piranha 31 */
    static const Part telco[] = {
        {"agcol1", 27, -1, -1}, {"quirexcol2", 29, -1, -1}, {"quirexcol1", 29, -1, -1}, {"quirexcol", 29, -1, -1},
        {"agcol", 27, -1, -1}, {"pirancol2", 31, -1, -1}, {"fiescol2", 30, -1, -1}, {"fiescol1", 30, -1, -1},
        {"fiescol", 30, -1, -1}, {"auricol2", 28, -1, -1}, {"auricol1", 28, -1, -1}, {"auricol", 28, -1, -1},
        {"agcol2", 27, -1, -1}, {"pirancol1", 31, -1, -1}, {"pirancol", 31, -1, -1}};
    static const Part rescu[] = {{"grid2", 13, -1, -1}};
    static const Part wreck[] = {{"hull", 26, -1, -1}};
    static const Part mine[] = {{"top", 14, -1, -1}};
    static const Part shield[] = {{"shield", 32, -1, -1}};
    /* the droid and its screen, the screen first: the game lays cdroid.cmp's pictures on an object's first polygon */
    static const Part sroid[] = {{"screen", 16, 15, -1}, {"screen", 16, 15, -1}};
    /* the game tints light1's first FT4s (up to 8, stopping at another kind): the lamp first, the bar as GT4 */
    static const Part light[] = {{"light1", 1, 0, 1}};
    static const struct {
        const char *file;
        const char *container;
        int region, gouraud, cmp, count;
        const Part *parts;
    } files[] = {
        {"terry.prm", "objects", R_TERRY, 0, 0, 5, terry}, {"terry.cmp", "objects", R_TERRY, 0, 1, 5, terry},
        {"wierd.prm", "objects2", R_TERRY, 0, 0, 5, wierd}, {"wierd.cmp", "objects2", R_TERRY, 0, 1, 5, wierd},
        {"telco.prm", "objects", R_TERRY, 0, 0, 15, telco},
        {"rescu.prm", "objects", R_RESCU, 1, 0, 1, rescu}, {"rescu.cmp", "objects", R_RESCU, 1, 1, 1, rescu},
        {"wreck.prm", "objects", R_WRECK, 0, 0, 1, wreck}, {"wreck.cmp", "objects", R_WRECK, 0, 1, 1, wreck},
        {"mine.prm", "objects", R_WEAPONS, 0, 0, 1, mine}, {"mine.cmp", "objects", R_WEAPONS, 0, 1, 1, mine},
        {"shld.prm", "objects", R_WEAPONS, 1, 0, 1, shield},
        {"sroid.prm", "objects", R_SROID, 0, 0, 2, sroid}, {"sroid.cmp", "objects", R_SROID, 0, 1, 2, sroid},
        {"light.prm", "objects", R_LIGHT, 0, 0, 1, light}, {"light.cmp", "objects", R_LIGHT, 0, 1, 1, light},
    };
    static const struct {
        const char *file;
        int model;
        const char *name;
    } logos[] = {{"vecto.prm", SUPER_VECTOR, "vect"}, {"venom.prm", SUPER_VENOM, "ven"},
                 {"rapie.prm", SUPER_RAPIER, "rap"}, {"phant.prm", SUPER_PHANTOM, "phant"}};
    size_t i;

    *size = 0;
    for (i = 0; i < SDL_arraysize(files); i++) {
        if (!SDL_strcasecmp(name, files[i].file)) {
            char rbh[64], vri[64];

            SDL_snprintf(rbh, sizeof(rbh), "art\\common\\%s.rbh", files[i].container);
            SDL_snprintf(vri, sizeof(vri), "art\\common\\%s.vri", files[i].container);
            return MakeObjects(rbh, vri, files[i].region, files[i].gouraud, files[i].parts, files[i].count,
                               files[i].cmp, size);
        }
    }
    if (!SDL_strcasecmp(name, "rock.prm")) {
        return StandIn("missile", 120, 18, 200, 200, 220, 4, size);
    }
    if (!SDL_strcasecmp(name, "miss.prm")) {
        return StandIn("cube1", 106, 32, 220, 120, 40, 4, size);
    }
    if (!SDL_strcasecmp(name, "ebolt.prm")) {
        return StandIn("sphere1", 33, 33, 80, 160, 255, 6, size);
    }
    if (!SDL_strcasecmp(name, "harry.prm")) {
        return MakeMenu(harryParts, (int)SDL_arraysize(harryParts), 0, size);
    }
    if (!SDL_strcasecmp(name, "june.prm")) {
        return MakeMenu(juneParts, (int)SDL_arraysize(juneParts), 0, size);
    }
    for (i = 0; i < SDL_arraysize(logos); i++) {
        if (!SDL_strcasecmp(name, logos[i].file)) {
            Part p = {logos[i].name, logos[i].model, -1, -1};

            return MakeMenu(&p, 1, 1, size);
        }
    }
    return NULL;
}

/* ---- a track's scenery and sky ---- */

static const char *SceneName(int track, int index, char *buffer, size_t size)
{
    const SaturnSceneNames *t;

    for (t = saturnSceneNames; t->names; t++) {
        if (t->track == track && index < t->count) {
            return t->names + t->index[index];
        }
    }
    SDL_snprintf(buffer, size, "obj%03d", index);
    return buffer;
}

static uint8_t *MakeScene(int track, int wantCmp, size_t *size)
{
    char path[64], buffer[16];
    Models ms;
    SaturnVideo *v;
    Converter cv;
    Buffer tims = {0};
    size_t *timSizes = NULL, prmSize = 0;
    int timCount = 0, i;
    uint8_t *prm, *out;

    SDL_snprintf(path, sizeof(path), "art\\track%02d\\course%02d.vri", track, track);
    v = SaturnLoadVideo(path);
    if (!v || !ReadCourse(track, &ms)) {
        SDL_free(v);
        return NULL;
    }
    StartConverter(&cv, R_SCENE, 1);
    for (i = 0; i < ms.count; i++) {
        PrepareModel(&cv, &ms.model[i], v);
    }
    AssignPalettes(&cv);
    for (i = 0; i < ms.count; i++) {
        const char *name = SceneName(track, i, buffer, sizeof(buffer));

        if (!strncmp(name, "redb", 4)) {
            /* the PlayStation's Red Bull boards: the game lays its two pictures on their first four quads as a 2 x 2
               grid (RedBullAnimate), which the Saturn's (a row of four, its own advert) are not. Flat-shaded, the
               game leaves them, and the Saturn's advert shows. */
            AddModelSplit(&cv, name, &ms.model[i], v, ms.model[i].position, 0, 0);
        } else {
            AddModel(&cv, name, &ms.model[i], v, ms.model[i].position);
        }
    }
    prm = BuildPrm(&cv, &prmSize, &tims, &timSizes, &timCount);
    if (wantCmp) {
        out = MakeCmp(&tims, timSizes, timCount, size);
        SDL_free(prm);
    } else {
        out = prm;
        *size = prmSize;
    }
    SDL_free(tims.data);
    SDL_free(timSizes);
    FreeConverter(&cv);
    FreeModels(&ms);
    SDL_free(v);
    return out;
}

#define SKY_W 2048
#define SKY_H 512
#define SKY_SHRINK 4
#define SKY_MAX_ROWS 118 /* two TIMs a page (the second page row has 236) */
#define SKY_SEGMENTS 16
#define SKY_RADIUS 12000
#define SKY_TIM_W 128

/* The sky's panorama: 4 pages of 64x64 1-word pattern names (12 bits of character number, in 0x20-byte units from
   the cells at VDP2 0x25e40000; 8-bit 8x8 cells) side by side. */
static uint8_t *SkyPanorama(int track, uint16_t cram[256])
{
    char path[64];
    size_t mapSize;
    uint8_t *map, *img;
    SaturnVideo *v;
    const uint8_t *colours;
    size_t i;
    int k;

    SDL_snprintf(path, sizeof(path), "art\\sky\\sky%02d.rbh", track);
    map = SaturnBlock(path, 0, &mapSize);
    SDL_snprintf(path, sizeof(path), "art\\sky\\sky%02d.vri", track);
    v = SaturnLoadVideo(path);
    if (!map || !v) {
        SDL_free(map);
        SDL_free(v);
        return NULL;
    }
    colours = SaturnVideoAt(v, COLOUR_RAM, 512);
    for (k = 0; k < 256; k++) {
        cram[k] = colours ? Be16(colours + k * 2) : 0;
    }
    img = SDL_calloc(SKY_W * SKY_H, 1);
    for (i = 0; i < mapSize / 2 && i < 4 * 64 * 64; i++) {
        int page = (int)i / 4096, j = (int)i % 4096, cx = page * 64 + j % 64, cy = j / 64, y;
        const uint8_t *cell = SaturnVideoAt(v, VDP2_RAM + 0x40000 + (uint32_t)(Be16(map + i * 2) & 0xfff) * 0x20, 64);

        for (y = 0; cell && y < 8; y++) {
            memcpy(img + (cy * 8 + y) * SKY_W + cx * 8, cell + y * 8, 8);
        }
    }
    SDL_free(map);
    SDL_free(v);
    return img;
}

/* The picture's rows: the longest run of rows of more than one colour (gaps under 16 rows bridged). */
static void SkyBand(const uint8_t *img, int *top, int *bottom)
{
    int y, x, start = -1, last = -1, bestA = 0, bestB = -1;

    for (y = 0; y < SKY_H; y++) {
        const uint8_t *row = img + y * SKY_W;

        for (x = 1; x < SKY_W && row[x] == row[0]; x++) {
        }
        if (x == SKY_W) {
            continue;
        }
        if (start < 0 || y - last >= 16) {
            if (start >= 0 && last - start > bestB - bestA) {
                bestA = start;
                bestB = last;
            }
            start = y;
        }
        last = y;
    }
    if (start >= 0 && last - start > bestB - bestA) {
        bestA = start;
        bestB = last;
    }
    if (bestB < 0) {
        *top = 0;
        *bottom = SKY_H;
    } else {
        *top = bestA;
        *bottom = bestB + 1;
    }
}

static uint8_t *MakeSky(int track, int wantCmp, size_t *size)
{
    static const int zero[3] = {0, 0, 0};
    uint16_t cram[256], clut[256];
    uint8_t *img = SkyPanorama(track, cram), *small, *px;
    int top, bottom, W = SKY_W / SKY_SHRINK, Hb, x, y, t, k, timCount = SKY_W / SKY_SHRINK / SKY_TIM_W;
    /* the Saturn scrolls its sky a pixel per unit of the camera's heading (4096 a turn): the picture goes round twice */
    double vstep, pxPerDeg = 2 * SKY_W / 360.0, horizon, eTop, eBottom;
    int vertex[4 * SKY_SEGMENTS + 2][3];
    Region r;
    Buffer tims = {0}, prims = {0}, out = {0};
    size_t timSizes[8];
    int topColour[3] = {0, 0, 0}, bottomColour[3] = {0, 0, 0};
    uint8_t topRgb[3], bottomRgb[3], grey[3] = {128, 128, 128};

    if (!img) {
        return NULL;
    }
    SkyBand(img, &top, &bottom);
    Hb = SDL_max(2, SDL_min(SKY_MAX_ROWS, (bottom - top) / SKY_SHRINK));
    vstep = (double)(bottom - top) / Hb;
    small = SDL_calloc((size_t)(W * Hb), 1);
    for (y = 0; y < Hb; y++) {
        int y0 = top + (int)(y * vstep), y1 = SDL_max(y0 + 1, top + (int)((y + 1) * vstep));

        for (x = 0; x < W; x++) {
            /* the average of the pixels it stands for, as the nearest of the colours (one in four, the mountains
               were noise) */
            int sum[3] = {0, 0, 0}, n = 0, best = 0, bestDistance = 1 << 30, i, j, c;

            for (j = y0; j < y1; j++) {
                for (i = x * SKY_SHRINK; i < (x + 1) * SKY_SHRINK; i++) {
                    int rgb[3];

                    Rgb5(cram[img[j * SKY_W + i]], rgb);
                    for (c = 0; c < 3; c++) {
                        sum[c] += rgb[c];
                    }
                    n++;
                }
            }
            for (k = 0; k < 256; k++) {
                int rgb[3], d = 0;

                Rgb5(cram[k], rgb);
                for (c = 0; c < 3; c++) {
                    int e = rgb[c] * n - sum[c];

                    d += e * e;
                }
                if (d < bestDistance) {
                    bestDistance = d;
                    best = k;
                }
            }
            small[y * W + x] = (uint8_t)best;
        }
    }
    for (k = 0; k < 256; k++) {
        clut[k] = ClutWord(cram[k], 0);
    }
    MakeRegion(&r, R_SKY);
    px = SDL_calloc((size_t)(SKY_TIM_W * Hb), 1);
    for (t = 0; t < timCount; t++) {
        int tx, ty, cx, cy;
        size_t before = tims.size;

        for (y = 0; y < Hb; y++) {
            memcpy(px + y * SKY_TIM_W, small + y * W + t * SKY_TIM_W, SKY_TIM_W);
        }
        PlaceTim(&r, SKY_TIM_W / 2, Hb, &tx, &ty);
        PlaceClut(&r, 256, &cx, &cy);
        PutTim8(&tims, px, SKY_TIM_W, Hb, clut, tx, ty, cx, cy);
        timSizes[t] = tims.size - before;
    }
    SDL_free(px);
    SDL_free(small);
    if (wantCmp) {
        uint8_t *cmp = MakeCmp(&tims, timSizes, timCount, size);

        SDL_free(tims.data);
        SDL_free(img);
        return cmp;
    }
    SDL_free(tims.data);
    /* a cylinder from the band's top to its bottom, closed by caps of the top and bottom rows' colours: rings at 70
       degrees up and down, then a point overhead and one underfoot (a triangle with that corner, beside the camera,
       is left out, so the rings keep the caps in view) */
    horizon = top + 0.6 * (bottom - top);
    eTop = (horizon - top) / pxPerDeg;
    eBottom = (horizon - bottom) / pxPerDeg;
    for (k = 0; k < SKY_SEGMENTS; k++) {
        double a = 2 * M_PI * k / SKY_SEGMENTS;
        int sx = (int)(SKY_RADIUS * sin(a)), sz = (int)(SKY_RADIUS * cos(a));

        vertex[k][0] = vertex[SKY_SEGMENTS + k][0] = sx;
        vertex[k][2] = vertex[SKY_SEGMENTS + k][2] = sz;
        vertex[k][1] = -(int)(SKY_RADIUS * tan(eTop * (M_PI / 180.0)));
        vertex[SKY_SEGMENTS + k][1] = -(int)(SKY_RADIUS * tan(eBottom * (M_PI / 180.0)));
        vertex[2 * SKY_SEGMENTS + k][0] = vertex[3 * SKY_SEGMENTS + k][0] = sx / 3;
        vertex[2 * SKY_SEGMENTS + k][2] = vertex[3 * SKY_SEGMENTS + k][2] = sz / 3;
        vertex[2 * SKY_SEGMENTS + k][1] = -(int)(SKY_RADIUS / 3 * tan(70 * (M_PI / 180.0)));
        vertex[3 * SKY_SEGMENTS + k][1] = (int)(SKY_RADIUS / 3 * tan(70 * (M_PI / 180.0)));
    }
    vertex[4 * SKY_SEGMENTS][0] = vertex[4 * SKY_SEGMENTS][2] = 0;
    vertex[4 * SKY_SEGMENTS][1] = -2 * SKY_RADIUS;
    vertex[4 * SKY_SEGMENTS + 1][0] = vertex[4 * SKY_SEGMENTS + 1][2] = 0;
    vertex[4 * SKY_SEGMENTS + 1][1] = 2 * SKY_RADIUS;
    for (x = 0; x < SKY_W; x++) {
        int a[3], b[3];

        Rgb5(cram[img[top * SKY_W + x]], a);
        Rgb5(cram[img[(bottom - 1) * SKY_W + x]], b);
        for (k = 0; k < 3; k++) {
            topColour[k] += a[k];
            bottomColour[k] += b[k];
        }
    }
    for (k = 0; k < 3; k++) {
        topRgb[k] = (uint8_t)SDL_min(255, 8 * topColour[k] / SKY_W);
        bottomRgb[k] = (uint8_t)SDL_min(255, 8 * bottomColour[k] / SKY_W);
    }
    SDL_free(img);
    for (k = 0; k < SKY_SEGMENTS; k++) {
        int k2 = (k + 1) % SKY_SEGMENTS, per = SKY_SEGMENTS / (2 * timCount);
        double segment = (double)SKY_TIM_W / per;
        int u0 = (int)((k % per) * segment), u1 = SDL_min(SKY_TIM_W - 1, (int)((k % per + 1) * segment));
        int quad[4] = {k, k2, SKY_SEGMENTS + k, SKY_SEGMENTS + k2};
        int upper[4] = {2 * SKY_SEGMENTS + k, 2 * SKY_SEGMENTS + k2, k, k2};
        int lower[4] = {SKY_SEGMENTS + k, SKY_SEGMENTS + k2, 3 * SKY_SEGMENTS + k, 3 * SKY_SEGMENTS + k2};
        int up[3] = {4 * SKY_SEGMENTS, 2 * SKY_SEGMENTS + k2, 2 * SKY_SEGMENTS + k};
        int down[3] = {4 * SKY_SEGMENTS + 1, 3 * SKY_SEGMENTS + k, 3 * SKY_SEGMENTS + k2};
        uint8_t uv[8] = {(uint8_t)u0, 0, (uint8_t)u1, 0, (uint8_t)u0, (uint8_t)(Hb - 1), (uint8_t)u1, (uint8_t)(Hb - 1)};
        int c;

        PutBe16(&prims, GT4);
        PutBe16(&prims, 0); /* double-sided */
        for (c = 0; c < 4; c++) {
            PutBe16(&prims, quad[c]);
        }
        PutBe16(&prims, k / per % timCount);
        PutBe16(&prims, 0);
        PutBe16(&prims, 0);
        Put(&prims, uv, 8);
        PutBe16(&prims, 0);
        for (c = 0; c < 4; c++) {
            PutColour(&prims, grey);
        }
        PutFlat(&prims, upper, 4, 0, topRgb, 1);
        PutFlat(&prims, lower, 4, 0, bottomRgb, 1);
        PutFlat(&prims, up, 3, 0, topRgb, 1);
        PutFlat(&prims, down, 3, 0, bottomRgb, 1);
    }
    PutObject(&out, "sky", vertex, 4 * SKY_SEGMENTS + 2, zero, &prims, 5 * SKY_SEGMENTS);
    SDL_free(prims.data);
    *size = out.size;
    return out.data;
}

unsigned char *SaturnMakeTrackModel(int track, const char *name, size_t *size)
{
    static const int tracks[] = {1, 2, 6, 7, 8, 13, 17, 20};
    size_t i;

    *size = 0;
    for (i = 0; i < SDL_arraysize(tracks) && tracks[i] != track; i++) {
    }
    if (i == SDL_arraysize(tracks)) {
        return NULL;
    }
    if (!SDL_strcasecmp(name, "scene.prm") || !SDL_strcasecmp(name, "scene.cmp")) {
        return MakeScene(track, !SDL_strcasecmp(name, "scene.cmp"), size);
    }
    if (!SDL_strcasecmp(name, "sky.prm") || !SDL_strcasecmp(name, "sky.cmp")) {
        return MakeSky(track, !SDL_strcasecmp(name, "sky.cmp"), size);
    }
    return NULL;
}
