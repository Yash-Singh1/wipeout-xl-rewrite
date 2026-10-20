#include "functions.h"

/* One directory entry of a wad as the loader reads it: the name, a field the loader
   does not keep, the flag byte. The entry size goes straight into wad->sizes. */
typedef struct WadEntryHeader {
    char name[16];
    int ignoredEntryWord;
    char flag;
    char pad15[11];
} WadEntryHeader;

/* $gp-relative globals are tentative definitions; the rest are extern. */
s16        loadFrame;
WADFILE   *currentWad;
int       lastWadCurrent;
int       loadTicks;
char      *linePrims;
extern HeapBlock *heap;
extern HeapBlock *reserveHeap;

/* functions.h declares WDclose without arguments; the retail caller passes the wad handle,
   which the callee ignores, so the call goes through this declaration. */
void WDclose__wadfile(int handle) __asm__("WDclose");


WADFILE *openwadfile(int fileNo)
{
    WadEntryHeader header;
    WADFILE       *wad;
    int            handle;

    wad = DAlloc(heap, 0x108);
    handle = WDopen(fileNo, 0, 0);
    wad->handle = handle;
    if (handle == -1) {
        printf("wadfile.c:openwadfile(): can't open %s\n", fileNo);
        return 0;
    }
    WDread(handle, (u_char *)&wad->count, 2);
    fileNo = 0;
    if (wad->count > 0) {
        do {
            WDread(wad->handle, (u_char *)header.name, 0x10);
            WDread(wad->handle, (u_char *)&header.ignoredEntryWord, 4);
            WDread(wad->handle, (u_char *)&wad->sizes[fileNo], 4);
            WDread(wad->handle, (u_char *)&header.flag, 1);
            fileNo += 1;
        } while (fileNo < wad->count);
    }
    wad->current = 0;
    currentWad = wad;
    lastWadCurrent = 0;
    loadTicks = 0;
    InitPop();
    VSyncCallback(VsyncFunc);
    SetDispMask(1);
    return wad;
}


WADFILE *openwadfilenvc(int fileNo)
{
    WadEntryHeader header;
    WADFILE       *wad;
    int            handle;

    wad = DAlloc(heap, 0x108);
    handle = WDopen(fileNo, 0, 0);
    wad->handle = handle;
    if (handle == -1) {
        printf("wadfile.c:openwadfilenvc(): can't open %s\n", fileNo);
        return 0;
    }
    WDread(handle, (u_char *)&wad->count, 2);
    fileNo = 0;
    if (wad->count > 0) {
        do {
            WDread(wad->handle, (u_char *)header.name, 0x10);
            WDread(wad->handle, (u_char *)&header.ignoredEntryWord, 4);
            WDread(wad->handle, (u_char *)&wad->sizes[fileNo], 4);
            WDread(wad->handle, (u_char *)&header.flag, 1);
            fileNo += 1;
        } while (fileNo < wad->count);
    }
    wad->current = 0;
    currentWad = wad;
    lastWadCurrent = 0;
    loadTicks = 0;
    SetDispMask(1);
    return wad;
}


void closewadfilenvc(WADFILE *wad)
{
    WDclose__wadfile(wad->handle);
    DFree(heap, (char *)wad);
}


void closewadfile(WADFILE *wad)
{
    do { } while (0);
    while (loadFrame < 0);
    WDclose__wadfile(wad->handle);
    DFree(heap, (char *)wad);
    VSyncCallback(0);
    DrawSync(0);
    Close2DParticle();
    DFree(reserveHeap, linePrims);
}


void readnextfile(WADFILE *wad, char *dest)
{
    WDread(wad->handle, (u_char *)dest, wad->sizes[wad->current]);
    wad->current = (s16)((u16)wad->current + 1);
}
