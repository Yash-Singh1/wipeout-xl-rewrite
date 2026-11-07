/* The frame recorder (framecap.h). */
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <SDL3/SDL.h>

#include "framecap.h"
#include "gpu.h"
#include "../render/state.h"
#ifdef __APPLE__
#include <malloc/malloc.h>
#endif

#ifdef PORT_HAVE_TURBOJPEG
#include <turbojpeg.h>
#endif
#include "pad.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_WRITE_NO_STDIO
#include "../third_party/stb/stb_image_write.h"
#define STBI_NO_STDIO
#include "../third_party/stb/stb_image.h" /* (its implementation is hdtex.c's) */
#include "../debug_ui.h"
#include "../platform.h"
#include "../settings.h"
#include "../link.h"

int PortGameInfo(char *out, int size); /* (game/src/portinfo.c) */

#define LAST_SECONDS 3 /* RECORD_LAST_SECONDS: the last three seconds */
#define TAB_SECONDS 10 /* RECORD_WITH_TAB: up to the last ten */
#define FRAMES 1200    /* (at most: ten seconds of 120 pictures a second, five of 240) */
#define PICTURE_W 320
/* Keep the displayed detail: Box's effective scale, or the rendering scale for Adaptive and Off.
   Read back asynchronously (gpu.h's GPU_READ_RECORDER) and compress with worker threads at this
   JPEG quality (above 90: colours not subsampled): 1 to 3 MB a frame at 4K. */
#define PICTURE_QUALITY 95

static FrameCapFrame frames[FRAMES];
static uint8_t skippedPictures[FRAMES];
static uint32_t nextNumber;
static uint64_t lastPictureTicks; /* when the last recorded picture was taken */
static int inBetween;             /* a picture between frames is being recorded (FrameCapBetween) ... */
static int betweenSaved = -1;     /* ... and this was the frame being recorded before it */

/* Pictures a second kept, and the frames the recording holds at most. */
static int Rate(void)
{
    return portSettings.recordRate ? SDL_clamp(portSettings.recordRate, 30, 240) : 60;
}

static int Capacity(void)
{
    return SDL_min(Rate() * (FrameCapMode() == RECORD_WITH_TAB ? TAB_SECONDS : LAST_SECONDS), FRAMES);
}
static int first, count; /* the ring: frames[first], ... count of them */
static int current = -1; /* the frame being recorded */
static int newFrame = 1; /* a picture was shown since the last table: the next table starts a frame */
static int skipFrame;    /* the rest of a dropped frame (DropUnfinished) is not recorded, up to its picture */
static int frozen;
static int displayEnabled = 1;
static int taking;       /* RECORD_WITH_TAB: between the Tab that started a recording and the one that stops it */
static int generation;
static char savedDir[1024];

/* Video-memory snapshots, shared by the frames drawn while it did not change. */
#define VRAM_PAGE_WORDS 2048
#define VRAM_PAGES (sizeof(psxVram) / (VRAM_PAGE_WORDS * sizeof(uint16_t)))
typedef struct VramPage {
    int users;
    uint16_t pixels[VRAM_PAGE_WORDS];
} VramPage;
typedef struct Snapshot {
    VramPage *pages[VRAM_PAGES];
    int users;
} Snapshot;
#define SNAPSHOTS (FRAMES + 2)
static Snapshot snapshots[SNAPSHOTS];
static unsigned vramVersion = 1, snapshotVersion;
static int lastSnapshot = -1;
static uint16_t *snapshotPixels;
static int decodedSnapshot = -1;
static uint8_t *decodedPicture;
static unsigned decodedPictureToken;
/* Saved pictures already exist on disk. Keep their paths, not another copy of
   every full-resolution JPEG while the recorder is frozen. */
static struct SavedPicture { char *path; unsigned token; } savedPictures[FRAMES];
static void TrimRecorderMemory(void)
{
    RenderTrimMemory();
#ifdef __APPLE__
    malloc_zone_pressure_relief(NULL, 0);
#endif
}

/* What built the packets: address ranges, rebuilt for each frame the game builds. */
typedef struct Tag {
    const char *start, *end;
    char text[48];
} Tag;
static Tag *tags;
static int tagCount, tagCapacity, tagsSorted, tagsStale;

/* RECORD_*: the settings', or the tests' (WIPEOUT_FRAMECAP: the last seconds, =tab recording with Tab; the
   hidden runs have none otherwise) */
int FrameCapMode(void)
{
    static int forced = -1;

    if (forced < 0) {
        const char *value = SDL_getenv("WIPEOUT_FRAMECAP");

        forced = !value ? RECORD_OFF : !SDL_strcasecmp(value, "tab") ? RECORD_WITH_TAB : RECORD_LAST_SECONDS;
    }
    return forced ? forced : portConfig.hidden ? RECORD_OFF : portSettings.recordFrames;
}

int FrameCapRecording(void)
{
    return displayEnabled && (FrameCapMode() == RECORD_LAST_SECONDS ? !frozen :
                              FrameCapMode() == RECORD_WITH_TAB && taking);
}

void FrameCapDisplayEnabled(int enabled)
{
    if (displayEnabled != enabled) {
        displayEnabled = enabled;
        current = -1;
        newFrame = 1;
    }
}

int FrameCapTaking(void)
{
    return FrameCapMode() == RECORD_WITH_TAB && taking;
}

int FrameCapViewable(void)
{
    return count > 0 && (frozen || (FrameCapMode() == RECORD_WITH_TAB && !taking));
}

int FrameCapGeneration(void)
{
    return generation;
}

int FrameCapFrozen(void)
{
    return frozen;
}

const char *FrameCapSavedDir(void)
{
    return savedDir[0] ? savedDir : NULL;
}

int FrameCapFrameCount(void)
{
    return count;
}

const FrameCapFrame *FrameCapFrameAt(int index)
{
    return index >= 0 && index < count ? &frames[(first + index) % FRAMES] : NULL;
}

int FrameCapPictureSkipped(int index)
{
    const FrameCapFrame *f = FrameCapFrameAt(index);
    return f && skippedPictures[f - frames];
}

const uint16_t *FrameCapVram(int snapshot)
{
    if (snapshot < 0 || snapshot >= SNAPSHOTS || !snapshots[snapshot].users) return NULL;
    if (!snapshotPixels) snapshotPixels = malloc(sizeof(psxVram));
    if (!snapshotPixels) return NULL;
    if (decodedSnapshot != snapshot) {
        for (size_t i = 0; i < VRAM_PAGES; i++)
            memcpy(snapshotPixels + i * VRAM_PAGE_WORDS, snapshots[snapshot].pages[i]->pixels,
                   VRAM_PAGE_WORDS * sizeof(uint16_t));
        decodedSnapshot = snapshot;
    }
    return snapshotPixels;
}

void FrameCapVramChanged(void)
{
    vramVersion++;
}

static void Release(int snapshot)
{
    if (snapshot >= 0 && --snapshots[snapshot].users <= 0) {
        for (size_t i = 0; i < VRAM_PAGES; i++) {
            VramPage *page = snapshots[snapshot].pages[i];
            if (!--page->users) free(page);
            snapshots[snapshot].pages[i] = NULL;
        }
        if (decodedSnapshot == snapshot) decodedSnapshot = -1;
        snapshots[snapshot].users = 0;
        if (snapshot == lastSnapshot) {
            lastSnapshot = -1;
        }
    }
}

static int TakeSnapshot(void)
{
    int i;

    if (lastSnapshot >= 0 && snapshotVersion == vramVersion) {
        snapshots[lastSnapshot].users++;
        return lastSnapshot;
    }
    for (i = 0; i < SNAPSHOTS && snapshots[i].users; i++) {
    }
    if (i == SNAPSHOTS) {
        return -1;
    }
    for (size_t p = 0; p < VRAM_PAGES; p++) {
        VramPage *page = lastSnapshot >= 0 ? snapshots[lastSnapshot].pages[p] : NULL;
        const uint8_t *source = (const uint8_t *)psxVram + p * VRAM_PAGE_WORDS * sizeof(uint16_t);
        if (page && !memcmp(page->pixels, source, sizeof(page->pixels))) {
            page->users++;
        } else {
            page = malloc(sizeof(*page));
            if (!page) abort();
            page->users = 1;
            memcpy(page->pixels, source, sizeof(page->pixels));
        }
        snapshots[i].pages[p] = page;
    }
    snapshots[i].users = 1;
    if (decodedSnapshot == i) decodedSnapshot = -1;
    lastSnapshot = i;
    snapshotVersion = vramVersion;
    return i;
}

/* ---- the pictures: read back, compressed by worker threads ---- */

typedef struct RawPicture RawPicture;
typedef struct Job {
    uint8_t *pixels; /* RGBA */
    int w, h, slot;
    unsigned token;
    RawPicture *raw; /* shared while one identical picture is being encoded */
} Job;
#define JOBS 3 /* (each a whole picture uncompressed: 24 MB at 8x) */
static Job jobs[JOBS];
static int jobFirst, jobCount, jobsRunning;
static SDL_Mutex *lockMutex;
static SDL_Condition *jobWaiting, *jobDone;
static int workersStarted, workerCount;
static unsigned nextToken;
static int readSlot = -1;
static unsigned readToken;

static SDL_Mutex *Lock(void)
{
    if (!lockMutex) {
        lockMutex = SDL_CreateMutex();
        jobWaiting = SDL_CreateCondition();
        jobDone = SDL_CreateCondition();
    }
    return lockMutex;
}

typedef struct Output {
    uint8_t *data;
    size_t size, capacity;
} Output;

typedef struct EncodedPicture {
    uint8_t *data;
    size_t size;
    unsigned users;
} EncodedPicture;
static EncodedPicture *encodedPictures[FRAMES], *lastEncodedPicture;
struct RawPicture {
    unsigned users;
    uint8_t *pixels;
    int width, height;
    EncodedPicture *encoded;
};
static RawPicture *lastRawPicture;
static struct { RawPicture *raw; unsigned token; } pendingPictures[FRAMES];

static void ReleaseEncoded(EncodedPicture *picture)
{
    if (picture && !--picture->users) {
        if (lastEncodedPicture == picture) lastEncodedPicture = NULL;
        free(picture->data);
        free(picture);
    }
}

static void ReleaseRaw(RawPicture *raw)
{
    if (raw && !--raw->users) {
        ReleaseEncoded(raw->encoded);
        free(raw->pixels);
        free(raw);
    }
}

/* Caller holds the recorder lock, or has drained all compression jobs. */
static void ReleaseJpeg(FrameCapFrame *f)
{
    EncodedPicture *picture = encodedPictures[f - frames];
    if (picture) {
        ReleaseEncoded(picture);
        encodedPictures[f - frames] = NULL;
    } else free(f->jpeg);
    f->jpeg = NULL;
}

/* Caller holds the recorder lock. A pending frame keeps its own token, even
   when its identical pixels share an in-flight compression job. */
static void ShareRawPicture(FrameCapFrame *f, RawPicture *raw)
{
    ReleaseJpeg(f);
    int slot = (int)(f - frames);
    pendingPictures[slot].raw = NULL;
    if (raw->encoded) {
        EncodedPicture *picture = raw->encoded;
        picture->users++;
        encodedPictures[slot] = picture;
        f->jpeg = picture->data;
        f->jpegSize = picture->size;
    } else {
        pendingPictures[slot].raw = raw;
        pendingPictures[slot].token = f->pictureToken;
    }
}

static void Write(void *context, void *data, int size)
{
    Output *o = (Output *)context;

    if (o->size + (size_t)size > o->capacity) {
        o->capacity = SDL_max(o->capacity * 2, o->size + (size_t)size + 65536);
        o->data = realloc(o->data, o->capacity);
    }
    memcpy(o->data + o->size, data, (size_t)size);
    o->size += (size_t)size;
}

/* A job's picture compressed, and given to its frame if that is still the frame it was taken for. */
static void Encode(Job *job)
{
    Output out = {NULL, 0, 0};
    FrameCapFrame *f = &frames[job->slot];

#ifdef PORT_HAVE_TURBOJPEG
    /* Each job owns its compressor. Keep 4:4:4 chroma and the same quality as
       the fallback, with the accurate DCT for small text and pixel artwork. */
    tjhandle compressor = tjInitCompress();
    unsigned char *jpeg = NULL;
    unsigned long bytes = 0;
    int compressed = compressor && !tjCompress2(compressor, job->pixels, job->w, 0, job->h,
        TJPF_RGBA, &jpeg, &bytes, TJSAMP_444, PICTURE_QUALITY, TJFLAG_ACCURATEDCT);
    if (compressed) Write(&out, jpeg, (int)bytes);
    tjFree(jpeg);
    if (compressor) tjDestroy(compressor);
#endif
    /* Allocation/codec failures retain the portable encoder, without lowering quality. */
    if (!out.data && !stbi_write_jpg_to_func(Write, &out, job->w, job->h, 4, job->pixels, PICTURE_QUALITY)) {
        free(out.data);
        out.data = NULL;
    }
    if (out.data) {
        uint8_t *fitted = realloc(out.data, out.size);
        if (fitted) out.data = fitted;
    }
    if (!job->raw) free(job->pixels);
    SDL_LockMutex(Lock());
    if (out.data && (job->raw || f->pictureToken == job->token)) {
        EncodedPicture *picture = lastEncodedPicture;
        if (picture && picture->size == out.size && !memcmp(picture->data, out.data, out.size)) {
            picture->users++;
            free(out.data);
        } else {
            picture = malloc(sizeof(*picture));
            if (!picture) abort();
            *picture = (EncodedPicture){out.data, out.size, 1};
            lastEncodedPicture = picture;
        }
        if (job->raw) {
            job->raw->encoded = picture; /* job/cache owns the initial reference */
            for (int i = 0; i < FRAMES; i++) {
                if (pendingPictures[i].raw != job->raw) continue;
                if (frames[i].pictureToken == pendingPictures[i].token)
                    ShareRawPicture(&frames[i], job->raw);
                else pendingPictures[i].raw = NULL;
            }
        } else {
            ReleaseJpeg(f);
            encodedPictures[f - frames] = picture;
            f->jpeg = picture->data;
            f->jpegSize = picture->size;
        }
    } else {
        free(out.data);
    }
    if (job->raw && !job->raw->encoded) {
        for (int i = 0; i < FRAMES; i++)
            if (pendingPictures[i].raw == job->raw) pendingPictures[i].raw = NULL;
        if (lastRawPicture == job->raw) {
            ReleaseRaw(lastRawPicture);
            lastRawPicture = NULL;
        }
    }
    ReleaseRaw(job->raw);
    SDL_UnlockMutex(lockMutex);
}

static int SDLCALL Worker(void *unused)
{
    (void)unused;
    SDL_LockMutex(lockMutex);
    for (;;) {
        Job job;

        while (!jobCount) {
            SDL_WaitCondition(jobWaiting, lockMutex);
        }
        job = jobs[jobFirst];
        jobFirst = (jobFirst + 1) % JOBS;
        jobCount--;
        jobsRunning++;
        SDL_UnlockMutex(lockMutex);
        Encode(&job);
        SDL_LockMutex(lockMutex);
        jobsRunning--;
        SDL_BroadcastCondition(jobDone);
    }
    return 0;
}

/* The picture read back: a copy queued for the workers (or compressed here when there are none). */
static void ReadBack(const uint8_t *pixels, int w, int h, void *user)
{
    Job job;

    (void)user;
    if (readSlot < 0) {
        return;
    }
    size_t size = (size_t)w * h * 4;
    SDL_LockMutex(Lock());
    if (frames[readSlot].pictureToken != readToken) {
        readSlot = -1;
        SDL_UnlockMutex(lockMutex);
        return;
    }
    if (lastRawPicture && lastRawPicture->width == w && lastRawPicture->height == h &&
        !memcmp(lastRawPicture->pixels, pixels, size)) {
        ShareRawPicture(&frames[readSlot], lastRawPicture);
        readSlot = -1;
        SDL_UnlockMutex(lockMutex);
        return;
    }
    /* Recording is best effort. A full encoder queue must never stall game
       updates or presentation. Keep geometry/VRAM inspection for this frame. */
    if (workerCount && jobCount == JOBS) {
        skippedPictures[readSlot] = 1;
        readSlot = -1;
        SDL_UnlockMutex(lockMutex);
        return;
    }
    uint8_t *recycled = NULL;
    if (lastRawPicture && lastRawPicture->users == 1 && lastRawPicture->width == w && lastRawPicture->height == h) {
        recycled = lastRawPicture->pixels;
        lastRawPicture->pixels = NULL;
        ReleaseRaw(lastRawPicture);
        lastRawPicture = NULL;
    }
    SDL_UnlockMutex(lockMutex);
    RawPicture *raw = calloc(1, sizeof(*raw));
    if (!raw) { free(recycled); return; }
    raw->pixels = recycled ? recycled : malloc(size);
    if (!raw->pixels) { free(raw); return; }
    memcpy(raw->pixels, pixels, size);
    raw->width = w; raw->height = h;
    raw->users = 2; /* the latest-picture cache and the queued/running job */
    job = (Job){.pixels = raw->pixels, .w = w, .h = h, .slot = readSlot, .token = readToken, .raw = raw};
    SDL_LockMutex(lockMutex);
    ReleaseRaw(lastRawPicture);
    lastRawPicture = raw;
    ShareRawPicture(&frames[readSlot], raw);
    readSlot = -1;
    SDL_UnlockMutex(lockMutex);
    if (!workersStarted) {
        int i, cores = SDL_GetNumLogicalCPUCores();

        workersStarted = 1;
        for (i = 0; i < SDL_clamp(cores - 2, 1, 8); i++) {
            SDL_Thread *thread = SDL_CreateThread(Worker, "framecap", NULL);

            if (thread) {
                SDL_DetachThread(thread);
                workerCount++;
            }
        }
    }
    if (!workerCount) {
        Encode(&job);
        return;
    }
    SDL_LockMutex(lockMutex);
    /* Only this thread produces jobs; consumers can only make more room
       after the admission check above. */
    jobs[(jobFirst + jobCount) % JOBS] = job;
    jobCount++;
    SDL_SignalCondition(jobWaiting);
    SDL_UnlockMutex(lockMutex);
}

/* The picture on its way back from the GPU (with `wait`, waiting for it). */
static void FinishRead(int wait)
{
    GpuReadPictureDone(GPU_READ_RECORDER, wait, ReadBack, NULL);
}

/* Every picture read back and compressed (before the recording is looked at or saved). */
static void FinishPictures(void)
{
    FinishRead(1);
    if (!lockMutex) {
        return;
    }
    SDL_LockMutex(lockMutex);
    while (jobCount || jobsRunning) {
        SDL_WaitCondition(jobDone, lockMutex);
    }
    ReleaseRaw(lastRawPicture);
    lastRawPicture = NULL;
    SDL_UnlockMutex(lockMutex);
}

/* A frame's picture forgotten (its slot is used again, or the recording cleared). */
static void ForgetPicture(FrameCapFrame *f)
{
    SDL_LockMutex(Lock());
    ReleaseJpeg(f);
    pendingPictures[f - frames].raw = NULL;
    skippedPictures[f - frames] = 0;
    f->jpegSize = 0;
    f->pictureToken = 0;
    SDL_free(savedPictures[f - frames].path);
    memset(&savedPictures[f - frames], 0, sizeof(savedPictures[0]));
    SDL_UnlockMutex(lockMutex);
}

static const uint8_t *PictureJpeg(const FrameCapFrame *f, size_t *size, void **loaded)
{
    *loaded = NULL;
    *size = f->jpegSize;
    if (f->jpeg) return f->jpeg;
    const struct SavedPicture *saved = &savedPictures[f - frames];
    if (!saved->path || saved->token != f->pictureToken) return NULL;
    *loaded = SDL_LoadFile(saved->path, size);
    return *loaded;
}

/* Only drop the RAM copy after a complete successful write. Re-saving a
   file-backed frame reads it before opening the destination, even at the same path. */
static int SavePicture(FrameCapFrame *f, const char *path)
{
    const struct SavedPicture *existing = &savedPictures[f - frames];
    if (!f->jpeg && existing->path && existing->token == f->pictureToken && !strcmp(existing->path, path))
        return 1;
    size_t size;
    void *loaded;
    const uint8_t *jpeg = PictureJpeg(f, &size, &loaded);
    if (!jpeg || !f->shown) { SDL_free(loaded); return 0; }
    FILE *file = fopen(path, "wb");
    if (!file) { SDL_free(loaded); return 0; }
    size_t written = fwrite(jpeg, 1, size, file);
    int closed = fclose(file);
    SDL_free(loaded);
    if (written != size || closed) return 0;
    char *copy = SDL_strdup(path);
    if (!copy) return 0;
    struct SavedPicture *saved = &savedPictures[f - frames];
    SDL_free(saved->path);
    saved->path = copy;
    saved->token = f->pictureToken;
    ReleaseJpeg(f);
    return 1;
}

int FrameCapPictureReady(int index)
{
    const FrameCapFrame *f = FrameCapFrameAt(index);
    int ready;
    if (!f) return 0;
    SDL_LockMutex(Lock());
    const struct SavedPicture *saved = &savedPictures[f - frames];
    ready = f->shown && (f->jpeg || (saved->path && saved->token == f->pictureToken));
    SDL_UnlockMutex(lockMutex);
    return ready;
}

const uint8_t *FrameCapPicture(int index, int *w, int *h)
{
    const FrameCapFrame *f;
    const uint8_t *jpeg;
    size_t size;
    unsigned token;
    int n;

    if (index < 0 || index >= count) {
        return NULL;
    }
    f = FrameCapFrameAt(index);
    if (!f->shown) return NULL;
    SDL_LockMutex(Lock());
    token = f->pictureToken;
    if (!decodedPicture || decodedPictureToken != token) {
        int dw, dh;
        void *loaded;
        jpeg = PictureJpeg(f, &size, &loaded);
        if (!jpeg) { SDL_UnlockMutex(lockMutex); return NULL; }
        stbi_image_free(decodedPicture);
        decodedPicture = stbi_load_from_memory(jpeg, (int)size, &dw, &dh, &n, 3);
        SDL_free(loaded);
        decodedPictureToken = token;
        if (decodedPicture && (dw != f->pictureW || dh != f->pictureH)) {
            stbi_image_free(decodedPicture);
            decodedPicture = NULL;
        }
    }
    SDL_UnlockMutex(lockMutex);
    *w = f->pictureW;
    *h = f->pictureH;
    return decodedPicture;
}

/* The next frame of the ring (the oldest given up when it is full). */
static FrameCapFrame *NewFrame(void)
{
    FrameCapFrame *f;
    FrameCapPrim *reusable = NULL;
    int reusableCapacity = 0;

    while (count >= Capacity()) {
        f = &frames[first];
        if (betweenSaved == first) {
            betweenSaved = -1;
        }
        Release(f->vram);
        ForgetPicture(f);
        /* Move storage with the active history, rather than retaining an
           allocation in every one of the 1200 possible ring slots. */
        free(reusable);
        reusable = f->prims;
        reusableCapacity = f->capacity;
        f->prims = NULL;
        f->count = f->capacity = 0;
        first = (first + 1) % FRAMES;
        count--;
    }
    f = &frames[(first + count) % FRAMES];
    if (reusable) {
        free(f->prims);
        f->prims = reusable;
        f->capacity = reusableCapacity;
    }
    count++;
    f->number = ++nextNumber;
    return f;
}

void FrameCapTable(int offsetX, int offsetY, int areaX, int areaY, int areaW, int areaH)
{
    FrameCapFrame *f;

    tagsStale = 1; /* the game builds the next frame's packets after this */
    if (!FrameCapRecording()) {
        current = -1;
        return;
    }
    if (!newFrame && current >= 0) {
        return; /* the second table (the HUD) of the same frame */
    }
    if (skipFrame && !newFrame) {
        return;
    }
    skipFrame = 0;
    newFrame = 0;
    f = NewFrame();
    current = (int)(f - frames);
    f->gameFrame = PortGameFrames();
    f->between = 0.0f;
    f->shown = 0;
    f->offsetX = offsetX;
    f->offsetY = offsetY;
    f->areaX = areaX;
    f->areaY = areaY;
    f->areaW = areaW;
    f->areaH = areaH;
    f->count = 0;
    f->vram = TakeSnapshot();
    ForgetPicture(f);
}

static int CompareTags(const void *a, const void *b)
{
    const Tag *x = a, *y = b;

    return x->start < y->start ? -1 : x->start > y->start;
}

static const char *TagOf(const void *address)
{
    const char *p = address;
    int low = 0, high = tagCount - 1;

    if (!tagsSorted) {
        qsort(tags, (size_t)tagCount, sizeof(Tag), CompareTags);
        tagsSorted = 1;
    }
    while (low <= high) {
        int mid = (low + high) / 2;

        if (p < tags[mid].start) {
            high = mid - 1;
        } else if (p >= tags[mid].end) {
            low = mid + 1;
        } else {
            return tags[mid].text;
        }
    }
    return "";
}

FrameCapPrim *FrameCapAdd(void)
{
    FrameCapFrame *f;
    FrameCapPrim *p;

    if (current < 0 || !FrameCapRecording()) {
        return NULL;
    }
    f = &frames[current];
    if (f->count == f->capacity) {
        f->capacity = f->capacity ? f->capacity * 2 : 4096;
        f->prims = realloc(f->prims, (size_t)f->capacity * sizeof(FrameCapPrim));
    }
    p = &f->prims[f->count];
    memset(p, 0, sizeof(*p));
    p->serial = (uint32_t)f->count++;
    return p;
}

/* libgpu fills address in; the tag follows from it. */
void FrameCapTagRecord(FrameCapPrim *p)
{
    SDL_strlcpy(p->tag, TagOf((const void *)(uintptr_t)p->address), sizeof(p->tag));
}

void FrameCapTag(const void *start, const void *end, const char *what, const char *name, int index)
{
    Tag *t;

    if (!FrameCapRecording() || end <= start) {
        return;
    }
    if (tagsStale) {
        tagCount = 0;
        tagsStale = 0;
    }
    if (tagCount == tagCapacity) {
        tagCapacity = tagCapacity ? tagCapacity * 2 : 8192;
        tags = realloc(tags, (size_t)tagCapacity * sizeof(Tag));
    }
    t = &tags[tagCount++];
    t->start = start;
    t->end = end;
    if (name) {
        char clean[17];
        int i;

        for (i = 0; i < 16 && name[i]; i++) {
            clean[i] = name[i] >= ' ' && name[i] < 127 ? name[i] : '?';
        }
        clean[i] = 0;
        SDL_snprintf(t->text, sizeof(t->text), "%s %s #%d", what, clean, index);
    } else {
        SDL_snprintf(t->text, sizeof(t->text), "%s #%d", what, index);
    }
    tagsSorted = 0;
}

void PortTagPrims(const void *start, const void *end, const char *what, const char *name, int index)
{
    FrameCapTag(start, end, what, name, index);
}

/* The picture at (x, y, w, h) read back for frame f: the console's pixels, and with widescreen the sides as well. */
static void TakePicture(FrameCapFrame *f, int x, int y, int w, int h)
{
    int margin = GpuShownMargin(x, w), scale = SDL_max(GpuScale(), 1);
    int downsample = PortLinkDownsample(), style = PortLinkDownsampleStyle();
    int width, height;

    if (downsample < 0) downsample = portSettings.downsample;
    if (style < 0) style = portSettings.downsampleStyle;
    /* Box presentation already discarded the higher-resolution pixels. Keep
       that image directly instead of enlarging it before readback and JPEG.
       Adaptive retains fine detail and must stay at the rendering scale. */
    if (style == DOWNSAMPLE_BOX && downsample > 0) {
        scale = SDL_min(scale, downsample);
    }
    width = (PICTURE_W + 2 * margin) * scale;
    height = GPU_LINES * scale; /* (240 lines, or PAL's 256) */

    FinishRead(0); /* poll completed captures; presentation never waits for readback */
    ForgetPicture(f);
    f->pictureW = width;
    f->pictureH = height;
    f->pictureScale = scale;
    f->pictureX = -margin;
    f->shown = PortShownFrames() + 1;
    if (GpuReadPictureBusy(GPU_READ_RECORDER)) {
        skippedPictures[f - frames] = 1;
        lastPictureTicks = SDL_GetTicksNS();
        return;
    }
    SDL_LockMutex(Lock());
    f->pictureToken = ++nextToken;
    SDL_UnlockMutex(lockMutex);
    readSlot = (int)(f - frames);
    readToken = f->pictureToken;
    if (!GpuReadPictureStart(GPU_READ_RECORDER, x, y, w, h, margin, width, height)) {
        skippedPictures[f - frames] = 1;
        readSlot = -1;
    }
    lastPictureTicks = SDL_GetTicksNS();
}

int FrameCapBetween(float along, int offsetX, int offsetY, int areaX, int areaY, int areaW, int areaH)
{
    FrameCapFrame *f;
    uint64_t now = SDL_GetTicksNS();

    if (!FrameCapRecording() || Rate() <= 30 || inBetween ||
        now - lastPictureTicks < 1000000000ull / (uint64_t)Rate() * 9 / 10) {
        return 0;
    }
    betweenSaved = current;
    f = NewFrame();
    f->gameFrame = PortGameFrames();
    f->between = SDL_clamp(along, 0.001f, 0.999f);
    f->shown = 0;
    f->offsetX = offsetX;
    f->offsetY = offsetY;
    f->areaX = areaX;
    f->areaY = areaY;
    f->areaW = areaW;
    f->areaH = areaH;
    f->count = 0;
    f->vram = TakeSnapshot(); /* (the frame before's, unless the game loaded something since) */
    ForgetPicture(f);
    current = (int)(f - frames);
    inBetween = 1;
    return 1;
}

void FrameCapBetweenShown(int x, int y, int w, int h)
{
    if (!inBetween) {
        return;
    }
    if (current >= 0 && frames[current].between > 0.0f) {
        TakePicture(&frames[current], x, y, w, h);
    }
    current = betweenSaved;
    betweenSaved = -1;
    inBetween = 0;
}

void FrameCapShown(int x, int y, int w, int h)
{
    int i;

    newFrame = 1;
    if (!FrameCapRecording() || w <= 0 || h <= 0) {
        return;
    }
    /* the newest of the game's frames drawn where the picture is taken from, not yet shown */
    for (i = count - 1; i >= 0; i--) {
        FrameCapFrame *f = &frames[(first + i) % FRAMES];

        if (f->between > 0.0f) {
            continue;
        }
        if (x >= f->areaX && x < f->areaX + f->areaW && y >= f->areaY && y < f->areaY + f->areaH) {
            if (!f->shown) {
                TakePicture(f, x, y, w, h);
            }
            return;
        }
    }
}

/* ---- explaining a pixel ---- */

static int Inside(const GpuVertex *a, const GpuVertex *b, const GpuVertex *c, float x, float y, float l[3])
{
    float d = (b->y - c->y) * (a->x - c->x) + (c->x - b->x) * (a->y - c->y);

    if (fabsf(d) < 1e-6f) {
        return 0;
    }
    l[0] = ((b->y - c->y) * (x - c->x) + (c->x - b->x) * (y - c->y)) / d;
    l[1] = ((c->y - a->y) * (x - c->x) + (a->x - c->x) * (y - c->y)) / d;
    l[2] = 1.0f - l[0] - l[1];
    return l[0] >= -1e-4f && l[1] >= -1e-4f && l[2] >= -1e-4f;
}

static uint16_t Fetch(const uint16_t *vram, int x, int y)
{
    return vram[(y & (VRAM_HEIGHT - 1)) * VRAM_WIDTH + (x & (VRAM_WIDTH - 1))];
}

/* What the fragment shader of gpu_gl.c computes at barycentric l. */
static void Shade(const FrameCapPrim *p, const GpuVertex *t[3], const float l[3], const uint16_t *vram, FrameCapHit *h)
{
    float k[3], sum = 0, c[4] = {0, 0, 0, 0};
    int i;

    for (i = 0; i < 3; i++) {
        float w = t[i]->w > 0 ? t[i]->w : 1.0f;
        k[i] = l[i] / w;
        sum += k[i];
    }
    for (i = 0; i < 3; i++) {
        k[i] /= sum;
        h->u += k[i] * t[i]->u;
        h->v += k[i] * t[i]->v;
        c[0] += k[i] * t[i]->r;
        c[1] += k[i] * t[i]->g;
        c[2] += k[i] * t[i]->b;
        c[3] += k[i] * t[i]->a;
    }
    h->alpha = c[3];
    h->texel = -1;
    if (!p->draw.textured || p->draw.depth == 3 || !vram) {
        h->r = c[0];
        h->g = c[1];
        h->b = c[2];
        return;
    }
    {
        int u = (int)floorf(h->u) & 255, v = (int)floorf(h->v) & 255, texel;

        u = (u & ~(p->draw.windowMaskX * 8)) | ((p->draw.windowOffsetX & p->draw.windowMaskX) * 8);
        v = (v & ~(p->draw.windowMaskY * 8)) | ((p->draw.windowOffsetY & p->draw.windowMaskY) * 8);
        if (p->draw.depth == 0) {
            int index = (Fetch(vram, p->draw.pageX + (u >> 2), p->draw.pageY + v) >> ((u & 3) * 4)) & 15;
            texel = Fetch(vram, p->draw.clutX + index, p->draw.clutY);
        } else if (p->draw.depth == 1) {
            int index = (Fetch(vram, p->draw.pageX + (u >> 1), p->draw.pageY + v) >> ((u & 1) * 8)) & 255;
            texel = Fetch(vram, p->draw.clutX + index, p->draw.clutY);
        } else {
            texel = Fetch(vram, p->draw.pageX + u, p->draw.pageY + v);
        }
        h->texel = texel;
        h->transparent = texel == 0;
        {
            float t8[3] = {(texel & 31) * 8.0f, ((texel >> 5) & 31) * 8.0f, ((texel >> 10) & 31) * 8.0f};
            for (i = 0; i < 3; i++) {
                if (!p->draw.raw) {
                    t8[i] = SDL_min(t8[i] * c[i] / 128.0f, 255.0f);
                }
            }
            h->r = t8[0];
            h->g = t8[1];
            h->b = t8[2];
        }
    }
}

int FrameCapHits(int frame, int x, int y, FrameCapHit *hits, int max)
{
    const FrameCapFrame *f = FrameCapFrameAt(frame);
    const uint16_t *vram;
    int i, n = 0;
    float px = x + 0.5f, py = y + 0.5f;

    if (!f) {
        return 0;
    }
    vram = FrameCapVram(f->vram);
    for (i = 0; i < f->count && n < max; i++) {
        const FrameCapPrim *p = &f->prims[i];
        int triangles = p->kind == FRAMECAP_LINE ? 0 : p->kind == FRAMECAP_POLYGON && p->count == 3 ? 1 : 2, k;

        if (p->kind == FRAMECAP_FILL) {
            if (px >= p->v[0].x && px < p->v[1].x && py >= p->v[0].y && py < p->v[1].y) {
                memset(&hits[n], 0, sizeof(hits[n]));
                hits[n].prim = i;
                hits[n].texel = -1;
                hits[n].r = p->v[0].r;
                hits[n].g = p->v[0].g;
                hits[n].b = p->v[0].b;
                hits[n].alpha = 255;
                n++;
            }
            continue;
        }
        for (k = 0; k < triangles && n < max; k++) {
            const GpuVertex *t[3] = {&p->v[k], &p->v[k + 1], &p->v[k + 2]};
            float l[3];

            if (Inside(t[0], t[1], t[2], px, py, l)) {
                FrameCapHit *h = &hits[n++];

                memset(h, 0, sizeof(*h));
                h->prim = i;
                h->triangle = k;
                h->skipped = (p->flags & FRAMECAP_DROPPED) || (p->flags & (k ? FRAMECAP_SKIP1 : FRAMECAP_SKIP0));
                Shade(p, t, l, vram, h);
                if (p->draw.semi == 4 && ((x + y) & 1)) h->transparent = 1;
            }
        }
    }
    return n;
}

/* ---- saving ---- */

static void Put(FILE *f, const void *data, size_t bytes)
{
    fwrite(data, 1, bytes, f);
}

static void PutI32(FILE *f, int32_t v)
{
    Put(f, &v, 4);
}

/* A rapid re-save can reuse an export directory while ring indices have
   changed. Read all backing images before overwriting any indexed files. */
static int ProtectBackingPictures(const char *directory)
{
    size_t length = strlen(directory);
    int overlap = 0;
    for (int i = 0; i < count; i++) {
        const struct SavedPicture *saved = &savedPictures[(first + i) % FRAMES];
        if (saved->path && !strncmp(saved->path, directory, length) && saved->path[length] == '/') {
            char destination[1200];
            SDL_snprintf(destination, sizeof(destination), "%s/picture_%03d.jpg", directory, i);
            if (strcmp(saved->path, destination)) overlap = 1;
        }
    }
    if (!overlap) return 1;
    for (int i = 0; i < count; i++) {
        FrameCapFrame *f = &frames[(first + i) % FRAMES];
        if (!f->jpeg && savedPictures[f - frames].path) {
            void *loaded;
            size_t size;
            const uint8_t *data = PictureJpeg(f, &size, &loaded);
            if (!data) return 0;
            f->jpeg = malloc(size);
            if (f->jpeg) memcpy(f->jpeg, data, size);
            SDL_free(loaded);
            if (!f->jpeg) return 0;
            f->jpegSize = size;
        }
    }
    return 1;
}

static void Save(void)
{
    FinishPictures();
    char *base = SDL_GetPrefPath("Psygnosis", "WipeoutXL");
    const char *forced = SDL_getenv("WIPEOUT_FRAMECAP_DIR");
    char path[1200];
    time_t now = time(NULL);
    FILE *file;
    int i, k;

    if (forced) {
        SDL_strlcpy(savedDir, forced, sizeof(savedDir));
    } else if (base) {
        char stamp[64];

        strftime(stamp, sizeof(stamp), "%Y-%m-%d_%H-%M-%S", localtime(&now));
        SDL_snprintf(savedDir, sizeof(savedDir), "%sframes/%s", base, stamp);
    }
    SDL_free(base);
    if (!savedDir[0]) {
        return;
    }
    SDL_CreateDirectory(savedDir);
    if (!ProtectBackingPictures(savedDir)) return;
    SDL_snprintf(path, sizeof(path), "%s/frames.bin", savedDir);
    if (!(file = fopen(path, "wb"))) {
        savedDir[0] = 0;
        return;
    }
    /* header: magic, version, frames; per frame its fields, then its primitives (see tools/framecap.py) */
    Put(file, "XLFC", 4);
    PutI32(file, 3);
    PutI32(file, count);
    for (i = 0; i < count; i++) {
        const FrameCapFrame *f = FrameCapFrameAt(i);
        int64_t game = (int64_t)f->gameFrame;

        Put(file, &game, 8);
        PutI32(file, f->shown);
        PutI32(file, f->offsetX);
        PutI32(file, f->offsetY);
        PutI32(file, f->areaX);
        PutI32(file, f->areaY);
        PutI32(file, f->areaW);
        PutI32(file, f->areaH);
        PutI32(file, f->vram);
        PutI32(file, f->count);
        PutI32(file, f->pictureX); /* version 2 */
        Put(file, &f->between, 4); /* version 3 */
        for (k = 0; k < f->count; k++) {
            const FrameCapPrim *p = &f->prims[k];
            int j;

            PutI32(file, (int32_t)p->serial);
            Put(file, &p->kind, 1);
            Put(file, &p->code, 1);
            Put(file, &p->flags, 1);
            Put(file, &p->count, 1);
            Put(file, &p->tpage, 2);
            Put(file, &p->clut, 2);
            Put(file, p->ix, 8);
            Put(file, p->iy, 8);

            for (j = 0; j < 4; j++) {
                const GpuVertex *v = &p->v[j];
                Put(file, &v->x, 4);
                Put(file, &v->y, 4);
                Put(file, &v->w, 4);
                Put(file, &v->r, 1);
                Put(file, &v->g, 1);
                Put(file, &v->b, 1);
                Put(file, &v->a, 1);
                Put(file, &v->u, 4);
                Put(file, &v->v, 4);
                {
                    /* left out of the depth buffer (1: the sky, corners found by their pixels); 2: without the
                       slack (seen from behind, or GteSetExactDepth) */
                    uint8_t mark = (uint8_t)(v->noDepth ? 1 : v->backFacing || v->exactDepth ? 2 : 0);
                    Put(file, &mark, 1);
                }
            }
            {
                const int *d = &p->draw.textured;
                for (j = 0; j < 14; j++) {
                    PutI32(file, d[j]);
                }
                PutI32(file, 0); /* ... and of the depth test */
            }
            Put(file, &p->address, 8);
            Put(file, p->tag, 48);
        }
    }
    fclose(file);
    for (i = 0; i < SNAPSHOTS; i++) {
        if (snapshots[i].users) {
            SDL_snprintf(path, sizeof(path), "%s/vram_%d.bin", savedDir, i);
            if ((file = fopen(path, "wb"))) {
                Put(file, FrameCapVram(i), sizeof(psxVram));
                fclose(file);
            }
        }
    }
    for (i = 0; i < count; i++) {
        FrameCapFrame *f = &frames[(first + i) % FRAMES];
        SDL_snprintf(path, sizeof(path), "%s/picture_%03d.jpg", savedDir, i);
        SavePicture(f, path);
    }
    /* what the game was doing (game/src/portinfo.c) and how it was shown */
    SDL_snprintf(path, sizeof(path), "%s/info.json", savedDir);
    if ((file = fopen(path, "w"))) {
        char info[2048];
        int n = PortGameInfo(info, (int)sizeof(info));
        int skipped = 0;
        for (int frame = 0; frame < count; frame++)
            skipped += skippedPictures[(first + frame) % FRAMES] != 0;

        fprintf(file, "{\n  \"version\": \"%s\",\n  %.*s  \"widescreen\": %d,\n  \"frame rate\": %d,\n"
                      "  \"console depth\": %d,\n  \"recorded frames\": %d,\n  \"skipped previews\": %d\n}\n",
                PORT_VERSION, n, info, portSettings.widescreen, portSettings.frameRate, portSettings.consoleDepth,
                count, skipped);
        fclose(file);
    }
    PortLog("frame recorder: %d frames saved to %s\n", count, savedDir);
}

void FrameCapSave(void)
{
    Save();
    TrimRecorderMemory();
}

/* The frame being built when the recording stops: never finished nor shown, nothing to look at. */
static void DropUnfinished(void)
{
    if (count > 0 && current == (first + count - 1) % FRAMES && !frames[current].shown) {
        Release(frames[current].vram);
        count--;
        skipFrame = 1;
    }
    current = -1;
}

void FrameCapToggleFreeze(void)
{
    if (frozen) {
        frozen = 0;
        return;
    }
    DropUnfinished();
    if (!count) {
        return;
    }
    frozen = 1;
    PsxPadStopRumble(); /* Fields stop while inspecting frames, including the motor expiry updates. */
    generation++;
    Save();
    TrimRecorderMemory();
}

void FrameCapClear(void)
{
    int i;

    FinishPictures();
    for (i = 0; i < count; i++) {
        Release(frames[(first + i) % FRAMES].vram);
    }
    /* A dropped unfinished frame can retain storage just outside the history. */
    for (i = 0; i < FRAMES; i++) {
        FrameCapFrame *f = &frames[i];
        ForgetPicture(f);
        free(f->prims);
        f->prims = NULL;
        f->count = f->capacity = 0;
    }
    free(snapshotPixels);
    snapshotPixels = NULL;
    stbi_image_free(decodedPicture);
    decodedPicture = NULL;
    decodedPictureToken = 0;
    decodedSnapshot = -1;
    first = count = 0;
    current = -1;
    newFrame = 1;
    skipFrame = 0;
    inBetween = 0;
    betweenSaved = -1;
    frozen = taking = 0;
    savedDir[0] = 0;
    generation++;
    TrimRecorderMemory();
}

void FrameCapTab(void)
{
    if (FrameCapMode() != RECORD_WITH_TAB) {
        FrameCapToggleFreeze();
    } else if (taking) {
        taking = 0;
        DropUnfinished();
        FinishPictures();
        generation++;
        TrimRecorderMemory();
    } else {
        FrameCapClear();
        taking = 1;
    }
}
