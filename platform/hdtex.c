/* HD textures: a texture pack made for DuckStation's texture replacement, drawn in place of the console's
   textures it replaces. The port comes with none: the player chooses their own (the debug console's Graphics
   tab), the .zip as downloaded or the folder it was extracted to.

   A pack names each image after the texture it replaces, the way DuckStation's texture cache dumps them:
   "texupload-P4-<upload hash>-<palette hash>-<W>x<H>-<x>-<y>-<w>x<h>-P<first>-<last>" is the part (x, y, w, h,
   in texels) of an upload to video memory (W x H pixels: LoadImage) whose pixels hash to <upload hash>;
   "texpage-..." the part of a texture page whose pixels there hash to it. Either is seen through a palette whose
   colours hash to <palette hash>: all its colours, or the range first to last when the name gives a narrower
   one. ("STP4" and the like: the image's alpha says which texels are semi-transparent; "C16": 15-bit colour,
   no palette.) The hashes are XXH3, 64 bits, over the 16-bit pixels row after row, computed here as
   DuckStation computes them, one of its quirks included (a palette range's hash covers as many colours from the
   palette's first). config.yaml's Aliases name the files under replacements/.

   gpu_gl.c asks for each textured primitive which replacement covers its texels (HdTexFind) and then samples
   that image instead of video memory there. An image is decoded the first time it is asked for, on a thread of
   its own (meanwhile the console's texture is drawn), and kept at the framebuffer's scale. */
#include <SDL3/SDL.h>
#include <stdlib.h>
#include <string.h>

#include "hdtex.h"
#include "platform.h"
#include "settings.h"
#include "psx/gpu.h"

#define XXH_INLINE_ALL
#include "third_party/xxhash/xxhash.h"
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#include "third_party/stb/stb_image.h"

enum { KIND_UPLOAD, KIND_PAGE };

typedef struct Replacement {
    char *name;
    int kind;
    int depth;              /* 0 4-bit, 1 8-bit, 2 15-bit */
    int semi;               /* the image's alpha says which texels are semi-transparent */
    uint64_t sourceHash, paletteHash;
    int offsetX, offsetY, w, h; /* in texels: of the upload, or of the page */
    int paletteFirst, paletteLast;
    int image;
} Replacement;

enum { IMAGE_NONE, IMAGE_QUEUED, IMAGE_READY, IMAGE_FAILED };

typedef struct Image {
    char *file;             /* in the pack's replacements folder */
    int nameW, nameH;       /* the texels it replaces (its first name's) */
    int semi;
    int state;
    uint8_t *pixels;
    int w, h;
} Image;

static char source[1024];
static char about[1024];
static Replacement *replacements;
static int replacementCount;
static Image *images;
static int imageCount;

/* ---- the pack's files: a folder, or a .zip held in memory ---- */

static char folder[1100];          /* the replacements folder, with a separator at the end */
static uint8_t *zip;
static size_t zipSize;
static char zipFolder[512];        /* the replacements folder in the zip ("SCUS-94351/replacements/") */
typedef struct ZipEntry {
    char *name;
    uint32_t offset, packed, size;
    int method;
} ZipEntry;
static ZipEntry *zipEntries;
static int zipCount;

static uint32_t Read16(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8;
}

static uint32_t Read32(const uint8_t *p)
{
    return Read16(p) | Read16(p + 2) << 16;
}

/* The zip's central directory into zipEntries. */
static int ZipIndex(void)
{
    size_t at, end;
    uint32_t count, offset;
    int i;

    if (zipSize < 22) {
        return 0;
    }
    for (at = zipSize - 22;; at--) {
        if (Read32(zip + at) == 0x06054b50) {
            break;
        }
        if (at == 0 || zipSize - at > 65535 + 22) {
            return 0;
        }
    }
    count = Read16(zip + at + 10);
    offset = Read32(zip + at + 16);
    end = at;
    zipEntries = SDL_calloc(count ? count : 1, sizeof(ZipEntry));
    for (i = 0; i < (int)count && offset + 46 <= end; i++) {
        const uint8_t *p = zip + offset;
        uint32_t nameLength = Read16(p + 28);

        if (Read32(p) != 0x02014b50 || offset + 46 + nameLength > end) {
            break;
        }
        zipEntries[zipCount].method = (int)Read16(p + 10);
        zipEntries[zipCount].packed = Read32(p + 20);
        zipEntries[zipCount].size = Read32(p + 24);
        zipEntries[zipCount].offset = Read32(p + 42);
        zipEntries[zipCount].name = SDL_malloc(nameLength + 1);
        memcpy(zipEntries[zipCount].name, p + 46, nameLength);
        zipEntries[zipCount].name[nameLength] = 0;
        zipCount++;
        offset += 46 + nameLength + Read16(p + 30) + Read16(p + 32);
    }
    return zipCount;
}

static uint8_t *ZipRead(const ZipEntry *e, size_t *size)
{
    const uint8_t *local = zip + e->offset;
    size_t data;
    uint8_t *out;

    if ((size_t)e->offset + 30 > zipSize || Read32(local) != 0x04034b50) {
        return NULL;
    }
    data = (size_t)e->offset + 30 + Read16(local + 26) + Read16(local + 28);
    if (data + e->packed > zipSize || (e->method != 0 && e->method != 8)) {
        return NULL;
    }
    out = SDL_malloc(e->size ? e->size : 1);
    if (e->method == 0) {
        memcpy(out, zip + data, e->size);
    } else {
        /* stb's inflate reads a little past the end of the stream (in a PNG the checksum follows it): zeros */
        char *packed = SDL_calloc(e->packed + 16, 1);
        int got;

        memcpy(packed, zip + data, e->packed);
        got = stbi_zlib_decode_noheader_buffer((char *)out, (int)e->size, packed, (int)e->packed + 16);
        SDL_free(packed);
        if (got != (int)e->size) {
            SDL_free(out);
            return NULL;
        }
    }
    *size = e->size;
    return out;
}

/* A file of the replacements folder (path in it, '/' between folders). */
static uint8_t *PackRead(const char *path, size_t *size)
{
    char full[2048];
    int i;

    if (zip) {
        SDL_snprintf(full, sizeof(full), "%s%s", zipFolder, path);
        for (i = 0; i < zipCount; i++) {
            if (!strcmp(zipEntries[i].name, full)) {
                return ZipRead(&zipEntries[i], size);
            }
        }
        return NULL;
    }
    SDL_snprintf(full, sizeof(full), "%s%s", folder, path);
    return SDL_LoadFile(full, size);
}

/* ---- names ---- */

static int ParseName(const char *title, Replacement *r)
{
    char copy[160], *tokens[12], *p;
    int count = 0, mode, palette, t = 0;
    static const char *const modes[] = {"P4", "P8", "C16", "C16", "STP4", "STP8", "STC16", "STC16"};

    if (strlen(title) >= sizeof(copy)) {
        return 0;
    }
    strcpy(copy, title);
    for (p = copy; count < 12;) {
        tokens[count++] = p;
        p = strchr(p, '-');
        if (!p) {
            break;
        }
        *p++ = 0;
    }
    memset(r, 0, sizeof(*r));
    if (!strcmp(tokens[0], "texupload")) {
        r->kind = KIND_UPLOAD;
    } else if (!strcmp(tokens[0], "texpage")) {
        r->kind = KIND_PAGE;
    } else {
        return 0;
    }
    for (mode = 0; mode < 8 && (count < 2 || strcmp(tokens[1], modes[mode])); mode++) {
    }
    if (mode == 8) {
        return 0;
    }
    r->depth = mode & 3;
    r->depth = r->depth > 2 ? 2 : r->depth;
    r->semi = mode >= 4;
    palette = r->depth < 2;
    if (count != (palette ? 10 : 7) || strlen(tokens[2]) != 16 || (palette && strlen(tokens[3]) != 16)) {
        return 0;
    }
    r->sourceHash = SDL_strtoull(tokens[2], NULL, 16);
    t = 3;
    if (palette) {
        r->paletteHash = SDL_strtoull(tokens[3], NULL, 16);
        t = 4;
    }
    /* tokens[t]: the upload's (or page's) size, unused here; then the offset and the size */
    r->offsetX = SDL_atoi(tokens[t + 1]);
    r->offsetY = SDL_atoi(tokens[t + 2]);
    if (SDL_sscanf(tokens[t + 3], "%dx%d", &r->w, &r->h) != 2 || r->w <= 0 || r->h <= 0) {
        return 0;
    }
    if (palette) {
        if (tokens[t + 4][0] != 'P') {
            return 0;
        }
        r->paletteFirst = SDL_atoi(tokens[t + 4] + 1);
        r->paletteLast = SDL_atoi(tokens[t + 5]);
        if (r->paletteFirst < 0 || r->paletteLast < r->paletteFirst || r->paletteLast >= (r->depth ? 256 : 16)) {
            return 0;
        }
    }
    return 1;
}

static int ImageFor(const char *file, const Replacement *r)
{
    int i;

    for (i = 0; i < imageCount; i++) {
        if (!strcmp(images[i].file, file)) {
            return i;
        }
    }
    images = SDL_realloc(images, sizeof(Image) * (imageCount + 1));
    memset(&images[imageCount], 0, sizeof(Image));
    images[imageCount].file = SDL_strdup(file);
    images[imageCount].nameW = r->w;
    images[imageCount].nameH = r->h;
    images[imageCount].semi = r->semi;
    return imageCount++;
}

/* A replacement: the file (in the replacements folder) of the texture named title. */
static void Add(const char *title, const char *file)
{
    Replacement r;
    const char *dot = strrchr(file, '.');
    int i;

    if (!dot || (SDL_strcasecmp(dot, ".png") && SDL_strcasecmp(dot, ".jpg") && SDL_strcasecmp(dot, ".jpeg"))) {
        return; /* (DuckStation's other formats, WebP and DDS, are not read here) */
    }
    if (!ParseName(title, &r)) {
        return;
    }
    for (i = 0; i < replacementCount; i++) {
        if (!strcmp(replacements[i].name, title)) {
            return; /* the first of the same name, as DuckStation keeps */
        }
    }
    r.name = SDL_strdup(title);
    r.image = ImageFor(file, &r);
    replacements = SDL_realloc(replacements, sizeof(Replacement) * (replacementCount + 1));
    replacements[replacementCount++] = r;
}

/* A file in the replacements folder named after the texture it replaces. */
static void AddFile(const char *path)
{
    const char *base = strrchr(path, '/');
    char title[160];
    const char *dot;

    base = base ? base + 1 : path;
    dot = strrchr(base, '.');
    if (!dot || dot - base >= (int)sizeof(title)) {
        return;
    }
    memcpy(title, base, (size_t)(dot - base));
    title[dot - base] = 0;
    Add(title, path);
}

/* config.yaml: its heading (the comments before anything else: the pack's name, author and page) and its
   Aliases (texture name: file). */
static void ReadConfig(char *text)
{
    char *line, *next;
    int heading = 1, aliases = 0;

    for (line = text; line && *line; line = next) {
        char *end, *colon, *value;

        next = strchr(line, '\n');
        if (next) {
            *next++ = 0;
        }
        for (end = line + strlen(line); end > line && (end[-1] == '\r' || end[-1] == ' ' || end[-1] == '\t'); end--) {
            end[-1] = 0;
        }
        if (heading && line[0] == '#') {
            char *s = line;
            while (*s == '#' || *s == ' ') {
                s++;
            }
            for (end = s + strlen(s); end > s && (end[-1] == '#' || end[-1] == ' '); end--) {
                end[-1] = 0;
            }
            if (*s && strlen(about) + strlen(s) + 2 < sizeof(about)) {
                strcat(about, s);
                strcat(about, "\n");
            }
            continue;
        }
        if (line[0]) {
            heading = 0;
        }
        if (!line[0] || line[0] == '#') {
            continue;
        }
        if (line[0] != ' ' && line[0] != '\t') {
            aliases = !strncmp(line, "Aliases:", 8);
            continue;
        }
        if (!aliases || !(colon = strstr(line, ": "))) {
            continue;
        }
        *colon = 0;
        while (*line == ' ' || *line == '\t') {
            line++;
        }
        value = colon + 2;
        while (*value == ' ') {
            value++;
        }
        if ((*value == '"' || *value == '\'') && strlen(value) >= 2 && value[strlen(value) - 1] == *value) {
            value[strlen(value) - 1] = 0;
            value++;
        }
        for (end = value; *end; end++) {
            if (*end == '\\') {
                *end = '/';
            }
        }
        Add(line, value);
    }
    if (about[0]) {
        about[strlen(about) - 1] = 0;
    }
}

static char scanFolder[2048];

static SDL_EnumerationResult ScanEntry(void *data, const char *dir, const char *name)
{
    char path[2048], relative[2048];
    SDL_PathInfo info;

    (void)data;
    SDL_snprintf(path, sizeof(path), "%s%s", dir, name);
    if (!SDL_GetPathInfo(path, &info)) {
        return SDL_ENUM_CONTINUE;
    }
    SDL_snprintf(relative, sizeof(relative), "%s%s", scanFolder, name);
    if (info.type == SDL_PATHTYPE_DIRECTORY) {
        char saved[2048];
        SDL_strlcpy(saved, scanFolder, sizeof(saved));
        SDL_snprintf(scanFolder, sizeof(scanFolder), "%s/", relative);
        SDL_EnumerateDirectory(path, ScanEntry, NULL);
        SDL_strlcpy(scanFolder, saved, sizeof(scanFolder));
    } else if (!strncmp(name, "texupload-", 10) || !strncmp(name, "texpage-", 8)) {
        AddFile(relative);
    }
    return SDL_ENUM_CONTINUE;
}

static int IsFolder(const char *path)
{
    SDL_PathInfo info;
    return SDL_GetPathInfo(path, &info) && info.type == SDL_PATHTYPE_DIRECTORY;
}

static int IsFile(const char *path)
{
    SDL_PathInfo info;
    return SDL_GetPathInfo(path, &info) && info.type == SDL_PATHTYPE_FILE;
}

/* A pack's own folder: holds config.yaml or replacements/. */
static int PackFolder(const char *path)
{
    char test[1200];

    SDL_snprintf(test, sizeof(test), "%s/config.yaml", path);
    if (IsFile(test)) {
        return 1;
    }
    SDL_snprintf(test, sizeof(test), "%s/replacements", path);
    return IsFolder(test);
}

static char found[1100];

static SDL_EnumerationResult FindPackFolder(void *data, const char *dir, const char *name)
{
    char path[1200];

    (void)data;
    SDL_snprintf(path, sizeof(path), "%s%s", dir, name);
    if (IsFolder(path) && PackFolder(path) && (!found[0] || !strcmp(name, "SCUS-94351"))) {
        SDL_strlcpy(found, path, sizeof(found));
    }
    return SDL_ENUM_CONTINUE;
}

static int OpenFolder(const char *path)
{
    char root[1100], config[1200];
    size_t length;
    char *text;

    SDL_strlcpy(root, path, sizeof(root));
    length = strlen(root);
    while (length > 1 && (root[length - 1] == '/' || root[length - 1] == '\\')) {
        root[--length] = 0;
    }
    if (!PackFolder(root)) {
        /* the replacements folder itself, or a folder above the pack's (DuckStation's textures folder, or where
           the download was extracted) */
        const char *base = SDL_max(SDL_strrchr(root, '/'), SDL_strrchr(root, '\\'));
        if (base && !SDL_strcasecmp(base + 1, "replacements")) {
            root[base - root] = 0;
        } else {
            found[0] = 0;
            SDL_EnumerateDirectory(root, FindPackFolder, NULL);
            if (!found[0]) {
                return 0;
            }
            SDL_strlcpy(root, found, sizeof(root));
        }
    }
    SDL_snprintf(folder, sizeof(folder), "%s/replacements/", root);
    if (!IsFolder(folder)) {
        SDL_snprintf(folder, sizeof(folder), "%s/", root);
    }
    SDL_snprintf(config, sizeof(config), "%s/config.yaml", root);
    if ((text = SDL_LoadFile(config, NULL))) {
        ReadConfig(text);
        SDL_free(text);
    }
    scanFolder[0] = 0;
    SDL_EnumerateDirectory(folder, ScanEntry, NULL);
    return 1;
}

static int OpenZip(const char *path)
{
    const char *config = NULL;
    size_t length;
    int i;

    zip = SDL_LoadFile(path, &zipSize);
    if (!zip || !ZipIndex()) {
        return 0;
    }
    /* config.yaml nearest the top (the US disc's, SCUS-94351, when the pack has several) */
    for (i = 0; i < zipCount; i++) {
        const char *name = zipEntries[i].name;
        const char *base = strrchr(name, '/');

        if (strcmp(base ? base + 1 : name, "config.yaml")) {
            continue;
        }
        if (!config || strstr(name, "SCUS-94351/") || (!strstr(config, "SCUS-94351/") && strlen(name) < strlen(config))) {
            config = name;
        }
    }
    zipFolder[0] = 0;
    if (config) {
        const char *base = strrchr(config, '/');
        length = base ? (size_t)(base - config + 1) : 0;
        SDL_snprintf(zipFolder, sizeof(zipFolder), "%.*sreplacements/", (int)length, config);
        for (i = 0; i < zipCount && strncmp(zipEntries[i].name, zipFolder, strlen(zipFolder)); i++) {
        }
        if (i == zipCount) {
            SDL_snprintf(zipFolder, sizeof(zipFolder), "%.*s", (int)length, config);
        }
    }
    length = strlen(zipFolder);
    for (i = 0; i < zipCount; i++) {
        const char *name = zipEntries[i].name;
        const char *base = strrchr(name, '/');

        base = base ? base + 1 : name;
        if (!strncmp(name, zipFolder, length) && (!strncmp(base, "texupload-", 10) || !strncmp(base, "texpage-", 8))) {
            AddFile(name + length);
        }
    }
    for (i = 0; config && i < zipCount; i++) {
        if (zipEntries[i].name == config) {
            size_t size;
            uint8_t *text = ZipRead(&zipEntries[i], &size);
            if (text) {
                char *copy = SDL_malloc(size + 1);
                memcpy(copy, text, size);
                copy[size] = 0;
                ReadConfig(copy);
                SDL_free(copy);
                SDL_free(text);
            }
        }
    }
    return 1;
}

/* ---- the uploads: video memory written by LoadImage, still whole ---- */

typedef struct Upload {
    int x, y, w, h;
    uint64_t hash;
    int hashed, named;
} Upload;
#define MAX_UPLOADS 4096
static Upload uploads[MAX_UPLOADS];
static int uploadCount;

static uint64_t HashRect(int x, int y, int w, int h)
{
    XXH3_state_t state;
    int row;

    XXH3_64bits_reset(&state);
    for (row = 0; row < h; row++) {
        XXH3_64bits_update(&state, &psxVram[y + row][x], (size_t)w * 2);
    }
    return XXH3_64bits_digest(&state);
}

/* (after the pack is opened: an upload's hash, and whether the pack names any texture after it) */
static void HashUpload(Upload *u)
{
    int i;

    u->hash = HashRect(u->x, u->y, u->w, u->h);
    u->hashed = 1;
    u->named = 0;
    for (i = 0; i < replacementCount && !u->named; i++) {
        u->named = replacements[i].kind == KIND_UPLOAD && replacements[i].sourceHash == u->hash;
    }
}

static int Overlap(int ax, int ay, int aw, int ah, int bx, int by, int bw, int bh)
{
    return ax < bx + bw && bx < ax + aw && ay < by + bh && by < ay + ah;
}

void HdTexOverwritten(int x, int y, int w, int h)
{
    int i, kept = 0;

    for (i = 0; i < uploadCount; i++) {
        if (!Overlap(x, y, w, h, uploads[i].x, uploads[i].y, uploads[i].w, uploads[i].h)) {
            uploads[kept++] = uploads[i];
        }
    }
    uploadCount = kept;
}

void HdTexUploaded(int x, int y, int w, int h)
{
    Upload *u;

    HdTexOverwritten(x, y, w, h);
    if (w <= 0 || h <= 0 || x < 0 || y < 0 || x + w > VRAM_WIDTH || y + h > VRAM_HEIGHT) {
        return;
    }
    if (uploadCount == MAX_UPLOADS) {
        memmove(uploads, uploads + 1, sizeof(Upload) * (MAX_UPLOADS - 1));
        uploadCount--;
    }
    u = &uploads[uploadCount++];
    u->x = x;
    u->y = y;
    u->w = w;
    u->h = h;
    u->hashed = 0;
    if (replacementCount) {
        HashUpload(u);
    }
}

/* ---- what replaces what: per texture (page, colour depth, palette), until video memory there changes ---- */

static uint32_t pageChanges[2][16];     /* video memory's pages, 64 x 256 */
static uint32_t paletteChanges[VRAM_HEIGHT];

void HdTexChanged(int x, int y, int w, int h)
{
    int px, py, row;

    if (x < 0 || y < 0 || x + w > VRAM_WIDTH || y + h > VRAM_HEIGHT) {
        x = 0;
        y = 0;
        w = VRAM_WIDTH;
        h = VRAM_HEIGHT;
    }
    if (w <= 0 || h <= 0) {
        return;
    }
    for (py = y / 256; py <= (y + h - 1) / 256; py++) {
        for (px = x / 64; px <= (x + w - 1) / 64; px++) {
            pageChanges[py][px]++;
        }
    }
    for (row = y; row < y + h; row++) {
        paletteChanges[row]++;
    }
}

#define SUBS 8
typedef struct Sub {
    int16_t x, y, w, h;     /* in the page, texels */
    int16_t srcX, srcY;     /* in the replacement, texels */
    int replacement;
} Sub;
typedef struct Texture {
    uint32_t key, stamp;
    int count;
    Sub subs[SUBS];
} Texture;
#define TEXTURE_SLOTS 4096
static Texture *textures;
static int textureCount;

static int Shift(int depth)
{
    return 2 - depth; /* texels per pixel: 4, 2, 1 */
}

/* The pixels a texture is read from (page to its right edge, not wrapping) */
static void PageRect(int pageX, int pageY, int depth, int *x1, int *y1)
{
    *x1 = SDL_min(pageX + (256 >> Shift(depth)), VRAM_WIDTH);
    *y1 = SDL_min(pageY + 256, VRAM_HEIGHT);
}

static uint32_t Stamp(int pageX, int pageY, int depth, int clutY)
{
    int x1, y1, px;
    uint32_t stamp = 0;

    PageRect(pageX, pageY, depth, &x1, &y1);
    for (px = pageX / 64; px <= (x1 - 1) / 64; px++) {
        stamp += pageChanges[pageY / 256][px];
    }
    return stamp + (depth < 2 ? paletteChanges[clutY] : 0);
}

static uint64_t PaletteHash(int clutX, int clutY, int depth)
{
    int colours = depth == 0 ? 16 : SDL_min(256, VRAM_WIDTH - clutX);
    return XXH3_64bits(&psxVram[clutY][clutX], (size_t)colours * 2);
}

static int PaletteMatches(const Replacement *r, uint64_t full, int clutX, int clutY)
{
    if (r->depth == 2) {
        return 1;
    }
    if (r->paletteFirst == 0 && r->paletteLast == (r->depth ? 255 : 15)) {
        return r->paletteHash == full;
    }
    if (clutX + r->paletteLast >= VRAM_WIDTH) {
        return 0;
    }
    /* (DuckStation hashes the range's count of colours from the palette's first) */
    return r->paletteHash ==
           XXH3_64bits(&psxVram[clutY][clutX], (size_t)(r->paletteLast - r->paletteFirst + 1) * 2);
}

static void AddSub(Texture *t, int replacement, int x, int y, int x1, int y1, int srcX, int srcY)
{
    Sub *s;

    if (t->count == SUBS) {
        return;
    }
    s = &t->subs[t->count++];
    s->x = (int16_t)x;
    s->y = (int16_t)y;
    s->w = (int16_t)(x1 - x);
    s->h = (int16_t)(y1 - y);
    s->srcX = (int16_t)srcX;
    s->srcY = (int16_t)srcY;
    s->replacement = replacement;
}

static void Replace(Texture *t, int pageX, int pageY, int depth, int clutX, int clutY)
{
    int shift = Shift(depth), x1, y1, i, j;
    uint64_t palette = depth < 2 ? PaletteHash(clutX, clutY, depth) : 0;

    t->count = 0;
    PageRect(pageX, pageY, depth, &x1, &y1);
    /* parts of the page */
    for (i = 0; i < replacementCount; i++) {
        const Replacement *r = &replacements[i];
        int left, right;

        if (r->kind != KIND_PAGE || r->depth != depth || !PaletteMatches(r, palette, clutX, clutY)) {
            continue;
        }
        left = pageX + (r->offsetX >> shift);
        right = pageX + ((r->offsetX + r->w) >> shift);
        if (right > x1 || pageY + r->offsetY + r->h > y1 || right <= left) {
            continue;
        }
        if (HashRect(left, pageY + r->offsetY, right - left, r->h) != r->sourceHash) {
            continue;
        }
        AddSub(t, i, r->offsetX, r->offsetY, SDL_min(r->offsetX + r->w, 256), SDL_min(r->offsetY + r->h, 256), 0, 0);
    }
    /* parts of the uploads on it */
    for (j = 0; j < uploadCount; j++) {
        Upload *u = &uploads[j];
        int toPageX, toPageY;

        if (!Overlap(pageX, pageY, x1 - pageX, y1 - pageY, u->x, u->y, u->w, u->h)) {
            continue;
        }
        if (!u->hashed) {
            HashUpload(u);
        }
        if (!u->named) {
            continue;
        }
        toPageX = (pageX - u->x) << shift;
        toPageY = pageY - u->y;
        for (i = 0; i < replacementCount; i++) {
            const Replacement *r = &replacements[i];
            int left, top, right, bottom;

            if (r->kind != KIND_UPLOAD || r->depth != depth || r->sourceHash != u->hash ||
                !PaletteMatches(r, palette, clutX, clutY)) {
                continue;
            }
            left = r->offsetX - toPageX;
            top = r->offsetY - toPageY;
            right = left + r->w;
            bottom = top + r->h;
            if (right <= 0 || bottom <= 0 || left >= 256 || top >= 256) {
                continue;
            }
            AddSub(t, i, SDL_max(left, 0), SDL_max(top, 0), SDL_min(right, 256), SDL_min(bottom, 256),
                   left < 0 ? -left : 0, top < 0 ? -top : 0);
        }
    }
}

static Texture *Lookup(int pageX, int pageY, int depth, int clutX, int clutY)
{
    uint32_t key = 1u + ((uint32_t)pageX / 64 | (uint32_t)pageY / 256 << 4 | (uint32_t)depth << 5 |
                         (depth < 2 ? (uint32_t)clutX / 16 << 7 | (uint32_t)clutY << 13 : 0));
    uint32_t stamp = Stamp(pageX, pageY, depth, clutY);
    uint32_t slot = (key * 2654435761u) % TEXTURE_SLOTS;
    Texture *t;

    if (!textures) {
        textures = SDL_calloc(TEXTURE_SLOTS, sizeof(Texture));
    }
    for (;;) {
        t = &textures[slot];
        if (t->key == key || !t->key) {
            break;
        }
        slot = (slot + 1) % TEXTURE_SLOTS;
    }
    if (!t->key) {
        if (textureCount >= TEXTURE_SLOTS * 3 / 4) {
            memset(textures, 0, sizeof(Texture) * TEXTURE_SLOTS);
            textureCount = 0;
            return Lookup(pageX, pageY, depth, clutX, clutY);
        }
        textureCount++;
        t->key = key;
        t->stamp = stamp + 1; /* (not yet worked out) */
    }
    if (t->stamp != stamp) {
        Replace(t, pageX, pageY, depth, clutX, clutY);
        t->stamp = stamp;
    }
    return t;
}

int HdTexFind(int pageX, int pageY, int depth, int clutX, int clutY, float u0, float v0, float u1, float v1,
              HdTexHit *hit)
{
    Texture *t;
    float best = 0.0f;
    int i, chosen = -1;

    if (!replacementCount || depth > 2 || pageY + 256 > VRAM_HEIGHT || clutY >= VRAM_HEIGHT) {
        return 0;
    }
    t = Lookup(pageX, pageY, depth, clutX, clutY);
    for (i = 0; i < t->count; i++) {
        const Sub *s = &t->subs[i];
        float across = SDL_min((float)(s->x + s->w), u1 + 0.5f) - SDL_max((float)s->x, u0 - 0.5f);
        float down = SDL_min((float)(s->y + s->h), v1 + 0.5f) - SDL_max((float)s->y, v0 - 0.5f);

        if (across > 0.0f && down > 0.0f && across * down > best) {
            best = across * down;
            chosen = i;
        }
    }
    if (chosen < 0) {
        return 0;
    }
    {
        const Sub *s = &t->subs[chosen];
        const Replacement *r = &replacements[s->replacement];

        hit->image = r->image;
        hit->x = s->x;
        hit->y = s->y;
        hit->w = s->w;
        hit->h = s->h;
        hit->srcX = s->srcX;
        hit->srcY = s->srcY;
        hit->nameW = r->w;
        hit->nameH = r->h;
    }
    return 1;
}

/* ---- the images: decoded on a thread of their own, kept at the framebuffer's scale ---- */

static SDL_Thread *worker;
static SDL_Mutex *lock;
static SDL_Condition *wake;
static int *queue;
static int queued, quitting;
static int imageScale = 4;
static unsigned epoch = 1;

/* One texel of an image (RGBA): 0 transparent, 1 semi-transparent, 2 opaque, as DuckStation reads a
   replacement's alpha. */
static int Kind(const uint8_t *p, int semi)
{
    if (!semi) {
        return p[3] >= 128 ? 2 : 0;
    }
    if (!p[0] && !p[1] && !p[2] && !p[3]) {
        return 0;
    }
    return p[3] <= 242 ? 1 : 2;
}

/* The image at w x h: each texel the average of those it covers that are drawn, drawn as most of them are. */
static uint8_t *Reduce(const uint8_t *in, int inW, int inH, int w, int h, int semi)
{
    uint8_t *out = SDL_malloc((size_t)w * h * 4);
    int x, y, i, j;

    for (y = 0; y < h; y++) {
        int top = (int)((int64_t)y * inH / h), bottom = SDL_max(top + 1, (int)((int64_t)(y + 1) * inH / h));
        for (x = 0; x < w; x++) {
            int left = (int)((int64_t)x * inW / w), right = SDL_max(left + 1, (int)((int64_t)(x + 1) * inW / w));
            unsigned counts[3] = {0, 0, 0}, sum[3] = {0, 0, 0};
            uint8_t *o = out + ((size_t)y * w + x) * 4;

            for (j = top; j < bottom; j++) {
                for (i = left; i < right; i++) {
                    const uint8_t *p = in + ((size_t)j * inW + i) * 4;
                    int kind = Kind(p, semi);

                    counts[kind]++;
                    if (kind) {
                        sum[0] += p[0];
                        sum[1] += p[1];
                        sum[2] += p[2];
                    }
                }
            }
            if (counts[0] * 2 > counts[0] + counts[1] + counts[2]) {
                o[0] = o[1] = o[2] = o[3] = 0;
            } else {
                unsigned drawn = counts[1] + counts[2];
                o[0] = (uint8_t)(sum[0] / drawn);
                o[1] = (uint8_t)(sum[1] / drawn);
                o[2] = (uint8_t)(sum[2] / drawn);
                o[3] = counts[1] > counts[2] ? 128 : 255;
            }
        }
    }
    return out;
}

static uint8_t *Decode(int image, int scale, int *w, int *h)
{
    size_t size;
    uint8_t *bytes, *rgba, *out;
    int inW, inH, channels;
    const Image *im = &images[image];

    bytes = PackRead(im->file, &size);
    if (!bytes) {
        return NULL;
    }
    rgba = stbi_load_from_memory(bytes, (int)size, &inW, &inH, &channels, 4);
    SDL_free(bytes);
    if (!rgba) {
        SDL_Log("HD textures: %s: %s", im->file, stbi_failure_reason());
        return NULL;
    }
    *w = SDL_min(inW, im->nameW * scale);
    *h = SDL_min(inH, im->nameH * scale);
    out = Reduce(rgba, inW, inH, *w, *h, im->semi);
    stbi_image_free(rgba);
    return out;
}

static int SDLCALL Work(void *data)
{
    (void)data;
    SDL_LockMutex(lock);
    for (;;) {
        int image, scale, w = 0, h = 0;
        unsigned at;
        uint8_t *pixels;

        while (!quitting && !queued) {
            SDL_WaitCondition(wake, lock);
        }
        if (quitting) {
            break;
        }
        image = queue[--queued];
        scale = imageScale;
        at = epoch;
        SDL_UnlockMutex(lock);
        pixels = Decode(image, scale, &w, &h);
        SDL_LockMutex(lock);
        if (at != epoch || images[image].state != IMAGE_QUEUED) {
            SDL_free(pixels); /* the scale changed meanwhile: asked for again */
            continue;
        }
        images[image].pixels = pixels;
        images[image].w = w;
        images[image].h = h;
        images[image].state = pixels ? IMAGE_READY : IMAGE_FAILED;
    }
    SDL_UnlockMutex(lock);
    return 0;
}

const uint8_t *HdTexImage(int image, int *w, int *h)
{
    const uint8_t *pixels = NULL;
    Image *im;

    if (image < 0 || image >= imageCount) {
        return NULL;
    }
    im = &images[image];
    if (!worker && im->state == IMAGE_NONE) {
        /* no thread (the web, or the tests' hidden runs, whose pictures must not depend on its timing) */
        im->pixels = Decode(image, imageScale, &im->w, &im->h);
        im->state = im->pixels ? IMAGE_READY : IMAGE_FAILED;
    }
    if (lock) {
        SDL_LockMutex(lock);
    }
    if (im->state == IMAGE_READY) {
        pixels = im->pixels;
        *w = im->w;
        *h = im->h;
    } else if (im->state == IMAGE_NONE) {
        im->state = IMAGE_QUEUED;
        queue[queued++] = image;
        SDL_SignalCondition(wake);
    }
    if (lock) {
        SDL_UnlockMutex(lock);
    }
    return pixels;
}

int HdTexImageCount(void)
{
    return imageCount;
}

unsigned HdTexEpoch(void)
{
    return epoch;
}

/* Every image to be decoded again (at another scale). */
static void Forget(void)
{
    int i;

    if (lock) {
        SDL_LockMutex(lock);
    }
    for (i = 0; i < imageCount; i++) {
        SDL_free(images[i].pixels);
        images[i].pixels = NULL;
        images[i].state = IMAGE_NONE;
    }
    queued = 0;
    epoch++;
    if (lock) {
        SDL_UnlockMutex(lock);
    }
}

void HdTexSetScale(int scale)
{
    scale = SDL_clamp(scale, 1, 16);
    if (scale != imageScale) {
        imageScale = scale;
        Forget();
    }
}

/* ---- opening and closing ---- */

static void Close(void)
{
    int i;

    if (worker) {
        SDL_LockMutex(lock);
        quitting = 1;
        SDL_SignalCondition(wake);
        SDL_UnlockMutex(lock);
        SDL_WaitThread(worker, NULL);
        worker = NULL;
        quitting = 0;
    }
    Forget();
    for (i = 0; i < replacementCount; i++) {
        SDL_free(replacements[i].name);
    }
    for (i = 0; i < imageCount; i++) {
        SDL_free(images[i].file);
    }
    for (i = 0; i < zipCount; i++) {
        SDL_free(zipEntries[i].name);
    }
    SDL_free(replacements);
    SDL_free(images);
    SDL_free(zipEntries);
    SDL_free(zip);
    SDL_free(queue);
    replacements = NULL;
    images = NULL;
    zipEntries = NULL;
    zip = NULL;
    queue = NULL;
    replacementCount = imageCount = zipCount = 0;
    zipSize = 0;
    source[0] = about[0] = folder[0] = zipFolder[0] = 0;
    if (textures) {
        memset(textures, 0, sizeof(Texture) * TEXTURE_SLOTS);
        textureCount = 0;
    }
}

int HdTexOpen(const char *path, char *message, size_t messageSize)
{
    const char *dot;
    int ok, i;

    Close();
    if (message && messageSize) {
        message[0] = 0;
    }
    if (!path || !path[0]) {
        return 0;
    }
    dot = strrchr(path, '.');
    ok = dot && !SDL_strcasecmp(dot, ".zip") ? OpenZip(path) : IsFolder(path) && OpenFolder(path);
    if (!ok || !replacementCount) {
        if (message) {
            SDL_snprintf(message, messageSize,
                         !ok ? "Not a texture pack: choose its .zip, or the folder with its config.yaml and "
                               "replacements folder."
                             : "No replacement textures in it (texupload-... or texpage-... images).");
        }
        Close();
        return 0;
    }
    SDL_strlcpy(source, path, sizeof(source));
    queue = SDL_malloc(sizeof(int) * (size_t)imageCount);
    for (i = 0; i < uploadCount; i++) {
        HashUpload(&uploads[i]);
    }
#ifndef __EMSCRIPTEN__
    if (!portConfig.hidden) {
        if (!lock) {
            lock = SDL_CreateMutex();
            wake = SDL_CreateCondition();
        }
        worker = SDL_CreateThread(Work, "hdtex", NULL);
    }
#endif
    if (message) {
        SDL_snprintf(message, messageSize, "%d replacement textures (%d images).", replacementCount, imageCount);
    }
    return replacementCount;
}

const char *HdTexSource(void)
{
    return source;
}

int HdTexCount(void)
{
    return replacementCount;
}

const char *HdTexAbout(void)
{
    return about;
}

int HdTexActive(void)
{
    static int tried;

    if (!portSettings.hdTextures) {
        return 0;
    }
    if (!tried) {
        /* the settings' pack, opened the first time it is drawn */
        tried = 1;
        if (!source[0] && portSettings.hdPackPath[0]) {
            HdTexOpen(portSettings.hdPackPath, NULL, 0);
        }
    }
    return replacementCount > 0;
}
