/* The saves: which save the game uses (remembered in the settings, for the game's autosave: game/src/portsave.c),
   and Wipeout XL's saves in other emulators' memory cards, imported into the game's own.

   Memory card images are found where emulators keep them (DuckStation, RetroArch's cores, OpenEmu, Mednafen,
   ePSXe, PCSX-R, PCSX-Redux, on macOS, Linux and Windows) or where the player points: raw 128 KiB images (.mcd,
   .mcr, .srm, .mc, .ps, .ddf, .vm1 ...), and the ones with a header in front (DexDrive .gme, Connectix .mem/.vgs,
   the PSP's .vmp), and single saves (.mcs/.mcb/.psx: a directory frame, then the blocks). A card's directory
   names each save; Wipeout XL's are "BASCUS-94351" and a name of 8 letters (WipEout 2097's in Europe
   BESLES-00327, in Japan BISIPS-60010 (and BISCPS-45078): the same game, imported under the name of the disc's
   release). An import
   copies the save's blocks (its header with title and icon, the records, the options) onto the game's first card;
   WipEout 2097's header is 256 bytes where the others' is 512, so a save from the other format has its header cut
   to the title and icon, or padded, before the records. */
#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

#include "platform.h"
#include "saves.h"
#include "settings.h"
#include "psx/psx_io.h"
#include "region.h"

#define CARD_BYTES (128 * 1024)
#define BLOCK_BYTES 8192
#define FRAME 128

/* ---- the save in use (the game's: port_platform.h) ---- */

static int lastWrite;
static uint64_t lastWriteAt;
static int importCard = -1;
static char importName[9];

/* Not in the tests' hidden runs (WIPEOUT_AUTOSAVE: unless asked), nor in the second player's game (whose cards
   are its own, and whose settings are not kept). */
static int SavesOn(void)
{
    return !portConfig.secondPlayer && (!portConfig.hidden || SDL_getenv("WIPEOUT_AUTOSAVE"));
}

int PortSaveSlot(char name[9])
{
    const char *slot = portSettings.saveSlot;

    name[0] = 0;
    if (!SavesOn() || (slot[0] != '0' && slot[0] != '1') || slot[1] != ':' || !slot[2]) {
        return -1;
    }
    SDL_strlcpy(name, slot + 2, 9);
    return slot[0] - '0';
}

void PortSaveSlotSet(int card, const char *name)
{
    char slot[16];

    if (!SavesOn()) {
        return;
    }
    if (card < 0) {
        slot[0] = 0;
    } else {
        SDL_snprintf(slot, sizeof(slot), "%d:%.8s", card, name);
    }
    if (strcmp(slot, portSettings.saveSlot)) {
        SDL_strlcpy(portSettings.saveSlot, slot, sizeof(portSettings.saveSlot));
        PortSettingsSave();
    }
}

int PortAutoSaveEnabled(void)
{
    return SavesOn() && !portSettings.noAutoSave;
}

int PortSaveTakeImport(char name[9])
{
    int card = importCard;

    SDL_strlcpy(name, importName, 9);
    importCard = -1;
    return card;
}

void PortSaveWritten(int ok)
{
    lastWrite = ok ? 1 : -1;
    lastWriteAt = SDL_GetTicks();
}

int PortSavesLastWrite(uint64_t *when)
{
    *when = lastWriteAt;
    return lastWrite;
}

void PortSavesUse(int card, const char *name)
{
    importCard = card;
    SDL_strlcpy(importName, name, sizeof(importName));
}

/* ---- other programs' saves ---- */

#define MAX_FOUND 256
static FoundSave found[MAX_FOUND];
static int foundCount;

static const struct {
    const char *prefix;
    const char *region;
} games[] = {{"BASCUS-94351", "USA"}, {"BESLES-00327", "Europe"}, {"BISIPS-60010", "Japan"}, {"BISCPS-45078", "Japan"}};

/* The game a directory entry's name is a save of (an index of games[]), -1 when another's. */
static int GameOf(const uint8_t *name)
{
    size_t i;

    for (i = 0; i < SDL_arraysize(games); i++) {
        if (!memcmp(name, games[i].prefix, 12)) {
            return (int)i;
        }
    }
    return -1;
}

static void Add(const char *file, const char *program, const char *format, const uint8_t *name, int blocks)
{
    FoundSave *s;
    int game = GameOf(name), i;

    if (game < 0 || foundCount == MAX_FOUND) {
        return;
    }
    s = &found[foundCount++];
    memset(s, 0, sizeof(*s));
    SDL_strlcpy(s->file, file, sizeof(s->file));
    SDL_strlcpy(s->program, program, sizeof(s->program));
    SDL_strlcpy(s->format, format, sizeof(s->format));
    for (i = 0; i < 8 && name[12 + i]; i++) {
        s->name[i] = (char)name[12 + i];
    }
    SDL_strlcpy(s->region, games[game].region, sizeof(s->region));
    s->blocks = blocks;
}

/* Where a file's raw card image starts (-1: none), and its format's name. */
static long CardOffset(const uint8_t *data, size_t size, const char **format)
{
    static const struct {
        long offset;
        const char *magic;
        const char *name;
    } kinds[] = {{0, "MC", "raw card image"},
                 {3904, "123-456-STD", "DexDrive (.gme)"},
                 {64, "VgsM", "Connectix (.mem, .vgs)"},
                 {128, "\0PMV", "PSP (.vmp)"}};
    size_t i;

    for (i = 0; i < SDL_arraysize(kinds); i++) {
        size_t magic = i == 3 ? 4 : strlen(kinds[i].magic);

        if (size >= (size_t)kinds[i].offset + CARD_BYTES && !memcmp(data, kinds[i].magic, magic) &&
            data[kinds[i].offset] == 'M' && data[kinds[i].offset + 1] == 'C') {
            *format = kinds[i].name;
            return kinds[i].offset;
        }
    }
    return -1;
}

/* The blocks of the save starting at directory entry `first` of a card (in order; how many, 0 if broken). */
static int Chain(const uint8_t *card, int first, int *blocks)
{
    int block = first, count = 0;

    while (block >= 0 && block < 15 && count < 15) {
        const uint8_t *entry = card + (size_t)(block + 1) * FRAME;
        int next = entry[8] | entry[9] << 8;

        blocks[count++] = block;
        block = next == 0xFFFF ? -1 : next;
    }
    return count;
}

/* A file: its saves of the game, if it is a card image or a single save. */
static void ScanFile(const char *path, const char *program)
{
    size_t size = 0;
    uint8_t *data;
    const char *format;
    long offset;
    int i;

    data = SDL_LoadFile(path, &size);
    if (!data) {
        return;
    }
    offset = CardOffset(data, size, &format);
    if (offset >= 0) {
        const uint8_t *card = data + offset;

        for (i = 0; i < 15; i++) {
            const uint8_t *entry = card + (size_t)(i + 1) * FRAME;
            int blocks[15];

            if (entry[0] == 0x51) {
                Add(path, program, format, entry + 10, Chain(card, i, blocks));
            }
        }
    } else if (size >= FRAME + BLOCK_BYTES && (size - FRAME) % BLOCK_BYTES == 0 && data[0] == 0x51) {
        Add(path, program, "single save (.mcs)", data + 10, (int)((size - FRAME) / BLOCK_BYTES));
    }
    SDL_free(data);
}

/* The size of a card image, with or without a header, or a single save of a few blocks. */
static int Likely(Sint64 size)
{
    return (size >= CARD_BYTES && size <= CARD_BYTES + 4096) ||
           (size >= FRAME + BLOCK_BYTES && size <= FRAME + 15 * BLOCK_BYTES && (size - FRAME) % BLOCK_BYTES == 0);
}

static const char *scanProgram;
static int scanDepth;

static SDL_EnumerationResult SDLCALL ScanEntry(void *user, const char *dir, const char *name)
{
    char path[2048];
    SDL_PathInfo info;

    (void)user;
    if (name[0] == '.') {
        return SDL_ENUM_CONTINUE;
    }
    SDL_snprintf(path, sizeof(path), "%s%s", dir, name);
    if (!SDL_GetPathInfo(path, &info)) {
        return SDL_ENUM_CONTINUE;
    }
    if (info.type == SDL_PATHTYPE_DIRECTORY && scanDepth < 3) {
        scanDepth++;
        SDL_EnumerateDirectory(path, ScanEntry, NULL);
        scanDepth--;
    } else if (info.type == SDL_PATHTYPE_FILE && Likely(info.size)) {
        /* (not the game's own cards) */
        if (!portConfig.savePath || strncmp(path, portConfig.savePath, strlen(portConfig.savePath))) {
            ScanFile(path, scanProgram);
        }
    }
    return SDL_ENUM_CONTINUE;
}

static void ScanPlace(const char *program, const char *base, const char *rest)
{
    char path[1100];
    SDL_PathInfo info;

    if (!base) {
        return;
    }
    SDL_snprintf(path, sizeof(path), "%s%s%s", base,
                 base[0] && base[strlen(base) - 1] != '/' && base[strlen(base) - 1] != '\\' ? "/" : "", rest);
    if (!SDL_GetPathInfo(path, &info)) {
        return;
    }
    scanProgram = program;
    if (info.type == SDL_PATHTYPE_DIRECTORY) {
        scanDepth = 0;
        SDL_EnumerateDirectory(path, ScanEntry, NULL);
    } else {
        ScanFile(path, program);
    }
}

int PortSavesScan(const char *extra)
{
    const char *home = SDL_GetUserFolder(SDL_FOLDER_HOME);
    const char *documents = SDL_GetUserFolder(SDL_FOLDER_DOCUMENTS);
    const char *appData = SDL_getenv("APPDATA");
    static const struct {
        const char *program;
        const char *path; /* under the home folder */
    } homePlaces[] = {
        /* macOS */
        {"DuckStation", "Library/Application Support/DuckStation/memcards"},
        {"RetroArch", "Library/Application Support/RetroArch/saves"},
        {"OpenEmu", "Library/Application Support/OpenEmu/Mednafen/Battery Saves"},
        {"PCSX-Redux", "Library/Application Support/pcsx-redux"},
        {"PCSX-R", "Library/Application Support/Pcsxr/memcards"},
        /* Linux */
        {"DuckStation", ".local/share/duckstation/memcards"},
        {"DuckStation", ".var/app/org.duckstation.DuckStation/data/duckstation/memcards"},
        {"RetroArch", ".config/retroarch/saves"},
        {"RetroArch", ".var/app/org.libretro.RetroArch/config/retroarch/saves"},
        {"PCSX-Redux", ".config/pcsx-redux"},
        {"PCSX-R", ".pcsxr/memcards"},
        /* everywhere */
        {"Mednafen", ".mednafen/sav"},
        {"ePSXe", ".epsxe/memcards"},
        {"RetroArch", "RetroArch/saves"},
    };
    size_t i;

    foundCount = 0;
    for (i = 0; i < SDL_arraysize(homePlaces); i++) {
        ScanPlace(homePlaces[i].program, home, homePlaces[i].path);
    }
    /* Windows */
    ScanPlace("DuckStation", documents, "DuckStation/memcards");
    ScanPlace("RetroArch", appData, "RetroArch/saves");
    ScanPlace("PCSX-Redux", appData, "pcsx-redux");
    ScanPlace("RetroArch", "C:/RetroArch-Win64", "saves");
    ScanPlace("ePSXe", "C:/ePSXe", "memcards");
    if (extra && extra[0]) {
        ScanPlace("Chosen", extra, "");
    }
    return foundCount;
}

const FoundSave *PortSavesFound(void)
{
    return found;
}

int PortSavesCount(void)
{
    return foundCount;
}

/* A found save's blocks (blocks x 8 KiB, the caller frees). */
static uint8_t *SaveData(const FoundSave *s)
{
    size_t size = 0;
    uint8_t *data = SDL_LoadFile(s->file, &size), *out = NULL;
    const char *format;
    long offset;
    int i, k;

    if (!data) {
        return NULL;
    }
    offset = CardOffset(data, size, &format);
    if (offset >= 0) {
        const uint8_t *card = data + offset;

        for (i = 0; i < 15 && !out; i++) {
            const uint8_t *entry = card + (size_t)(i + 1) * FRAME;
            int blocks[15], count;

            if (entry[0] != 0x51 || GameOf(entry + 10) < 0 || strncmp((const char *)entry + 22, s->name, 8)) {
                continue;
            }
            count = Chain(card, i, blocks);
            if (count == s->blocks) {
                out = SDL_malloc((size_t)count * BLOCK_BYTES);
                for (k = 0; k < count; k++) {
                    memcpy(out + (size_t)k * BLOCK_BYTES, card + (size_t)(blocks[k] + 1) * BLOCK_BYTES, BLOCK_BYTES);
                }
            }
        }
    } else if (size == FRAME + (size_t)s->blocks * BLOCK_BYTES) {
        out = SDL_malloc((size_t)s->blocks * BLOCK_BYTES);
        memcpy(out, data + FRAME, (size_t)s->blocks * BLOCK_BYTES);
    }
    SDL_free(data);
    return out;
}

int PortSavesImport(int index, char *message, int messageSize)
{
    const FoundSave *s;
    uint8_t *data;
    char name[24];
    int ok;

    if (index < 0 || index >= foundCount) {
        return 0;
    }
    s = &found[index];
    data = SaveData(s);
    if (!data) {
        SDL_snprintf(message, (size_t)messageSize, "The save could not be read from %s.", s->file);
        return 0;
    }
    {
        /* the header size of the save's format and of the disc's (the records follow it) */
        size_t from = !strcmp(s->region, "Europe") ? 256 : 512, to = portEuropeCard ? 256 : 512;
        size_t records = 0x600 + 0x100, size = (size_t)s->blocks * BLOCK_BYTES;

        if (from != to && size >= from + records && size >= to + records) {
            uint8_t *converted = SDL_calloc(1, size);

            if (converted) {
                memcpy(converted, data, 256); /* the title frame and the icon */
                memcpy(converted + to, data + from, records);
                SDL_free(data);
                data = converted;
            }
        }
    }
    SDL_snprintf(name, sizeof(name), "%s%s", portEuropeCard ? "BESLES-00327" : "BASCUS-94351", s->name);
    ok = PsxCardWriteFile(0, name, data, s->blocks * BLOCK_BYTES);
    SDL_free(data);
    if (!ok) {
        SDL_snprintf(message, (size_t)messageSize, "The first memory card has no room for it (%d blocks).", s->blocks);
        return 0;
    }
    PortSavesUse(0, s->name);
    SDL_snprintf(message, (size_t)messageSize, "%s imported from %s: the game loads it (in the menus).", s->name,
                 s->program);
    return 1;
}
