/* WAV tracks as CD-DA sectors: the samples, the cue's gaps, and the next file's address. No disc needed. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../../platform/psx/disc.h"
#include "testutil.h"

#define PCM_BYTES (3 * DISC_RAW_SECTOR + 4)
#define PCM_OFFSET 54

static char folder[] = "/tmp/psxio-disc-XXXXXX";
static char cuePath[256];

static void put32(uint8_t *p, uint32_t value)
{
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
    p[2] = (uint8_t)(value >> 16);
    p[3] = (uint8_t)(value >> 24);
}

static void write_file(const char *name, const void *data, size_t size)
{
    char path[256];
    FILE *f;

    snprintf(path, sizeof(path), "%s/%s", folder, name);
    f = fopen(path, "wb");
    CHECK(f != NULL);
    if (f) {
        CHECK(fwrite(data, 1, size, f) == size);
        CHECK_EQ_INT(fclose(f), 0);
    }
}

static void write_cue(const char *type, const char *mode, int gap)
{
    FILE *f = fopen(cuePath, "w");

    CHECK(f != NULL);
    if (f) {
        fprintf(f, "FILE \"raw.bin\" BINARY\n  TRACK 01 MODE2/2352\n    INDEX 01 00:00:00\n"
                   "FILE \"music.wav\" %s\n  TRACK 02 %s\n%s"
                   "FILE \"raw.bin\" BINARY\n  TRACK 03 AUDIO\n    INDEX 01 00:00:00\n",
                type, mode, gap ? "    PREGAP 00:00:01\n    INDEX 01 00:00:00\n"
                                : "    INDEX 00 00:00:00\n    INDEX 01 00:00:01\n");
        CHECK_EQ_INT(fclose(f), 0);
    }
}

static void compare_samples(const uint8_t *pcm, const uint8_t *raw, int gap)
{
    char error[512] = "";
    Disc *disc = DiscOpen(cuePath, error, sizeof(error));
    int i;

    if (!disc) {
        fprintf(stderr, "%s\n", error);
    }
    CHECK(disc != NULL);
    if (!disc) {
        return;
    }
    CHECK_EQ_INT(DiscTrackCount(disc), 3);
    CHECK_EQ_INT(DiscTrackInfo(disc, 1)->index0, 2);
    CHECK_EQ_INT(DiscTrackInfo(disc, 1)->index1, 3);
    CHECK_EQ_INT(DiscTrackInfo(disc, 1)->end, 6 + gap);
    CHECK_EQ_INT(DiscTrackInfo(disc, 2)->index1, 6 + gap);
    CHECK_EQ_INT(DiscLeadOut(disc), 8 + gap);
    CHECK(memcmp(DiscRawSector(disc, 0), raw, DISC_RAW_SECTOR) == 0);
    for (i = 0; i < 4; i++) {
        uint8_t want[DISC_RAW_SECTOR] = {0};
        size_t bytes = i < 3 ? DISC_RAW_SECTOR : 4;
        const uint8_t *got = DiscRawSector(disc, 2 + gap + i);

        memcpy(want, pcm + i * DISC_RAW_SECTOR, bytes);
        CHECK(got != NULL);
        if (got) {
            CHECK(memcmp(got, want, sizeof(want)) == 0);
        }
        DiscPrefetch(disc, 2 + gap + i, 8);
    }
    if (gap) {
        uint8_t silence[DISC_RAW_SECTOR] = {0};

        CHECK(memcmp(DiscRawSector(disc, 2), silence, sizeof(silence)) == 0);
    }
    CHECK(memcmp(DiscRawSector(disc, 6 + gap), raw, DISC_RAW_SECTOR) == 0);
    CHECK(DiscRawSector(disc, 8 + gap) == NULL);
    DiscClose(disc);
}

static void rejected(const char *message)
{
    char error[512] = "";
    Disc *disc = DiscOpen(cuePath, error, sizeof(error));

    CHECK(disc == NULL);
    CHECK(strstr(error, message) != NULL);
    DiscClose(disc);
}

int main(void)
{
    uint8_t raw[2 * DISC_RAW_SECTOR], wave[PCM_OFFSET + PCM_BYTES + 12], changed[sizeof(wave)];
    uint8_t *fmt = wave + 30;
    char path[256];
    size_t i;

    if (!mkdtemp(folder)) {
        perror("mkdtemp");
        return 1;
    }
    snprintf(cuePath, sizeof(cuePath), "%s/disc.cue", folder);
    for (i = 0; i < sizeof(raw); i++) {
        raw[i] = (uint8_t)(i * 17 + i / 256);
    }
    memset(wave, 0, sizeof(wave));
    memcpy(wave, "RIFF", 4);
    put32(wave + 4, sizeof(wave) - 8);
    memcpy(wave + 8, "WAVEJUNK", 8);
    put32(wave + 16, 1); /* an odd chunk, with its pad byte */
    wave[20] = 0xA5;
    memcpy(wave + 22, "fmt ", 4);
    put32(wave + 26, 16);
    fmt[0] = 1;
    fmt[2] = 2;
    put32(fmt + 4, 44100);
    put32(fmt + 8, 176400);
    fmt[12] = 4;
    fmt[14] = 16;
    memcpy(wave + 46, "data", 4);
    put32(wave + 50, PCM_BYTES);
    for (i = 0; i < PCM_BYTES; i++) {
        wave[PCM_OFFSET + i] = (uint8_t)(i * 31 + i / 256);
    }
    memcpy(wave + PCM_OFFSET + PCM_BYTES, "LIST\4\0\0\0INFO", 12);
    write_file("raw.bin", raw, sizeof(raw));
    write_file("music.wav", wave, sizeof(wave));
    write_cue("WAVE", "AUDIO", 0);
    compare_samples(wave + PCM_OFFSET, raw, 0);
    write_cue("BINARY", "AUDIO", 0);
    compare_samples(wave + PCM_OFFSET, raw, 0);
    write_cue("WAVE", "AUDIO", 1);
    compare_samples(wave + PCM_OFFSET, raw, 1);

    /* Wrong sample format, damaged chunks, missing chunks, and WAVE naming a raw file. */
    write_cue("WAVE", "AUDIO", 0);
    for (i = 0; i < 6; i++) {
        static const int fields[] = {0, 2, 4, 8, 12, 14};

        memcpy(changed, wave, sizeof(wave));
        changed[30 + fields[i]] ^= 1;
        write_file("music.wav", changed, sizeof(changed));
        rejected("16-bit stereo PCM at 44100 Hz");
    }
    for (i = 0; i < 6; i++) {
        memcpy(changed, wave, sizeof(wave));
        if (i == 0) {
            put32(changed + 4, sizeof(wave)); /* RIFF past the file */
        } else if (i == 1) {
            put32(changed + 50, sizeof(wave)); /* data past RIFF */
        } else if (i == 2) {
            memcpy(changed + 22, "JUNK", 4); /* no fmt */
        } else if (i == 3) {
            memcpy(changed + 46, "JUNK", 4); /* no data */
        } else if (i == 4) {
            put32(changed + 50, PCM_BYTES - 1); /* incomplete sample frame */
        } else {
            put32(changed + 16, 0xFFFFFFFFu); /* oversized odd chunk */
        }
        write_file("music.wav", changed, sizeof(changed));
        rejected("invalid or truncated WAV");
    }
    write_file("music.wav", raw, sizeof(raw));
    rejected("invalid or truncated WAV");
    write_file("music.wav", wave, sizeof(wave));
    write_cue("BINARY", "MODE2/2352", 0);
    rejected("WAV file used for a data track");
    write_cue("MP3", "AUDIO", 0);
    rejected("only BINARY and WAVE");

    remove(cuePath);
    snprintf(path, sizeof(path), "%s/raw.bin", folder);
    remove(path);
    snprintf(path, sizeof(path), "%s/music.wav", folder);
    remove(path);
    rmdir(folder);
    return test_report("test_disc");
}
