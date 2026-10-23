/* The Sega Saturn's Wipeout XL (Japan, T-18619G) and WipEout 2097 (Europe): their disc in place of the PlayStation's.

   The Saturn disc holds two files, its program (0.AAA) and CDIMAGE.ELF, an archive of the game's data under the
   hashes of their names (h = h * 2 + each character, of paths as art\track01\course01.rbh): containers of blocks
   (BIFF/RBHF: each block's Saturn address, size and flags, then BODY chunks), raw video memory images (.vri), the
   sound driver and samples, and movies. Nothing there is in the PlayStation's formats: the files the PlayStation
   game reads are made here from the Saturn's (the game's file layer asks PortSaturnFileInfo and PortSaturnRead
   first, as for original.c's and region.c's files), each when first asked for.

   Made so far: a track's vertices, faces, sections, view lists and faces' textures and flags (track.trv, .trf, .trs,
   .vew in track.wad, and track.tex), and its texture library (library.cmp, .ttf). The rest of what the game reads
   (scenery, skies, ships, sounds, menus) is not yet: while it is not, a PlayStation disc named by
   WIPEOUT_SATURN_BORROW (development only) answers for it.

   A course (art\trackNN\courseNN.rbh, the PlayStation's track folder NN; .vri its textures) is, by block: 0 the
   sections (a u32 count, at 0x1b4 records of 172 bytes: junction, previous and next as byte offsets in the block,
   the centre, 15 view-list byte offsets and their counts, at +116 the offset of the section's faces in block 1, flags,
   radii, face count), 1 per section a list of shorts and then its faces (22 bytes: palette, texture, flags (the low
   byte the PlayStation's, the high VDP1's draw mode), Gouraud colour, 4 vertices, the normal), and the first block
   loaded with flags 0 the view lists, then the vertices (3 x 32 bits). The faces are the PlayStation's, in its
   order, with its textures and flags after track.tex. The vri (its Saturn address and size, then the data): the
   textures (0x1400 bytes each, 4-bit: the whole at 64x32, then 16 tiles of 32x16, the texture at 128x64), then
   the palettes (32 bytes each, a face's lighting in them), then the Gouraud table (8 bytes an entry). */
#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

#include "original.h"
#include "platform.h"
#include "saturn.h"
#include "psx/disc.h"
#include "psx/psx_io.h"

#define VIRTUAL_LOC 0x60000000 /* the sectors the files are read at: beyond any disc, apart from the others' */
#define VIRTUAL_SPAN 0x1000

static int checked;
static int saturn;          /* the disc is the Saturn's */
static int32_t archiveLba;  /* CDIMAGE.ELF */
static uint32_t archiveSize;
static uint8_t *archive;    /* its entries after a 16-byte head: key, sector, size, sectors (big-endian) */
static int entries;
static Disc *borrow;        /* WIPEOUT_SATURN_BORROW */

/* ---- reading ---- */

static uint32_t Be32(const uint8_t *p)
{
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}

static uint16_t Be16(const uint8_t *p)
{
    return (uint16_t)(p[0] << 8 | p[1]);
}

static void PutBe32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

static void PutBe16(uint8_t *p, int v)
{
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)v;
}

static void PutLe32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static void PutLe16(uint8_t *p, int v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

/* Whether a disc is the Saturn's Wipeout XL / 2097: its program and archive, and the Saturn's header. */
int PortSaturnDiscIs(const Disc *disc)
{
    int32_t lba;
    uint32_t size;
    uint8_t header[2048];

    return disc && DiscReadData(disc, 0, 1, header) && !memcmp(header, "SEGA SEGASATURN", 15) &&
           DiscFindFile(disc, "\\CDIMAGE.ELF", &lba, &size) && DiscFindFile(disc, "\\0.AAA", &lba, &size);
}

static void Check(void)
{
    const Disc *disc = PsxIoDisc();

    if (checked) {
        return;
    }
    checked = 1;
    if (!PortSaturnDiscIs(disc) || !DiscFindFile(disc, "\\CDIMAGE.ELF", &archiveLba, &archiveSize)) {
        return;
    }
    {
        uint8_t first[2048];

        if (!DiscReadData(disc, archiveLba, 1, first)) {
            return;
        }
        entries = (int)Be32(first);
        if (entries <= 0 || entries > 4096) {
            return;
        }
        archive = SDL_malloc((size_t)(16 + entries * 16 + 2047) / 2048 * 2048);
        if (!archive || !DiscReadData(disc, archiveLba, (16 + entries * 16 + 2047) / 2048, archive)) {
            SDL_free(archive);
            archive = NULL;
            return;
        }
    }
    saturn = 1;
    {
        const char *path = SDL_getenv("WIPEOUT_SATURN_BORROW");
        char error[256];

        if (path) {
            borrow = DiscOpen(path, error, sizeof(error));
        }
    }
}

int PortSaturnDisc(void)
{
    Check();
    return saturn;
}

/* An archive entry by its path (art\track01\course01.rbh), read whole (the caller frees). */
static uint8_t *Entry(const char *path, size_t *size)
{
    uint32_t key = 0;
    const char *c;
    int i;

    Check();
    if (!saturn) {
        return NULL;
    }
    for (c = path; *c; c++) {
        key = key * 2 + (uint8_t)*c;
    }
    for (i = 0; i < entries; i++) {
        const uint8_t *e = archive + 16 + i * 16;

        if (Be32(e) == key) {
            uint32_t sector = Be32(e + 4), length = Be32(e + 8), sectors = (length + 2047) / 2048;
            uint8_t *data = SDL_malloc((size_t)sectors * 2048);

            if (data && DiscReadData(PsxIoDisc(), archiveLba + (int32_t)sector, (int)sectors, data)) {
                *size = length;
                return data;
            }
            SDL_free(data);
            return NULL;
        }
    }
    return NULL;
}

/* A container's blocks: data, size and load flags. */
#define MAX_BLOCKS 64
typedef struct Blocks {
    uint8_t *file;
    int count;
    const uint8_t *data[MAX_BLOCKS];
    uint32_t size[MAX_BLOCKS], flags[MAX_BLOCKS];
} Blocks;

static int ReadBlocks(const char *path, Blocks *b)
{
    size_t length;
    uint32_t header, at, i;

    memset(b, 0, sizeof(*b));
    b->file = Entry(path, &length);
    if (!b->file || length < 20 || memcmp(b->file, "BIFF", 4) || memcmp(b->file + 8, "RBHFRBHH", 8)) {
        SDL_free(b->file);
        b->file = NULL;
        return 0;
    }
    header = Be32(b->file + 16);
    at = 20 + header;
    for (i = 0; i < header / 12 && b->count < MAX_BLOCKS; i++) {
        uint32_t size;

        if (at + 8 > length) {
            break;
        }
        size = Be32(b->file + at + 4);
        if (at + 8 + size > length) {
            break;
        }
        b->flags[b->count] = Be32(b->file + 20 + i * 12 + 8);
        b->data[b->count] = b->file + at + 8;
        b->size[b->count] = size;
        b->count++;
        at += 8 + size;
        at += at & 1;
    }
    return b->count;
}

/* ---- a track ---- */

#define SECTIONS_AT 0x1b4
#define SECTION 172
#define FACE 22
#define TEXTURE_BYTES 0x1400
#define SPEED_UP 0x20
#define WEAPON_PAD 0x40
#define SPD 0x4000 /* a face's flags, the high byte: VDP1's draw mode, pixel 0 drawn (not see-through) */

typedef struct Course {
    Blocks rbh;
    uint8_t *vri;
    size_t vriSize;
    const uint8_t *sections, *faces, *views;
    uint32_t sectionCount, facesSize, viewsSize, viewsEnd;
    uint32_t textures, palettes, gouraud; /* the vri's parts (Saturn addresses) */
    int textureCount, paletteCount;
} Course;

static int ReadCourse(int number, Course *c)
{
    char path[64];
    int i, maxTexture = 0, maxPalette = 0;

    memset(c, 0, sizeof(*c));
    SDL_snprintf(path, sizeof(path), "art\\track%02d\\course%02d.rbh", number, number);
    if (ReadBlocks(path, &c->rbh) < 3) {
        return 0;
    }
    SDL_snprintf(path, sizeof(path), "art\\track%02d\\course%02d.vri", number, number);
    c->vri = Entry(path, &c->vriSize);
    if (!c->vri || c->vriSize < 8) {
        return 0;
    }
    c->sections = c->rbh.data[0];
    c->sectionCount = Be32(c->sections);
    c->faces = c->rbh.data[1];
    c->facesSize = c->rbh.size[1];
    for (i = 2; i < c->rbh.count && !c->views; i++) {
        if (c->rbh.flags[i] == 0) {
            c->views = c->rbh.data[i];
            c->viewsSize = c->rbh.size[i];
        }
    }
    if (!c->views || SECTIONS_AT + c->sectionCount * SECTION > c->rbh.size[0]) {
        return 0;
    }
    for (i = 0; i < (int)c->sectionCount; i++) {
        const uint8_t *s = c->sections + SECTIONS_AT + i * SECTION;
        uint32_t facesAt = Be32(s + 120), n = Be16(s + 158), k;
        int v;

        for (v = 0; v < 15; v++) {
            uint32_t end = Be32(s + 24 + v * 4) + Be16(s + 84 + v * 2) * 2u;

            c->viewsEnd = end > c->viewsEnd ? end : c->viewsEnd;
        }
        if (facesAt + n * FACE > c->facesSize) {
            return 0;
        }
        for (k = 0; k < n; k++) {
            const uint8_t *f = c->faces + facesAt + k * FACE;

            maxPalette = Be16(f) > maxPalette ? Be16(f) : maxPalette;
            maxTexture = Be16(f + 2) > maxTexture ? Be16(f + 2) : maxTexture;
        }
    }
    if (c->viewsEnd > c->viewsSize) {
        return 0;
    }
    /* the vri's parts follow each other: the textures, the palettes, the Gouraud table */
    c->textureCount = maxTexture + 1;
    c->paletteCount = maxPalette + 1;
    c->textures = Be32(c->vri);
    c->palettes = c->textures + (uint32_t)c->textureCount * TEXTURE_BYTES;
    c->gouraud = c->palettes + (uint32_t)c->paletteCount * 32;
    return 1;
}

static void FreeCourse(Course *c)
{
    SDL_free(c->rbh.file);
    SDL_free(c->vri);
}

/* A byte of the vri at a Saturn address, NULL beyond it. */
static const uint8_t *Vri(const Course *c, uint32_t address, size_t bytes)
{
    size_t at = (size_t)(address - c->textures) + 8;

    return address >= c->textures && at + bytes <= c->vriSize ? c->vri + at : NULL;
}

/* A Gouraud table entry as the PlayStation's face colour: VDP1's adds to the texel (16 neutral), the PlayStation's
   multiplies (128): those above neutral were put in at a sixteenth of the PlayStation's step, those below an eighth. */
static void Colour(const Course *c, int entry, uint8_t rgb[3])
{
    const uint8_t *p = Vri(c, c->gouraud + (uint32_t)entry * 8, 2);
    int value = p ? Be16(p) : 0x4210, k;

    for (k = 0; k < 3; k++) {
        int g = (value >> (5 * k)) & 31;

        rgb[k] = (uint8_t)(g <= 16 ? g * 8 : SDL_min(255, 128 + (g - 16) * 16));
    }
}

typedef struct Buffer {
    uint8_t *data;
    size_t size, capacity;
} Buffer;

static void Put(Buffer *b, const void *data, size_t size)
{
    if (b->size + size > b->capacity) {
        b->capacity = (b->size + size) * 2 + 1024;
        b->data = SDL_realloc(b->data, b->capacity);
    }
    memcpy(b->data + b->size, data, size);
    b->size += size;
}

/* track.trv, .trf, .trs, .vew and .tex */
static void MakeGeometry(const Course *c, Buffer *trv, Buffer *trf, Buffer *trs, Buffer *vew, Buffer *tex)
{
    uint32_t i, k;
    int faceStart = 0;

    for (i = 0; c->viewsEnd + i * 12 + 12 <= c->viewsSize; i++) {
        uint8_t v[16] = {0};

        memcpy(v, c->views + c->viewsEnd + i * 12, 12);
        Put(trv, v, 16);
    }
    for (i = 0; i < c->sectionCount; i++) {
        const uint8_t *s = c->sections + SECTIONS_AT + i * SECTION;
        uint32_t facesAt = Be32(s + 120), n = Be16(s + 158);
        int plain = -1, j, d;
        uint8_t r[156] = {0};

        /* the speed and weapon pads have the Saturn's glow in their colour: shown in the colour of the track about
           them (a plain face of the section) */
        for (k = 0; k < n && plain < 0; k++) {
            const uint8_t *f = c->faces + facesAt + k * FACE;

            if (!(Be16(f + 4) & (SPEED_UP | WEAPON_PAD))) {
                plain = Be16(f + 6);
            }
        }
        for (k = 0; k < n; k++) {
            const uint8_t *f = c->faces + facesAt + k * FACE;
            int flags = Be16(f + 4), colour = Be16(f + 6);
            uint8_t out[20] = {0};

            if ((flags & (SPEED_UP | WEAPON_PAD)) && plain >= 0) {
                colour = plain;
            }
            memcpy(out, f + 8, 14); /* the corners and the normal */
            out[14] = f[3];         /* the texture */
            out[15] = (uint8_t)flags;
            Colour(c, colour, out + 16);
            Put(trf, out, 20);
            Put(tex, out + 14, 2);
        }
        /* the PlayStation's record; its view pointers and objects are the game's to set */
        for (j = 0; j < 3; j++) {
            uint32_t at = Be32(s + j * 4);

            PutBe32(r + j * 4, at == 0xFFFFFFFFu ? 0xFFFFFFFFu : (at - SECTIONS_AT) / SECTION);
        }
        memcpy(r + 12, s + 12, 12); /* the centre */
        PutBe16(r + 24, 8);         /* the version the game checks */
        memcpy(r + 96, s + 84, 30); /* the view counts */
        PutBe16(r + 142, faceStart);
        PutBe16(r + 144, (int)n);
        memcpy(r + 146, s + 154, 4); /* the radii */
        memcpy(r + 150, s + 144, 2); /* the flags */
        memcpy(r + 152, s + 160, 2); /* (the section number: the game numbers them again) */
        Put(trs, r, 156);
        faceStart += (int)n;
        /* the view lists: per detail, north, south, east, west and all (the counts are kept by direction) */
        for (j = 0; j < 3; j++) {
            for (d = 0; d < 5; d++) {
                int list = d * 3 + j;

                Put(vew, c->views + Be32(s + 24 + list * 4), Be16(s + 84 + list * 2) * 2u);
            }
        }
    }
}

/* The track's tiles: per texture 16 of the high detail, 4 medium and 1 low, as 32x32 4-bit TIMs. */
#define MAX_TILES 275 /* the game's room for a track's (main.c's TextureTableCount 0x113, the HUD's after it) */
typedef struct Tile {
    uint8_t pixels[32][32];
    uint16_t palette[16];
} Tile;

/* where tile n goes: in the second half of video memory (rows 512 on, as original.c puts an original track's):
   texels in 14 rows of 32 from row 512, palettes from row 1023 up */
static void Place(int n, int *tx, int *ty, int *cx, int *cy)
{
    *tx = (n / 14) * 8;
    *ty = 512 + (n % 14) * 32;
    *cx = (n % 64) * 16;
    *cy = 1023 - n / 64;
}

static void MakeLibrary(const Course *c, Buffer *cmp, Buffer *ttf)
{
    Tile *tiles = SDL_calloc(MAX_TILES, sizeof(Tile));
    int tileCount = 0, t, i, k, ch;
    double (*sum)[16][3] = SDL_calloc((size_t)c->textureCount, sizeof(*sum));
    int (*counted)[16][3] = SDL_calloc((size_t)c->textureCount, sizeof(*counted));
    uint16_t (*fallback)[16] = SDL_calloc((size_t)c->textureCount, sizeof(*fallback));
    uint8_t *seen = SDL_calloc((size_t)c->textureCount, 1), *opaque = SDL_calloc((size_t)c->textureCount, 1);
    Buffer tims = {0}, sizes = {0};

    /* a texture's own palette: its faces' palettes with the face's colour taken out (palette * 128 / colour),
       averaged (the saturated and the very dark left out where others can say) */
    for (i = 0; i < (int)c->sectionCount; i++) {
        const uint8_t *s = c->sections + SECTIONS_AT + i * SECTION;
        uint32_t facesAt = Be32(s + 120), n = Be16(s + 158), f;

        for (f = 0; f < n; f++) {
            const uint8_t *face = c->faces + facesAt + f * FACE;
            int palette = Be16(face), texture = Be16(face + 2);
            const uint8_t *p = Vri(c, c->palettes + (uint32_t)palette * 32, 32);
            uint8_t colour[3];

            Colour(c, Be16(face + 6), colour);
            if (Be16(face + 4) & SPD) {
                opaque[texture] = 1;
            }
            if (!p) {
                continue;
            }
            for (k = 0; k < 16; k++) {
                int value = Be16(p + k * 2);

                if (!seen[texture]) {
                    fallback[texture][k] = (uint16_t)value;
                }
                for (ch = 0; ch < 3; ch++) {
                    int level = (value >> (5 * ch)) & 31;

                    if (colour[ch] >= 32 && level < 31) {
                        sum[texture][k][ch] += level * 128.0 / colour[ch];
                        counted[texture][k][ch]++;
                    }
                }
            }
            seen[texture] = 1;
        }
    }
    for (t = 0; t < c->textureCount; t++) {
        const uint8_t *data = Vri(c, c->textures + (uint32_t)t * TEXTURE_BYTES, TEXTURE_BYTES);
        uint8_t px[TEXTURE_BYTES * 2], whole[32][64], high[64][128];
        uint16_t palette[16];
        int16_t entry[21];
        int e;

        for (k = 0; k < 16; k++) {
            int rgb = 0;

            for (ch = 0; ch < 3; ch++) {
                int level = counted[t][k][ch] ? (int)SDL_lround(sum[t][k][ch] / counted[t][k][ch])
                                              : (fallback[t][k] >> (5 * ch)) & 31;

                rgb |= SDL_min(31, level) << (5 * ch);
            }
            /* pixel 0 see-through as VDP1 draws it unless the faces' draw mode says otherwise (the PlayStation's
               0x0000); other blacks keep the mask bit */
            palette[k] = (uint16_t)(k == 0 && !opaque[t] ? 0 : rgb ? rgb : 0x8000);
        }
        memset(px, 0, sizeof(px));
        if (data) {
            for (i = 0; i < TEXTURE_BYTES; i++) {
                px[i * 2] = data[i] >> 4;
                px[i * 2 + 1] = data[i] & 15;
            }
        }
        for (i = 0; i < 32; i++) {
            memcpy(whole[i], px + i * 64, 64);
        }
        for (k = 0; k < 16; k++) {
            for (i = 0; i < 16; i++) {
                memcpy(&high[(k / 4) * 16 + i][(k % 4) * 32], px + 2048 + k * 512 + i * 32, 32);
            }
        }
        for (e = 0; e < 21; e++) {
            Tile tile;
            int y, x, found;

            memcpy(tile.palette, palette, sizeof(palette));
            for (y = 0; y < 32; y++) {
                for (x = 0; x < 32; x++) {
                    if (e < 16) { /* high: the 4 x 4 tiles, rows doubled */
                        tile.pixels[y][x] = high[(e / 4) * 16 + y / 2][(e % 4) * 32 + x];
                    } else if (e < 20) { /* medium: the whole's quarters, rows doubled */
                        tile.pixels[y][x] = whole[((e - 16) / 2) * 16 + y / 2][((e - 16) % 2) * 32 + x];
                    } else { /* low: the whole, every other column */
                        tile.pixels[y][x] = whole[y][x * 2];
                    }
                }
            }
            for (found = 0; found < tileCount && memcmp(&tiles[found], &tile, sizeof(tile)); found++) {
            }
            if (found == tileCount && tileCount < MAX_TILES) {
                tiles[tileCount++] = tile;
            }
            entry[e] = (int16_t)SDL_min(found, MAX_TILES - 1);
        }
        for (e = 0; e < 21; e++) {
            uint8_t be[2];

            PutBe16(be, entry[e]);
            Put(ttf, be, 2);
        }
    }
    for (t = 0; t < tileCount; t++) {
        uint8_t head[8 + 12 + 32 + 12 + 512], size[4];
        int tx, ty, cx, cy, y, x;

        Place(t, &tx, &ty, &cx, &cy);
        PutLe32(head, 0x10);
        PutLe32(head + 4, 8); /* 4-bit, with a palette */
        PutLe32(head + 8, 12 + 32);
        PutLe16(head + 12, cx);
        PutLe16(head + 14, cy);
        PutLe16(head + 16, 16);
        PutLe16(head + 18, 1);
        for (k = 0; k < 16; k++) {
            PutLe16(head + 20 + k * 2, tiles[t].palette[k]);
        }
        PutLe32(head + 52, 12 + 512);
        PutLe16(head + 56, tx);
        PutLe16(head + 58, ty);
        PutLe16(head + 60, 8);
        PutLe16(head + 62, 32);
        for (y = 0; y < 32; y++) {
            for (x = 0; x < 32; x += 2) {
                head[64 + y * 16 + x / 2] = (uint8_t)(tiles[t].pixels[y][x] | tiles[t].pixels[y][x + 1] << 4);
            }
        }
        Put(&tims, head, sizeof(head));
        PutLe32(size, sizeof(head));
        Put(&sizes, size, 4);
    }
    {
        uint8_t count[4];
        size_t packedSize;
        uint8_t *packed = PortLzssStore(tims.data, tims.size, &packedSize);

        PutLe32(count, (uint32_t)tileCount);
        Put(cmp, count, 4);
        Put(cmp, sizes.data, sizes.size);
        Put(cmp, packed, packedSize);
        SDL_free(packed);
    }
    SDL_free(tims.data);
    SDL_free(sizes.data);
    SDL_free(tiles);
    SDL_free(sum);
    SDL_free(counted);
    SDL_free(fallback);
    SDL_free(seen);
    SDL_free(opaque);
}

/* ---- the files ---- */

/* A file of the borrowed PlayStation disc (WIPEOUT_SATURN_BORROW), by the game's path (\wipeout2\...). */
static uint8_t *Borrowed(const char *path, size_t *size)
{
    char upper[256];
    int32_t lba;
    uint32_t length;
    uint8_t *data;
    size_t i;

    if (!borrow) {
        return NULL;
    }
    for (i = 0; path[i] && i + 1 < sizeof(upper); i++) {
        upper[i] = (char)SDL_toupper((unsigned char)path[i]);
    }
    upper[i] = 0;
    if (!DiscFindFile(borrow, upper, &lba, &length)) {
        return NULL;
    }
    data = SDL_malloc((length + 2047) / 2048 * 2048);
    if (!data || !DiscReadData(borrow, lba, (int)((length + 2047) / 2048), data)) {
        SDL_free(data);
        return NULL;
    }
    *size = length;
    return data;
}

/* A track's wad: the game reads its parts in this order (track.c, init.c) */
static const char *const wadParts[] = {"track.trv", "track.trf", "track.trs", "track.vew", "library.cmp",
                                       "library.ttf", "scene.cmp", "sky.cmp", "scene.prm", "sky.prm"};
#define WAD_PARTS ((int)SDL_arraysize(wadParts))

/* A part of a wad the borrowed disc has (its own track's), the caller frees. */
static uint8_t *BorrowedWadPart(int track, const char *name, size_t *size)
{
    char path[64];
    size_t length, at;
    uint8_t *wad;
    int count, i;

    SDL_snprintf(path, sizeof(path), "\\wipeout2\\track%02d\\track.wad", track);
    wad = Borrowed(path, &length);
    if (!wad || length < 2) {
        SDL_free(wad);
        return NULL;
    }
    count = wad[0] | wad[1] << 8;
    at = 2 + (size_t)count * 25;
    for (i = 0; i < count && at <= length; i++) {
        const uint8_t *e = wad + 2 + i * 25;
        size_t partSize = e[20] | e[21] << 8 | (size_t)e[22] << 16 | (size_t)e[23] << 24;

        if (!SDL_strncasecmp((const char *)e, name, 16) && at + partSize <= length) {
            uint8_t *part = SDL_malloc(partSize + 1);

            memcpy(part, wad + at, partSize);
            *size = partSize;
            SDL_free(wad);
            return part;
        }
        at += partSize;
    }
    SDL_free(wad);
    return NULL;
}

static uint8_t *MakeTrackWad(int track, size_t *size, uint8_t **texFile, size_t *texSize)
{
    Course c;
    Buffer parts[WAD_PARTS], tex = {0}, wad = {0};
    int i;

    if (!ReadCourse(track, &c)) {
        FreeCourse(&c);
        return NULL;
    }
    memset(parts, 0, sizeof(parts));
    MakeGeometry(&c, &parts[0], &parts[1], &parts[2], &parts[3], &tex);
    MakeLibrary(&c, &parts[4], &parts[5]);
    FreeCourse(&c);
    for (i = 6; i < WAD_PARTS; i++) {
        size_t length = 0;
        uint8_t *part = BorrowedWadPart(track, wadParts[i], &length);

        if (part) {
            Put(&parts[i], part, length);
            SDL_free(part);
        }
    }
    {
        uint8_t count[2];

        PutLe16(count, WAD_PARTS);
        Put(&wad, count, 2);
    }
    for (i = 0; i < WAD_PARTS; i++) {
        uint8_t e[25] = {0};

        SDL_strlcpy((char *)e, wadParts[i], 16);
        PutLe32(e + 20, (uint32_t)parts[i].size);
        Put(&wad, e, 25);
    }
    for (i = 0; i < WAD_PARTS; i++) {
        Put(&wad, parts[i].data, parts[i].size);
        SDL_free(parts[i].data);
    }
    *texFile = tex.data;
    *texSize = tex.size;
    *size = wad.size;
    return wad.data;
}

/* The files made so far, by the game's path (lower case, from \wipeout2\). */
#define MAX_FILES 512
static struct {
    char path[64];
    uint8_t *data;
    size_t size;
} made[MAX_FILES];
static int madeCount;

static int Remember(const char *path, uint8_t *data, size_t size)
{
    if (madeCount >= MAX_FILES) {
        SDL_free(data);
        return -1;
    }
    SDL_strlcpy(made[madeCount].path, path, sizeof(made[madeCount].path));
    made[madeCount].data = data;
    made[madeCount].size = size;
    return madeCount++;
}

static int Find(const char *path)
{
    int i, track;
    char tail[32];

    for (i = 0; i < madeCount; i++) {
        if (!strcmp(made[i].path, path)) {
            return i;
        }
    }
    if (SDL_sscanf(path, "\\wipeout2\\track%d\\%31s", &track, tail) == 2 &&
        (!strcmp(tail, "track.wad") || !strcmp(tail, "track.tex"))) {
        size_t wadSize = 0, texSize = 0;
        uint8_t *tex = NULL, *wad = MakeTrackWad(track, &wadSize, &tex, &texSize);
        char other[64];

        if (wad) {
            SDL_snprintf(other, sizeof(other), "\\wipeout2\\track%02d\\track.wad", track);
            Remember(other, wad, wadSize);
            SDL_snprintf(other, sizeof(other), "\\wipeout2\\track%02d\\track.tex", track);
            Remember(other, tex, texSize);
            for (i = 0; i < madeCount; i++) {
                if (!strcmp(made[i].path, path)) {
                    return i;
                }
            }
        }
    }
    {
        size_t size = 0;
        uint8_t *data = Borrowed(path, &size);

        if (data) {
            return Remember(path, data, size);
        }
    }
    return -1;
}

int PortSaturnFileInfo(const char *pathname, int *loc, int *size)
{
    char path[64];
    const char *p = pathname;
    size_t i;
    int file;

    if (!PortSaturnDisc()) {
        return 0;
    }
    if (p[0] && p[1] == ':') {
        p += 2;
    }
    for (i = 0; p[i] && i + 1 < sizeof(path); i++) {
        path[i] = (char)SDL_tolower((unsigned char)p[i]);
    }
    path[i] = 0;
    file = Find(path);
    if (file < 0) {
        printf("saturn.c: no %s yet\n", path);
        return 0;
    }
    *loc = VIRTUAL_LOC + file * VIRTUAL_SPAN;
    *size = (int)made[file].size;
    return 1;
}

int PortSaturnRead(int loc, int sectors, void *buffer)
{
    int file = (loc - VIRTUAL_LOC) / VIRTUAL_SPAN, sector = (loc - VIRTUAL_LOC) % VIRTUAL_SPAN;
    size_t from = (size_t)sector * 2048, length = (size_t)sectors * 2048;

    if (!saturn || loc < VIRTUAL_LOC || file >= madeCount) {
        return 0;
    }
    memset(buffer, 0, length);
    if (from < made[file].size) {
        memcpy(buffer, made[file].data + from, SDL_min(length, made[file].size - from));
    }
    return 1;
}
