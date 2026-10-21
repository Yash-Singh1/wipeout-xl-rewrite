/* The button glyphs of the player's input device, in place of the console's icons.

   The game draws every glyph through DoButton, as a 16x16 quad of the console's 4-bit icons. For a
   keyboard and for Xbox and Nintendo gamepads the platform keeps its own glyphs in a page of texture
   memory beyond the console's (the glyph page of gpu_gl.c, selected by colour depth 3), and
   PortButtonGlyph rewrites the quad to show one of them:

     keyboard    key caps. The Windows release's art (PICS/HI-RES/HUD2.TGA of the PC data: ESC, ENTER and
                 the arrows) where it exists, otherwise, and for the keys it has no art for, caps drawn
                 here in the same style with the name of the key on them.
     Xbox        A B X Y discs in the colours of the pad's buttons, by position (south is A).
     Nintendo    B A Y X discs by position (south is B).

   PlayStation pads, unknown pads and a player who has not touched anything yet keep the console's
   icons: nothing here runs for them. */
#include <SDL3/SDL.h>
#include <stdlib.h>
#include <string.h>

#include "controls.h"
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

static uint16_t page[GLYPH_PAGE_SIZE * GLYPH_PAGE_SIZE];
static bool built;
static Glyph keyIcons[ICON_COUNT];     /* the keyboard's glyph for each console icon */
static Glyph keyRows[ROW_COUNT];       /* the key of each row of the controller screen */
static Glyph faceIcons[SET_COUNT][FACE_COUNT];
static int consoleGlyphs;

/* ---- pixels ---- */

#define RGB(r, g, b) ((uint16_t)(((r) >> 3) | (((g) >> 3) << 5) | (((b) >> 3) << 10)))

static void Plot(int x, int y, uint16_t colour)
{
    if (x >= 0 && y >= 0 && x < GLYPH_PAGE_SIZE && y < GLYPH_PAGE_SIZE) {
        page[y * GLYPH_PAGE_SIZE + x] = colour ? colour : 1; /* 0 is transparent */
    }
}

/* ---- the lettering of the caps: a 5x7 face of capitals and digits, one row per string ---- */

static const struct { char c; const char *rows; } font[] = {
    {'A', "01110/10001/10001/11111/10001/10001/10001"}, {'B', "11110/10001/10001/11110/10001/10001/11110"},
    {'C', "01110/10001/10000/10000/10000/10001/01110"}, {'D', "11110/10001/10001/10001/10001/10001/11110"},
    {'E', "11111/10000/10000/11110/10000/10000/11111"}, {'F', "11111/10000/10000/11110/10000/10000/10000"},
    {'G', "01110/10001/10000/10111/10001/10001/01111"}, {'H', "10001/10001/10001/11111/10001/10001/10001"},
    {'I', "01110/00100/00100/00100/00100/00100/01110"}, {'J', "00111/00010/00010/00010/00010/10010/01100"},
    {'K', "10001/10010/10100/11000/10100/10010/10001"}, {'L', "10000/10000/10000/10000/10000/10000/11111"},
    {'M', "10001/11011/10101/10101/10001/10001/10001"}, {'N', "10001/11001/10101/10011/10001/10001/10001"},
    {'O', "01110/10001/10001/10001/10001/10001/01110"}, {'P', "11110/10001/10001/11110/10000/10000/10000"},
    {'Q', "01110/10001/10001/10001/10101/10010/01101"}, {'R', "11110/10001/10001/11110/10100/10010/10001"},
    {'S', "01111/10000/10000/01110/00001/00001/11110"}, {'T', "11111/00100/00100/00100/00100/00100/00100"},
    {'U', "10001/10001/10001/10001/10001/10001/01110"}, {'V', "10001/10001/10001/10001/10001/01010/00100"},
    {'W', "10001/10001/10001/10101/10101/11011/10001"}, {'X', "10001/10001/01010/00100/01010/10001/10001"},
    {'Y', "10001/10001/01010/00100/00100/00100/00100"}, {'Z', "11111/00001/00010/00100/01000/10000/11111"},
    {'0', "01110/10001/10011/10101/11001/10001/01110"}, {'1', "00100/01100/00100/00100/00100/00100/01110"},
    {'2', "01110/10001/00001/00010/00100/01000/11111"}, {'3', "11110/00001/00001/01110/00001/00001/11110"},
    {'4', "00010/00110/01010/10010/11111/00010/00010"}, {'5', "11111/10000/11110/00001/00001/10001/01110"},
    {'6', "00110/01000/10000/11110/10001/10001/01110"}, {'7', "11111/00001/00010/00100/01000/01000/01000"},
    {'8', "01110/10001/10001/01110/10001/10001/01110"}, {'9', "01110/10001/10001/01111/00001/00010/01100"}};

#define LETTER_W 5
#define LETTER_H 7

static const char *LetterRows(char c)
{
    size_t i;

    for (i = 0; i < sizeof(font) / sizeof(font[0]); i++) {
        if (font[i].c == c) {
            return font[i].rows;
        }
    }
    return NULL;
}

/* The width of a text in texels at a scale, one scaled column between letters. */
static int TextWidth(const char *text, int scale)
{
    int n = (int)strlen(text);

    return n ? n * (LETTER_W + 1) * scale - scale : 0;
}

static void DrawLetter(int x, int y, char c, int scale, uint16_t colour)
{
    const char *rows = LetterRows(c);
    int row, col;

    for (row = 0; rows && row < LETTER_H; row++) {
        for (col = 0; col < LETTER_W; col++) {
            if (rows[row * (LETTER_W + 1) + col] == '1') {
                int i, j;

                for (j = 0; j < scale; j++) {
                    for (i = 0; i < scale; i++) {
                        Plot(x + col * scale + i, y + row * scale + j, colour);
                    }
                }
            }
        }
    }
}

static void DrawText(int x, int y, const char *text, int scale, uint16_t colour)
{
    for (; *text; text++) {
        DrawLetter(x, y, *text, scale, colour);
        x += (LETTER_W + 1) * scale;
    }
}

/* ---- placing glyphs in the page ---- */

static int shelfX, shelfY, shelfH;

/* The next free w x h texels, filled row by row; the right edge stays inside 255 because a quad's
   UV is a byte and its far edge is one past the last texel. */
static bool Place(int w, int h, Glyph *glyph)
{
    if (shelfX + w > 255) {
        shelfX = 0;
        shelfY += shelfH + 1;
        shelfH = 0;
    }
    if (shelfY + h > 255) {
        return false;
    }
    glyph->x = shelfX;
    glyph->y = shelfY;
    glyph->w = w;
    glyph->h = h;
    shelfX += w + 1;
    if (h > shelfH) {
        shelfH = h;
    }
    return true;
}

/* ---- the PC release's art ---- */

#define ART_WIDTH 768
#define ART_HEIGHT 256

static uint16_t *art; /* ART_WIDTH x ART_HEIGHT console pixels, 0 transparent; NULL if not found */

static uint8_t *LoadFirst(const char *root, const char *const *names, size_t *size)
{
    char path[1024];
    int i;

    for (i = 0; names[i]; i++) {
        uint8_t *data;

        SDL_snprintf(path, sizeof(path), "%s/%s", root, names[i]);
        data = SDL_LoadFile(path, size);
        if (data) {
            return data;
        }
    }
    return NULL;
}

/* PICS/HI-RES/HUD2.TGA: uncompressed 16-bit A1R5G5B5, three 256x256 pages side by side. The rows
   are in file order (the game's coordinates count them so, whatever the origin bit says). */
static void LoadArt(void)
{
    static const char *const names[] = {"PICS/HI-RES/HUD2.TGA", "pics/hi-res/hud2.tga", "Pics/Hi-Res/Hud2.tga", NULL};
    size_t size = 0;
    uint8_t *tga;
    int x, y;

    if (!portConfig.pcDataPath || !(tga = LoadFirst(portConfig.pcDataPath, names, &size))) {
        return;
    }
    if (size < 18 || tga[1] != 0 || tga[2] != 2 || tga[16] != 16 ||
        tga[12] + tga[13] * 256 != ART_WIDTH || tga[14] + tga[15] * 256 != ART_HEIGHT ||
        size < (size_t)18 + tga[0] + (size_t)ART_WIDTH * ART_HEIGHT * 2) {
        SDL_free(tga);
        return;
    }
    art = malloc(sizeof(uint16_t) * ART_WIDTH * ART_HEIGHT);
    for (y = 0; y < ART_HEIGHT; y++) {
        const uint8_t *row = tga + 18 + tga[0] + (size_t)y * ART_WIDTH * 2;

        for (x = 0; x < ART_WIDTH; x++) {
            unsigned v = row[x * 2] | row[x * 2 + 1] << 8;
            /* black is the art's transparent colour */
            art[y * ART_WIDTH + x] = v & 0x7FFF ? (uint16_t)(((v >> 10) & 31) | (v & 31) << 10 | ((v >> 5) & 31) << 5) : 0;
        }
    }
    SDL_free(tga);
}

/* Copies a rectangle of the art into the page as a glyph; false without the art. */
static bool Blit(int ax, int ay, int w, int h, Glyph *glyph)
{
    int x, y;

    if (!art || !Place(w, h, glyph)) {
        return false;
    }
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            page[(glyph->y + y) * GLYPH_PAGE_SIZE + glyph->x + x] = art[(ay + y) * ART_WIDTH + ax + x];
        }
    }
    return true;
}

/* ---- key caps ---- */

#define CAP_HEIGHT 32

/* A key cap showing `text`, in the style of the art: a dark outline, a grey bevelled face and red
   lettering with a darker shadow. Narrower than the height it is a square cap. */
static bool MakeCap(const char *text, Glyph *glyph)
{
    const int scale = 2;
    int w = TextWidth(text, scale) + 16, x, y;
    int tx, ty;
    const int radius = 7;

    if (w < CAP_HEIGHT) {
        w = CAP_HEIGHT;
    }
    if (!Place(w, CAP_HEIGHT, glyph)) {
        return false;
    }
    for (y = 0; y < CAP_HEIGHT; y++) {
        for (x = 0; x < w; x++) {
            /* distance inside the rounded outline: -1 outside, 0 the outline, 1.. inwards */
            int cx = x < radius ? radius - x : x >= w - radius ? x - (w - 1 - radius) : 0;
            int cy = y < radius ? radius - y : y >= CAP_HEIGHT - radius ? y - (CAP_HEIGHT - 1 - radius) : 0;
            int d = SDL_min(SDL_min(x, w - 1 - x), SDL_min(y, CAP_HEIGHT - 1 - y));
            uint16_t colour;

            if (cx && cy) {
                int r2 = cx * cx + cy * cy;

                if (r2 > radius * radius) {
                    continue;
                }
                d = radius - (int)SDL_sqrtf((float)r2);
            }
            if (d <= 0) {
                colour = RGB(30, 30, 34);
            } else if (d <= 2) {
                colour = y * 2 < CAP_HEIGHT ? RGB(96, 98, 98) : RGB(54, 56, 58);
            } else {
                int shade = 98 - y * 12 / CAP_HEIGHT;

                colour = RGB(shade, shade + 4, shade + 4);
            }
            page[(glyph->y + y) * GLYPH_PAGE_SIZE + glyph->x + x] = colour;
        }
    }
    tx = glyph->x + (w - TextWidth(text, scale)) / 2;
    ty = glyph->y + (CAP_HEIGHT - LETTER_H * scale) / 2;
    DrawText(tx + 1, ty + 1, text, scale, RGB(140, 0, 0));
    DrawText(tx, ty, text, scale, RGB(204, 28, 12));
    glyph->dx = 0;
    glyph->dy = 0;
    glyph->qw = w / 2;
    glyph->qh = CAP_HEIGHT / 2;
    return true;
}

/* A cap from the art if it has one there, otherwise drawn. */
static bool KeyCap(int ax, int ay, const char *name, Glyph *glyph)
{
    if (ax >= 0 && Blit(ax, ay, 30, 30, glyph)) {
        glyph->dx = glyph->dy = 0;
        glyph->qw = glyph->qh = 16;
        return true;
    }
    return MakeCap(name, glyph);
}

/* An arrow for the direction keys (32 x 16 texels in the art); drawn when there is none. */
static bool MakeArrow(int ax, bool right, Glyph *glyph)
{
    int x, y;

    if (!Blit(ax, 226, 32, 16, glyph)) {
        if (!Place(32, 16, glyph)) {
            return false;
        }
        for (y = 0; y < 16; y++) {
            for (x = 0; x < 32; x++) {
                int fx = right ? x : 31 - x; /* the arrow points towards larger fx */
                float above = SDL_fabsf(y - 7.5f);
                bool head = fx >= 20 && above <= 7.5f * (31 - fx) / 11.0f;
                bool shaft = fx < 22 && above <= 3.2f;

                if (head || shaft) {
                    page[(glyph->y + y) * GLYPH_PAGE_SIZE + glyph->x + x] = RGB(136, 124, 60);
                }
            }
        }
    }
    /* where the console's arrow icons are: 16x8 at the top of their 16x16 box */
    glyph->dx = 0;
    glyph->dy = 0;
    glyph->qw = 16;
    glyph->qh = 8;
    return true;
}

/* ---- gamepad face buttons ---- */

/* A dark disc with a letter in the colour of the button. */
static bool MakeDisc(char letter, uint16_t colour, Glyph *glyph)
{
    char name[2] = {letter, 0};
    int x, y;

    if (!Place(32, 32, glyph)) {
        return false;
    }
    for (y = 0; y < 32; y++) {
        for (x = 0; x < 32; x++) {
            float dx = x - 15.5f, dy = y - 15.5f, d = SDL_sqrtf(dx * dx + dy * dy);
            uint16_t c;

            if (d > 15.7f) {
                continue;
            }
            if (d > 14.2f) {
                c = RGB(26, 26, 30);
            } else if (d > 12.0f) {
                c = dy < 0 ? RGB(86, 88, 92) : RGB(50, 52, 56);
            } else {
                int shade = 44 - y * 16 / 32;

                c = RGB(shade, shade, shade + 4);
            }
            page[(glyph->y + y) * GLYPH_PAGE_SIZE + glyph->x + x] = c;
        }
    }
    /* bold: each letter pixel is three texels wide */
    DrawText(glyph->x + 16 - (LETTER_W * 3) / 2, glyph->y + 16 - (LETTER_H * 3) / 2, name, 3, colour);
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
    static const uint16_t xboxColours[FACE_COUNT] = {RGB(228, 64, 42), RGB(134, 196, 64), RGB(46, 123, 214),
                                                     RGB(245, 196, 0)};
    static const char *const rowKeys[ROW_COUNT] = {"S", "SPACE", "F1", "ALT", "A", "D"};
    int i;

    built = true;
    LoadArt();
    /* the keys: ENTER and ESC from the art (158,194) and (158,162), the arrows (160,226), (194,226) */
    KeyCap(art ? 158 : -1, 162, "ESC", &keyIcons[ICON_TRIANGLE]);
    KeyCap(art ? 158 : -1, 194, "ENTER", &keyIcons[ICON_CROSS]);
    MakeCap("SPACE", &keyIcons[ICON_CIRCLE]);
    MakeCap("ALT", &keyIcons[ICON_SQUARE]);
    MakeArrow(160, false, &keyIcons[ICON_LEFT]);
    MakeArrow(194, true, &keyIcons[ICON_RIGHT]);
    for (i = 0; i < ROW_COUNT; i++) {
        if (!strcmp(rowKeys[i], "SPACE")) {
            keyRows[i] = keyIcons[ICON_CIRCLE];
        } else if (!strcmp(rowKeys[i], "ALT")) {
            keyRows[i] = keyIcons[ICON_SQUARE];
        } else {
            MakeCap(rowKeys[i], &keyRows[i]);
        }
    }
    for (i = 0; i < FACE_COUNT; i++) {
        MakeDisc(xboxLetters[i], xboxColours[i], &faceIcons[SET_XBOX][i]);
        MakeDisc(nintendoLetters[i], RGB(236, 236, 236), &faceIcons[SET_NINTENDO][i]);
    }
    free(art);
    art = NULL;
    GpuSetGlyphMemory(page);
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
