/* Runs the game's own loaders on the PlayStation game's data and measures its heaps.

   The real PsxMain (up to its menu loop), MenuControl (up to its menu loop) and race (up to its main loop)
   are linked with the rest of the game's sources and run on the disc directory; the stand-ins for the
   hardware and the CD file system are in loadtest_standins.c, the build is tools/recordtest/build_loadtest.py.
   The game's heap calls go through the recorder below, which

     - runs the real DAlloc/DFree (native: 32-byte headers, 8-byte blocks) on a heap whose size the test
       chooses, and poisons the heap for AddressSanitizer: block headers, free blocks and the bytes
       behind the size the game asked for can not be touched;
     - records every call with the source line that made it;
     - replays the calls through a model of the allocator, once with the native parameters (which must
       reproduce the real allocator's block addresses exactly: that validates the model) and once with
       the console's: 20-byte headers, 4-byte blocks, the console's sizes for the records that hold
       pointers, the console's heap sizes (0x18F04 main, 0xE1000 reserve).

   usage: loadtest ISO_DIR --sizes FILE [--tracks 1,2,..] [--classes 0,1,2,3] [--dump DIR] [--fill HEX]
          loadtest --list-sites                         the heap call sites the test knows
*/
#include <ctype.h>
#include <fcntl.h>
#include <libgen.h>
#include <sanitizer/asan_interface.h>
#include <setjmp.h>
#include <signal.h>
#include <stdarg.h>
#include <sys/wait.h>
#include <unistd.h>

#include "functions.h"
#include "port_widen.h"
#include "track.h"
#include "hud.h"
#include "ayrace.h"
#include "hiscores.h"
#include "effects.h"
#include "weapon.h"
#include "sparks.h"
#include "trails.h"
#include "dynam.h"
/* psyq.h maps the PsyQ file calls onto the C library's names */
#undef open
#undef close
#undef read
#undef write
#undef lseek

/* The game's data and functions the test touches. */
extern char errorString[];
extern ConfigData *gameData;
extern TrackData *track;
extern Object *prmObjects[];
extern u_short prmCount;
extern char loadtestDiscRoot[];
extern int loadtestPathTableBytes;
extern int loadtestFilesOpened;
extern char loadtestLastError[];
extern int skyPos[];
extern int raceType;
extern int ownShip;
extern int remoteShip;
extern unsigned char raceTracks[];
extern DB doubleBuffer[2];
extern DISPENV DispEnv0, DispEnv1;
extern int MenuControl(ConfigData *gameData, void *menuTexture, int *state, int raceMemorySize);
extern void race(ConfigData *gameData, int context, int scene);
extern void *RealDAlloc(HeapBlock *head, int size);
extern void RealDFree(HeapBlock *head, char *ptr);
extern void RealInitDynamicMem(HeapBlock *head, int size);

/* The console's heaps: main() computed 0x18F04 bytes for the main heap from the program's size (see
   docs/RECORDS.md) and InitDynamicMem(reserveHeap, 0xE1000). */
#define CONSOLE_MAIN_HEAP    0x18F04
#define CONSOLE_RESERVE_HEAP 0xE1000
#define NATIVE_HEADER        ((int)sizeof(HeapBlock))
#define CONSOLE_HEADER       20
#define SEARCH_LIMIT         0x800000     /* DAlloc's "smallest" start value: no block of this size is ever chosen */

enum { MAIN = 0, RESERVE = 1 };

/* ================================================================================================
   Record sizes: the console's (from clang's MIPS layouts, via --sizes) and the native ones
   ============================================================================================== */
typedef struct TypeSize {
    const char *name;
    int native;         /* sizeof, compiled in */
    int console;        /* from the sizes file */
} TypeSize;

#define TYPE(t) { #t, sizeof(t), 0 }
static TypeSize typeSizes[] = {
    TYPE(Object), TYPE(Skeleton), TYPE(ShipData), TYPE(WeaponGrid), TYPE(WeaponShape), TYPE(WeaponData),
    TYPE(Texture), TYPE(Shadow), TYPE(Trail), TYPE(SParticle), TYPE(AutoMeshData), TYPE(Colors), TYPE(HeapBlock),
    TYPE(TrackSection), TYPE(TrackData), TYPE(WADFILE),
};

static TypeSize *LookupType(const char *name)
{
    unsigned i;

    for (i = 0; i < sizeof(typeSizes) / sizeof(typeSizes[0]); i++) {
        if (!strcmp(typeSizes[i].name, name)) {
            return &typeSizes[i];
        }
    }
    fprintf(stderr, "loadtest: no type %s in the size table\n", name);
    exit(2);
}

static void LoadSizes(const char *path)
{
    FILE *f = fopen(path, "r");
    char name[1024];
    int console, host;
    unsigned i;

    if (!f) {
        fprintf(stderr, "loadtest: cannot read %s\n", path);
        exit(2);
    }
    while (fscanf(f, "%63s %d %d", name, &console, &host) == 3) {
        for (i = 0; i < sizeof(typeSizes) / sizeof(typeSizes[0]); i++) {
            if (!strcmp(typeSizes[i].name, name)) {
                typeSizes[i].console = console;
                if (host != typeSizes[i].native) {
                    fprintf(stderr, "loadtest: sizeof(%s) is %d in the build but the layout dump says %d\n", name, typeSizes[i].native, host);
                    exit(2);
                }
            }
        }
    }
    fclose(f);
    for (i = 0; i < sizeof(typeSizes) / sizeof(typeSizes[0]); i++) {
        if (!typeSizes[i].console) {
            fprintf(stderr, "loadtest: no console size of %s in %s\n", typeSizes[i].name, path);
            exit(2);
        }
    }
}

/* ================================================================================================
   The heap calls of the game and what each asks for
   ============================================================================================== */
enum {
    SAME,       /* the size does not depend on a record that holds pointers: same on the console */
    PTRARRAY,   /* an array of pointers: half the bytes on the console */
    ELEMS,      /* count * sizeof(type) + slack, type from the table */
    CLONE,      /* weapon.c copies a loaded PRM image: the console copied the file's length */
    WIDEN,      /* the native copy of a file image that port_widen.c makes (the console has none) */
};

typedef struct Site {
    const char *file;
    const char *text;       /* occurs in the source line of the call */
    int kind;
    const char *type;
    int slack;
    int lines[10];
    int hits;
    const char *invoke;     /* a macro whose expansion makes the call: the calls are the lines that start with this */
} Site;

/* Every DAlloc in game/src has a line here (run.py checks that with --list-sites). */
static Site sites[] = {
    { "display.c", "quad = DAlloc(heap, sizeof(POLY_F4))", SAME },
    { "display.c", "blk = DAlloc(reserveHeap, sizeof(BLK_FILL))", SAME },
    { "effects.c", "shadows = DAlloc(heap, 15 * sizeof(Shadow) + 32)", SAME },
    { "iso9660.c", "pathTable = DAlloc(heap, pathBytes);", SAME },
    { "menus.c", "menu = DAlloc(reserveHeap, 0x6A70)", SAME },
    { "menus.c", "menuLayout = DAlloc(reserveHeap, 0x10760)", SAME },
    { "menus.c", "fileBuffer = DAlloc(reserveHeap, fileLength + 0x20)", SAME },
    { "menus.c", "imageInfo = DAlloc(reserveHeap, 0x34)", SAME },
    { "menus.c", "imageData = DAlloc(reserveHeap, fileLength + 0x20)", SAME },
    { "init.c", "texture = DAlloc(heap, sizeof(Texture))", SAME },
    { "init.c", "fileBuffer = DAlloc(reserveHeap, fileSize + 32)", SAME },
    { "init.c", "timList.memBase = DAlloc(reserveHeap, totalSize)", SAME },
    { "init.c", "buffer = DAlloc(reserveHeap, wad->sizes[wad->current] + 32)", SAME },
    { "init.c", "wadTimList.memBase = DAlloc(reserveHeap, length + 32)", SAME },
    { "raceinit.c", "buffer = DAlloc(heap, fileSize + 0x20)", SAME },
    { "raceinit.c", "fileBuffer = DAlloc(heap, fileSize + 32)", SAME },
    { "progress.c", "particlePolys = DAlloc(reserveHeap, 0xA00)", SAME },
    { "progress.c", "DAlloc(reserveHeap, 0x200)", SAME },
    { "progress.c", "linePrims = DAlloc(reserveHeap, 0x2000)", SAME },
    { "radscn.c", "section->objects = DAlloc(heap, objectCount * sizeof(short))", SAME },
    { "radscn.c", "trackObjects = DAlloc(heap, (objectCount + 1) * sizeof(Object *))", PTRARRAY },
    { "scene.c", "start = DAlloc(heap, length + 32);", SAME },
    { "scene.c", "copy = DAlloc(heap, sizeof(Object))", ELEMS, "Object", 0 },
    { "scene.c", "copy->skeleton = DAlloc(heap, sizeof(Skeleton))", ELEMS, "Skeleton", 0 },
    { "skeleton.c", "skeleton = DAlloc(heap, sizeof(Skeleton))", ELEMS, "Skeleton", 0 },
    { "sparks.c", "particles = DAlloc(heap, PARTICLE_COUNT * sizeof(SParticle))", SAME },
    { "sparks.c", "birdPrims = DAlloc(reserveHeap, BIRD_COUNT * 2 * sizeof(POLY_FT4))", SAME },
    { "sparks.c", "birds = DAlloc(reserveHeap, BIRD_COUNT * sizeof(Bird))", SAME },
    { "tim.c", "texture = DAlloc(heap, sizeof(Texture))", SAME },
    { "tim.c", "tim = DAlloc(reserveHeap, length + 32)", SAME },
    { "tim.c", "tim = DAlloc(reserveHeap, wad->sizes[wad->current] + 32)", SAME },
    { "racectrl.c", "trackCache=DAlloc(reserveHeap,sizeof(AutoMeshData))", SAME },
    { "racectrl.c", "shipData = DAlloc(heap, 17 * sizeof(ShipData))", ELEMS, "ShipData", 0 },
    { "racectrl.c", "weaponGrids = DAlloc(heap, 64 * sizeof(WeaponGrid) + 32)", ELEMS, "WeaponGrid", 32 },
    { "racectrl.c", "weaponBars = DAlloc(heap, 31 * sizeof(WeaponShape) + 32)", ELEMS, "WeaponShape", 32 },
    { "racectrl.c", "weaponPool = DAlloc(heap, 30 * sizeof(WeaponData) + 32)", ELEMS, "WeaponData", 32 },
    { "track.c", "track->vertices = (VECTOR *)DAlloc(heap, length + 32)", SAME },
    { "track.c", "track->faces = (Face *)DAlloc(heap, length + 32)", SAME },
    { "track.c", "track->colCopy = DAlloc(heap, track->faceCount * sizeof(Colors))", SAME },
    { "track.c", "track->sections = (TrackSection *)DAlloc(heap, length + 32)", SAME },
    { "track.c", "track->viewList = (short *)DAlloc(heap, length + 32)", SAME },
    { "trackasm.c", "= DAlloc(heap, 0x", SAME },
    { "trails.c", "shipTrails = DAlloc(reserveHeap, 15 * sizeof(Trail))", SAME },
    { "sound.c", "sampleBuffer = DAlloc(reserveHeap, 0xC800)", SAME },
    { "wadfile.c", "wad = DAlloc(heap, 0x108)", SAME },
    { "ttf.c", "ttfFile = DAlloc(heap, length + 32)", SAME },
    { "ttf.c", "ttfFile = (u_short *)DAlloc(heap, length + 32)", SAME },
    { "weapon.c", "start = DAlloc(heap, length + 32);", CLONE, 0, 0, {0}, 0, "CLONE_SHAPES(" },
    { "port_widen.c", "char *widened = DAlloc(heap, bytes + 32)", WIDEN },
    { "port_widen.c", "char *image = DAlloc(heap, widened + 32)", WIDEN },
};
#define SITE_COUNT ((int)(sizeof(sites) / sizeof(sites[0])))

static void FindSites(const char *gameSource)
{
    int i;

    for (i = 0; i < SITE_COUNT; i++) {
        char path[700], line[1024];
        FILE *f;
        int n = 0;

        snprintf(path, sizeof(path), "%s/%s", gameSource, sites[i].file);
        f = fopen(path, "r");
        if (!f) {
            fprintf(stderr, "loadtest: cannot read %s\n", path);
            exit(2);
        }
        while (fgets(line, sizeof(line), f)) {
            n++;
            const char *p = line;
            int found;

            while (*p == ' ') {
                p++;
            }
            if (sites[i].invoke) {
                /* the definition holds the call, the lines that start with the macro's name are the calls */
                if (strstr(line, "DAlloc(") && strstr(line, sites[i].text)) {
                    sites[i].lines[9] = -1;     /* the definition was seen */
                }
                found = !strncmp(p, sites[i].invoke, strlen(sites[i].invoke));
            } else {
                found = strstr(line, "DAlloc(") && strstr(line, sites[i].text);
            }
            if (found) {
                if (sites[i].hits >= 9) {
                    fprintf(stderr, "loadtest: too many lines for %s %s\n", sites[i].file, sites[i].text);
                    exit(2);
                }
                sites[i].lines[sites[i].hits++] = n;
            }
        }
        fclose(f);
        if (sites[i].invoke && sites[i].lines[9] != -1) {
            fprintf(stderr, "loadtest: %s: the definition of %s with \"%s\" is gone\n", path, sites[i].invoke, sites[i].text);
            exit(2);
        }
        if (!sites[i].hits) {
            fprintf(stderr, "loadtest: %s: no DAlloc line contains \"%s\" (the source changed: update the site table)\n", path, sites[i].text);
            exit(2);
        }
    }
}

static int FindSite(const char *file, int line)
{
    int i, j;

    for (i = 0; i < SITE_COUNT; i++) {
        if (strcmp(sites[i].file, file)) {
            continue;
        }
        for (j = 0; j < sites[i].hits; j++) {
            if (sites[i].lines[j] == line) {
                return i;
            }
        }
    }
    return -1;
}

static int FindSiteByText(const char *file, const char *text)
{
    int i;

    for (i = 0; i < SITE_COUNT; i++) {
        if (!strcmp(sites[i].file, file) && !strcmp(sites[i].text, text)) {
            return i;
        }
    }
    return -1;
}

/* ================================================================================================
   The recorder
   ============================================================================================== */
enum { EV_ALLOC, EV_FREE, EV_INIT };

typedef struct Event {
    unsigned char heap;
    unsigned char kind;
    unsigned char ok;       /* the call succeeded (alloc: got a block; free: the block was live) */
    short site;             /* index in sites, -1 for a free */
    int size;               /* the size asked for (alloc); the capacity given (init) */
    void *ptr;
    int line;
} Event;

static Event *events;
static int eventCount, eventCapacity;

typedef struct Live {
    void *ptr;
    int size;
    int event;
} Live;

static Live *liveTable[2];      /* open addressing on the pointer */
static int liveCapacity = 1 << 16;

static char *heapStorage[2];
static size_t heapStorageSize[2];
static int heapCapacity[2];            /* what InitDynamicMem is given, whatever the game asks for */
static int allocationFailures, unclassified, freesOfUnknown, misaligned, outOfSearchLimit;
static unsigned char fillByte;
static int fillEnabled;

char *LoadtestMainHeap(void) { return heapStorage[MAIN]; }
char *LoadtestReserveHeap(void) { return heapStorage[RESERVE]; }

static int HeapId(HeapBlock *head)
{
    if ((char *)head == heapStorage[MAIN]) {
        return MAIN;
    }
    if ((char *)head == heapStorage[RESERVE]) {
        return RESERVE;
    }
    fprintf(stderr, "loadtest: the game used a heap that is neither the main nor the reserve heap (%p)\n", (void *)head);
    exit(2);
}

static unsigned PointerHash(void *p)
{
    return (unsigned)(((uintptr_t)p >> 3) * 2654435761u) & (liveCapacity - 1);
}

static void LiveAdd(void *p, int size, int event)
{
    unsigned h = PointerHash(p);

    while (liveTable[0][h].ptr && liveTable[0][h].ptr != (void *)1) {
        h = (h + 1) & (liveCapacity - 1);
    }
    liveTable[0][h].ptr = p;
    liveTable[0][h].size = size;
    liveTable[0][h].event = event;
}

static Live *LiveFind(void *p)
{
    unsigned h = PointerHash(p);

    while (liveTable[0][h].ptr) {
        if (liveTable[0][h].ptr == p) {
            return &liveTable[0][h];
        }
        h = (h + 1) & (liveCapacity - 1);
    }
    return 0;
}

static void LiveRemove(Live *l)
{
    l->ptr = (void *)1;     /* tombstone */
}

static void LiveClearHeap(int id)
{
    int i;

    for (i = 0; i < liveCapacity; i++) {
        Live *l = &liveTable[0][i];
        if (l->ptr && l->ptr != (void *)1 && (char *)l->ptr >= heapStorage[id] && (char *)l->ptr < heapStorage[id] + heapStorageSize[id]) {
            l->ptr = (void *)1;
        }
    }
}

static int Record(int heap, int kind, int ok, int site, int size, void *ptr, int line)
{
    Event *e;

    if (eventCount == eventCapacity) {
        eventCapacity = eventCapacity ? eventCapacity * 2 : 1 << 16;
        events = realloc(events, eventCapacity * sizeof(Event));
    }
    e = &events[eventCount];
    e->heap = heap;
    e->kind = kind;
    e->ok = ok;
    e->site = site;
    e->size = size;
    e->ptr = ptr;
    e->line = line;
    return eventCount++;
}

void HarnessInitHeap(HeapBlock *head, int size, const char *file, int line)
{
    int id = HeapId(head);

    RealInitDynamicMem(head, heapCapacity[id]);
    LiveClearHeap(id);
    __asan_poison_memory_region(heapStorage[id], heapStorageSize[id]);
    Record(id, EV_INIT, 1, -1, heapCapacity[id], 0, line);
}

static const char *lastFile;
static void *AllocAt(HeapBlock *head, int size, int site, int line)
{
    int id = HeapId(head);
    void *p = RealDAlloc(head, size);
    int event;

    if (site < 0) {
        unclassified++;
        fprintf(stderr, "loadtest: DAlloc call at line %d is not in the site table (%s)\n", line, lastFile);
        { int q, z; for (q = 0; q < SITE_COUNT; q++) if (strstr(lastFile, sites[q].file)) { fprintf(stderr, "  %s:", sites[q].file); for (z = 0; z < sites[q].hits; z++) fprintf(stderr, " %d", sites[q].lines[z]); fprintf(stderr, "\n"); } }
    }
    event = Record(id, EV_ALLOC, p != 0, site, size, p, line);
    if (!p) {
        allocationFailures++;
        return 0;
    }
    if (((uintptr_t)p & 7) != 0) {
        misaligned++;
    }
    if (size > 0) {
        __asan_unpoison_memory_region(p, size);
    }
    if (fillEnabled && size > 0) {
        memset(p, fillByte, size);
    }
    LiveAdd(p, size, event);
    return p;
}

void *HarnessAlloc(HeapBlock *head, int size, const char *file, int line)
{
    char name[1024];

    snprintf(name, sizeof(name), "%s", file);
    lastFile = file;
    return AllocAt(head, size, FindSite(basename(name), line), line);
}

/* For the stand-ins, which stand in for source lines of a file that is not built. */
void *HarnessAllocAt(HeapBlock *head, int size, const char *file, const char *text)
{
    int site = FindSiteByText(file, text);

    return AllocAt(head, size, site, site >= 0 ? sites[site].lines[0] : 0);
}

void HarnessFree(HeapBlock *head, char *ptr, const char *file, int line)
{
    int id = HeapId(head);
    Live *l = LiveFind(ptr);
    char name[1024];
    int site = -1;
    int i;

    snprintf(name, sizeof(name), "%s", file);
    if (!strcmp(basename(name), "port_widen.c")) {
        site = FindSiteByText("port_widen.c", "char *widened = DAlloc(heap, bytes + 32)");   /* either of the two port_widen.c sites */
        for (i = 0; i < SITE_COUNT; i++) {
            if (!strcmp(sites[i].file, "port_widen.c")) {
                site = i;
            }
        }
    }
    if (!l) {
        freesOfUnknown++;
        Record(id, EV_FREE, 0, site, 0, ptr, line);
        RealDFree(head, ptr);
        return;
    }
    Record(id, EV_FREE, 1, site, l->size, ptr, line);
    if (l->size > 0) {
        __asan_poison_memory_region(ptr, l->size);
    }
    LiveRemove(l);
    RealDFree(head, ptr);
}

/* ================================================================================================
   The allocator model
   ============================================================================================== */
typedef struct MBlock {
    int flags;
    int size;
    int tag;            /* identifies an allocation while the blocks move in the array */
    int event;          /* the event that allocated it */
} MBlock;

typedef struct Model {
    int native;                 /* 1: the native allocator's parameters, 0: the console's */
    int header;
    MBlock *blocks;             /* address order; blocks[0] is the heap header and its free block */
    int count;
    int capacity;
    int use;                    /* sum of header + size of the allocated blocks */
    int peakUse;
    int peakBlocks;
    int blocksInUse;
    int biggest;                /* the largest block ever allocated (its size without the header) */
    int minHeadFree;            /* the smallest the heap block (blocks[0]) ever got: the footprint */
    int failures;
    int tags;
    int allocs, frees;
    int endOffsetMismatch;      /* native: blocks at other addresses than the real allocator's */
} Model;

static int RoundSize(const Model *m, int size)
{
    size += 4;
    return m->native ? (size + 7) & ~7 : (size | 3) ^ 3;
}

static void ModelInit(Model *m, int native, int capacity)
{
    int avail;

    memset(m, 0, sizeof(*m));
    m->native = native;
    m->header = native ? NATIVE_HEADER : CONSOLE_HEADER;
    avail = capacity - m->header;
    if (native) {
        avail &= ~7;
    }
    m->capacity = capacity;
    m->blocks = calloc(1 << 16, sizeof(MBlock));
    m->count = 1;
    m->blocks[0].size = avail;
    m->minHeadFree = avail;
}

static void ModelDestroy(Model *m)
{
    free(m->blocks);
}

/* Returns the tag of the new block, or -1. offset (if not null) receives the block's data offset from the heap start. */
static int ModelAlloc(Model *m, int size, int event, long *offset)
{
    int smallest = SEARCH_LIMIT;
    int best = -1;
    int i;
    long position = 0;
    long bestPosition = 0;

    size = RoundSize(m, size);
    for (i = 0; i < m->count; i++) {
        MBlock *b = &m->blocks[i];
        if (b->size >= size + m->header && b->size < smallest && b->flags == 0) {
            smallest = b->size;
            best = i;
            bestPosition = position;
        }
        if (b->size == size && b->flags == 0) {
            b->flags = 1;
            b->tag = ++m->tags;
            b->event = event;
            m->use += size;
            m->blocksInUse++;
            if (offset) {
                *offset = position + m->header;
            }
            goto account;
        }
        position += m->header + b->size;
    }
    if (best < 0) {
        m->failures++;
        return -1;
    }
    {
        int leftover = m->blocks[best].size - m->header - size;

        memmove(&m->blocks[best + 2], &m->blocks[best + 1], (m->count - best - 1) * sizeof(MBlock));
        m->blocks[best].size = leftover;
        m->blocks[best + 1].flags = 1;
        m->blocks[best + 1].size = size;
        m->blocks[best + 1].tag = ++m->tags;
        m->blocks[best + 1].event = event;
        m->count++;
        m->use += m->header + size;
        m->blocksInUse++;
        if (offset) {
            *offset = bestPosition + m->header + leftover + m->header;
        }
        if (best == 0 && leftover < m->minHeadFree) {
            m->minHeadFree = leftover;
        }
        i = best + 1;
    }
account:
    m->allocs++;
    if (m->use > m->peakUse) {
        m->peakUse = m->use;
    }
    if (m->blocksInUse > m->peakBlocks) {
        m->peakBlocks = m->blocksInUse;
    }
    if (size > m->biggest) {
        m->biggest = size;
    }
    return m->blocks[i].tag;
}

static void ModelFree(Model *m, int tag)
{
    int index, prev;
    MBlock *block;

    for (index = 1; index < m->count && m->blocks[index].tag != tag; index++)
        ;
    if (index >= m->count) {
        return;
    }
    block = &m->blocks[index];
    prev = index - 1;
    m->frees++;
    m->use -= block->size;
    m->blocksInUse--;
    if (index + 1 < m->count && m->blocks[index + 1].flags == 0) {
        block->size += m->blocks[index + 1].size + m->header;
        memmove(&m->blocks[index + 1], &m->blocks[index + 2], (m->count - index - 2) * sizeof(MBlock));
        m->count--;
    }
    if (m->blocks[prev].flags == 1) {
        block->flags = 0;
        block->tag = 0;
        return;
    }
    m->blocks[prev].size += block->size + m->header;
    memmove(&m->blocks[index], &m->blocks[index + 1], (m->count - index - 1) * sizeof(MBlock));
    m->count--;
    if (prev == 0 && m->blocks[0].size > m->minHeadFree) {
        /* the heap block grew back: nothing to record, minHeadFree is the minimum */
    }
}

/* The console's size for an allocation event. */
static int ConsoleSize(const Event *e, int lastImage)
{
    const Site *s = &sites[e->site];

    switch (s->kind) {
    case SAME:
        return e->size;
    case PTRARRAY:
        return e->size / 2;
    case ELEMS: {
        const TypeSize *t = LookupType(s->type);
        int bytes = e->size - s->slack;

        if (bytes % t->native) {
            fprintf(stderr, "loadtest: %s: %d bytes is not a whole number of %s\n", s->file, e->size, s->type);
            exit(2);
        }
        return bytes / t->native * t->console + s->slack;
    }
    case CLONE:
        return lastImage;
    }
    return e->size;
}

typedef struct Replay {
    Model model;
    int tag[1];          /* unused */
} Replay;

/* Replays the recorded calls of one heap, from its last InitDynamicMem on. mode: 0 the console, 1 the
   native allocator (the real allocator's addresses are checked). capacity < 0: the one the heap had
   (console: the console's size). Returns the model's final state in *out (the caller destroys it). */
static void ReplayHeap(int heap, int native, int capacity, Model *out)
{
    int first = 0, i;
    int *tags;
    int lastImage = 0;
    long base = 0;

    for (i = 0; i < eventCount; i++) {
        if (events[i].heap == heap && events[i].kind == EV_INIT) {
            first = i;
        }
    }
    tags = malloc(eventCount * sizeof(int));
    for (i = 0; i < eventCount; i++) {
        tags[i] = 0;
    }
    ModelInit(out, native, capacity);
    for (i = first; i < eventCount; i++) {
        Event *e = &events[i];
        int j;

        if (e->heap != heap) {
            continue;
        }
        if (e->kind == EV_INIT) {
            continue;
        }
        if (e->kind == EV_ALLOC) {
            long offset = 0;
            int size = e->size;

            if (!e->ok) {
                continue;
            }
            if (!native) {
                if (sites[e->site].kind == WIDEN) {
                    /* the native copy of a file image: in the console the image is the file's block, which this copy
                       replaces (the DFree of the file image follows at once). The copy takes over the file's tag. */
                    Event *f = &events[i + 1];
                    int source = -1;
                    for (j = i - 1; j >= first; j--) {
                        if (events[j].kind == EV_ALLOC && events[j].ptr == f->ptr && tags[j] > 0) {
                            source = j;
                            break;
                        }
                    }
                    if (f->kind != EV_FREE || !f->ok) {
                        fprintf(stderr, "loadtest: a widened copy is not followed by the free of its file image\n");
                        exit(2);
                    }
                    if (source < 0) {
                        continue;       /* the file image did not fit the model's heap: already counted as a failure */
                    }
                    tags[i] = tags[source];
                    tags[source] = -1;      /* the free of the file image is skipped */
                    continue;
                }
                if (!strcmp(sites[e->site].file, "scene.c") && sites[e->site].kind == SAME && strstr(sites[e->site].text, "length + 32")) {
                    lastImage = e->size;
                }
                size = ConsoleSize(e, lastImage);
            }
            tags[i] = ModelAlloc(out, size, i, &offset);
            if (tags[i] < 0) {
                continue;
            }
            if (native) {
                long real = (char *)e->ptr - heapStorage[heap];
                if (real != offset) {
                    out->endOffsetMismatch++;
                }
            }
            (void)base;
            continue;
        }
        /* a free */
        if (!e->ok) {
            continue;
        }
        if (!native && e->site >= 0 && !strcmp(sites[e->site].file, "port_widen.c")) {
            continue;       /* it frees a file image the console keeps for the widened copy */
        }
        for (j = i - 1; j >= first; j--) {
            if (events[j].kind == EV_ALLOC && events[j].ptr == e->ptr && tags[j] != 0) {
                if (tags[j] > 0) {
                    ModelFree(out, tags[j]);
                }
                tags[j] = -1;
                break;
            }
        }
        if (native) {
            /* a native free is of the block allocated last at that address: events[j] */
        }
    }
    free(tags);
}

/* ================================================================================================
   Results of a scenario
   ============================================================================================== */
typedef struct HeapStats {
    int capacity;           /* the heap's size in this model */
    int endUse;
    int peakUse;
    int endBlocks;
    int peakBlocks;
    int biggest;
    int fails;              /* allocation failures at that capacity */
    int footprint;          /* the capacity at which the replay just fits (bisected) */
    int mismatch;           /* native only: allocations at other addresses than the real allocator's */
} HeapStats;

typedef struct Result {
    int ok;                         /* the scenario reached its end */
    char error[300];
    int events;
    int unclassified, freesOfUnknown, misaligned, allocationFailures;
    int variantClass, track;
    HeapStats con[2], nat[2];
    int filesOpened;
    int mainHeapEventsAfterStart;   /* heap calls on the main heap after the scenario began (the load itself) */
    int hits[64];                   /* heap calls made at each site since the program began */
} Result;

static void Stats(HeapStats *s, Model *m, int capacity)
{
    s->capacity = capacity;
    s->endUse = m->use;
    s->peakUse = m->peakUse;
    s->endBlocks = m->blocksInUse;
    s->peakBlocks = m->peakBlocks;
    s->biggest = m->biggest;
    s->fails = m->failures;
    s->mismatch = m->endOffsetMismatch;
}

/* The smallest capacity at which the replay has no failure. */
static int MinCapacity(int heap, int native, int hi)
{
    int lo = CONSOLE_HEADER;
    Model m;

    /* hi must fit */
    ReplayHeap(heap, native, hi, &m);
    if (m.failures) {
        ModelDestroy(&m);
        return -1;
    }
    ModelDestroy(&m);
    while (hi - lo > 1) {
        int mid = lo + (hi - lo) / 2;

        ReplayHeap(heap, native, mid, &m);
        if (m.failures) {
            lo = mid;
        } else {
            hi = mid;
        }
        ModelDestroy(&m);
    }
    return hi;
}

static void Measure(Result *r, int heap)
{
    Model m;
    int consoleCapacity = heap == MAIN ? CONSOLE_MAIN_HEAP : CONSOLE_RESERVE_HEAP;
    int natCapacity = heapCapacity[heap];

    ReplayHeap(heap, 0, consoleCapacity, &m);
    Stats(&r->con[heap], &m, consoleCapacity);
    ModelDestroy(&m);
    r->con[heap].footprint = MinCapacity(heap, 0, consoleCapacity * 4);
    ReplayHeap(heap, 1, natCapacity, &m);
    Stats(&r->nat[heap], &m, natCapacity);
    ModelDestroy(&m);
    r->nat[heap].footprint = MinCapacity(heap, 1, natCapacity);
}

/* ================================================================================================
   Dumps for the independent check (run.py's expected_dump)
   ============================================================================================== */
static FILE *dumpFile;

static void DumpObjectList(Object *first, int index)
{
    Object *o;

    for (o = first; o; o = o->next) {
        Skeleton *k = o->skeleton;
        int a, b;

        fprintf(dumpFile, "O %d %.16s extent %d primitives %d", index, o->name, o->extent, o->primitiveCount);
        fprintf(dumpFile, " rel");
        for (a = 0; a < 3; a++) {
            for (b = 0; b < 3; b++) {
                fprintf(dumpFile, " %d", k->relative.m[a][b]);
            }
        }
        fprintf(dumpFile, " t %d %d %d abs %d %d %d update %d\n", k->relative.t[0], k->relative.t[1], k->relative.t[2],
                k->absolute.t[0], k->absolute.t[1], k->absolute.t[2], k->update);
    }
}

static void DumpTrack(int act)
{
    int i, j;
    short *viewList = track->viewList;

    fprintf(dumpFile, "T %d sky %d\n", act, skyPos[gameData->trackNum]);
    fprintf(dumpFile, "N sections %d vertices %d faces %d\n", track->sectionCount, track->vertexCount, track->faceCount);
    for (i = 0; i < track->sectionCount; i++) {
        TrackSection *s = &track->sections[i];
        short *groups[5][3];
        short *counts[5];
        int g;

        fprintf(dumpFile, "S %d version %d pad %d centre %d %d %d", i, s->version, s->pad, s->centre.vx, s->centre.vy, s->centre.vz);
        fprintf(dumpFile, " prev %d next %d junction %d", (int)(s->prevSection.ptr - track->sections),
                (int)(s->nextSection.ptr - track->sections),
                s->junction.ptr == (TrackSection *)-1 ? -1 : (int)(s->junction.ptr - track->sections));
        fprintf(dumpFile, " face %d %d radius %d %d flags %d secNo %d bump %d snow %d", s->faceStart, s->faceCount, s->globalRadius,
                s->localRadius, s->flags, s->secNo, s->bump, s->snow);
        counts[0] = s->northViewCount;
        counts[1] = s->southViewCount;
        counts[2] = s->eastViewCount;
        counts[3] = s->westViewCount;
        counts[4] = s->allViewCount;
        for (j = 0; j < 3; j++) {
            groups[0][j] = s->northViewSection[j];
            groups[1][j] = s->southViewSection[j];
            groups[2][j] = s->eastViewSection[j];
            groups[3][j] = s->westViewSection[j];
            groups[4][j] = s->allViewSection[j];
        }
        fprintf(dumpFile, " counts");
        for (g = 0; g < 5; g++) {
            for (j = 0; j < 3; j++) {
                fprintf(dumpFile, " %d", counts[g][j]);
            }
        }
        fprintf(dumpFile, " views");
        for (g = 0; g < 5; g++) {
            for (j = 0; j < 3; j++) {
                fprintf(dumpFile, " %d", (int)(groups[g][j] - viewList));
            }
        }
        fprintf(dumpFile, " high %d %d %d %d med %d %d %d %d\n", s->high[0], s->high[1], s->high[2], s->high[3], s->med[0],
                s->med[1], s->med[2], s->med[3]);
    }
    for (i = 0; i < track->vertexCount; i++) {
        fprintf(dumpFile, "V %d %d %d %d\n", i, track->vertices[i].vx, track->vertices[i].vy, track->vertices[i].vz);
    }
    for (i = 0; i < track->faceCount; i++) {
        Face *f = &track->faces[i];
        Colors *c = (Colors *)track->colCopy;

        fprintf(dumpFile, "F %d %d %d %d %d n %d %d %d tex %d flags %d rgb %d %d %d copy %d %d %d\n", i, f->vertex[0], f->vertex[1], f->vertex[2],
                f->vertex[3], f->normal.vx, f->normal.vy, f->normal.vz, f->texture, f->flags, f->red, f->green, f->blue, c[i].red, c[i].green, c[i].blue);
    }
    {
        /* scene.prm and sky.prm: WADInitScene fills prmObjects[0] and [1]; the sky is moved to (0, skyPos, 0) */
        DumpObjectList(prmObjects[0], 0);
        DumpObjectList(prmObjects[1], 1);
    }
}

/* ================================================================================================
   Running the scenarios
   ============================================================================================== */
enum { MODE_DRIVER, MODE_BOOT_ONLY, MODE_MENU, MODE_RACE };

static TrackData bootTrack;
static int selftest;
static int mode = MODE_DRIVER;
static int resultFd = -1;
static Result *currentResult;
static int scenarioTrack, scenarioClass;
static const char *dumpDirectory;
static int selectedTracks[32], selectedTrackCount;
static int selectedClasses[4] = {0, 1, 2, 3}, selectedClassCount = 4;
static int menuTimeout = 0xE10;

static int eventsAtStart;

static void SendResult(Result *r)
{
    int i;

    for (i = 0; i < eventCount; i++) {
        if (events[i].kind == EV_ALLOC && events[i].site >= 0) {
            r->hits[events[i].site]++;
        }
        if (i >= eventsAtStart && events[i].heap == MAIN && events[i].kind != EV_INIT) {
            r->mainHeapEventsAfterStart++;
        }
    }
    r->events = eventCount;
    r->unclassified = unclassified;
    r->freesOfUnknown = freesOfUnknown;
    r->misaligned = misaligned;
    r->allocationFailures = allocationFailures;
    r->filesOpened = loadtestFilesOpened;
    if (write(resultFd, r, sizeof(*r)) != (ssize_t)sizeof(*r)) {
        _exit(5);
    }
}

void LoadtestExit(int status)
{
    Result r;

    memset(&r, 0, sizeof(r));
    r.ok = 0;
    snprintf(r.error, sizeof(r.error), "the game stopped: %s", loadtestLastError);
    if (mode == MODE_DRIVER) {
        fprintf(stderr, "loadtest: the game stopped during start-up: %s\n", loadtestLastError);
        _exit(3);
    }
    SendResult(&r);
    _exit(0);
}

/* Starts a child that runs fn and reports a Result. Returns the result (ok = 0 if the child died). */
static Result RunChild(int childMode, void (*fn)(void), int track, int shipClass)
{
    int fds[2];
    pid_t pid;
    Result r;
    int status;
    ssize_t got;

    fflush(stdout);
    fflush(stderr);
    if (pipe(fds)) {
        perror("pipe");
        exit(2);
    }
    pid = fork();
    if (pid == 0) {
        close(fds[0]);
        resultFd = fds[1];
        eventsAtStart = eventCount;
        mode = childMode;
        scenarioTrack = track;
        scenarioClass = shipClass;
        fn();
        _exit(0);
    }
    close(fds[1]);
    memset(&r, 0, sizeof(r));
    got = read(fds[0], &r, sizeof(r));
    close(fds[0]);
    waitpid(pid, &status, 0);
    if (got != (ssize_t)sizeof(r)) {
        r.ok = 0;
        snprintf(r.error, sizeof(r.error), "the scenario process died (status %d)", status);
    }
    return r;
}

static void ScenarioMenu(void)
{
    int state = 0;

    gameType = 2;
    raceType = 0x63;
    MenuControl(gameData, 0, &state, menuTimeout);
    /* not reached: the hook (stage 1) reports and exits */
    LoadtestExit(0);
}

static void ScenarioRace(void)
{
    int t;

    gameType = 2;
    raceType = 0;
    ownShip = 9;
    remoteShip = -1;
    gameData->shipClass = scenarioClass;
    gameData->actTrackNum = scenarioTrack;
    gameData->trackNum = 0;
    for (t = 0; t < 8; t++) {
        if (raceTracks[t] == scenarioTrack) {
            gameData->trackNum = t;
        }
    }
    race(gameData, 0, menuTimeout);
    LoadtestExit(0);
}

typedef struct Scenario {
    int track, shipClass;
    Result result;
    Result atFootprint;         /* the race run on a heap of exactly the bisected size */
    Result belowFootprint;      /* ... and 8 bytes less */
} Scenario;

static Scenario scenarios[64];
static int scenarioCount;
static Result menuResult;

static void RunDriver(void)
{
    int t, c;

    menuResult = RunChild(MODE_MENU, ScenarioMenu, 0, 0);
    for (t = 0; t < selectedTrackCount; t++) {
        for (c = 0; c < selectedClassCount; c++) {
            Scenario *s = &scenarios[scenarioCount++];

            s->track = selectedTracks[t];
            s->shipClass = selectedClasses[c];
            /* the first ship class writes the dump */
            if (c != 0) {
                const char *keep = dumpDirectory;
                dumpDirectory = 0;
                s->result = RunChild(MODE_RACE, ScenarioRace, s->track, s->shipClass);
                dumpDirectory = keep;
            } else {
                s->result = RunChild(MODE_RACE, ScenarioRace, s->track, s->shipClass);
            }
        }
    }
    /* the real allocator on a heap of the bisected size and of 8 bytes less (c == 0 only) */
    for (t = 0; t < scenarioCount; t++) {
        Scenario *s = &scenarios[t];
        int fp = s->result.nat[RESERVE].footprint;
        int saved = heapCapacity[RESERVE];
        const char *keep = dumpDirectory;

        if (!s->result.ok || fp <= 0) {
            continue;
        }
        dumpDirectory = 0;
        heapCapacity[RESERVE] = fp;
        s->atFootprint = RunChild(MODE_RACE, ScenarioRace, s->track, s->shipClass);
        heapCapacity[RESERVE] = fp - 8;
        s->belowFootprint = RunChild(MODE_RACE, ScenarioRace, s->track, s->shipClass);
        heapCapacity[RESERVE] = saved;
        dumpDirectory = keep;
    }
}

static Result bootResult;       /* from the probe run */
static int driverDone;

int LoadtestHook(int stage)
{
    Result r;

    if (stage == 0) {
        if (mode == MODE_BOOT_ONLY) {
            memset(&r, 0, sizeof(r));
            r.ok = 1;
            Measure(&r, MAIN);
            SendResult(&r);
            _exit(0);
        }
        if (mode == MODE_DRIVER) {
            RunDriver();
            driverDone = 1;
        }
        return 1;
    }
    memset(&r, 0, sizeof(r));
    r.ok = 1;
    if (stage == 1 && mode == MODE_MENU) {
        Measure(&r, RESERVE);
        SendResult(&r);
        _exit(0);
    }
    if (stage == 2 && mode == MODE_RACE) {
        if (dumpDirectory) {
            char name[700];

            snprintf(name, sizeof(name), "%s/track%02d.txt", dumpDirectory, scenarioTrack);
            dumpFile = fopen(name, "w");
            DumpTrack(scenarioTrack);
            fclose(dumpFile);
        }
        Measure(&r, RESERVE);
        SendResult(&r);
        _exit(0);
    }
    fprintf(stderr, "loadtest: unexpected hook %d in mode %d\n", stage, mode);
    _exit(4);
}

/* Does the heap poisoning catch what it is there for? Each line below must make AddressSanitizer report
   (use-after-poison), which run.py looks for; the writes are the kinds of bug the loaders could have. */
static int SelfTest(void)
{
    volatile char *p;
    int site = FindSiteByText("scene.c", "start = DAlloc(heap, length + 32);");

    HarnessInitHeap((HeapBlock *)heapStorage[RESERVE], 0, "scene.c", 0);
    p = AllocAt((HeapBlock *)heapStorage[RESERVE], 100, site, 1);
    fprintf(stderr, "selftest: write inside the block\n");
    p[0] = 1;
    p[99] = 1;
    fprintf(stderr, "selftest: write one byte after the block\n");
    p[100] = 1;
    HarnessFree((HeapBlock *)heapStorage[RESERVE], (char *)p, "scene.c", 1);
    fprintf(stderr, "selftest: write to a freed block\n");
    p[0] = 1;
    fprintf(stderr, "selftest: write before the block (into its header)\n");
    p[-1] = 1;
    fprintf(stderr, "selftest: done\n");
    return 0;
}

static void ScenarioBoot(void)
{
    PsxMain();
    LoadtestExit(0);
}

/* ================================================================================================
   Report
   ============================================================================================== */
static int problems;

static void Problem(const char *format, ...)
{
    va_list args;

    va_start(args, format);
    printf("PROBLEM ");
    vprintf(format, args);
    printf("\n");
    va_end(args);
    problems++;
}

static double Scale(int native, int console)
{
    return (double)native / console;
}

static void PrintRow(const char *label, int heap, const Result *r)
{
    const HeapStats *c = &r->con[heap];
    const HeapStats *n = &r->nat[heap];

    if (!r->ok) {
        printf("%-18s FAILED: %s\n", label, r->error);
        problems++;
        return;
    }
    printf("%-18s %6d | %8d %8d %8d %s | %8d %8d %8d | %6.3f %5d %5d\n", label, n->endBlocks, c->endUse, c->peakUse, c->footprint,
           c->fails ? "NO " : "yes", n->endUse, n->peakUse, n->footprint, Scale(n->footprint, c->capacity), c->biggest, n->biggest);
    if (c->fails) {
        Problem("%s: the console model does not fit its heap of %d bytes", label, c->capacity);
    }
    if (n->mismatch) {
        Problem("%s: the native model puts %d blocks at other addresses than the real allocator", label, n->mismatch);
    }
}

static void PrintHeader(const char *title, int capacity)
{
    printf("\n%s (console heap %d bytes = 0x%X)\n", title, capacity, capacity);
    printf("%-18s %6s | %8s %8s %8s %s | %8s %8s %8s | %6s %5s %5s\n", "", "blocks", "con end", "con peak", "con min", "fit", "nat end", "nat peak",
           "nat min", "scale", "bigC", "bigN");
}

static void Check(const Result *r, const char *label)
{
    if (r->unclassified) {
        Problem("%s: %d heap calls are not in the site table", label, r->unclassified);
    }
    if (r->misaligned) {
        Problem("%s: %d blocks are not 8-byte aligned", label, r->misaligned);
    }
    if (r->freesOfUnknown) {
        printf("NOTE %s: %d DFree calls with a pointer that is not a live block\n", label, r->freesOfUnknown);
    }
}

static int Report(void)
{
    int i;
    double worst = 0;
    char label[64];

    printf("native header %d bytes, console 20; native blocks are multiples of 8, the console's of 4\n", NATIVE_HEADER);
    PrintHeader("start-up: the main heap", CONSOLE_MAIN_HEAP);
    PrintRow("InitResident etc.", MAIN, &bootResult);
    Check(&bootResult, "start-up");
    if (bootResult.ok && Scale(bootResult.nat[MAIN].footprint, CONSOLE_MAIN_HEAP) > worst) {
        worst = Scale(bootResult.nat[MAIN].footprint, CONSOLE_MAIN_HEAP);
    }
    PrintHeader("menu: the reserve heap at the end of MenuControl's loading", CONSOLE_RESERVE_HEAP);
    PrintRow("MenuControl", RESERVE, &menuResult);
    Check(&menuResult, "menu");
    PrintHeader("race: the reserve heap at the end of race()'s loading", CONSOLE_RESERVE_HEAP);
    for (i = 0; i < scenarioCount; i++) {
        Scenario *s = &scenarios[i];

        snprintf(label, sizeof(label), "track %02d class %d", s->track, s->shipClass);
        PrintRow(label, RESERVE, &s->result);
        Check(&s->result, label);
        if (s->result.ok) {
            int fp = s->result.nat[RESERVE].footprint;

            if (Scale(fp, CONSOLE_RESERVE_HEAP) > worst) {
                worst = Scale(fp, CONSOLE_RESERVE_HEAP);
            }
            if (!s->atFootprint.ok) {
                Problem("%s: the real allocator fails on a heap of %d bytes, the size the model says is enough", label, fp);
            }
            if (s->belowFootprint.ok) {
                Problem("%s: the real allocator still succeeds on a heap of %d bytes, 8 less than the model's minimum", label, fp - 8);
            }
        }
    }
    printf("\nthe largest footprint over the console's heap: %.3f\n", worst);
    printf("the game's heaps now: main %d bytes, reserve %d bytes (PORT_HEAP_SCALE %d)\n", MAIN_HEAP_SIZE, RESERVE_HEAP_SIZE, PORT_HEAP_SCALE);
    {
        double mainNeed = bootResult.ok ? Scale(bootResult.nat[MAIN].footprint, CONSOLE_MAIN_HEAP) : 0;
        double reserveNeed = 0;

        for (i = 0; i < scenarioCount; i++) {
            if (scenarios[i].result.ok && Scale(scenarios[i].result.nat[RESERVE].footprint, CONSOLE_RESERVE_HEAP) > reserveNeed) {
                reserveNeed = Scale(scenarios[i].result.nat[RESERVE].footprint, CONSOLE_RESERVE_HEAP);
            }
        }
        if (menuResult.ok && Scale(menuResult.nat[RESERVE].footprint, CONSOLE_RESERVE_HEAP) > reserveNeed) {
            reserveNeed = Scale(menuResult.nat[RESERVE].footprint, CONSOLE_RESERVE_HEAP);
        }
        printf("smallest heaps that load everything, over the console's: main %.4f, reserve %.4f\n", mainNeed, reserveNeed);
        if (mainNeed > PORT_HEAP_SCALE || reserveNeed > PORT_HEAP_SCALE) {
            Problem("PORT_HEAP_SCALE %d is smaller than the %.4f these loads need", PORT_HEAP_SCALE, mainNeed > reserveNeed ? mainNeed : reserveNeed);
        }
    }
    {
        int hit[64];
        int mainEvents = 0;

        memset(hit, 0, sizeof(hit));
        for (i = 0; i < SITE_COUNT; i++) {
            int k;

            hit[i] += bootResult.hits[i] + menuResult.hits[i];
            for (k = 0; k < scenarioCount; k++) {
                hit[i] += scenarios[k].result.hits[i];
            }
        }
        for (i = 0; i < scenarioCount; i++) {
            mainEvents += scenarios[i].result.mainHeapEventsAfterStart;
        }
        mainEvents += menuResult.mainHeapEventsAfterStart;
        printf("heap calls on the main heap during the menu's and the races' loading: %d\n", mainEvents);
        printf("heap call sites the sequences never reached:");
        for (i = 0; i < SITE_COUNT; i++) {
            if (!hit[i]) {
                int line = sites[i].lines[0];
                printf(" %s:%d", sites[i].file, line);
            }
        }
        printf("\n");
    }
    return problems != 0;
}

int main(int argc, char **argv)
{
    const char *gameSource = getenv("LOADTEST_GAME_SRC");
    const char *sizes = 0;
    int i;
    int boot;

    if (argc >= 2 && !strcmp(argv[1], "--list-sites")) {
        for (i = 0; i < SITE_COUNT; i++) {
            printf("%s\t%s\n", sites[i].file, sites[i].text);
        }
        return 0;
    }
    if (argc >= 2 && !strcmp(argv[1], "--list-types")) {
        for (i = 0; i < (int)(sizeof(typeSizes) / sizeof(typeSizes[0])); i++) {
            printf("%s\n", typeSizes[i].name);
        }
        return 0;
    }
    if (argc < 2) {
        fprintf(stderr, "usage: loadtest ISO_DIR --sizes FILE [--tracks 1,2,..] [--classes 0,1,2,3] [--dump DIR] [--fill HEX]\n");
        return 2;
    }
    snprintf(loadtestDiscRoot, 512, "%s", argv[1]);
    for (i = 2; i < argc; i++) {
        if (!strcmp(argv[i], "--sizes") && i + 1 < argc) {
            sizes = argv[++i];
        } else if (!strcmp(argv[i], "--dump") && i + 1 < argc) {
            dumpDirectory = argv[++i];
        } else if (!strcmp(argv[i], "--fill") && i + 1 < argc) {
            fillEnabled = 1;
            fillByte = (unsigned char)strtol(argv[++i], 0, 16);
        } else if (!strcmp(argv[i], "--tracks") && i + 1 < argc) {
            char *list = strdup(argv[++i]);
            char *tok;

            for (tok = strtok(list, ","); tok; tok = strtok(0, ",")) {
                selectedTracks[selectedTrackCount++] = atoi(tok);
            }
        } else if (!strcmp(argv[i], "--classes") && i + 1 < argc) {
            char *list = strdup(argv[++i]);
            char *tok;

            selectedClassCount = 0;
            for (tok = strtok(list, ","); tok; tok = strtok(0, ",")) {
                selectedClasses[selectedClassCount++] = atoi(tok);
            }
        } else if (!strcmp(argv[i], "--selftest")) {
            selftest = 1;
        } else if (!strcmp(argv[i], "--path-table-bytes") && i + 1 < argc) {
            loadtestPathTableBytes = atoi(argv[++i]);
        } else {
            fprintf(stderr, "loadtest: unknown argument %s\n", argv[i]);
            return 2;
        }
    }
    if (!sizes) {
        fprintf(stderr, "loadtest: --sizes is required\n");
        return 2;
    }
    if (!selectedTrackCount) {
        static const int all[] = {1, 2, 6, 7, 8, 13, 17, 20};
        for (i = 0; i < 8; i++) {
            selectedTracks[selectedTrackCount++] = all[i];
        }
    }
    LoadSizes(sizes);
    /* InitSystem calls PutDispEnv(doubleBuffer[0].disp) before it fills doubleBuffer in: the console read its low
       memory through the null pointer, natively that is a crash (reported in docs/RECORDS.md). */
    doubleBuffer[0].disp = &DispEnv0;
    doubleBuffer[1].disp = &DispEnv1;
    /* InitResident reads track->sections (to pass to loadShipData, which ignores it) while track is still null. */
    track = &bootTrack;
    FindSites(gameSource ? gameSource : "game/src");
    liveTable[0] = calloc(liveCapacity, sizeof(Live));
    heapCapacity[MAIN] = CONSOLE_MAIN_HEAP * 4;
    heapCapacity[RESERVE] = CONSOLE_RESERVE_HEAP * 4;
    for (i = 0; i < 2; i++) {
        heapStorageSize[i] = (size_t)heapCapacity[i] + 4096;
        heapStorage[i] = aligned_alloc(4096, (heapStorageSize[i] + 4095) & ~4095);
        memset(heapStorage[i], 0, heapStorageSize[i]);
    }

    if (selftest) {
        return SelfTest();
    }
    /* the start-up: measured in a process of its own, then on heaps of the measured size */
    bootResult = RunChild(MODE_BOOT_ONLY, ScenarioBoot, 0, 0);
    if (!bootResult.ok) {
        printf("start-up failed: %s\n", bootResult.error);
        return 1;
    }
    boot = bootResult.nat[MAIN].footprint;
    {
        Result at, below;
        int saved = heapCapacity[MAIN];

        heapCapacity[MAIN] = boot;
        at = RunChild(MODE_BOOT_ONLY, ScenarioBoot, 0, 0);
        heapCapacity[MAIN] = boot - 8;
        below = RunChild(MODE_BOOT_ONLY, ScenarioBoot, 0, 0);
        heapCapacity[MAIN] = saved;
        if (!at.ok) {
            Problem("start-up fails on a main heap of %d bytes, the size the model says is enough", boot);
        }
        if (below.ok) {
            Problem("start-up still succeeds on a main heap of %d bytes, 8 less than the model's minimum", boot - 8);
        }
    }
    mode = MODE_DRIVER;
    PsxMain();
    if (!driverDone) {
        fprintf(stderr, "loadtest: the start-up did not reach the menu loop\n");
        return 2;
    }
    return Report();
}
