/* Drive the texture upload clock with a tiny compressed sequence, including track-specific cadence. */
#include "../../platform/original.c"
#include <assert.h>
static int picture;
int LoadImage(RECT *rect, u_long *data)
{
    if (rect->h == 2) picture = Le16((uint8_t *)data);
    return 0;
}
static void Sequence(const char *name, int trackIndex)
{
    TimList frames = {0}, scene = {0};
    uint8_t *cmp;
    size_t size;
    int own = 0;
    for (int k = 0; k < 4; k++) {
        uint8_t *tim = SDL_calloc(1, 68);
        PutLe32(tim, 16); PutLe32(tim + 4, 8);
        PutLe32(tim + 8, 44); PutLe16(tim + 16, 16); PutLe16(tim + 18, 1);
        PutLe32(tim + 52, 16); PutLe16(tim + 62, 2); PutLe16(tim + 60, 1);
        PutLe16(tim + 64, k);
        TimAdd(&frames, tim, 68);
    }
    {
        uint8_t *first = SDL_malloc(68);
        memcpy(first, frames.tims[0], 68);
        TimAdd(&scene, first, 68);
    }
    cmp = Pack(&frames, &size);
    romSize = 28 + size;
    rom = SDL_calloc(1, romSize);
    effects = 1; rom[1] = 1;
    memcpy(rom + 3, name, strlen(name)); PutLe32(rom + 19, (uint32_t)size);
    memcpy(rom + 28, cmp, size);
    chosen = builtFor = 0; guests[0].from = FROM_64; guests[0].track = trackIndex;
    FindAnimations(&scene, &own);
    assert(own == 1 && animationCount == 1);
    picture = -1; PortOriginalAnimate(0); assert(picture == 0);
    if (!strcmp(name, "flame.cmp")) {
        PortOriginalAnimate(1); PortOriginalAnimate(2); assert(picture == 0);
        PortOriginalAnimate(3); assert(picture == 1);
        PortOriginalAnimate(6); assert(picture == 2);
    } else if (trackIndex == 2) {
        PortOriginalAnimate(1); assert(picture == 1);
        PortOriginalAnimate(2); assert(picture == 2);
    } else {
        PortOriginalAnimate(1); assert(picture == 0);
        PortOriginalAnimate(2); assert(picture == 1);
    }
    FreeAnimations(); TimFree(&frames); TimFree(&scene); SDL_free(cmp); SDL_free(rom); rom = NULL;
}
int main(void)
{
    Sequence("flame.cmp", 4);
    Sequence("steam.cmp", 2);
    Sequence("steam.cmp", 5);
    puts("Machaon flames and Sokana/Terafumos steam upload at their original cadence: OK");
    return 0;
}
