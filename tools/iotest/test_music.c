/* The game's music handling end to end: InitCD (table of contents, 0x4A sectors back), CdMusicPlay
   (Setmode 5, Setloc, Play), SetInGameCDTrack (GetlocP each frame; start the next song when the
   drive reports a different track), CdMusicStop (Pause) and resume, through libcd, the SPU's CD
   input, its volume and the master volume, down to the output samples. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../platform/psx/internal.h"
#include "testenv.h"
#include "testutil.h"

typedef struct { uint8_t minute, second, sector, track; } Loc;
typedef struct { int16_t left, right; } Volume;
typedef struct { Volume volume; int32_t reverb, mix; } ExtAttr;
typedef struct {
    uint32_t mask;
    Volume mvol, mvolmode, mvolx;
    ExtAttr cd, ext;
} CommonAttr;

int CdInit(void);
int CdSetDebug(int);
int CdControl(int, const uint8_t *, uint8_t *);
int CdControlB(int, const uint8_t *, uint8_t *);
int CdRead(int, void *, int);
int CdReadSync(int, uint8_t *);
int CdGetToc(Loc *);
void CdIntToPos(int, Loc *);
int CdPosToInt(const Loc *);
void SpuInit(void);
void SpuSetCommonAttr(const CommonAttr *);

#define btoi(b) ((((b) >> 4) * 10) + ((b) & 15))

static Loc cdTrackLocs[100];
static int cdMusicCount;
static signed char cdSetMode;

/* sound.c InitCD */
static int InitCD(Loc *locs)
{
    uint8_t mode = 5;
    int count, i;

    CdInit();
    CdSetDebug(0);
    CdControlB(0xE, &mode, 0);
    count = CdGetToc(locs);
    cdMusicCount = count;
    for (i = 1; i < count; i++) {
        CdIntToPos(CdPosToInt(&locs[i]) - 0x4A, &locs[i]);
    }
    return cdMusicCount;
}

/* sound.c CdMusicPlay / CdMusicStop */
static void CdMusicPlay(int track)
{
    cdSetMode = 5;
    CdControlB(0xE, (uint8_t *)&cdSetMode, 0);
    CdControlB(2, (uint8_t *)&cdTrackLocs[track], 0);
    CdControlB(3, 0, 0);
}

static void CdMusicStop(void)
{
    CdControlB(9, 0, 0);
}

static uint8_t position[8];

/* what SetInGameCDTrack reads each frame: track number (BCD) at result[0] */
static int polled_track(void)
{
    CdControlB(0x11, 0, position);
    return btoi(position[0]);
}

static void frames(int n)
{
    while (n-- > 0) {
        PsxVBlank();
    }
}

static void set_volumes(void)
{
    CommonAttr a;

    memset(&a, 0, sizeof(a));
    a.mvol.left = 0x3FC0;
    a.mvol.right = 0x3FC0;
    a.cd.volume.left = 0x2FC0; /* the music volume setting in the capture */
    a.cd.volume.right = 0x2FC0;
    a.cd.mix = 1;
    a.cd.reverb = 0;
    SpuSetCommonAttr(&a);
}

static int16_t sat(int64_t v)
{
    return (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
}

/* the output sample for a CD sample after the SPU's CD volume and the master volume */
static int16_t through_spu(int16_t cd, int cdVolume, int master)
{
    int32_t stage = sat(((int64_t)cd * cdVolume) >> 15);

    return sat(((int64_t)stage * master) >> 15);
}

int main(void)
{
    char cueDir[1024];
    uint8_t ref[2352];
    int i;

    if (!test_io_start(1.0, NULL)) {
        printf("test_music: disc not found, skipped\n");
        return 0;
    }
    snprintf(cueDir, sizeof(cueDir), "%s", test_disc_path());
    *strrchr(cueDir, '/') = 0;
    SpuInit();
    set_volumes();
    CHECK_EQ_INT(InitCD(cdTrackLocs), 12);

    /* CdMusicPlay(2): the first song starts 17 sectors into track 2's pregap */
    CdMusicPlay(2);
    frames(40); /* the seek takes a moment */
    CHECK_EQ_INT(polled_track(), 2);
    CHECK_EQ_INT(position[1], 0); /* index 00: still in the pregap */
    {
        uint32_t abs = (uint32_t)(btoi(position[5]) * 60 + btoi(position[6])) * 75 + (uint32_t)btoi(position[7]);

        CHECK(abs >= 36601 + 150 && abs < 36601 + 150 + 3 * 75);
    }

    /* the output is the disc's audio through the CD volume (0x2FC0) and master volume (0x3FC0 -> 0x7F80) */
    {
        int16_t out[2 * 4000];
        int32_t startPos;
        unsigned startPhase;
        long k;

        PsxLock();
        startPos = g_io.cd.pos;
        startPhase = g_io.cd.phase;
        PsxUnlock();
        PsxAudioRender(out, 4000);
        {
            long bad = 0;

            for (k = 0; k < 4000; k++) {
                int32_t lba = startPos + (int32_t)((startPhase + (unsigned)k) / 588);
                unsigned phase = (startPhase + (unsigned)k) % 588;
                char path[1100];
                FILE *f;
                int16_t l, r;
                int track = lba < 63059 ? 2 : 3;
                int32_t base = lba < 63059 ? 36584 : 63059;

                if (phase == 0 || k == 0) {
                    snprintf(path, sizeof(path), "%s/Wipeout XL (USA) (Track %02d).bin", cueDir, track);
                    f = fopen(path, "rb");
                    fseek(f, (long)(lba - base) * 2352, SEEK_SET);
                    CHECK_EQ_INT((long)fread(ref, 1, 2352, f), 2352);
                    fclose(f);
                }
                l = (int16_t)(ref[phase * 4] | ref[phase * 4 + 1] << 8);
                r = (int16_t)(ref[phase * 4 + 2] | ref[phase * 4 + 3] << 8);
                if (out[k * 2] != through_spu(l, 0x2FC0, 0x7F80) || out[k * 2 + 1] != through_spu(r, 0x2FC0, 0x7F80)) {
                    bad++;
                }
            }
            CHECK_EQ_INT(bad, 0);
        }
    }

    /* Pause (CdMusicStop) holds the position; resuming (CdControl 3) carries on from it */
    {
        uint8_t before[8], after[8];
        int16_t out[2 * 2000];
        long loud = 0, k;

        CdMusicStop();
        CdControlB(0x11, 0, before);
        frames(30);
        CdControlB(0x11, 0, after);
        CHECK(memcmp(before, after, 8) == 0);
        PsxAudioRender(out, 2000);
        for (k = 0; k < 4000; k++) {
            loud += out[k] != 0;
        }
        CHECK_EQ_INT(loud, 0); /* silent while paused */
        CdControl(3, 0, 0);
        frames(10);
        CdControlB(0x11, 0, after);
        CHECK(after[0] == before[0]);
        CHECK(btoi(after[7]) != btoi(before[7]) || after[6] != before[6]);
    }

    /* SetInGameCDTrack: play track 2 to its end; the drive runs into track 3's pregap and the game
       sees the new track number within a frame of the boundary. Start 200 sectors before it. */
    {
        Loc where;
        int32_t boundary = 63059; /* INDEX 00 of track 3 */
        long frameIndex = 0, detected = -1, expected;
        int32_t startLba = boundary - 200;

        CdIntToPos(startLba, &where);
        CdControlB(2, (uint8_t *)&where, 0);
        CdControlB(3, 0, 0);
        /* wait for the seek, then count frames from the moment the head is on the start sector */
        for (i = 0; i < 100; i++) {
            PsxLock();
            if (g_io.cd.state == CD_PLAYING) {
                PsxUnlock();
                break;
            }
            PsxUnlock();
            frames(1);
        }
        CHECK(polled_track() == 2);
        for (frameIndex = 0; frameIndex < 400 && detected < 0; frameIndex++) {
            int32_t pos;
            unsigned phase;

            PsxLock();
            pos = g_io.cd.pos;
            phase = g_io.cd.phase;
            PsxUnlock();
            /* the sample position of the head now, in samples since the start sector began */
            expected = (long)(pos - startLba) * 588 + phase;
            if (polled_track() == 3) {
                detected = expected;
            }
            frames(1);
        }
        printf("track change seen %ld samples after the start; the boundary is at %d\n", detected, 200 * 588);
        CHECK(detected >= 200 * 588);
        CHECK(detected < 200 * 588 + 736 + 600); /* within a frame of the boundary */
    }

    /* A data read stops the music: the head moves to track 1 and GetlocP says so (the game
       then starts the next song) */
    {
        static uint8_t buffer[2048 * 4];
        Loc loc;

        CdIntToPos(100, &loc);
        CdControl(2, (uint8_t *)&loc, 0);
        CdRead(4, buffer, 0x80);
        CdReadSync(0, 0);
        CHECK_EQ_INT(polled_track(), 1);
        CdMusicPlay(5);
        frames(60);
        CHECK_EQ_INT(polled_track(), 5);
    }
    test_io_stop();
    return test_report("music");
}
