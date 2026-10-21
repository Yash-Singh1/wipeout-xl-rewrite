/* The button glyphs of the player's input device, in place of the console's icons.

   The game draws every glyph through DoButton, as a 16x16 quad of the console's 4-bit icons. For a
   keyboard and for Xbox and Nintendo gamepads the platform keeps its own glyphs in a page of texture
   memory beyond the console's (the glyph page of gpu_gl.c, selected by colour depth 3), and
   PortButtonGlyph rewrites the quad to show one of them:

     keyboard    the Windows release's key caps and gold direction arrows (keyglyphs.c, from its HUD2
                 art: ESC, ENTER and the arrows as it draws them, the other keys' caps made of the same
                 pieces), at the size it draws them.
     Xbox        A B X Y buttons in the colours of the pad's buttons, by position (south is A).
     Nintendo    B A Y X buttons by position (south is B).

   The gamepad buttons are drawn here at four times the page's resolution with smooth edges (distance
   fields and a stroke face of their letters), so they stay sharp at any window size.

   PlayStation pads, unknown pads and a player who has not touched anything yet keep the console's
   icons: nothing here runs for them. */
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "controls.h"
#include "keyglyphs.h"
#include "platform.h"
#include "psx/gpu.h"
#include "psx/pad.h"

/* A glyph: where its texels are in the glyph page, and the quad it is drawn into, in console
   pixels, relative to the 16x16 box of the icon it replaces. The caps are drawn at twice the
   console's density. */
typedef struct Glyph {
    int x, y, w, h;
    int dx, dy, qw, qh;
} Glyph;

enum { ICON_CIRCLE, ICON_CROSS, ICON_SQUARE, ICON_TRIANGLE, ICON_LEFT, ICON_RIGHT, ICON_COUNT };
enum { FACE_COUNT = 4 };
enum { SET_XBOX, SET_NINTENDO, SET_COUNT };
enum { ROW_COUNT = 6 }; /* the controller screen's rows: thrust, fire, view, drop weapon, left and right air brake */

static bool built;
static Glyph keyIcons[ICON_COUNT];     /* the keyboard's glyph for each console icon */
static Glyph keyRows[ROW_COUNT];       /* the key of each row of the controller screen */
static Glyph faceIcons[SET_COUNT][FACE_COUNT];
static int consoleGlyphs;

/* ---- painting ----

   The page is drawn GLYPH_ATLAS_SCALE times finer than its coordinates, in RGBA with the alpha the
   shapes' coverage (premultiplied), from distance fields: every edge is smooth at any size. A glyph's
   shapes are given in the page's coordinates, relative to the glyph's corner. */

typedef struct Colour {
    float r, g, b, a;
} Colour;

static uint32_t atlas[GLYPH_ATLAS_SIZE * GLYPH_ATLAS_SIZE];

static Colour Rgb(int r, int g, int b)
{
    Colour c = {r / 255.0f, g / 255.0f, b / 255.0f, 1.0f};
    return c;
}

static Colour Mix(Colour a, Colour b, float t)
{
    Colour c = {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t, a.a + (b.a - a.a) * t};
    return c;
}

/* The coverage of a pixel by a shape at signed distance d (in page units; negative inside). */
static float Coverage(float d)
{
    float c = 0.5f - d * GLYPH_ATLAS_SCALE;

    return c < 0 ? 0 : c > 1 ? 1 : c;
}

/* Paints colour over the page's pixel with the given coverage. */
static void Over(int ax, int ay, Colour colour, float coverage)
{
    uint32_t *p = &atlas[ay * GLYPH_ATLAS_SIZE + ax];
    float a = colour.a * coverage, r, g, b, da;

    if (a <= 0) {
        return;
    }
    r = (*p & 255) / 255.0f;
    g = ((*p >> 8) & 255) / 255.0f;
    b = ((*p >> 16) & 255) / 255.0f;
    da = (*p >> 24) / 255.0f;
    r = colour.r * a + r * (1 - a);
    g = colour.g * a + g * (1 - a);
    b = colour.b * a + b * (1 - a);
    da = a + da * (1 - a);
    *p = (uint32_t)(r * 255 + 0.5f) | (uint32_t)(g * 255 + 0.5f) << 8 | (uint32_t)(b * 255 + 0.5f) << 16 |
         (uint32_t)(da * 255 + 0.5f) << 24;
}

static float Length(float x, float y)
{
    return SDL_sqrtf(x * x + y * y);
}

static float Segment(float x, float y, float x0, float y0, float x1, float y1)
{
    float dx = x1 - x0, dy = y1 - y0, t = ((x - x0) * dx + (y - y0) * dy) / (dx * dx + dy * dy + 1e-9f);

    t = t < 0 ? 0 : t > 1 ? 1 : t;
    return Length(x - x0 - dx * t, y - y0 - dy * t);
}

/* Distance to an arc of a circle (angles in degrees, y down, from a0 increasing to a1). */
static float Arc(float x, float y, float cx, float cy, float r, float a0, float a1)
{
    float angle = SDL_atan2f(y - cy, x - cx) * 57.29578f;
    float ex, ey;

    while (angle < a0) {
        angle += 360;
    }
    if (angle <= a1) {
        return SDL_fabsf(Length(x - cx, y - cy) - r);
    }
    ex = cx + r * SDL_cosf(a0 / 57.29578f);
    ey = cy + r * SDL_sinf(a0 / 57.29578f);
    {
        float d0 = Length(x - ex, y - ey);
        ex = cx + r * SDL_cosf(a1 / 57.29578f);
        ey = cy + r * SDL_sinf(a1 / 57.29578f);
        return SDL_min(d0, Length(x - ex, y - ey));
    }
}

/* ---- the lettering: a stroke face of the gamepad buttons' letters on a 4 x 6 grid ----

   Each character is strokes: "L x0,y0 x1,y1" a line, "A cx,cy,r,a0,a1" an arc; separated by '|'. */

static const struct { char c; const char *strokes; } font[] = {
    {'A', "L0,6 2,0|L2,0 4,6|L0.7,4 3.3,4"},
    {'B', "L0,0 0,6|L0,0 2.5,0|A2.5,1.5,1.5,270,450|L0,3 2.6,3|A2.6,4.5,1.5,270,450|L0,6 2.6,6"},
    {'X', "L0,0 4,6|L4,0 0,6"},
    {'Y', "L0,0 2,3|L4,0 2,3|L2,3 2,6"}};

#define FONT_ADVANCE 5.4f /* grid units from one character to the next */

static const char *Strokes(char c)
{
    size_t i;

    for (i = 0; i < SDL_arraysize(font); i++) {
        if (font[i].c == c) {
            return font[i].strokes;
        }
    }
    return NULL;
}

/* Distance from (x, y), in grid units, to a character's strokes (their centre lines). */
static float CharDistance(const char *strokes, float x, float y)
{
    float best = 1e9f;

    while (strokes && *strokes) {
        float v[5];
        float d = 1e9f;

        if (*strokes == 'L' && SDL_sscanf(strokes + 1, "%f,%f %f,%f", &v[0], &v[1], &v[2], &v[3]) == 4) {
            d = Segment(x, y, v[0], v[1], v[2], v[3]);
        } else if (*strokes == 'A' && SDL_sscanf(strokes + 1, "%f,%f,%f,%f,%f", &v[0], &v[1], &v[2], &v[3], &v[4]) == 5) {
            d = Arc(x, y, v[0], v[1], v[2], v[3], v[4]);
        }
        best = SDL_min(best, d);
        strokes = SDL_strchr(strokes, '|');
        if (strokes) {
            strokes++;
        }
    }
    return best;
}

/* The width of a text, in page units, with letters `height` tall. */
static float TextWidth(const char *text, float height)
{
    size_t n = SDL_strlen(text);

    return n ? (n * FONT_ADVANCE - (FONT_ADVANCE - 4)) * height / 6 : 0;
}

/* Paints a text with letters `height` tall and strokes `weight` thick (both in page units) at
   (x, y), its top left, in a glyph whose page corner is (gx, gy) and size w x h. */
static void PaintText(int gx, int gy, int w, int h, float x, float y, const char *text, float height, float weight,
                      Colour colour)
{
    float unit = height / 6;
    int ax, ay;

    for (ay = gy * GLYPH_ATLAS_SCALE; ay < (gy + h) * GLYPH_ATLAS_SCALE; ay++) {
        for (ax = gx * GLYPH_ATLAS_SCALE; ax < (gx + w) * GLYPH_ATLAS_SCALE; ax++) {
            float px = (ax + 0.5f) / GLYPH_ATLAS_SCALE - gx, py = (ay + 0.5f) / GLYPH_ATLAS_SCALE - gy;
            float d = 1e9f, cx = x;
            const char *c;

            if (py < y - weight || py > y + height + weight) {
                continue;
            }
            for (c = text; *c; c++, cx += FONT_ADVANCE * unit) {
                if (px > cx - weight && px < cx + 4 * unit + weight) {
                    d = SDL_min(d, CharDistance(Strokes(*c), (px - cx) / unit, (py - y) / unit) * unit);
                }
            }
            Over(ax, ay, colour, Coverage(d - weight / 2));
        }
    }
}

/* ---- placing glyphs in the page ---- */

static int shelfX, shelfY, shelfH;

/* The next free w x h units, filled row by row; the right edge stays inside 255 because a quad's
   UV is a byte and its far edge is one past the last texel. Glyphs keep a unit apart. */
static bool Place(int w, int h, Glyph *glyph)
{
    if (shelfX + w > 255) {
        shelfX = 0;
        shelfY += shelfH + 2;
        shelfH = 0;
    }
    if (shelfY + h > 255) {
        return false;
    }
    glyph->x = shelfX;
    glyph->y = shelfY;
    glyph->w = w;
    glyph->h = h;
    shelfX += w + 2;
    if (h > shelfH) {
        shelfH = h;
    }
    return true;
}

/* ---- the keyboard ---- */

/* One of the Windows release's keyboard glyphs, texel for texel (each texel GLYPH_ATLAS_SCALE pixels
   of the page), drawn as that release draws it: half its texels in console pixels, from the top left
   of the icon's box. */
static bool CopyKeyArt(const KeyArt *art, Glyph *glyph)
{
    int x, y;

    if (!Place(art->width, art->height, glyph)) {
        return false;
    }
    for (y = 0; y < art->height * GLYPH_ATLAS_SCALE; y++) {
        for (x = 0; x < art->width * GLYPH_ATLAS_SCALE; x++) {
            atlas[(glyph->y * GLYPH_ATLAS_SCALE + y) * GLYPH_ATLAS_SIZE + glyph->x * GLYPH_ATLAS_SCALE + x] =
                art->pixels[(y / GLYPH_ATLAS_SCALE) * art->width + x / GLYPH_ATLAS_SCALE];
        }
    }
    glyph->dx = 0;
    glyph->dy = 0;
    glyph->qw = art->width / 2;
    glyph->qh = art->height / 2;
    return true;
}

/* ---- gamepad face buttons ---- */

/* A round face button: a dark, slightly domed cap with a thin rim and the letter in its colour. */
static bool MakeDisc(char letter, Colour colour, Glyph *glyph)
{
    char name[2] = {letter, 0};
    const float height = 14, weight = 2.9f;
    int x, y;

    if (!Place(32, 32, glyph)) {
        return false;
    }
    for (y = 0; y < 32 * GLYPH_ATLAS_SCALE; y++) {
        for (x = 0; x < 32 * GLYPH_ATLAS_SCALE; x++) {
            float px = (x + 0.5f) / GLYPH_ATLAS_SCALE, py = (y + 0.5f) / GLYPH_ATLAS_SCALE;
            float d = Length(px - 16, py - 16);
            int ax = glyph->x * GLYPH_ATLAS_SCALE + x, ay = glyph->y * GLYPH_ATLAS_SCALE + y;
            /* light from above: the dome is brighter at the top, the rim catches it */
            float light = SDL_clamp(0.5f - (py - 16) / 30.0f, 0.0f, 1.0f);

            Over(ax, ay, Rgb(12, 12, 14), Coverage(d - 15.6f));
            Over(ax, ay, Mix(Rgb(44, 46, 50), Rgb(112, 116, 122), light), Coverage(d - 14.6f));
            Over(ax, ay, Mix(Rgb(22, 23, 26), Rgb(58, 60, 66), light * light), Coverage(d - 13.2f));
        }
    }
    PaintText(glyph->x, glyph->y, 32, 32, 16 - TextWidth(name, height) / 2, 16 - height / 2, name, height, weight,
              colour);
    glyph->dx = glyph->dy = 0;
    glyph->qw = glyph->qh = 16;
    return true;
}

/* ---- the sets ---- */

static void Build(void)
{
    /* the glyph named by (face icon, ...): circle east, cross south, square west, triangle north */
    static const char xboxLetters[FACE_COUNT] = {'B', 'A', 'X', 'Y'};
    static const char nintendoLetters[FACE_COUNT] = {'A', 'B', 'Y', 'X'};
    const Colour xboxColours[FACE_COUNT] = {Rgb(236, 64, 52), Rgb(110, 196, 52), Rgb(52, 136, 240), Rgb(248, 196, 20)};
    static const int rowKeys[ROW_COUNT] = {KEY_ART_S, KEY_ART_SPACE, KEY_ART_F1, KEY_ART_ALT, KEY_ART_A, KEY_ART_D};
    int i;

    built = true;
    CopyKeyArt(&keyArt[KEY_ART_ESC], &keyIcons[ICON_TRIANGLE]);
    CopyKeyArt(&keyArt[KEY_ART_ENTER], &keyIcons[ICON_CROSS]);
    CopyKeyArt(&keyArt[KEY_ART_SPACE], &keyIcons[ICON_CIRCLE]);
    CopyKeyArt(&keyArt[KEY_ART_ALT], &keyIcons[ICON_SQUARE]);
    CopyKeyArt(&keyArt[KEY_ART_LEFT], &keyIcons[ICON_LEFT]);
    CopyKeyArt(&keyArt[KEY_ART_RIGHT], &keyIcons[ICON_RIGHT]);
    for (i = 0; i < ROW_COUNT; i++) {
        if (rowKeys[i] == KEY_ART_SPACE) {
            keyRows[i] = keyIcons[ICON_CIRCLE];
        } else if (rowKeys[i] == KEY_ART_ALT) {
            keyRows[i] = keyIcons[ICON_SQUARE];
        } else {
            CopyKeyArt(&keyArt[rowKeys[i]], &keyRows[i]);
        }
    }
    for (i = 0; i < FACE_COUNT; i++) {
        MakeDisc(xboxLetters[i], xboxColours[i], &faceIcons[SET_XBOX][i]);
        MakeDisc(nintendoLetters[i], Rgb(236, 236, 236), &faceIcons[SET_NINTENDO][i]);
    }
    GpuSetGlyphMemory(atlas);
    {
        /* tests (WIPEOUT_GLYPH_DUMP=file.ppm): the page over grey */
        const char *path = SDL_getenv("WIPEOUT_GLYPH_DUMP");
        FILE *file = path ? fopen(path, "wb") : NULL;

        if (file) {
            int k;

            fprintf(file, "P6\n%d %d\n255\n", GLYPH_ATLAS_SIZE, GLYPH_ATLAS_SIZE);
            for (k = 0; k < GLYPH_ATLAS_SIZE * GLYPH_ATLAS_SIZE; k++) {
                uint32_t v = atlas[k];
                int a = (int)(v >> 24), c;

                for (c = 0; c < 3; c++) {
                    fputc((int)((v >> (c * 8)) & 255) + (80 * (255 - a)) / 255, file);
                }
            }
            fclose(file);
        }
    }
}

/* The glyph of the player's device for a console icon, or NULL to keep the console's. */
static const Glyph *Find(int icon)
{
    PadDevice device = PsxPadLastDevice();

    if (icon >= PORT_ICON_ACTION) {
        icon -= PORT_ICON_ACTION;
        return icon < ROW_COUNT ? &keyRows[icon] : NULL;
    }
    if (icon >= 6 && icon < 10) {
        icon -= 6; /* the NegCon's face buttons are the same four */
    }
    if (consoleGlyphs) {
        return NULL;
    }
    switch (device) {
    case PAD_DEVICE_KEYBOARD:
        return icon < ICON_COUNT ? &keyIcons[icon] : NULL;
    case PAD_DEVICE_XBOX:
    case PAD_DEVICE_NINTENDO:
        return icon < FACE_COUNT ? &faceIcons[device == PAD_DEVICE_XBOX ? SET_XBOX : SET_NINTENDO][icon] : NULL;
    default:
        return NULL;
    }
}

int PortShowsKeys(void)
{
    return PsxPadLastDevice() == PAD_DEVICE_KEYBOARD;
}

void PortConsoleGlyphs(int forced)
{
    consoleGlyphs = forced;
}

int PortButtonGlyph(POLY_FT4 *poly, int icon, int x, int y)
{
    const Glyph *g;
    int left, top;

    if (!built && (icon >= PORT_ICON_ACTION || PsxPadLastDevice() != PAD_DEVICE_NONE)) {
        Build();
    }
    g = built ? Find(icon) : NULL;
    if (!g) {
        return 0;
    }
    left = x + g->dx;
    top = y + g->dy;
    poly->x0 = poly->x2 = (short)left;
    poly->x1 = poly->x3 = (short)(left + g->qw);
    poly->y0 = poly->y1 = (short)top;
    poly->y2 = poly->y3 = (short)(top + g->qh);
    poly->u0 = poly->u2 = (u_char)g->x;
    poly->u1 = poly->u3 = (u_char)(g->x + g->w);
    poly->v0 = poly->v1 = (u_char)g->y;
    poly->v2 = poly->v3 = (u_char)(g->y + g->h);
    poly->tpage = (u_short)GetTPage(3, 0, 0, 0); /* the glyph page */
    poly->clut = 0;
    return g->qw > 16 ? g->qw - 16 : 0;
}
