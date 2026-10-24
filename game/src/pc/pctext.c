/* The PC version's text (hud.c): its fonts are in its HUD2 page (texture code 32), 16 and 8 lines high and a 12-line
   one for the menus, laid out in 640x480 (a glyph's width and height its pixels there, its place in the console's
   320x240 doubled); colour 0 white, 1 yellow, 2 green, as Direct3D's vertex colour over the white glyphs. The PS1's
   text calls (hud.c) come here when the PC version's data is in use. */
#include "pc.h"

typedef struct PcFontGlyph {
    u_short vramX;
    u_short vramY;
    u_short width;
    u_short advance;
} PcFontGlyph;

/* The glyph of each character from 32 to 95 (50: none, a space); from 48 on, the digits' (hud.c's glyphIndex). */
static const u_char glyphIndex[96] = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  30, 30, 158, 162, 30, 30, 158, 194, 30, 30, 160, 226, 32, 16,
    194, 226, 32, 16, 0, 0,  0,  0,  50, 47, 50, 48, 50, 46, 50, 42, 50, 50, 50, 45, 38, 44, 37, 41,
    26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 50, 50, 50, 50, 38, 42, 0,  1,  2,  3,  4,  5,  6,
    7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 39, 49, 40, 50, 50,
};
#define DIGIT_GLYPH(d) glyphIndex[48 + (d)]

/* Each font's rows in its page: the number of rows, then per row its y, its glyph count and the x of each glyph and
   of the end of the last. */
static const short fontRows16[48] = {
    3, 0, 26, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23,
    24, 25, 26, 126, 7, 0, 48, 68, 104, 138, 174, 210, 244, 159, 5, 0, 36, 80, 130, 144, 158,
};
static const short fontRows8[52] = {
    4, 0, 12, 0, 26, 52, 72, 98, 124, 142, 168, 188, 196, 214, 234, 254, 18, 10, 0, 32, 52, 78,
    104, 132, 158, 178, 196, 214, 232, 36, 11, 0, 32, 60, 78, 98, 124, 136, 158, 178, 198, 220,
    240, 54, 6, 0, 20, 44, 72, 80, 88, 108,
};
static const short fontRows12[62] = {
    4, 192, 13, 0, 12, 24, 36, 48, 60, 72, 84, 96, 108, 120, 132, 144, 156, 204, 13, 0, 12, 24, 36,
    48, 60, 72, 84, 96, 108, 120, 132, 144, 156, 216, 13, 0, 12, 24, 36, 48, 60, 72, 84, 96, 108,
    120, 132, 144, 156, 228, 9, 0, 12, 24, 36, 48, 60, 72, 84, 96, 108, 0,
};

static PcFontGlyph fontGlyphs16[48], fontGlyphs8[48], fontGlyphs12[52];
static POLY_FT4 textQuad;
float pcTextDepth = 0.1f;

void PcInitText(void)
{
    const short *rows[3] = {fontRows16, fontRows8, fontRows12};
    PcFontGlyph *fonts[3] = {fontGlyphs16, fontGlyphs8, fontGlyphs12};
    int f;

    for (f = 0; f < 3; f++) {
        const short *table = rows[f];
        PcFontGlyph *glyph = fonts[f];
        int at = 1, row;

        for (row = 0; row < table[0]; row++) {
            int y = table[at], count = table[at + 1], i;

            at += 2;
            for (i = 0; i < count; i++, at++) {
                int width = table[at + 1] - table[at];

                glyph->vramX = (u_short)table[at];
                glyph->vramY = (u_short)y;
                glyph->width = (u_short)(f == 2 ? width - 2 : width - 1);
                glyph->advance = (u_short)width;
                glyph++;
            }
            at++;
        }
    }
}

void PcSetTextQuad(POLY_FT4 *quad, short x, short y, u_char u, u_char v, u_char width, u_char height, short tpage,
                   int colour)
{
    SetPolyFT4(quad);
    if (colour == 0) {
        quad->r0 = 255, quad->g0 = 255, quad->b0 = 255;
    } else if (colour == 1) {
        quad->r0 = 240, quad->g0 = 192, quad->b0 = 48;
    } else if (colour == 2) {
        quad->r0 = 0, quad->g0 = 192, quad->b0 = 0;
    }
    quad->tpage = tpage;
    quad->u0 = u, quad->v0 = v;
    quad->u1 = u + width, quad->v1 = v;
    quad->u2 = u, quad->v2 = v + height;
    quad->u3 = u + width, quad->v3 = v + height;
    x *= 2;
    y *= 2;
    quad->x0 = x, quad->y0 = y;
    quad->x1 = x + width, quad->y1 = y;
    quad->x2 = x, quad->y2 = y + height;
    quad->x3 = x + width, quad->y3 = y + height;
}

static const PcFontGlyph *Font(int size)
{
    return size == 8 ? fontGlyphs8 : fontGlyphs16;
}

/* One glyph at x (the console's columns), the next x. */
static int Glyph(const PcFontGlyph *glyph, int x, int y, int size, int colour, float depth)
{
    PcSetTextQuad(&textQuad, (short)x, (short)y, (u_char)glyph->vramX, (u_char)glyph->vramY, (u_char)glyph->width,
                  (u_char)(size * 2), 32, colour);
    DrawScaledTexturedQuad(depth, &textQuad, 0);
    return x + (glyph->advance >> 1);
}

/* The width of a text in the console's columns. */
static int Width(const char *text, const PcFontGlyph *font)
{
    int width = 0;
    u_char letter;

    while ((letter = (u_char)*text++) != 0) {
        letter = letter >= 32 && letter < 96 ? glyphIndex[letter] : 50;
        width += letter == 50 ? 8 : font[letter].advance >> 1;
    }
    return width;
}

static void Text(const char *text, int x, int y, int size, int colour)
{
    const PcFontGlyph *font = Font(size);
    u_char letter;

    while ((letter = (u_char)*text++) != 0) {
        letter = letter >= 32 && letter < 96 ? glyphIndex[letter] : 50;
        x = letter == 50 ? x + 8 : Glyph(&font[letter], x, y, size, colour, pcTextDepth);
    }
}

void PcAddText(const char *text, int x, int y, int size, int colour)
{
    Text(text, x, y, size, colour);
}

void PcCentreText(const char *text, int x, int y, int size, int colour)
{
    (void)x;
    Text(text, 160 - (Width(text, Font(size)) >> 1), y, size, colour);
}

void PcCentreText2(const char *text, int x, int y, int size, int colour)
{
    Text(text, x - (Width(text, Font(size)) >> 1), y, size, colour);
}

void PcAddNumber(int number, int x, int y, int size, int colour)
{
    const PcFontGlyph *font = Font(size);
    int digits[12], length = 0;

    for (;;) {
        digits[length] = number % 10;
        number /= 10;
        if (number == 0 || length >= 5) {
            break;
        }
        length++;
    }
    for (; length >= 0; length--) {
        x = Glyph(&font[DIGIT_GLYPH(digits[length])], x, y, size, colour, 0.1f);
    }
}

void PcAddJustifyNumber(int number, int x, int y, int size, int colour)
{
    const PcFontGlyph *font = Font(size);
    int digits[12], length = 0, i;

    do {
        digits[length] = number % 10;
        number /= 10;
        if (number == 0) {
            break;
        }
        length++;
    } while (length < 5);
    for (i = length; i >= 0; i--) {
        x -= font[DIGIT_GLYPH(digits[i])].advance >> 1;
    }
    for (i = length; i >= 0; i--) {
        x = Glyph(&font[DIGIT_GLYPH(digits[i])], x, y, size, colour, 0.1f);
    }
}

void PcAddTime(int time, int x, int y, int size, int colour, u_char flags)
{
    const PcFontGlyph *font = Font(size);
    int digits[2], length = 0, minutes, seconds, tenths, i;

    tenths = (time / 6) % 10;
    time /= 60;
    seconds = time % 60;
    minutes = time / 60;
    digits[0] = minutes % 10;
    if (!(flags & 1)) {
        digits[1] = minutes > 9 ? minutes / 10 % 10 : 0;
        length = 1;
    }
    if (!(flags & 2)) {
        for (i = length; i >= 0; i--) {
            x = Glyph(&font[DIGIT_GLYPH(digits[i])], x, y, size, colour, 0.1f);
        }
        x = Glyph(&font[36], x, y, size, colour, 0.1f); /* : */
    }
    digits[0] = seconds % 10;
    digits[1] = seconds > 9 ? seconds / 10 % 10 : 0;
    for (i = 1; i >= 0; i--) {
        x = Glyph(&font[DIGIT_GLYPH(digits[i])], x, y, size, colour, 0.1f);
    }
    x = Glyph(&font[37], x, y, size, colour, 0.1f); /* . */
    if (size != 8) {
        y += 8;
    }
    Glyph(&fontGlyphs8[DIGIT_GLYPH(tenths)], x, y, 8, colour, 0.1f);
}

void PcPrint(const char *text, int x, int y, int colour, int extendedSpacing)
{
    int at = x * 2;
    u_char letter;

    while ((letter = (u_char)*text++) != 0) {
        const PcFontGlyph *glyph;

        letter = letter >= 32 && letter < 96 ? glyphIndex[letter] : 50;
        if (letter == 50) {
            at += extendedSpacing ? 12 : 8;
            continue;
        }
        glyph = &fontGlyphs12[letter];
        if (!extendedSpacing && (letter == 8 || letter == 37)) {
            /* I and . drawn narrower */
            PcSetTextQuad(&textQuad, (short)(at >> 1), (short)y, (u_char)(glyph->vramX + 4), (u_char)glyph->vramY,
                          (u_char)(glyph->width - 6), 10, 32, colour);
            DrawScaledTexturedQuad(pcTextDepth, &textQuad, 0);
            at += glyph->width - 6;
        } else {
            PcSetTextQuad(&textQuad, (short)(at >> 1), (short)y, (u_char)glyph->vramX, (u_char)glyph->vramY,
                          (u_char)glyph->width, 10, 32, colour);
            DrawScaledTexturedQuad(pcTextDepth, &textQuad, 0);
            at += glyph->advance;
        }
    }
}

void PcPrintCX(const char *text, int x, int y, int colour)
{
    const char *scan;
    int width = 0;
    u_char letter;

    for (scan = text; (letter = (u_char)*scan++) != 0;) {
        letter = letter >= 32 && letter < 96 ? glyphIndex[letter] : 50;
        width += letter == 50 ? 4 : letter == 8 || letter == 37 ? 3 : fontGlyphs12[letter].advance >> 1;
    }
    x -= width >> 1;
    while ((letter = (u_char)*text++) != 0) {
        const PcFontGlyph *glyph;

        letter = letter >= 32 && letter < 96 ? glyphIndex[letter] : 50;
        if (letter == 50) {
            x += 4;
            continue;
        }
        glyph = &fontGlyphs12[letter];
        if (letter == 8 || letter == 37) {
            /* I and . drawn narrower */
            PcSetTextQuad(&textQuad, (short)x, (short)y, (u_char)(glyph->vramX + 2), (u_char)glyph->vramY,
                          (u_char)(glyph->width - 3), 10, 32, colour);
            DrawScaledTexturedQuad(pcTextDepth, &textQuad, 0);
            x += (glyph->width - 3) >> 1;
        } else {
            PcSetTextQuad(&textQuad, (short)x, (short)y, (u_char)glyph->vramX, (u_char)glyph->vramY,
                          (u_char)glyph->width, 10, 32, colour);
            DrawScaledTexturedQuad(pcTextDepth, &textQuad, 0);
            x += glyph->advance >> 1;
        }
    }
}

int PcTextWidth(const char *text, int size)
{
    return Width(text, Font(size));
}
