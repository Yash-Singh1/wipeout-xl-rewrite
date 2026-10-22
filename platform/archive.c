/* Disc images in archives: a .7z holding a disc image (as images are often kept, the .cue and its .bin tracks in
   a folder) is extracted once into the user's data folder (discs/<the archive's name>/), with a note of the
   archive's size and date and where the .cue is; while the note matches, the extracted image is used as it is.
   The archive is read with the LZMA SDK's 7z decoder (platform/third_party/lzma, public domain); a solid
   block is unpacked in memory, one at a time. */
#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

#include "archive.h"

#include "third_party/lzma/7z.h"
#include "third_party/lzma/7zAlloc.h"
#include "third_party/lzma/7zCrc.h"
#include "third_party/lzma/7zFile.h"

static SDL_AtomicInt progress; /* the extraction's, in thousandths */

int PortArchiveProgress(void)
{
    return SDL_GetAtomicInt(&progress);
}

int PortIsArchive(const char *path)
{
    size_t length = path ? strlen(path) : 0;

    return length > 3 && !SDL_strcasecmp(path + length - 3, ".7z");
}

/* The folder the archive's image goes to, with a separator at the end. */
static void Destination(const char *archive, char *out, size_t size)
{
    char *base = SDL_GetPrefPath("Psygnosis", "WipeoutXL");
    const char *name = SDL_strrchr(archive, '/');
#ifdef _WIN32
    if (SDL_strrchr(archive, '\\') > name) {
        name = SDL_strrchr(archive, '\\');
    }
#endif
    name = name ? name + 1 : archive;
    SDL_snprintf(out, size, "%sdiscs/%.*s/", base ? base : "./", (int)(strlen(name) - 3), name);
    SDL_free(base);
}

/* The note of an extraction: "size date\ncue path". */
static void Note(const char *archive, char *out, size_t size)
{
    SDL_PathInfo info;

    if (!SDL_GetPathInfo(archive, &info)) {
        out[0] = 0;
        return;
    }
    SDL_snprintf(out, size, "%lld %lld\n", (long long)info.size, (long long)info.modify_time);
}

static void Title(const char *text)
{
    int count = 0;
    SDL_Window **windows = SDL_GetWindows(&count);

    if (windows && count) {
        SDL_SetWindowTitle(windows[0], text);
    }
    SDL_free(windows);
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

int PortArchiveDisc(const char *archive, char *cue, size_t cueSize, char *message, size_t messageSize)
{
    char folder[1100], notePath[1200], note[128], *saved;
    size_t savedSize = 0;
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

    cue[0] = 0;
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
        cue[0] = 0;
    } else {
        SDL_free(saved);
    }

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
    SDL_SetAtomicInt(&progress, 0);
    for (i = 0; ok && i < db.NumFiles; i++) {
        UInt16 name16[1024];
        size_t length = SzArEx_GetFileNameUtf16(&db, i, NULL), offset = 0, processed = 0;
        char *name, path[2048], title[300];
        FILE *out;

        if (length > SDL_arraysize(name16) || SzArEx_IsDir(&db, i)) {
            continue;
        }
        SzArEx_GetFileNameUtf16(&db, i, name16);
        name = SDL_iconv_string("UTF-8", "UTF-16LE", (const char *)name16, (length - 1) * 2);
        if (!name || strstr(name, "..")) {
            SDL_free(name);
            continue;
        }
        for (char *c = name; *c; c++) {
            if (*c == '\\') {
                *c = '/';
            }
        }
        SDL_snprintf(title, sizeof(title), "Wipeout XL: unpacking the disc image, %d%%", (int)(done * 100 / (total ? total : 1)));
        Title(title);
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
        if (ok && strlen(name) > 4 && !SDL_strcasecmp(name + strlen(name) - 4, ".cue") && !cue[0]) {
            SDL_strlcpy(cue, name, cueSize); /* (relative for now) */
        }
        SDL_free(name);
        done += processed;
        SDL_SetAtomicInt(&progress, (int)(done * 1000 / (total ? total : 1)));
    }
    ISzAlloc_Free(&alloc, outBuffer);
    SzArEx_Free(&db, &alloc);
    ISzAlloc_Free(&alloc, look.buf);
    File_Close(&file.file);
    Title("Wipeout XL");
    if (ok && !cue[0]) {
        SDL_snprintf(message, messageSize, "There is no disc image (.cue) in %s.", archive);
        ok = 0;
    }
    if (!ok) {
        cue[0] = 0;
        return 0;
    }
    /* the note, last: an extraction cut short is done again */
    {
        FILE *f = fopen(notePath, "wb");

        if (f) {
            fprintf(f, "%s%s\n", note, cue);
            fclose(f);
        }
    }
    {
        char relative[1024];

        SDL_strlcpy(relative, cue, sizeof(relative));
        SDL_snprintf(cue, cueSize, "%s%s", folder, relative);
    }
    SDL_SetAtomicInt(&progress, 1000);
    return 1;
}
