#include "functions.h"
#define DIR_CACHE_SIZE 8192


/* Integer <-> BCD, as used by the CD drive's minute/second/sector fields. */
#define itob(i) (((i) / 10) * 16 + ((i) % 10))
#define btoi(b) ((((b) >> 4) * 10) + ((b) & 15))

/* Both-endian 32-bit field of a directory or path table record. */
#define DirRecordLong(rec, off) ((*(u_short *)((rec) + (off))) | (*(u_short *)((rec) + (off) + 2) << 16))

int  toupper(int c);

extern u_char toc_loc[];
extern u_char dirCache[];
extern IsoFile fd_tab[];

extern int track1_start_sec;
extern int initIso;
extern int num_of_open_files;
extern HeapBlock *isoHeap;
extern int cdReadEvent;
extern int cdErrorEvent;
extern u_char *pathTable;
extern int pathTableSize;
extern int cached_dir_loc;
extern int cached_dir_start_loc;
extern int cached_dir_file_len;
extern int cache_size;
#define WINDOW_BITS 13
#define WINDOW_SIZE (1 << WINDOW_BITS)
#define WINDOW_MASK (WINDOW_SIZE - 1)
#define LENGTH_BITS 4
#define MATCH_LENGTH_BIAS ((1 + WINDOW_BITS + LENGTH_BITS) / 9)

void exit(int status);

/* LZSS decoder. Flag bits are read MSB first; a set flag is a literal byte, a clear flag
   is a (position, length) back-reference into the sliding window. */


u_char *iso_toc(void)
{
    return toc_loc;
}


int iso_filesize(int fileNo)
{
    if (fd_tab[fileNo - 1].fd != fileNo) {
        return -1;
    }
    return fd_tab[fileNo - 1].fi.size;
}


int iso_fileloc(int fileNo)
{
    if (fd_tab[fileNo - 1].fd != fileNo) {
        return -1;
    }
    return fd_tab[fileNo - 1].fi.loc;
}


/* Port: an event handler returns int (OpenEvent's callback type); this one never returns. */
int my_error_handler(void)
{
    exit(-1);
}
void LogicalSectorToLoc(u_int sector, CdlLOC *loc)
{
    sector += track1_start_sec;
    loc->track = 1;
    loc->minute = itob(sector / 4500);
    loc->second = itob((sector % 4500) / 75);
    loc->sector = itob(sector % 75);
}


int iso_init(HeapBlock *heap)
{
    /* Port: CdRead fills the buffer a word at a time and the volume descriptor is read as words,
       so the sector buffer is word aligned (a plain u_char array is not guaranteed to be). */
    union { u_char bytes[2048]; u_int words[2048 / 4]; } sector;
    CdlLOC loc;
    int    pathBytes;
    int    i;

    if (initIso != 0) {
        return initIso;
    }
    EnterCriticalSection();
    cdReadEvent = OpenEvent(0xF0000003, 0x20, 0x2000, 0);
    cdErrorEvent = OpenEvent(0xF0000003, 0x8000, 0x1000, my_error_handler);
    EnableEvent(cdReadEvent);
    EnableEvent(cdErrorEvent);
    ExitCriticalSection();
    CdGetToc2(1, (CdlLOC *)toc_loc);
    track1_start_sec = btoi(toc_loc[5]) * 75 + btoi(toc_loc[6]);
    LogicalSectorToLoc(0x10, &loc);
    CdControlF(0x15, (u_char *)&loc);
    WaitEvent(cdReadEvent);
    CdRead(1, sector.words, 0x80);
    CdReadSync(0, 0);
    if (sector.bytes[0] == 1) {
        isoHeap = heap;
        pathBytes = ((*(int *)(sector.bytes + 0x84) + 0x7FF) >> 11) << 11;
        pathTableSize = *(int *)(sector.bytes + 0x84);
        pathTable = DAlloc(heap, pathBytes);
        if (pathTable != 0) {
            LogicalSectorToLoc(*(int *)(sector.bytes + 0x8C), &loc);
            CdControlF(0x15, (u_char *)&loc);
            WaitEvent(cdReadEvent);
            CdRead(pathBytes / 0x800, (u_long *)pathTable, 0x80);
            CdReadSync(0, 0);
            num_of_open_files = 0;
            for (i = 15; i >= 0; i--) {
                fd_tab[i].fd = 0;
            }
            initIso = 1;
            return 1;
        }
    }
    return 0;
}


void close_iso_events(void)
{
    if (initIso != 0) {
        DFree(isoHeap, (char *)pathTable);
        EnterCriticalSection();
        CloseEvent(cdReadEvent);
        CloseEvent(cdErrorEvent);
        ExitCriticalSection();
        initIso = 0;
    }
}


u_char *NextPath(u_char *dirRecord)
{
    u_char *next;

    next = dirRecord + ((*dirRecord + 9) & 0xFE);
    return (u_char *)((uintptr_t)next & -(uintptr_t)((u_long)(next - pathTable) < (u_long)pathTableSize));
}


int match_dir(char *dirname, char *testname, u_char test_len)
{
    int  i;
    int  len;
    char *dn;
    char *tn;

    dn = dirname;
    tn = testname;
    len = (int)strlen(dirname);
    if (len != test_len) {
        return 0;
    }
    for (i = 0; i < len; i++) {
        if ((*(dn++)) != (*(tn++))) {
            return 0;
        }
    }
    return 1;
}


int match_file(char *filename, char *testname, u_char test_len)
{
    int  i;
    char *fn;
    char *tn;

    fn = filename;
    tn = testname;
    if (testname[test_len - 2] == ';') {
        test_len -= 2;
    }
    if (strlen(filename) != test_len) {
        return 0;
    }
    for (i = 0; i < test_len; i++) {
        if ((*(fn++)) != (*(tn++))) {
            return 0;
        }
    }
    return 1;
}


int search(char *filename, IsoFileInfo *fi)
{
    int     dirPos;
    u_char *dirRec;

    dirPos = 0;
    dirRec = dirCache;
    while (1) {
        if (!(dirRec[0x19] & 2)) {
            if (match_file(filename, dirRec + 0x21, dirRec[0x20])) {
                fi->loc = DirRecordLong(dirRec, 2);
                fi->size = DirRecordLong(dirRec, 10);
                return 1;
            }
        }
        dirPos += dirRec[0];
        if (((cached_dir_loc - cached_dir_start_loc) * 2048 + dirPos) >= cached_dir_file_len) {
            return -1;
        }
        dirRec = dirCache + dirPos;
        if (dirPos >= 0x2000) {
            return 0;
        }
        if (*dirRec == 0) {
            dirPos = ((dirPos + 0x7FF) >> 11) << 11;
            if (((cached_dir_loc - cached_dir_start_loc) * 2048 + dirPos) >= cached_dir_file_len) {
                return -1;
            }
            if (dirPos >= 0x2000) {
                return 0;
            }
            dirRec = dirCache + dirPos;
        }
    }
}


int find_file(char *filename, int loc, IsoFileInfo *fi)
{
    int      res;
    int      skipLoc;
    int      searchLoc;
    CdlLOC   cdLoc;
    u_char  *dirRec;

    if ((loc != cached_dir_loc) && (loc != cached_dir_start_loc)) {
        cache_size = DIR_CACHE_SIZE;
        LogicalSectorToLoc(loc, &cdLoc);
        CdControlF(0x15, (u_char *)&cdLoc);
        WaitEvent(cdReadEvent);
        CdRead(DIR_CACHE_SIZE / 2048, (u_long *)dirCache, 0x80);
        CdReadSync(0, 0);
        cached_dir_loc = cached_dir_start_loc = loc;
        dirRec = dirCache;
        cached_dir_file_len = DirRecordLong(dirRec, 10);
    }
    res = search(filename, fi);
    if (res == 1) {
        return 1;
    } else if (res == -1) {
        if (cached_dir_file_len <= DIR_CACHE_SIZE) {
            return 0;
        }
    }
    skipLoc = cached_dir_loc;
    searchLoc = cached_dir_start_loc;
    res = 0;
    while (res == 0) {
        if (searchLoc == skipLoc) {
            searchLoc += (DIR_CACHE_SIZE / 2048);
            if ((searchLoc - cached_dir_start_loc) * 2048 >= cached_dir_file_len) {
                return 0;
            }
        }
        cache_size = DIR_CACHE_SIZE;
        LogicalSectorToLoc(searchLoc, &cdLoc);
        CdControlF(0x15, (u_char *)&cdLoc);
        WaitEvent(cdReadEvent);
        CdRead(DIR_CACHE_SIZE / 2048, (u_long *)dirCache, 0x80);
        CdReadSync(0, 0);
        cached_dir_loc = searchLoc;
        res = search(filename, fi);
        searchLoc += (DIR_CACHE_SIZE / 2048);
    }
    if (res == 1) {
        return 1;
    } else {
        return 0;
    }
}


int iso_get_file_info(char *pathname, IsoFileInfo *fi)
{
    char    dirName[8][10];
    char    tmpString[13];
    char   *tmpS;
    char   *c;
    char   *filename;
    char   *searchS;
    int     numLevels = 0;
    u_char *path;
    int     fnameLen;
    int     prevDir;
    int     dirNum;
    int     pathLevel = 0;
    int    loc;

    if (PortOriginalFileInfo(pathname, &fi->loc, &fi->size)) {
        return 1; /* Port: a file made from the original Wipeout's, for its track raced as a guest */
    }
    if (PortRegionFileInfo(pathname, &fi->loc, &fi->size)) {
        return 1; /* Port: WipEout 2097's piece in place of Wipeout XL's (platform/region.c) */
    }
    if (PortSaturnFileInfo(pathname, &fi->loc, &fi->size)) {
        return 1; /* Port: made from the Sega Saturn's disc (platform/saturn.c) */
    }
    tmpS = tmpString;
    c = pathname;
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
            *tmpS = 0;
            if ((tmpS - tmpString) > 8) {
                return 0;
            }
            strcpy(dirName[numLevels], tmpString);
            numLevels++;
            tmpS = tmpString;
        }
        *tmpS++ = toupper(*c++);
    }
    *tmpS = 0;
    fnameLen = (int)(tmpS - tmpString);
    filename = tmpString;
    if ((fnameLen < 1) || (fnameLen > 12)) {
        return 0;
    }
    path = pathTable;
    if (pathLevel == 0) {
        loc = DirRecordLong(path, 2);
    }
    prevDir = 1;
    dirNum = 2;
    path = NextPath(path);
    for (pathLevel = 0; pathLevel < numLevels; pathLevel++) {
        searchS = dirName[pathLevel];
        while (path != 0) {
            if ((*(u_short *)(path + 6) == prevDir) && match_dir(searchS, path + 8, path[0])) {
                loc = DirRecordLong(path, 2);
                break;
            }
            path = NextPath(path);
            dirNum++;
        }
        if (path == 0) {
            return 0;
        }
        prevDir = dirNum;
        dirNum++;
        path = NextPath(path);
    }
    return find_file(filename, loc, fi);
}


int iso_open(char *pathname, int flags)
{
    int fd;

    printf("\niso_open: %s\n\n", pathname);
    fd = 0;
    while (fd_tab[fd].fd != 0) {
        fd++;
        if (fd == 16) {
            return -1;
        }
    }
    if (iso_get_file_info(pathname, &fd_tab[fd].fi)) {
        fd_tab[fd].fd = fd + 1;
        num_of_open_files++;
        fd_tab[fd].fp = fd_tab[fd].fi.loc;
        return fd + 1;
    } else {
        return -1;
    }
}


void iso_close(int fd)
{
    if (fd_tab[fd - 1].fd != 0) {
        fd_tab[fd - 1].fd = 0;
        num_of_open_files--;
    }
}


int iso_read(void *buffer, int nsec, int fd)
{
    CdlLOC cdLoc;
    int    check = -1;

    if (nsec == 0) {
        return 0;
    }
    if (fd_tab[fd - 1].fd != fd) {
        return 0;
    }
    fd--;
    if ((fd_tab[fd].fp - fd_tab[fd].fi.loc + ((fd_tab[fd].fi.size + 2047) >> 11)) < nsec) {
        return 0;
    }
    if (PortOriginalRead(fd_tab[fd].fp, nsec, buffer) || PortRegionRead(fd_tab[fd].fp, nsec, buffer) ||
        PortSaturnRead(fd_tab[fd].fp, nsec, buffer)) {
        fd_tab[fd].fp += nsec; /* Port: a file made in memory (PortOriginalFileInfo, PortRegionFileInfo, PortSaturn...) */
        return nsec;
    }
    LogicalSectorToLoc(fd_tab[fd].fp, &cdLoc);
    while (check == -1) {
        CdControlF(0x15, (u_char *)&cdLoc);
        WaitEvent(cdReadEvent);
        CdRead(nsec, buffer, 0x80);
        while ((check = CdReadSync(1, 0)) > 0) {
        }
    }
    fd_tab[fd].fp += nsec;
    return nsec;
}
