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
    size_t offset;   /* where the sectors start: the PCM payload of a WAV, a track in an .mdf (0 for a raw file);
                        data and size keep the whole mapping */
    int stride;      /* bytes from one sector to the next: 2352, 2048 (user data alone: cooked), or 2448 (with the
                        subchannel) */
    bool cooked;     /* its sectors are 2048 bytes of user data, to be given a sync and header */
    uint8_t *tail;   /* a WAV's last sector, padded with silence when its samples leave it short */
    int32_t start;   /* LBA of its first sector */
    int32_t sectors; /* in the file */
    int32_t length;  /* on the disc: its sectors and the gaps the cue sheet puts among them (PREGAP) */
    void *handle; /* platform mapping handle */
} DiscFile;

/* The raw sector made for a cooked one is in the caller's thread's own buffer (the music is read by another
   thread). */
#if defined(_MSC_VER) && !defined(__clang__)
#define THREAD_LOCAL __declspec(thread)
#else
#define THREAD_LOCAL _Thread_local
#endif

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

/* The 12 bytes a raw sector starts with. */
static const uint8_t raw_sync[12] = {0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0};

/* The tracks' ends and the lead-out once the tracks' starts (index0 and index1, -1 for none) are known, and the
   raw data tracks' modes as their sectors give them; a data track alone gets the silent music tracks. */
static bool finish_layout(Disc *disc, const int32_t *index0, const int32_t *index1, const char *name, char *error,
                          int errorSize)
{
    int i;

    if (!disc->trackCount) {
        fail(error, errorSize, "no tracks in %s", name);
        return false;
    }
    for (i = 0; i < disc->trackCount; i++) {
        DiscTrack *t = &disc->tracks[i];

        if (index1[i] < 0) {
            fail(error, errorSize, "track without INDEX 01 in %s", name);
            return false;
        }
        t->index1 = index1[i];
        t->index0 = index0[i] >= 0 ? index0[i] : index1[i];
    }
    for (i = 0; i < disc->trackCount; i++) {
        DiscTrack *t = &disc->tracks[i];
        const DiscFile *last = &disc->files[disc->fileCount - 1];

        t->end = i + 1 < disc->trackCount ? disc->tracks[i + 1].index0 : last->start + last->length;
        if (t->end <= t->index1 || (i && t->index0 < disc->tracks[i - 1].index1)) {
            fail(error, errorSize, "inconsistent track layout in %s", name);
            return false;
        }
    }
    disc->leadOut = disc->tracks[disc->trackCount - 1].end;
    for (i = 0; i < disc->trackCount; i++) {
        /* the mode in the header of the track's first sector, whatever the cue sheet says: some say MODE1/2352 of
           the console's mode 2 discs, which emulators read all the same (the user data 8 bytes later) */
        DiscTrack *t = &disc->tracks[i];
        const uint8_t *raw = !t->audio && t->sector_size == DISC_RAW_SECTOR ? DiscRawSector(disc, t->index1) : NULL;

        if (raw && !memcmp(raw, raw_sync, sizeof(raw_sync)) && (raw[15] == 1 || raw[15] == 2)) {
            t->data_offset = raw[15] == 2 ? 24 : 16;
        }
    }
    if (disc->trackCount == 1 && !disc->tracks[0].audio) {
        /* its data track alone (many copies leave the music out): the music tracks are there, silent (the game
           counts and plays them as the disc's; with fewer it would divide by none, and play past the disc) */
        for (i = 1; i <= SILENT_TRACKS; i++) {
            DiscTrack *t = &disc->tracks[disc->trackCount++];

            t->number = i + 1;
            t->audio = true;
            t->data_offset = 0;
            t->sector_size = DISC_RAW_SECTOR;
            t->index0 = disc->leadOut;
            t->index1 = disc->leadOut + 150;
            t->end = t->index1 + SILENT_TRACK_SECTORS;
            disc->leadOut = t->end;
        }
    }
    return true;
}

static bool has_extension(const char *path, const char *extension)
{
    size_t length = strlen(path), n = strlen(extension);
    size_t i;

    if (length <= n || path[length - n - 1] != '.') {
        return false;
    }
    for (i = 0; i < n; i++) {
        if (tolower((unsigned char)path[length - n + i]) != extension[i]) {
            return false;
        }
    }
    return true;
}

/* Maps the file beside path that has the same name with another extension (written in lower case here): as it is
   written lower case, else upper case, as an image's descriptor and data are often named. */
static bool map_beside(DiscFile *f, const char *path, const char *extension)
{
    char name[2048], *dot;
    int upper, i;

    snprintf(name, sizeof(name), "%s", path);
    dot = strrchr(name, '.');
    if (!dot || strlen(extension) + 2 > sizeof(name) - (size_t)(dot - name)) {
        return false;
    }
    for (upper = 0; upper < 2; upper++) {
        for (i = 0; extension[i]; i++) {
            dot[1 + i] = (char)(upper ? toupper((unsigned char)extension[i]) : extension[i]);
        }
        dot[1 + i] = 0;
        if (map_file(f, name)) {
            return true;
        }
    }
    return false;
}

/* Sets up a file whose sectors are all one kind, from its first byte: raw (2352 bytes) or cooked (2048). */
static void set_sectors(DiscFile *f, int stride, size_t offset, int32_t start)
{
    f->stride = stride;
    f->cooked = stride == 2048;
    f->offset = offset;
    f->start = start;
    f->sectors = f->size > offset ? (int32_t)((f->size - offset) / (size_t)stride) : 0;
    f->length = f->sectors;
}

/* Whether a volume descriptor (ISO 9660: type 1, "CD001") is at data. */
static bool iso_volume(const uint8_t *data)
{
    return data[0] == 1 && !memcmp(data + 1, "CD001", 5);
}

/* The data track by itself: an .iso (2048-byte sectors), or a .bin or .img of raw sectors, as the volume descriptor
   in sector 16 says it is kept. */
static Disc *open_bare(const char *path, char *error, int errorSize)
{
    Disc *disc = calloc(1, sizeof(*disc));
    DiscFile *f;
    DiscTrack *t;
    int32_t index = 0;
    const uint8_t *raw;
    int stride, mode = 1;

    if (!disc) {
        return NULL;
    }
    f = &disc->files[0];
    if (!map_file(f, path)) {
        fail(error, errorSize, "cannot read disc file %s", path);
        free(disc);
        return NULL;
    }
    disc->fileCount = 1;
    raw = f->data + 16 * (size_t)DISC_RAW_SECTOR;
    if (f->size > 17 * (size_t)DISC_RAW_SECTOR && !memcmp(raw, raw_sync, sizeof(raw_sync)) &&
        (raw[15] == 1 || raw[15] == 2) && iso_volume(raw + (raw[15] == 2 ? 24 : 16))) {
        stride = DISC_RAW_SECTOR;
        mode = raw[15];
    } else if (f->size > 17 * 2048 && iso_volume(f->data + 16 * 2048)) {
        stride = 2048;
    } else {
        fail(error, errorSize, "%s is not a data track (it has no ISO 9660 volume)", path);
        goto bad;
    }
    set_sectors(f, stride, 0, 0);
    t = &disc->tracks[disc->trackCount++];
    t->number = 1;
    t->data_offset = mode == 2 ? 24 : 16;
    t->sector_size = stride;
    if (!finish_layout(disc, &index, &index, path, error, errorSize)) {
        goto bad;
    }
    return disc;
bad:
    DiscClose(disc);
    return NULL;
}

/* A CloneCD .ccd, with the .img beside it: raw sectors of every track in one file, from the disc's first. Its tracks
   are the [TRACK n] sections, MODE= 0 for audio, 1 or 2 for data, and INDEX 0= and INDEX 1= sectors. */
static Disc *open_ccd(const char *path, char *error, int errorSize)
{
    Disc *disc = calloc(1, sizeof(*disc));
    FILE *ccd = path ? fopen(path, "r") : NULL;
    char line[256];
    int32_t index0[DISC_MAX_TRACKS], index1[DISC_MAX_TRACKS];
    int mode[DISC_MAX_TRACKS];
    int current = -1, i;

    if (!disc) {
        if (ccd) {
            fclose(ccd);
        }
        return NULL;
    }
    if (!ccd) {
        fail(error, errorSize, "cannot open CloneCD descriptor %s", path ? path : "(none)");
        free(disc);
        return NULL;
    }
    while (fgets(line, sizeof(line), ccd)) {
        const char *p = line;
        int number, value;

        while (isspace((unsigned char)*p)) {
            p++;
        }
        if (*p == '[') {
            current = -1;
            if (sscanf(p, "[TRACK %d]", &number) == 1) {
                if (disc->trackCount >= DISC_MAX_TRACKS) {
                    fail(error, errorSize, "too many tracks in %s", path);
                    goto bad;
                }
                current = disc->trackCount++;
                disc->tracks[current].number = number;
                index0[current] = index1[current] = -1;
                mode[current] = -1;
            }
        } else if (current >= 0) {
            if (sscanf(p, "MODE=%d", &value) == 1) {
                mode[current] = value;
            } else if (sscanf(p, "INDEX 0=%d", &value) == 1) {
                index0[current] = value;
            } else if (sscanf(p, "INDEX 1=%d", &value) == 1) {
                index1[current] = value;
            }
        }
    }
    fclose(ccd);
    ccd = NULL;
    for (i = 0; i < disc->trackCount; i++) {
        DiscTrack *t = &disc->tracks[i];

        if (mode[i] < 0 || mode[i] > 2) {
            fail(error, errorSize, "a track has no usable MODE in %s", path);
            goto bad;
        }
        t->audio = mode[i] == 0;
        t->data_offset = mode[i] == 2 ? 24 : mode[i] == 1 ? 16 : 0;
        t->sector_size = DISC_RAW_SECTOR;
    }
    if (!map_beside(&disc->files[0], path, "img")) {
        fail(error, errorSize, "cannot read the .img beside %s", path);
        goto bad;
    }
    disc->fileCount = 1;
    set_sectors(&disc->files[0], DISC_RAW_SECTOR, 0, 0);
    if (!finish_layout(disc, index0, index1, path, error, errorSize)) {
        goto bad;
    }
    return disc;
bad:
    if (ccd) {
        fclose(ccd);
    }
    DiscClose(disc);
    return NULL;
}

static uint64_t le64(const uint8_t *p)
{
    return le32(p) | (uint64_t)le32(p + 4) << 32;
}

/* The whole of a small file (an .mds), or NULL. */
static uint8_t *read_whole(const char *path, size_t *size)
{
    FILE *f = fopen(path, "rb");
    uint8_t *data = NULL;
    long length;

    if (f && !fseek(f, 0, SEEK_END) && (length = ftell(f)) > 0 && length < (4 << 20) && !fseek(f, 0, SEEK_SET) &&
        (data = malloc((size_t)length)) != NULL && fread(data, 1, (size_t)length, f) == (size_t)length) {
        *size = (size_t)length;
    } else {
        free(data);
        data = NULL;
    }
    if (f) {
        fclose(f);
    }
    return data;
}

/* An Alcohol 120% .mds, with the .mdf beside it: the first (only) session's track blocks say each track's mode, sector
   size (2352, 2448 with subchannel, or 2048), first sector (its INDEX 01) and where it starts in the .mdf. A track
   runs to the next one's start, so a pregap the image holds is the previous track's tail. */
static Disc *open_mds(const char *path, char *error, int errorSize)
{
    Disc *disc = calloc(1, sizeof(*disc));
    size_t size = 0, k;
    uint8_t *mds = disc ? read_whole(path, &size) : NULL;
    const uint8_t *session;
    int32_t index0[DISC_MAX_TRACKS], index1[DISC_MAX_TRACKS];
    int32_t length[DISC_MAX_TRACKS]; /* a track's own count of sectors (0: not said) */
    int32_t pregap[DISC_MAX_TRACKS]; /* its pregap's sectors before index1 (0: none or not said) */
    uint64_t offset[DISC_MAX_TRACKS];
    int stride[DISC_MAX_TRACKS];
    size_t blocks, tracksAt;
    int i;

    if (!disc) {
        return NULL;
    }
    if (!mds) {
        fail(error, errorSize, "cannot read Alcohol descriptor %s", path);
        goto bad;
    }
    if (size < 88 || memcmp(mds, "MEDIA DESCRIPTOR", 16) || le32(mds + 80) > size - 24) {
        fail(error, errorSize, "%s is not an Alcohol 120%% descriptor", path);
        goto bad;
    }
    if (le16(mds + 18) >= 0x10 || le16(mds + 20) != 1) {
        fail(error, errorSize, "only single-session CD images are supported (%s)", path);
        goto bad;
    }
    session = mds + le32(mds + 80);
    blocks = session[10];
    tracksAt = le32(session + 20);
    if (tracksAt > size || blocks * 80 > size - tracksAt) {
        fail(error, errorSize, "damaged Alcohol descriptor %s", path);
        goto bad;
    }
    for (k = 0; k < blocks; k++) {
        const uint8_t *block = mds + tracksAt + k * 80;
        DiscTrack *t;
        uint32_t extra = le32(block + 12);
        int sectorBytes = le16(block + 16), n;

        if (block[4] < 1 || block[4] > 99) {
            continue; /* (the session's first, last and lead-out entries) */
        }
        if (disc->trackCount >= DISC_MAX_TRACKS) {
            fail(error, errorSize, "too many tracks in %s", path);
            goto bad;
        }
        n = disc->trackCount++;
        t = &disc->tracks[n];
        t->number = block[4];
        t->audio = block[0] == 0xA9;
        if (!t->audio && (block[0] < 0xAA || block[0] > 0xAD)) {
            fail(error, errorSize, "unsupported track mode in %s", path);
            goto bad;
        }
        if ((sectorBytes != 2352 && sectorBytes != 2448 && (sectorBytes != 2048 || t->audio))) {
            fail(error, errorSize, "unsupported sector size in %s", path);
            goto bad;
        }
        stride[n] = sectorBytes;
        t->sector_size = sectorBytes == 2048 ? 2048 : DISC_RAW_SECTOR;
        t->data_offset = t->audio ? 0 : sectorBytes == 2048 || block[0] == 0xAA ? 16 : 24;
        index0[n] = -1;
        index1[n] = (int32_t)le32(block + 36);
        offset[n] = le64(block + 40);
        pregap[n] = extra && extra <= size - 8 ? (int32_t)le32(mds + extra) : 0;
        length[n] = extra && extra <= size - 8 ? (int32_t)le32(mds + extra + 4) : 0;
    }
    for (i = 0; i < disc->trackCount; i++) {
        DiscFile *f = &disc->files[i];
        int32_t span = i + 1 < disc->trackCount ? index1[i + 1] - index1[i] : length[i];

        if (!map_beside(f, path, "mdf")) {
            fail(error, errorSize, "cannot read the .mdf beside %s", path);
            goto bad;
        }
        disc->fileCount++;
        if (index1[i] < 0 || offset[i] > f->size) {
            fail(error, errorSize, "damaged track in %s", path);
            goto bad;
        }
        set_sectors(f, stride[i], (size_t)offset[i], index1[i]);
        if (length[i] > 0 && length[i] < f->sectors) {
            f->sectors = length[i]; /* (what follows is the next track's pregap) */
        }
        if (span > 0 && span < f->sectors) {
            f->sectors = span;
        }
        f->length = span > 0 ? span : f->sectors;
        /* a pregap the descriptor names, in the room the previous track leaves */
        index0[i] = pregap[i] > 0 && i && index1[i] - pregap[i] >= index1[i - 1] + disc->files[i - 1].sectors
                        ? index1[i] - pregap[i]
                        : -1;
    }
    free(mds);
    if (!finish_layout(disc, index0, index1, path, error, errorSize)) {
        DiscClose(disc);
        return NULL;
    }
    return disc;
bad:
    free(mds);
    DiscClose(disc);
    return NULL;
}

static Disc *open_cue(const char *cuePath, char *error, int errorSize)
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
            f->stride = 0; /* (the first TRACK in the file says) */
            if ((!strcmp(type, "WAVE") || (f->size >= 12 && !memcmp(f->data, "RIFF", 4) &&
                                           !memcmp(f->data + 8, "WAVE", 4))) &&
                !wave_file(f, path, error, errorSize)) {
                goto bad;
            }
            if (f->offset) {
                f->stride = DISC_RAW_SECTOR;
            }
            f->length = f->sectors;
            f->start = disc->fileCount > 1 ? disc->files[disc->fileCount - 2].start +
                                               disc->files[disc->fileCount - 2].length
                                         : 0;
        } else if (!strncmp(p, "TRACK", 5)) {
            int number;
            char mode[32] = "";
            DiscTrack *t;
            DiscFile *f;
            int stride;

            if (sscanf(p + 5, "%d %31s", &number, mode) != 2 || disc->trackCount >= DISC_MAX_TRACKS ||
                !disc->fileCount) {
                fail(error, errorSize, "unsupported TRACK line in %s", cuePath);
                goto bad;
            }
            t = &disc->tracks[disc->trackCount];
            current = disc->trackCount++;
            t->number = number;
            t->audio = !strcmp(mode, "AUDIO");
            f = &disc->files[disc->fileCount - 1];
            if (!t->audio && f->offset) {
                fail(error, errorSize, "WAV file used for a data track in %s", cuePath);
                goto bad;
            }
            t->sector_size = DISC_RAW_SECTOR;
            if (t->audio) {
                t->data_offset = 0;
            } else if (!strcmp(mode, "MODE2/2352")) {
                t->data_offset = 24;
            } else if (!strcmp(mode, "MODE1/2352")) {
                t->data_offset = 16;
            } else if (!strcmp(mode, "MODE1/2048")) {
                t->data_offset = 16; /* (in the raw sector made up for it) */
                t->sector_size = 2048;
            } else {
                fail(error, errorSize, "unsupported track mode in %s (need MODE1/2048, MODE1/2352 or MODE2/2352)",
                     cuePath);
                goto bad;
            }
            stride = t->sector_size;
            if (f->stride && f->stride != stride) {
                fail(error, errorSize, "sector sizes mixed in one file in %s", cuePath);
                goto bad;
            }
            if (!f->stride) {
                /* the file's first track decides how its sectors are counted */
                f->stride = stride;
                f->cooked = stride == 2048;
                f->sectors = (int32_t)(f->size / (size_t)stride);
                f->length = f->sectors;
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
    for (i = 0; i < disc->fileCount; i++) {
        if (!disc->files[i].stride) {
            disc->files[i].stride = DISC_RAW_SECTOR; /* (a file no track is in) */
        }
    }
    if (!finish_layout(disc, index0, index1, cuePath, error, errorSize)) {
        goto bad;
    }
    return disc;
bad:
    if (cue) {
        fclose(cue);
    }
    DiscClose(disc);
    return NULL;
}

Disc *DiscOpen(const char *path, char *error, int errorSize)
{
    if (path && has_extension(path, "ccd")) {
        return open_ccd(path, error, errorSize);
    }
    if (path && has_extension(path, "mds")) {
        return open_mds(path, error, errorSize);
    }
    if (path && (has_extension(path, "iso") || has_extension(path, "bin") || has_extension(path, "img"))) {
        return open_bare(path, error, errorSize);
    }
    return open_cue(path, error, errorSize);
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

/* A cooked sector as the drive gives it: the sync, the header (the sector's minute, second and frame address, in
   BCD, and mode 1) and the 2048 bytes of data; the error detection and correction codes are left zero. */
void DiscMakeRawSector(uint8_t *raw, int32_t lba, const uint8_t *data)
{
    int32_t address = lba + 150;

    memset(raw, 0xFF, 12);
    raw[0] = raw[11] = 0;
    raw[12] = (uint8_t)(address / 4500 / 10 << 4 | address / 4500 % 10);
    raw[13] = (uint8_t)(address / 75 % 60 / 10 << 4 | address / 75 % 60 % 10);
    raw[14] = (uint8_t)(address % 75 / 10 << 4 | address % 75 % 10);
    raw[15] = 1;
    memcpy(raw + 16, data, 2048);
    memset(raw + 2064, 0, DISC_RAW_SECTOR - 2064);
}

const uint8_t *DiscRawSector(const Disc *disc, int32_t lba)
{
    static const uint8_t silence[DISC_RAW_SECTOR];
    static THREAD_LOCAL uint8_t made[DISC_RAW_SECTOR];
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
                const uint8_t *data = f->data + f->offset + (size_t)sector * (size_t)f->stride;

                if (f->tail && sector == f->sectors - 1) {
                    return f->tail;
                }
                if (f->cooked) {
                    DiscMakeRawSector(made, lba, data);
                    return made;
                }
                return data;
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
            first = f->offset + (size_t)sector * (size_t)f->stride;
            length = (size_t)sectors * (size_t)f->stride;
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

/* The file (or, with wantDirectory, the folder) at path. */
static bool find_entry(const Disc *disc, const char *path, bool wantDirectory, int32_t *lba, uint32_t *size)
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

                if (name_matches(rec, p, length) && isDir == (last ? wantDirectory : true)) {
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

bool DiscFindFile(const Disc *disc, const char *path, int32_t *lba, uint32_t *size)
{
    return find_entry(disc, path, false, lba, size);
}

bool DiscIsPcGame(const Disc *disc)
{
    int32_t lba;
    uint32_t size;

    return find_entry(disc, "\\COMMON", true, &lba, &size) && find_entry(disc, "\\TRACK01", true, &lba, &size) &&
           find_entry(disc, "\\COMMON\\VECTO.PRM", false, &lba, &size);
}
