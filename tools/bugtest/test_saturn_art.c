/* Generated HUD bars must match their draw origin; pipe sprites retain the disc's pixels. */
#include "../../platform/saturn_art.c"
#include <assert.h>

static unsigned char vdp1[0x80000], vdp2[0x80000], cram[0x1000];
static unsigned char map[64 * 64 * 2], pipeSprites[4 + 13 * 4 + 13 * 16];
static void BeWord(unsigned char *p, unsigned n) { p[0] = n >> 8; p[1] = n; }
static void BeLong(unsigned char *p, unsigned n) { BeWord(p, n >> 16); BeWord(p + 2, n); }
int PortSaturnEurope(void) { return 0; }
int SaturnTitlePicture(unsigned short *pixels) { return 0; }
SaturnVideo *SaturnLoadVideo(const char *path)
{
    assert(!strcmp(path, "art\\track01\\course01.vri"));
    return (SaturnVideo *)vdp1;
}
unsigned char *SaturnBlock(const char *path, int block, size_t *size)
{
    assert(!strcmp(path, "art\\track01\\course01.rbh") && block == 4);
    *size = sizeof(pipeSprites);
    return pipeSprites;
}
unsigned char *SaturnVideoAt(SaturnVideo *v, unsigned address, size_t bytes)
{
    if (address >= VDP1_RAM && address + bytes <= VDP1_RAM + sizeof(vdp1)) return vdp1 + address - VDP1_RAM;
    if (address >= 0x25e00000 && address + bytes <= 0x25e00000 + sizeof(vdp2)) return vdp2 + address - 0x25e00000;
    if (address >= COLOUR_RAM && address + bytes <= COLOUR_RAM + sizeof(cram)) return cram + address - COLOUR_RAM;
    return NULL;
}

static int uploads;
static unsigned short uploaded[512];
int LoadImage(RECT *rect, u_long *data)
{
    if (rect->h == 128) {
        assert(rect->x == 456 && rect->y == 808 && rect->w == 4);
        memcpy(uploaded, data, sizeof(uploaded));
    } else {
        assert(rect->x == 784 && rect->y == 1002 && rect->w == 16 && rect->h == 1);
        const unsigned short *p = (const unsigned short *)data;
        assert(p[0] == 0 && p[1] == 31 && p[2] == 0x8000);
    }
    uploads++;
    return 0;
}

int main(void)
{
    loaded = 1; video = (SaturnVideo *)vdp1; hudMap = map; hudMapSize = sizeof(map);
    BeWord(cram + 2, 31);
    /* A closed frame with a one-pixel divider at x41. The pixel immediately
       right of it is the user's reported gap at screen 269,191 in widescreen. */
    for (int cy = 0; cy < 8; cy++) for (int cx = 0; cx < 14; cx++) {
        int cell = cy * 14 + cx;
        BeWord(map + ((21 + cy) * 64 + 28 + cx) * 2, cell * 2);
        for (int y = 0; y < 8; y++) for (int x = 0; x < 8; x++) {
            int fx = cx * 8 + x - 1, fy = cy * 8 + y - 1;
            int ink = ((fx == 10 || fx == 105 || fx == 41) && fy >= 10 && fy <= 33) ||
                      ((fy == 10 || fy == 33) && fx >= 10 && fx <= 105);
            vdp2[0x20000 + cell * 64 + y * 8 + x] = ink;
        }
    }
    unsigned pixels[96 * 96] = {0}; Canvas bar = {96, 22, pixels};
    assert(Draw("revs.tim", 0, &bar));
    assert(pixels[18 * 96 + 30] != 0); /* Frame x42, not divider x41. */
    assert(pixels[18 * 96 + 29] == 0);
    for (int y = 1; y < 22; y++) for (int x = 0; x < 92; x++)
        assert(!!pixels[y * 96 + x] == (x + 12 != 41));

    BeLong(pipeSprites, 4);
    BeWord(vdp1 + 0x10000 + 2, 31 | 31 << 5);
    for (int frame = 0; frame < 13; frame++) {
        int at = 4 + 13 * 4 + frame * 16, src = 0x20000 + frame * 32 * 96 / 2;
        BeLong(pipeSprites + 4 + frame * 4, at);
        BeWord(pipeSprites + at, 1); BeWord(pipeSprites + at + 4, 8);
        BeWord(pipeSprites + at + 6, 0x10000 / 8);
        BeWord(pipeSprites + at + 12, src / 8); BeWord(pipeSprites + at + 14, 4 << 8 | 96);
        vdp1[src + (frame * 32 + frame) / 2] = frame & 1 ? 1 : 0x10;
    }
    Canvas smoke = {32, 96, pixels};
    for (int frame = 0; frame < 26; frame++) {
        memset(pixels, 0, sizeof(pixels)); Smoke(&smoke, frame);
        int ink = 0;
        for (int i = 0; i < 32 * 96; i++) if (pixels[i]) {
            ink++; assert(i == (frame % 13) * 33); assert(pixels[i] == 0xff00f8f8);
        }
        assert(ink == 1);
    }
    Canvas reflection = {64, 64, pixels};
    Chrome(&reflection);
    int dark = 0, bright = 0;
    for (int i = 0; i < 64 * 64; i++) {
        dark += (pixels[i] & 255) < 16;
        bright += (pixels[i] & 255) > 232;
        assert(pixels[i] >> 24 == 255 && Ps1Colour(pixels[i]) != 0);
    }
    assert(dark > 100 && bright > 100);
    unsigned char lightModels[2048] = {0};
    models = lightModels; modelsSize = sizeof(lightModels);
    pieces = sprites = map;
    assert(!PortSaturnLightFrame(0, 0, 0, 0, 0));
    assert(!PortSaturnLightFrame(0, 0, 0, 0, 13));
    for (int frame = 1; frame <= 12; frame++) {
        unsigned at = 64 + frame * 32, group = 512 + frame * 32;
        unsigned src = 0x38000 + frame * 1024, lut = 0x10000 + frame * 32;
        BeLong(lightModels + frame * 4, at); BeLong(lightModels + at, group);
        BeWord(lightModels + group + 20, src / 8); BeWord(lightModels + group + 26, lut / 8);
        memset(vdp1 + src, 0x12, 1024);
        vdp1[src] = frame << 4 | 2; BeWord(vdp1 + lut + 2, 0x801f);
        assert(PortSaturnLightFrame(7 | 16 | 2048, 1002 * 64 + 49, 32, 40, frame));
        assert(uploaded[0] == (0x2120 | frame) && uploads == frame * 2);
        assert(PortSaturnLightFrame(7 | 16 | 2048, 1002 * 64 + 49, 32, 40, frame));
        assert(uploads == frame * 2); /* Shared gantries reuse one upload. */
    }
    puts("Saturn HUD alignment, thirteen pipe sprites, opaque reflections and twelve gantry frames: OK");
}
