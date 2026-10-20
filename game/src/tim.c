#include "functions.h"
#include "tim.h"
#define Clut4Bit 0
#define Clut8Bit 1
#define CLUT4 0x00
#define CLUT8 0x01
#define TPAGE(x, y) ((x >> 6) + ((y & 0x100) >> 4))
#define CBA(x, y) ((y << 6) | x)
#define TSB(c, a, t) ((c << 7) | (a << 5) | t)
#define TextureHOffset(x) (x & 0xffc0)
#define TextureVOffset(y) (y & 0x100)
#define ClutType(t) (t->flags & 7)




extern HeapBlock *heap;
extern HeapBlock *reserveHeap;
extern char errorString[];

/* Next free CLUT slot and texture cell in VRAM for auto-placed textures. */
extern short gClutX;
extern short gClutY;
extern short gTextureX;
extern short gTextureY;

void readnextfile(WADFILE *wad, char *buffer);

/* Uploads an in-memory TIM and returns its texture record. */
Texture *loadtexcom(Tim *tim, short translucent)
{
    TimClut4 *tc4;
    TimClut8 *tc8;
    Texture  *texture;
    RECT      rect;
    int       x, y;

    if (ClutType(tim) == Clut4Bit) {
        tc4 = (TimClut4 *)tim;
        texture = DAlloc(heap, sizeof(Texture));
        texture->type = CLUT4;

        if ((!tc4->textureX) && (!tc4->textureY)) {
            tc4->clutX = gClutX;
            tc4->clutY = gClutY;
            tc4->clutW = 16;
            tc4->clutH = 1;
            tc4->textureX = gTextureX;
            tc4->textureY = gTextureY;

            gClutX += 16;
            if (gClutX >= 384) {
                gClutX = 320;
                gClutY += 1;
            }

            gTextureY += 32;
            if (gTextureY >= 256) {
                gTextureX += 8;
                gTextureY = 0;
            }
        }

        texture->clutX = tc4->clutX;
        texture->clutY = tc4->clutY;
        texture->clutW = tc4->clutW;
        texture->clutH = tc4->clutH;

        texture->textureX = tc4->textureX;
        texture->textureY = tc4->textureY;
        texture->textureW = tc4->textureW;
        texture->textureH = tc4->textureH;

        x = tc4->textureX - TextureHOffset(tc4->textureX);
        y = tc4->textureY - TextureVOffset(tc4->textureY);

        texture->u0 = (x << 2);
        texture->v0 = (y);
        texture->u1 = ((x + tc4->textureW) << 2) - 1;
        texture->v1 = (y);
        texture->u2 = (x << 2);
        texture->v2 = (y + tc4->textureH) - 1;
        texture->u3 = ((x + tc4->textureW) << 2) - 1;
        texture->v3 = (y + tc4->textureH) - 1;

        texture->tsb = TSB(Clut4Bit, translucent, TPAGE(texture->textureX, texture->textureY));
        texture->cba = CBA(texture->clutX >> 4, texture->clutY);

        rect.x = tc4->clutX;
        rect.y = tc4->clutY;
        rect.w = tc4->clutW;
        rect.h = tc4->clutH;
        LoadImage(&rect, (u_long *)(&tc4->clut));
        DrawSync(0);

        rect.x = tc4->textureX;
        rect.y = tc4->textureY;
        rect.w = tc4->textureW;
        rect.h = tc4->textureH;
        LoadImage(&rect, (u_long *)(tc4 + 1));
        DrawSync(0);
    } else if (ClutType(tim) == Clut8Bit) {
        tc8 = (TimClut8 *)tim;
        texture = DAlloc(heap, sizeof(Texture));
        texture->type = CLUT8;

        if ((!tc8->textureX) && (!tc8->textureY)) {
            Error("tim.c:LoadTexture(): Cannot auto place 8 bit textures", 2);
        }

        texture->clutX = tc8->clutX;
        texture->clutY = tc8->clutY;
        texture->clutW = tc8->clutW;
        texture->clutH = tc8->clutH;

        texture->textureX = tc8->textureX;
        texture->textureY = tc8->textureY;
        texture->textureW = tc8->textureW;
        texture->textureH = tc8->textureH;

        x = tc8->textureX - TextureHOffset(tc8->textureX);
        y = tc8->textureY - TextureVOffset(tc8->textureY);

        texture->u0 = (x << 1);
        texture->v0 = (y);
        texture->u1 = ((x + tc8->textureW) << 1) - 1;
        texture->v1 = (y);
        texture->u2 = (x << 1);
        texture->v2 = (y + tc8->textureH) - 1;
        texture->u3 = ((x + tc8->textureW) << 1) - 1;
        texture->v3 = (y + tc8->textureH) - 1;

        texture->tsb = TSB(Clut8Bit, translucent, TPAGE(texture->textureX, texture->textureY));
        texture->cba = CBA(texture->clutX >> 4, texture->clutY);

        rect.x = tc8->clutX;
        rect.y = tc8->clutY;
        rect.w = tc8->clutW;
        rect.h = tc8->clutH;
        LoadImage(&rect, (u_long *)(&tc8->clut));
        DrawSync(0);

        rect.x = tc8->textureX;
        rect.y = tc8->textureY;
        rect.w = tc8->textureW;
        rect.h = tc8->textureH;
        LoadImage(&rect, (u_long *)(tc8 + 1));
        DrawSync(0);
    } else {
        texture = 0;
    }

    return texture;
}


/* Loads a TIM file from the disc and uploads it. */
Texture *LoadTexture(char *name, short translucent)
{
    Tim     *tim;
    Texture *texture;
    int     length;

    length = FileLength(name);
    if (length <= 0) {
        sprintf(errorString, "Tim.c:LoadTexture: File %s not found\n", name);
        Error(errorString, 0);
        return;
    }

    tim = DAlloc(reserveHeap, length + 32);

    if (!tim) {
        sprintf(errorString, "Tim.c:LoadTexture(): Failed to allocate memory for file %s\n", name);
        Error(errorString, 2);
    }

    if (LoadFile(name, (char *)tim) != length) {
        sprintf(errorString, "Tim.c:LoadTexture(): Failed to load file %s\n", name);
        Error(errorString, 2);
    }

    texture = loadtexcom(tim, translucent);

    DFree(reserveHeap, (char *)tim);

    return texture;
}


/* Uploads the next TIM entry of a WAD file. */
Texture *WADLoadTexture(WADFILE *wad, short translucent)
{
    Tim     *tim;
    Texture *texture;

    tim = DAlloc(reserveHeap, wad->sizes[wad->current] + 32);
    readnextfile(wad, (char *)tim);
    texture = loadtexcom(tim, translucent);
    DFree(reserveHeap, (char *)tim);

    return texture;
}
