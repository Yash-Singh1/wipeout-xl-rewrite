/* Port: frame interpolation of the game's 2D (portinterp.h). */
#include "functions.h"
#include "portinterp.h"
#include "../../platform/psx/gte.h"

uint32_t PortProjection(const void *sxy)
{
    const short *xy = sxy;

    return GteBoundHandle(sxy, (u_short)xy[0] | (uint32_t)(u_short)xy[1] << 16);
}

/* Recorded beside the projection at its depth (so it goes with the projection and the view between frames), as what
   moves with something does (GTE_KEY_MOVING) when the projection does. */
void PortBindAbout(void *corner, uint32_t centre, int x, int y, uint64_t which)
{
    const short *xy = corner;
    GtePrecise   at;
    double       scale;
    int          dx = xy[0] - x, dy = xy[1] - y;
    uint32_t     handle;

    if (!centre || !GtePreciseOf(centre, &at) || !at.key || at.view[2] <= 0.0f) {
        return;
    }
    scale = at.view[2] / (double)(u_short)GteReadControl(26);
    GteKeyNext(0, GteKeyMix(at.key, which + 1) | (at.key & GTE_KEY_MOVING));
    handle = GteRecordProjection(*(const uint32_t *)corner, at.x + (float)dx, at.y + (float)dy, 0.0f, 1.0f);
    GteRecordView(handle, at.view[0] + dx * scale, at.view[1] + dy * scale, at.view[2]);
    GteBindPrecise(corner, handle);
}

/* At the projection distance, where view space is the screen's (offset by its centre). */
void PortBindScreen(void *corner, uint64_t key)
{
    const short *xy = corner;
    double       h = (double)(u_short)GteReadControl(26);
    uint32_t     handle;

    GteKeyNext(0, key);
    handle = GteRecordProjection(*(const uint32_t *)corner, xy[0], xy[1], 0.0f, 1.0f);
    GteRecordView(handle, xy[0] - GteReadControl(24) / 65536.0, xy[1] - GteReadControl(25) / 65536.0, h);
    GteBindPrecise(corner, handle);
}
