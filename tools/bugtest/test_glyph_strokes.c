/* Compare numeric strokes with the former text representation across the
   rasterizer's coordinate range, and verify atlas preparation is idempotent. */
#include <assert.h>
#include <inttypes.h>
#include "../../platform/glyphs.c"

static int uploads;
PadDevice PsxPadLastDevice(void) { return PAD_DEVICE_KEYBOARD; }
u_short GetTPage(int tp, int abr, int x, int y) { (void)tp; (void)abr; (void)x; (void)y; return 0; }
void GpuSetGlyphMemory(const uint32_t *rgba) { assert(rgba == atlas); uploads++; }

static float ReferenceDistance(const char *strokes, float x, float y)
{
    float best = 1e9f;
    while (strokes && *strokes) {
        float v[5], distance = 1e9f;
        if (*strokes == 'L' && SDL_sscanf(strokes + 1, "%f,%f %f,%f", &v[0], &v[1], &v[2], &v[3]) == 4)
            distance = Segment(x, y, v[0], v[1], v[2], v[3]);
        else if (*strokes == 'A' && SDL_sscanf(strokes + 1, "%f,%f,%f,%f,%f", &v[0], &v[1], &v[2], &v[3], &v[4]) == 5)
            distance = Arc(x, y, v[0], v[1], v[2], v[3], v[4]);
        best = SDL_min(best, distance);
        strokes = SDL_strchr(strokes, '|');
        if (strokes) strokes++;
    }
    return best;
}

int main(void)
{
    static const struct { char letter; const char *strokes; } reference[] = {
        {'A', "L0,6 2,0|L2,0 4,6|L0.7,4 3.3,4"},
        {'B', "L0,0 0,6|L0,0 2.5,0|A2.5,1.5,1.5,270,450|L0,3 2.6,3|A2.6,4.5,1.5,270,450|L0,6 2.6,6"},
        {'X', "L0,0 4,6|L4,0 0,6"},
        {'Y', "L0,0 2,3|L4,0 2,3|L2,3 2,6"}
    };
    for (size_t c = 0; c < SDL_arraysize(reference); c++) {
        const Letter *letter = FindLetter(reference[c].letter);
        assert(letter);
        for (int y = -32; y <= 128; y++) for (int x = -32; x <= 128; x++)
            assert(CharDistance(letter, x / 16.0f, y / 16.0f) ==
                   ReferenceDistance(reference[c].strokes, x / 16.0f, y / 16.0f));
    }
    assert(CharDistance(FindLetter('?'), 0, 0) == 1e9f);
    PortPrepareButtonGlyphs();
    assert(uploads == 1);
    PortPrepareButtonGlyphs();
    POLY_FT4 poly = {0};
    PortButtonGlyph(&poly, 1, 30, 198);
    assert(uploads == 1);
    uint64_t hash = UINT64_C(14695981039346656037);
    for (size_t i = 0; i < SDL_arraysize(atlas); i++) for (unsigned byte = 0; byte < 4; byte++)
        hash = (hash ^ ((atlas[i] >> (byte * 8)) & 255)) * UINT64_C(1099511628211);
    printf("Glyph strokes: 103684 exact reference distances; atlas %016" PRIx64 "; single preparation: OK\n", hash);
    return 0;
}
