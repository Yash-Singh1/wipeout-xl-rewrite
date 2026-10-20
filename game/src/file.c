#include "functions.h"


void InitCDFS(HeapBlock *heap)
{
    CdInit();
    iso_init(heap);
}
int FileLength(char *name)
{
    IsoFileInfo entry;

    if (iso_get_file_info(name, &entry)) {
        return entry.size;
    }
    return 0;
}


int LoadFile(char *name, char *buffer)
{
    int     fd;
    int     length;
    int     sectors;
    int     i;
    int     leftOver;
    u_int    sectorBuf[2048 / 4];
    u_int   *dst;
    u_int   *src;

    fd = iso_open(name, 0);
    if (fd == -1) {
        return 0;
    }

    length = iso_filesize(fd);
    sectors = length >> 11;

    if (iso_read(buffer, sectors, fd) != sectors) {
        iso_close(fd);
        return 0;
    }
    if (iso_read(sectorBuf, 1, fd) != 1) {
        iso_close(fd);
        return 0;
    }
    leftOver = (length - sectors * 2048 + 3) >> 2;
    dst = (u_int *)(buffer + sectors * 2048);
    src = sectorBuf;
    for (i = 0; i < leftOver; i++) {
        *dst++ = *src++;
    }
    iso_close(fd);
    return length;
}


void SaveFile(char *name, char *buffer, int length)
{
}
