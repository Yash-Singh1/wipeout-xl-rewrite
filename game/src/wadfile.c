#include "functions.h"

/* One directory entry of a wad as the loader reads it: the name, a field the loader
   does not keep, the flag byte. The entry size goes straight into wad->sizes. */
typedef struct WadEntryHeader {
    char name[16];
    int ignoredEntryWord;
    char flag;
    char pad15[11];
} WadEntryHeader;

/* loadFrame, currentWad, lastWadCurrent, loadTicks and linePrims are the load screen's state
   and are defined in progress.c (declared in globals.h). */


WADFILE *openwadfile(char *name)
{
    WadEntryHeader header;
    WADFILE       *wad;
    int            handle;
    int            entry;

    wad = DAlloc(heap, 0x108);
    handle = WDopen(name, 0, 0);
    wad->handle = handle;
    if (handle == -1) {
        printf("wadfile.c:openwadfile(): can't open %s\n", name);
        return 0;
    }
    WDread(handle, (u_char *)&wad->count, 2);
    entry = 0;
    if (wad->count > 0) {
        do {
            WDread(wad->handle, (u_char *)header.name, 0x10);
            WDread(wad->handle, (u_char *)&header.ignoredEntryWord, 4);
            WDread(wad->handle, (u_char *)&wad->sizes[entry], 4);
            WDread(wad->handle, (u_char *)&header.flag, 1);
            entry += 1;
        } while (entry < wad->count);
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


WADFILE *openwadfilenvc(char *name)
{
    WadEntryHeader header;
    WADFILE       *wad;
    int            handle;
    int            entry;

    wad = DAlloc(heap, 0x108);
    handle = WDopen(name, 0, 0);
    wad->handle = handle;
    if (handle == -1) {
        printf("wadfile.c:openwadfilenvc(): can't open %s\n", name);
        return 0;
    }
    WDread(handle, (u_char *)&wad->count, 2);
    entry = 0;
    if (wad->count > 0) {
        do {
            WDread(wad->handle, (u_char *)header.name, 0x10);
            WDread(wad->handle, (u_char *)&header.ignoredEntryWord, 4);
            WDread(wad->handle, (u_char *)&wad->sizes[entry], 4);
            WDread(wad->handle, (u_char *)&header.flag, 1);
            entry += 1;
        } while (entry < wad->count);
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
    WDclose(wad->handle);
    DFree(heap, (char *)wad);
}


void closewadfile(WADFILE *wad)
{
    do { } while (0);
    while (loadFrame < 0);
    WDclose(wad->handle);
    DFree(heap, (char *)wad);
    VSyncCallback(0);
    DrawSync(0);
    Close2DParticle();
    DFree(reserveHeap, (char *)linePrims);
}


void readnextfile(WADFILE *wad, char *dest)
{
    WDread(wad->handle, (u_char *)dest, wad->sizes[wad->current]);
    wad->current = (s16)((u16)wad->current + 1);
}
