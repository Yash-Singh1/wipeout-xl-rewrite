#include "functions.h"
#include "trackasm.h"
#include "ttf.h"

int  OpenEvent(u_long desc, int spec, int mode, int func);
int  EnableEvent(int event);
int  CloseEvent(int event);
void _card_info(int channel);

extern HeapBlock *heap;
extern short        CurrentScreen;
extern Texture     *textureTable[];
extern int         lastWadCurrent[];
extern int         particleX[];
extern int        *frameValues;

/* Double-buffered polygon pools for the three tile levels. */
POLY_FT4 *TrackLoPolys;
extern POLY_FT4 *TrackLoPolysAlt;
POLY_FT4 *TrackMedPolys;
extern POLY_FT4 *TrackMedPolysAlt;
POLY_FT4 *TrackHiPolys;
extern POLY_FT4 *TrackHiPolysAlt;

Surface tileSurfaces[24][2];

short loTileCount;
short medTileCount;
short hiTileCount;
void AsmLowResTile(TexTemplate *template, short index, short reversed)
{
    Texture *texture;
    Tile    *tile;

    texture = textureTable[template[index].tex1[0]];
    tile = &tileSurfaces[index][reversed].low[0];
    tile->cba = texture->cba;
    tile->tsb = texture->tsb;
    if (reversed) {
        tile->u0 = texture->u1;
        tile->u1 = texture->u0;
        tile->u2 = texture->u3;
        tile->u3 = texture->u2;
        tile->v0 = texture->v1;
        tile->v1 = texture->v0;
        tile->v2 = texture->v3;
        tile->v3 = texture->v2;
    } else {
        tile->u0 = texture->u0;
        tile->u1 = texture->u1;
        tile->u2 = texture->u2;
        tile->u3 = texture->u3;
        tile->v0 = texture->v0;
        tile->v1 = texture->v1;
        tile->v2 = texture->v2;
        tile->v3 = texture->v3;
    }
}


void AsmMediumResTile(TexTemplate *template, short index, short reversed)
{
    Texture *texture;
    Tile    *tile;
    short    i;
    short    tex;

    for (i = 0; i < 4; i++) {
        if (reversed) {
            tex = ((i & 2) + 1) - (i & 1);
        } else {
            tex = i;
        }
        texture = textureTable[template[index].tex4[tex]];
        tile = &tileSurfaces[index][reversed].med[i];
        tile->cba = texture->cba;
        tile->tsb = texture->tsb;
        if (reversed) {
            tile->u0 = texture->u1;
            tile->u1 = texture->u0;
            tile->u2 = texture->u3;
            tile->u3 = texture->u2;
            tile->v0 = texture->v1;
            tile->v1 = texture->v0;
            tile->v2 = texture->v3;
            tile->v3 = texture->v2;
        } else {
            tile->u0 = texture->u0;
            tile->u1 = texture->u1;
            tile->u2 = texture->u2;
            tile->u3 = texture->u3;
            tile->v0 = texture->v0;
            tile->v1 = texture->v1;
            tile->v2 = texture->v2;
            tile->v3 = texture->v3;
        }
    }
}


void AsmHighResTile(TexTemplate *template, short index, short reversed)
{
    Texture *texture;
    Tile    *tile;
    short    i;
    short    tex;

    for (i = 0; i < 16; i++) {
        if (reversed) {
            tex = (3 - (i & 3)) + (i & 12);
        } else {
            tex = i;
        }
        texture = textureTable[template[index].tex16[tex]];
        tile = &tileSurfaces[index][reversed].high[i];
        tile->cba = texture->cba;
        tile->tsb = texture->tsb;
        if (reversed) {
            tile->u0 = texture->u1;
            tile->u1 = texture->u0;
            tile->u2 = texture->u3;
            tile->u3 = texture->u2;
            tile->v0 = texture->v1;
            tile->v1 = texture->v0;
            tile->v2 = texture->v3;
            tile->v3 = texture->v2;
        } else {
            tile->u0 = texture->u0;
            tile->u1 = texture->u1;
            tile->u2 = texture->u2;
            tile->u3 = texture->u3;
            tile->v0 = texture->v0;
            tile->v1 = texture->v1;
            tile->v2 = texture->v2;
            tile->v3 = texture->v3;
        }
    }
}


void AsmGenerateTiles(TexTemplate *tiles, short tileCount)
{
    short tile;

    for (tile = 0; tile < tileCount; tile++) {
        AsmLowResTile(tiles, tile, 0);
        AsmLowResTile(tiles, tile, 1);
        AsmMediumResTile(tiles, tile, 0);
        AsmMediumResTile(tiles, tile, 1);
        AsmHighResTile(tiles, tile, 0);
        AsmHighResTile(tiles, tile, 1);
    }

    TrackLoPolys = DAlloc(heap, 0x5780);
    TrackLoPolysAlt = DAlloc(heap, 0x5780);
    TrackMedPolys = DAlloc(heap, 0x1180);
    TrackMedPolysAlt = DAlloc(heap, 0x1180);
    TrackHiPolys = DAlloc(heap, 0x4600);
    TrackHiPolysAlt = DAlloc(heap, 0x4600);

    for (tile = 0; tile < 0x230; tile++) {
        SetPolyFT4(TrackLoPolys + tile);
        SetPolyFT4(TrackLoPolysAlt + tile);
    }

    for (tile = 0; tile < 0x70; tile++) {
        SetPolyFT4(TrackMedPolys + tile);
        SetPolyFT4(TrackMedPolysAlt + tile);
    }

    for (tile = 0; tile < 0x1c0; tile++) {
        SetPolyFT4(TrackHiPolys + tile);
        SetPolyFT4(TrackHiPolysAlt + tile);
    }
}


void AsmNextFrame(void)
{
    loTileCount = 0;
    medTileCount = 0;
    hiTileCount = 0;
    if (CurrentScreen == 0) {
        frameValues = lastWadCurrent;
    } else {
        frameValues = particleX;
    }
}
