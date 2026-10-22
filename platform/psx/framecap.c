/* The frame recorder (framecap.h). */
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <SDL3/SDL.h>

#include "framecap.h"
#include "../debug_ui.h"
#include "../platform.h"
#include "../settings.h"

#define LAST_FRAMES 90 /* RECORD_LAST_SECONDS: three seconds of the game's 30 frames a second */
#define TAB_FRAMES 300 /* RECORD_WITH_TAB: up to the last ten */
#define FRAMES TAB_FRAMES
#define PICTURE_W 320
#define PICTURE_H 240

static FrameCapFrame frames[FRAMES];
static int first, count; /* the ring: frames[first], ... count of them */
static int current = -1; /* the frame being recorded */
static int newFrame = 1; /* a picture was shown since the last table: the next table starts a frame */
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
    newFrame = 0;
    while (count >= (FrameCapMode() == RECORD_WITH_TAB ? TAB_FRAMES : LAST_FRAMES)) {
        f = &frames[first];
        Release(f->vram);
        first = (first + 1) % FRAMES;
        count--;
    }
    current = (first + count) % FRAMES;
    count++;
    f = &frames[current];
    f->gameFrame = PortGameFrames();
    f->shown = 0;
    f->offsetX = offsetX;
    f->offsetY = offsetY;
    f->areaX = areaX;
    f->areaY = areaY;
    f->areaW = areaW;
    f->areaH = areaH;
    f->count = 0;
    f->vram = TakeSnapshot();
    if (f->picture) {
        memset(f->picture, 0, (size_t)f->pictureW * f->pictureH * 4);
    }
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

void FrameCapShown(int x, int y, int w, int h)
{
    int i;

    newFrame = 1;
    if (!FrameCapRecording() || w <= 0 || h <= 0) {
        return;
    }
    /* the newest frame drawn where the picture is taken from, not yet shown */
    for (i = count - 1; i >= 0; i--) {
        FrameCapFrame *f = &frames[(first + i) % FRAMES];

        if (x >= f->areaX && x < f->areaX + f->areaW && y >= f->areaY && y < f->areaY + f->areaH) {
            if (!f->shown) {
                /* the console's pixels, and with widescreen the sides as well */
                int margin = GpuShownMargin(x, w), width = PICTURE_W + 2 * margin;
                uint8_t *pixels = GpuReadPicture(x, y, w, h, margin, width, PICTURE_H);

                if (!f->picture || f->pictureW != width) {
                    f->picture = realloc(f->picture, (size_t)width * PICTURE_H * 4);
                }
                f->pictureW = width;
                f->pictureH = PICTURE_H;
                f->pictureX = -margin;
                memcpy(f->picture, pixels, (size_t)width * PICTURE_H * 4);
                f->shown = PortShownFrames() + 1;
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
    PutI32(file, 2);
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
            const uint8_t none = 0;

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
                Put(file, &none, 1); /* the file keeps the place of the sky's mark of earlier versions */
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
        int x, y;

        if (!f->picture || !f->shown) {
            continue;
        }
        SDL_snprintf(path, sizeof(path), "%s/picture_%03d.ppm", savedDir, i);
        if ((file = fopen(path, "wb"))) {
            fprintf(file, "P6\n%d %d\n255\n", f->pictureW, f->pictureH);
            for (y = 0; y < f->pictureH; y++) {
                for (x = 0; x < f->pictureW; x++) {
                    Put(file, f->picture + ((size_t)y * f->pictureW + x) * 4, 3);
                }
            }
            fclose(file);
        }
    }
    PortLog("frame recorder: %d frames saved to %s\n", count, savedDir);
}

void FrameCapSave(void)
{
    Save();
}

void FrameCapToggleFreeze(void)
{
    if (frozen) {
        frozen = 0;
        return;
    }
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

    for (i = 0; i < count; i++) {
        Release(frames[(first + i) % FRAMES].vram);
    }
    first = count = 0;
    current = -1;
    newFrame = 1;
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
        current = -1;
        generation++;
    } else {
        FrameCapClear();
        taking = 1;
    }
}
