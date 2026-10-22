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

#include "original.h"
#include "platform.h"
#include "settings.h"
#include "psx/disc.h"

#define SECTION_BYTES 156 /* a track section record (the original's and XL's alike) */
#define FACE_BYTES 20

/* The original's circuits, each a Venom and a Rapier track (the folders track01 .. track14), with the
   start line's section and the sky's height. */
static const struct {
    int folder;
    const char *name;
    int shipClass; /* XL's: 1 Venom, 2 Rapier */
    int startLine;
    int skyY;
} originals[] = {
    {2, "ALTIMA VII", 1, 27, -2520},   {3, "ALTIMA VII", 2, 27, -1930},  {4, "KARBONIS V", 1, 16, -5000},
    {5, "KARBONIS V", 2, 16, -5000},   {1, "TERRAMAX", 1, 27, -820},     {6, "TERRAMAX", 2, 27, 0},
    {12, "KORODERA", 1, 16, -2120},    {7, "KORODERA", 2, 16, -2260},    {8, "ARRIDOS IV", 1, 16, -40},
    {11, "ARRIDOS IV", 2, 16, -240},   {9, "SILVERSTREAM", 1, 16, -2700}, {13, "SILVERSTREAM", 2, 16, -2700},
    {10, "FIRESTAR", 1, 27, 0},        {14, "FIRESTAR", 2, 27, 0},
};
#define ORIGINALS ((int)SDL_arraysize(originals))

static char source[1024];       /* the disc image or the folder */
static Disc *disc;              /* when the source is a disc image */
static char folder[1100];       /* when it is a folder: the one holding track01 .. track14 */
static int present[ORIGINALS];  /* the tracks the source has */
static int presentCount;
static int chosen = -1;         /* the track raced, an index of originals[], or -1 */

/* The files made for the chosen track, as the game asks for them in Talon's Reach's folder. */
enum { FILE_WAD, FILE_TEX, FILE_CHK0, FILE_CHK1, FILE_CHK2, FILE_CHK3, FILES };
static const char *const fileNames[FILES] = {"track.wad", "track.tex", "cpoint0.chk", "cpoint1.chk", "cpoint2.chk",
                                             "cpoint3.chk"};
static uint8_t *files[FILES];
static size_t fileSizes[FILES];
static int builtFor = -1;
#define VIRTUAL_LOC 0x40000000  /* the sectors the files are read at: beyond any disc */
#define VIRTUAL_SPAN 0x10000    /* ... 128 MB apart */

/* ---- reading the original's files ---- */

static uint8_t *ReadOriginal(int folderNumber, const char *name, size_t *size)
{
    char path[1300];
    void *data;

    if (disc) {
        int32_t lba;
        uint32_t length;

        SDL_snprintf(path, sizeof(path), "\\WIPEOUT\\TRACK%02d\\%s", folderNumber, name);
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
    SDL_snprintf(path, sizeof(path), "%s/track%02d/%s", folder, folderNumber, name);
    if ((data = SDL_LoadFile(path, size))) {
        return data;
    }
    SDL_snprintf(path, sizeof(path), "%s/TRACK%02d/%s", folder, folderNumber, name);
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
    chosen = -1;
    builtFor = -1;
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
        disc = DiscOpen(path, error, sizeof(error));
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
    SDL_snprintf(message, (size_t)messageSize, "%d of the original's %d tracks found.", presentCount, ORIGINALS);
    return presentCount;
}

const char *PortOriginalSource(void)
{
    return source;
}

/* ---- the tracks, for the game ---- */

/* The game's list of the original's tracks: the ones the source has, in the original's order. */
static int Nth(int index)
{
    int i;

    for (i = 0; i < ORIGINALS; i++) {
        if (present[i] && index-- == 0) {
            return i;
        }
    }
    return -1;
}

int PortOriginalTrackCount(void)
{
    static int tried;

    if (!tried) {
        /* the settings' copy, opened the first time the game asks */
        char message[256];

        tried = 1;
        /* WIPEOUT_ORIGINAL (tests, whose hidden runs read no settings) */
        const char *path = SDL_getenv("WIPEOUT_ORIGINAL");

        if (!path && !portConfig.hidden) {
            path = portSettings.originalPath;
        }
        if (!source[0] && path && path[0]) {
            PortOriginalOpen(path, message, sizeof(message));
        }
    }
    return presentCount;
}

const char *PortOriginalTrackName(int index)
{
    int i = Nth(index);

    return i < 0 ? "" : originals[i].name;
}

int PortOriginalTrackClass(int index)
{
    int i = Nth(index);

    return i < 0 ? 1 : originals[i].shipClass;
}

void PortOriginalTrackSelect(int index)
{
    chosen = index < 0 ? -1 : Nth(index);
}

int PortOriginalTrack(void)
{
    int i, index = 0;

    if (chosen < 0) {
        return -1;
    }
    for (i = 0; i < chosen; i++) {
        index += present[i];
    }
    return index;
}

int PortOriginalStartLine(void)
{
    return chosen < 0 ? 0 : originals[chosen].startLine;
}

int PortOriginalSkyY(void)
{
    return chosen < 0 ? 0 : originals[chosen].skyY;
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

/* The arcade's checkpoints (gates): the start line, then a third and two thirds round the lap. A gate is a
   section whose track faces say CHECKPOINT (track.tex); its cell gives the seconds it adds to the clock
   (which starts with the class's own time): a lap's time, from the track's length (Talon's Reach gives
   about 15 hundredths of a second a section; less for the faster classes), shared out among them. */
static int gates[3];        /* the gates' section numbers */
static int mainSections;    /* the sections of the main loop (a lap) */

static uint8_t *MakeCheckpoints(int shipClass, size_t *size)
{
    static const int hundredthsPerSection[4] = {16, 12, 10, 9};
    uint8_t *out = SDL_calloc(1, 36);
    int lap = mainSections * hundredthsPerSection[shipClass & 3] / 100, i;
    int seconds[3] = {lap * 40 / 100, lap * 30 / 100, lap * 30 / 100};

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
}

static int32_t Be32(const uint8_t *p)
{
    return (int32_t)((uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]);
}

/* The sections' numbers, as XL gives them when it loads a track (InitWeaponGrids; the original's files
   have none of their own: the original numbered its sections when it loaded them, differently): along the
   main loop from the first section, and where a branch starts (its first section says JunctionStart) the
   branch's sections, then on along the main loop after them. Then the gates: the start line and a third and
   two thirds of the main loop on. */
static int NumberSections(uint8_t *sections, int count)
{
    int *numbers = SDL_malloc(sizeof(int) * (size_t)count);
    int section = 0, number = 0, start = -1, steps, i, guard;

#define FIELD32(s, at) Be32(sections + (size_t)(s) * SECTION_BYTES + (at))
#define FLAGS(s) Be16(sections + (size_t)(s) * SECTION_BYTES + 150)
    for (i = 0; i < count; i++) {
        numbers[i] = 0;
    }
    mainSections = 0;
    for (guard = 0; guard < count * 2; guard++) {
        int junction = FIELD32(section, 0);

        numbers[section] = number;
        mainSections++;
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
            mainSections++;
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
    for (i = 0; i < count; i++) {
        PutBe16(sections + (size_t)i * SECTION_BYTES + 152, numbers[i]);
    }
    /* the start line on the main loop, and the gates along it */
    for (section = 0, guard = 0; guard < mainSections; guard++, section = FIELD32(section, 8)) {
        if (numbers[section] == originals[chosen].startLine) {
            start = section;
            break;
        }
    }
    if (start >= 0 && mainSections >= 3) {
        for (i = 0; i < 3; i++) {
            section = start;
            for (steps = 0; steps < mainSections * i / 3; steps++) {
                section = FIELD32(section, 8);
            }
            gates[i] = numbers[section];
        }
    }
#undef FIELD32
#undef FLAGS
    SDL_free(numbers);
    return start >= 0 && mainSections >= 3;
}

static uint8_t *Relocate(const uint8_t *cmp, size_t length, int *paletteSlot, size_t *size);

static int Build(void)
{
    static const char *const wadParts[] = {"track.trv", "track.trf", "track.trs", "track.vew", "library.cmp",
                                           "library.ttf", "scene.cmp", "sky.cmp", "scene.prm", "sky.prm"};
    enum { PARTS = (int)SDL_arraysize(wadParts) };
    uint8_t *parts[PARTS] = {0};
    size_t sizes[PARTS] = {0}, total;
    int folderNumber = originals[chosen].folder, i, sectionCount, ok = 1;
    uint8_t *wad, *at;

    if (builtFor == chosen) {
        return 1;
    }
    FreeFiles();
    for (i = 0; i < PARTS; i++) {
        parts[i] = ReadOriginal(folderNumber, wadParts[i], &sizes[i]);
        if (!parts[i] && i != 3) {
            fprintf(stderr, "original Wipeout: track%02d has no %s\n", folderNumber, wadParts[i]);
            ok = 0;
        }
    }
    sectionCount = ok ? (int)(sizes[2] / SECTION_BYTES) : 0;
    if (ok && !NumberSections(parts[2], sectionCount)) {
        fprintf(stderr, "original Wipeout: track%02d's sections make no loop\n", folderNumber);
        ok = 0;
    }
    if (ok && !parts[3]) {
        parts[3] = MakeViewLists(parts[2], sectionCount, &sizes[3]);
    }
    if (ok) {
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
        /* XL's two bytes after the original's section record (bump, snow): none */
        for (i = 0; i < sectionCount; i++) {
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
            files[FILE_CHK0 + i] = MakeCheckpoints(i, &fileSizes[FILE_CHK0 + i]);
        }
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
   palettes, which must stay in the first half, to a block of their own where XL's own scenery would be
   (x 704 to 895 of the upper rows). The track's tiles (library.cmp) are placed by the game itself, as XL's. */

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

/* ---- the game's file layer ---- */

int PortOriginalFileInfo(const char *pathname, int *loc, int *size)
{
    const char *name;
    int i;

    if (chosen < 0 || !presentCount) {
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

    if (loc < VIRTUAL_LOC || file >= FILES || !files[file]) {
        return 0;
    }
    from = (size_t)sector * 2048;
    length = (size_t)sectors * 2048;
    memset(buffer, 0, length);
    if (from < fileSizes[file]) {
        memcpy(buffer, files[file] + from, SDL_min(length, fileSizes[file] - from));
    }
    return 1;
}
