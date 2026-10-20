/* Differential test of the C ports of the renderer's assembly routines against the retail machine code.

   For each scenario the same scene (track files, objects, camera) is built twice: as native C structures
   for the port in game/src, and as the console's 32-bit structures in the interpreter's RAM
   (tools/asmtest/mips.c).  The retail routine runs in the interpreter, the port natively; what each drew
   (the packets, where they were linked into the ordering table, the counters) is compared.  Both use the
   platform's GTE, so this tests the routines, not the GTE.

   usage: render_test EXE SYMBOLS DISCROOT [routine ...]     (routine: lo med hi)  */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "mips.h"
#include "functions.h"
#include "trackasm.h"
#include "track.h"
#include "port_widen.h"
#include "../../platform/psx/gte.h"

/* ---- what the port expects from the rest of the game ---- */
char errorString[256];
char msgInsufficientMemory[] = "Insufficient memory\n";
char msgDFreeNotFound[] = "DFree: block not found\n";
char msgAllocRequest[] = "Alloc request %d, free %d\n";
HeapBlock *heap, *mainheapcopy;
void Error(char *text, short level) { fprintf(stderr, "Error: %s\n", text); exit(2); }

short CurrentScreen;
TrackData *track;
u_long *OT[2];
POLY_FT4 *TrackLoPolys[2], *TrackMedPolys[2], *TrackHiPolys[2];
Surface tileSurfaces[24][2];
short loTileCount, medTileCount, hiTileCount;
PrimPtr nextPrim;

/* The port links its packets with AddPrim; the test records them instead. */
typedef struct Linked { int index; void *poly; } Linked;
#define MAX_LINKED 20000
static Linked linked[MAX_LINKED];
static int linkedCount;

void AddPrim(u_long *ot, void *p)
{
    linked[linkedCount].index = (int)(ot - OT[CurrentScreen]);
    linked[linkedCount].poly = p;
    linkedCount++;
}

/* ---- symbols of the console executable ---- */
static struct { char name[48]; uint32_t address; } symbols[4000];
static int symbolCount;

static void LoadSymbols(const char *path)
{
    FILE *f = fopen(path, "r");
    char line[256];

    if (!f) { perror(path); exit(2); }
    while (fgets(line, sizeof(line), f)) {
        char name[64];
        unsigned address;

        if (sscanf(line, "%63s = 0x%x", name, &address) == 2 && symbolCount < 4000) {
            strncpy(symbols[symbolCount].name, name, 47);
            symbols[symbolCount].address = address;
            symbolCount++;
        }
    }
    fclose(f);
}

static uint32_t Sym(const char *name)
{
    int i;

    for (i = 0; i < symbolCount; i++) {
        if (!strcmp(symbols[i].name, name)) {
            return symbols[i].address;
        }
    }
    fprintf(stderr, "no symbol %s\n", name);
    exit(2);
}

/* ---- emulated RAM allocator (above the program's image and bss, below the stack) ---- */
static uint32_t emuNext = 0x80110000u;

static uint32_t EmuAlloc(uint32_t size)
{
    uint32_t a = (emuNext + 7) & ~7u;

    emuNext = a + size;
    if (emuNext > 0x801F0000u) {
        fprintf(stderr, "out of emulated RAM\n");
        exit(2);
    }
    return a;
}

/* ---- the track's files ---- */
static unsigned char *ReadFile(const char *path, long *size)
{
    FILE *f = fopen(path, "rb");
    unsigned char *data;

    if (!f) { perror(path); exit(2); }
    fseek(f, 0, SEEK_END);
    *size = ftell(f);
    fseek(f, 0, SEEK_SET);
    data = malloc((size_t)*size + 32);
    if (fread(data, 1, (size_t)*size, f) != (size_t)*size) { perror(path); exit(2); }
    fclose(f);
    return data;
}

typedef struct Wad { unsigned char *data; int count; struct { char name[17]; long offset, size; } entry[64]; } Wad;

static void OpenWad(Wad *wad, const char *path)
{
    long size;
    long pos;
    int i;

    wad->data = ReadFile(path, &size);
    wad->count = wad->data[0] | wad->data[1] << 8;
    pos = 2 + 25L * wad->count;
    for (i = 0; i < wad->count; i++) {
        unsigned char *e = wad->data + 2 + 25L * i;

        memcpy(wad->entry[i].name, e, 16);
        wad->entry[i].name[16] = 0;
        wad->entry[i].size = e[20] | e[21] << 8 | e[22] << 16 | (long)e[23] << 24;
        wad->entry[i].offset = pos;
        pos += wad->entry[i].size;
    }
}

static unsigned char *WadFile(Wad *wad, const char *name, long *size)
{
    int i;

    for (i = 0; i < wad->count; i++) {
        if (!strcmp(wad->entry[i].name, name)) {
            *size = wad->entry[i].size;
            return wad->data + wad->entry[i].offset;
        }
    }
    fprintf(stderr, "no %s in wad\n", name);
    exit(2);
}

static void Swap16(unsigned char *p) { unsigned char t = p[0]; p[0] = p[1]; p[1] = t; }
static void Swap32(unsigned char *p) { unsigned char t = p[0]; p[0] = p[3]; p[3] = t; t = p[1]; p[1] = p[2]; p[2] = t; }

typedef struct World {
    int vertexCount, faceCount, sectionCount;
    unsigned char *vertexBytes, *faceBytes, *sectionBytes; /* console layout, byte swapped, as the loaders leave them */
    TrackData native;
    TrackSection *sections;
    uint32_t emuTrack, emuVertices, emuFaces, emuSections;
} World;

static void BuildWorld(World *w, const char *discRoot, int trackNumber)
{
    char path[512];
    Wad wad;
    long size;
    unsigned char *src;
    int i, j;

    snprintf(path, sizeof(path), "%s/TRACK%02d/TRACK.WAD", discRoot, trackNumber);
    OpenWad(&wad, path);

    src = WadFile(&wad, "track.trv", &size);
    w->vertexCount = (int)(size / 16);
    w->vertexBytes = malloc((size_t)size);
    memcpy(w->vertexBytes, src, (size_t)size);
    for (i = 0; i < w->vertexCount; i++) {
        Swap32(w->vertexBytes + 16 * i); Swap32(w->vertexBytes + 16 * i + 4); Swap32(w->vertexBytes + 16 * i + 8);
    }
    src = WadFile(&wad, "track.trf", &size);
    w->faceCount = (int)(size / 20);
    w->faceBytes = malloc((size_t)size);
    memcpy(w->faceBytes, src, (size_t)size);
    for (i = 0; i < w->faceCount; i++) {
        for (j = 0; j < 7; j++) {
            Swap16(w->faceBytes + 20 * i + 2 * j);   /* vertex[4], normal x y z */
        }
    }
    src = WadFile(&wad, "track.trs", &size);
    w->sectionCount = (int)(size / 156);
    w->sectionBytes = malloc((size_t)size);
    memcpy(w->sectionBytes, src, (size_t)size);
    for (i = 0; i < w->sectionCount; i++) {
        unsigned char *r = w->sectionBytes + 156 * i;

        Swap32(r); Swap32(r + 4); Swap32(r + 8); Swap32(r + 12); Swap32(r + 16); Swap32(r + 20);
        Swap16(r + 24); Swap16(r + 26);
        for (j = 0; j < 15; j++) Swap16(r + 96 + 2 * j);   /* the five view counts, three levels each */
        for (j = 0; j < 8; j++) Swap16(r + 126 + 2 * j);   /* high[4], med[4] */
        for (j = 0; j < 6; j++) Swap16(r + 142 + 2 * j);   /* faceStart faceCount radii flags secNo */
    }

    /* the native world */
    w->native.vertexCount = w->vertexCount;
    w->native.faceCount = w->faceCount;
    w->native.sectionCount = w->sectionCount;
    w->native.vertices = (VECTOR *)w->vertexBytes;
    w->native.faces = (Face *)w->faceBytes;
    w->sections = malloc((size_t)w->sectionCount * sizeof(TrackSection) + 32);
    PortWidenRecords(&PortRecord_TrackSection, w->sectionBytes, w->sectionCount, w->sections);
    w->native.sections = w->sections;
    w->native.viewList = 0;
    w->native.colCopy = 0;

    /* the console's world */
    w->emuVertices = EmuAlloc((uint32_t)w->vertexCount * 16);
    memcpy(MipsPointer(w->emuVertices), w->vertexBytes, (size_t)w->vertexCount * 16);
    w->emuFaces = EmuAlloc((uint32_t)w->faceCount * 20);
    memcpy(MipsPointer(w->emuFaces), w->faceBytes, (size_t)w->faceCount * 20);
    w->emuSections = EmuAlloc((uint32_t)w->sectionCount * 156);
    memcpy(MipsPointer(w->emuSections), w->sectionBytes, (size_t)w->sectionCount * 156);
    w->emuTrack = EmuAlloc(32);
    MipsStore32(w->emuTrack + 0, (uint32_t)w->vertexCount);
    MipsStore32(w->emuTrack + 4, (uint32_t)w->faceCount);
    MipsStore32(w->emuTrack + 8, (uint32_t)w->sectionCount);
    MipsStore32(w->emuTrack + 12, w->emuVertices);
    MipsStore32(w->emuTrack + 16, w->emuFaces);
    MipsStore32(w->emuTrack + 20, w->emuSections);
    MipsStore32(Sym("track"), w->emuTrack);
    track = &w->native;
}

/* ---- random numbers ---- */
static uint32_t rngState = 12345;
static uint32_t Rand(void) { rngState = rngState * 1664525u + 1013904223u; return rngState >> 8; }
static int RandRange(int lo, int hi) { return lo + (int)(Rand() % (uint32_t)(hi - lo + 1)); }

/* ---- the camera: a skeleton with a parent ---- */
typedef struct Camera { Skeleton native[2]; uint32_t emu[2]; } Camera;

static void SkeletonRandom(Skeleton *s, int translate[3])
{
    s->relative.t[0] = translate[0];
    s->relative.t[1] = translate[1];
    s->relative.t[2] = translate[2];
    SetSkeletonDirection(s, (short)RandRange(0, 4095), (short)RandRange(0, 4095), (short)RandRange(0, 4095));
}

static uint32_t MarshalSkeleton(Skeleton *s, uint32_t superAddress)
{
    uint32_t a = EmuAlloc(80);
    unsigned char file[80];

    s->update = 1;
    PortNarrowRecords(&PortRecord_Skeleton, s, 1, file);
    memcpy(MipsPointer(a), file, 80);
    MipsStore32(a + 68, superAddress);
    return a;
}

static void GteSetup(void)
{
    memset(&gte, 0, sizeof(gte));
    InitGeom();
    SetGeomOffset(160, 120);
    SetGeomScreen(300);
}

/* ---- comparing what was drawn ---- */
static int failures, checks;
static long totalPackets;

static void Fail(const char *what, int a, int b)
{
    failures++;
    if (failures <= 20) {
        printf("FAIL %s: retail %d, port %d\n", what, a, b);
    }
}

/* Compares the packets linked by the port with the chains in the console's ordering table. */
static void CompareDrawn(const char *label, uint32_t emuOt, uint32_t emuPool, void *nativePool, int packetSize, int count)
{
    static int expectedFirst[4096];
    int i, n = 0;

    /* The console side: walk the chains. */
    for (i = 0; i < 0x2000; i++) {
        uint32_t entry = MipsLoad32(emuOt + 4u * (uint32_t)i) & 0xFFFFFF;
        int k = 0;

        while (entry) {
            uint32_t address = 0x80000000u | entry;
            uint32_t slot = (address - emuPool);
            int j;
            int found = -1;
            Linked *l;

            /* find the port's k-th packet of this ordering table entry: the port inserted at the head, so the
               chain order is the reverse of the insertion order */
            {
                int seen = 0;
                for (j = linkedCount - 1; j >= 0; j--) {
                    if (linked[j].index == i) {
                        if (seen == k) { found = j; break; }
                        seen++;
                    }
                }
            }
            if (found < 0) {
                Fail("packet in the console's table that the port did not link", i, (int)slot);
                return;
            }
            l = &linked[found];
            if ((int)((char *)l->poly - (char *)nativePool) != (int)slot) {
                Fail("packet position", (int)slot, (int)((char *)l->poly - (char *)nativePool));
            }
            /* the packet bytes except the tag */
            if (memcmp(MipsPointer(address) + 4, (unsigned char *)l->poly + 4, (size_t)packetSize - 4) != 0) {
                int k;

                for (k = 4; k < packetSize; k++) {
                    if (MipsPointer(address)[k] != ((unsigned char *)l->poly)[k]) break;
                }
                if (failures < 20) {
                    printf("  %s offset %d byte %d: retail %02x port %02x\n", label, (int)slot, k, MipsPointer(address)[k], ((unsigned char *)l->poly)[k]);
                }
                Fail("packet contents", (int)slot, 0);
            }
            n++;
            k++;
            entry = MipsLoad32(address) & 0xFFFFFF;
        }
    }
    (void)expectedFirst;
    totalPackets += n;
    if (n != linkedCount) {
        Fail("number of linked packets", n, linkedCount);
    }
    (void)label; (void)count;
    checks++;
}

/* ---- the scenarios ---- */
static void ClearOts(uint32_t emuOt)
{
    int i;

    for (i = 0; i < 0x2000; i++) {
        MipsStore32(emuOt + 4u * (uint32_t)i, 0);
    }
}

static void InitPool(POLY_FT4 *native, uint32_t emu, int count)
{
    int i;

    for (i = 0; i < count; i++) {
        memset(&native[i], 0, sizeof(POLY_FT4));
        SetPolyFT4(&native[i]);
        MipsStore32(emu + 40u * (uint32_t)i, 0);
        memcpy(MipsPointer(emu + 40u * (uint32_t)i) + 3, (unsigned char *)&native[i] + 3, 1);
        memcpy(MipsPointer(emu + 40u * (uint32_t)i) + 7, (unsigned char *)&native[i] + 7, 1);
    }
}

#define POOL 0x1000
/* level: 0 low, 1 medium, 2 high.  The high routine also takes packets from the general buffer (nextPrim), which
   the test puts in the second half of the pool. */
static void RunTrack(World *w, int trials, int level)
{
    static POLY_FT4 nativePool[2 * POOL];
    uint32_t emuPool = EmuAlloc(2 * POOL * 40), emuOt = EmuAlloc(0x2000 * 4);
    const char *routine = level == 2 ? "AsmTrackHi" : level ? "AsmTrackMed" : "AsmTrackLo";
    const char *poolSymbol = level == 2 ? "TrackHiPolys" : level ? "TrackMedPolys" : "TrackLoPolys";
    const char *counterSymbol = level == 2 ? "hiTileCount" : level ? "medTileCount" : "loTileCount";
    short *counter = level == 2 ? &hiTileCount : level ? &medTileCount : &loTileCount;
    uint32_t cameraSym = 0;
    int t, i;
    static u_long nativeOt[0x2000];

    (void)cameraSym;
    OT[0] = nativeOt;
    OT[1] = nativeOt;
    TrackLoPolys[0] = TrackLoPolys[1] = TrackMedPolys[0] = TrackMedPolys[1] = TrackHiPolys[0] = TrackHiPolys[1] = nativePool;
    MipsStore32(Sym("OT"), emuOt);
    MipsStore32(Sym("OT") + 4, emuOt);
    MipsStore32(Sym(poolSymbol), emuPool);
    MipsStore32(Sym(poolSymbol) + 4, emuPool);
    MipsStore16(Sym("CurrentScreen"), 0);
    CurrentScreen = 0;

    /* random texture tiles, the same in both worlds (the face's texture and flip select one of 32x2 surfaces) */
    for (t = 0; t < (int)sizeof(tileSurfaces); t++) {
        unsigned char b = (unsigned char)Rand();

        ((unsigned char *)tileSurfaces)[t] = b;
        MipsStore8(Sym("tileSurfaces") + (uint32_t)t, b);
    }
    for (t = 0; t < trials; t++) {
        int sectionIndex = RandRange(0, w->sectionCount - 3);
        int count = RandRange(1, 3);
        int spread;
        int translate[3], superTranslate[3] = { 0, 0, 0 };
        Camera cam;
        TrackSection *s = &w->sections[sectionIndex];
        uint32_t emuSuper, emuCamera;
        uint32_t result;

        spread = (t % 3 == 0) ? 2500 : (t % 3 == 1) ? 600 : 150;   /* the near cameras cut faces by the screen edge or the near plane */
        translate[0] = -s->centre.vx + RandRange(-spread, spread);
        translate[1] = -s->centre.vy + RandRange(-spread, spread);
        translate[2] = -s->centre.vz + RandRange(-spread, spread);
        memset(&cam, 0, sizeof(cam));
        SkeletonRandom(&cam.native[1], superTranslate);    /* the parent: rotation only */
        SkeletonRandom(&cam.native[0], translate);
        cam.native[0].super = &cam.native[1];
        emuSuper = MarshalSkeleton(&cam.native[1], 0);
        emuCamera = MarshalSkeleton(&cam.native[0], emuSuper);

        ClearOts(emuOt);
        memset(nativeOt, 0, sizeof(nativeOt));
        InitPool(nativePool, emuPool, POOL);
        for (i = POOL; i < 2 * POOL; i++) { MipsStore32(emuPool + 40u * (uint32_t)i + 4, 0); }
        *counter = (short)RandRange(0, 20);
        MipsStore16(Sym(counterSymbol), (uint16_t)*counter);
        linkedCount = 0;
        nextPrim.ptr = (char *)(nativePool + POOL);
        MipsStore32(Sym("nextPrim"), emuPool + 40u * POOL);
        memset(nativePool + POOL, 0, POOL * sizeof(POLY_FT4));

        GteSetup();
        result = MipsCall(Sym(routine), w->emuSections + 156u * (uint32_t)sectionIndex, emuCamera, level == 2 ? 0 : (uint32_t)count, level == 2 ? (uint32_t)count : 0, 0, 0);
        (void)result;
        GteSetup();
        if (level == 2) AsmTrackHi(&w->sections[sectionIndex], &cam.native[0], 0, (short)count);
        else if (level) AsmTrackMed(&w->sections[sectionIndex], &cam.native[0], (short)count);
        else AsmTrackLo(&w->sections[sectionIndex], &cam.native[0], (short)count);

        if ((short)MipsLoad16(Sym(counterSymbol)) != *counter) {
            Fail(counterSymbol, (short)MipsLoad16(Sym(counterSymbol)), *counter);
        }
        if (level == 2 && MipsLoad32(Sym("nextPrim")) - emuPool != (uint32_t)((POLY_FT4 *)nextPrim.ptr - nativePool) * 40u) {
            Fail("nextPrim", (int)(MipsLoad32(Sym("nextPrim")) - emuPool) / 40, (int)((POLY_FT4 *)nextPrim.ptr - nativePool));
        }
        CompareDrawn(routine, emuOt, emuPool, nativePool, 40, 0);
    }
}

/* ---- particles ---- */
#include "sparks.h"

void SetLineG2(LINE_G2 *p) { ((unsigned char *)p)[3] = 4; ((unsigned char *)p)[7] = 0x50; }
void SetSemiTrans(void *p, int abe) { ((unsigned char *)p)[7] = (unsigned char)((((unsigned char *)p)[7] & ~2) | (abe ? 2 : 0)); }

static void RunParticles(World *w, int trials)
{
    uint32_t emuTable = EmuAlloc(128 * 0x44), emuOt = EmuAlloc(0x2000 * 4);
    static SParticle nativeTable[128];
    static u_long nativeOt[0x2000];
    int t, i;

    OT[0] = OT[1] = nativeOt;
    MipsStore32(Sym("OT"), emuOt);
    MipsStore32(Sym("OT") + 4, emuOt);
    for (t = 0; t < trials; t++) {
        int mode = t % 3 == 2 ? 1 : 0;
        int translate[3], superTranslate[3] = { 0, 0, 0 };
        Camera cam;
        uint32_t emuSuper, emuCamera;
        unsigned char *bytes = (unsigned char *)nativeTable;
        int spread = (t % 4 == 0) ? 20000 : 3000;

        CurrentScreen = (short)(t & 1);
        MipsStore16(Sym("CurrentScreen"), (uint16_t)CurrentScreen);
        translate[0] = RandRange(-spread, spread);
        translate[1] = RandRange(-spread, spread);
        translate[2] = RandRange(-spread, spread);
        memset(&cam, 0, sizeof(cam));
        SkeletonRandom(&cam.native[1], superTranslate);
        SkeletonRandom(&cam.native[0], translate);
        cam.native[0].super = &cam.native[1];
        emuSuper = MarshalSkeleton(&cam.native[1], 0);
        emuCamera = MarshalSkeleton(&cam.native[0], emuSuper);

        memset(nativeTable, 0, sizeof(nativeTable));
        for (i = 0; i < 128; i++) {
            SParticle *p = &nativeTable[i];

            p->type = (char)(RandRange(0, 8) > 6 ? 0 : RandRange(0, 6));
            p->flags = (char)RandRange(0, 2);
            p->vel.vx = (short)RandRange(-300, 300);
            p->vel.vy = (short)RandRange(-300, 300);
            p->vel.vz = (short)RandRange(-300, 300);
            p->pos.vx = RandRange(-spread, spread);
            p->pos.vy = RandRange(-spread, spread);
            p->pos.vz = RandRange(-spread, spread) + 1500;
            p->pos.pad = (int)Rand();
            p->life = RandRange(-3, 40);
            /* the packets start as leftovers: random bytes */
            { int k; for (k = 0; k < 40; k++) bytes[i * 0x44 + 0x1C + k] = (unsigned char)Rand(); }
        }
        memcpy(MipsPointer(emuTable), nativeTable, sizeof(nativeTable));
        /* the console record is the same bytes (no pointers in it) */
        ClearOts(emuOt);
        memset(nativeOt, 0, sizeof(nativeOt));
        linkedCount = 0;

        GteSetup();
        MipsCall(Sym("AsmDrawParticles"), emuTable, emuCamera, (uint32_t)mode, 0, 0, 0);
        GteSetup();
        AsmDrawParticles(nativeTable, &cam.native[0], mode);

        /* the table afterwards, except the tag words of the packets (they hold links) */
        for (i = 0; i < 128; i++) {
            unsigned char *a = MipsPointer(emuTable + 0x44u * (uint32_t)i), *b = bytes + 0x44 * i;
            int k, bad = 0;

            for (k = 0; k < 0x44; k++) {
                int inTag = (k >= 0x1C && k < 0x1F) || (k >= 0x30 && k < 0x33);

                if (!inTag && a[k] != b[k]) { bad = k + 1; break; }
            }
            if (bad) {
                if (failures < 20) printf("  particle %d byte 0x%x: retail %02x port %02x\n", i, bad - 1, a[bad - 1], b[bad - 1]);
                Fail("particle record", i, bad - 1);
            }
        }
        CompareDrawn("AsmDrawParticles", emuOt, emuTable, nativeTable, 20, 0);
    }
    (void)w;
}

/* ---- objects ---- */
Texture *textureTable[800];

/* The size of the packet the retail code builds, from its code byte. */
static int PacketSizeByCode(int code)
{
    switch (code & ~2) {
    case 0x20: return 0x14;
    case 0x24: return 0x20;
    case 0x28: return 0x18;
    case 0x2C: return 0x28;
    case 0x30: return 0x1C;
    case 0x34: return 0x28;
    case 0x38: return 0x24;
    case 0x3C: return 0x34;
    }
    return 0;
}

static long objectPackets[3];
static const int primitiveSize[24] = { 0, 0x10, 0x1C, 0x10, 0x20, 0x18, 0x24, 0x1C, 0x2C, 0, 0x10, 0x10 };

static void RunObjects(World *w, int trials, int kind)
{
    static u_long nativeOt[0x2000];
    static char nativeBuffer[0x10000];
    static SVECTOR nativeVertices[400];
    static unsigned char nativePrims[0x4000];
    static CVECTOR nativeColour;
    uint32_t emuBuffer = EmuAlloc(0x10000), emuOt = EmuAlloc(0x2000 * 4), emuVertices = EmuAlloc(400 * 8);
    uint32_t emuPrims = EmuAlloc(0x4000), emuObject = EmuAlloc(0x40), emuColour = EmuAlloc(8);
    uint32_t emuTextures = EmuAlloc(8 * 0x30), emuTableSym = Sym("textureTable");
    static Texture nativeTextures[8];
    const char *routine = kind == 0 ? "AsmDrawObject" : kind == 1 ? "AsmDrawObjectLit" : "AsmDrawObjectFixed";
    int t, i, k;
    static const int vertexCounts[] = { 3, 9, 40, 127, 129, 250, 1 };

    OT[0] = OT[1] = nativeOt;
    MipsStore32(Sym("OT"), emuOt);
    MipsStore32(Sym("OT") + 4, emuOt);
    /* textures for the sprites */
    for (i = 0; i < 8; i++) {
        short *f = (short *)&nativeTextures[i];

        for (k = 0; k < (int)(sizeof(Texture) / 2); k++) f[k] = (short)(Rand() & 0x1FF);
        for (k = 0; k < (int)(sizeof(Texture)); k++) MipsStore8(emuTextures + 0x30u * (uint32_t)i + (uint32_t)k, ((unsigned char *)&nativeTextures[i])[k]);
        textureTable[i] = &nativeTextures[i];
        MipsStore32(emuTableSym + 4u * (uint32_t)i, emuTextures + 0x30u * (uint32_t)i);
    }
    for (t = 0; t < trials; t++) {
        int vertexCount = vertexCounts[t % 7];
        int primitiveCount = RandRange(1, 40);
        int translate[3], superTranslate[3] = { 0, 0, 0 };
        Camera cam;
        uint32_t emuSuper, emuSkeleton;
        Object object;
        int spread = (t % 3 == 0) ? 3500 : (t % 3 == 1) ? 1200 : 300;
        int depth = (t % 5 == 0) ? RandRange(300, 1500) : RandRange(1800, 9000);
        int offset = 0;
        int roll = RandRange(0, 4095);

        translate[0] = RandRange(-1500, 1500);
        translate[1] = RandRange(-1500, 1500);
        translate[2] = depth;
        memset(&cam, 0, sizeof(cam));
        SkeletonRandom(&cam.native[1], superTranslate);
        SkeletonRandom(&cam.native[0], translate);
        cam.native[0].super = &cam.native[1];
        emuSuper = MarshalSkeleton(&cam.native[1], 0);
        emuSkeleton = MarshalSkeleton(&cam.native[0], emuSuper);

        for (i = 0; i < vertexCount; i++) {
            nativeVertices[i].vx = (short)RandRange(-spread, spread);
            nativeVertices[i].vy = (short)RandRange(-spread, spread);
            nativeVertices[i].vz = (short)RandRange(-spread, spread);
            nativeVertices[i].pad = (short)Rand();
        }
        memset(nativePrims, 0, sizeof(nativePrims));
        for (i = 0; i < primitiveCount; i++) {
            int type = RandRange(1, 11);
            unsigned char *p;
            int size;

            if (type == 9) type = 6;
            if (i == primitiveCount - 1 && t % 4 == 0) type = RandRange(12, 23);   /* ends the object */
            size = primitiveSize[type] ? primitiveSize[type] : 0x10;
            p = nativePrims + offset;
            for (k = 0; k < size; k++) p[k] = (unsigned char)Rand();
            *(short *)p = (short)type;
            *(short *)(p + 2) = (short)(RandRange(0, 7));       /* flag: single sided / translucent bits */
            if (type == 10 || type == 11) {
                *(short *)(p + 4) = (short)RandRange(0, vertexCount - 1);
                *(short *)(p + 6) = (short)RandRange(-100, 600);
                *(short *)(p + 8) = (short)RandRange(-100, 600);
                *(short *)(p + 10) = (short)RandRange(0, 7);
            } else {
                int n = (type == 1 || type == 2 || type == 5 || type == 6) ? 3 : 4;

                if (type == 5 || type == 6 || type == 1 || type == 2) n = 3;
                if (type == 3 || type == 4 || type == 7 || type == 8) n = 4;
                for (k = 0; k < n; k++) *(short *)(p + 4 + 2 * k) = (short)RandRange(0, vertexCount - 1);
            }
            if (type > 11) size = 0x10;
            offset += size;
        }
        memset(&object, 0, sizeof(object));
        object.vertexCount = (short)vertexCount;
        object.vertices = nativeVertices;
        object.primitiveCount = (short)primitiveCount;
        object.primitives = (Primitive *)nativePrims;
        object.skeleton = &cam.native[0];
        nativeColour.r = (u_char)Rand(); nativeColour.g = (u_char)Rand(); nativeColour.b = (u_char)Rand(); nativeColour.cd = (u_char)Rand();

        memcpy(MipsPointer(emuVertices), nativeVertices, (size_t)vertexCount * 8);
        memcpy(MipsPointer(emuPrims), nativePrims, (size_t)offset + 0x40);
        memset(MipsPointer(emuObject), 0, 0x40);
        MipsStore16(emuObject + 0x10, (uint16_t)vertexCount);
        MipsStore32(emuObject + 0x14, emuVertices);
        MipsStore16(emuObject + 0x20, (uint16_t)primitiveCount);
        MipsStore32(emuObject + 0x24, emuPrims);
        MipsStore32(emuObject + 0x30, emuSkeleton);
        memcpy(MipsPointer(emuColour), &nativeColour, 4);

        ClearOts(emuOt);
        memset(nativeOt, 0, sizeof(nativeOt));
        memset(nativeBuffer, 0, sizeof(nativeBuffer));
        memset(MipsPointer(emuBuffer), 0, 0x10000);
        linkedCount = 0;
        CurrentScreen = (short)(t & 1);
        MipsStore16(Sym("CurrentScreen"), (uint16_t)CurrentScreen);
        MipsStore32(Sym("nextPrim"), emuBuffer);
        nextPrim.ptr = nativeBuffer;

        GteSetup();
        MipsCall(Sym(routine), emuObject, 0, kind == 2 ? 8191u : (uint32_t)roll, kind == 1 ? emuColour : 0, 0, 0);
        GteSetup();
        if (kind == 0) AsmDrawObject(&object, 0, roll);
        else if (kind == 1) AsmDrawObjectLit(&object, 0, roll, &nativeColour);
        else AsmDrawObjectFixed(&object, 0, 8191);

        /* where the buffer ended, and its contents packet by packet */
        {
            uint32_t endEmu = MipsLoad32(Sym("nextPrim")) - emuBuffer;
            int endNative = (int)(nextPrim.ptr - nativeBuffer);
            int pos = 0;

            if ((int)endEmu != endNative) {
                Fail("end of the primitive buffer", (int)endEmu, endNative);
            } else {
                while (pos < endNative) {
                    unsigned char *a = MipsPointer(emuBuffer + (uint32_t)pos), *b = (unsigned char *)nativeBuffer + pos;
                    int size = PacketSizeByCode(a[7]), m;

                    if (!size) { Fail("packet code", a[7], b[7]); break; }
                    for (m = 3; m < size; m++) {
                        if (a[m] != b[m]) {
                            if (failures < 20) printf("  %s trial %d packet at %d (code %02x) byte %d: retail %02x port %02x\n", routine, t, pos, a[7], m, a[m], b[m]);
                            Fail("packet contents", pos, m);
                            break;
                        }
                    }
                    pos += size;
                }
            }
        }
        objectPackets[kind] += linkedCount;
        CompareDrawn(routine, emuOt, emuBuffer, nativeBuffer, 4, 0);
    }
    (void)w;
}

int main(int argc, char **argv)
{
    World world;
    int i, trials = 200;
    int doLo = 1;

    if (argc < 4) { fprintf(stderr, "usage: render_test EXE SYMBOLS DISCROOT [lo]\n"); return 2; }
    if (MipsLoadExe(argv[1])) { fprintf(stderr, "cannot load %s\n", argv[1]); return 2; }
    LoadSymbols(argv[2]);
    (void)doLo;
    for (i = 4; i < argc; i++) { (void)argv[i]; }
    {
        int trackNumbers[] = { 1, 2, 6, 7, 8, 13, 17, 20 };
        int k;

        for (k = 0; k < 8; k++) {
            if (!MipsLoadExe(argv[1])) { }
            emuNext = 0x80110000u;
            BuildWorld(&world, argv[3], trackNumbers[k]);
            {
                uint32_t afterWorld = emuNext;
                int level;

                for (level = 0; level < 3; level++) {
                    emuNext = afterWorld;
                    RunTrack(&world, trials, level);
                }
                emuNext = afterWorld;
                RunParticles(&world, trials);
                for (level = 0; level < 3; level++) {
                    emuNext = afterWorld;
                    RunObjects(&world, trials, level);
                }
            }
            printf("track %d: %d trials each, %d failures so far\n", trackNumbers[k], trials, failures);
        }
    }
    printf("object packets drawn: plain %ld, lit %ld, fixed %ld\n", objectPackets[0], objectPackets[1], objectPackets[2]);
    printf("%d checks, %ld packets compared, %d failures\n", checks, totalPackets, failures);
    return failures != 0;
}

/* libgpu's packet initialiser, without the rest of the library */
void SetPolyFT4(POLY_FT4 *p) { ((unsigned char *)p)[3] = 9; ((unsigned char *)p)[7] = 0x2C; }
int lowTideMark;
char msgSkeletonAllocFailed[] = "NewSkeleton failed\n";
