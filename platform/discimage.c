/* Disc images as they are kept. A .cue sheet with its .bin or WAV tracks is used as it is (psx/disc.c), and so is an
   .iso (the data track by itself), a CloneCD .ccd (with its .img) and an Alcohol 120% .mds (with its .mdf). A disc's
   .bin (or .img) without its .cue is given one, made in the user's data folder (discs/<its name>/): its data track, and
   its audio tracks when they are beside it as "... (Track N).bin" (Redump's names, each starting with its 2-second
   pregap); an .img or .mdf with its .ccd or .mds beside it is that instead.
   An archive holding a disc image, known by its first bytes (else its name), is extracted once into
   discs/<the archive's name>/, with a note of the archive's size and date and where the image's .cue is; while the
   note matches, the extracted image is used as it is. The image in it is found by what it is, not by its name or
   any checksum: a .cue, .ccd or .mds, or an .iso, whose first track is a game disc's (an ISO 9660 volume of the
   system "PLAYSTATION", or the PC game's files: DiscIsPcGame), else such a .bin or .img, given a .cue as above.
   Which game it is, is for the caller.
   A .7z is read with the LZMA SDK's decoder (platform/third_party/lzma, public domain; a solid block is unpacked in
   memory, one at a time), the others with libarchive (zip, rar, tar, gzip, bzip2, xz, zstd; where the program was
   built with it). Copies kept as discs were about 2009 are taken as they come: archives in the archive are unpacked
   too, a track packed with ECM (".bin.ecm", ecm.c) or in Monkey's Audio (".ape", ape.c) is made its .bin again,
   and a .cue naming them is made to name the .bins. Such a data track chosen by itself (its .ecm, or a .cue naming
   its .bin when only the .ecm is there) is made a .bin in the user's data folder, with the .ape tracks beside it.
   A version 5 CD CHD is made a raw .bin and .cue there too (chd.c), once, with the same note and progress. */
#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

#include "ape.h"
#include "chd.h"
#include "discimage.h"
#include "ecm.h"
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
    return HasExtension(name, "cue") || HasExtension(name, "bin") || HasExtension(name, "img") ||
           HasExtension(name, "iso") || HasExtension(name, "ccd") || HasExtension(name, "mds") ||
           HasExtension(name, "mdf") || HasExtension(name, "ecm") || HasExtension(name, "chd") || IsArchiveName(name);
}

enum { NOT_ARCHIVE, ARCHIVE_7Z, ARCHIVE_OTHER, CHD_IMAGE, ECM_TRACK };

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
        {257, 5, "ustar", ARCHIVE_OTHER},          {0, 8, "MComprHD", CHD_IMAGE},
    };
    unsigned char head[264];
    size_t got = 0, i;
    FILE *f = path ? fopen(path, "rb") : NULL;

    if (!f) {
        if (path && HasExtension(path, "chd")) {
            return CHD_IMAGE;
        }
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

/* Whether a disc's volume descriptor (sector 16 of its data track) is a PlayStation disc's, or a Sega Saturn disc's
   (saturn.c); or the disc holds the PC game's files. */
static int GameDisc(const Disc *disc)
{
    const uint8_t *sector = DiscRawSector(disc, 16);
    const uint8_t *data = sector ? sector + DiscTrackInfo(disc, 0)->data_offset : NULL;

    return (data && data[0] == 1 && !memcmp(data + 1, "CD001", 5) &&
            (!memcmp(data + 8, "PLAYSTATION", 11) || !memcmp(data + 8, "SEGA SEGASATURN", 15))) ||
           DiscIsPcGame(disc);
}

/* Whether an image opens as a game disc: a .cue, .ccd, .mds or .iso, or a .bin or .img of the data track alone. */
static int GameImage(const char *path)
{
    Disc *disc = DiscOpen(path, NULL, 0);
    int ok = disc && GameDisc(disc);

    if (disc) {
        DiscClose(disc);
    }
    return ok;
}

/* The data track of a .bin or .img that is a game disc's: its mode (1 or 2) into *mode and its sector size (2352 raw,
   or 2048) into *sectorSize. */
static int GameTrack(const char *path, int *mode, int *sectorSize)
{
    Disc *disc = DiscOpen(path, NULL, 0);
    const DiscTrack *track = disc ? DiscTrackInfo(disc, 0) : NULL;
    int ok = track && GameDisc(disc);

    if (ok) {
        *mode = track->data_offset == 24 ? 2 : 1;
        *sectorSize = track->sector_size;
    }
    if (disc) {
        DiscClose(disc);
    }
    return ok;
}

/* ---- paths ---- */

const char *PortPathName(const char *path)
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
    const char *name = PortPathName(path), *dot = SDL_strrchr(name, '.');
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
    const char *prefix, *extension; /* the data track's name before "(Track N)", and what follows it (" [SLES-00327].bin") */
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

/* Writes a .cue at cuePath for the data track bin (of the mode and sector size found) and the audio tracks beside
   it. */
static int MakeCue(const char *bin, int mode, int sectorSize, const char *cuePath, char *message, size_t messageSize)
{
    char dir[1100], cueDir[1100], path[1200];
    const char *name = PortPathName(bin), *at;
    int first = TrackNumber(name, &at), i, track = 1, relative;
    Siblings siblings;
    FILE *f;

    SDL_snprintf(dir, sizeof(dir), "%.*s", (int)(name - bin), bin);
    SDL_snprintf(cueDir, sizeof(cueDir), "%.*s", (int)(PortPathName(cuePath) - cuePath), cuePath);
    relative = !strcmp(dir, cueDir);
    memset(&siblings, 0, sizeof(siblings));
    if (first > 0) {
        /* Redump's "Game (Track 01).bin" with "Game (Track 02).bin" and on: the audio tracks */
        siblings.prefix = name;
        siblings.prefixLength = (size_t)(at - name);
        siblings.extension = strchr(at, ')') ? strchr(at, ')') + 1 : "";
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
    fprintf(f, "FILE \"%s\" BINARY\n  TRACK 01 MODE%d/%d\n    INDEX 01 00:00:00\n", relative ? name : bin, mode,
            sectorSize);
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

/* The game disc image among the files extracted into folder: a .cue, .ccd, .mds or .iso (the first of them by that
   order), else a .bin or .img (given a .cue, made for it), relative to folder, into cue. */
static int FindImage(const char *folder, char *cue, size_t cueSize)
{
    static const char *const described[] = {"cue", "ccd", "mds", "iso"};
    Listing listing = {folder, NULL, 0, 0};
    char path[2048], made[2048], message[256];
    int i, k, mode, sectorSize, found = 0;

    SDL_EnumerateDirectory(folder, ListEntry, &listing);
    if (listing.count) {
        SDL_qsort(listing.paths, (size_t)listing.count, sizeof(char *), CompareListed);
    }
    for (k = 0; !found && k < (int)SDL_arraysize(described); k++) {
        for (i = 0; !found && i < listing.count; i++) {
            SDL_snprintf(path, sizeof(path), "%s%s", folder, listing.paths[i]);
            if (HasExtension(path, described[k]) && GameImage(path)) {
                SDL_strlcpy(cue, listing.paths[i], cueSize);
                found = 1;
            }
        }
    }
    for (i = 0; !found && i < listing.count; i++) {
        SDL_snprintf(path, sizeof(path), "%s%s", folder, listing.paths[i]);
        if ((HasExtension(path, "bin") || HasExtension(path, "img")) && GameTrack(path, &mode, &sectorSize)) {
            char *dot;

            SDL_strlcpy(made, path, sizeof(made));
            dot = SDL_strrchr(made, '.');
            SDL_strlcpy(dot, ".cue", sizeof(made) - (size_t)(dot - made));
            if (MakeCue(path, mode, sectorSize, made, message, sizeof(message))) {
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
            const char *base = PortPathName(archive), *dot = SDL_strrchr(base, '.');

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
/* ---- copies kept as discs were about 2009: archives in the archive, .ecm and .ape tracks ---- */

/* The listing of folder's files (relative to it), nearer the top first. */
static Listing List(const char *folder)
{
    Listing listing = {folder, NULL, 0, 0};

    SDL_EnumerateDirectory(folder, ListEntry, &listing);
    if (listing.count) {
        SDL_qsort(listing.paths, (size_t)listing.count, sizeof(char *), CompareListed);
    }
    return listing;
}

static void FreeListing(Listing *listing)
{
    int i;

    for (i = 0; i < listing->count; i++) {
        SDL_free(listing->paths[i]);
    }
    SDL_free(listing->paths);
}

/* The name a packed track is made into: "x.bin.ecm" -> "x.bin", "x.ecm" -> "x.bin", "x.ape" -> "x.bin". */
static void Unpacked(const char *path, char *out, size_t size)
{
    size_t length = strlen(path) - 4; /* less ".ecm" or ".ape" */

    SDL_snprintf(out, size, "%.*s", (int)length, path);
    if (HasExtension(path, "ape") || (!HasExtension(out, "bin") && !HasExtension(out, "img"))) {
        SDL_strlcat(out, ".bin", size);
    }
}

/* Makes the packed track at path its .bin at out. */
static int UnpackTrack(const char *path, const char *out, char *message, size_t messageSize)
{
    return HasExtension(path, "ape") ? PortApeDecode(path, out, message, messageSize)
                                     : PortEcmDecode(path, out, message, messageSize);
}

/* A .cue naming packed tracks made to name their .bins: "x.bin.ecm" as "x.bin", "x.ape" WAVE as "x.bin" BINARY. */
static void RenameInCue(const char *path)
{
    size_t size = 0;
    char *text = (char *)SDL_LoadFile(path, &size), *line, *next, *out;
    size_t used = 0;
    int changed = 0;

    if (!text) {
        return;
    }
    out = (char *)SDL_malloc(size * 2 + 64);
    for (line = text; out && *line; line = next) {
        char *quote1 = NULL, *quote2 = NULL, name[1024], renamed[1100];
        size_t length;

        next = strchr(line, '\n');
        next = next ? next + 1 : line + strlen(line);
        length = (size_t)(next - line);
        if (length > 4 && !SDL_strncasecmp(line + strspn(line, " \t"), "FILE", 4)) {
            quote1 = strchr(line, '"');
            quote2 = quote1 && quote1 < next ? strchr(quote1 + 1, '"') : NULL;
        }
        if (quote2 && quote2 < next && (size_t)(quote2 - quote1 - 1) < sizeof(name)) {
            SDL_snprintf(name, sizeof(name), "%.*s", (int)(quote2 - quote1 - 1), quote1 + 1);
            if (HasExtension(name, "ecm") || HasExtension(name, "ape")) {
                Unpacked(name, renamed, sizeof(renamed));
                used += (size_t)SDL_snprintf(out + used, size * 2 + 64 - used, "FILE \"%s\" BINARY\n", renamed);
                changed = 1;
                continue;
            }
        }
        memcpy(out + used, line, length);
        used += length;
    }
    if (out && changed) {
        FILE *f = fopen(path, "wb");

        if (f) {
            fwrite(out, 1, used, f);
            fclose(f);
        }
    }
    SDL_free(out);
    SDL_free(text);
}

/* A CD CHD, made its .bin and .cue in folder. */
static int UnpackChd(const char *path, const char *folder, char *message, size_t messageSize)
{
    char bin[2200], cue[2200];

    SDL_snprintf(bin, sizeof(bin), "%sdisc.bin", folder);
    SDL_snprintf(cue, sizeof(cue), "%sdisc.cue", folder);
    MakeFolders(bin);
    return PortChdDecode(path, bin, cue, Progress, message, messageSize);
}

/* In folder (extracted): the archives in it unpacked where they are (twice over at most), then its .ecm and .ape
   tracks made .bins, and its .cues made to name them. */
static int UnpackOldCopy(const char *folder, char *message, size_t messageSize)
{
    char path[2048], out[2100], dir[2048];
    Listing listing;
    int round, i, total = 0, done = 0, ok = 1;

    for (round = 0; round < 2 && ok; round++) {
        int found = 0;

        listing = List(folder);
        for (i = 0; ok && i < listing.count; i++) {
            int kind;

            SDL_snprintf(path, sizeof(path), "%s%s", folder, listing.paths[i]);
            if (HasExtension(path, "bin") || HasExtension(path, "img") || HasExtension(path, "cue") ||
                HasExtension(path, "ecm") || HasExtension(path, "ape") ||
                (kind = ArchiveKind(path)) == NOT_ARCHIVE) {
                continue;
            }
            SDL_snprintf(dir, sizeof(dir), "%.*s", (int)(PortPathName(path) - path), path);
            if (kind == CHD_IMAGE) {
                SDL_snprintf(dir, sizeof(dir), "%s.unpacked/", path);
                ok = UnpackChd(path, dir, message, messageSize);
            } else {
                ok = kind == ARCHIVE_7Z ? Extract7z(path, dir, message, messageSize)
                                       : ExtractOther(path, dir, message, messageSize);
            }
            SDL_RemovePath(path);
            found = 1;
        }
        FreeListing(&listing);
        if (!found) {
            break;
        }
    }
    listing = List(folder);
    for (i = 0; i < listing.count; i++) {
        total += HasExtension(listing.paths[i], "ecm") || HasExtension(listing.paths[i], "ape");
    }
    for (i = 0; ok && i < listing.count; i++) {
        SDL_snprintf(path, sizeof(path), "%s%s", folder, listing.paths[i]);
        if (!HasExtension(path, "ecm") && !HasExtension(path, "ape")) {
            continue;
        }
        Unpacked(path, out, sizeof(out));
        ok = UnpackTrack(path, out, message, messageSize);
        SDL_RemovePath(path);
        Progress(++done * 1000 / total);
    }
    for (i = 0; ok && i < listing.count; i++) {
        SDL_snprintf(path, sizeof(path), "%s%s", folder, listing.paths[i]);
        if (HasExtension(path, "cue")) {
            RenameInCue(path);
        }
    }
    FreeListing(&listing);
    return ok;
}

typedef struct {
    const char *prefix;  /* the data track's name before its "(Track N)" */
    char suffix[256];    /* ... and what follows that up to its extensions (" [SLES-00327]") */
    size_t prefixLength;
    const char *dir, *folder;
    int ok, count, done;
    char *message;
    size_t messageSize;
} LooseTracks;

static SDL_EnumerationResult SDLCALL LooseTrack(void *user, const char *dir, const char *name)
{
    LooseTracks *l = (LooseTracks *)user;
    const char *at, *close;
    char path[2048], out[2100], wanted[300];

    (void)dir;
    SDL_snprintf(wanted, sizeof(wanted), "%s.ape", l->suffix);
    if (TrackNumber(name, &at) < 2 || (size_t)(at - name) != l->prefixLength ||
        strncmp(name, l->prefix, l->prefixLength) || !(close = strchr(at, ')')) || SDL_strcasecmp(close + 1, wanted)) {
        return SDL_ENUM_CONTINUE;
    }
    if (!l->folder) {
        l->count++;
        return SDL_ENUM_CONTINUE;
    }
    SDL_snprintf(path, sizeof(path), "%s%s", l->dir, name);
    SDL_snprintf(out, sizeof(out), "%s%s", l->folder, name);
    Unpacked(out, out, sizeof(out));
    l->ok = PortApeDecode(path, out, l->message, l->messageSize);
    Progress(++l->done * 1000 / (l->count + 1));
    return l->ok ? SDL_ENUM_CONTINUE : SDL_ENUM_FAILURE;
}

/* A data track packed with ECM, chosen by itself: made a .bin in folder, with the .ape tracks beside it ("... (Track
   N) ....ape", named as it is) made .bins there too. */
static int UnpackLoose(const char *ecm, const char *folder, char *message, size_t messageSize)
{
    char out[2100], path[2048], dir[1100];
    const char *name = PortPathName(ecm), *at;
    LooseTracks tracks;

    SDL_snprintf(path, sizeof(path), "%s%s", folder, name);
    MakeFolders(path);
    Unpacked(path, out, sizeof(out));
    Progress(0);
    if (!PortEcmDecode(ecm, out, message, messageSize)) {
        return 0;
    }
    memset(&tracks, 0, sizeof(tracks));
    if (TrackNumber(name, &at) == 1) {
        SDL_snprintf(dir, sizeof(dir), "%.*s", (int)(name - ecm), ecm);
        tracks.prefix = name;
        tracks.prefixLength = (size_t)(at - name);
        SDL_strlcpy(tracks.suffix, strchr(at, ')') + 1, sizeof(tracks.suffix));
        tracks.suffix[strlen(tracks.suffix) - 4] = 0; /* less ".ecm" */
        if (HasExtension(tracks.suffix, "bin") || HasExtension(tracks.suffix, "img")) {
            tracks.suffix[strlen(tracks.suffix) - 4] = 0;
        }
        tracks.dir = dir;
        tracks.ok = 1;
        tracks.message = message;
        tracks.messageSize = messageSize;
        SDL_EnumerateDirectory(dir[0] ? dir : ".", LooseTrack, &tracks); /* (counted, then made) */
        tracks.folder = folder;
        SDL_EnumerateDirectory(dir[0] ? dir : ".", LooseTrack, &tracks);
        if (!tracks.ok) {
            return 0;
        }
    }
    return 1;
}

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
    ok = kind == ARCHIVE_7Z     ? Extract7z(archive, folder, message, messageSize)
         : kind == CHD_IMAGE ? UnpackChd(archive, folder, message, messageSize)
         : kind == ECM_TRACK ? UnpackLoose(archive, folder, message, messageSize)
                             : ExtractOther(archive, folder, message, messageSize);
    ok = ok && UnpackOldCopy(folder, message, messageSize);
    Title("Wipeout XL");
    if (ok && !FindImage(folder, relative, sizeof(relative))) {
        SDL_snprintf(message, messageSize,
                     "There is no game disc image (a .cue with its .bin, a .ccd, .mds or .iso, or a .bin) in %s.", archive);
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

/* A descriptor beside an image's data (.img of a .ccd, .mdf of a .mds): its path into out, 1 if it is there. The
   extension is tried as written here, then in capitals. */
static int DescriptorBeside(const char *path, const char *extension, char *out, size_t size)
{
    char *dot;
    int upper, i;

    SDL_strlcpy(out, path, size);
    dot = SDL_strrchr(out, '.');
    if (!dot || strlen(extension) + 2 > size - (size_t)(dot - out)) {
        return 0;
    }
    for (upper = 0; upper < 2; upper++) {
        for (i = 0; extension[i]; i++) {
            dot[1 + i] = (char)(upper ? SDL_toupper(extension[i]) : extension[i]);
        }
        dot[1 + i] = 0;
        if (SDL_GetPathInfo(out, NULL)) {
            return 1;
        }
    }
    return 0;
}

static int BinImage(const char *bin, char *cue, size_t cueSize, char *message, size_t messageSize)
{
    CueSearch search;
    char dir[1100], folder[1100];
    int mode, sectorSize;

    /* the image of a CloneCD or Alcohol disc: its descriptor */
    if ((HasExtension(bin, "img") && DescriptorBeside(bin, "ccd", cue, cueSize)) ||
        (HasExtension(bin, "mdf") && DescriptorBeside(bin, "mds", cue, cueSize))) {
        return 1;
    }

    /* its own .cue, when it is beside it */
    memset(&search, 0, sizeof(search));
    search.name = PortPathName(bin);
    SDL_snprintf(dir, sizeof(dir), "%.*s", (int)(search.name - bin), bin);
    SDL_EnumerateDirectory(dir[0] ? dir : ".", CueEntry, &search);
    if (search.cue[0]) {
        SDL_strlcpy(cue, search.cue, cueSize);
        return 1;
    }
    if (!GameTrack(bin, &mode, &sectorSize)) {
        SDL_snprintf(message, messageSize, "%s is not the data track of a PlayStation or PC game disc.", bin);
        return 0;
    }
    Destination(bin, folder, sizeof(folder));
    SDL_snprintf(cue, cueSize, "%s%s", folder, "disc.cue");
    return MakeCue(bin, mode, sectorSize, cue, message, messageSize);
}

/* A .cue whose first file is not there but is beside it packed with ECM: that .ecm (into ecm), else 0. */
static int CueOfEcm(const char *cuePath, char *ecm, size_t ecmSize)
{
    char *text = (char *)SDL_LoadFile(cuePath, NULL), *quote1, *quote2, *file;
    int found = 0;

    file = text ? SDL_strcasestr(text, "FILE") : NULL;
    quote1 = file ? strchr(file, '"') : NULL;
    quote2 = quote1 ? strchr(quote1 + 1, '"') : NULL;
    if (quote2) {
        char bin[1200];

        SDL_snprintf(bin, sizeof(bin), "%.*s%.*s", (int)(PortPathName(cuePath) - cuePath), cuePath,
                     (int)(quote2 - quote1 - 1), quote1 + 1);
        SDL_snprintf(ecm, ecmSize, "%s.ecm", bin);
        found = !SDL_GetPathInfo(bin, NULL) && PortIsEcm(ecm);
    }
    SDL_free(text);
    return found;
}

int PortDiscImage(const char *path, char *cue, size_t cueSize, char *message, size_t messageSize)
{
    int kind = ArchiveKind(path);
    char ecm[1200];

    cue[0] = 0;
    if (kind == NOT_ARCHIVE && HasExtension(path, "ecm") && PortIsEcm(path)) {
        kind = ECM_TRACK;
    } else if (kind == NOT_ARCHIVE && HasExtension(path, "cue") && CueOfEcm(path, ecm, sizeof(ecm))) {
        return ArchiveImage(ecm, ECM_TRACK, cue, cueSize, message, messageSize);
    }
    if (kind != NOT_ARCHIVE) {
        return ArchiveImage(path, kind, cue, cueSize, message, messageSize);
    }
    if (HasExtension(path, "bin") || HasExtension(path, "img") || HasExtension(path, "mdf")) {
        return BinImage(path, cue, cueSize, message, messageSize);
    }
    SDL_strlcpy(cue, path, cueSize);
    return 1;
}
