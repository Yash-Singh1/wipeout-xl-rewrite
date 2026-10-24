/* Disc images as they are kept. A .cue sheet with its .bin tracks is used as it is (psx/disc.c). A disc's .bin (or
   .img) without its .cue is given one, made in the user's data folder (discs/<its name>/): its data track, and its
   audio tracks when they are beside it as "... (Track N).bin" (Redump's names, each starting with its 2-second
   pregap). An archive holding a disc image, known by its first bytes (else its name), is extracted once into
   discs/<the archive's name>/, with a note of the archive's size and date and where the image's .cue is; while the
   note matches, the extracted image is used as it is. The image in it is found by what it is, not by its name or
   any checksum: a .cue whose first track is a PlayStation disc's (raw 2352-byte sectors, an ISO 9660 volume of
   the system "PLAYSTATION"), else such a .bin or .img, given a .cue as above. Which game it is, is for the caller.
   A .7z is read with the LZMA SDK's decoder (platform/third_party/lzma, public domain; a solid block is unpacked in
   memory, one at a time), the others with libarchive (zip, rar, tar, gzip, bzip2, xz, zstd; where the program was
   built with it). */
#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

#include "discimage.h"
#include "psx/disc.h"

#include "third_party/lzma/7z.h"
#include "third_party/lzma/7zAlloc.h"
#include "third_party/lzma/7zCrc.h"
#include "third_party/lzma/7zFile.h"

#ifdef PORT_HAVE_LIBARCHIVE
#include <archive.h>
#include <archive_entry.h>
#endif

static SDL_AtomicInt progress; /* the extraction's, in thousandths */

int PortArchiveProgress(void)
{
    return SDL_GetAtomicInt(&progress);
}

/* ---- what a file is ---- */

static int HasExtension(const char *name, const char *extension)
{
    size_t length = strlen(name), n = strlen(extension);

    return length > n && name[length - n - 1] == '.' && !SDL_strcasecmp(name + length - n, extension);
}

static const char *const archiveExtensions[] = {"7z", "zip", "rar", "tar", "gz", "tgz", "bz2", "tbz2", "xz", "txz",
                                                "zst", "tzst"};

static int IsArchiveName(const char *name)
{
    size_t i;

    for (i = 0; i < SDL_arraysize(archiveExtensions); i++) {
        if (HasExtension(name, archiveExtensions[i])) {
            return 1;
        }
    }
    return 0;
}

int PortIsDiscName(const char *name)
{
    return HasExtension(name, "cue") || HasExtension(name, "bin") || HasExtension(name, "img") || IsArchiveName(name);
}

enum { NOT_ARCHIVE, ARCHIVE_7Z, ARCHIVE_OTHER };

/* What the file's first bytes say it is; a file that cannot be read goes by its name. */
static int ArchiveKind(const char *path)
{
    static const struct {
        int at, length;
        const char *bytes;
        int kind;
    } magic[] = {
        {0, 6, "7z\xBC\xAF\x27\x1C", ARCHIVE_7Z},  {0, 4, "PK\x03\x04", ARCHIVE_OTHER},
        {0, 4, "PK\x05\x06", ARCHIVE_OTHER},       {0, 6, "Rar!\x1A\x07", ARCHIVE_OTHER},
        {0, 2, "\x1F\x8B", ARCHIVE_OTHER},         {0, 3, "BZh", ARCHIVE_OTHER},
        {0, 6, "\xFD" "7zXZ\0", ARCHIVE_OTHER},    {0, 4, "\x28\xB5\x2F\xFD", ARCHIVE_OTHER},
        {257, 5, "ustar", ARCHIVE_OTHER},
    };
    unsigned char head[264];
    size_t got = 0, i;
    FILE *f = path ? fopen(path, "rb") : NULL;

    if (!f) {
        return path && IsArchiveName(path) ? (HasExtension(path, "7z") ? ARCHIVE_7Z : ARCHIVE_OTHER) : NOT_ARCHIVE;
    }
    got = fread(head, 1, sizeof(head), f);
    fclose(f);
    for (i = 0; i < SDL_arraysize(magic); i++) {
        if (got >= (size_t)(magic[i].at + magic[i].length) &&
            !memcmp(head + magic[i].at, magic[i].bytes, (size_t)magic[i].length)) {
            return magic[i].kind;
        }
    }
    return NOT_ARCHIVE;
}

int PortIsArchive(const char *path)
{
    return ArchiveKind(path) != NOT_ARCHIVE;
}

/* Whether the 2352-byte sector holds a PlayStation disc's volume descriptor (sector 16 of its data track), or a
   Sega Saturn disc's (saturn.c); its mode (1 or 2) into *mode. */
static int PlayStationVolume(const unsigned char *sector, int *mode)
{
    static const unsigned char sync[12] = {0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0};
    const unsigned char *data;

    if (memcmp(sector, sync, sizeof(sync)) || (sector[15] != 1 && sector[15] != 2)) {
        return 0;
    }
    *mode = sector[15];
    data = sector + (*mode == 2 ? 24 : 16);
    return data[0] == 1 && !memcmp(data + 1, "CD001", 5) &&
           (!memcmp(data + 8, "PLAYSTATION", 11) || !memcmp(data + 8, "SEGA SEGASATURN", 15));
}

/* Whether a .bin or .img is a PlayStation disc's data track; its mode into *mode. */
static int PlayStationImage(const char *path, int *mode)
{
    unsigned char sector[DISC_RAW_SECTOR];
    FILE *f = fopen(path, "rb");
    int ok;

    if (!f) {
        return 0;
    }
    ok = !fseek(f, 16L * DISC_RAW_SECTOR, SEEK_SET) && fread(sector, 1, sizeof(sector), f) == sizeof(sector) &&
         PlayStationVolume(sector, mode);
    fclose(f);
    return ok;
}

/* Whether a .cue opens as a PlayStation disc. */
static int PlayStationCue(const char *path)
{
    Disc *disc = DiscOpen(path, NULL, 0);
    const uint8_t *sector = disc ? DiscRawSector(disc, 16) : NULL;
    int mode, ok = sector && PlayStationVolume(sector, &mode);

    if (disc) {
        DiscClose(disc);
    }
    return ok;
}

/* ---- paths ---- */

static const char *NameOf(const char *path)
{
    const char *name = SDL_strrchr(path, '/');
#ifdef _WIN32
    if (SDL_strrchr(path, '\\') > name) {
        name = SDL_strrchr(path, '\\');
    }
#endif
    return name ? name + 1 : path;
}

/* The folder in the user's data folder for a disc image of this name (less its extension: a .tar's too), with a
   separator at the end. */
static void Destination(const char *path, char *out, size_t size)
{
    char *base = SDL_GetPrefPath("Psygnosis", "WipeoutXL");
    const char *name = NameOf(path), *dot = SDL_strrchr(name, '.');
    int length = (int)(dot && dot > name ? (size_t)(dot - name) : strlen(name));

    if (length > 4 && !SDL_strncasecmp(name + length - 4, ".tar", 4)) {
        length -= 4;
    }

    SDL_snprintf(out, size, "%sdiscs/%.*s/", base ? base : "./", length ? length : 4, length ? name : "disc");
    SDL_free(base);
}

/* Makes the folders of a path (up to its last separator). */
static void MakeFolders(char *path)
{
    char *at;

    for (at = path + 1; *at; at++) {
        if (*at == '/') {
            *at = 0;
            SDL_CreateDirectory(path);
            *at = '/';
        }
    }
}

static SDL_EnumerationResult SDLCALL RemoveEntry(void *user, const char *dir, const char *name)
{
    char path[2048];
    SDL_PathInfo info;

    (void)user;
    SDL_snprintf(path, sizeof(path), "%s%s", dir, name);
    if (SDL_GetPathInfo(path, &info) && info.type == SDL_PATHTYPE_DIRECTORY) {
        SDL_EnumerateDirectory(path, RemoveEntry, NULL);
    }
    SDL_RemovePath(path);
    return SDL_ENUM_CONTINUE;
}

/* Removes one of discs/'s folders (what an earlier extraction left there, or a failed one). */
static void Remove(const char *folder)
{
    size_t length = strlen(folder);

    if (length > 8 && strstr(folder, "discs/") && folder[length - 1] == '/' && strcmp(folder + length - 7, "discs//")) {
        SDL_EnumerateDirectory(folder, RemoveEntry, NULL);
        SDL_RemovePath(folder);
    }
}

static void Title(const char *text)
{
    int count = 0;
    SDL_Window **windows;

    if (!SDL_IsMainThread()) {
        return; /* (the debug console shows its own progress) */
    }
    windows = SDL_GetWindows(&count);
    if (windows && count) {
        SDL_SetWindowTitle(windows[0], text);
    }
    SDL_free(windows);
}

static void Progress(int thousandths)
{
    static int shown = -1;
    char title[96];

    SDL_SetAtomicInt(&progress, thousandths);
    if (thousandths / 10 != shown) {
        shown = thousandths / 10;
        SDL_snprintf(title, sizeof(title), "Wipeout XL: unpacking the disc image, %d%%", shown);
        Title(title);
    }
}

/* An archive's file name made safe to extract (or NULL: a name outside the folder); '\' made '/'. */
static char *SafeName(const char *name)
{
    char *safe, *c;

    if (!name || !name[0] || name[0] == '/' || name[0] == '\\' || (name[0] && name[1] == ':') || strstr(name, "..")) {
        return NULL;
    }
    safe = SDL_strdup(name);
    for (c = safe; *c; c++) {
        if (*c == '\\') {
            *c = '/';
        }
    }
    return safe;
}

/* ---- a .cue for a disc image without one ---- */

/* The number in a name's "(Track N)" (0 if none); where it starts into *at. */
static int TrackNumber(const char *name, const char **at)
{
    const char *track = NULL, *p;

    for (p = name; (p = SDL_strcasestr(p, "(Track ")) != NULL; p++) {
        track = p;
    }
    *at = track;
    return track ? SDL_atoi(track + 7) : 0;
}

typedef struct {
    const char *prefix, *extension; /* the data track's name before "(Track N)", and its extension */
    size_t prefixLength;
    char *names[DISC_MAX_TRACKS + 1]; /* by track number */
} Siblings;

static SDL_EnumerationResult SDLCALL SiblingEntry(void *user, const char *dir, const char *name)
{
    Siblings *s = (Siblings *)user;
    const char *at;
    int number = TrackNumber(name, &at);
    const char *close = at ? strchr(at, ')') : NULL;

    (void)dir;
    if (number > 0 && number <= DISC_MAX_TRACKS && (size_t)(at - name) == s->prefixLength &&
        !strncmp(name, s->prefix, s->prefixLength) && close && !SDL_strcasecmp(close + 1, s->extension) &&
        !s->names[number]) {
        s->names[number] = SDL_strdup(name);
    }
    return SDL_ENUM_CONTINUE;
}

/* Writes a .cue at cuePath for the data track bin (of the mode found) and the audio tracks beside it. */
static int MakeCue(const char *bin, int mode, const char *cuePath, char *message, size_t messageSize)
{
    char dir[1100], cueDir[1100], path[1200];
    const char *name = NameOf(bin), *at;
    int first = TrackNumber(name, &at), i, track = 1, relative;
    Siblings siblings;
    FILE *f;

    SDL_snprintf(dir, sizeof(dir), "%.*s", (int)(name - bin), bin);
    SDL_snprintf(cueDir, sizeof(cueDir), "%.*s", (int)(NameOf(cuePath) - cuePath), cuePath);
    relative = !strcmp(dir, cueDir);
    memset(&siblings, 0, sizeof(siblings));
    if (first > 0) {
        /* Redump's "Game (Track 01).bin" with "Game (Track 02).bin" and on: the audio tracks */
        siblings.prefix = name;
        siblings.prefixLength = (size_t)(at - name);
        siblings.extension = SDL_strrchr(name, '.') ? SDL_strrchr(name, '.') : "";
        SDL_EnumerateDirectory(dir[0] ? dir : ".", SiblingEntry, &siblings);
    }
    SDL_strlcpy(path, cuePath, sizeof(path));
    MakeFolders(path);
    f = fopen(cuePath, "w");
    if (!f) {
        SDL_snprintf(message, messageSize, "%s could not be written.", cuePath);
        for (i = 0; i <= DISC_MAX_TRACKS; i++) {
            SDL_free(siblings.names[i]);
        }
        return 0;
    }
    fprintf(f, "FILE \"%s\" BINARY\n  TRACK 01 MODE%d/2352\n    INDEX 01 00:00:00\n", relative ? name : bin, mode);
    for (i = first + 1; first > 0 && i <= DISC_MAX_TRACKS; i++) {
        if (siblings.names[i]) {
            track++;
            fprintf(f, "FILE \"%s%s\" BINARY\n  TRACK %02d AUDIO\n    INDEX 00 00:00:00\n    INDEX 01 00:02:00\n",
                    relative ? "" : dir, siblings.names[i], track);
        }
    }
    for (i = 0; i <= DISC_MAX_TRACKS; i++) {
        SDL_free(siblings.names[i]);
    }
    fclose(f);
    return 1;
}

/* ---- the image among extracted files ---- */

typedef struct {
    const char *root;
    char **paths; /* relative to root */
    int count, capacity;
} Listing;

static SDL_EnumerationResult SDLCALL ListEntry(void *user, const char *dir, const char *name)
{
    Listing *l = (Listing *)user;
    char path[2048];
    SDL_PathInfo info;

    SDL_snprintf(path, sizeof(path), "%s%s", dir, name);
    if (!SDL_GetPathInfo(path, &info)) {
        return SDL_ENUM_CONTINUE;
    }
    if (info.type == SDL_PATHTYPE_DIRECTORY) {
        SDL_strlcat(path, "/", sizeof(path));
        SDL_EnumerateDirectory(path, ListEntry, l);
    } else if (info.type == SDL_PATHTYPE_FILE) {
        if (l->count == l->capacity) {
            l->capacity = l->capacity ? l->capacity * 2 : 32;
            l->paths = (char **)SDL_realloc(l->paths, (size_t)l->capacity * sizeof(char *));
        }
        l->paths[l->count++] = SDL_strdup(path + strlen(l->root));
    }
    return SDL_ENUM_CONTINUE;
}

static int Depth(const char *path)
{
    int depth = 0;

    for (; *path; path++) {
        depth += *path == '/';
    }
    return depth;
}

/* Nearer the top first, then by name. */
static int CompareListed(const void *a, const void *b)
{
    const char *x = *(const char *const *)a, *y = *(const char *const *)b;

    return Depth(x) != Depth(y) ? Depth(x) - Depth(y) : SDL_strcasecmp(x, y);
}

/* The PlayStation disc image among the files extracted into folder: its .cue (made, for a .bin without one),
   relative to folder, into cue. */
static int FindImage(const char *folder, char *cue, size_t cueSize)
{
    Listing listing = {folder, NULL, 0, 0};
    char path[2048], made[2048], message[256];
    int i, mode, found = 0;

    SDL_EnumerateDirectory(folder, ListEntry, &listing);
    if (listing.count) {
        SDL_qsort(listing.paths, (size_t)listing.count, sizeof(char *), CompareListed);
    }
    for (i = 0; !found && i < listing.count; i++) {
        SDL_snprintf(path, sizeof(path), "%s%s", folder, listing.paths[i]);
        if (HasExtension(path, "cue") && PlayStationCue(path)) {
            SDL_strlcpy(cue, listing.paths[i], cueSize);
            found = 1;
        }
    }
    for (i = 0; !found && i < listing.count; i++) {
        SDL_snprintf(path, sizeof(path), "%s%s", folder, listing.paths[i]);
        if ((HasExtension(path, "bin") || HasExtension(path, "img")) && PlayStationImage(path, &mode)) {
            char *dot;

            SDL_strlcpy(made, path, sizeof(made));
            dot = SDL_strrchr(made, '.');
            SDL_strlcpy(dot, ".cue", sizeof(made) - (size_t)(dot - made));
            if (MakeCue(path, mode, made, message, sizeof(message))) {
                SDL_strlcpy(cue, made + strlen(folder), cueSize);
                found = 1;
            }
        }
    }
    for (i = 0; i < listing.count; i++) {
        SDL_free(listing.paths[i]);
    }
    SDL_free(listing.paths);
    return found;
}

/* ---- extraction ---- */

static int Extract7z(const char *archive, const char *folder, char *message, size_t messageSize)
{
    ISzAlloc alloc = {SzAlloc, SzFree}, allocTemp = {SzAllocTemp, SzFreeTemp};
    CFileInStream file;
    CLookToRead2 look;
    CSzArEx db;
    UInt32 blockIndex = 0xFFFFFFFF, i;
    Byte *outBuffer = NULL;
    size_t outBufferSize = 0;
    UInt64 total = 0, done = 0;
    SRes result;
    int ok = 1;

    if (InFile_Open(&file.file, archive)) {
        SDL_snprintf(message, messageSize, "%s could not be opened.", archive);
        return 0;
    }
    FileInStream_CreateVTable(&file);
    file.wres = 0;
    LookToRead2_CreateVTable(&look, False);
    look.buf = (Byte *)ISzAlloc_Alloc(&alloc, 1 << 18);
    look.bufSize = 1 << 18;
    look.realStream = &file.vt;
    LookToRead2_INIT(&look);
    CrcGenerateTable();
    SzArEx_Init(&db);
    result = SzArEx_Open(&db, &look.vt, &alloc, &allocTemp);
    if (result != SZ_OK) {
        SDL_snprintf(message, messageSize, "%s is not a 7z archive this can read (error %d).", archive, (int)result);
        ok = 0;
    }
    for (i = 0; ok && i < db.NumFiles; i++) {
        total += SzArEx_GetFileSize(&db, i);
    }
    for (i = 0; ok && i < db.NumFiles; i++) {
        UInt16 name16[1024];
        size_t length = SzArEx_GetFileNameUtf16(&db, i, NULL), offset = 0, processed = 0;
        char *utf8, *name, path[2048];
        FILE *out;

        if (length > SDL_arraysize(name16) || SzArEx_IsDir(&db, i)) {
            continue;
        }
        SzArEx_GetFileNameUtf16(&db, i, name16);
        utf8 = SDL_iconv_string("UTF-8", "UTF-16LE", (const char *)name16, (length - 1) * 2);
        name = SafeName(utf8);
        SDL_free(utf8);
        if (!name) {
            continue;
        }
        result = SzArEx_Extract(&db, &look.vt, i, &blockIndex, &outBuffer, &outBufferSize, &offset, &processed, &alloc,
                                &allocTemp);
        if (result != SZ_OK) {
            SDL_snprintf(message, messageSize, "%s could not be unpacked from the archive (error %d).", name, (int)result);
            SDL_free(name);
            ok = 0;
            break;
        }
        SDL_snprintf(path, sizeof(path), "%s%s", folder, name);
        MakeFolders(path);
        out = fopen(path, "wb");
        if (!out || fwrite(outBuffer + offset, 1, processed, out) != processed) {
            SDL_snprintf(message, messageSize, "%s could not be written (is the disk full?).", path);
            ok = 0;
        }
        if (out) {
            fclose(out);
        }
        SDL_free(name);
        done += processed;
        Progress((int)(done * 1000 / (total ? total : 1)));
    }
    ISzAlloc_Free(&alloc, outBuffer);
    SzArEx_Free(&db, &alloc);
    ISzAlloc_Free(&alloc, look.buf);
    File_Close(&file.file);
    return ok;
}

#ifdef PORT_HAVE_LIBARCHIVE
static int ExtractOther(const char *archive, const char *folder, char *message, size_t messageSize)
{
    struct archive *a = archive_read_new();
    struct archive_entry *entry;
    SDL_PathInfo info;
    Sint64 size = SDL_GetPathInfo(archive, &info) && info.size ? (Sint64)info.size : 1;
    int ok = 1, r = ARCHIVE_EOF;

    archive_read_support_filter_all(a);
    archive_read_support_format_all(a);
    archive_read_support_format_raw(a); /* a single compressed file (a .bin.gz) */
    if (archive_read_open_filename(a, archive, 1 << 18) != ARCHIVE_OK) {
        SDL_snprintf(message, messageSize, "%s is not an archive this can read (%s).", archive, archive_error_string(a));
        archive_read_free(a);
        return 0;
    }
    while (ok && (r = archive_read_next_header(a, &entry)) == ARCHIVE_OK) {
        const char *entryName = archive_entry_pathname_utf8(entry);
        char raw[1024], path[2048], *name;
        const void *block;
        size_t length;
        la_int64_t offset, at = 0;
        FILE *out;

        if (archive_format(a) == ARCHIVE_FORMAT_RAW) {
            /* named as the archive, less its extension */
            const char *base = NameOf(archive), *dot = SDL_strrchr(base, '.');

            SDL_snprintf(raw, sizeof(raw), "%.*s", (int)(dot && dot > base ? (size_t)(dot - base) : strlen(base)), base);
            entryName = raw;
        } else if (!entryName) {
            entryName = archive_entry_pathname(entry);
        }
        if (archive_entry_filetype(entry) != AE_IFREG || !(name = SafeName(entryName))) {
            continue;
        }
        SDL_snprintf(path, sizeof(path), "%s%s", folder, name);
        SDL_free(name);
        MakeFolders(path);
        out = fopen(path, "wb");
        if (!out) {
            SDL_snprintf(message, messageSize, "%s could not be written.", path);
            ok = 0;
            break;
        }
        while ((r = archive_read_data_block(a, &block, &length, &offset)) == ARCHIVE_OK) {
            if (offset != at && fseek(out, (long)offset, SEEK_SET)) {
                r = ARCHIVE_FATAL;
                break;
            }
            if (fwrite(block, 1, length, out) != length) {
                SDL_snprintf(message, messageSize, "%s could not be written (is the disk full?).", path);
                ok = 0;
                break;
            }
            at = offset + (la_int64_t)length;
            Progress((int)SDL_min(999, archive_filter_bytes(a, -1) * 1000 / size));
        }
        fclose(out);
        if (ok && r != ARCHIVE_EOF) {
            SDL_snprintf(message, messageSize, "%s could not be unpacked from the archive (%s).", entryName,
                         archive_error_string(a));
            ok = 0;
        }
    }
    if (ok && r != ARCHIVE_EOF) {
        SDL_snprintf(message, messageSize, "%s could not be read (%s).", archive, archive_error_string(a));
        ok = 0;
    }
    archive_read_free(a);
    return ok;
}
#else
static int ExtractOther(const char *archive, const char *folder, char *message, size_t messageSize)
{
    (void)folder;
    SDL_snprintf(message, messageSize, "%s: this build of the game reads only .7z archives.", archive);
    return 0;
}
#endif

/* The note of an extraction: "size date\n" (then the .cue's path). */
static void Note(const char *archive, char *out, size_t size)
{
    SDL_PathInfo info;

    if (!SDL_GetPathInfo(archive, &info)) {
        out[0] = 0;
        return;
    }
    SDL_snprintf(out, size, "%lld %lld\n", (long long)info.size, (long long)info.modify_time);
}

static int ArchiveImage(const char *archive, int kind, char *cue, size_t cueSize, char *message, size_t messageSize)
{
    char folder[1100], notePath[1200], note[128], relative[1024], *saved;
    size_t savedSize = 0;
    int ok;

    Destination(archive, folder, sizeof(folder));
    SDL_snprintf(notePath, sizeof(notePath), "%sextracted.txt", folder);
    Note(archive, note, sizeof(note));
    if (!note[0]) {
        SDL_snprintf(message, messageSize, "%s is not there.", archive);
        return 0;
    }
    /* extracted before, from this very archive */
    saved = SDL_LoadFile(notePath, &savedSize);
    if (saved && !strncmp(saved, note, strlen(note))) {
        SDL_snprintf(cue, cueSize, "%s%s", folder, saved + strlen(note));
        cue[strcspn(cue, "\r\n")] = 0;
        SDL_free(saved);
        if (SDL_GetPathInfo(cue, NULL)) {
            return 1;
        }
    } else {
        SDL_free(saved);
    }

    Remove(folder);
    Progress(0);
    ok = kind == ARCHIVE_7Z ? Extract7z(archive, folder, message, messageSize)
                            : ExtractOther(archive, folder, message, messageSize);
    Title("Wipeout XL");
    if (ok && !FindImage(folder, relative, sizeof(relative))) {
        SDL_snprintf(message, messageSize, "There is no PlayStation disc image (a .cue with its .bin, or a .bin) in %s.",
                     archive);
        ok = 0;
    }
    if (!ok) {
        Remove(folder);
        cue[0] = 0;
        return 0;
    }
    /* the note, last: an extraction cut short is done again */
    {
        FILE *f = fopen(notePath, "wb");

        if (f) {
            fprintf(f, "%s%s\n", note, relative);
            fclose(f);
        }
    }
    SDL_snprintf(cue, cueSize, "%s%s", folder, relative);
    Progress(1000);
    return 1;
}

/* ---- a disc's .bin without its .cue ---- */

typedef struct {
    const char *name; /* the .bin's */
    char cue[1200];   /* a .cue beside it naming it */
} CueSearch;

static SDL_EnumerationResult SDLCALL CueEntry(void *user, const char *dir, const char *name)
{
    CueSearch *s = (CueSearch *)user;
    char path[2048], quoted[1100];
    char *text;

    if (!HasExtension(name, "cue")) {
        return SDL_ENUM_CONTINUE;
    }
    SDL_snprintf(path, sizeof(path), "%s%s", dir, name);
    SDL_snprintf(quoted, sizeof(quoted), "\"%s\"", s->name);
    text = (char *)SDL_LoadFile(path, NULL);
    if (text && strstr(text, quoted)) {
        SDL_strlcpy(s->cue, path, sizeof(s->cue));
    }
    SDL_free(text);
    return s->cue[0] ? SDL_ENUM_SUCCESS : SDL_ENUM_CONTINUE;
}

static int BinImage(const char *bin, char *cue, size_t cueSize, char *message, size_t messageSize)
{
    CueSearch search;
    char dir[1100], folder[1100];
    int mode;

    /* its own .cue, when it is beside it */
    memset(&search, 0, sizeof(search));
    search.name = NameOf(bin);
    SDL_snprintf(dir, sizeof(dir), "%.*s", (int)(search.name - bin), bin);
    SDL_EnumerateDirectory(dir[0] ? dir : ".", CueEntry, &search);
    if (search.cue[0]) {
        SDL_strlcpy(cue, search.cue, cueSize);
        return 1;
    }
    if (!PlayStationImage(bin, &mode)) {
        SDL_snprintf(message, messageSize, "%s is not a PlayStation disc's data track (raw 2352-byte sectors).", bin);
        return 0;
    }
    Destination(bin, folder, sizeof(folder));
    SDL_snprintf(cue, cueSize, "%s%s", folder, "disc.cue");
    return MakeCue(bin, mode, cue, message, messageSize);
}

int PortDiscImage(const char *path, char *cue, size_t cueSize, char *message, size_t messageSize)
{
    int kind = ArchiveKind(path);

    cue[0] = 0;
    if (kind != NOT_ARCHIVE) {
        return ArchiveImage(path, kind, cue, cueSize, message, messageSize);
    }
    if (HasExtension(path, "bin") || HasExtension(path, "img")) {
        return BinImage(path, cue, cueSize, message, messageSize);
    }
    SDL_strlcpy(cue, path, cueSize);
    return 1;
}
