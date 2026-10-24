/* The PC CD-ROM version of Wipeout XL / WipEout 2097 (Psygnosis, 1997): its files in place of the PlayStation
   disc's (docs/PC.md).

   The PC CD holds the game's data at its root (COMMON, TEXTURES, SOUND, TRACKnn), where the PlayStation disc has it
   in \WIPEOUT2, much of it byte for byte the PlayStation's; the rest of the CD is the PC version's own (its texture
   pages in PICS, its pictures in BLITSURF, its movies in MPEG). The files are read from a disc image or from a
   folder with the CD's files (copied, or an installed copy with its data).

   The game's file layer asks here first (PortPcFileInfo, PortPcRead, as for saturn.c's files): a file of \WIPEOUT2
   is the PC's of the same name, read whole into memory the first time; samples.wad is made from the PC's WAVs
   (pc_sound.c). A file the PC never had (the PlayStation's menus, HUD and fonts, which the PC version draws from its
   own pages) is made from its shape with nothing in it (saturn_shapes.c's table): images see-through, models
   without polygons. The PC front end (game/src/pc) reads the PC's own files with PortPcLoadFile. */
#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

#include "original.h"
#include "pc.h"
#include "platform.h"
#include "saturn_shapes.h"
#include "psx/disc.h"
#include "psx/gpu.h"
#include "psx/psx_io.h"

#define VIRTUAL_LOC 0x68000000 /* the sectors the files are read at: beyond any disc, apart from saturn.c's */
#define VIRTUAL_SPAN 0x1000

static int checked;
static int pc;              /* the PC version's files are in use */
static char folder[1024];   /* ... from this folder (else the disc image) */

/* ---- where the files are ---- */

int PortPcDiscIs(const Disc *disc)
{
    int32_t lba;
    uint32_t size;

    return disc && DiscIsPcGame(disc) &&
           DiscFindFile(disc, "\\TRACK01\\TRACK.WAD", &lba, &size) &&
           !DiscFindFile(disc, "\\WIPEOUT2\\SOUND\\SAMPLES.WAD", &lba, &size);
}

/* The name in `dir` matching `name` in any case (FAT and ISO names are capitals, copies may not be), into found. */
typedef struct Match {
    const char *name;
    char found[256];
} Match;

static SDL_EnumerationResult SDLCALL MatchName(void *user, const char *dir, const char *name)
{
    Match *m = user;

    (void)dir;
    if (!SDL_strcasecmp(name, m->name)) {
        SDL_strlcpy(m->found, name, sizeof(m->found));
        return SDL_ENUM_SUCCESS;
    }
    return SDL_ENUM_CONTINUE;
}

/* The host path of `path` (components separated by \ or /) under `root`, each matched in any case; 0 when a
   component is not there. */
static int FolderPath(const char *root, const char *path, char *out, size_t outSize)
{
    char component[256];
    const char *p = path;

    SDL_strlcpy(out, root, outSize);
    while (*p) {
        size_t n = 0;
        Match m;

        while (*p == '\\' || *p == '/') {
            p++;
        }
        while (*p && *p != '\\' && *p != '/' && n + 1 < sizeof(component)) {
            component[n++] = *p++;
        }
        component[n] = 0;
        if (!n) {
            break;
        }
        m.name = component;
        m.found[0] = 0;
        SDL_EnumerateDirectory(out, MatchName, &m);
        if (!m.found[0]) {
            return 0;
        }
        SDL_strlcat(out, "/", outSize);
        SDL_strlcat(out, m.found, outSize);
    }
    return 1;
}

int PortPcFolderIs(const char *path)
{
    char found[1200];

    return path && FolderPath(path, "COMMON/VECTO.PRM", found, sizeof(found)) &&
           FolderPath(path, "TRACK01/TRACK.WAD", found, sizeof(found)) &&
           !FolderPath(path, "WIPEOUT2", found, sizeof(found));
}

void PortPcUseFolder(const char *path)
{
    SDL_strlcpy(folder, path, sizeof(folder));
    checked = 0;
}

static void Check(void)
{
    if (checked) {
        return;
    }
    checked = 1;
    pc = folder[0] ? PortPcFolderIs(folder) : PortPcDiscIs(PsxIoDisc());
}

int PortPcData(void)
{
    Check();
    return pc;
}

const char *PortPcTitle(void)
{
    return "Wipeout XL / WipEout 2097 (PC CD-ROM)";
}

unsigned char *PortPcLoadFile(const char *path, size_t *size)
{
    Check();
    if (!pc) {
        return NULL;
    }
    if (folder[0]) {
        char host[1200];

        return FolderPath(folder, path, host, sizeof(host)) ? SDL_LoadFile(host, size) : NULL;
    }
    {
        const Disc *disc = PsxIoDisc();
        char isoPath[256] = "\\";
        int32_t lba;
        uint32_t bytes;
        uint8_t *data;

        SDL_strlcat(isoPath, path, sizeof(isoPath));
        if (!DiscFindFile(disc, isoPath, &lba, &bytes) || !(data = SDL_malloc((size_t)bytes / 2048 * 2048 + 2048))) {
            return NULL;
        }
        if (!DiscReadData(disc, lba, (int)((bytes + 2047) / 2048), data)) {
            SDL_free(data);
            return NULL;
        }
        *size = bytes;
        return data;
    }
}

/* ---- the PC version's texture pages ---- */

/* A page's texels, from a 15-bit colour (0 see-through, the PC's colour key) to premultiplied RGBA. */
static uint32_t Rgba15(int c)
{
    int r = c & 31, g = c >> 5 & 31, b = c >> 10 & 31;

    return c & 0x7FFF ? (uint32_t)(r * 255 / 31) | (uint32_t)(g * 255 / 31) << 8 | (uint32_t)(b * 255 / 31) << 16 |
                            0xFF000000u
                      : 0;
}

int PortPcLoadPages(const char *name, int firstPage, int pageCount)
{
    char path[128];
    size_t size = 0;
    uint8_t *tga;
    uint32_t *page;
    int w, h, k, x, y;

    SDL_snprintf(path, sizeof(path), "PICS\\HI-RES\\%s", name);
    if (!(tga = PortPcLoadFile(path, &size))) {
        return 0;
    }
    /* a TGA of 16-bit colours (A1R5G5B5: the PC's 15-bit colour, unused alpha), its rows from the top (as the PC
       reads them: its descriptor says the bottom) */
    w = tga[12] | tga[13] << 8;
    h = tga[14] | tga[15] << 8;
    if (size < 18 || tga[2] != 2 || tga[16] != 16 || size < (size_t)18 + tga[0] + (size_t)w * h * 2 || h > 256) {
        SDL_free(tga);
        return 0;
    }
    page = SDL_malloc(256 * 256 * sizeof(uint32_t));
    for (k = 0; k < pageCount && k * 256 < w; k++) {
        for (y = 0; y < 256; y++) {
            for (x = 0; x < 256; x++) {
                int row = y, column = k * 256 + x;
                const uint8_t *p = tga + 18 + tga[0] + ((size_t)row * w + column) * 2;
                int c = y < h && column < w ? p[0] | p[1] << 8 : 0;

                /* (the file's X1R5G5B5 to the console's order: red in the low bits) */
                page[y * 256 + x] = Rgba15((c >> 10 & 31) | (c & 0x3E0) | (c & 31) << 10);
            }
        }
        GpuSetPortPage(firstPage + k + 1, page);
    }
    SDL_free(page);
    SDL_free(tga);
    return k;
}

int PortPcLoadPicture(const char *name, int firstPage)
{
    char path[128];
    size_t size = 0;
    uint8_t *ppm;
    uint32_t *page;
    int w = 0, h = 0, max = 0, used = 0, k, x, y;

    SDL_snprintf(path, sizeof(path), "BLITSURF\\HI-RES\\%s", name);
    if (!(ppm = PortPcLoadFile(path, &size))) {
        return 0;
    }
    {
        /* P6, a comment, the width, height and largest value, one white space, the RGB bytes */
        char *text = SDL_malloc(size + 1);
        char *p;

        memcpy(text, ppm, size);
        text[size] = 0;
        p = text + 2;
        for (k = 0; k < 3 && *p; ) {
            if (*p == '#') {
                while (*p && *p != '\n') {
                    p++;
                }
            } else if (*p >= '0' && *p <= '9') {
                int v = (int)SDL_strtol(p, &p, 10);

                *(k == 0 ? &w : k == 1 ? &h : &max) = v;
                k++;
                continue;
            }
            p++;
        }
        used = (int)(p - text) + 1;
        SDL_free(text);
    }
    if (memcmp(ppm, "P6", 2) || w <= 0 || h <= 0 || w > 768 || h > 512 || max != 255 ||
        size < (size_t)used + (size_t)w * h * 3) {
        SDL_free(ppm);
        return 0;
    }
    /* the picture over 3 x 2 pages, left to right and top to bottom: at 640 x 480, the console's screen twice */
    page = SDL_malloc(256 * 256 * sizeof(uint32_t));
    for (k = 0; k < 6; k++) {
        for (y = 0; y < 256; y++) {
            for (x = 0; x < 256; x++) {
                int px = k % 3 * 256 + x, py = k / 3 * 256 + y;
                const uint8_t *p = ppm + used + ((size_t)py * w + px) * 3;

                page[y * 256 + x] = px < w && py < h ? (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 |
                                                           0xFF000000u
                                                     : 0;
            }
        }
        GpuSetPortPage(firstPage + k + 1, page);
    }
    SDL_free(page);
    SDL_free(ppm);
    return 1;
}

/* ---- files made ---- */

typedef struct Buffer {
    uint8_t *data;
    size_t size, capacity;
} Buffer;

static void Put(Buffer *b, const void *data, size_t size)
{
    if (b->size + size > b->capacity) {
        b->capacity = (b->size + size) * 2 + 4096;
        b->data = SDL_realloc(b->data, b->capacity);
    }
    memcpy(b->data + b->size, data, size);
    b->size += size;
}

static void PutLe16(uint8_t *p, int v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

static void PutLe32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static void PutBe16(uint8_t *p, int v)
{
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)v;
}

static void PutBe32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

/* A TIM of a shape with nothing in it: its palette all see-through, its texels 0. */
static void PutTim(Buffer *b, const Shape *s)
{
    uint8_t head[20], *zeros;
    int pixels = s->r2[2] * 2 * s->r2[3];

    if (s->a == 15) {
        return; /* an empty one of a sequence */
    }
    PutLe32(head, 0x10);
    PutLe32(head + 4, (uint32_t)s->a);
    Put(b, head, 8);
    if (s->a & 8) {
        int bytes = s->r1[2] * s->r1[3] * 2;

        PutLe32(head, (uint32_t)(12 + bytes));
        PutLe16(head + 4, s->r1[0]);
        PutLe16(head + 6, s->r1[1]);
        PutLe16(head + 8, s->r1[2]);
        PutLe16(head + 10, s->r1[3]);
        Put(b, head, 12);
        zeros = SDL_calloc(1, (size_t)bytes + 1);
        Put(b, zeros, (size_t)bytes);
        SDL_free(zeros);
    }
    PutLe32(head, (uint32_t)(12 + pixels));
    PutLe16(head + 4, s->r2[0]);
    PutLe16(head + 6, s->r2[1]);
    PutLe16(head + 8, s->r2[2]);
    PutLe16(head + 10, s->r2[3]);
    Put(b, head, 12);
    zeros = SDL_calloc(1, (size_t)pixels + 1);
    Put(b, zeros, (size_t)pixels);
    SDL_free(zeros);
}

/* The wad of shape `s` from its parts' contents (freed here). */
static void PutWad(Buffer *b, const Shape *s, Buffer *parts)
{
    uint8_t count[2];
    int i;

    PutLe16(count, s->count);
    Put(b, count, 2);
    for (i = 0; i < s->count; i++) {
        uint8_t e[25] = {0};

        SDL_strlcpy((char *)e, s->list[i].name, 16);
        PutLe32(e + 20, (uint32_t)parts[i].size);
        Put(b, e, 25);
    }
    for (i = 0; i < s->count; i++) {
        Put(b, parts[i].data, parts[i].size);
        SDL_free(parts[i].data);
    }
    SDL_free(parts);
}

/* A file of a shape with nothing in it: images see-through, models without polygons, sounds silent. */
static void PutStandIn(Buffer *b, const Shape *s)
{
    int i;

    switch (s->kind) {
    case SHAPE_TIM:
        PutTim(b, s);
        break;
    case SHAPE_CMP: {
        Buffer tims = {0};
        uint8_t word[4];
        size_t packedSize;
        uint8_t *packed;

        PutLe32(word, (uint32_t)s->count);
        Put(b, word, 4);
        for (i = 0; i < s->count; i++) {
            size_t before = tims.size;

            PutTim(&tims, &s->list[i]);
            PutLe32(word, (uint32_t)(tims.size - before));
            Put(b, word, 4);
        }
        packed = PortLzssStore(tims.data ? tims.data : (const uint8_t *)"", tims.size, &packedSize);
        Put(b, packed, packedSize);
        SDL_free(packed);
        SDL_free(tims.data);
        break;
    }
    case SHAPE_PRM:
        for (i = 0; i < s->count; i++) {
            const Shape *o = &s->list[i];
            uint8_t object[64 + 80] = {0};
            size_t bytes = (size_t)(o->r1[0] + o->r1[1]) * 8;
            uint8_t *zeros = SDL_calloc(1, bytes + 1);
            int k;

            SDL_strlcpy((char *)object, o->name, 16);
            PutBe16(object + 16, o->r1[0]);
            PutBe16(object + 24, o->r1[1]);
            PutBe32(object + 52, 1);    /* the extent */
            PutBe16(object + 56, o->a); /* the flags */
            for (k = 0; k < 3; k++) {
                PutBe16(object + 64 + k * 8, 4096); /* the skeleton: identity */
                PutBe16(object + 64 + 32 + k * 8, 4096);
            }
            Put(b, object, sizeof(object));
            Put(b, zeros, bytes); /* the vertices and normals, no primitives */
            SDL_free(zeros);
        }
        break;
    case SHAPE_VAG: {
        uint8_t *v = SDL_calloc(1, (size_t)s->count + 16);

        if (s->count >= 64) {
            memcpy(v, "VAGp", 4);
            PutBe32(v + 4, 3);
            PutBe32(v + 12, (uint32_t)(s->count - 48));
            PutBe32(v + 16, (uint32_t)s->r2[0]);
            v[s->count - 16 + 1] = 1; /* the last block ends it */
        }
        Put(b, v, (size_t)s->count);
        SDL_free(v);
        break;
    }
    case SHAPE_WAD: {
        Buffer *parts = SDL_calloc((size_t)s->count, sizeof(Buffer));

        for (i = 0; i < s->count; i++) {
            PutStandIn(&parts[i], s->list[i].part);
        }
        PutWad(b, s, parts);
        break;
    }
    case SHAPE_RAW: {
        uint8_t *zeros = SDL_calloc(1, (size_t)s->count + 1);

        Put(b, zeros, (size_t)s->count);
        SDL_free(zeros);
        break;
    }
    default:
        break;
    }
}

static const Shape *ShapeOf(const char *path)
{
    int i;

    for (i = 0; saturnShapes[i].path; i++) {
        if (!strcmp(saturnShapes[i].path, path)) {
            return saturnShapes[i].shape;
        }
    }
    return NULL;
}

static uint32_t Le32(const uint8_t *p)
{
    return (uint32_t)(p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24);
}

/* The PC's video memory has its own arrangement: where the PC puts a picture, the PlayStation's track textures can
   be. A TIM of `size` bytes is moved to the rectangles the PlayStation's file of the same name has (its shape);
   returns whether it moved. */
static int PlaceTim(uint8_t *tim, size_t size, const Shape *s)
{
    size_t at = 8;
    int moved = 0;

    if (size < 20 || s->kind != SHAPE_TIM || s->a == 15) {
        return 0;
    }
    if (tim[4] & 8) {
        if (Le32(tim + 8) + 20 > size) {
            return 0;
        }
        moved = tim[12] != (s->r1[0] & 255) || tim[13] != (s->r1[0] >> 8) || tim[14] != (s->r1[1] & 255) ||
                tim[15] != (s->r1[1] >> 8);
        PutLe16(tim + 12, s->r1[0]); /* the palette's position */
        PutLe16(tim + 14, s->r1[1]);
        at += Le32(tim + 8);
    }
    moved |= tim[at + 4] != (s->r2[0] & 255) || tim[at + 5] != (s->r2[0] >> 8) || tim[at + 6] != (s->r2[1] & 255) ||
             tim[at + 7] != (s->r2[1] >> 8);
    PutLe16(tim + at + 4, s->r2[0]); /* the picture's */
    PutLe16(tim + at + 6, s->r2[1]);
    return moved;
}

/* A file of the PC's (a TIM, or a sequence of them) with its pictures moved to where the PlayStation's are, as far as
   the shape has pictures; the same file when they are there already. data is consumed. */
static uint8_t *PlaceFile(uint8_t *data, size_t *size, const Shape *shape)
{
    uint8_t *tims, *out, *packed;
    size_t header, total = 0, at = 0, packedSize;
    int count, i, moved = 0;

    if (shape->kind == SHAPE_TIM) {
        PlaceTim(data, *size, shape);
        return data;
    }
    if (shape->kind != SHAPE_CMP || *size < 4) {
        return data;
    }
    count = (int)Le32(data);
    header = 4 + (size_t)count * 4;
    if (count <= 0 || count > 4096 || header > *size) {
        return data;
    }
    for (i = 0; i < count; i++) {
        total += Le32(data + 4 + i * 4);
    }
    tims = SDL_malloc(total + 1);
    if (PortLzssDecompress(data + header, *size - header, tims, total) != total) {
        SDL_free(tims);
        return data;
    }
    for (i = 0; i < count; i++) {
        size_t timSize = Le32(data + 4 + i * 4);

        if (i < shape->count) {
            moved |= PlaceTim(tims + at, timSize, &shape->list[i]);
        }
        at += timSize;
    }
    if (!moved) {
        SDL_free(tims);
        return data;
    }
    packed = PortLzssStore(tims, total, &packedSize);
    out = SDL_malloc(header + packedSize);
    memcpy(out, data, header);
    memcpy(out + header, packed, packedSize);
    SDL_free(packed);
    SDL_free(tims);
    SDL_free(data);
    *size = header + packedSize;
    return out;
}

/* alphared.cmp, the Red Bull boards of track 1 as the PlayStation loads them: the PC has the picture as TH.TIM, so it
   is made a sequence of that one picture, at the PlayStation's first one's place. */
static uint8_t *MakeRedBull(const Shape *shape, size_t *size)
{
    size_t bytes = 0, packedSize;
    uint8_t *tim = PortPcLoadFile("TEXTURES\\TH.TIM", &bytes), *packed, *cmp;

    if (!tim) {
        return NULL;
    }
    packed = PortLzssStore(tim, bytes, &packedSize);
    cmp = SDL_malloc(8 + packedSize);
    PutLe32(cmp, 1);
    PutLe32(cmp + 4, (uint32_t)bytes);
    memcpy(cmp + 8, packed, packedSize);
    SDL_free(packed);
    SDL_free(tim);
    *size = 8 + packedSize;
    return PlaceFile(cmp, size, shape);
}

/* startwad.wad, which the PC never had: the PlayStation game keeps the weapons, start droid, explosion and light
   models and textures it loads at the start of a race in this one wad, and the PC has them as files of their own
   (COMMON, TEXTURES). Its parts are those files, in the wad's order; the HUD gauges and fonts the PC draws from its
   own pages are empty. */
static uint8_t *MakeStartWad(const Shape *shape, size_t *size)
{
    Buffer *parts = SDL_calloc((size_t)shape->count, sizeof(Buffer));
    Buffer wad = {0};
    int i;

    for (i = 0; i < shape->count; i++) {
        static const char *const folders[] = {"COMMON\\", "TEXTURES\\"};
        size_t bytes = 0;
        uint8_t *file = NULL;
        int f;

        for (f = 0; f < 2 && !file; f++) {
            char path[64];

            SDL_snprintf(path, sizeof(path), "%s%s", folders[f], shape->list[i].name);
            file = PortPcLoadFile(path, &bytes);
        }
        if (file) {
            file = PlaceFile(file, &bytes, shape->list[i].part);
            Put(&parts[i], file, bytes);
            SDL_free(file);
        } else {
            PutStandIn(&parts[i], shape->list[i].part);
        }
    }
    PutWad(&wad, shape, parts);
    *size = wad.size;
    return wad.data;
}

/* ---- the game's file layer ---- */

typedef struct Made {
    char path[64];
    uint8_t *data;
    size_t size;
} Made;

static Made *made;
static int madeCount, madeCapacity;

static int Remember(const char *path, uint8_t *data, size_t size)
{
    if (madeCount == madeCapacity) {
        madeCapacity = madeCapacity ? madeCapacity * 2 : 64;
        made = SDL_realloc(made, (size_t)madeCapacity * sizeof(Made));
    }
    SDL_strlcpy(made[madeCount].path, path, sizeof(made[madeCount].path));
    made[madeCount].data = data;
    made[madeCount].size = size;
    return madeCount++;
}

/* The file the game asks for (\wipeout2\common\vecto.prm), made the first time; -1 when there is nothing for it. */
static int Find(const char *path)
{
    size_t size = 0;
    uint8_t *data = NULL;
    const Shape *shape;
    int i;

    for (i = 0; i < madeCount; i++) {
        if (!strcmp(made[i].path, path)) {
            return i;
        }
    }
    if (!strncmp(path, "\\wipeout2\\", 10)) {
        data = PortPcLoadFile(path + 10, &size);
        if (data && (shape = ShapeOf(path))) {
            data = PlaceFile(data, &size, shape);
        }
    }
    if (!data && !strcmp(path, "\\wipeout2\\sound\\samples.wad")) {
        data = PcMakeSamples(&size);
    }
    if (!data && !strcmp(path, "\\wipeout2\\common\\startwad.wad") && (shape = ShapeOf(path))) {
        data = MakeStartWad(shape, &size);
    }
    if (!data && !strcmp(path, "\\wipeout2\\textures\\alphared.cmp") && (shape = ShapeOf(path))) {
        data = MakeRedBull(shape, &size);
    }
    if (!data && (shape = ShapeOf(path))) {
        Buffer b = {0};

        PutStandIn(&b, shape);
        data = b.data ? b.data : SDL_calloc(1, 1);
        size = b.size;
        PortLog("pc.c: %s is not on the PC CD: an empty one\n", path);
    }
    if (!data) {
        return -1;
    }
    return Remember(path, data, size);
}

int PortPcFileInfo(const char *pathname, int *loc, int *size)
{
    char path[64];
    const char *p = pathname;
    size_t i;
    int file;

    if (!PortPcData()) {
        return 0;
    }
    if (p[0] && p[1] == ':') {
        p += 2;
    }
    for (i = 0; p[i] && p[i] != ';' && i + 1 < sizeof(path); i++) {
        path[i] = (char)SDL_tolower((unsigned char)p[i]);
    }
    path[i] = 0;
    file = Find(path);
    if (file < 0) {
        PortLog("pc.c: no %s\n", path);
        return 0;
    }
    *loc = VIRTUAL_LOC + file * VIRTUAL_SPAN;
    *size = (int)made[file].size;
    return 1;
}

int PortPcRead(int loc, int sectors, void *buffer)
{
    int file = (loc - VIRTUAL_LOC) / VIRTUAL_SPAN, sector = (loc - VIRTUAL_LOC) % VIRTUAL_SPAN;
    size_t from = (size_t)sector * 2048, length = (size_t)sectors * 2048;

    if (!pc || loc < VIRTUAL_LOC || file >= madeCount) {
        return 0;
    }
    memset(buffer, 0, length);
    if (from < made[file].size) {
        memcpy(buffer, made[file].data + from, SDL_min(length, made[file].size - from));
    }
    return 1;
}
