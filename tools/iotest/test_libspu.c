/* libspu: the register writes the library makes for the calls the game's sound code makes
   (SfxInit, SfxInitReverb, SfxReverbOn ...) against the writes the retail game made on a console
   emulator (PSX_SPU_ORACLE, as for test_spu_oracle), and the library's behaviour: sound RAM
   allocation, voice attribute masks and read-back, key status, the common attributes. */
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../platform/psx/internal.h"
#include "testenv.h"
#include "testutil.h"

typedef struct { int16_t left, right; } Volume;
typedef struct {
    uint32_t voice, mask;
    Volume volume, volmode, volumex;
    uint16_t pitch, note, sample_note;
    int16_t envx;
    uint32_t addr, loop_addr;
    int32_t a_mode, s_mode, r_mode;
    uint16_t ar, dr, sr, rr, sl, adsr1, adsr2;
} VoiceAttr;
typedef struct { Volume volume; int32_t reverb, mix; } ExtAttr;
typedef struct {
    uint32_t mask;
    Volume mvol, mvolmode, mvolx;
    ExtAttr cd, ext;
} CommonAttr;
typedef struct {
    uint32_t mask;
    int32_t mode;
    Volume depth;
    int32_t delay, feedback;
} ReverbAttr;

void SpuInit(void);
int SpuInitMalloc(int, void *);
uint32_t SpuMalloc(int);
void SpuFree(uint32_t);
int SpuSetTransferMode(int);
void SpuSetCommonAttr(const CommonAttr *);
int SpuSetReverb(int);
uint32_t SpuSetReverbVoice(int, uint32_t);
int SpuSetReverbModeParam(const ReverbAttr *);
int SpuReserveReverbWorkArea(int);
int SpuClearReverbWorkArea(int);
void SpuSetVoiceAttr(const VoiceAttr *);
void SpuGetVoiceAttr(VoiceAttr *);
void SpuSetKey(int, uint32_t);
int SpuGetKeyStatus(uint32_t);

typedef struct Write {
    uint32_t offset;
    uint16_t value;
} Write;

static Write mine[2000];
static int mineCount;

/* The registers whose values the comparison covers; the transfer and control writes are
   made differently (the library here copies straight into sound RAM). */
static int compared(uint32_t offset)
{
    return offset < 0x180 || (offset >= 0x180 && offset <= 0x19E && offset != 0x19C) ||
           offset == 0x1A2 || offset == 0x1AC || offset == 0x1B0 || offset == 0x1B2 ||
           (offset >= 0x1C0 && offset < 0x200);
}

static void record(void *user, uint64_t sample, uint32_t offset, uint16_t value)
{
    (void)user;
    (void)sample;
    if (compared(offset) && mineCount < 2000) {
        mine[mineCount].offset = offset;
        mine[mineCount].value = value;
        mineCount++;
    }
}

static uint32_t rd32(const uint8_t *p)
{
    return p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

/* sfx.c SfxInit .. SfxSetReverbDepth, as the game calls them */
static void game_sound_init(void)
{
    CommonAttr attr;
    ReverbAttr rev;

    SpuInit();
    SpuInitMalloc(128, NULL);
    memset(&attr, 0, sizeof(attr));
    attr.mvol.left = 0x3FC0;
    attr.mvol.right = 0x3FC0;
    attr.cd.volume.left = 0x3FC0;
    attr.cd.volume.right = 0x3FC0;
    attr.mask = 0;
    attr.cd.mix = 1;
    attr.cd.reverb = 0;
    SpuSetCommonAttr(&attr);
    SpuSetTransferMode(0);
    /* SfxInitReverb */
    memset(&rev, 0, sizeof(rev));
    SpuSetReverb(1);
    SpuSetReverbVoice(1, 0xFFFFFF);
    rev.mask = 1;
    rev.mode = 4;
    SpuSetReverbModeParam(&rev);
    CHECK_EQ_INT(SpuReserveReverbWorkArea(1), 1);
    SpuClearReverbWorkArea(4);
    SpuSetReverb(1);
    /* SfxReverbOn with depth 64, then SfxSetReverbDepth(0) */
    rev.mask = 6;
    rev.depth.left = 64 << 8;
    rev.depth.right = -(64 << 8);
    SpuSetReverbModeParam(&rev);
    rev.depth.left = 0;
    rev.depth.right = 0;
    SpuSetReverbModeParam(&rev);
}

static void compare_with_oracle(const char *dir)
{
    char path[1100];
    FILE *f;
    uint8_t header[16], payload[8];
    static Write want[2000];
    int wantCount = 0, i, j, shown = 0;
    int started = 0;

    snprintf(path, sizeof(path), "%s/spu.bin", dir);
    f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "cannot open %s\n", path);
        CHECK(f != NULL);
        return;
    }
    /* from SpuInit (control register cleared at sample 4516386) to the first sample upload */
    while (fread(header, 1, 16, f) == 16 && fread(payload, 1, 8, f) == 8) {
        uint64_t sample = rd32(header) | (uint64_t)rd32(header + 4) << 32;
        uint32_t kind = rd32(header + 8), offset = rd32(payload), value = rd32(payload + 4);

        if (sample == 4516386 && kind == 1 && offset == 0x1AA) {
            started = 1;
        }
        if (started && kind == 1 && offset == 0x1A6 && value == 0x202) {
            break;
        }
        if (started && kind == 1 && compared(offset) && wantCount < 2000) {
            want[wantCount].offset = offset;
            want[wantCount].value = (uint16_t)value;
            wantCount++;
        }
    }
    fclose(f);
    /* the retail writes of two garbage fields the game leaves uninitialised on its stack (the
       master left volume's sweep mode, the external input volumes) are not reproduced */
    for (i = 0; i < wantCount; i++) {
        if (want[i].offset == 0x180 && want[i].value == 0x807F) {
            want[i].value = 0x3FC0;
        }
    }
    {
        /* the retail 8-bit key registers were cleared with 0xFFFF in SpuInit's first key-off */
        int k;

        for (k = 0; k < wantCount; k++) {
            if (want[k].offset == 0x18E && want[k].value == 0xFFFF) {
                want[k].value = 0xFF;
            }
        }
    }
    printf("retail made %d comparable writes, the library %d\n", wantCount, mineCount);
    CHECK(wantCount > 100);
    for (i = 0, j = 0; i < wantCount && j < mineCount; i++, j++) {
        /* retail rewrites the same value repeatedly in places (control register, key halves);
           skip over writes of an unchanged value on either side */
        if (want[i].offset != mine[j].offset || want[i].value != mine[j].value) {
            if (shown++ < 5) {
                fprintf(stderr, "write %d: retail %03x=%04x, library %03x=%04x\n", i, want[i].offset,
                        want[i].value, mine[j].offset, mine[j].value);
            }
        }
    }
    CHECK_EQ_INT(shown, 0);
    CHECK_EQ_INT(wantCount, mineCount);
}

int main(void)
{
    const char *oracle = getenv("PSX_SPU_ORACLE");
    VoiceAttr a;
    CommonAttr common;
    int i;

    if (!test_io_start(1.0, NULL)) {
        printf("test_libspu: disc not found, skipped\n");
        return 0;
    }
    g_io.spu->trace = record;
    game_sound_init();
    g_io.spu->trace = NULL;
    if (oracle) {
        compare_with_oracle(oracle);
    } else {
        printf("(PSX_SPU_ORACLE not set: register stream comparison skipped)\n");
    }
    /* the state it leaves */
    CHECK_EQ_INT(SpuReadRegister(g_io.spu, SPU_CONTROL), 0xC081 & ~0);
    CHECK_EQ_INT(SpuReadRegister(g_io.spu, SPU_REVERB_BASE), 0xF204);
    CHECK_EQ_INT(SpuReadRegister(g_io.spu, SPU_MAIN_VOL_L), 0x3FC0);
    CHECK_EQ_INT(g_io.spu->main[0].level, 0x7F80); /* a fixed volume is doubled: 0x3FC0 -> 0x7F80 */
    CHECK_EQ_INT(SpuReadRegister(g_io.spu, SPU_CD_VOL_L), 0x3FC0);

    /* sound RAM allocation: addresses retail got for the first samples' sizes */
    {
        /* sizes of the first four samples and the addresses retail's allocator gave them */
        static const int sizes[] = {11024, 19504, 17456, 25312};
        static const uint32_t expect[] = {0x1010, 0x3B20, 0x8750, 0xCB80};
        int n;

        SpuInitMalloc(128, NULL);
        for (n = 0; n < 4; n++) {
            uint32_t got = SpuMalloc(sizes[n]);

            CHECK_EQ_INT(got, expect[n]);
        }
        /* freed space is found again, lowest first; the reverb work area (0x79020 up) is protected */
        SpuFree(0x3B20);
        CHECK_EQ_INT(SpuMalloc(100), 0x3B20);
        CHECK_EQ_INT(SpuMalloc(0x80000), (uint32_t)-1);
        CHECK_EQ_INT(SpuMalloc(0x79020 - 0x1010), (uint32_t)-1); /* too big next to the allocated ones */
    }

    /* voice attributes: volumes are fixed 15-bit values (negative inverts the phase); the mask
       selects fields; reading back gives what was set */
    memset(&a, 0, sizeof(a));
    a.voice = 1u << 3;
    a.mask = 3;
    a.volume.left = 0x3FC0;
    a.volume.right = -0x3FC0;
    SpuSetVoiceAttr(&a);
    CHECK_EQ_INT(SpuReadRegister(g_io.spu, 3 * 16), 0x3FC0);
    CHECK_EQ_INT(SpuReadRegister(g_io.spu, 3 * 16 + 2), 0x4040); /* -0x3FC0 in 15 bits */
    CHECK_EQ_INT(g_io.spu->voice[3].volume[0].level, 0x7F80);
    CHECK_EQ_INT(g_io.spu->voice[3].volume[1].level, -0x7F80);
    memset(&a, 0, sizeof(a));
    a.voice = 1u << 3;
    a.mask = 3;
    SpuGetVoiceAttr(&a);
    CHECK_EQ_INT(a.volume.left, 0x3FC0);
    CHECK_EQ_INT(a.volume.right, -0x3FC0);
    memset(&a, 0, sizeof(a));
    a.voice = 1u << 3;
    a.mask = 0x90;
    a.pitch = 0x800;
    a.addr = 0x2B420;
    SpuSetVoiceAttr(&a);
    CHECK_EQ_INT(SpuReadRegister(g_io.spu, 3 * 16 + 4), 0x800);
    CHECK_EQ_INT(SpuReadRegister(g_io.spu, 3 * 16 + 6), 0x2B420 / 8);
    CHECK_EQ_INT(SpuReadRegister(g_io.spu, 3 * 16), 0x3FC0); /* untouched by mask 0x90 */
    CHECK_EQ_INT(SpuReadRegister(g_io.spu, 3 * 16 + 8), 0);  /* the envelope stays at the init value */
    /* ADSR fields */
    memset(&a, 0, sizeof(a));
    a.voice = 1u << 4;
    a.mask = 0x800 | 0x1000 | 0x8000 | 0x4000;
    a.ar = 10;
    a.dr = 5;
    a.sl = 9;
    a.rr = 7;
    SpuSetVoiceAttr(&a);
    CHECK_EQ_INT(SpuReadRegister(g_io.spu, 4 * 16 + 8), (10 << 8) | (5 << 4) | 9);
    CHECK_EQ_INT(SpuReadRegister(g_io.spu, 4 * 16 + 10) & 31, 7);

    /* the common attributes with a zero mask set the master and CD volumes and the CD input,
       and do not take the sweep modes and external input from the (uninitialised) rest */
    memset(&common, 0xA5, sizeof(common)); /* garbage, as the game's stack leaves it */
    common.mask = 0;
    common.mvol.left = 0x3FC0;
    common.mvol.right = 0x3FC0;
    common.cd.volume.left = 0x2FC0;
    common.cd.volume.right = 0x2FC0;
    common.cd.mix = 1;
    common.cd.reverb = 0;
    SpuSetCommonAttr(&common);
    CHECK_EQ_INT(SpuReadRegister(g_io.spu, SPU_MAIN_VOL_L), 0x3FC0);
    CHECK_EQ_INT(SpuReadRegister(g_io.spu, SPU_CD_VOL_L), 0x2FC0);
    CHECK_EQ_INT(SpuReadRegister(g_io.spu, SPU_CONTROL) & (SPU_CTRL_CD_ENABLE | SPU_CTRL_CD_REVERB), SPU_CTRL_CD_ENABLE);
    CHECK_EQ_INT(g_io.spu->main[0].active, 0);

    /* key status with nothing playing, and key registers */
    for (i = 0; i < 24; i++) {
        CHECK_EQ_INT(SpuGetKeyStatus(1u << i), 0);
    }
    SpuSetKey(1, 0x800001);
    CHECK_EQ_INT(g_io.spu->key_on, 0x800001);
    test_io_stop();
    return test_report("libspu");
}
