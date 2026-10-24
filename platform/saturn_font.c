/* The game's font with the Sega Saturn's disc (saturn.c): the Saturn has no alphabet the PlayStation game could
   draw its text from (its own text is whole words, drawn ready), so the font is drawn here, a face of rounded
   strokes in the manner of the game's, into the places the game's text code gives each letter (hud.c's
   SetCharTextures, from ships_data.c's widths: in the texture page at (576, 256), the 16-line letters from row 0,
   the 12-line from 64, the 8-line from 100), as the PlayStation's drfonts.cmp holds them. */
#include <SDL3/SDL.h>
#include <string.h>

#include "saturn.h"

extern unsigned short glyphWidths16[38], glyphWidths12[38], glyphWidths8[38];

/* A..Z, 0..9, ':' and '.' (the game's glyphs 0-37), on a grid 4 wide and 6 tall, y down: "L x0,y0 x1,y1" a line,
   "A cx,cy,r,a0,a1" an arc (degrees, from a0 increasing to a1), separated by '|'. */
#define TOP_LEFT "A1,1,1,180,270"
#define TOP_RIGHT "A3,1,1,270,360"
#define BOTTOM_RIGHT "A3,5,1,0,90"
#define BOTTOM_LEFT "A1,5,1,90,180"
#define RING TOP_LEFT "|L1,0 3,0|" TOP_RIGHT "|L4,1 4,5|" BOTTOM_RIGHT "|L3,6 1,6|" BOTTOM_LEFT "|L0,5 0,1"
static const char *const strokes[38] = {
    "L0,6 0,1|" TOP_LEFT "|L1,0 3,0|" TOP_RIGHT "|L4,1 4,6|L0,3.5 4,3.5",                              /* A */
    "L0,0 0,6|L0,0 2.5,0|A2.5,1.5,1.5,270,450|L0,3 2.5,3|A2.5,4.5,1.5,270,450|L0,6 2.5,6",              /* B */
    TOP_LEFT "|L1,0 4,0|L0,1 0,5|" BOTTOM_LEFT "|L1,6 4,6",                                             /* C */
    "L0,0 0,6|L0,0 3,0|" TOP_RIGHT "|L4,1 4,5|" BOTTOM_RIGHT "|L3,6 0,6",                               /* D */
    "L0,0 0,6|L0,0 4,0|L0,3 3,3|L0,6 4,6",                                                              /* E */
    "L0,0 0,6|L0,0 4,0|L0,3 3,3",                                                                       /* F */
    TOP_LEFT "|L1,0 4,0|L0,1 0,5|" BOTTOM_LEFT "|L1,6 3,6|" BOTTOM_RIGHT "|L4,5 4,3|L4,3 2,3",          /* G */
    "L0,0 0,6|L4,0 4,6|L0,3 4,3",                                                                       /* H */
    "L2,0 2,6",                                                                                         /* I */
    "L4,0 4,5|" BOTTOM_RIGHT "|L3,6 1,6|" BOTTOM_LEFT,                                                  /* J */
    "L0,0 0,6|L0,3.2 4,0|L0,3.2 4,6",                                                                   /* K */
    "L0,0 0,6|L0,6 4,6",                                                                                /* L */
    "L0,6 0,1|" TOP_LEFT "|L1,0 3,0|" TOP_RIGHT "|L4,1 4,6|L2,0 2,4",                                   /* M */
    "L0,6 0,1|" TOP_LEFT "|L1,0 3,0|" TOP_RIGHT "|L4,1 4,6",                                            /* N */
    RING,                                                                                               /* O */
    "L0,0 0,6|L0,0 3,0|" TOP_RIGHT "|L4,1 4,2.5|A3,2.5,1,0,90|L3,3.5 0,3.5",                            /* P */
    RING "|L2.6,4.2 4.4,6.2",                                                                           /* Q */
    "L0,0 0,6|L0,0 3,0|" TOP_RIGHT "|L4,1 4,2.5|A3,2.5,1,0,90|L3,3.5 0,3.5|L2,3.5 4,6",                 /* R */
    "L4,0 1,0|" TOP_LEFT "|L0,1 0,2|A1,2,1,90,180|L1,3 3,3|A3,4,1,270,360|L4,4 4,5|" BOTTOM_RIGHT
    "|L3,6 0,6",                                                                                        /* S */
    "L0,0 4,0|L2,0 2,6",                                                                                /* T */
    "L0,0 0,5|" BOTTOM_LEFT "|L1,6 3,6|" BOTTOM_RIGHT "|L4,5 4,0",                                      /* U */
    "L0,0 2,6|L2,6 4,0",                                                                                /* V */
    "L0,0 0,5|" BOTTOM_LEFT "|L1,6 3,6|" BOTTOM_RIGHT "|L4,5 4,0|L2,2 2,6",                             /* W */
    "L0,0 4,6|L4,0 0,6",                                                                                /* X */
    "L0,0 2,3|L4,0 2,3|L2,3 2,6",                                                                       /* Y */
    "L0,0 4,0|L4,0 0,6|L0,6 4,6",                                                                       /* Z */
    RING,                                                                                               /* 0 */
    "L1,1 2,0|L2,0 2,6",                                                                                /* 1 */
    "L0,0 3,0|" TOP_RIGHT "|L4,1 4,2|A3,2,1,0,90|L3,3 1,3|A1,4,1,180,270|L0,4 0,6|L0,6 4,6",            /* 2 */
    "L0,0 3,0|" TOP_RIGHT "|L4,1 4,5|" BOTTOM_RIGHT "|L3,6 0,6|L1,3 4,3",                               /* 3 */
    "L0,0 0,3|L0,3 4,3|L3,1.5 3,6",                                                                     /* 4 */
    "L4,0 0,0|L0,0 0,3|L0,3 3,3|A3,4,1,270,360|L4,4 4,5|" BOTTOM_RIGHT "|L3,6 0,6",                     /* 5 */
    "L3,0 1,0|" TOP_LEFT "|L0,1 0,5|" BOTTOM_LEFT "|L1,6 3,6|" BOTTOM_RIGHT
    "|L4,5 4,4|A3,4,1,270,360|L3,3 0,3",                                                                /* 6 */
    "L0,0 4,0|L4,0 4,6",                                                                                /* 7 */
    RING "|L0,3 4,3",                                                                                   /* 8 */
    "L1,6 3,6|" BOTTOM_RIGHT "|L4,5 4,1|" TOP_RIGHT "|L3,0 1,0|" TOP_LEFT "|L0,1 0,2|A1,2,1,90,180|L1,3 4,3", /* 9 */
    "L2,1.4 2,1.6|L2,4.4 2,4.6",                                                                        /* : */
    "L2,5.6 2,5.8",                                                                                     /* . */
};

static float Segment(float x, float y, float x0, float y0, float x1, float y1)
{
    float dx = x1 - x0, dy = y1 - y0, t = ((x - x0) * dx + (y - y0) * dy) / (dx * dx + dy * dy + 1e-9f);

    t = t < 0 ? 0 : t > 1 ? 1 : t;
    return SDL_sqrtf((x - x0 - dx * t) * (x - x0 - dx * t) + (y - y0 - dy * t) * (y - y0 - dy * t));
}

static float Arc(float x, float y, float cx, float cy, float r, float a0, float a1)
{
    float angle = SDL_atan2f(y - cy, x - cx) * 57.29578f, ex, ey, d0;

    while (angle < a0) {
        angle += 360;
    }
    if (angle <= a1) {
        return SDL_fabsf(SDL_sqrtf((x - cx) * (x - cx) + (y - cy) * (y - cy)) - r);
    }
    ex = cx + r * SDL_cosf(a0 / 57.29578f);
    ey = cy + r * SDL_sinf(a0 / 57.29578f);
    d0 = SDL_sqrtf((x - ex) * (x - ex) + (y - ey) * (y - ey));
    ex = cx + r * SDL_cosf(a1 / 57.29578f);
    ey = cy + r * SDL_sinf(a1 / 57.29578f);
    return SDL_min(d0, SDL_sqrtf((x - ex) * (x - ex) + (y - ey) * (y - ey)));
}

/* Distance (in pixels) from a pixel's centre to a glyph's strokes, the grid stretched over w x h pixels. */
static float Distance(const char *s, float px, float py, float sx, float sy, float ox, float oy)
{
    float best = 1e9f;

    while (s && *s) {
        float v[5], d = 1e9f;

        if (*s == 'L' && SDL_sscanf(s + 1, "%f,%f %f,%f", &v[0], &v[1], &v[2], &v[3]) == 4) {
            d = Segment(px, py, ox + v[0] * sx, oy + v[1] * sy, ox + v[2] * sx, oy + v[3] * sy);
        } else if (*s == 'A' && SDL_sscanf(s + 1, "%f,%f,%f,%f,%f", &v[0], &v[1], &v[2], &v[3], &v[4]) == 5) {
            /* (an arc of the stretched grid: an ellipse, measured on the circle scaled back) */
            float k = sy / sx;
            d = Arc(px, (py - oy) / k + oy, ox + v[0] * sx, oy + v[1] * sx, v[2] * sx, v[3], v[4]);
        }
        best = SDL_min(best, d);
        s = SDL_strchr(s, '|');
        if (s) {
            s++;
        }
    }
    return best;
}

/* Draws the 38 glyphs of a size into a page of 4-bit texels (256 x 256, the font's texture page): each in its
   place and width, the palette's 1-15 a ramp of coverage. */
void SaturnDrawFont(uint8_t *page, int pageWidth, int size)
{
    const unsigned short *widths = size == 16 ? glyphWidths16 : size == 12 ? glyphWidths12 : glyphWidths8;
    int top = size == 16 ? 0 : size == 12 ? 64 : 100, glyph, previous;
    float weight = size == 16 ? 3.4f : size == 12 ? 2.6f : 1.7f;

    for (glyph = 0; glyph < 38; glyph++) {
        int row = widths[glyph] / 100, width = widths[glyph] % 100, x0 = 0, x, y;
        float pad = weight * 0.5f + 0.5f, sx, sy;

        for (previous = 0; previous < glyph; previous++) {
            if (widths[previous] / 100 == row) {
                x0 += widths[previous] % 100;
            }
        }
        sx = (width - 1 - pad * 2) / 4.0f;
        sy = (size - 1 - pad * 2) / 6.0f;
        for (y = 0; y < size; y++) {
            for (x = 0; x < width; x++) {
                float d = Distance(strokes[glyph], x + 0.5f, y + 0.5f, sx, sy, pad, pad);
                float cover = SDL_clamp(weight * 0.5f + 0.5f - d, 0.0f, 1.0f);
                int level = (int)(cover * 15 + 0.5f), px = x0 + x, py = top + row * size + y;

                if (level && px < pageWidth && py < 256) {
                    uint8_t *b = &page[py * (pageWidth / 2) + px / 2];

                    *b = (uint8_t)(px & 1 ? (*b & 0x0F) | level << 4 : (*b & 0xF0) | level);
                }
            }
        }
    }
}
