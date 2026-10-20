#include "functions.h"
#include "trackasm.h"
#include "ttf.h"

extern short        CurrentScreen;
extern Texture     *textureTable[];
/* Port: the console stored the address of lastWadCurrent or of particleX here (as int *); nothing
   reads it back. */
extern void       *frameValues;

/* Double-buffered polygon pools for the three tile levels: one pool per screen buffer.
   Port: these were the adjacent symbols TrackLoPolys/TrackLoPolysAlt, TrackMedPolys/...Alt and
   TrackHiPolys/...Alt, indexed by CurrentScreen in the assembly tile drawers. */
extern POLY_FT4 *TrackLoPolys[2];
extern POLY_FT4 *TrackMedPolys[2];
extern POLY_FT4 *TrackHiPolys[2];

extern Surface tileSurfaces[24][2];

extern short loTileCount;
extern short medTileCount;
extern short hiTileCount;
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

    TrackLoPolys[0] = DAlloc(heap, 0x5780);
    TrackLoPolys[1] = DAlloc(heap, 0x5780);
    TrackMedPolys[0] = DAlloc(heap, 0x1180);
    TrackMedPolys[1] = DAlloc(heap, 0x1180);
    TrackHiPolys[0] = DAlloc(heap, 0x4600);
    TrackHiPolys[1] = DAlloc(heap, 0x4600);

    for (tile = 0; tile < 0x230; tile++) {
        SetPolyFT4(TrackLoPolys[0] + tile);
        SetPolyFT4(TrackLoPolys[1] + tile);
    }

    for (tile = 0; tile < 0x70; tile++) {
        SetPolyFT4(TrackMedPolys[0] + tile);
        SetPolyFT4(TrackMedPolys[1] + tile);
    }

    for (tile = 0; tile < 0x1c0; tile++) {
        SetPolyFT4(TrackHiPolys[0] + tile);
        SetPolyFT4(TrackHiPolys[1] + tile);
    }
}


void AsmNextFrame(void)
{
    loTileCount = 0;
    medTileCount = 0;
    hiTileCount = 0;
    if (CurrentScreen == 0) {
        frameValues = &lastWadCurrent;
    } else {
        frameValues = &particleX;
    }
}
