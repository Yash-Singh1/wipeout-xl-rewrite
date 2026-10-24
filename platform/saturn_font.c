/* The game's font with the Sega Saturn's disc (saturn.c): the Saturn's own HUD letters (saturn_art.c), its big ones
   for the 16-line and (scaled) 12-line glyphs and its small ones for the 8-line, each fitted to the width the game
   gives the glyph and drawn into the place the game's text code gives it (hud.c's SetCharTextures, from
   ships_data.c's widths: in the texture page at (576, 256), the 16-line letters from row 0, the 12-line from 64, the
   8-line from 100), as the PlayStation's drfonts.cmp holds them. */
#include <SDL3/SDL.h>
#include <string.h>

#include "saturn.h"

extern unsigned short glyphWidths16[38], glyphWidths12[38], glyphWidths8[38];

#define SOURCE_W 24
#define SOURCE_H 16

/* How much of a source pixel is lit (the letters are white, their edges grey). */
static float Lit(const unsigned *rgba, int x, int y)
{
    unsigned p;

    if (x < 0 || y < 0 || x >= SOURCE_W || y >= SOURCE_H) {
        return 0;
    }
    p = rgba[y * SOURCE_W + x];
    return p >> 24 ? (float)((p & 0xff) + ((p >> 8) & 0xff) + ((p >> 16) & 0xff)) / (3 * 248.0f) : 0;
}

/* Draws the 38 glyphs of a size into a page of 4-bit texels (256 x 256, the font's texture page): each in its
   place and width, the palette's 1-15 a ramp of brightness. */
void SaturnDrawFont(unsigned char *page, int pageWidth, int size)
{
    const unsigned short *widths = size == 16 ? glyphWidths16 : size == 12 ? glyphWidths12 : glyphWidths8;
    int top = size == 16 ? 0 : size == 12 ? 64 : 100, small = size == 8, sourceH = small ? 8 : 16, glyph, previous;
    unsigned rgba[SOURCE_W * SOURCE_H];

    for (glyph = 0; glyph < 38; glyph++) {
        int row = widths[glyph] / 100, width = widths[glyph] % 100, x0 = 0, left = SOURCE_W, right = -1, x, y;
        float scale = (float)size / sourceH, sx, offset;

        if (!SaturnFontGlyph(glyph, small, rgba, SOURCE_W, SOURCE_H)) {
            return;
        }
        for (previous = 0; previous < glyph; previous++) {
            if (widths[previous] / 100 == row) {
                x0 += widths[previous] % 100;
            }
        }
        for (y = 0; y < sourceH; y++) {
            for (x = 0; x < SOURCE_W; x++) {
                if (Lit(rgba, x, y) > 0) {
                    left = SDL_min(left, x);
                    right = SDL_max(right, x);
                }
            }
        }
        if (right < left) {
            continue;
        }
        /* the ink, as tall as the glyph, no wider than it (a column to spare), centred in it */
        sx = SDL_min(scale, (float)(width - 1) / (right - left + 1));
        offset = (width - (right - left + 1) * sx) / 2;
        for (y = 0; y < size; y++) {
            for (x = 0; x < width; x++) {
                float sum = 0;
                int i, k, level, px = x0 + x, py = top + row * size + y;

                for (k = 0; k < 4; k++) {
                    for (i = 0; i < 4; i++) {
                        sum += Lit(rgba, left + (int)SDL_floorf((x + (i + 0.5f) / 4 - offset) / sx),
                                   (int)SDL_floorf((y + (k + 0.5f) / 4) / scale));
                    }
                }
                level = (int)(sum / 16 * 15 + 0.5f);
                if (level > 0 && px < pageWidth && py < 256 && x + 0.5f >= offset) {
                    unsigned char *b = &page[py * (pageWidth / 2) + px / 2];

                    level = SDL_min(level, 15);
                    *b = (unsigned char)(px & 1 ? (*b & 0x0F) | level << 4 : (*b & 0xF0) | level);
                }
            }
        }
    }
}
