/* The original Wipeout's tracks, raced in Wipeout XL (the debug console's Files tab picks the player's copy
   of the original: its PlayStation disc image, a .cue, or a folder of its data, the disc's WIPEOUT folder).

   XL loads a track the way the original did, from the same kinds of files: XL's track.wad is the original's
   track.trv, .trf, .trs and .vew (vertices, faces, sections, view lists), library.cmp and .ttf (the track's
   textures and tiles), scene.cmp, sky.cmp, scene.prm and sky.prm, one after the other, in the same formats
   (XL's sections have two more bytes, bump and snow, where the original's records had padding). Besides
   the wad XL reads the faces' textures and flags again from track.tex, and the arcade's checkpoints, with
   their bonus seconds, from cpoint<class>.chk. So an original track is raced as a guest in the slot of
   Talon's Reach (folder 1): while it is chosen, the game's reads of that track's files are answered with
   the files made here from the original's (the game's file layer asks PortOriginalFileInfo and
   PortOriginalRead first), and the game side (game/src/portorig.c) gives the slot the guest's start line
   and sky. Where the original has no view lists (some copies of its data leave track.vew out), each
   section sees the sections around and ahead of it; the far view draws the rest. */
#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

#include "discimage.h"
#include "original.h"
#include "platform.h"
#include "saturn.h"
#include "settings.h"
#include "psx/disc.h"

#define SECTION_BYTES 156 /* a track section record (the original's and XL's alike) */
#define FACE_BYTES 20

/* The seconds each of a guest's gates adds to the clock (the start line's, then those a third and two thirds round
   the lap; see MakeCheckpoints): measured with tools/checkpoint_times.py, which times XL's autopilot from each gate
   to the next and gives half as long again, as XL's own tracks give their gates (half as long again as the
   autopilot's lap, about, at Venom and Rapier). */
typedef struct GateSeconds {
    int seconds[3];
} GateSeconds;

/* The original's circuits, each a Venom and a Rapier track (the folders track01 .. track14), with the
   start line's section, the sky's height and the gates' seconds. */
static const struct {
    int folder;
    const char *name;
    int shipClass; /* XL's: 1 Venom, 2 Rapier */
    int startLine;
    int skyY;
    const char *model; /* its circuit's model in the original's common/track.prm (its track menu's) */
    GateSeconds gates;
} originals[] = {
    {2, "ALTIMA VII", 1, 27, -2520, "altima", {{27, 29, 28}}},
    {3, "ALTIMA VII", 2, 27, -1930, "altima", {{24, 23, 23}}},
    {4, "KARBONIS V", 1, 16, -5000, "karbonis", {{16, 19, 18}}},
    {5, "KARBONIS V", 2, 16, -5000, "karbonis", {{12, 15, 15}}},
    {1, "TERRAMAX", 1, 27, -820, "terramax", {{16, 18, 18}}},
    {6, "TERRAMAX", 2, 27, 0, "terramax", {{14, 15, 15}}},
    {12, "KORODERA", 1, 16, -2120, "korrodera", {{28, 29, 27}}},
    {7, "KORODERA", 2, 16, -2260, "korrodera", {{24, 25, 23}}},
    {8, "ARRIDOS IV", 1, 16, -40, "arridos", {{25, 26, 23}}},
    {11, "ARRIDOS IV", 2, 16, -240, "arridos", {{20, 20, 19}}},
    {9, "SILVERSTREAM", 1, 16, -2700, "silverstream", {{20, 8, 31}}},
    {13, "SILVERSTREAM", 2, 16, -2700, "silverstream", {{18, 7, 24}}},
    {10, "FIRESTAR", 1, 27, 0, "firestar", {{23, 23, 21}}},
    {14, "FIRESTAR", 2, 27, 0, "firestar", {{19, 18, 17}}},
};
#define ORIGINALS ((int)SDL_arraysize(originals))

static char source[1024];       /* the disc image or the folder */
static Disc *disc;              /* when the source is a disc image */
static char folder[1100];       /* when it is a folder: the one holding track01 .. track14 */
static int present[ORIGINALS];  /* the tracks the source has */
static int presentCount;

/* Wipeout 64 (the player's cartridge image): each track a wad of the same files, its scenery in three
   (common, and for one player or several), and after it a copy of its faces' textures and flags. */
#define MAX_WADS 16
static uint8_t *rom;            /* the cartridge, in its own (big-endian) byte order */
static size_t romSize;
static char romSource[1024];
static size_t wads[MAX_WADS];   /* where each track's wad starts */
static int wadCount;
static size_t frontEnd;         /* the wad of its menus' files (frntEnd.cmp, models.prm...), 0 when not found */
/* Its tracks in the cartridge's order (its own track list's, each entry with its wad's place), and their models in
   its menus' models.prm */
static const char *const n64Names[] = {"KLIES BRIDGE", "QORON IV", "SOKANA", "DYRONESS", "MACHAON II", "TERAFUMOS",
                                       "VELOCITAR"};
static const char *const n64Models[] = {"klies", "qoron", "sokana", "dyroness", "machaon", "terafumos", "velocitar"};
static const GateSeconds n64Gates[] = {{{12, 12, 13}}, {{18, 19, 18}}, {{18, 21, 20}}, {{12, 15, 15}},
                                       {{18, 18, 20}}, {{16, 15, 17}}, {{17, 19, 19}}}; /* (raced at Rapier) */

/* The guests the class menu offers: the original's tracks the source has, then Wipeout 64's. */
enum { FROM_ORIGINAL, FROM_64 };
typedef struct Guest {
    int from;
    int track;                  /* an index of originals[], or of wads[] */
    const char *name;
    int shipClass;              /* XL's: 1 Venom, 2 Rapier */
    int startLine;              /* in XL's section numbers */
    int skyY;
} Guest;
static Guest guests[ORIGINALS + MAX_WADS];
static int guestCount;
static int chosen = -1;         /* the track raced, an index of guests[], or -1 */

/* The files made for the chosen track, as the game asks for them in Talon's Reach's folder. */
enum { FILE_WAD, FILE_TEX, FILE_CHK0, FILE_CHK1, FILE_CHK2, FILE_CHK3, FILES };
static const char *const fileNames[FILES] = {"track.wad", "track.tex", "cpoint0.chk", "cpoint1.chk", "cpoint2.chk",
                                             "cpoint3.chk"};
static uint8_t *files[FILES];
static size_t fileSizes[FILES];
/* The original's track models (its common/track.prm), asked for as the menus' MODELS_NAME in XL's common
   folder: read at the first ask, read at the virtual sector after the guest's files. */
#define MODELS_NAME "\\origtrk.prm"
static uint8_t *modelsFile;
static size_t modelsSize;
/* ... and its track menu's pictures of its circuits (its textures/track.cmp), asked for as PICTURES_NAME: read at
   the virtual sector after the models */
#define PICTURES_NAME "\\origpics.cmp"
static uint8_t *picturesFile;
static size_t picturesSize;
static int builtFor = -1;
static int libraryTextures;     /* the TIMs of the built track's library.cmp (its surface's textures) */
static void ListGuests(void);
#define VIRTUAL_LOC 0x40000000  /* the sectors the files are read at: beyond any disc */
#define VIRTUAL_SPAN 0x10000    /* ... 128 MB apart */

/* ---- reading the original's files ---- */

static uint8_t *ReadOriginal(int folderNumber, const char *name, size_t *size); /* (folder 0 common, -1 textures) */

/* The original's track menu models (common/track.prm) in XL's model format. That file has flat and gouraud quads
   only (F4, G4), in a record of their own: the type, the four corners, two bytes left over from the tool that
   wrote it, and each colour as eight bytes (four of nothing, then red, green, blue); after each the two
   PlayStation packets its menu drew it with (double-buffered, left over too). They become XL's F4 and G4 (type,
   flag, corners, colours). NULL when the file is not one of those. */
static uint8_t *OriginalModels(size_t *size)
{
    /* by type (3 F4, 7 G4): the menu file's record (with its packets) and XL's, the colours */
    static const int record[9] = {0, 0, 0, 16 + 2 * 24, 0, 0, 0, 28 + 2 * 36, 0};
    static const int xlRecord[9] = {0, 0, 0, 16, 0, 0, 0, 28, 0};
    static const int colours[9] = {0, 0, 0, 1, 0, 0, 0, 4, 0};
    size_t length, in = 0, out = 0;
    uint8_t *file = ReadOriginal(0, "track.prm", &length), *models;

    if (!file || !(models = SDL_malloc(length))) {
        SDL_free(file);
        return NULL;
    }
    while (in + 144 <= length) {
        /* the object and its skeleton (144 bytes), its vertices and normals, its primitives (big-endian) */
        int vertices = (int16_t)(file[in + 16] << 8 | file[in + 17]);
        int normals = (int16_t)(file[in + 24] << 8 | file[in + 25]);
        int primitives = (int16_t)(file[in + 32] << 8 | file[in + 33]), i;
        size_t fixed = 144 + (size_t)SDL_max(vertices, 0) * 8 + (size_t)SDL_max(normals, 0) * 8;

        if (vertices < 0 || normals < 0 || primitives < 0 || in + fixed > length) {
            break;
        }
        memcpy(models + out, file + in, fixed);
        in += fixed;
        out += fixed;
        for (i = 0; i < primitives; i++) {
            int type = in + 2 <= length ? (file[in] << 8 | file[in + 1]) : 0, k;
            uint8_t *to = models + out;

            if (type < 1 || type > 8 || !record[type] || in + (size_t)record[type] > length) {
                SDL_free(file);
                SDL_free(models);
                return NULL;
            }
            memset(to, 0, (size_t)xlRecord[type]);
            memcpy(to, file + in, 2);              /* the type (the flag 0: drawn from either side) */
            memcpy(to + 4, file + in + 2, 8);      /* the corners */
            for (k = 0; k < colours[type]; k++) {
                memcpy(to + 12 + 4 * k, file + in + 16 + 8 * k, 3); /* red, green, blue */
            }
            in += (size_t)record[type];
            out += (size_t)xlRecord[type];
        }
    }
    SDL_free(file);
    if (in != length) {
        SDL_free(models);
        return NULL;
    }
    *size = out;
    return models;
}

static const uint8_t *WadPart(size_t at, const char *name, size_t *size);

/* Wipeout 64's track models, from its menus' models.prm (XL's format already, flat and gouraud shaded), each made
   as large as XL's own track models look in the menu (its are of all sizes): its widest 488 units, XL's 650 at the
   depth they are drawn, 1800, against the guests' 1350. */
static uint8_t *Wipeout64Models(size_t *size)
{
    size_t length, in = 0, out = 0;
    const uint8_t *file = frontEnd ? WadPart(frontEnd, "models.prm", &length) : NULL;
    uint8_t *models;

    if (!file || !(models = SDL_malloc(length))) {
        return NULL;
    }
    while (in + 144 <= length) {
        int vertices = (int16_t)(file[in + 16] << 8 | file[in + 17]);
        int normals = (int16_t)(file[in + 24] << 8 | file[in + 25]);
        int primitives = (int16_t)(file[in + 32] << 8 | file[in + 33]), i, k, wanted = 0, widest = 1;
        size_t at = in + 144 + (size_t)SDL_max(vertices, 0) * 8 + (size_t)SDL_max(normals, 0) * 8;

        if (vertices < 0 || normals < 0 || primitives < 0 || at > length) {
            break;
        }
        for (i = 0; i < primitives && at + 2 <= length; i++) {
            /* (the record sizes of XL's primitives: F3 FT3 F4 FT4 G3 GT3 G4 GT4, sprites, lit ones, splines, lights) */
            static const int sizes[24] = {0, 16, 28, 16, 32, 24, 36, 28, 44, 0, 16, 16,
                                          16, 28, 20, 32, 28, 40, 36, 52, 56, 16, 28, 40};
            int type = file[at] << 8 | file[at + 1];

            if (type < 1 || type > 23 || !sizes[type]) {
                SDL_free(models);
                return NULL;
            }
            at += (size_t)sizes[type];
        }
        for (k = 0; k < (int)SDL_arraysize(n64Models); k++) {
            wanted |= !strncmp((const char *)file + in, n64Models[k], 16);
        }
        if (wanted && at <= length) {
            uint8_t *object = models + out;
            int32_t extent;

            memcpy(object, file + in, at - in);
            for (i = 0; i < vertices; i++) {
                for (k = 0; k < 3; k += 2) {
                    int v = (int16_t)(object[144 + i * 8 + k * 2] << 8 | object[144 + i * 8 + k * 2 + 1]);
                    widest = SDL_max(widest, SDL_abs(v));
                }
            }
            for (i = 0; i < vertices * 3; i++) {
                uint8_t *p = object + 144 + (i / 3) * 8 + (i % 3) * 2;
                int v = (int16_t)(p[0] << 8 | p[1]) * 488 / widest;

                p[0] = (uint8_t)(v >> 8);
                p[1] = (uint8_t)v;
            }
            extent = (int32_t)((uint32_t)object[52] << 24 | object[53] << 16 | object[54] << 8 | object[55]);
            extent = (int32_t)((int64_t)extent * 488 / widest);
            object[52] = (uint8_t)(extent >> 24), object[53] = (uint8_t)(extent >> 16);
            object[54] = (uint8_t)(extent >> 8), object[55] = (uint8_t)extent;
            out += at - in;
        }
        in = at;
    }
    *size = out;
    return out ? models : (SDL_free(models), NULL);
}

/* The menus' guest track models (MODELS_NAME): the original's, then Wipeout 64's. */
static uint8_t *MenuModels(size_t *size)
{
    size_t originalSize = 0, n64Size = 0;
    uint8_t *original = presentCount ? OriginalModels(&originalSize) : NULL, *n64 = Wipeout64Models(&n64Size), *both;

    if (!original || !n64) {
        *size = original ? originalSize : n64Size;
        return original ? original : n64;
    }
    both = SDL_malloc(originalSize + n64Size);
    memcpy(both, original, originalSize);
    memcpy(both + originalSize, n64, n64Size);
    SDL_free(original);
    SDL_free(n64);
    *size = originalSize + n64Size;
    return both;
}

static uint8_t *ReadOriginal(int folderNumber, const char *name, size_t *size)
{
    char path[1300];
    void *data;

    if (disc) {
        int32_t lba;
        uint32_t length;

        if (folderNumber > 0) {
            SDL_snprintf(path, sizeof(path), "\\WIPEOUT\\TRACK%02d\\%s", folderNumber, name);
        } else {
            /* (folder 0: the common files; -1: the textures) */
            SDL_snprintf(path, sizeof(path), "\\WIPEOUT\\%s\\%s", folderNumber ? "TEXTURES" : "COMMON", name);
        }
        for (char *c = path; *c; c++) {
            *c = (char)SDL_toupper((unsigned char)*c);
        }
        if (!DiscFindFile(disc, path, &lba, &length)) {
            return NULL;
        }
        data = SDL_malloc(((size_t)length + 2047) / 2048 * 2048 + 1);
        if (!data || !DiscReadData(disc, lba, (int)((length + 2047) / 2048), data)) {
            SDL_free(data);
            return NULL;
        }
        *size = length;
        return data;
    }
    /* a folder: the names as the disc has them or in lower case */
    if (folderNumber > 0) {
        SDL_snprintf(path, sizeof(path), "%s/track%02d/%s", folder, folderNumber, name);
    } else {
        SDL_snprintf(path, sizeof(path), "%s/%s/%s", folder, folderNumber ? "textures" : "common", name);
    }
    if ((data = SDL_LoadFile(path, size))) {
        return data;
    }
    if (folderNumber > 0) {
        SDL_snprintf(path, sizeof(path), "%s/TRACK%02d/%s", folder, folderNumber, name);
    } else {
        SDL_snprintf(path, sizeof(path), "%s/%s/%s", folder, folderNumber ? "TEXTURES" : "COMMON", name);
    }
    for (char *c = path + strlen(folder); *c; c++) {
        *c = (char)SDL_toupper((unsigned char)*c);
    }
    return SDL_LoadFile(path, size);
}

static int HasTrack(int folderNumber)
{
    size_t size;
    uint8_t *data = ReadOriginal(folderNumber, "track.trs", &size);

    SDL_free(data);
    return data != NULL;
}

void PortOriginalClose(void)
{
    if (disc) {
        DiscClose(disc);
        disc = NULL;
    }
    source[0] = folder[0] = 0;
    presentCount = 0;
    memset(present, 0, sizeof(present));
    SDL_free(modelsFile);
    modelsFile = NULL;
    modelsSize = 0;
    SDL_free(picturesFile);
    picturesFile = NULL;
    picturesSize = 0;
    ListGuests();
}

int PortOriginalOpen(const char *path, char *message, int messageSize)
{
    SDL_PathInfo info;
    char error[256];
    int i;

    PortOriginalClose();
    if (!path || !path[0]) {
        SDL_snprintf(message, (size_t)messageSize, "No copy of the original Wipeout chosen.");
        return 0;
    }
    if (!SDL_GetPathInfo(path, &info)) {
        SDL_snprintf(message, (size_t)messageSize, "%s is not there.", path);
        return 0;
    }
    if (info.type == SDL_PATHTYPE_DIRECTORY) {
        /* the WIPEOUT folder itself, or the folder holding it */
        static const char *const inside[] = {"", "/wipeout", "/WIPEOUT"};

        for (i = 0; i < (int)SDL_arraysize(inside); i++) {
            SDL_snprintf(folder, sizeof(folder), "%s%s", path, inside[i]);
            if (HasTrack(1) || HasTrack(2)) {
                break;
            }
        }
        if (i == (int)SDL_arraysize(inside)) {
            folder[0] = 0;
        }
    } else {
        char cue[1200];

        /* (an archive or a .bin: its .cue, discimage.c) */
        if (!PortDiscImage(path, cue, sizeof(cue), message, (size_t)messageSize)) {
            return 0;
        }
        disc = DiscOpen(cue, error, sizeof(error));
        if (!disc) {
            SDL_snprintf(message, (size_t)messageSize, "%s", error);
            return 0;
        }
    }
    for (i = 0; i < ORIGINALS; i++) {
        present[i] = (disc || folder[0]) && HasTrack(originals[i].folder);
        presentCount += present[i];
    }
    if (!presentCount) {
        SDL_snprintf(message, (size_t)messageSize,
                     "No tracks of the original Wipeout there (a folder WIPEOUT with TRACK01 .. TRACK14 is wanted).");
        PortOriginalClose();
        return 0;
    }
    SDL_strlcpy(source, path, sizeof(source));
    ListGuests();
    SDL_snprintf(message, (size_t)messageSize, "%d of the original's %d tracks found.", presentCount, ORIGINALS);
    return presentCount;
}

const char *PortOriginalSource(void)
{
    return source;
}

/* ---- the tracks, for the game ---- */

static void Wipeout64Guests(void);

/* The guests: the original's tracks the source has, in the original's order, then Wipeout 64's. */
static void ListGuests(void)
{
    int i;

    guestCount = 0;
    chosen = -1;
    builtFor = -1;
    SDL_free(modelsFile); /* (made again from the sources now in use, at the next ask) */
    modelsFile = NULL;
    SDL_free(picturesFile);
    picturesFile = NULL;
    for (i = 0; i < ORIGINALS; i++) {
        if (present[i]) {
            Guest *g = &guests[guestCount++];

            g->from = FROM_ORIGINAL;
            g->track = i;
            g->name = originals[i].name;
            g->shipClass = originals[i].shipClass;
            g->startLine = originals[i].startLine;
            g->skyY = originals[i].skyY;
        }
    }
    Wipeout64Guests();
}

/* The settings' copies (or the tests': WIPEOUT_ORIGINAL, WIPEOUT_64; their hidden runs read no settings),
   opened the first time the game asks. */
static void OpenSettings(void)
{
    static int tried;
    char message[256];
    const char *path;

    if (tried) {
        return;
    }
    tried = 1;
    path = SDL_getenv("WIPEOUT_ORIGINAL");
    if (!path && !portConfig.hidden) {
        path = portSettings.originalPath;
    }
    if (!source[0] && path && path[0]) {
        PortOriginalOpen(path, message, sizeof(message));
    }
    path = SDL_getenv("WIPEOUT_64");
    if (!path && !portConfig.hidden) {
        path = portSettings.wipeout64Path;
    }
    if (!romSource[0] && path && path[0]) {
        PortWipeout64Open(path, message, sizeof(message));
    }
}

int PortOriginalTrackCount(void)
{
    OpenSettings();
    return guestCount;
}

int PortOriginalFoundCount(void)
{
    OpenSettings();
    return presentCount;
}

const char *PortOriginalTrackName(int index)
{
    return index >= 0 && index < guestCount ? guests[index].name : "";
}

int PortOriginalHasModels(void)
{
    OpenSettings();
    if (!modelsFile && (presentCount || frontEnd)) {
        modelsFile = MenuModels(&modelsSize);
    }
    return modelsFile != NULL;
}

static uint8_t *MakePictures(size_t *size);

int PortOriginalHasPictures(void)
{
    OpenSettings();
    if (!picturesFile && presentCount) {
        picturesFile = MakePictures(&picturesSize);
    }
    return picturesFile != NULL;
}

int PortOriginalTrackPicture(int index)
{
    /* (originals[] is in its circuits' order, each a Venom and a Rapier track, as the pictures are) */
    return index >= 0 && index < guestCount && guests[index].from == FROM_ORIGINAL ? guests[index].track / 2 : -1;
}

const char *PortOriginalTrackModel(int index)
{
    if (index < 0 || index >= guestCount) {
        return NULL;
    }
    if (guests[index].from == FROM_64) {
        return guests[index].track < (int)SDL_arraysize(n64Models) ? n64Models[guests[index].track] : NULL;
    }
    return originals[guests[index].track].model;
}

const char *PortOriginalTrackGame(int index)
{
    return index >= 0 && index < guestCount && guests[index].from == FROM_64 ? "WIPEOUT 64" : "THE ORIGINAL WIPEOUT";
}

int PortOriginalTrackClass(int index)
{
    return index >= 0 && index < guestCount ? guests[index].shipClass : 1;
}

void PortOriginalTrackSelect(int index)
{
    chosen = index >= 0 && index < guestCount ? index : -1;
}

int PortOriginalTrack(void)
{
    return chosen;
}

int PortOriginalTrackIs64(void)
{
    return chosen >= 0 && guests[chosen].from == FROM_64;
}

int PortOriginalStartLine(void)
{
    return chosen < 0 ? 0 : guests[chosen].startLine;
}

int PortOriginalLibraryTextures(void)
{
    return chosen >= 0 && builtFor == chosen ? libraryTextures : 0;
}

int PortOriginalSkyY(void)
{
    return chosen < 0 ? 0 : guests[chosen].skyY;
}

/* ---- XL's files from the original's ---- */

static uint16_t Be16(const uint8_t *p)
{
    return (uint16_t)(p[0] << 8 | p[1]);
}

static void PutBe16(uint8_t *p, int v)
{
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)v;
}

/* View lists where the original's track.vew is missing: for each detail level (near, middle, far) the runs
   (first section, count) of sections in file order around and ahead of each section, the same for every
   direction the camera may face, as many as XL's own lists hold (four sections near, the most its
   subdivision of near faces has room for, four in the middle); the section records get the lists'
   lengths. */
static uint8_t *MakeViewLists(uint8_t *sections, int count, size_t *size)
{
    static const int from[3] = {-1, 3, -8}, to[3] = {3, 7, 45};
    uint8_t *out = SDL_malloc((size_t)count * 3 * 5 * 6 * 2);
    size_t at = 0;
    int i, level, direction;

    for (i = 0; i < count; i++) {
        for (level = 0; level < 3; level++) {
            int runs[6], runCount = 0, first = ((i + from[level]) % count + count) % count;
            int length = SDL_min(to[level] - from[level], count);

            if (level == 2) {
                /* far: behind (up to the near ones) and ahead (from the middle ones on) */
                int behind = ((i - 8) % count + count) % count, ahead = (i + 7) % count;

                runs[runCount++] = behind;
                runs[runCount++] = SDL_min(7, count - behind);
                first = ahead;
                length = SDL_min(38, count);
            }
            while (length > 0 && runCount < 6) {
                int run = SDL_min(length, count - first);

                runs[runCount++] = first;
                runs[runCount++] = run;
                length -= run;
                first = 0;
            }
            for (direction = 0; direction < 5; direction++) {
                int k;

                for (k = 0; k < runCount; k++) {
                    PutBe16(out + at, runs[k]);
                    at += 2;
                }
                /* north, south, east, west and all: counts at 96, 102, 108, 114 and 120, one per level */
                PutBe16(sections + (size_t)i * SECTION_BYTES + 96 + direction * 6 + level * 2, runCount);
            }
        }
    }
    *size = at;
    return out;
}

/* The arcade's checkpoints (gates): the start line, then a third and two thirds round the lap (NumberSections). A
   gate is a section whose track faces say CHECKPOINT (track.tex); its cell gives the seconds it adds to the clock
   (which starts with the class's own time), the track's measured ones (GateSeconds). The same for every class: a
   guest is raced at its own. */
static int gates[3];        /* the gates' section numbers */
static int mainSections;    /* the sections of the main loop (a lap) */

static uint8_t *MakeCheckpoints(size_t *size)
{
    const Guest *guest = &guests[chosen];
    static const GateSeconds unknown = {{18, 18, 18}}; /* (a cartridge with more tracks than Wipeout 64's own) */
    const int *seconds = guest->from != FROM_64 ? originals[guest->track].gates.seconds
                         : guest->track < (int)SDL_arraysize(n64Gates) ? n64Gates[guest->track].seconds
                         : unknown.seconds;
    uint8_t *out = SDL_calloc(1, 36);
    int i;

    for (i = 0; i < 6; i++) {
        uint8_t *cell = out + i * 6;
        /* (an unused cell has no seconds: the checkpoint droids take a cell with seconds for a gate) */
        int section = i < 3 ? gates[i] : -1, bonus = i < 3 ? seconds[i] : 0;

        cell[0] = (uint8_t)section;
        cell[1] = (uint8_t)(section >> 8);
        cell[2] = (uint8_t)bonus;
        cell[3] = (uint8_t)(bonus >> 8);
    }
    *size = 36;
    return out;
}

static void FreeFiles(void)
{
    int i;

    for (i = 0; i < FILES; i++) {
        SDL_free(files[i]);
        files[i] = NULL;
        fileSizes[i] = 0;
    }
    builtFor = -1;
    libraryTextures = 0;
}

static int32_t Be32(const uint8_t *p)
{
    return (int32_t)((uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]);
}

/* The sections' numbers, as XL gives them when it loads a track (InitWeaponGrids; the original's files
   have none of their own: the original numbered its sections when it loaded them, differently): along the
   main loop from the first section, and where a branch starts (its first section says JunctionStart) the
   branch's sections, then on along the main loop after them. Then the gates: the start line and a third and
   two thirds of the main loop on, each moved to the nearest section every way round the lap goes through, two
   sections or more from where ways part or meet. A gate on one of two ways (XL's tracks have none) gave nothing to
   a ship that took the other, and one where they meet nothing to a ship that went from the way it came along to
   the section after it in one step. */
#define FIELD32(s, at) Be32(sections + (size_t)(s) * SECTION_BYTES + (at))
#define FLAGS(s) Be16(sections + (size_t)(s) * SECTION_BYTES + 150)

/* numbers[] for each section; returns the length of the main loop (in sections). */
static int Number(const uint8_t *sections, int count, int *numbers)
{
    int section = 0, number = 0, guard, i, main = 0;

    for (i = 0; i < count; i++) {
        numbers[i] = 0;
    }
    for (guard = 0; guard < count * 2; guard++) {
        int junction = FIELD32(section, 0);

        numbers[section] = number;
        main++;
        if (junction >= 0 && junction < count && (FLAGS(junction) & 16)) {
            int original = section, branch = junction, k;

            for (k = 0; k < count; k++) {
                numbers[branch] = ++number;
                branch = FIELD32(branch, 8);
                if (branch < 0 || branch >= count || FIELD32(branch, 0) != -1) {
                    break;
                }
            }
            section = FIELD32(original, 8);
            if (section < 0 || section >= count) {
                break;
            }
            numbers[section] = ++number;
            main++;
            if (section == 0) {
                break;
            }
        }
        section = FIELD32(section, 8);
        number++;
        if (section <= 0 || section >= count) {
            break;
        }
    }
    return main;
}

/* Whether every way round the lap from `from` back to it goes through `section`: the ways are the sections' next
   ones and their junctions (where a way parts or meets another). */
static int OnEveryWay(const uint8_t *sections, int count, int from, int section)
{
    int *queue = SDL_malloc(sizeof(int) * (size_t)count * 2);
    uint8_t *seen = SDL_calloc((size_t)count, 1);
    int head = 0, tail = 0, back = 0;

    queue[tail++] = from;
    seen[from] = 1;
    while (head < tail && !back) {
        int at = queue[head++], next[2], k;

        next[0] = FIELD32(at, 8);
        next[1] = FIELD32(at, 0);
        for (k = 0; k < 2; k++) {
            if (next[k] == from) {
                back = 1;
            } else if (next[k] >= 0 && next[k] < count && next[k] != section && !seen[next[k]]) {
                seen[next[k]] = 1;
                queue[tail++] = next[k];
            }
        }
    }
    SDL_free(queue);
    SDL_free(seen);
    return !back;
}

/* Whether `section` and the two before and after it are on one way: no junction, nor a way's start or end. */
static int Plain(const uint8_t *sections, int count, int section)
{
    int before = section, after = section, k;

    for (k = 0; k < 3; k++) {
        if (before < 0 || before >= count || after < 0 || after >= count || FIELD32(before, 0) != -1 ||
            FIELD32(after, 0) != -1 || (FLAGS(before) & 0x18) || (FLAGS(after) & 0x18)) {
            return 0;
        }
        before = FIELD32(before, 4);
        after = FIELD32(after, 8);
    }
    return 1;
}

static int NumberSections(uint8_t *sections, int count)
{
    int *numbers = SDL_malloc(sizeof(int) * (size_t)(count ? count : 1));
    int section, start = -1, steps, i, guard;

    mainSections = Number(sections, count, numbers);
    for (i = 0; i < count; i++) {
        PutBe16(sections + (size_t)i * SECTION_BYTES + 152, numbers[i]);
    }
    /* the start line on the main loop, and the gates along it */
    for (section = 0, guard = 0; guard < mainSections; guard++, section = FIELD32(section, 8)) {
        if (numbers[section] == guests[chosen].startLine) {
            start = section;
            break;
        }
    }
    if (start >= 0 && mainSections >= 3) {
        int at[3];

        for (i = 0; i < 3; i++) {
            section = start;
            for (steps = 0; steps < mainSections * i / 3; steps++) {
                section = FIELD32(section, 8);
            }
            at[i] = section;
        }
        for (i = 2; i >= 0; i--) {
            /* (the start line's, judged from the next gate on, which every way goes through) */
            int from = i ? start : at[1];

            int ahead = at[i], behind = at[i];

            for (guard = 0; guard < mainSections / 3; guard++) {
                if (Plain(sections, count, ahead) && OnEveryWay(sections, count, from, ahead)) {
                    at[i] = ahead;
                    break;
                }
                if (i && Plain(sections, count, behind) && OnEveryWay(sections, count, from, behind)) {
                    at[i] = behind; /* (the start line's only ahead: it is where the race starts) */
                    break;
                }
                ahead = FIELD32(ahead, 8);
                behind = FIELD32(behind, 4);
                if (ahead < 0 || ahead >= count || behind < 0 || behind >= count) {
                    break;
                }
            }
            gates[i] = numbers[at[i]];
        }
    }
    SDL_free(numbers);
    return start >= 0 && mainSections >= 3;
}

static uint8_t *Relocate(const uint8_t *cmp, size_t length, int *paletteSlot, size_t *size);
static int Wipeout64Parts(int index, uint8_t **parts, size_t *sizes);
int PortPrmShiftTextures(char *file, int length, int shift); /* (game/src/port_widen.c) */

static int CompareInts(const void *a, const void *b)
{
    return *(const int *)a - *(const int *)b;
}

static int Build(void)
{
    static const char *const wadParts[] = {"track.trv", "track.trf", "track.trs", "track.vew", "library.cmp",
                                           "library.ttf", "scene.cmp", "sky.cmp", "scene.prm", "sky.prm"};
    enum { PARTS = (int)SDL_arraysize(wadParts) };
    uint8_t *parts[PARTS] = {0};
    size_t sizes[PARTS] = {0}, total;
    const Guest *guest = &guests[chosen];
    int from64 = guest->from == FROM_64;
    int folderNumber = from64 ? 0 : originals[guest->track].folder, i, sectionCount, ok = 1;
    uint8_t *wad, *at;

    if (builtFor == chosen) {
        return 1;
    }
    FreeFiles();
    if (from64) {
        ok = Wipeout64Parts(guest->track, parts, sizes);
    } else {
        for (i = 0; i < PARTS; i++) {
            parts[i] = ReadOriginal(folderNumber, wadParts[i], &sizes[i]);
            if (!parts[i] && i != 3) {
                fprintf(stderr, "original Wipeout: track%02d has no %s\n", folderNumber, wadParts[i]);
                ok = 0;
            }
        }
    }
    sectionCount = ok ? (int)(sizes[2] / SECTION_BYTES) : 0;
    if (ok && !NumberSections(parts[2], sectionCount)) {
        fprintf(stderr, "guest track %s: its sections make no loop\n", guest->name);
        ok = 0;
    }
    if (ok && !parts[3]) {
        parts[3] = MakeViewLists(parts[2], sectionCount, &sizes[3]);
    }
    if (ok && !from64) {
        int paletteSlot = 0;

        for (i = 6; i <= 7; i++) {
            size_t size;
            uint8_t *moved = Relocate(parts[i], sizes[i], &paletteSlot, &size);

            if (moved) {
                SDL_free(parts[i]);
                parts[i] = moved;
                sizes[i] = size;
            } else {
                fprintf(stderr, "original Wipeout: track%02d's %s is not a texture sequence\n", folderNumber, wadParts[i]);
            }
        }
    }
    if (ok) {
        /* XL's two bytes after the original's section record (bump, snow): none (Wipeout 64's records are
           XL's) */
        for (i = 0; i < sectionCount && !from64; i++) {
            parts[2][(size_t)i * SECTION_BYTES + 154] = 0;
            parts[2][(size_t)i * SECTION_BYTES + 155] = 0;
        }
        /* the wad: a count, the entries (name, size twice, a flag), the files */
        total = 2 + PARTS * 25;
        for (i = 0; i < PARTS; i++) {
            total += sizes[i];
        }
        wad = at = SDL_calloc(1, total);
        at[0] = PARTS;
        at += 2;
        for (i = 0; i < PARTS; i++) {
            SDL_strlcpy((char *)at, wadParts[i], 16);
            memcpy(at + 16, &(int32_t){(int32_t)sizes[i]}, 4);
            memcpy(at + 20, &(int32_t){(int32_t)sizes[i]}, 4);
            at += 25;
        }
        for (i = 0; i < PARTS; i++) {
            memcpy(at, parts[i], sizes[i]);
            at += sizes[i];
        }
        files[FILE_WAD] = wad;
        fileSizes[FILE_WAD] = total;
        /* track.tex: each face's texture and flags, section by section */
        {
            uint8_t *tex = SDL_malloc(sizes[1] / FACE_BYTES * 2 + 1);
            size_t n = 0;

            for (i = 0; i < sectionCount; i++) {
                const uint8_t *s = parts[2] + (size_t)i * SECTION_BYTES;
                int face, first = (int16_t)Be16(s + 142), count = (int16_t)Be16(s + 144);
                int number = (int16_t)Be16(s + 152);
                int gate = number == gates[0] || number == gates[1] || number == gates[2];

                for (face = first; face < first + count && (size_t)(face + 1) * FACE_BYTES <= sizes[1]; face++) {
                    uint8_t flags = parts[1][(size_t)face * FACE_BYTES + 15];

                    if (from64) {
                        flags &= 0x7F; /* (its own gates, which XL's are made in place of; its pits, 0x40, are XL's) */
                    }

                    if (gate && (flags & 1)) {
                        flags |= 0x80; /* CHECKPOINT: on the section's track faces */
                    }
                    tex[n++] = parts[1][(size_t)face * FACE_BYTES + 14];
                    tex[n++] = flags;
                }
            }
            files[FILE_TEX] = tex;
            fileSizes[FILE_TEX] = n;
        }
        for (i = 0; i < 4; i++) {
            files[FILE_CHK0 + i] = MakeCheckpoints(&fileSizes[FILE_CHK0 + i]);
        }
        /* (a .cmp's count of TIMs: its first four bytes, little-endian) */
        libraryTextures = sizes[4] >= 4 ? (int)(parts[4][0] | parts[4][1] << 8 | parts[4][2] << 16 |
                                                (uint32_t)parts[4][3] << 24)
                                        : 0;
        builtFor = chosen;
    }
    for (i = 0; i < PARTS; i++) {
        SDL_free(parts[i]);
    }
    return ok;
}

/* ---- the original's textures in XL's video memory ----

   The original laid out its scene's and sky's textures (positioned TIMs) over the right of video memory's
   upper half, where XL keeps textures of its own for the whole race (the ships', for one): loaded together
   the two would overwrite each other. So the images go to the same places in the second half of video
   memory (platform/psx/gpu.h: rows 512 on, which XL never uses), keeping the original's layout, and their
   palettes of 16 colours to a block of their own where XL's own scenery would be (x 704 to 895 of the upper rows);
   the others (256 colours) to their own places in the second half as well. The track's tiles (library.cmp) are
   placed by the game itself, as XL's. */

static int LzssBit(const uint8_t *in, size_t length, size_t *at, int *mask, int *rack)
{
    int value;

    if (*mask == 0x80) {
        *rack = *at < length ? in[*at] : 0;
        (*at)++;
    }
    value = *rack & *mask;
    *mask >>= 1;
    if (!*mask) {
        *mask = 0x80;
    }
    return value != 0;
}

static int LzssBits(const uint8_t *in, size_t length, size_t *at, int *mask, int *rack, int count)
{
    int value = 0;

    while (count--) {
        value = value << 1 | LzssBit(in, length, at, mask, rack);
    }
    return value;
}

/* The games' compression (13-bit window, 4-bit lengths), into out (size bytes at most). */
static size_t LzssDecompress(const uint8_t *in, size_t length, uint8_t *out, size_t size)
{
    uint8_t window[8192] = {0};
    size_t at = 0, n = 0;
    int position = 1, mask = 0x80, rack = 0, i;

    while (at <= length) {
        if (LzssBit(in, length, &at, &mask, &rack)) {
            int c = LzssBits(in, length, &at, &mask, &rack, 8);

            if (n < size) {
                out[n++] = (uint8_t)c;
            }
            window[position] = (uint8_t)c;
            position = (position + 1) & 8191;
        } else {
            int from = LzssBits(in, length, &at, &mask, &rack, 13), count;

            if (!from) {
                break;
            }
            count = LzssBits(in, length, &at, &mask, &rack, 4) + 2;
            for (i = 0; i <= count; i++) {
                uint8_t c = window[(from + i) & 8191];

                if (n < size) {
                    out[n++] = c;
                }
                window[position] = c;
                position = (position + 1) & 8191;
            }
        }
    }
    return n;
}

/* The same stream without back references: each byte as it is (a flag bit and eight), then the end. */
static uint8_t *LzssStore(const uint8_t *in, size_t length, size_t *size)
{
    uint8_t *out = SDL_calloc(1, length * 9 / 8 + 8);
    size_t bit = 0;
    size_t i;
    int k;

#define PUT(b) (out[bit >> 3] |= (uint8_t)((b) ? 0x80 >> (bit & 7) : 0), bit++)
    for (i = 0; i < length; i++) {
        PUT(1);
        for (k = 7; k >= 0; k--) {
            PUT((in[i] >> k) & 1);
        }
    }
    for (k = 0; k < 14; k++) {
        PUT(0); /* a reference, at window position 0: the end */
    }
#undef PUT
    *size = (bit + 7) / 8;
    return out;
}

static int16_t Le16(const uint8_t *p)
{
    return (int16_t)(p[0] | p[1] << 8);
}

static void PutLe16(uint8_t *p, int v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

/* A texture sequence (.cmp: a count, the TIMs' sizes, the TIMs compressed) with its positioned TIMs' images in
   the second half of video memory and their palettes in the palette block; *paletteSlot counts the block's. */
static uint8_t *Relocate(const uint8_t *cmp, size_t length, int *paletteSlot, size_t *size)
{
    int count, i;
    size_t total = 0, header, at, packedSize;
    uint8_t *tims, *packed, *out;

    if (length < 4) {
        return NULL;
    }
    count = (int)(cmp[0] | cmp[1] << 8 | cmp[2] << 16 | (uint32_t)cmp[3] << 24);
    header = 4 + (size_t)count * 4;
    if (count <= 0 || count > 4096 || header > length) {
        return NULL;
    }
    for (i = 0; i < count; i++) {
        const uint8_t *s = cmp + 4 + i * 4;

        total += (size_t)(s[0] | s[1] << 8 | s[2] << 16 | (uint32_t)s[3] << 24);
    }
    tims = SDL_malloc(total + 1);
    if (!tims || LzssDecompress(cmp + header, length - header, tims, total) != total) {
        SDL_free(tims);
        return NULL;
    }
    for (i = 0, at = 0; i < count; i++) {
        const uint8_t *s = cmp + 4 + i * 4;
        size_t timSize = (size_t)(s[0] | s[1] << 8 | s[2] << 16 | (uint32_t)s[3] << 24);
        uint8_t *tim = tims + at, *image = tim + 8;
        int flags = tim[4];

        if (timSize >= 20) {
            if (flags & 8) {
                /* the palette block, then the image block */
                uint8_t *clut = tim + 8;
                uint32_t clutLength = (uint32_t)(clut[0] | clut[1] << 8 | clut[2] << 16 | (uint32_t)clut[3] << 24);

                image = clut + clutLength;
                if ((size_t)(image - tims) + 12 <= total && (Le16(image + 4) || Le16(image + 6)) &&
                    Le16(clut + 8) == 16 && Le16(clut + 10) == 1 && *paletteSlot < 12 * 16) {
                    PutLe16(clut + 4, 880 - (*paletteSlot % 12) * 16);
                    PutLe16(clut + 6, *paletteSlot / 12);
                    (*paletteSlot)++;
                } else if ((size_t)(image - tims) + 12 <= total && (Le16(image + 4) || Le16(image + 6))) {
                    /* a palette the block has no room for (256 colours; the block full): in the second half too, at
                       its own place there (gpu.h: a palette's row 512 on; XL's HUD has one of 256 where the
                       original put some of its own) */
                    PutLe16(clut + 6, (Le16(clut + 6) & 0x1FF) + 512);
                }
            }
            if ((size_t)(image - tims) + 12 <= total && (Le16(image + 4) || Le16(image + 6))) {
                PutLe16(image + 6, (Le16(image + 6) & 0x1FF) + 512);
            }
        }
        at += timSize;
    }
    packed = LzssStore(tims, total, &packedSize);
    out = SDL_malloc(header + packedSize);
    memcpy(out, cmp, header);
    memcpy(out + header, packed, packedSize);
    SDL_free(packed);
    SDL_free(tims);
    *size = header + packedSize;
    return out;
}

/* The original's pictures of its circuits (textures/track.cmp: seven 128x74 TIMs of 256 colours, in its circuits'
   order) for the class menu, in the second half of video memory where nothing else is during the menus: the
   images three to a texture page from x 640, the palettes below the last. */
static uint32_t Le32(const uint8_t *p);

static uint8_t *MakePictures(size_t *size)
{
    size_t length, header, total = 0, at, packedSize;
    uint8_t *cmp = ReadOriginal(-1, "track.cmp", &length), *tims, *packed, *out;
    int count, i;

    if (!cmp || length < 4) {
        SDL_free(cmp);
        return NULL;
    }
    count = (int)(cmp[0] | cmp[1] << 8 | cmp[2] << 16 | (uint32_t)cmp[3] << 24);
    header = 4 + (size_t)count * 4;
    if (count <= 0 || count > 16 || header > length) {
        SDL_free(cmp);
        return NULL;
    }
    for (i = 0; i < count; i++) {
        total += Le32(cmp + 4 + i * 4);
    }
    tims = SDL_malloc(total + 1);
    if (!tims || LzssDecompress(cmp + header, length - header, tims, total) != total) {
        SDL_free(tims);
        SDL_free(cmp);
        return NULL;
    }
    for (i = 0, at = 0; i < count; i++) {
        size_t timSize = Le32(cmp + 4 + i * 4);
        uint8_t *tim = tims + at, *image;

        /* (an 8-bit TIM: its palette block, then its image block, 64 of video memory's pixels across) */
        if (timSize < 20 || (tim[4] & 0xF) != 9 || 8 + (size_t)Le32(tim + 8) + 12 > timSize) {
            SDL_free(tims);
            SDL_free(cmp);
            return NULL;
        }
        image = tim + 8 + Le32(tim + 8);
        PutLe16(tim + 8 + 4, 768);
        PutLe16(tim + 8 + 6, 600 + i);
        PutLe16(image + 4, 640 + (i / 3) * 64);
        PutLe16(image + 6, 512 + (i % 3) * 74);
        at += timSize;
    }
    packed = LzssStore(tims, total, &packedSize);
    out = SDL_malloc(header + packedSize);
    memcpy(out, cmp, header);
    memcpy(out + header, packed, packedSize);
    SDL_free(packed);
    SDL_free(tims);
    SDL_free(cmp);
    *size = header + packedSize;
    return out;
}

/* ---- the game's file layer ---- */

int PortOriginalFileInfo(const char *pathname, int *loc, int *size)
{
    const char *name;
    int i;

    if (SDL_strcasestr(pathname, MODELS_NAME)) {
        if (!PortOriginalHasModels()) {
            return 0;
        }
        *loc = VIRTUAL_LOC + FILES * VIRTUAL_SPAN;
        *size = (int)modelsSize;
        return 1;
    }
    if (SDL_strcasestr(pathname, PICTURES_NAME)) {
        if (!PortOriginalHasPictures()) {
            return 0;
        }
        *loc = VIRTUAL_LOC + (FILES + 1) * VIRTUAL_SPAN;
        *size = (int)picturesSize;
        return 1;
    }
    if (chosen < 0 || !guestCount) {
        return 0;
    }
    /* Talon's Reach's folder: ...\track01\NAME */
    name = SDL_strcasestr(pathname, "\\track01\\");
    if (!name) {
        return 0;
    }
    name += 9;
    for (i = 0; i < FILES; i++) {
        if (!SDL_strncasecmp(name, fileNames[i], strlen(fileNames[i]))) {
            if (!Build()) {
                return 0;
            }
            *loc = VIRTUAL_LOC + i * VIRTUAL_SPAN;
            *size = (int)fileSizes[i];
            return 1;
        }
    }
    return 0;
}

int PortOriginalRead(int loc, int sectors, void *buffer)
{
    int file = (loc - VIRTUAL_LOC) / VIRTUAL_SPAN, sector = (loc - VIRTUAL_LOC) % VIRTUAL_SPAN;
    size_t from, length;

    const uint8_t *data;
    size_t size;

    if (loc < VIRTUAL_LOC || file > FILES + 1 || (file < FILES && !files[file]) || (file == FILES && !modelsFile) ||
        (file == FILES + 1 && !picturesFile)) {
        return 0;
    }
    data = file == FILES + 1 ? picturesFile : file == FILES ? modelsFile : files[file];
    size = file == FILES + 1 ? picturesSize : file == FILES ? modelsSize : fileSizes[file];
    from = (size_t)sector * 2048;
    length = (size_t)sectors * 2048;
    memset(buffer, 0, length);
    if (from < size) {
        memcpy(buffer, data + from, SDL_min(length, size - from));
    }
    return 1;
}

/* ---- Wipeout 64 ----

   The Nintendo 64 game keeps the PlayStation's formats: each of its tracks is a wad (as XL's track.wad: a count,
   then for each file its name, size twice and a flag, then the files) of the same files as XL's, its sections
   XL's records, its textures compressed TIMs. What differs: the scenery comes in three (sceneCom common to all,
   sceneSin for one player, sceneMul for several), made one here, Com then Sin; the TIMs have no places in video
   memory (the N64 has none), so they get places here, in the second half of video memory, palettes too (rows
   512 on, gpu.h); the track's surface is whole 64x64 tiles where XL draws each tile from 21 pieces (4x4 near,
   2x2 at middle distance, one far), so each tile becomes 21 TIMs, windows onto its one image; and its faces
   mark the starting grid (0x40), behind the start line. */

static const char *const parts64[] = {"track.trv", "track.trf", "track.trs", "track.vew", "library.cmp", "library.ttf",
                                      "sceneSin.cmp", "sceneMul.cmp", "sceneCom.cmp", "sky.cmp", "sceneSin.prm",
                                      "sceneMul.prm", "sceneCom.prm", "sky.prm"};
#define PARTS64 ((int)SDL_arraysize(parts64))

static uint32_t Le32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static void PutLe32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

/* A file of the wad at `at` (NULL when it has none). */
static const uint8_t *WadPart(size_t at, const char *name, size_t *size)
{
    int count, i;
    size_t data;

    if (at + 2 > romSize) {
        return NULL;
    }
    count = rom[at] | rom[at + 1] << 8;
    data = at + 2 + (size_t)count * 25;
    for (i = 0; i < count && data <= romSize; i++) {
        const uint8_t *entry = rom + at + 2 + (size_t)i * 25;
        uint32_t length = Le32(entry + 16);

        if (!strncmp((const char *)entry, name, 16)) {
            if (data + length > romSize) {
                return NULL;
            }
            *size = length;
            return rom + data;
        }
        data += length;
    }
    return NULL;
}

/* A track's wad at `at`: the fourteen files, all there. */
static int IsWad(size_t at)
{
    size_t size;
    int i;

    if (at + 2 > romSize || (rom[at] | rom[at + 1] << 8) != PARTS64) {
        return 0;
    }
    for (i = 0; i < PARTS64; i++) {
        if (strncmp((const char *)rom + at + 2 + (size_t)i * 25, parts64[i], 16) || !WadPart(at, parts64[i], &size)) {
            return 0;
        }
    }
    return 1;
}

static int Wipeout64Track(int index, int *startLine, int *skyY);

static void Wipeout64Guests(void)
{
    int i;

    for (i = 0; i < wadCount && guestCount < (int)SDL_arraysize(guests); i++) {
        Guest *g = &guests[guestCount];

        g->from = FROM_64;
        g->track = i;
        g->name = i < (int)SDL_arraysize(n64Names) ? n64Names[i] : "WIPEOUT 64";
        g->shipClass = 2; /* (Rapier: the pace of Wipeout 64's own races) */
        if (Wipeout64Track(i, &g->startLine, &g->skyY)) {
            guestCount++;
        }
    }
}

void PortWipeout64Close(void)
{
    SDL_free(rom);
    rom = NULL;
    romSize = 0;
    romSource[0] = 0;
    wadCount = 0;
    frontEnd = 0;
    ListGuests();
}

int PortWipeout64Open(const char *path, char *message, int messageSize)
{
    size_t i;

    PortWipeout64Close();
    if (!path || !path[0]) {
        SDL_snprintf(message, (size_t)messageSize, "No Wipeout 64 chosen.");
        return 0;
    }
    rom = SDL_LoadFile(path, &romSize);
    if (!rom || romSize < 0x1000) {
        SDL_snprintf(message, (size_t)messageSize, "%s could not be read.", path);
        PortWipeout64Close();
        return 0;
    }
    /* the cartridge's own byte order (.z64), from the swapped ones of .v64 (in 16 bits) and .n64 (in 32) */
    if (rom[0] == 0x37 && rom[1] == 0x80) {
        for (i = 0; i + 1 < romSize; i += 2) {
            uint8_t t = rom[i];
            rom[i] = rom[i + 1];
            rom[i + 1] = t;
        }
    } else if (rom[0] == 0x40 && rom[1] == 0x12) {
        for (i = 0; i + 3 < romSize; i += 4) {
            uint8_t a = rom[i], b = rom[i + 1];
            rom[i] = rom[i + 3];
            rom[i + 1] = rom[i + 2];
            rom[i + 2] = b;
            rom[i + 3] = a;
        }
    }
    if (rom[0] != 0x80 || rom[1] != 0x37 || SDL_strncasecmp((const char *)rom + 0x20, "WIPEOUT 64", 10)) {
        SDL_snprintf(message, (size_t)messageSize, "That is not Wipeout 64 (a Nintendo 64 cartridge image: .z64, .n64 or .v64).");
        PortWipeout64Close();
        return 0;
    }
    /* the wads: found by their first file's name (the menus' by its own) */
    for (i = 2; i + 12 < romSize; i++) {
        size_t size;

        if (rom[i] == 't' && wadCount < MAX_WADS && !memcmp(rom + i, "track.trv", 10) && IsWad(i - 2)) {
            wads[wadCount++] = i - 2;
        } else if (rom[i] == 'f' && !frontEnd && !memcmp(rom + i, "frntEnd.cmp", 12) && WadPart(i - 2, "models.prm", &size)) {
            frontEnd = i - 2;
        }
    }
    if (!wadCount) {
        SDL_snprintf(message, (size_t)messageSize, "No tracks found in that copy of Wipeout 64.");
        PortWipeout64Close();
        return 0;
    }
    SDL_strlcpy(romSource, path, sizeof(romSource));
    ListGuests();
    SDL_snprintf(message, (size_t)messageSize, "Wipeout 64's %d tracks found.", wadCount);
    return wadCount;
}

const char *PortWipeout64Source(void)
{
    return romSource;
}

int PortWipeout64TrackCount(void)
{
    OpenSettings();
    return wadCount;
}

/* A track's start line (the section after its starting grid, in XL's numbers) and sky height (the middle of
   the track's heights: Wipeout 64's own heights are in its code, not its data). */
static int Wipeout64Track(int index, int *startLine, int *skyY)
{
    size_t trsSize, trfSize, trvSize;
    const uint8_t *sections = WadPart(wads[index], "track.trs", &trsSize);
    const uint8_t *faces = WadPart(wads[index], "track.trf", &trfSize);
    const uint8_t *vertices = WadPart(wads[index], "track.trv", &trvSize);
    int count = (int)(trsSize / SECTION_BYTES), *numbers, *heights, i, found = 0;
    int vertexCount = (int)(trvSize / 16);

    if (!sections || !faces || !vertices || count < 3 || !vertexCount) {
        return 0;
    }
    numbers = SDL_malloc(sizeof(int) * (size_t)count * 2);
    Number(sections, count, numbers);
    /* the grid's sections: those with faces marked 0x40; the line, after the last of them */
    for (i = 0; i < count; i++) {
        int first = (int16_t)Be16(sections + (size_t)i * SECTION_BYTES + 142);
        int n = (int16_t)Be16(sections + (size_t)i * SECTION_BYTES + 144), face, grid = 0;

        for (face = first; face < first + n && (size_t)(face + 1) * FACE_BYTES <= trfSize; face++) {
            grid |= faces[(size_t)face * FACE_BYTES + 15] & 0x40;
        }
        numbers[count + i] = grid != 0;
    }
    *startLine = 0;
    for (i = 0; i < count && !found; i++) {
        int next = FIELD32(i, 8);

        if (numbers[count + i] && next >= 0 && next < count && !numbers[count + next]) {
            *startLine = numbers[next];
            found = 1;
        }
    }
    SDL_free(numbers);
    heights = SDL_malloc(sizeof(int) * (size_t)vertexCount);
    for (i = 0; i < vertexCount; i++) {
        heights[i] = Be32(vertices + (size_t)i * 16 + 4);
    }
    SDL_qsort(heights, (size_t)vertexCount, sizeof(int), CompareInts);
    *skyY = heights[vertexCount / 2];
    SDL_free(heights);
    return found;
}

/* ---- Wipeout 64's textures ---- */

typedef struct TimList {
    uint8_t **tims;
    size_t *sizes;
    int count;
} TimList;

static void TimAdd(TimList *list, uint8_t *tim, size_t size)
{
    list->tims = SDL_realloc(list->tims, sizeof(uint8_t *) * (size_t)(list->count + 1));
    list->sizes = SDL_realloc(list->sizes, sizeof(size_t) * (size_t)(list->count + 1));
    list->tims[list->count] = tim;
    list->sizes[list->count++] = size;
}

static void TimFree(TimList *list)
{
    int i;

    for (i = 0; i < list->count; i++) {
        SDL_free(list->tims[i]);
    }
    SDL_free(list->tims);
    SDL_free(list->sizes);
    memset(list, 0, sizeof(*list));
}

/* A texture sequence's TIMs (each a copy of its own), added to the list. */
static int Unpack(const uint8_t *cmp, size_t length, TimList *list)
{
    int count, i;
    size_t total = 0, header, at = 0;
    uint8_t *all;

    if (length < 4) {
        return 0;
    }
    count = (int)Le32(cmp);
    header = 4 + (size_t)count * 4;
    if (count <= 0 || count > 4096 || header > length) {
        return 0;
    }
    for (i = 0; i < count; i++) {
        total += Le32(cmp + 4 + i * 4);
    }
    all = SDL_malloc(total + 1);
    if (LzssDecompress(cmp + header, length - header, all, total) != total) {
        SDL_free(all);
        return 0;
    }
    for (i = 0; i < count; i++) {
        size_t size = Le32(cmp + 4 + i * 4);
        uint8_t *tim = SDL_malloc(size);

        memcpy(tim, all + at, size);
        TimAdd(list, tim, size);
        at += size;
    }
    SDL_free(all);
    return 1;
}

/* The list as a texture sequence (stored, not compressed). */
static uint8_t *Pack(const TimList *list, size_t *size)
{
    size_t total = 0, header = 4 + (size_t)list->count * 4, packedSize, at = 0;
    uint8_t *all, *packed, *out;
    int i;

    for (i = 0; i < list->count; i++) {
        total += list->sizes[i];
    }
    all = SDL_malloc(total + 1);
    for (i = 0; i < list->count; i++) {
        memcpy(all + at, list->tims[i], list->sizes[i]);
        at += list->sizes[i];
    }
    packed = LzssStore(all, total, &packedSize);
    out = SDL_malloc(header + packedSize);
    PutLe32(out, (uint32_t)list->count);
    for (i = 0; i < list->count; i++) {
        PutLe32(out + 4 + i * 4, (uint32_t)list->sizes[i]);
    }
    memcpy(out + header, packed, packedSize);
    SDL_free(packed);
    SDL_free(all);
    *size = header + packedSize;
    return out;
}

/* Places in the second half of video memory: images page by page (64 pixels of video memory across, 256 rows,
   as a texture page holds them), shelf by shelf; palettes in a block of their own at the bottom right (x 768 on,
   rows 768 on), the 16-colour ones from its top, the 256-colour ones from its bottom. */
typedef struct Layout {
    int page, shelfX, shelfY, shelfH;
    int small, large;           /* palettes placed */
    uint8_t (*palettes)[512];   /* ... their colours, to place each different one once */
    int *paletteX, *paletteY, *paletteSize, paletteCount;
} Layout;
#define PALETTE_X 768
#define PALETTE_Y (512 + 256)
#define IMAGE_PAGES 28          /* (the last four hold the palettes) */

static int PlaceImage(Layout *l, int w, int h, int *x, int *y)
{
    if (w > 64 || h > 256) {
        return 0;
    }
    if (l->shelfX + w > 64) {
        l->shelfY += l->shelfH;
        l->shelfX = l->shelfH = 0;
    }
    if (l->shelfY + h > 256) {
        l->page++;
        l->shelfX = l->shelfY = l->shelfH = 0;
    }
    while (PortSaturnDisc() && l->page % 16 >= 3 && l->page % 16 <= 5) {
        l->page++; /* (x 192-383: the Saturn's ships and other models kept for the whole game, saturn_models.c) */
    }
    if (l->page >= IMAGE_PAGES) {
        return 0;
    }
    *x = (l->page % 16) * 64 + l->shelfX;
    *y = 512 + (l->page / 16) * 256 + l->shelfY;
    l->shelfX += w;
    l->shelfH = SDL_max(l->shelfH, h);
    return 1;
}

static int PlacePalette(Layout *l, const uint8_t *colours, int count, int *x, int *y)
{
    int i;

    for (i = 0; i < l->paletteCount; i++) {
        if (l->paletteSize[i] == count && !memcmp(l->palettes[i], colours, (size_t)count * 2)) {
            *x = l->paletteX[i];
            *y = l->paletteY[i];
            return 1;
        }
    }
    if (count == 16) {
        *x = PALETTE_X + (l->small % 16) * 16;
        *y = PALETTE_Y + l->small / 16;
        l->small++;
    } else {
        *x = PALETTE_X;
        *y = 1023 - l->large;
        l->large++;
    }
    if (PALETTE_Y + (l->small + 15) / 16 > 1023 - l->large + 1) {
        return 0; /* (the block is full) */
    }
    l->palettes = SDL_realloc(l->palettes, sizeof(*l->palettes) * (size_t)(l->paletteCount + 1));
    l->paletteX = SDL_realloc(l->paletteX, sizeof(int) * (size_t)(l->paletteCount + 1));
    l->paletteY = SDL_realloc(l->paletteY, sizeof(int) * (size_t)(l->paletteCount + 1));
    l->paletteSize = SDL_realloc(l->paletteSize, sizeof(int) * (size_t)(l->paletteCount + 1));
    memcpy(l->palettes[l->paletteCount], colours, (size_t)count * 2);
    l->paletteX[l->paletteCount] = *x;
    l->paletteY[l->paletteCount] = *y;
    l->paletteSize[l->paletteCount++] = count;
    return 1;
}

static void LayoutFree(Layout *l)
{
    SDL_free(l->palettes);
    SDL_free(l->paletteX);
    SDL_free(l->paletteY);
    SDL_free(l->paletteSize);
}

/* A TIM's palette and image blocks (4- or 8-bit, with a palette): NULL when it is not such a TIM. */
static uint8_t *TimBlocks(uint8_t *tim, size_t size, uint8_t **image)
{
    uint8_t *clut = tim + 8;
    uint32_t clutLength;

    if (size < 20 || !(tim[4] & 8) || (tim[4] & 7) > 1) {
        return NULL;
    }
    clutLength = Le32(clut);
    if (clutLength < 12 || 8 + (size_t)clutLength + 12 > size) {
        return NULL;
    }
    *image = clut + clutLength;
    if ((size_t)(*image - tim) + 12 + (size_t)Le16(*image + 8) * Le16(*image + 10) * 2 > size) {
        return NULL;
    }
    return clut;
}

/* Each TIM of the list given places for its image and palette. */
static int PlaceTims(Layout *l, TimList *list)
{
    int i, x, y;

    for (i = 0; i < list->count; i++) {
        uint8_t *image, *clut = TimBlocks(list->tims[i], list->sizes[i], &image);

        if (!clut || !PlaceImage(l, Le16(image + 8), Le16(image + 10), &x, &y)) {
            return 0;
        }
        list->tims[i][4] &= 0x0F; /* (the N64's own flags above the mode) */
        PutLe16(image + 4, x);
        PutLe16(image + 6, y);
        if (!PlacePalette(l, clut + 12, Le16(clut + 8), &x, &y)) {
            return 0;
        }
        PutLe16(clut + 4, x);
        PutLe16(clut + 6, y);
    }
    return 1;
}

/* A TIM of part of an image: w x h of video memory's pixels from (fromX, fromY) of the image, at (x, y). */
static uint8_t *Window(const uint8_t *clut, const uint8_t *image, int fromX, int fromY, int w, int h, int x, int y,
                       int flags, size_t *size)
{
    uint32_t clutLength = Le32(clut), imageLength = 12 + (uint32_t)(w * h * 2);
    int imageW = Le16(image + 8), row;
    uint8_t *tim;

    *size = 8 + clutLength + imageLength;
    tim = SDL_calloc(1, *size);
    tim[0] = 0x10;
    tim[4] = (uint8_t)flags;
    memcpy(tim + 8, clut, clutLength);
    PutLe32(tim + 8 + clutLength, imageLength);
    PutLe16(tim + 8 + clutLength + 4, x);
    PutLe16(tim + 8 + clutLength + 6, y);
    PutLe16(tim + 8 + clutLength + 8, w);
    PutLe16(tim + 8 + clutLength + 10, h);
    for (row = 0; row < h; row++) {
        memcpy(tim + 8 + clutLength + 12 + (size_t)row * w * 2, image + 12 + ((size_t)(fromY + row) * imageW + fromX) * 2,
               (size_t)w * 2);
    }
    return tim;
}

/* The track's surface tiles as XL's: for each, 16 near pieces (4x4), 4 middle (2x2) and the whole tile far, all
   windows onto its one image; and the template file (library.ttf) naming them. */
static int MakeLibrary(const uint8_t *cmp, size_t length, Layout *l, uint8_t **library, size_t *librarySize,
                       uint8_t **templates, size_t *templatesSize)
{
    TimList tiles = {0}, pieces = {0};
    int t, k, ok = Unpack(cmp, length, &tiles) && PlaceTims(l, &tiles);

    *templates = SDL_malloc((size_t)(tiles.count ? tiles.count : 1) * 21 * 2);
    for (t = 0; ok && t < tiles.count; t++) {
        uint8_t *image, *clut = TimBlocks(tiles.tims[t], tiles.sizes[t], &image);
        int w = Le16(image + 8), h = Le16(image + 10), x = Le16(image + 4), y = Le16(image + 6);
        size_t size;

        for (k = 0; k < 21; k++) {
            int across = k < 16 ? 4 : k < 20 ? 2 : 1, n = k < 16 ? k : k < 20 ? k - 16 : 0;
            int pw = w / across, ph = h / across, px = (n % across) * pw, py = (n / across) * ph;
            uint8_t *piece;

            if (w % 4 || h % 4) {
                pw = w;
                ph = h;
                px = py = 0;
            }
            piece = Window(clut, image, px, py, pw, ph, x + px, y + py, tiles.tims[t][4], &size);
            TimAdd(&pieces, piece, size); /* (size only once Window has set it: the order a call's arguments
                                             are worked out in is the compiler's, and GCC's is the other way) */
            PutBe16(*templates + ((size_t)t * 21 + k) * 2, t * 21 + k);
        }
    }
    *templatesSize = (size_t)tiles.count * 21 * 2;
    *library = ok ? Pack(&pieces, librarySize) : NULL;
    TimFree(&tiles);
    TimFree(&pieces);
    return ok;
}

/* XL's ten files of a Wipeout 64 track. */
static int Wipeout64Parts(int index, uint8_t **parts, size_t *sizes)
{
    static const char *const copied[] = {"track.trv", "track.trf", "track.trs", "track.vew"};
    size_t at = wads[index], size, comSize, sinSize;
    Layout layout = {0};
    TimList scene = {0}, sky = {0};
    const uint8_t *data, *com, *sin;
    int i, comCount, ok = 1;

    for (i = 0; i < 4; i++) {
        data = WadPart(at, copied[i], &size);
        parts[i] = SDL_malloc(size + 1);
        memcpy(parts[i], data, size);
        sizes[i] = size;
    }
    data = WadPart(at, "library.cmp", &size);
    ok = MakeLibrary(data, size, &layout, &parts[4], &sizes[4], &parts[5], &sizes[5]);
    /* the scenery: common, then for one player */
    com = WadPart(at, "sceneCom.cmp", &comSize);
    sin = WadPart(at, "sceneSin.cmp", &sinSize);
    ok = ok && Unpack(com, comSize, &scene);
    comCount = scene.count;
    ok = ok && Unpack(sin, sinSize, &scene) && PlaceTims(&layout, &scene);
    data = WadPart(at, "sky.cmp", &size);
    ok = ok && Unpack(data, size, &sky) && PlaceTims(&layout, &sky);
    if (ok) {
        parts[6] = Pack(&scene, &sizes[6]);
        parts[7] = Pack(&sky, &sizes[7]);
        com = WadPart(at, "sceneCom.prm", &comSize);
        sin = WadPart(at, "sceneSin.prm", &sinSize);
        parts[8] = SDL_malloc(comSize + sinSize + 1);
        memcpy(parts[8], com, comSize);
        memcpy(parts[8] + comSize, sin, sinSize);
        sizes[8] = comSize + sinSize;
        ok = PortPrmShiftTextures((char *)parts[8] + comSize, (int)sinSize, comCount);
        data = WadPart(at, "sky.prm", &size);
        parts[9] = SDL_malloc(size + 1);
        memcpy(parts[9], data, size);
        sizes[9] = size;
    }
    if (!ok) {
        fprintf(stderr, "Wipeout 64: %s's textures or scenery could not be made XL's\n", guests[chosen].name);
    }
    TimFree(&scene);
    TimFree(&sky);
    LayoutFree(&layout);
    return ok;
}

/* ---- the games' compression, for others (region.c) ---- */

size_t PortLzssDecompress(const uint8_t *in, size_t length, uint8_t *out, size_t size)
{
    return LzssDecompress(in, length, out, size);
}

uint8_t *PortLzssStore(const uint8_t *in, size_t length, size_t *size)
{
    return LzssStore(in, length, size);
}
