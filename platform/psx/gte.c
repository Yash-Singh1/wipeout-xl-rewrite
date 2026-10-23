/* The geometry coprocessor, implemented from the documented hardware
   behaviour (results, saturation and flags exactly as the console computes
   them), for the instructions the PsyQ library routines the game calls use:
   RTPS, RTPT, MVMVA, AVSZ3, AVSZ4, NCLIP, SQR, OP, and the LZCS/LZCR count. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gte.h"

Gte gte;
int gtePrecision = 1;
static int fadeOn;
static double fadeNear = 64000.0, fadeFar = 96000.0;

void GteSetFade(int on, double nearDistance, double farDistance)
{
    fadeOn = on;
    if (farDistance > nearDistance) {
        fadeNear = nearDistance;
        fadeFar = farDistance;
    }
}

float GteFade(double distance)
{
    double t;

    if (!fadeOn || distance <= fadeNear) {
        return 1.0f;
    }
    if (distance >= fadeFar) {
        return 0.0f;
    }
    t = (distance - fadeNear) / (fadeFar - fadeNear);
    return (float)(1.0 - t * t * (3.0 - 2.0 * t));
}

/* Data registers */
enum {
    VXY0, VZ0, VXY1, VZ1, VXY2, VZ2, RGBC, OTZ, IR0, IR1, IR2, IR3, SXY0, SXY1, SXY2, SXYP,
    SZ0, SZ1, SZ2, SZ3, RGB0, RGB1, RGB2, RES1, MAC0, MAC1, MAC2, MAC3, IRGB, ORGB, LZCS, LZCR
};
/* Control registers */
enum {
    RT11RT12, RT13RT21, RT22RT23, RT31RT32, RT33, TRX, TRY, TRZ, L11L12, L13L21, L22L23, L31L32, L33,
    RBK, GBK, BBK, LR1LR2, LR3LG1, LG2LG3, LB1LB2, LB3, RFC, GFC, BFC, OFX, OFY, H, DQA, DQB, ZSF3,
    ZSF4, FLAG
};

/* FLAG bits */
#define FLAG_MAC1_POS (1u << 30)
#define FLAG_MAC2_POS (1u << 29)
#define FLAG_MAC3_POS (1u << 28)
#define FLAG_MAC1_NEG (1u << 27)
#define FLAG_MAC2_NEG (1u << 26)
#define FLAG_MAC3_NEG (1u << 25)
#define FLAG_IR1 (1u << 24)
#define FLAG_IR2 (1u << 23)
#define FLAG_IR3 (1u << 22)
#define FLAG_COLOR_R (1u << 21)
#define FLAG_COLOR_G (1u << 20)
#define FLAG_COLOR_B (1u << 19)
#define FLAG_SZ3_OTZ (1u << 18)
#define FLAG_DIVIDE (1u << 17)
#define FLAG_MAC0_POS (1u << 16)
#define FLAG_MAC0_NEG (1u << 15)
#define FLAG_SX2 (1u << 14)
#define FLAG_SY2 (1u << 13)
#define FLAG_IR0 (1u << 12)
#define FLAG_ERROR_MASK 0x7F87E000u

static uint32_t flag;
/* The handles of the precise projections in SXY0..SXY2 (gte.h), moved along with them. */
static uint32_t sxyHandle[3];

static int16_t Low16(int32_t value)
{
    return (int16_t)(value & 0xFFFF);
}

static int16_t High16(int32_t value)
{
    return (int16_t)((uint32_t)value >> 16);
}

/* A 44-bit accumulator step: flags overflow and wraps like the hardware. */
static int64_t Check44(int index, int64_t value)
{
    static const uint32_t positive[3] = {FLAG_MAC1_POS, FLAG_MAC2_POS, FLAG_MAC3_POS};
    static const uint32_t negative[3] = {FLAG_MAC1_NEG, FLAG_MAC2_NEG, FLAG_MAC3_NEG};

    if (value > 0x7FFFFFFFFFFLL) {
        flag |= positive[index];
    } else if (value < -0x80000000000LL) {
        flag |= negative[index];
    }
    /* sign-extend from 44 bits */
    return (int64_t)((uint64_t)value << 20) >> 20;
}

/* MAC1..3 = value >> shift, keeping the 44-bit flagging of the sum. */
static int32_t SetMac(int index, int64_t value, int shift)
{
    int32_t result;

    value = Check44(index, value);
    result = (int32_t)(value >> shift);
    gte.data[MAC1 + index] = result;
    return result;
}

static void SetIr(int index, int32_t value, int lm)
{
    static const uint32_t bits[3] = {FLAG_IR1, FLAG_IR2, FLAG_IR3};
    int32_t low = lm ? 0 : -0x8000;

    if (value < low) {
        value = low;
        flag |= bits[index];
    } else if (value > 0x7FFF) {
        value = 0x7FFF;
        flag |= bits[index];
    }
    gte.data[IR1 + index] = value;
}

static int32_t SetMac0(int64_t value)
{
    if (value > 0x7FFFFFFFLL) {
        flag |= FLAG_MAC0_POS;
    } else if (value < -0x80000000LL) {
        flag |= FLAG_MAC0_NEG;
    }
    gte.data[MAC0] = (int32_t)value;
    return (int32_t)value;
}

static void PushSz(int32_t value)
{
    if (value < 0) {
        value = 0;
        flag |= FLAG_SZ3_OTZ;
    } else if (value > 0xFFFF) {
        value = 0xFFFF;
        flag |= FLAG_SZ3_OTZ;
    }
    gte.data[SZ0] = gte.data[SZ1];
    gte.data[SZ1] = gte.data[SZ2];
    gte.data[SZ2] = gte.data[SZ3];
    gte.data[SZ3] = value;
}

static void PushSxy(int32_t x, int32_t y)
{
    if (x < -0x400) {
        x = -0x400;
        flag |= FLAG_SX2;
    } else if (x > 0x3FF) {
        x = 0x3FF;
        flag |= FLAG_SX2;
    }
    if (y < -0x400) {
        y = -0x400;
        flag |= FLAG_SY2;
    } else if (y > 0x3FF) {
        y = 0x3FF;
        flag |= FLAG_SY2;
    }
    gte.data[SXY0] = gte.data[SXY1];
    gte.data[SXY1] = gte.data[SXY2];
    gte.data[SXY2] = (int32_t)((uint32_t)(uint16_t)x | ((uint32_t)(uint16_t)y << 16));
    sxyHandle[0] = sxyHandle[1];
    sxyHandle[1] = sxyHandle[2];
    sxyHandle[2] = 0;
}

static void SetOtz(int64_t value)
{
    int32_t otz = (int32_t)(value >> 12);

    if (otz < 0) {
        otz = 0;
        flag |= FLAG_SZ3_OTZ;
    } else if (otz > 0xFFFF) {
        otz = 0xFFFF;
        flag |= FLAG_SZ3_OTZ;
    }
    gte.data[OTZ] = otz;
}

/* The hardware's division: an unsigned Newton-Raphson reciprocal of SZ3
   (with its 257-entry seed table), which differs from exact division in the
   last bit for some values. */
static uint32_t Divide(uint32_t h, uint32_t sz3)
{
    static uint8_t unr[257];
    static int ready;
    uint32_t z, n, d, u;
    uint64_t result;

    if (!ready) {
        int i;
        for (i = 0; i < 257; i++) {
            int value = (0x40000 / (i + 0x100) + 1) / 2 - 0x101;
            unr[i] = (uint8_t)(value < 0 ? 0 : value);
        }
        ready = 1;
    }
    if (h >= sz3 * 2) {
        flag |= FLAG_DIVIDE;
        return 0x1FFFF;
    }
    z = 0;
    while (z < 16 && !(sz3 & (0x8000u >> z))) {
        z++;
    }
    n = h << z;
    d = sz3 << z;
    u = unr[(d - 0x7FC0) >> 7] + 0x101;
    d = (0x2000080 - d * u) >> 8;
    d = (0x0000080 + d * u) >> 8;
    result = ((uint64_t)n * d + 0x8000) >> 16;
    return result > 0x1FFFF ? 0x1FFFF : (uint32_t)result;
}

/* ---- the precise vertex cache ----

   Every projection of the frame is kept, in order, under the integer screen
   position the game received. Different points can round to the same pixel
   (a ship's corner in front of a track vertex), so one position may hold
   several projections; a primitive's vertices are resolved together, taking
   for each the projection made closest in time to the projections of the
   others: the game projects the points of a face or a model together and
   builds its primitives from them. A position that came from 2D arithmetic
   rather than a projection finds nothing, or only a projection made far from
   the primitive's others, and is drawn as given. */

#define PRECISE_SLOTS 262144  /* hash of screen positions */
#define PRECISE_RECORDS 131072 /* projections of one frame */
#define PRECISE_CANDIDATES 16 /* projections considered per vertex */
#define PRECISE_SPREAD 1024   /* projections apart that still make one primitive */

typedef struct PreciseSlot {
    uint32_t key;    /* SXY as the game received it, plus 1 */
    uint32_t frame;  /* the frame the slot belongs to (others are empty) */
    int32_t latest;  /* the last record with this key */
} PreciseSlot;

typedef struct PreciseRecord {
    GtePrecise value;
    uint32_t key;     /* the screen position it is kept under */
    int32_t previous; /* the record before it with the same key, or -1 */
} PreciseRecord;

/* A projection's handle: its record and the frame it belongs to (0: none). */
#define HANDLE_INDEX_BITS 17
#define HANDLE_INDEX_MASK ((1u << HANDLE_INDEX_BITS) - 1)

/* Exact links: the renderers that build packets from projections bind each packet vertex (the
   address of its xy word) to the projection it came from, for this frame. */
#define BINDING_SLOTS 262144
typedef struct Binding {
    const void *address;
    uint32_t frame;
    uint32_t handle;
} Binding;
static Binding bindings[BINDING_SLOTS];


static PreciseSlot slots[PRECISE_SLOTS];
static PreciseRecord records[PRECISE_RECORDS];
static int recordCount;
static uint32_t preciseFrame = 1;
static int clearPending; /* the frame was handed to the GPU: the next record starts a new frame */

static uint32_t Hash(uint32_t key)
{
    key ^= key >> 16;
    key *= 0x7FEB352Du;
    key ^= key >> 15;
    return key & (PRECISE_SLOTS - 1);
}

static PreciseSlot *FindSlot(uint32_t key, int create)
{
    uint32_t slot = Hash(key), probe;

    for (probe = 0; probe < 64; probe++) {
        PreciseSlot *entry = &slots[(slot + probe) & (PRECISE_SLOTS - 1)];
        if (entry->frame != preciseFrame) {
            if (!create) {
                return NULL;
            }
            entry->key = key;
            entry->frame = preciseFrame;
            entry->latest = -1;
            return entry;
        }
        if (entry->key == key) {
            return entry;
        }
    }
    return NULL;
}

/* ---- frame interpolation ---- */

static int32_t HandleRecord(uint32_t handle);
static uint64_t pendingKey[3];
static int keepPast;

#define PAST_SLOTS 262144
typedef struct PastEntry {
    uint64_t key;      /* 0: empty */
    uint32_t frame;    /* the past frame it belongs to */
    float view[3];
    float fade;
} PastEntry;
static PastEntry past[PAST_SLOTS];
static uint32_t pastFrame;       /* the frame whose projections `past` holds */
static uint32_t cutCheckedFrame; /* the frame the camera cut test was made for */
static int cut;

uint64_t GteKeyMix(uint64_t a, uint64_t b)
{
    uint64_t v = a * 0x9E3779B97F4A7C15ull ^ (b + 0x632BE59BD9B4E019ull + (a << 6) + (a >> 2));

    v ^= v >> 31;
    v *= 0xBF58476D1CE4E5B9ull;
    v ^= v >> 29;
    return v ? v : 1;
}

void GteKeyNext(int slot, uint64_t key)
{
    if (slot >= 0 && slot < 3) {
        pendingKey[slot] = key;
    }
}

uint32_t GteFrameNumber(void)
{
    if (clearPending) {
        clearPending = 0;
        GteClearPrecise();
    }
    return preciseFrame;
}

uint32_t GteFrameRecorded(void)
{
    return preciseFrame;
}

void GteKeepPast(int on)
{
    keepPast = on;
}

static PastEntry *PastSlot(uint64_t key, int create)
{
    uint32_t slot = (uint32_t)(key ^ (key >> 32)) & (PAST_SLOTS - 1), probe;

    for (probe = 0; probe < 32; probe++) {
        PastEntry *entry = &past[(slot + probe) & (PAST_SLOTS - 1)];

        if (entry->frame != pastFrame || !entry->key) {
            return create ? entry : NULL;
        }
        if (entry->key == key) {
            return create ? NULL : entry; /* the first projection under a key stays */
        }
    }
    return NULL;
}

/* The frame's keyed projections become the frame before. */
static void KeepPast(void)
{
    int i;

    pastFrame++;
    if (!keepPast) {
        return;
    }
    for (i = 0; i < recordCount; i++) {
        PastEntry *entry;

        if (!records[i].value.key || records[i].value.view[2] <= 0.0f || !(entry = PastSlot(records[i].value.key, 1))) {
            continue;
        }
        entry->key = records[i].value.key;
        entry->frame = pastFrame;
        memcpy(entry->view, records[i].value.view, sizeof(entry->view));
        entry->fade = records[i].value.fade;
    }
}

static float Moved(const float a[3], const float b[3])
{
    float dx = a[0] - b[0], dy = a[1] - b[1], dz = a[2] - b[2];

    return dx * dx + dy * dy + dz * dz;
}

#define CUT_DISTANCE 6000.0f /* view-space units a vertex cannot move in one frame of the game */

/* The view's motion between the two frames, fitted to what both projected: cur = R past + T (small rotations:
   the rotation vector w solved linearly, R made from it), refitted without the projections that do not follow it
   (moving ships). What one frame projects and the other did not (track coming into view) moves by it. */
static double motionR[3][3], motionT[3];
static int motionValid;

static int Solve6(double a[6][7], double x[6])
{
    int i, j, k;

    for (i = 0; i < 6; i++) {
        int best = i;

        for (j = i + 1; j < 6; j++) {
            best = fabs(a[j][i]) > fabs(a[best][i]) ? j : best;
        }
        if (fabs(a[best][i]) < 1e-9) {
            return 0;
        }
        for (k = 0; k < 7; k++) {
            double t = a[i][k];

            a[i][k] = a[best][k];
            a[best][k] = t;
        }
        for (j = 0; j < 6; j++) {
            if (j != i) {
                double f = a[j][i] / a[i][i];

                for (k = i; k < 7; k++) {
                    a[j][k] -= f * a[i][k];
                }
            }
        }
    }
    for (i = 0; i < 6; i++) {
        x[i] = a[i][6] / a[i][i];
    }
    return 1;
}

static void FitMotion(int pass)
{
    double a[6][7] = {{0}}, x[6], angle, k[3], c, s;
    int i, r, n = 0;

    for (i = 0; i < recordCount; i++) {
        PastEntry *entry = records[i].value.key ? PastSlot(records[i].value.key, 0) : NULL;
        double p[3], d[3], row[3][6];

        if (!entry || records[i].value.view[2] <= 0.0f) {
            continue;
        }
        for (r = 0; r < 3; r++) {
            p[r] = entry->view[r];
            d[r] = records[i].value.view[r] - p[r];
        }
        if (pass) {
            double e = 0.0;

            for (r = 0; r < 3; r++) {
                double moved = motionR[r][0] * p[0] + motionR[r][1] * p[1] + motionR[r][2] * p[2] + motionT[r];

                e += (moved - records[i].value.view[r]) * (moved - records[i].value.view[r]);
            }
            if (e > 150.0 * 150.0) {
                continue; /* it moves on its own */
            }
        }
        /* d = w x p + T: rows of [-[p]x | I] */
        memset(row, 0, sizeof(row));
        row[0][1] = p[2], row[0][2] = -p[1];
        row[1][0] = -p[2], row[1][2] = p[0];
        row[2][0] = p[1], row[2][1] = -p[0];
        row[0][3] = row[1][4] = row[2][5] = 1.0;
        for (r = 0; r < 3; r++) {
            int u, v;

            for (u = 0; u < 6; u++) {
                for (v = 0; v < 6; v++) {
                    a[u][v] += row[r][u] * row[r][v];
                }
                a[u][6] += row[r][u] * d[r];
            }
        }
        n++;
    }
    if (n < 16 || !Solve6(a, x)) {
        motionValid = 0;
        return;
    }
    angle = sqrt(x[0] * x[0] + x[1] * x[1] + x[2] * x[2]);
    for (r = 0; r < 3; r++) {
        k[r] = angle > 1e-12 ? x[r] / angle : 0.0;
        motionT[r] = x[3 + r];
    }
    c = cos(angle), s = sin(angle);
    /* Rodrigues: R = cI + s[k]x + (1 - c)kk' */
    for (r = 0; r < 3; r++) {
        for (i = 0; i < 3; i++) {
            motionR[r][i] = (r == i ? c : 0.0) + (1.0 - c) * k[r] * k[i];
        }
    }
    motionR[0][1] -= s * k[2], motionR[0][2] += s * k[1];
    motionR[1][0] += s * k[2], motionR[1][2] -= s * k[0];
    motionR[2][0] -= s * k[1], motionR[2][1] += s * k[0];
    motionValid = 1;
}

/* Whether the view jumped between the two frames (a cut to another camera, a restart): most of what both
   frames projected moved too far. The view's motion is fitted at the same time. */
static int CameraCut(void)
{
    int i, matched = 0, far = 0;

    if (cutCheckedFrame == preciseFrame) {
        return cut;
    }
    cutCheckedFrame = preciseFrame;
    for (i = 0; i < recordCount; i += 7) {
        PastEntry *entry = records[i].value.key ? PastSlot(records[i].value.key, 0) : NULL;

        if (entry) {
            matched++;
            far += Moved(entry->view, records[i].value.view) > CUT_DISTANCE * CUT_DISTANCE;
        }
    }
    cut = matched < 8 || far * 3 > matched;
    if (!cut) {
        FitMotion(0);
        if (motionValid) {
            FitMotion(1);
        }
    }
    return cut;
}

int GteInterpolate(const GtePrecise *p, float along, GtePrecise *out)
{
    PastEntry *entry;
    double h = (double)(uint16_t)gte.control[H], was[3], x, y, z;
    int r;

    if (!keepPast || !p->key || p->view[2] <= 0.0f || pastFrame != preciseFrame - 1 || CameraCut()) {
        return 0;
    }
    entry = PastSlot(p->key, 0);
    if (entry && Moved(entry->view, p->view) <= CUT_DISTANCE * CUT_DISTANCE) {
        for (r = 0; r < 3; r++) {
            was[r] = entry->view[r];
        }
    } else if (motionValid) {
        /* new in this frame (or another thing under its key): where the view's motion brings it from */
        double d[3];

        for (r = 0; r < 3; r++) {
            d[r] = p->view[r] - motionT[r];
        }
        for (r = 0; r < 3; r++) {
            was[r] = motionR[0][r] * d[0] + motionR[1][r] * d[1] + motionR[2][r] * d[2];
        }
        entry = NULL;
    } else {
        return 0;
    }
    x = was[0] + (p->view[0] - was[0]) * along;
    y = was[1] + (p->view[1] - was[1]) * along;
    z = was[2] + (p->view[2] - was[2]) * along;
    if (z < h * 0.5 || z < 1.0) {
        return 0; /* too near the camera to project */
    }
    *out = *p;
    out->x = (float)(gte.control[OFX] / 65536.0 + x * h / z);
    out->y = (float)(gte.control[OFY] / 65536.0 + y * h / z);
    out->z = (float)(z * (p->z / p->view[2])); /* (the far view draws a hair deeper: kept) */
    if (entry) {
        out->fade = entry->fade + (p->fade - entry->fade) * along;
    }
    return 1;
}

int GteViewAt(const GtePrecise *p, float along, double view[3])
{
    PastEntry *entry;
    int r;

    if (p->view[0] == 0.0f && p->view[1] == 0.0f && p->view[2] == 0.0f) {
        return 0;
    }
    for (r = 0; r < 3; r++) {
        view[r] = p->view[r];
    }
    if (along >= 1.0f || !keepPast || !p->key || pastFrame != preciseFrame - 1 || CameraCut()) {
        return 1; /* (where the frame has it: as GteInterpolate leaves a vertex it cannot move) */
    }
    entry = PastSlot(p->key, 0);
    if (entry && Moved(entry->view, p->view) <= CUT_DISTANCE * CUT_DISTANCE) {
        for (r = 0; r < 3; r++) {
            view[r] = entry->view[r] + (p->view[r] - entry->view[r]) * along;
        }
    } else if (motionValid) {
        double d[3], was[3];

        for (r = 0; r < 3; r++) {
            d[r] = p->view[r] - motionT[r];
        }
        for (r = 0; r < 3; r++) {
            was[r] = motionR[0][r] * d[0] + motionR[1][r] * d[1] + motionR[2][r] * d[2];
        }
        for (r = 0; r < 3; r++) {
            view[r] = was[r] + (p->view[r] - was[r]) * along;
        }
    }
    return 1;
}

double GteNearPlane(void)
{
    double h = (double)(uint16_t)gte.control[H];

    return h * 0.5 > 1.0 ? h * 0.5 : 1.0;
}

void GteProjectView(const double view[3], float *x, float *y)
{
    double h = (double)(uint16_t)gte.control[H];

    *x = (float)(gte.control[OFX] / 65536.0 + view[0] * h / view[2]);
    *y = (float)(gte.control[OFY] / 65536.0 + view[1] * h / view[2]);
}

void GteRecordView(uint32_t handle, double x, double y, double z)
{
    int32_t record = HandleRecord(handle);

    if (record >= 0) {
        records[record].value.view[0] = (float)x;
        records[record].value.view[1] = (float)y;
        records[record].value.view[2] = (float)z;
    }
}

static int backgroundOn; /* (GteSetBackground) */

void GteSetBackground(int on)
{
    backgroundOn = on;
}

static uint32_t RecordPrecise(uint32_t sxy, float x, float y, float z)
{
    PreciseSlot *slot;
    PreciseRecord *record;

    if (clearPending) {
        clearPending = 0;
        GteClearPrecise();
    }
    if (recordCount >= PRECISE_RECORDS - 1 || !(slot = FindSlot(sxy + 1, 1))) {
        return 0;
    }
    record = &records[recordCount];
    record->value.x = x;
    record->value.y = y;
    record->value.z = z;
    record->value.fade = 1.0f;
    record->value.background = backgroundOn;
    record->value.key = 0;
    record->value.view[0] = record->value.view[1] = record->value.view[2] = 0.0f;
    record->key = sxy;
    record->previous = slot->latest;
    slot->latest = recordCount++;
    return (uint32_t)recordCount | (preciseFrame << HANDLE_INDEX_BITS);
}

/* The record of a handle of this frame, or -1. */
static int32_t HandleRecord(uint32_t handle)
{
    uint32_t index = handle & HANDLE_INDEX_MASK;

    if (!index || (handle >> HANDLE_INDEX_BITS) != (preciseFrame & (0xFFFFFFFFu >> HANDLE_INDEX_BITS)) ||
        index > (uint32_t)recordCount) {
        return -1;
    }
    return (int32_t)index - 1;
}

static uint32_t BindingSlot(const void *address)
{
    uint64_t v = (uint64_t)(uintptr_t)address;

    v ^= v >> 29;
    v *= 0xBF58476D1CE4E5B9ull;
    v ^= v >> 32;
    return (uint32_t)v & (BINDING_SLOTS - 1);
}

void GteBindPrecise(const void *address, uint32_t handle)
{
    uint32_t slot = BindingSlot(address), probe;

    for (probe = 0; probe < 64; probe++) {
        Binding *b = &bindings[(slot + probe) & (BINDING_SLOTS - 1)];
        if (!handle && b->frame != preciseFrame) {
            return; /* nothing to undo */
        }
        if (b->frame != preciseFrame || b->address == address) {
            b->address = address;
            b->frame = preciseFrame;
            b->handle = handle;
            return;
        }
    }
}

#define FLAT_HANDLE 0xFFFFFFFFu /* (GteBindFlat) */

void GteBindFlat(const void *address)
{
    GteBindPrecise(address, FLAT_HANDLE);
}

/* Whether a packet vertex was bound flat this frame. */
static int BoundFlat(const void *address)
{
    uint32_t slot = BindingSlot(address), probe;

    for (probe = 0; probe < 64; probe++) {
        Binding *b = &bindings[(slot + probe) & (BINDING_SLOTS - 1)];
        if (b->frame != preciseFrame) {
            return 0;
        }
        if (b->address == address) {
            return b->handle == FLAT_HANDLE;
        }
    }
    return 0;
}

/* The projection a packet vertex was bound to this frame, if its word still holds that projection's
   screen position. */
static int32_t BoundRecord(const void *address, uint32_t xy)
{
    uint32_t slot = BindingSlot(address), probe;

    for (probe = 0; probe < 64; probe++) {
        Binding *b = &bindings[(slot + probe) & (BINDING_SLOTS - 1)];
        if (b->frame != preciseFrame) {
            return -1;
        }
        if (b->address == address) {
            int32_t record = b->handle == FLAT_HANDLE ? -1 : HandleRecord(b->handle);
            return record >= 0 && records[record].key == xy ? record : -1;
        }
    }
    return -1;
}

uint32_t GteBoundHandle(const void *address, uint32_t xy)
{
    int32_t record = BoundRecord(address, xy);

    return record >= 0 ? (uint32_t)(record + 1) | (preciseFrame << HANDLE_INDEX_BITS) : 0;
}

uint32_t GtePreciseHandle(int reg)
{
    return reg >= 12 && reg <= 14 ? sxyHandle[reg - 12] : reg == 15 ? sxyHandle[2] : 0;
}

int GtePreciseOf(uint32_t handle, GtePrecise *out)
{
    int32_t record = HandleRecord(handle);

    if (record < 0) {
        return 0;
    }
    *out = records[record].value;
    return 1;
}

/* The records under a screen position, the latest first. */
static int Candidates(uint32_t xy, int32_t out[PRECISE_CANDIDATES])
{
    PreciseSlot *slot = FindSlot(xy + 1, 0);
    int32_t index = slot ? slot->latest : -1;
    int n = 0;

    while (index >= 0 && n < PRECISE_CANDIDATES) {
        out[n++] = index;
        index = records[index].previous;
    }
    return n;
}

static int32_t Distance(int32_t a, int32_t b)
{
    return a > b ? a - b : b - a;
}

static int resolvedBound; /* (GteResolvedBound) */

int GteResolvedBound(void)
{
    return resolvedBound;
}

unsigned GteResolvePrecise(const uint32_t *xy, const void *const *addresses, int count, GtePrecise *out)
{
    int32_t candidates[GTE_RESOLVE_MAX][PRECISE_CANDIDATES], chosen[GTE_RESOLVE_MAX];
    int found[GTE_RESOLVE_MAX], i, j, anchors = 0, any = 0, bound = 0;
    int32_t sorted[GTE_RESOLVE_MAX], middle;
    unsigned mask = 0;

    resolvedBound = 0;
    for (i = 0; addresses && i < count; i++) {
        if (BoundFlat(addresses[i])) {
            return 0;
        }
    }
    for (i = 0; i < count; i++) {
        int32_t record = addresses ? BoundRecord(addresses[i], xy[i]) : -1;

        if (record >= 0) {
            /* bound by the renderer that built the packet: exact */
            found[i] = 1;
            candidates[i][0] = record;
            chosen[i] = record;
            anchors++;
            bound++;
            continue;
        }
        found[i] = Candidates(xy[i], candidates[i]);
        chosen[i] = found[i] ? candidates[i][0] : -1;
        anchors += found[i] == 1;
        any |= found[i] > 1;
    }
    if (any) {
        if (anchors) {
            /* each ambiguous vertex: the projection nearest to an unambiguous one */
            for (i = 0; i < count; i++) {
                int32_t best = -1, bestDistance = 0x7FFFFFFF;
                int k;
                if (found[i] < 2) {
                    continue;
                }
                for (k = 0; k < found[i]; k++) {
                    for (j = 0; j < count; j++) {
                        if (found[j] == 1 && Distance(candidates[i][k], chosen[j]) < bestDistance) {
                            bestDistance = Distance(candidates[i][k], chosen[j]);
                            best = candidates[i][k];
                        }
                    }
                }
                chosen[i] = best;
            }
        } else {
            /* none unambiguous: around each candidate of the first ambiguous
               vertex, the nearest of the others; the tightest group wins */
            int first = 0, k, bestSpread = 0x7FFFFFFF;
            int32_t group[GTE_RESOLVE_MAX];

            while (found[first] < 2) {
                first++;
            }
            for (k = 0; k < found[first]; k++) {
                int32_t low = candidates[first][k], high = low;
                for (i = 0; i < count; i++) {
                    int32_t best = -1;
                    int m;
                    for (m = 0; m < found[i]; m++) {
                        if (best < 0 || Distance(candidates[i][m], candidates[first][k]) <
                                            Distance(best, candidates[first][k])) {
                            best = candidates[i][m];
                        }
                    }
                    group[i] = best;
                    if (best >= 0) {
                        low = best < low ? best : low;
                        high = best > high ? best : high;
                    }
                }
                if (high - low < bestSpread) {
                    bestSpread = high - low;
                    memcpy(chosen, group, sizeof(group));
                }
            }
        }
    }

    /* a projection far from the primitive's others is a stranger at the same pixel */
    j = 0;
    for (i = 0; i < count; i++) {
        if (chosen[i] >= 0) {
            int k = j++;
            while (k > 0 && sorted[k - 1] > chosen[i]) {
                sorted[k] = sorted[k - 1];
                k--;
            }
            sorted[k] = chosen[i];
        }
    }
    if (!j) {
        return 0;
    }
    if (!bound && j < count) {
        return 0; /* only some corners of an unbound primitive match: by chance (a 2D shape) */
    }
    resolvedBound = bound == count;
    middle = sorted[(j - 1) / 2];
    for (i = 0; i < count; i++) {
        if (chosen[i] >= 0 && (j == 1 || Distance(chosen[i], middle) <= PRECISE_SPREAD)) {
            out[i] = records[chosen[i]].value;
            mask |= 1u << i;
        }
    }
    return mask;
}

uint32_t GteRecordProjection(uint32_t sxy, float x, float y, float z, float fade)
{
    uint32_t handle = RecordPrecise(sxy, x, y, z);

    if (handle) {
        records[(handle & HANDLE_INDEX_MASK) - 1].value.fade = fade;
        records[(handle & HANDLE_INDEX_MASK) - 1].value.key = pendingKey[0];
    }
    pendingKey[0] = 0;
    return handle;
}

void GteRecordGrid(const uint32_t corner[4], const void *const cornerAddress[4], const int *x, const int *y, int size,
                   uint32_t *handles)
{
    GtePrecise c[4];
    int row, column, i;

    for (i = 0; i < size * size; i++) {
        handles[i] = 0;
    }
    if (size < 2 || GteResolvePrecise(corner, cornerAddress, 4, c) != 15) {
        return;
    }
    handles[0] = GteBoundHandle(cornerAddress[0], corner[0]);
    handles[size - 1] = GteBoundHandle(cornerAddress[1], corner[1]);
    handles[size * (size - 1)] = GteBoundHandle(cornerAddress[2], corner[2]);
    handles[size * size - 1] = GteBoundHandle(cornerAddress[3], corner[3]);
    for (row = 0; row < size; row++) {
        for (column = 0; column < size; column++) {
            float u = (float)column / (float)(size - 1), v = (float)row / (float)(size - 1);
            i = row * size + column;

            if ((row == 0 || row == size - 1) && (column == 0 || column == size - 1)) {
                continue; /* the corners are projections already */
            }
            handles[i] = RecordPrecise((uint32_t)(uint16_t)x[i] | (uint32_t)(uint16_t)y[i] << 16,
                          (1 - u) * (1 - v) * c[0].x + u * (1 - v) * c[1].x + (1 - u) * v * c[2].x + u * v * c[3].x,
                          (1 - u) * (1 - v) * c[0].y + u * (1 - v) * c[1].y + (1 - u) * v * c[2].y + u * v * c[3].y,
                          0.0f);
        }
    }
}

void GteFrameDrawn(void)
{
    clearPending = 1;
}

void GteClearPrecise(void)
{
    KeepPast();
    recordCount = 0;
    preciseFrame++;
    if (preciseFrame == 0xFFFFFFFFu) {
        memset(slots, 0, sizeof(slots));
        preciseFrame = 1;
    }
}

/* ---- instructions ---- */

/* RTPS for vector v (0..2): perspective transformation. */
static void Rtp(int v, int last, int sf)
{
    int shift = sf ? 12 : 0;
    int32_t vx = Low16(gte.data[VXY0 + v * 2]);
    int32_t vy = High16(gte.data[VXY0 + v * 2]);
    int32_t vz = Low16(gte.data[VZ0 + v * 2]);
    int32_t *c = gte.control;
    int64_t mac1, mac2, mac3;
    int32_t m3;
    uint32_t n;
    int32_t sx, sy;

    mac1 = Check44(0, ((int64_t)c[TRX] * 4096) + (int64_t)Low16(c[RT11RT12]) * vx);
    mac1 = Check44(0, mac1 + (int64_t)High16(c[RT11RT12]) * vy);
    mac1 = mac1 + (int64_t)Low16(c[RT13RT21]) * vz;
    mac2 = Check44(1, ((int64_t)c[TRY] * 4096) + (int64_t)High16(c[RT13RT21]) * vx);
    mac2 = Check44(1, mac2 + (int64_t)Low16(c[RT22RT23]) * vy);
    mac2 = mac2 + (int64_t)High16(c[RT22RT23]) * vz;
    mac3 = Check44(2, ((int64_t)c[TRZ] * 4096) + (int64_t)Low16(c[RT31RT32]) * vx);
    mac3 = Check44(2, mac3 + (int64_t)High16(c[RT31RT32]) * vy);
    mac3 = mac3 + (int64_t)Low16(c[RT33]) * vz;
    SetIr(0, SetMac(0, mac1, shift), 0);
    SetIr(1, SetMac(1, mac2, shift), 0);
    m3 = SetMac(2, mac3, shift);
    /* IR3 saturates on MAC3, but its flag follows MAC3 >> 12 (a hardware quirk) */
    {
        int32_t value = m3;
        int32_t check = (int32_t)(Check44(2, mac3) >> 12);
        gte.data[IR3] = value < -0x8000 ? -0x8000 : value > 0x7FFF ? 0x7FFF : value;
        if (check < -0x8000 || check > 0x7FFF) {
            flag |= FLAG_IR3;
        }
    }
    PushSz((int32_t)(Check44(2, mac3) >> 12));
    n = Divide((uint32_t)(uint16_t)c[H], (uint32_t)gte.data[SZ3]);
    sx = (int32_t)(SetMac0((int64_t)n * gte.data[IR1] + c[OFX]) >> 16);
    sy = (int32_t)(SetMac0((int64_t)n * gte.data[IR2] + c[OFY]) >> 16);
    PushSxy(sx, sy);
    if (gtePrecision) {
        /* The same projection without the integer steps. */
        double x = (double)Check44(0, mac1) / 4096.0;
        double y = (double)Check44(1, mac2) / 4096.0;
        double z = (double)Check44(2, mac3) / 4096.0;
        double h = (double)(uint16_t)c[H];
        if (z > 0.0 && h < z * 2.0 && x > -32768.0 && x < 32767.0 && y > -32768.0 && y < 32767.0) {
            float px = (float)(c[OFX] / 65536.0 + x * h / z);
            float py = (float)(c[OFY] / 65536.0 + y * h / z);
            if (px > -1024.0f && px < 1024.0f && py > -1024.0f && py < 1024.0f) {
                sxyHandle[2] = RecordPrecise((uint32_t)gte.data[SXY2], px, py, (float)z);
                if (fadeOn && sxyHandle[2]) {
                    records[(sxyHandle[2] & HANDLE_INDEX_MASK) - 1].value.fade = GteFade(sqrt(x * x + y * y + z * z));
                }
                if (sxyHandle[2]) {
                    PreciseRecord *record = &records[(sxyHandle[2] & HANDLE_INDEX_MASK) - 1];

                    record->value.key = pendingKey[v];
                    record->value.view[0] = (float)x;
                    record->value.view[1] = (float)y;
                    record->value.view[2] = (float)z;
                }
            }
        }
    }
    pendingKey[v] = 0;
    if (last) {
        int64_t mac0 = SetMac0((int64_t)n * Low16(c[DQA]) + c[DQB]);
        int32_t ir0 = (int32_t)(mac0 >> 12);
        if (ir0 < 0) {
            ir0 = 0;
            flag |= FLAG_IR0;
        } else if (ir0 > 0x1000) {
            ir0 = 0x1000;
            flag |= FLAG_IR0;
        }
        gte.data[IR0] = ir0;
    }
}

/* MVMVA: multiply vector by matrix and add vector. */
static void Mvmva(uint32_t command)
{
    int sf = (command >> 19) & 1;
    int mx = (command >> 17) & 3;
    int v = (command >> 15) & 3;
    int cv = (command >> 13) & 3;
    int lm = (command >> 10) & 1;
    int shift = sf ? 12 : 0;
    int32_t *c = gte.control;
    int32_t matrix[3][3];
    int32_t vector[3];
    int64_t add[3];
    int i;

    if (mx == 3) {
        fprintf(stderr, "GTE: MVMVA with the garbage matrix is not implemented\n");
        abort();
    } else {
        int base = mx == 0 ? RT11RT12 : mx == 1 ? L11L12 : LR1LR2;
        matrix[0][0] = Low16(c[base]);
        matrix[0][1] = High16(c[base]);
        matrix[0][2] = Low16(c[base + 1]);
        matrix[1][0] = High16(c[base + 1]);
        matrix[1][1] = Low16(c[base + 2]);
        matrix[1][2] = High16(c[base + 2]);
        matrix[2][0] = Low16(c[base + 3]);
        matrix[2][1] = High16(c[base + 3]);
        matrix[2][2] = Low16(c[base + 4]);
    }
    if (v == 3) {
        vector[0] = Low16(gte.data[IR1]);
        vector[1] = Low16(gte.data[IR2]);
        vector[2] = Low16(gte.data[IR3]);
    } else {
        vector[0] = Low16(gte.data[VXY0 + v * 2]);
        vector[1] = High16(gte.data[VXY0 + v * 2]);
        vector[2] = Low16(gte.data[VZ0 + v * 2]);
    }
    if (cv == 0) {
        add[0] = (int64_t)c[TRX] * 4096;
        add[1] = (int64_t)c[TRY] * 4096;
        add[2] = (int64_t)c[TRZ] * 4096;
    } else if (cv == 1) {
        add[0] = (int64_t)c[RBK] * 4096;
        add[1] = (int64_t)c[GBK] * 4096;
        add[2] = (int64_t)c[BBK] * 4096;
    } else if (cv == 2) {
        fprintf(stderr, "GTE: MVMVA with the far colour vector is not implemented\n");
        abort();
    } else {
        add[0] = add[1] = add[2] = 0;
    }
    for (i = 0; i < 3; i++) {
        int64_t sum = Check44(i, add[i] + (int64_t)matrix[i][0] * vector[0]);
        sum = Check44(i, sum + (int64_t)matrix[i][1] * vector[1]);
        sum = sum + (int64_t)matrix[i][2] * vector[2];
        SetIr(i, SetMac(i, sum, shift), lm);
    }
}

void GteExecute(uint32_t command)
{
    int sf = (command >> 19) & 1;
    int lm = (command >> 10) & 1;
    int32_t *c = gte.control;

    flag = 0;
    switch (command & 0x3F) {
    case 0x01: /* RTPS */
        Rtp(0, 1, sf);
        break;
    case 0x30: /* RTPT */
        Rtp(0, 0, sf);
        Rtp(1, 0, sf);
        Rtp(2, 1, sf);
        break;
    case 0x12: /* MVMVA */
        Mvmva(command);
        break;
    case 0x2D: /* AVSZ3 */
        SetOtz(SetMac0((int64_t)Low16(c[ZSF3]) *
                       (gte.data[SZ1] + gte.data[SZ2] + gte.data[SZ3])));
        break;
    case 0x2E: /* AVSZ4 */
        SetOtz(SetMac0((int64_t)Low16(c[ZSF4]) *
                       (gte.data[SZ0] + gte.data[SZ1] + gte.data[SZ2] + gte.data[SZ3])));
        break;
    case 0x06: { /* NCLIP */
        int32_t x0 = Low16(gte.data[SXY0]), y0 = High16(gte.data[SXY0]);
        int32_t x1 = Low16(gte.data[SXY1]), y1 = High16(gte.data[SXY1]);
        int32_t x2 = Low16(gte.data[SXY2]), y2 = High16(gte.data[SXY2]);
        SetMac0((int64_t)x0 * y1 + (int64_t)x1 * y2 + (int64_t)x2 * y0 - (int64_t)x0 * y2 -
                (int64_t)x1 * y0 - (int64_t)x2 * y1);
        break;
    }
    case 0x28: { /* SQR */
        int shift = sf ? 12 : 0, i;
        for (i = 0; i < 3; i++) {
            int32_t ir = gte.data[IR1 + i];
            SetIr(i, SetMac(i, (int64_t)ir * ir, shift), lm);
        }
        break;
    }
    case 0x0C: { /* OP: outer product of IR with the rotation diagonal */
        int shift = sf ? 12 : 0;
        int32_t d1 = Low16(c[RT11RT12]), d2 = Low16(c[RT22RT23]), d3 = Low16(c[RT33]);
        int32_t ir1 = gte.data[IR1], ir2 = gte.data[IR2], ir3 = gte.data[IR3];
        SetIr(0, SetMac(0, (int64_t)ir3 * d2 - (int64_t)ir2 * d3, shift), lm);
        SetIr(1, SetMac(1, (int64_t)ir1 * d3 - (int64_t)ir3 * d1, shift), lm);
        SetIr(2, SetMac(2, (int64_t)ir2 * d1 - (int64_t)ir1 * d2, shift), lm);
        break;
    }
    default:
        fprintf(stderr, "GTE: command %08x is not implemented\n", (unsigned)command);
        abort();
    }
    if (flag & FLAG_ERROR_MASK) {
        flag |= 0x80000000u;
    }
    gte.control[FLAG] = (int32_t)flag;
}

/* ---- register moves ---- */

static int32_t CountLeading(int32_t value)
{
    uint32_t v = value < 0 ? ~(uint32_t)value : (uint32_t)value;
    int32_t n = 0;

    while (n < 32 && !(v & 0x80000000u)) {
        v <<= 1;
        n++;
    }
    return n;
}

void GteWriteData(int reg, int32_t value)
{
    switch (reg) {
    case VZ0: case VZ1: case VZ2: case IR0: case IR1: case IR2: case IR3:
        gte.data[reg] = Low16(value);
        break;
    case OTZ: case SZ0: case SZ1: case SZ2: case SZ3:
        gte.data[reg] = (uint16_t)value;
        break;
    case SXYP:
        gte.data[SXY0] = gte.data[SXY1];
        gte.data[SXY1] = gte.data[SXY2];
        gte.data[SXY2] = value;
        sxyHandle[0] = sxyHandle[1];
        sxyHandle[1] = sxyHandle[2];
        sxyHandle[2] = 0;
        break;
    case SXY0: case SXY1: case SXY2:
        gte.data[reg] = value;
        sxyHandle[reg - SXY0] = 0;
        break;
    case IRGB:
        gte.data[IR1] = (value & 0x1F) << 7;
        gte.data[IR2] = ((value >> 5) & 0x1F) << 7;
        gte.data[IR3] = ((value >> 10) & 0x1F) << 7;
        break;
    case ORGB: case LZCR:
        break;
    case LZCS:
        gte.data[LZCS] = value;
        gte.data[LZCR] = CountLeading(value);
        break;
    default:
        gte.data[reg] = value;
        break;
    }
}

static int32_t ColourComponent(int32_t ir)
{
    ir >>= 7;
    return ir < 0 ? 0 : ir > 0x1F ? 0x1F : ir;
}

int32_t GteReadData(int reg)
{
    switch (reg) {
    case SXYP:
        return gte.data[SXY2];
    case IRGB: case ORGB:
        return ColourComponent(gte.data[IR1]) | (ColourComponent(gte.data[IR2]) << 5) |
               (ColourComponent(gte.data[IR3]) << 10);
    default:
        return gte.data[reg];
    }
}

void GteWriteControl(int reg, int32_t value)
{
    switch (reg) {
    case RT33: case L33: case LB3: case H: case DQA: case ZSF3: case ZSF4:
        gte.control[reg] = Low16(value);
        break;
    case FLAG:
        gte.control[FLAG] = (int32_t)(((uint32_t)value & 0x7FFFF000u) |
                                      (((uint32_t)value & FLAG_ERROR_MASK) ? 0x80000000u : 0));
        break;
    default:
        gte.control[reg] = value;
        break;
    }
}

int32_t GteReadControl(int reg)
{
    return gte.control[reg];
}

int GteTable16(unsigned int address)
{
    unsigned int offset = address - gteTableBase;

    if (address < gteTableBase || address + 2 > gteTableEnd) {
        fprintf(stderr, "GTE: library table read outside the tables (%08x)\n", address);
        abort();
    }
    return (int16_t)(gteTables[offset] | (gteTables[offset + 1] << 8));
}
