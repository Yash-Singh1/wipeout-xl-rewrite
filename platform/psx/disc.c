#include "disc.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#elif defined(__EMSCRIPTEN__)
#include <emscripten.h>
/* The web: the page has read the player's disc files into memory, once (platform/web/shell.html); a file is
   found there by its name, without a second copy. */
EM_JS(const uint8_t *, WebDiscFile, (const char *path, size_t *size), {
    var file = Module.discFiles && Module.discFiles[UTF8ToString(path).split('/').pop()];
    if (!file) return 0;
    HEAPU32[size >> 2] = file.size;
    return file.data;
});
#else
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

typedef struct DiscFile {
    const uint8_t *data;
    size_t size;
    size_t offset;   /* the PCM payload of a WAV (0 for a raw file); data and size keep the whole mapping */
    uint8_t *tail;   /* a WAV's last sector, padded with silence when its samples leave it short */
    int32_t start;   /* LBA of its first sector */
    int32_t sectors; /* in the file */
    int32_t length;  /* on the disc: its sectors and the gaps the cue sheet puts among them (PREGAP) */
    void *handle; /* platform mapping handle */
} DiscFile;

/* A PREGAP: sectors of silence on the disc that its image leaves out (a track's pregap the cue sheet names
   instead of the file holding it), in the file `file`. */
typedef struct DiscGap {
    int32_t start, sectors;
    int file;
} DiscGap;

/* A disc image with its data track alone gets the original's eleven music tracks, silent, of three minutes each. */
#define SILENT_TRACKS 11
#define SILENT_TRACK_SECTORS (3 * 60 * 75)

struct Disc {
    DiscFile files[DISC_MAX_TRACKS];
    int fileCount;
    DiscTrack tracks[DISC_MAX_TRACKS];
    int trackCount;
    DiscGap gaps[DISC_MAX_TRACKS];
    int gapCount;
    int32_t leadOut;
};

/* The sector of file `file` at lba, or -1 in a gap (or outside the file). */
static int32_t FileSector(const Disc *disc, int file, int32_t lba)
{
    const DiscFile *f = &disc->files[file];
    int32_t sector = lba - f->start;
    int i;

    if (sector < 0 || sector >= f->length) {
        return -1;
    }
    for (i = 0; i < disc->gapCount; i++) {
        const DiscGap *g = &disc->gaps[i];

        if (g->file == file && lba >= g->start) {
            if (lba < g->start + g->sectors) {
                return -1;
            }
            sector -= g->sectors;
        }
    }
    return sector < f->sectors ? sector : -1;
}

static void fail(char *error, int size, const char *format, const char *a)
{
    if (error && size > 0) {
        snprintf(error, (size_t)size, format, a);
    }
}

static uint16_t le16(const uint8_t *p)
{
    return (uint16_t)(p[0] | p[1] << 8);
}

static uint32_t le32(const uint8_t *p)
{
    return p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

/* A WAV's samples are already CD-DA: little-endian, signed 16-bit stereo at 44100 Hz. The cue's indexes count
   from its first sample, including any pregap it holds. RIFF chunks can include tags and padding; neither is
   part of the track. Some old rippers named these files BINARY in their .cue, so the bytes decide too. */
static bool wave_file(DiscFile *f, const char *path, char *error, int errorSize)
{
    size_t at = 12, end, bytes = 0;
    bool format = false;

    if (f->size < 12 || memcmp(f->data, "RIFF", 4) || memcmp(f->data + 8, "WAVE", 4) ||
        le32(f->data + 4) < 4 || le32(f->data + 4) > f->size - 8) {
        goto damaged;
    }
    end = (size_t)le32(f->data + 4) + 8;
    while (at < end) {
        const uint8_t *chunk = f->data + at;
        size_t length;

        if (end - at < 8) {
            goto damaged;
        }
        length = le32(chunk + 4);
        at += 8;
        if (length > end - at || (length & 1) > end - at - length) {
            goto damaged;
        }
        if (!memcmp(chunk, "fmt ", 4)) {
            const uint8_t *fmt = f->data + at;

            if (format || length < 16) {
                goto damaged;
            }
            if (le16(fmt) != 1 || le16(fmt + 2) != 2 || le32(fmt + 4) != 44100 ||
                le32(fmt + 8) != 176400 || le16(fmt + 12) != 4 || le16(fmt + 14) != 16) {
                fail(error, errorSize, "WAV must be 16-bit stereo PCM at 44100 Hz (%s)", path);
                return false;
            }
            format = true;
        } else if (!memcmp(chunk, "data", 4)) {
            if (f->offset || !length || length % 4) {
                goto damaged;
            }
            f->offset = at;
            bytes = length;
        }
        at += length + (length & 1);
    }
    if (!format || !f->offset) {
        goto damaged;
    }
    f->sectors = (int32_t)(bytes / DISC_RAW_SECTOR);
    if (bytes % DISC_RAW_SECTOR) {
        f->tail = calloc(1, DISC_RAW_SECTOR);
        if (!f->tail) {
            fail(error, errorSize, "cannot allocate WAV sector for %s", path);
            return false;
        }
        memcpy(f->tail, f->data + f->offset + bytes - bytes % DISC_RAW_SECTOR, bytes % DISC_RAW_SECTOR);
        f->sectors++;
    }
    return true;
damaged:
    fail(error, errorSize, "invalid or truncated WAV file %s", path);
    return false;
}

static bool map_file(DiscFile *f, const char *path)
{
#ifdef _WIN32
    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL, NULL);
    LARGE_INTEGER size;
    HANDLE mapping;

    if (file == INVALID_HANDLE_VALUE || !GetFileSizeEx(file, &size)) {
        return false;
    }
    mapping = CreateFileMappingA(file, NULL, PAGE_READONLY, 0, 0, NULL);
    CloseHandle(file);
    if (!mapping) {
        return false;
    }
    f->data = MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0);
    CloseHandle(mapping);
    f->size = (size_t)size.QuadPart;
    return f->data != NULL;
#elif defined(__EMSCRIPTEN__)
    f->data = WebDiscFile(path, &f->size);
    return f->data != NULL;
#else
    struct stat st;
    int fd = open(path, O_RDONLY);
    void *p;

    if (fd < 0) {
        return false;
    }
    if (fstat(fd, &st) != 0 || st.st_size <= 0) {
        close(fd);
        return false;
    }
    p = mmap(NULL, (size_t)st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    if (p == MAP_FAILED) {
        return false;
    }
    f->data = p;
    f->size = (size_t)st.st_size;
    return true;
#endif
}

static void unmap_file(DiscFile *f)
{
    if (!f->data) {
        return;
    }
#ifdef _WIN32
    UnmapViewOfFile(f->data);
#elif defined(__EMSCRIPTEN__)
    /* the page's memory, for as long as the page is open */
#else
    munmap((void *)f->data, f->size);
#endif
    f->data = NULL;
}

void DiscClose(Disc *disc)
{
    int i;

    if (!disc) {
        return;
    }
    for (i = 0; i < disc->fileCount; i++) {
        unmap_file(&disc->files[i]);
        free(disc->files[i].tail);
    }
    free(disc);
}

static bool parse_msf(const char *s, int32_t *out)
{
    int m, sec, f;

    if (sscanf(s, "%d:%d:%d", &m, &sec, &f) != 3 || m < 0 || sec < 0 || sec > 59 || f < 0 || f > 74) {
        return false;
    }
    *out = (m * 60 + sec) * 75 + f;
    return true;
}

/* Directory part of a path, with a trailing separator ("" if none). */
static void dir_of(const char *path, char *out, size_t size)
{
    const char *slash = strrchr(path, '/');
#ifdef _WIN32
    const char *back = strrchr(path, '\\');

    if (back && (!slash || back > slash)) {
        slash = back;
    }
#endif
    if (!slash) {
        out[0] = 0;
        return;
    }
    snprintf(out, size, "%.*s", (int)(slash - path + 1), path);
}

Disc *DiscOpen(const char *cuePath, char *error, int errorSize)
{
    Disc *disc = calloc(1, sizeof(*disc));
    FILE *cue = cuePath ? fopen(cuePath, "r") : NULL;
    char line[1024], dir[1024];
    int current = -1;   /* current track index (into disc->tracks) */
    int32_t index0[DISC_MAX_TRACKS], index1[DISC_MAX_TRACKS];
    int fileOf[DISC_MAX_TRACKS];
    int32_t pregap = 0; /* the current track's PREGAP, put in before its first INDEX */
    int i;

    if (!disc) {
        return NULL;
    }
    if (!cue) {
        fail(error, errorSize, "cannot open cue sheet %s", cuePath ? cuePath : "(none)");
        free(disc);
        return NULL;
    }
    dir_of(cuePath, dir, sizeof(dir));
    for (i = 0; i < DISC_MAX_TRACKS; i++) {
        index0[i] = index1[i] = -1;
    }
    while (fgets(line, sizeof(line), cue)) {
        char *p = line;

        while (isspace((unsigned char)*p)) {
            p++;
        }
        if (!strncmp(p, "FILE", 4)) {
            char *q1 = strchr(p, '"'), *q2 = q1 ? strchr(q1 + 1, '"') : NULL;
            char name[1024], path[2048], type[32];
            DiscFile *f;

            if (!q1 || !q2 || disc->fileCount >= DISC_MAX_TRACKS) {
                fail(error, errorSize, "unsupported FILE line in %s", cuePath);
                goto bad;
            }
            if (sscanf(q2 + 1, "%31s", type) != 1 || (strcmp(type, "BINARY") && strcmp(type, "WAVE"))) {
                fail(error, errorSize, "only BINARY and WAVE files are supported (%s)", cuePath);
                goto bad;
            }
            snprintf(name, sizeof(name), "%.*s", (int)(q2 - q1 - 1), q1 + 1);
            snprintf(path, sizeof(path), "%s%s", name[0] == '/' || name[0] == '\\' || name[1] == ':' ? "" : dir,
                     name);
            f = &disc->files[disc->fileCount];
            if (!map_file(f, path)) {
                fail(error, errorSize, "cannot read disc file %s", path);
                goto bad;
            }
            disc->fileCount++;
            f->sectors = (int32_t)(f->size / DISC_RAW_SECTOR);
            if ((!strcmp(type, "WAVE") || (f->size >= 12 && !memcmp(f->data, "RIFF", 4) &&
                                           !memcmp(f->data + 8, "WAVE", 4))) &&
                !wave_file(f, path, error, errorSize)) {
                goto bad;
            }
            f->length = f->sectors;
            f->start = disc->fileCount > 1 ? disc->files[disc->fileCount - 2].start +
                                               disc->files[disc->fileCount - 2].length
                                         : 0;
        } else if (!strncmp(p, "TRACK", 5)) {
            int number;
            char mode[32] = "";
            DiscTrack *t;

            if (sscanf(p + 5, "%d %31s", &number, mode) != 2 || disc->trackCount >= DISC_MAX_TRACKS ||
                !disc->fileCount) {
                fail(error, errorSize, "unsupported TRACK line in %s", cuePath);
                goto bad;
            }
            t = &disc->tracks[disc->trackCount];
            current = disc->trackCount++;
            t->number = number;
            t->audio = !strcmp(mode, "AUDIO");
            if (!t->audio && disc->files[disc->fileCount - 1].offset) {
                fail(error, errorSize, "WAV file used for a data track in %s", cuePath);
                goto bad;
            }
            if (t->audio) {
                t->data_offset = 0;
            } else if (!strcmp(mode, "MODE2/2352")) {
                t->data_offset = 24;
            } else if (!strcmp(mode, "MODE1/2352")) {
                t->data_offset = 16;
            } else {
                fail(error, errorSize, "unsupported track mode in %s (need 2352-byte sectors)", cuePath);
                goto bad;
            }
            fileOf[current] = disc->fileCount - 1;
            pregap = 0;
        } else if (!strncmp(p, "INDEX", 5)) {
            DiscFile *f = &disc->files[disc->fileCount - 1];
            int n;
            char msf[32];
            int32_t value;

            if (current < 0 || sscanf(p + 5, "%d %31s", &n, msf) != 2 || !parse_msf(msf, &value)) {
                fail(error, errorSize, "bad INDEX line in %s", cuePath);
                goto bad;
            }
            /* (the file's times leave out the gaps put in it before) */
            value += f->start + f->length - f->sectors;
            if (pregap) {
                /* the PREGAP comes first: the track's index 0, silence the file has not */
                DiscGap *g = &disc->gaps[disc->gapCount++];

                g->start = value;
                g->sectors = pregap;
                g->file = disc->fileCount - 1;
                f->length += pregap;
                index0[current] = value;
                value += pregap;
                pregap = 0;
            }
            if (n == 0) {
                index0[current] = value;
            } else if (n == 1) {
                index1[current] = value;
            }
        } else if (!strncmp(p, "PREGAP", 6)) {
            char msf[32];

            if (current < 0 || index0[current] >= 0 || index1[current] >= 0 || disc->gapCount >= DISC_MAX_TRACKS ||
                sscanf(p + 6, "%31s", msf) != 1 || !parse_msf(msf, &pregap)) {
                fail(error, errorSize, "bad PREGAP line in %s", cuePath);
                goto bad;
            }
        }
    }
    fclose(cue);
    cue = NULL;
    if (!disc->trackCount) {
        fail(error, errorSize, "no tracks in %s", cuePath);
        goto bad;
    }
    for (i = 0; i < disc->trackCount; i++) {
        DiscTrack *t = &disc->tracks[i];

        if (index1[i] < 0) {
            fail(error, errorSize, "track without INDEX 01 in %s", cuePath);
            goto bad;
        }
        t->index1 = index1[i];
        t->index0 = index0[i] >= 0 ? index0[i] : index1[i];
    }
    for (i = 0; i < disc->trackCount; i++) {
        DiscTrack *t = &disc->tracks[i];
        const DiscFile *last = &disc->files[disc->fileCount - 1];

        t->end = i + 1 < disc->trackCount ? disc->tracks[i + 1].index0 : last->start + last->length;
        if (t->end <= t->index1 || (i && t->index0 < disc->tracks[i - 1].index1)) {
            fail(error, errorSize, "inconsistent track layout in %s", cuePath);
            goto bad;
        }
    }
    disc->leadOut = disc->tracks[disc->trackCount - 1].end;
    if (disc->trackCount == 1 && !disc->tracks[0].audio) {
        /* its data track alone (many copies leave the music out): the music tracks are there, silent (the game
           counts and plays them as the disc's; with fewer it would divide by none, and play past the disc) */
        for (i = 1; i <= SILENT_TRACKS; i++) {
            DiscTrack *t = &disc->tracks[disc->trackCount++];

            t->number = i + 1;
            t->audio = true;
            t->data_offset = 0;
            t->index0 = disc->leadOut;
            t->index1 = disc->leadOut + 150;
            t->end = t->index1 + SILENT_TRACK_SECTORS;
            disc->leadOut = t->end;
        }
    }
    return disc;
bad:
    if (cue) {
        fclose(cue);
    }
    DiscClose(disc);
    return NULL;
}

int DiscTrackCount(const Disc *disc)
{
    return disc->trackCount;
}

const DiscTrack *DiscTrackInfo(const Disc *disc, int index)
{
    return index >= 0 && index < disc->trackCount ? &disc->tracks[index] : NULL;
}

int32_t DiscLeadOut(const Disc *disc)
{
    return disc->leadOut;
}

const uint8_t *DiscRawSector(const Disc *disc, int32_t lba)
{
    static const uint8_t silence[DISC_RAW_SECTOR];
    const DiscFile *last = &disc->files[disc->fileCount - 1];
    int i;

    if (lba < 0) {
        return NULL;
    }
    if (lba >= last->start + last->length && lba < disc->leadOut) {
        return silence; /* (the silent music tracks of a disc with its data track alone) */
    }
    for (i = disc->fileCount - 1; i >= 0; i--) {
        const DiscFile *f = &disc->files[i];

        if (lba >= f->start) {
            int32_t sector = FileSector(disc, i, lba);

            if (sector >= 0) {
                return f->tail && sector == f->sectors - 1 ? f->tail
                                                          : f->data + f->offset + (size_t)sector * DISC_RAW_SECTOR;
            }
            return lba < f->start + f->length ? silence : NULL; /* (a PREGAP the image leaves out) */
        }
    }
    return NULL;
}

void DiscPrefetch(const Disc *disc, int32_t lba, int sectors)
{
#if !defined(_WIN32) && !defined(__EMSCRIPTEN__) /* a mapped file (the web's is in memory already) */
    int i;

    for (i = disc->fileCount - 1; i >= 0; i--) {
        const DiscFile *f = &disc->files[i];

        if (lba >= f->start) {
            size_t first, length;
            long page = sysconf(_SC_PAGESIZE);
            int32_t sector = FileSector(disc, i, lba);

            if (sector < 0 || page <= 0) {
                return;
            }
            first = f->offset + (size_t)sector * DISC_RAW_SECTOR;
            length = (size_t)sectors * DISC_RAW_SECTOR;
            if (first + length > f->size) {
                length = f->size - first;
            }
            length += first % (size_t)page;
            first -= first % (size_t)page;
            madvise((void *)(f->data + first), length, MADV_WILLNEED);
            return;
        }
    }
#else
    (void)disc;
    (void)lba;
    (void)sectors;
#endif
}

const DiscTrack *DiscTrackAt(const Disc *disc, int32_t lba)
{
    int i;

    for (i = disc->trackCount - 1; i >= 0; i--) {
        if (lba >= disc->tracks[i].index0) {
            return lba < disc->tracks[i].end ? &disc->tracks[i] : NULL;
        }
    }
    return NULL;
}

bool DiscReadData(const Disc *disc, int32_t lba, int count, void *out)
{
    int i;

    for (i = 0; i < count; i++) {
        const uint8_t *raw = DiscRawSector(disc, lba + i);
        const DiscTrack *t = DiscTrackAt(disc, lba + i);

        if (!raw || !t) {
            return false;
        }
        memcpy((uint8_t *)out + (size_t)i * 2048, raw + t->data_offset, 2048);
    }
    return true;
}

/* ---- ISO 9660 ----------------------------------------------------------------------- */

static bool name_matches(const uint8_t *record, const char *component, size_t length)
{
    size_t n = record[32];
    const char *name = (const char *)record + 33;
    size_t i;

    if (n >= 2 && name[n - 2] == ';') {
        n -= 2;
    }
    if (n == 1 && length != 1) {
        return false; /* "." and ".." entries (0x00, 0x01) */
    }
    if (n != length) {
        return false;
    }
    for (i = 0; i < n; i++) {
        if (toupper((unsigned char)name[i]) != toupper((unsigned char)component[i])) {
            return false;
        }
    }
    return true;
}

bool DiscFindFile(const Disc *disc, const char *path, int32_t *lba, uint32_t *size)
{
    uint8_t sector[2048];
    int32_t dirLba;
    uint32_t dirSize;
    const char *p = path;

    if (!DiscReadData(disc, 16, 1, sector) || sector[0] != 1 || memcmp(sector + 1, "CD001", 5)) {
        return false;
    }
    dirLba = (int32_t)le32(sector + 156 + 2);
    dirSize = le32(sector + 156 + 10);
    for (;;) {
        const char *end;
        size_t length;
        bool last, found = false;
        uint32_t offset;

        while (*p == '\\' || *p == '/') {
            p++;
        }
        if (!*p) {
            return false;
        }
        end = p;
        while (*end && *end != '\\' && *end != '/') {
            end++;
        }
        length = (size_t)(end - p);
        {
            const char *semi = memchr(p, ';', length);

            if (semi) {
                length = (size_t)(semi - p);
            }
        }
        last = !*end;
        for (offset = 0; offset < dirSize && !found; offset += 2048) {
            uint32_t pos = 0;

            if (!DiscReadData(disc, dirLba + (int32_t)(offset / 2048), 1, sector)) {
                return false;
            }
            while (pos < 2048 && sector[pos]) {
                const uint8_t *rec = sector + pos;
                bool isDir = (rec[25] & 2) != 0;

                if (name_matches(rec, p, length) && isDir != last) {
                    if (last) {
                        *lba = (int32_t)le32(rec + 2);
                        *size = le32(rec + 10);
                        return true;
                    }
                    dirLba = (int32_t)le32(rec + 2);
                    dirSize = le32(rec + 10);
                    found = true;
                    break;
                }
                pos += rec[0];
            }
        }
        if (!found) {
            return false;
        }
        p = end;
    }
}
