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

    return disc && DiscFindFile(disc, "\\COMMON\\VECTO.PRM", &lba, &size) &&
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
        uint8_t count[2];

        for (i = 0; i < s->count; i++) {
            PutStandIn(&parts[i], s->list[i].part);
        }
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
    }
    if (!data && !strcmp(path, "\\wipeout2\\sound\\samples.wad")) {
        data = PcMakeSamples(&size);
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
