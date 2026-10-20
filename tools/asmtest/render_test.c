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
Surface tileSurfaces[32][2];
short loTileCount, medTileCount, hiTileCount;

/* The port links its packets with AddPrim; the test records them instead. */
typedef struct Linked { int index; POLY_FT4 *poly; } Linked;
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
static void CompareDrawn(const char *label, uint32_t emuOt, uint32_t emuPool, POLY_FT4 *nativePool, int firstSlot, int count)
{
    static int expectedFirst[4096];
    int i, n = 0;

    /* The console side: walk the chains. */
    for (i = 0; i < 0x2000; i++) {
        uint32_t entry = MipsLoad32(emuOt + 4u * (uint32_t)i) & 0xFFFFFF;
        int k = 0;

        while (entry) {
            uint32_t address = 0x80000000u | entry;
            uint32_t slot = (address - emuPool) / 40;
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
            if ((int)(l->poly - nativePool) != (int)slot) {
                Fail("packet slot", (int)slot, (int)(l->poly - nativePool));
            }
            /* the packet bytes except the tag */
            if (memcmp(MipsPointer(address) + 4, (unsigned char *)l->poly + 4, 36) != 0) {
                Fail("packet contents", (int)slot, 0);
            }
            n++;
            k++;
            entry = MipsLoad32(address) & 0xFFFFFF;
        }
    }
    (void)expectedFirst; (void)firstSlot;
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
static void RunTrack(World *w, int trials, int medium)
{
    static POLY_FT4 nativePool[POOL];
    uint32_t emuPool = EmuAlloc(POOL * 40), emuOt = EmuAlloc(0x2000 * 4);
    const char *routine = medium ? "AsmTrackMed" : "AsmTrackLo";
    const char *poolSymbol = medium ? "TrackMedPolys" : "TrackLoPolys";
    const char *counterSymbol = medium ? "medTileCount" : "loTileCount";
    short *counter = medium ? &medTileCount : &loTileCount;
    uint32_t cameraSym = 0;
    int t;
    static u_long nativeOt[0x2000];

    (void)cameraSym;
    OT[0] = nativeOt;
    OT[1] = nativeOt;
    TrackLoPolys[0] = TrackLoPolys[1] = TrackMedPolys[0] = TrackMedPolys[1] = nativePool;
    MipsStore32(Sym("OT"), emuOt);
    MipsStore32(Sym("OT") + 4, emuOt);
    MipsStore32(Sym(poolSymbol), emuPool);
    MipsStore32(Sym(poolSymbol) + 4, emuPool);
    MipsStore16(Sym("CurrentScreen"), 0);
    CurrentScreen = 0;

    for (t = 0; t < trials; t++) {
        int sectionIndex = RandRange(0, w->sectionCount - 3);
        int count = RandRange(1, 3);
        int translate[3], superTranslate[3] = { 0, 0, 0 };
        Camera cam;
        TrackSection *s = &w->sections[sectionIndex];
        uint32_t emuSuper, emuCamera;
        uint32_t result;

        translate[0] = -s->centre.vx + RandRange(-2500, 2500);
        translate[1] = -s->centre.vy + RandRange(-2500, 2500);
        translate[2] = -s->centre.vz + RandRange(-2500, 2500);
        memset(&cam, 0, sizeof(cam));
        SkeletonRandom(&cam.native[1], superTranslate);    /* the parent: rotation only */
        SkeletonRandom(&cam.native[0], translate);
        cam.native[0].super = &cam.native[1];
        emuSuper = MarshalSkeleton(&cam.native[1], 0);
        emuCamera = MarshalSkeleton(&cam.native[0], emuSuper);

        ClearOts(emuOt);
        memset(nativeOt, 0, sizeof(nativeOt));
        InitPool(nativePool, emuPool, POOL);
        *counter = (short)RandRange(0, 20);
        MipsStore16(Sym(counterSymbol), (uint16_t)*counter);
        linkedCount = 0;

        GteSetup();
        result = MipsCall(Sym(routine), w->emuSections + 156u * (uint32_t)sectionIndex, emuCamera, (uint32_t)count, 0, 0, 0);
        (void)result;
        GteSetup();
        if (medium) AsmTrackMed(&w->sections[sectionIndex], &cam.native[0], (short)count);
        else AsmTrackLo(&w->sections[sectionIndex], &cam.native[0], (short)count);

        if ((short)MipsLoad16(Sym(counterSymbol)) != *counter) {
            Fail(counterSymbol, (short)MipsLoad16(Sym(counterSymbol)), *counter);
        }
        CompareDrawn("lo", emuOt, emuPool, nativePool, 0, 0);
    }
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
            RunTrack(&world, trials, 0);
            RunTrack(&world, trials, 1);
            printf("track %d: %d trials each, %d failures so far\n", trackNumbers[k], trials, failures);
        }
    }
    printf("%d checks, %ld packets compared, %d failures\n", checks, totalPackets, failures);
    return failures != 0;
}

/* libgpu's packet initialiser, without the rest of the library */
void SetPolyFT4(POLY_FT4 *p) { ((unsigned char *)p)[3] = 9; ((unsigned char *)p)[7] = 0x2C; }
int lowTideMark;
char msgSkeletonAllocFailed[] = "NewSkeleton failed\n";
