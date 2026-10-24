/* The ways a CD-ROM data track is kept: an .iso (2048-byte sectors) by itself or in a .cue, raw sectors, a CloneCD .ccd
   with its .img, an Alcohol 120% .mds with its .mdf. A small ISO 9660 volume holding the PC game's marker files is made
   here, with a few audio sectors; every layout gives the same sectors, and the 2048-byte one is given its sync and header. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../../platform/psx/disc.h"
#include "testutil.h"

#define DATA_SECTORS 24
#define AUDIO_SECTORS 6 /* each audio track: 2 of pregap, then 4 */
#define FILE_LBA 21
#define FILE_SIZE 100

static char folder[] = "/tmp/psxio-discfmt-XXXXXX";
static char made[16][64];
static int madeCount;

static void put32both(uint8_t *p, uint32_t v)
{
    p[0] = p[7] = (uint8_t)v;
    p[1] = p[6] = (uint8_t)(v >> 8);
    p[2] = p[5] = (uint8_t)(v >> 16);
    p[3] = p[4] = (uint8_t)(v >> 24);
}

/* A directory record of the volume at p; returns its length. */
static int record(uint8_t *p, const char *name, uint32_t lba, uint32_t size, int directory)
{
    int length = 33 + (int)strlen(name);

    length += length & 1;
    memset(p, 0, (size_t)length);
    p[0] = (uint8_t)length;
    put32both(p + 2, lba);
    put32both(p + 10, size);
    p[25] = directory ? 2 : 0;
    p[32] = (uint8_t)strlen(name);
    memcpy(p + 33, name, strlen(name));
    return length;
}

/* The volume: sector 16 its descriptor, 18 the root, 19 COMMON, 20 TRACK01, and the file COMMON\VECTO.PRM at 21. */
static void make_iso(uint8_t *iso, int withTrack)
{
    uint8_t *sector;
    int at, i;

    memset(iso, 0, DATA_SECTORS * 2048);
    sector = iso + 16 * 2048;
    sector[0] = 1;
    memcpy(sector + 1, "CD001", 5);
    sector[6] = 1;
    memcpy(sector + 8, "TESTSYSTEM", 10);
    record(sector + 156, "\0", 18, 2048, 1);
    iso[17 * 2048] = 255;
    memcpy(iso + 17 * 2048 + 1, "CD001", 5);
    sector = iso + 18 * 2048;
    at = record(sector, "COMMON", 19, 2048, 1);
    if (withTrack) {
        record(sector + at, "TRACK01", 20, 2048, 1);
    }
    record(iso + 19 * 2048, "VECTO.PRM;1", FILE_LBA, FILE_SIZE, 0);
    for (i = 0; i < FILE_SIZE; i++) {
        iso[FILE_LBA * 2048 + i] = (uint8_t)(i * 7 + 3);
    }
}

static void raw_sector(uint8_t *raw, int lba, const uint8_t *data)
{
    int a = lba + 150;

    memset(raw, 0xFF, 12);
    raw[0] = raw[11] = 0;
    raw[12] = (uint8_t)((a / 4500 / 10) << 4 | a / 4500 % 10);
    raw[13] = (uint8_t)((a / 75 % 60 / 10) << 4 | a / 75 % 60 % 10);
    raw[14] = (uint8_t)((a % 75 / 10) << 4 | a % 75 % 10);
    raw[15] = 1;
    memcpy(raw + 16, data, 2048);
    memset(raw + 2064, 0, 288);
}

/* An audio track's file: its pregap of silence, then sectors of a pattern that tells the tracks apart. */
static void audio(uint8_t *out, int track)
{
    int i;

    memset(out, 0, AUDIO_SECTORS * DISC_RAW_SECTOR);
    for (i = 2 * DISC_RAW_SECTOR; i < AUDIO_SECTORS * DISC_RAW_SECTOR; i++) {
        out[i] = (uint8_t)(i * 5 + track * 61 + i / 256);
    }
}

static void write_file(const char *name, const void *data, size_t size, int count)
{
    char path[128];
    FILE *f;

    snprintf(path, sizeof(path), "%s/%s", folder, name);
    f = fopen(path, "wb");
    CHECK(f != NULL);
    if (f) {
        CHECK(fwrite(data, 1, size * (size_t)count, f) == size * (size_t)count);
        fclose(f);
    }
    snprintf(made[madeCount++], sizeof(made[0]), "%s", name);
}

static void write_text(const char *name, const char *text)
{
    write_file(name, text, strlen(text), 1);
}

static Disc *open_made(const char *name)
{
    char path[128], error[256] = "";
    Disc *disc;

    snprintf(path, sizeof(path), "%s/%s", folder, name);
    disc = DiscOpen(path, error, sizeof(error));
    if (!disc) {
        fprintf(stderr, "%s: %s\n", name, error);
    }
    CHECK(disc != NULL);
    return disc;
}

/* The data track and the PC game's file as every layout gives them; with audio, tracks 2 and 3 as well. */
static void check_disc(const char *name, const uint8_t *iso, int sectorSize, int tracks)
{
    Disc *disc = open_made(name);
    uint8_t raw[DISC_RAW_SECTOR], file[2048], pattern[AUDIO_SECTORS * DISC_RAW_SECTOR];
    int32_t lba;
    uint32_t size;
    int i, t;

    if (!disc) {
        return;
    }
    CHECK_EQ_INT(DiscTrackCount(disc), tracks);
    CHECK(!DiscTrackInfo(disc, 0)->audio);
    CHECK_EQ_INT(DiscTrackInfo(disc, 0)->sector_size, sectorSize);
    CHECK_EQ_INT(DiscTrackInfo(disc, 0)->end, DATA_SECTORS);
    CHECK(DiscIsPcGame(disc));
    CHECK(DiscFindFile(disc, "\\COMMON\\VECTO.PRM", &lba, &size));
    CHECK_EQ_INT(lba, FILE_LBA);
    CHECK_EQ_INT(size, FILE_SIZE);
    CHECK(DiscReadData(disc, lba, 1, file) && !memcmp(file, iso + FILE_LBA * 2048, FILE_SIZE));
    for (i = 0; i < DATA_SECTORS; i++) {
        const uint8_t *got = DiscRawSector(disc, i);

        raw_sector(raw, i, iso + i * 2048);
        CHECK(got && !memcmp(got, raw, DISC_RAW_SECTOR));
    }
    for (t = 1; tracks == 3 && t < 3; t++) {
        const DiscTrack *track = DiscTrackInfo(disc, t);

        audio(pattern, t + 1);
        CHECK(track->audio);
        CHECK_EQ_INT(track->index0, DATA_SECTORS + (t - 1) * AUDIO_SECTORS);
        CHECK_EQ_INT(track->index1, track->index0 + 2);
        for (i = 0; i < AUDIO_SECTORS; i++) {
            const uint8_t *got = DiscRawSector(disc, track->index0 + i);

            CHECK(got && !memcmp(got, pattern + i * DISC_RAW_SECTOR, DISC_RAW_SECTOR));
        }
    }
    CHECK_EQ_INT(DiscLeadOut(disc) >= DATA_SECTORS + (tracks == 3 ? 2 * AUDIO_SECTORS : 0), 1);
    DiscClose(disc);
}

/* An .mds of one session: the three entries that are not tracks, then the data track and two audio tracks (with the
   2-sector pregap each names in its extra block along with its length), at the offsets given in the .mdf. */
static void descriptor_mds(const char *name, int dataSize, uint64_t offset2, uint64_t offset3)
{
    enum { BLOCKS = 6, TRACKS_AT = 88 + 24, EXTRA_AT = TRACKS_AT + 80 * BLOCKS };
    uint8_t mds[EXTRA_AT + 8 * BLOCKS];
    static const uint8_t points[BLOCKS] = {0xA0, 0xA1, 0xA2, 1, 2, 3};
    uint32_t starts[BLOCKS] = {0, 0, 0, 0, DATA_SECTORS + 2, DATA_SECTORS + AUDIO_SECTORS + 2};
    uint64_t offsets[BLOCKS] = {0, 0, 0, 0, offset2, offset3};
    int k;

    memset(mds, 0, sizeof(mds));
    memcpy(mds, "MEDIA DESCRIPTOR", 16);
    mds[16] = 1;
    mds[17] = 3;
    mds[20] = 1; /* one session */
    mds[80] = 88;
    mds[88 + 10] = BLOCKS;
    mds[88 + 20] = TRACKS_AT;
    for (k = 0; k < BLOCKS; k++) {
        uint8_t *b = mds + TRACKS_AT + 80 * k;
        uint32_t extra = EXTRA_AT + 8 * (uint32_t)k;
        uint32_t pregap = k > 3 ? 2 : 0, length = k > 3 ? AUDIO_SECTORS - 2 : DATA_SECTORS;
        int size = k < 3 ? 0 : k == 3 ? dataSize : DISC_RAW_SECTOR;

        b[4] = points[k];
        if (k < 3) {
            continue;
        }
        b[0] = k == 3 ? 0xAA : 0xA9;
        b[16] = (uint8_t)(size & 255);
        b[17] = (uint8_t)(size >> 8);
        memcpy(b + 36, &starts[k], 4);
        memcpy(b + 40, &offsets[k], 8);
        memcpy(b + 12, &extra, 4);
        memcpy(mds + extra, &pregap, 4);
        memcpy(mds + extra + 4, &length, 4);
    }
    write_file(name, mds, sizeof(mds), 1);
}

int main(void)
{
    static uint8_t iso[DATA_SECTORS * 2048], noTrack[DATA_SECTORS * 2048], rawData[DATA_SECTORS * DISC_RAW_SECTOR];
    static uint8_t a2[AUDIO_SECTORS * DISC_RAW_SECTOR], a3[AUDIO_SECTORS * DISC_RAW_SECTOR];
    static uint8_t all[DATA_SECTORS * DISC_RAW_SECTOR + 2 * AUDIO_SECTORS * DISC_RAW_SECTOR];
    static uint8_t mixed[DATA_SECTORS * 2048 + 2 * AUDIO_SECTORS * DISC_RAW_SECTOR];
    char path[128];
    Disc *disc;
    int i;

    if (!mkdtemp(folder)) {
        perror("mkdtemp");
        return 1;
    }
    make_iso(iso, 1);
    make_iso(noTrack, 0);
    for (i = 0; i < DATA_SECTORS; i++) {
        raw_sector(rawData + i * DISC_RAW_SECTOR, i, iso + i * 2048);
    }
    audio(a2, 2);
    audio(a3, 3);
    memcpy(all, rawData, sizeof(rawData));
    memcpy(all + sizeof(rawData), a2, sizeof(a2));
    memcpy(all + sizeof(rawData) + sizeof(a2), a3, sizeof(a3));
    memcpy(mixed, iso, sizeof(iso));
    memcpy(mixed + sizeof(iso), a2, sizeof(a2));
    memcpy(mixed + sizeof(iso) + sizeof(a2), a3, sizeof(a3));

    /* an .iso by itself: the data track, then the music tracks the game counts on, silent */
    write_file("game.iso", iso, sizeof(iso), 1);
    check_disc("game.iso", iso, 2048, 12);
    disc = open_made("game.iso");
    CHECK(disc && DiscRawSector(disc, DATA_SECTORS + 3) != NULL);
    DiscClose(disc);
    write_file("data.bin", rawData, sizeof(rawData), 1);
    check_disc("data.bin", iso, 2352, 12);

    /* a .cue: 2048-byte data track and audio tracks; the same in raw sectors in one file */
    write_file("t2.bin", a2, sizeof(a2), 1);
    write_file("t3.bin", a3, sizeof(a3), 1);
    write_text("cooked.cue", "FILE \"game.iso\" BINARY\n  TRACK 01 MODE1/2048\n    INDEX 01 00:00:00\n"
                             "FILE \"t2.bin\" BINARY\n  TRACK 02 AUDIO\n    INDEX 00 00:00:00\n    INDEX 01 00:00:02\n"
                             "FILE \"t3.bin\" BINARY\n  TRACK 03 AUDIO\n    INDEX 00 00:00:00\n    INDEX 01 00:00:02\n");
    check_disc("cooked.cue", iso, 2048, 3);
    write_file("all.bin", all, sizeof(all), 1);
    write_text("raw.cue", "FILE \"all.bin\" BINARY\n  TRACK 01 MODE1/2352\n    INDEX 01 00:00:00\n"
                          "  TRACK 02 AUDIO\n    INDEX 00 00:00:24\n    INDEX 01 00:00:26\n"
                          "  TRACK 03 AUDIO\n    INDEX 00 00:00:30\n    INDEX 01 00:00:32\n");
    check_disc("raw.cue", iso, 2352, 3);

    /* CloneCD */
    write_file("clone.img", all, sizeof(all), 1);
    write_text("clone.ccd", "[CloneCD]\nVersion=3\n[Disc]\nSessions=1\n[Entry 0]\nPoint=0xa0\nPLBA=1\n"
                            "[TRACK 1]\nMODE=1\nINDEX 1=0\n[TRACK 2]\nMODE=0\nINDEX 0=24\nINDEX 1=26\n"
                            "[TRACK 3]\nMODE=0\nINDEX 0=30\nINDEX 1=32\n");
    check_disc("clone.ccd", iso, 2352, 3);

    /* Alcohol 120%: raw sectors throughout, and a 2048-byte data track before the audio */
    write_file("raw.mdf", all, sizeof(all), 1);
    descriptor_mds("raw.mds", 2352, (uint64_t)DATA_SECTORS * 2352 + 2 * 2352,
                   (uint64_t)DATA_SECTORS * 2352 + (AUDIO_SECTORS + 2) * 2352);
    check_disc("raw.mds", iso, 2352, 3);
    write_file("mix.mdf", mixed, sizeof(mixed), 1);
    descriptor_mds("mix.mds", 2048, (uint64_t)DATA_SECTORS * 2048 + 2 * 2352,
                   (uint64_t)DATA_SECTORS * 2048 + (AUDIO_SECTORS + 2) * 2352);
    check_disc("mix.mds", iso, 2048, 3);

    /* not the PC game: no TRACK01 folder */
    write_file("other.iso", noTrack, sizeof(noTrack), 1);
    disc = open_made("other.iso");
    CHECK(disc && !DiscIsPcGame(disc));
    DiscClose(disc);

    /* damaged descriptors and images that are no data track */
    snprintf(path, sizeof(path), "%s/%s", folder, "junk.iso");
    write_file("junk.iso", all, 1000, 1);
    CHECK(DiscOpen(path, NULL, 0) == NULL);
    write_text("bad.ccd", "[CloneCD]\n[TRACK 1]\nMODE=1\n");
    snprintf(path, sizeof(path), "%s/%s", folder, "bad.ccd");
    CHECK(DiscOpen(path, NULL, 0) == NULL);
    write_text("bad.mds", "MEDIA DESCRIPTOR");
    snprintf(path, sizeof(path), "%s/%s", folder, "bad.mds");
    CHECK(DiscOpen(path, NULL, 0) == NULL);

    for (i = 0; i < madeCount; i++) {
        snprintf(path, sizeof(path), "%s/%s", folder, made[i]);
        remove(path);
    }
    rmdir(folder);
    return test_report("test_discfmt");
}
