/* The frame recorder (framecap.h). */
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <SDL3/SDL.h>

#include "framecap.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_WRITE_NO_STDIO
#include "../third_party/stb/stb_image_write.h"
#define STBI_NO_STDIO
#include "../third_party/stb/stb_image.h" /* (its implementation is hdtex.c's) */
#include "../debug_ui.h"
#include "../platform.h"
#include "../settings.h"

#define LAST_SECONDS 3 /* RECORD_LAST_SECONDS: the last three seconds */
#define TAB_SECONDS 10 /* RECORD_WITH_TAB: up to the last ten */
#define FRAMES 1200    /* (at most: ten seconds of 120 pictures a second, five of 240) */
#define PICTURE_W 320
#define PICTURE_H 240
/* The picture is kept at the framebuffer's scale (the pixels drawn, not the console's: thin lines and seams
   show), read back without waiting for the GPU (gpu.h's GPU_READ_RECORDER) and compressed as a JPEG of this
   quality (above 90: colours not subsampled) by worker threads: 1 to 3 MB a frame at 4K. */
#define PICTURE_QUALITY 95

static FrameCapFrame frames[FRAMES];
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
static int taking;       /* RECORD_WITH_TAB: between the Tab that started a recording and the one that stops it */
static int generation;
static char savedDir[1024];

/* Video-memory snapshots, shared by the frames drawn while it did not change. */
typedef struct Snapshot {
    uint16_t *pixels;
    int users;
} Snapshot;
#define SNAPSHOTS (FRAMES + 2)
static Snapshot snapshots[SNAPSHOTS];
static unsigned vramVersion = 1, snapshotVersion;
static int lastSnapshot = -1;

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
    return FrameCapMode() == RECORD_LAST_SECONDS ? !frozen : FrameCapMode() == RECORD_WITH_TAB && taking;
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

const uint16_t *FrameCapVram(int snapshot)
{
    return snapshot >= 0 && snapshot < SNAPSHOTS ? snapshots[snapshot].pixels : NULL;
}

void FrameCapVramChanged(void)
{
    vramVersion++;
}

static void Release(int snapshot)
{
    if (snapshot >= 0 && --snapshots[snapshot].users <= 0) {
        free(snapshots[snapshot].pixels);
        snapshots[snapshot].pixels = NULL;
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
    for (i = 0; i < SNAPSHOTS && snapshots[i].pixels; i++) {
    }
    if (i == SNAPSHOTS) {
        return -1;
    }
    snapshots[i].pixels = malloc(sizeof(psxVram));
    memcpy(snapshots[i].pixels, psxVram, sizeof(psxVram));
    snapshots[i].users = 1;
    lastSnapshot = i;
    snapshotVersion = vramVersion;
    return i;
}

/* ---- the pictures: read back, compressed by worker threads ---- */

typedef struct Job {
    uint8_t *pixels; /* RGBA */
    int w, h, slot;
    unsigned token;
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

    if (!stbi_write_jpg_to_func(Write, &out, job->w, job->h, 4, job->pixels, PICTURE_QUALITY)) {
        free(out.data);
        out.data = NULL;
    } else {
        uint8_t *fitted = realloc(out.data, out.size); /* (no room to grow kept) */

        out.data = fitted ? fitted : out.data;
    }
    free(job->pixels);
    SDL_LockMutex(Lock());
    if (f->pictureToken == job->token && out.data) {
        free(f->jpeg);
        f->jpeg = out.data;
        f->jpegSize = out.size;
    } else {
        free(out.data);
    }
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
    job.pixels = malloc((size_t)w * h * 4);
    if (!job.pixels) {
        return;
    }
    memcpy(job.pixels, pixels, (size_t)w * h * 4);
    job.w = w;
    job.h = h;
    job.slot = readSlot;
    job.token = readToken;
    readSlot = -1;
    Lock();
    if (!workersStarted) {
        int i, cores = SDL_GetNumLogicalCPUCores();

        workersStarted = 1;
        for (i = 0; i < SDL_clamp(cores - 2, 1, 4); i++) {
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
    while (jobCount == JOBS) {
        SDL_WaitCondition(jobDone, lockMutex); /* (the workers are behind: the game waits) */
    }
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
    SDL_UnlockMutex(lockMutex);
}

/* A frame's picture forgotten (its slot is used again, or the recording cleared). */
static void ForgetPicture(FrameCapFrame *f)
{
    SDL_LockMutex(Lock());
    free(f->jpeg);
    f->jpeg = NULL;
    f->jpegSize = 0;
    f->pictureToken = 0;
    SDL_UnlockMutex(lockMutex);
}

const uint8_t *FrameCapPicture(int index, int *w, int *h)
{
    static uint8_t *decoded;
    static unsigned decodedToken;
    const FrameCapFrame *f;
    const uint8_t *jpeg;
    size_t size;
    unsigned token;
    int n;

    if (index < 0 || index >= count) {
        return NULL;
    }
    f = FrameCapFrameAt(index);
    SDL_LockMutex(Lock());
    jpeg = f->jpeg;
    size = f->jpegSize;
    token = f->pictureToken;
    SDL_UnlockMutex(lockMutex);
    if (!jpeg || !f->shown) {
        return NULL;
    }
    if (!decoded || decodedToken != token) {
        int dw, dh;

        stbi_image_free(decoded);
        decoded = stbi_load_from_memory(jpeg, (int)size, &dw, &dh, &n, 3);
        decodedToken = token;
        if (decoded && (dw != f->pictureW || dh != f->pictureH)) {
            stbi_image_free(decoded);
            decoded = NULL;
        }
    }
    *w = f->pictureW;
    *h = f->pictureH;
    return decoded;
}

/* The next frame of the ring (the oldest given up when it is full). */
static FrameCapFrame *NewFrame(void)
{
    FrameCapFrame *f;

    while (count >= Capacity()) {
        f = &frames[first];
        if (betweenSaved == first) {
            betweenSaved = -1;
        }
        Release(f->vram);
        ForgetPicture(f);
        first = (first + 1) % FRAMES;
        count--;
    }
    f = &frames[(first + count) % FRAMES];
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
    int width = (PICTURE_W + 2 * margin) * scale, height = PICTURE_H * scale;

    FinishRead(1); /* (the picture before's: back by now) */
    ForgetPicture(f);
    f->pictureW = width;
    f->pictureH = height;
    f->pictureScale = scale;
    f->pictureX = -margin;
    SDL_LockMutex(Lock());
    f->pictureToken = ++nextToken;
    SDL_UnlockMutex(lockMutex);
    readSlot = (int)(f - frames);
    readToken = f->pictureToken;
    GpuReadPictureStart(GPU_READ_RECORDER, x, y, w, h, margin, width, height);
    f->shown = PortShownFrames() + 1;
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
                    /* left out of the depth buffer (1: the sky, corners found by their pixels; 2: seen from behind) */
                    uint8_t mark = (uint8_t)(v->noDepth ? 1 : v->backFacing ? 2 : 0);
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
        if (snapshots[i].pixels) {
            SDL_snprintf(path, sizeof(path), "%s/vram_%d.bin", savedDir, i);
            if ((file = fopen(path, "wb"))) {
                Put(file, snapshots[i].pixels, sizeof(psxVram));
                fclose(file);
            }
        }
    }
    for (i = 0; i < count; i++) {
        const FrameCapFrame *f = FrameCapFrameAt(i);

        if (!f->jpeg || !f->shown) {
            continue;
        }
        SDL_snprintf(path, sizeof(path), "%s/picture_%03d.jpg", savedDir, i);
        if ((file = fopen(path, "wb"))) {
            Put(file, f->jpeg, f->jpegSize);
            fclose(file);
        }
    }
    PortLog("frame recorder: %d frames saved to %s\n", count, savedDir);
}

void FrameCapSave(void)
{
    Save();
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
    generation++;
    Save();
}

void FrameCapClear(void)
{
    int i;

    FinishPictures();
    for (i = 0; i < count; i++) {
        Release(frames[(first + i) % FRAMES].vram);
        ForgetPicture(&frames[(first + i) % FRAMES]); /* (the memory back) */
    }
    first = count = 0;
    current = -1;
    newFrame = 1;
    skipFrame = 0;
    inBetween = 0;
    betweenSaved = -1;
    frozen = taking = 0;
    savedDir[0] = 0;
    generation++;
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
    } else {
        FrameCapClear();
        taking = 1;
    }
}
