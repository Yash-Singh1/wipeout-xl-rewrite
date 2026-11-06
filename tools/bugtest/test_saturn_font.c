/* Glyph ink starts at its bearing; the advance's trailing space must not become a left offset. */
#include "../../platform/saturn_font.c"
#include <assert.h>

int SaturnFontGlyph(int glyph, int small, unsigned *rgba, int w, int h)
{
    memset(rgba, 0, (size_t)w * h * sizeof(*rgba));
    /* Deliberately different source bearings and widths, including narrow digits. */
    int left = glyph % 3, width = glyph % 5 + 2;
    for (int y = 0; y < (small ? 8 : 16); y++)
        for (int x = left; x < left + width; x++) rgba[y * w + x] = 0xfff8f8f8;
    return 1;
}

int main(void)
{
    uint8_t page[128 * 256];
    const int sizes[] = {8, 12, 16};
    for (int n = 0; n < 3; n++) {
        int size = sizes[n], top = size == 16 ? 0 : size == 12 ? 64 : 100;
        const unsigned short *widths = size == 16 ? glyphWidths16 : size == 12 ? glyphWidths12 : glyphWidths8;
        memset(page, 0, sizeof(page));
        SaturnDrawFont(page, 256, size);
        for (int glyph = 0; glyph < 38; glyph++) {
            int x = 0, row = widths[glyph] / 100;
            for (int previous = 0; previous < glyph; previous++)
                if (widths[previous] / 100 == row) x += widths[previous] % 100;
            uint8_t texel = page[(top + row * size) * 128 + x / 2];
            assert((x & 1 ? texel >> 4 : texel & 15) != 0);
            x += widths[glyph] % 100 - 1;
            texel = page[(top + row * size) * 128 + x / 2];
            assert((x & 1 ? texel >> 4 : texel & 15) == 0);
        }
    }
    puts("Saturn fonts keep their bearing and trailing spacing at all three sizes: OK");
}
