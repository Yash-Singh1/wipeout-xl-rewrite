/* libcd as the game drives it: reading files the way iso9660.c does (seek with CdControlF,
   WaitEvent on the HwCD event, CdRead, spin on CdReadSync), byte-identical to the files
   extracted from the same disc; the command/response sequence of the music code (InitCD,
   CdMusicPlay, SetInGameCDTrack's GetlocP); and the drive's timing. */
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "../../platform/psx/internal.h"
#include "testenv.h"
#include "testutil.h"

typedef struct { uint8_t minute, second, sector, track; } Loc;
typedef struct { Loc pos; uint32_t size; char name[16]; } File;

int CdInit(void);
int CdSetDebug(int);
int CdControl(int, const uint8_t *, uint8_t *);
int CdControlF(int, const uint8_t *);
int CdControlB(int, const uint8_t *, uint8_t *);
int CdRead(int, void *, int);
int CdReadSync(int, uint8_t *);
int CdGetToc(Loc *);
int CdGetToc2(int, void *);
void CdIntToPos(int, Loc *);
int CdPosToInt(const Loc *);
File *CdSearchFile(File *, const char *);
int OpenEvent(uint32_t, int, int, void (*)(void));
int EnableEvent(int);
int WaitEvent(int);
int TestEvent(int);

static int errorsSeen;
static void on_error(void)
{
    errorsSeen++;
}

#define itob(i) (((i) / 10) * 16 + ((i) % 10))
#define btoi(b) ((((b) >> 4) * 10) + ((b) & 15))

static int track1Start;

static void logical_sector_to_loc(unsigned sector, Loc *loc)
{
    sector += (unsigned)track1Start;
    loc->track = 1;
    loc->minute = (uint8_t)itob(sector / 4500);
    loc->second = (uint8_t)itob((sector % 4500) / 75);
    loc->sector = (uint8_t)itob(sector % 75);
}

static int readEvent;
static int filesCompared;
static long bytesCompared;

/* The read loop of iso_read. */
static void game_read(unsigned lba, int count, void *buffer)
{
    Loc loc;
    int check = -1;

    logical_sector_to_loc(lba, &loc);
    while (check == -1) {
        CdControlF(0x15, (uint8_t *)&loc);
        WaitEvent(readEvent);
        CdRead(count, buffer, 0x80);
        while ((check = CdReadSync(1, 0)) > 0) {
        }
    }
}

static uint8_t *slurp(const char *path, long *size)
{
    FILE *f = fopen(path, "rb");
    uint8_t *data;

    if (!f) {
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    *size = ftell(f);
    fseek(f, 0, SEEK_SET);
    data = malloc((size_t)*size + 1);
    if (fread(data, 1, (size_t)*size, f) != (size_t)*size) {
        free(data);
        data = NULL;
    }
    fclose(f);
    return data;
}

/* Reads one file from the disc by its extracted copy's path relative to the root. */
static void compare_file(const char *root, const char *relative)
{
    char host[2048], disc[512];
    long size;
    uint8_t *want, *got;
    File file;
    int sectors, i;
    uint8_t *tail;
    struct stat st;

    snprintf(host, sizeof(host), "%s/%s", root, relative);
    if (stat(host, &st) != 0 || st.st_size == 0) {
        return; /* placeholders in the extracted tree */
    }
    want = slurp(host, &size);
    if (!want) {
        fprintf(stderr, "missing %s\n", host);
        CHECK(want != NULL);
        return;
    }
    snprintf(disc, sizeof(disc), "\\%s%s;1", relative, strchr(strrchr(relative, '/') ? strrchr(relative, '/') : relative, '.') ? "" : ".");
    for (i = 0; disc[i]; i++) {
        if (disc[i] == '/') {
            disc[i] = '\\';
        }
    }
    CHECK(CdSearchFile(&file, disc) != NULL);
    CHECK_EQ_INT(file.size, size);
    if (file.size != (uint32_t)size) {
        fprintf(stderr, "size differs: %s\n", relative);
    }
    sectors = (int)((size + 2047) / 2048);
    got = malloc((size_t)sectors * 2048);
    tail = got;
    /* the game reads whole sectors, then the last partial one separately */
    game_read((unsigned)CdPosToInt(&file.pos), sectors, got);
    if (memcmp(got, want, (size_t)size) != 0) {
        long k = 0;

        while (got[k] == want[k]) {
            k++;
        }
        fprintf(stderr, "contents differ: %s (first at byte %ld, file at sector %d)\n", relative, k,
                CdPosToInt(&file.pos));
        test_failures++;
    }
    test_checks++;
    (void)tail;
    filesCompared++;
    bytesCompared += size;
    free(got);
    free(want);
}

static void walk(const char *root, const char *relative)
{
    char path[2048];
    DIR *dir;
    struct dirent *entry;

    snprintf(path, sizeof(path), "%s/%s", root, relative);
    dir = opendir(path);
    if (!dir) {
        return;
    }
    while ((entry = readdir(dir)) != NULL) {
        char child[2048];
        struct stat st;

        if (entry->d_name[0] == '.') {
            continue;
        }
        snprintf(child, sizeof(child), "%s%s%s", relative, relative[0] ? "/" : "", entry->d_name);
        snprintf(path, sizeof(path), "%s/%s", root, child);
        if (stat(path, &st) != 0) {
            continue;
        }
        if (S_ISDIR(st.st_mode)) {
            walk(root, child);
        } else {
            compare_file(root, child);
        }
    }
    closedir(dir);
}

int main(int argc, char **argv)
{
    const char *root = test_root_dir();
    Loc toc[100];
    int errorEvent, count, i;
    uint8_t result[16];
    uint8_t sector[2048];

    if (!test_io_start(32.0, NULL) || !root) {
        printf("test_cd: disc or extracted files not found, skipped\n");
        return 0;
    }
    /* InitCDFS: CdInit first, then iso_init opens the events */
    CHECK_EQ_INT(CdInit(), 1);
    CdSetDebug(0);
    readEvent = OpenEvent(0xF0000003u, 0x20, 0x2000, 0);
    errorEvent = OpenEvent(0xF0000003u, 0x8000, 0x1000, on_error);
    EnableEvent(readEvent);
    EnableEvent(errorEvent);

    /* the table of contents: track 1 is at 00:02:00 (what iso_init reads from bytes 5 and 6) */
    {
        uint8_t raw[100 * 4];

        CdGetToc2(1, raw);
        track1Start = btoi(raw[5]) * 75 + btoi(raw[6]);
        CHECK_EQ_INT(track1Start, 150);
    }
    count = CdGetToc(toc);
    CHECK_EQ_INT(count, 12);

    /* the primary volume descriptor */
    game_read(16, 1, sector);
    CHECK_EQ_INT(sector[0], 1);
    CHECK(!memcmp(sector + 1, "CD001", 5));

    /* every non-empty file of the extracted tree, read as the game reads files */
    if (argc > 1) {
        for (i = 1; i < argc; i++) {
            compare_file(root, argv[i]);
        }
    } else {
        walk(root, "");
    }
    printf("compared %d files, %ld bytes\n", filesCompared, bytesCompared);
    CHECK(argc > 1 || filesCompared > 400);
    CHECK_EQ_INT(errorsSeen, 0);
    CHECK_EQ_INT(TestEvent(readEvent), 0);

    /* A completion event left over from an earlier command (the drive's re-initialisation here)
       must not make the next read start early or wrong. */
    CdInit();
    compare_file(root, "SYSTEM.CNF");
    compare_file(root, "SCUS_943.51");

    /* commands and their responses */
    CHECK_EQ_INT(CdControlB(0x13, NULL, result), 1); /* GetTN */
    CHECK_EQ_INT(result[1], 0x01);
    CHECK_EQ_INT(result[2], 0x12);
    {
        uint8_t track = 0x02;

        CHECK_EQ_INT(CdControlB(0x14, &track, result), 1); /* GetTD 2 */
        CHECK_EQ_INT(result[1], 0x08);
        CHECK_EQ_INT(result[2], 0x11);
    }
    {
        uint8_t bad = 0x99;

        CHECK_EQ_INT(CdControlB(0x14, &bad, result), 0); /* no such track: refused */
        CHECK(result[0] & 1);
    }

    /* timing: a far seek and a 100-sector read at double speed (here on a drive 32 times faster
       than the console's, so divide) */
    {
        static uint8_t big[100 * 2048];
        uint64_t before = PsxIoNow(), after;
        double seconds;

        game_read(30000, 100, big);
        after = PsxIoNow();
        seconds = (double)(after - before) / 44100.0 * 32.0;
        printf("far seek + 100 sectors: %.2f s of console time\n", seconds);
        CHECK(seconds > 100 / 150.0);       /* at least the sector time */
        CHECK(seconds < 100 / 150.0 + 1.0); /* plus a seek of under a second */
    }
    test_io_stop();
    return test_report("cd");
}
