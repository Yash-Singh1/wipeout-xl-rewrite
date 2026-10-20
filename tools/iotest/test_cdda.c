/* CD-DA delivery: the samples the drive hands to the SPU are exactly the disc's audio, starting
   and ending at the sectors the commands name, with the positions GetlocP reports, and through
   the SPU's CD input path to the final mix. The reference is read straight from the .bin files. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../platform/psx/cdrom.h"
#include "../../platform/psx/internal.h"
#include "testenv.h"
#include "testutil.h"

static char cueDir[1024];

/* One sector of a track's .bin file, read with stdio (not through the disc module's mapping). */
static int read_reference(int trackNumber, const DiscTrack *t, int32_t lba, uint8_t out[2352])
{
    char path[1100];
    FILE *f;
    int ok;

    snprintf(path, sizeof(path), "%s/Wipeout XL (USA) (Track %02d).bin", cueDir, trackNumber);
    f = fopen(path, "rb");
    if (!f) {
        return 0;
    }
    ok = fseek(f, (long)(lba - t->index0) * 2352, SEEK_SET) == 0 && fread(out, 1, 2352, f) == 2352;
    fclose(f);
    return ok;
}

static uint8_t response[16];

static void step(CdDrive *cd, long n)
{
    int16_t out[2];

    while (n-- > 0) {
        CdDriveSample(cd, out);
    }
}

static void command(CdDrive *cd, int c, const uint8_t *p, int n)
{
    CdDriveCommand(cd, c, p, n, response);
    step(cd, 100); /* the response arrives */
}

/* Runs until the drive plays (or reads), returns the samples spent seeking. */
static long seek_until_playing(CdDrive *cd)
{
    long n = 0;
    int16_t out[2];

    while (cd->state == CD_SEEKING && n < 100000) {
        CdDriveSample(cd, out);
        n++;
    }
    return n;
}

int main(void)
{
    const char *cue = test_disc_path();
    char error[256];
    Disc *disc;
    CdDrive cd;
    const DiscTrack *t2, *t3;
    uint8_t loc[3];
    uint8_t ref[2352];
    int16_t out[2];
    long i;

    if (!cue) {
        printf("test_cdda: disc not found, skipped\n");
        return 0;
    }
    snprintf(cueDir, sizeof(cueDir), "%s", cue);
    *strrchr(cueDir, '/') = 0;
    disc = DiscOpen(cue, error, sizeof(error));
    CHECK(disc != NULL);
    if (!disc) {
        return 1;
    }

    /* the layout: 12 tracks; every audio track starts with its 150-sector pregap (INDEX 00) */
    CHECK_EQ_INT(DiscTrackCount(disc), 12);
    t2 = DiscTrackInfo(disc, 1);
    t3 = DiscTrackInfo(disc, 2);
    CHECK_EQ_INT(t2->index0, 36584);
    CHECK_EQ_INT(t2->index1, 36584 + 150);
    CHECK_EQ_INT(t3->index0, 63059);
    CHECK_EQ_INT(t2->end, t3->index0);
    CHECK_EQ_INT(DiscTrackInfo(disc, 0)->end, 36584);

    CdDriveInit(&cd, disc, 1.0);
    command(&cd, CDC_SETMODE, (const uint8_t[]){CDM_CDDA | CDM_REPORT}, 1);

    /* the table of contents as libcd returns it: whole seconds, entry 0 the lead-out */
    {
        uint8_t toc[100][4];
        int count = CdDriveToc(&cd, toc);
        int32_t lba;

        CHECK_EQ_INT(count, 12);
        /* track 2 starts at LBA 36734 = absolute 8:11:59, reported as 08:11:00 */
        CHECK_EQ_INT(toc[2][0], 0x08);
        CHECK_EQ_INT(toc[2][1], 0x11);
        CHECK_EQ_INT(toc[2][2], 0x00);
        lba = CdMsfToLba(toc[2]);
        CHECK_EQ_INT(lba, 36675);
        /* InitCD subtracts 0x4A sectors: the drive is sent to 17 sectors into track 2's pregap */
        CHECK_EQ_INT(lba - 0x4A, 36601);
        CHECK_EQ_INT(36601 - t2->index0, 17);
        CdLbaToMsf(lba - 0x4A, loc);
        CHECK_EQ_INT(loc[0], 0x08);
        CHECK_EQ_INT(loc[1], 0x10);
        CHECK_EQ_INT(loc[2], 0x01);
    }

    /* CdMusicPlay: Setloc, Play with no parameter */
    command(&cd, CDC_SETLOC, loc, 3);
    command(&cd, CDC_PLAY, NULL, 0);
    CHECK(cd.state == CD_SEEKING);
    seek_until_playing(&cd);
    CHECK(cd.state == CD_PLAYING);
    CHECK_EQ_INT(cd.pos, 36601);

    /* the first sector delivered is LBA 36601: compare 3 sectors sample for sample */
    for (i = 0; i < 3; i++) {
        long k;

        CHECK(read_reference(2, t2, 36601 + i, ref));
        for (k = 0; k < 588; k++) {
            CdDriveSample(&cd, out);
            CHECK_EQ_INT(out[0], (int16_t)(ref[k * 4] | ref[k * 4 + 1] << 8));
            CHECK_EQ_INT(out[1], (int16_t)(ref[k * 4 + 2] | ref[k * 4 + 3] << 8));
        }
    }
    CHECK_EQ_INT(cd.pos, 36604);

    /* GetlocP: track 2, index 0 (pregap), counting down; absolute position 8:10:04 */
    CdDriveCommand(&cd, CDC_GETLOCP, NULL, 0, response);
    CHECK_EQ_INT(response[0], 0x02);
    CHECK_EQ_INT(response[1], 0x00);
    /* relative time: INDEX 01 is at 36734, so 36734 - 36604 - 1 = 129 sectors to go: 00:01:54 */
    CHECK_EQ_INT(response[2], 0x00);
    CHECK_EQ_INT(response[3], 0x01);
    CHECK_EQ_INT(response[4], 0x54);
    CHECK_EQ_INT(response[5], 0x08);
    CHECK_EQ_INT(response[6], 0x10);
    CHECK_EQ_INT(response[7], 0x04);
    step(&cd, 100);

    /* Pause keeps the position; Play with no parameters resumes from it */
    command(&cd, CDC_PAUSE, NULL, 0);
    CHECK(cd.state == CD_IDLE);
    {
        int32_t held = cd.pos;

        step(&cd, 20000);
        CHECK_EQ_INT(cd.pos, held);
        command(&cd, CDC_PLAY, NULL, 0);
        seek_until_playing(&cd);
        CHECK(cd.state == CD_PLAYING);
        CHECK_EQ_INT(cd.pos, held);
    }

    /* the end of track 2 is the start of track 3's pregap: the drive reports track 3, index 0,
       exactly when the first sector of the next file plays, and the audio continues unbroken */
    CdLbaToMsf(t3->index0 - 3, loc);
    command(&cd, CDC_SETLOC, loc, 3);
    command(&cd, CDC_PLAY, NULL, 0);
    seek_until_playing(&cd);
    CHECK_EQ_INT(cd.pos, t3->index0 - 3);
    for (i = 0; i < 6; i++) {
        long k;
        int32_t lba = t3->index0 - 3 + (int32_t)i;
        const DiscTrack *t = lba < t3->index0 ? t2 : t3;
        uint8_t where[8];

        CHECK(read_reference(lba < t3->index0 ? 2 : 3, t, lba, ref));
        CdDriveCommand(&cd, CDC_GETLOCP, NULL, 0, where);
        CHECK_EQ_INT(where[0], lba < t3->index0 ? 0x02 : 0x03);
        CHECK_EQ_INT(where[1], lba < t3->index0 ? 0x01 : 0x00); /* track 2 index 1, then track 3's pregap */
        for (k = 0; k < 588; k++) {
            CdDriveSample(&cd, out);
            CHECK_EQ_INT(out[0], (int16_t)(ref[k * 4] | ref[k * 4 + 1] << 8));
            CHECK_EQ_INT(out[1], (int16_t)(ref[k * 4 + 2] | ref[k * 4 + 3] << 8));
        }
    }

    /* the CD volume matrix: unity by default, right-only when set so */
    {
        uint8_t mixed[4] = {0, 0x80, 0, 0x80}; /* L->R and R->L: swap the channels */
        int16_t plain[2], swapped[2];
        CdDrive a, b;

        CdLbaToMsf(t2->index1 + 5000, loc);
        for (i = 0; i < 2; i++) {
            CdDrive *d = i ? &b : &a;

            CdDriveInit(d, disc, 4.0);
            if (i) {
                memcpy(d->mix, mixed, 4);
            }
            CdDriveCommand(d, CDC_SETLOC, loc, 3, response);
            step(d, 100);
            CdDriveCommand(d, CDC_PLAY, NULL, 0, response);
            step(d, 100);
            seek_until_playing(d);
            step(d, 1000);
            CdDriveSample(d, i ? swapped : plain);
        }
        CHECK_EQ_INT(swapped[0], plain[1]);
        CHECK_EQ_INT(swapped[1], plain[0]);
    }

    /* the lead-out: play stops at the end of the disc with a data-end event */
    {
        CdDrive d;
        CdEventKind kind;
        bool sawEnd = false;

        CdDriveInit(&d, disc, 8.0);
        CdLbaToMsf(DiscLeadOut(disc) - 2, loc);
        CdDriveCommand(&d, CDC_SETLOC, loc, 3, response);
        step(&d, 100);
        CdDriveCommand(&d, CDC_PLAY, NULL, 0, response);
        step(&d, 100);
        seek_until_playing(&d);
        step(&d, 588 * 3);
        while (CdDrivePopEvent(&d, &kind)) {
            sawEnd |= kind == CDE_DATA_END;
        }
        CHECK(sawEnd);
        CHECK(d.state == CD_IDLE);
    }
    DiscClose(disc);
    return test_report("cdda");
}
