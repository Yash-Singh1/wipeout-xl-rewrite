#include "functions.h"
#include "ttf.h"

extern char        errorString[];
extern HeapBlock *heap;

TexTemplate *LoadTtfFile(char *file, short *count, short libraryTextures)
{
    int   length;
    char  *ttfFile;
    short *tex;
    short  i;

    length = FileLength(file);
    if (length <= 0) {
        sprintf(errorString, "Ttf.c:LoadTtfFile: Ttf file %s not found\n", file);
        Error(errorString, Fatal);
    }

    ttfFile = DAlloc(heap, length + 32);
    if (!ttfFile) {
        /* Formats into the message literal, not errorString (behaviour kept). */
        sprintf("Ttf.c:LoadTtfFile: Failed to allocate memory for file %s\n", file);
        Error(errorString, Fatal);
    }

    if (LoadFile(file, ttfFile) != length) {
        sprintf(errorString, "Ttf.c:LoadTtfFile: Failed to load file %s\n", file);
        DFree(heap, ttfFile);
        Error(errorString, Fatal);
    }

    tex = (short *)ttfFile;
    while (tex < (short *)(ttfFile + length)) {
        for (i = 0; i < 21; i++) {
            *tex = ((*tex & 0x00ff) << 8) + ((*tex & 0xff00) >> 8);
            tex++;
        }
        (*count)++;
    }

    return (TexTemplate *)ttfFile;
}


/* Same byte swap as LoadTtfFile, reading the next entry of an open WAD. */
TexTemplate *WADLoadTtfFile(WADFILE *wad, short *count)
{
    int     length;
    u_short *ttfFile;
    u_short *tex;
    u_short *end;
    short    i;
    u_short  v;

    length = wad->sizes[wad->current];
    ttfFile = (u_short *)DAlloc(heap, length + 32);
    readnextfile(wad, (char *)ttfFile);

    tex = ttfFile;
    if (tex < (u_short *)((char *)ttfFile + length)) {
        end = (u_short *)((char *)ttfFile + length);
        do {
            i = 0;
            do {
                v = *tex;
                *tex++ = ((v & 0x00ff) << 8) + (v >> 8);
                i++;
            } while (i < 21);
            (*count)++;
        } while (tex < end);
    }

    return (TexTemplate *)ttfFile;
}
