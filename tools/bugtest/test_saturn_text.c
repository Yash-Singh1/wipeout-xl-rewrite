/* Fit Saturn text into a 4:3 image without changing its alignment or atlas samples. */
#include "../../platform/psx/libgpu.c"
#include <assert.h>
#include <math.h>
static float X(const short *xy)
{
    GtePrecise p;
    uint32_t key = (uint16_t)xy[0] | (uint32_t)(uint16_t)xy[1] << 16;
    assert(GtePreciseOf(GteBoundHandle(xy, key), &p));
    assert(p.z == 0 && p.fade == 1 && p.y == xy[1]);
    return p.x;
}
int main(void)
{
    for (int anchor = 32; anchor <= 288; anchor += 128) {
        /* The source congratulations artwork spans 231 columns. On a 352-column
           Saturn display fitted to the port's picture, that is exactly 210. */
        POLY_FT4 title = {.x0=anchor-116,.x1=anchor+115,.x2=anchor-116,.x3=anchor+115,
                         .y0=24,.y1=24,.y2=36,.y3=36,.u0=7,.v0=19,.u1=238,.v3=31};
        PortSaturnFontQuad(&title, anchor);
        assert(fabsf(X(&title.x1) - X(&title.x0) - 210) < 0.0001f);
        assert(X(&title.x0) == X(&title.x2) && X(&title.x1) == X(&title.x3));
        assert(title.u0==7 && title.v0==19 && title.u1==238 && title.v3==31);
        POLY_FT4 number = {.x0=anchor-37,.x1=anchor,.x2=anchor-37,.x3=anchor,
                          .y0=40,.y1=40,.y2=48,.y3=48};
        PortSaturnFontQuad(&number, anchor);
        assert(X(&number.x1) == anchor);
        assert(fabsf(X(&number.x0) - (anchor-37*320.0f/352)) < 0.0001f);
    }
    puts("Saturn text keeps its 352-column proportions, alignment, fractional positions and UVs: OK");
}
