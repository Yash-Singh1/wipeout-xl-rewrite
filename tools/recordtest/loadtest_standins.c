/* The parts of the machine the game's loaders run on that the load test replaces (see loadtest.c).

   The CD file system (iso9660.c) is replaced by reads of the extracted disc directory: iso_open,
   iso_read, iso_filesize, iso_get_file_info and iso_close keep the original's semantics (names are
   upper case, directories at most 8 and file names at most 12 characters, whole 2048-byte sectors,
   the same bound test in iso_read) and nothing else. The PsyQ libraries that are not computation
   (CD drive, sound, controller, memory card, event handling, the draw hardware) do nothing. The
   functions below are the ones the load sequence calls; any other undefined function stops the
   program when it is called (build_loadtest.py). */
#include "functions.h"
#include <ctype.h>
#include <stdarg.h>
#include <dirent.h>
#include <strings.h>
#include <sys/stat.h>

char loadtestDiscRoot[512];     /* the directory that holds COMMON, TEXTURES, TRACK01 ... */
int loadtestPathTableBytes = 2048;
int loadtestFilesOpened;

/* ------------------------------------------------------------------------------------------------
   The CD file system
   ---------------------------------------------------------------------------------------------- */
typedef struct DiscFile {
    int  fd;
    int  size;
    int  loc;       /* the first sector: any number, the files do not share sectors */
    int  fp;        /* the current sector, as in IsoFile */
    FILE *file;
} DiscFile;

static DiscFile discFiles[16];

extern void *HarnessAllocAt(HeapBlock *head, int size, const char *file, const char *text);

int iso_init(HeapBlock *heap)
{
    extern u_char *pathTable;
    extern HeapBlock *isoHeap;
    extern int initIso;

    if (initIso != 0) {
        return initIso;
    }
    isoHeap = heap;
    /* The volume descriptor of the disc says the path table is 214 bytes; the original reads whole sectors. */
    pathTable = HarnessAllocAt(heap, loadtestPathTableBytes, "iso9660.c", "pathTable = DAlloc(heap, pathBytes);");
    initIso = pathTable != 0;
    return initIso;
}

void close_iso_events(void) {}
u_char *iso_toc(void) { return 0; }

/* "c:\wipeout2\common\startwad.wad" -> <disc root>/COMMON/STARTWAD.WAD (the disc is upper case). */
static int HostPath(const char *pathname, char *out, int outSize)
{
    char dirs[8][10];
    char name[13];
    int levels = 0;
    int n = 0;
    const char *c = pathname;
    int i;

    while (*c != '\\') {
        if (*c == 0) {
            return 0;
        }
        c++;
    }
    while (*c == '\\') {
        c++;
    }
    while (*c != 0) {
        if (*c == '\\') {
            while (*c == '\\') {
                c++;
            }
            name[n] = 0;
            if (n > 8 || levels == 8) {
                return 0;
            }
            strcpy(dirs[levels++], name);
            n = 0;
        }
        if (n >= 12) {
            return 0;
        }
        name[n++] = toupper((unsigned char)*c++);
    }
    name[n] = 0;
    if (n < 1 || n > 12) {
        return 0;
    }
    /* the first directory is the disc's WIPEOUT2 directory: loadtestDiscRoot is that directory */
    if (levels < 1 || strcmp(dirs[0], "WIPEOUT2")) {
        return 0;
    }
    snprintf(out, outSize, "%s", loadtestDiscRoot);
    for (i = 1; i < levels; i++) {
        strncat(out, "/", outSize - strlen(out) - 1);
        strncat(out, dirs[i], outSize - strlen(out) - 1);
    }
    strncat(out, "/", outSize - strlen(out) - 1);
    strncat(out, name, outSize - strlen(out) - 1);
    return 1;
}

int iso_get_file_info(char *pathname, IsoFileInfo *fi)
{
    char path[1024];
    struct stat st;

    if (!HostPath(pathname, path, sizeof(path)) || stat(path, &st) || !S_ISREG(st.st_mode)) {
        return 0;
    }
    fi->loc = 1;
    fi->size = (int)st.st_size;
    return 1;
}

int iso_open(char *pathname, int flags)
{
    char path[1024];
    int fd = 0;
    IsoFileInfo info;

    while (discFiles[fd].fd != 0) {
        fd++;
        if (fd == 16) {
            return -1;
        }
    }
    if (!iso_get_file_info(pathname, &info) || !HostPath(pathname, path, sizeof(path))) {
        return -1;
    }
    discFiles[fd].file = fopen(path, "rb");
    if (!discFiles[fd].file) {
        return -1;
    }
    discFiles[fd].fd = fd + 1;
    discFiles[fd].size = info.size;
    discFiles[fd].loc = 1 + 16 * fd;
    discFiles[fd].fp = discFiles[fd].loc;
    loadtestFilesOpened++;
    return fd + 1;
}

void iso_close(int fd)
{
    if (discFiles[fd - 1].fd != 0) {
        fclose(discFiles[fd - 1].file);
        discFiles[fd - 1].fd = 0;
    }
}

int iso_filesize(int fd)
{
    if (discFiles[fd - 1].fd != fd) {
        return -1;
    }
    return discFiles[fd - 1].size;
}

int iso_fileloc(int fd)
{
    if (discFiles[fd - 1].fd != fd) {
        return -1;
    }
    return discFiles[fd - 1].loc;
}

/* Reads nsec whole sectors; the bytes after the end of the file are zero, as on the disc. */
int iso_read(void *buffer, int nsec, int fd)
{
    DiscFile *f;
    long offset;
    size_t got;

    if (nsec == 0) {
        return 0;
    }
    if (discFiles[fd - 1].fd != fd) {
        return 0;
    }
    f = &discFiles[fd - 1];
    /* the original's (odd) bound test */
    if ((f->fp - f->loc + ((f->size + 2047) >> 11)) < nsec) {
        return 0;
    }
    offset = (long)(f->fp - f->loc) * 2048;
    memset(buffer, 0, (size_t)nsec * 2048);
    fseek(f->file, offset, SEEK_SET);
    got = fread(buffer, 1, (size_t)nsec * 2048, f->file);
    (void)got;
    f->fp += nsec;
    return nsec;
}


/* ------------------------------------------------------------------------------------------------
   Error: the game's version draws a message screen and exits
   ---------------------------------------------------------------------------------------------- */
extern void LoadtestExit(int status);
extern char errorString[];

char loadtestLastError[512];

void Error(char *string, short errorLevel)
{
    if (errorLevel >= 2) {
        snprintf(loadtestLastError, sizeof(loadtestLastError), "%s", string);
        fprintf(stderr, "game error: %s", string);
        if (string[0] && string[strlen(string) - 1] != '\n') {
            fprintf(stderr, "\n");
        }
        LoadtestExit(0);
    }
}
