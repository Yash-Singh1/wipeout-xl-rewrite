/* Compile with GPU_SOURCE naming a reference renderer to compare every colour/depth bit.
   Covers pass elimination, palette uploads, wrapping, HD fallback and mixed batches. */
#ifndef GPU_SOURCE
#define GPU_SOURCE "../../platform/psx/gpu_gl.c"
#endif
#include GPU_SOURCE
#include <assert.h>
#include <stdio.h>

PortConfig portConfig;
PortSettings portSettings;
uint16_t psxVram[VRAM_HEIGHT][VRAM_WIDTH], psxVramHigh[VRAM_HEIGHT][VRAM_WIDTH];
int portPal;
static int hdEnabled, referenceMode;
static uint8_t hdPixels[16 * 16 * 4];
static uint32_t glyphPixels[GLYPH_ATLAS_SIZE * GLYPH_ATLAS_SIZE], portPixels[256 * 256];
int PortLinkDownsample(void) { return -1; }
int PortLinkDownsampleStyle(void) { return -1; }
void PortDrawableSize(int *w, int *h) { *w = 320; *h = 240; }
void HdTexSetScale(int n) {}
int HdTexActive(void) { return hdEnabled || referenceMode; }
unsigned HdTexEpoch(void) { return 1; }
int HdTexImageCount(void) { return 1; }
const uint8_t *HdTexImage(int i, int *w, int *h) { *w = *h = 16; return hdPixels; }
int HdTexFind(int a, int b, int c, int d, int e, float f, float g, float h, float i, HdTexHit *hit)
{
    *hit = (HdTexHit){0, 0, 0, 16, 16, 0, 0, 16, 16};
    return hdEnabled;
}
void HdTexChanged(int x, int y, int w, int h) {}
void HdTexOverwritten(int x, int y, int w, int h) {}

static uint32_t randomState = 0x21700983;
static uint32_t Next(void)
{
    randomState ^= randomState << 13; randomState ^= randomState >> 17; randomState ^= randomState << 5;
    return randomState;
}
static uint64_t Hash(const void *data, size_t size)
{
    const uint8_t *bytes = data;
    uint64_t result = UINT64_C(14695981039346656037);
    for (size_t i = 0; i < size; i++) result = (result ^ bytes[i]) * UINT64_C(1099511628211);
    return result;
}
static void Palette(int high, int x, int kind)
{
    uint16_t (*vram)[VRAM_WIDTH] = high ? psxVramHigh : psxVram;
    for (int i = 0; i < 256; i++) {
        uint16_t colour = (Next() & 0x7fff) | 1;
        if (kind == 1 || (kind == 2 && i % 2)) colour |= 0x8000;
        if (kind == 3 || i % 11 == 0) colour = 0;
        vram[400][(x + i) & 1023] = colour;
    }
    if (high) GpuVramHighChanged(); else GpuVramChanged(0, 400, 1024, 1);
}

/* An unchanged cache key must be reclassified after either half of VRAM is uploaded. */
static void PaletteChanges(void)
{
    GpuDrawState state = {.semi = 2, .textured = 1, .raw = 1, .depth = 0,
                          .pageX = 512, .pageY = 128, .clutX = 768};
    GpuVertex triangle[3] = {{.x = 0, .y = 0, .a = 255},
                             {.x = 20, .y = 0, .a = 255},
                             {.x = 0, .y = 20, .a = 255}};
    GpuSetDrawArea(0, 0, 32, 32, 0, 0);
    psxVram[128][512] = 0x1111;
    GpuVramChanged(512, 128, 1, 1);
    for (int high = 0; high < 2; high++) {
        uint16_t (*vram)[VRAM_WIDTH] = high ? psxVramHigh : psxVram;
        state.clutY = 400 + high * 512;
        memset(&vram[400][768], 0, 16 * sizeof(uint16_t));
        for (int step = 0; step < 3; step++) {
            vram[400][769] = step == 1 ? 0x8010 : 0x0010;
            if (high) GpuVramHighChanged(); else GpuVramChanged(768, 400, 16, 1);
            GpuClearArea(0, 0, 32, 32, 96, 96, 96);
            GpuTriangles(&state, triangle, 3);
            int w, h;
            const uint8_t *pixel = GpuReadArea(2, 2, 1, 1, &w, &h);
            assert(pixel[0] == (step == 1 ? 0 : 128));
            assert(pixel[1] == (step == 1 ? 96 : 0));
            assert(pixel[2] == (step == 1 ? 96 : 0));
        }
    }
}

/* More live palettes than cache layers, then texture and CLUT writes with both
   completed batches and a final pending primitive. Compare with raw VRAM sampling. */
static void PageChanges(void)
{
    uint64_t expected = 0;
    int savedReference = referenceMode;
    hdEnabled = 0;
    SetWideMargin(0);
    GpuSetDrawArea(0, 0, 128, 96, 0, 0);
    for (int raw = 0; raw < 2; raw++) {
        referenceMode = raw;
        for (int y = 0; y < VRAM_HEIGHT; y++) {
            for (int x = 0; x < VRAM_WIDTH; x++) {
                psxVram[y][x] = psxVramHigh[y][x] = 0x1111;
            }
        }
        for (int i = 0; i < 512; i++) {
            psxVram[300 + i / 64][(i % 64) * 16 + 1] = 0x8001u + (i * 31u & 0x7fffu);
        }
        GpuVramChanged(0, 0, VRAM_WIDTH, VRAM_HEIGHT);
        GpuVramHighChanged();
        GpuClearArea(0, 0, 128, 96, 160, 160, 160);
        for (int i = 0; i < 516; i++) {
            GpuDrawState s = {.semi = 2, .textured = 1, .raw = 1, .depth = i & 1,
                              .pageX = 1008, .pageY = 1008,
                              .clutX = (i % 64) * 16, .clutY = 300 + i / 64};
            GpuVertex v[3] = {{.x = i % 110, .y = i % 70, .u = 0, .v = 0, .a = 255},
                              {.x = i % 110 + 16, .y = i % 70, .u = 31, .v = 0, .a = 255},
                              {.x = i % 110, .y = i % 70 + 16, .u = 0, .v = 31, .a = 255}};
            for (int k = 0; k < 3; k++) v[k].r = v[k].g = v[k].b = 128;
            GpuTriangles(&s, v, 3);
            if (i == 510) {
                psxVramHigh[496][1008] = 0x2222;
                GpuVramHighChanged();
                psxVram[300][1] = 0x801f;
                GpuVramChanged(1, 300, 1, 1);
                psxVram[0][0] = 0x3333; /* the wrapped part of the page */
                GpuVramChanged(0, 0, 1, 1);
            }
        }
        int w, h;
        const uint8_t *pixels = GpuReadArea(0, 0, 128, 96, &w, &h);
        uint64_t result = Hash(pixels, (size_t)w * h * 4);
        if (!raw) expected = result;
        else assert(result == expected);
        assert(!RenderBackendError()[0]);
    }
    referenceMode = savedReference;
}

static void OffscreenPicture(void)
{
    GpuDrawState state = {.semi = -1};
    GpuVertex v[3] = {{.x = 500, .y = 0, .a = 255},
                      {.x = 520, .y = 0, .a = 255},
                      {.x = 500, .y = 20, .a = 255}};
    SetWideMargin(16);
    GpuSetDrawArea(0, 0, 320, 240, 0, 0);
    GpuClearArea(0, 0, 320, 240, 32, 48, 64);
    drawnSincePresent = wideDirty = 0;
    GpuTriangles(&state, v, 3);
    GpuFlush();
    assert(drawnSincePresent && wideDirty);
    int w, h;
    const uint8_t *pixel = GpuReadArea(2, 2, 1, 1, &w, &h);
    assert(pixel[0] == 32 && pixel[1] == 48 && pixel[2] == 64);
    SetWideMargin(0);
}

/* A partial or disjoint clear must preserve the depth outside its rectangle,
   including when depth was written into the separate widescreen target. */
static void PartialDepthClear(void)
{
    GpuDrawState state = {.semi = -1};
    GpuVertex v[3] = {{.x = 0, .y = 0, .w = 100, .r = 255, .a = 255},
                      {.x = 32, .y = 0, .w = 100, .r = 255, .a = 255},
                      {.x = 0, .y = 32, .w = 100, .r = 255, .a = 255}};
    portSettings.consoleDepth = 0;
    for (int wide = 0; wide < 2; wide++) {
        SetWideMargin(wide ? 16 : 0);
        GpuSetDrawArea(0, 0, 320, 240, 0, 0);
        GpuClearArea(0, 0, 320, 240, 0, 0, 0);
        for (int i = 0; i < 3; i++) { v[i].w = 100; v[i].r = 255; v[i].g = 0; }
        GpuTriangles(&state, v, 3);
        GpuClearDepth(64, 64, 8, 8); /* disjoint from the drawn triangle */
        GpuClearDepth(0, 0, 8, 8);
        for (int i = 0; i < 3; i++) { v[i].w = 300; v[i].r = 0; v[i].g = 255; }
        GpuTriangles(&state, v, 3);
        int w, h;
        const uint8_t *p = GpuReadArea(4, 4, 1, 1, &w, &h);
        assert(p[0] == 0 && p[1] == 255);
        p = GpuReadArea(12, 4, 1, 1, &w, &h);
        assert(p[0] == 255 && p[1] == 0);
    }
    SetWideMargin(0);
}

int main(int argc, char **argv)
{
    assert(SDL_Init(SDL_INIT_VIDEO));
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_Window *window = SDL_CreateWindow("GPU pass regression", 320, 240, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
    assert(window);
    SDL_GLContext context = SDL_GL_CreateContext(window);
    assert(context);
    portSettings.renderScale = 1;
    assert(RenderStart(window));
    assert(GpuInit());
    if(SDL_getenv("WIPEOUT_TEST_METAL")){assert(RenderRequestBackend(RENDER_METAL));assert(RenderApplyBackend());}
    PaletteChanges();
    PageChanges();
    OffscreenPicture();
    PartialDepthClear();
    for (int i = 0; i < 16 * 16; i++) {
        hdPixels[i * 4] = i; hdPixels[i * 4 + 1] = 255 - i; hdPixels[i * 4 + 2] = 128;
        hdPixels[i * 4 + 3] = i % 3 == 0 ? 0 : i % 3 == 1 ? 128 : 255;
    }
    for (int i = 0; i < GLYPH_ATLAS_SIZE * GLYPH_ATLAS_SIZE; i++) {
        glyphPixels[i] = ((uint32_t)i * 7919u) | 0xff000000u;
    }
    for (int i = 0; i < 256 * 256; i++) {
        portPixels[i] = (uint32_t)(i * 31) | (i % 3 ? 0xff000000u : 0);
    }
    for (int y = 128; y < 384; y++) for (int x = 512; x < 768; x++) psxVram[y][x] = Next();
    GpuVramChanged(512, 128, 256, 256);
    FILE *output = argc == 2 ? fopen(argv[1], "wb") : NULL;
    assert(argc != 2 || output);
    int cases = 0;
    uint8_t *expectedPixels = NULL;
    for (int factor = 1; factor <= SDL_min(3, GpuMaxScale()); factor += 2) {
        SetScale(factor);
        for (int scene = 0; scene < 256; scene++) {
            uint32_t sceneSeed = randomState;
            uint64_t expectedColour = 0, expectedDepth = 0;
            for (referenceMode = 0; referenceMode <= 1; referenceMode++) {
                randomState = sceneSeed;
                int high = (scene / 4) & 1, clutX = scene & 1 ? 1008 : 768, y = scene & 2 ? 256 : 0;
                portSettings.consoleDepth = (scene / 8) & 1;
                hdEnabled = (scene / 32) & 1;
                SetWideMargin(scene & 16 ? 16 : 0);
                GpuSetDrawArea(0, y, 128, 96, 0, y);
                GpuClearDepth(0, y, 128, 96);
                GpuFill(0, y, 128, 96, Next(), Next(), Next());
                Palette(high, clutX, scene % 4);
                GpuSetPortPage(1, portPixels);
                for (int primitive = 0; primitive < 24; primitive++) {
                    GpuDrawState s = {0};
                    s.semi = primitive % 5 - 1;
                    s.textured = primitive % 7 != 0;
                    s.raw = primitive & 1;
                    s.depth = primitive % 3;
                    s.pageX = 512; s.pageY = 128;
                    if (primitive % 11 == 9) { s.depth = 3; s.pageX = 0; s.pageY = 0; }
                    if (primitive % 11 == 8) { s.depth = 3; s.pageX = 64; s.pageY = 0; }
                    s.clutX = clutX; s.clutY = 400 + (high ? 512 : 0);
                    s.windowMaskX = scene & 1 ? 3 : 0;
                    s.windowOffsetX = scene & 2 ? 2 : 0;
                    GpuVertex v[3] = {0};
                    for (int i = 0; i < 3; i++) {
                        v[i].x = (int)(Next() % 160) - 16;
                        v[i].y = y + (int)(Next() % 128) - 16;
                        v[i].u = Next() % 280; v[i].v = Next() % 256;
                        v[i].r = Next(); v[i].g = Next(); v[i].b = Next();
                        v[i].a = scene & 2 ? 255 : Next();
                        v[i].w = primitive & 1 ? 100 + Next() % 300 : 0;
                        v[i].noDepth = primitive % 11 == 0;
                        /* These flags affect vertex depth, not the constant metadata varyings. */
                        v[i].exactDepth = primitive % 9 == 0 && i == 2;
                        v[i].backFacing = primitive % 13 == 0 && i == 1;
                    }
                    GpuTriangles(&s, v, 3);
                    /* Upload while the last primitive is pending: classification must use the new palette. */
                    if (primitive == 3) hdEnabled = !hdEnabled;
                    if (primitive == 6) GpuSetGlyphMemory(glyphPixels);
                    if (primitive == 16) GpuMoveFramebuffer(0, y, 32, 32, 64, y + 32);
                    if (primitive == 11) Palette(high, clutX, (scene + 1) % 4);
                }
                GpuFlush();
                int w, h;
                uint8_t *pixels = GpuReadArea(0, y, 128, 96, &w, &h);
                uint64_t colourHash = Hash(pixels, (size_t)w * h * 4);
                float *depth = malloc((size_t)w * h * sizeof(float));
                RgBindFramebuffer(RG_READ_FRAMEBUFFER, wideMargin ? wideFramebuffer : framebuffer);
                RgReadPixels(wideMargin * factor, y * factor, w, h, RG_DEPTH_COMPONENT, RG_FLOAT, depth);
                uint64_t depthHash = Hash(depth, (size_t)w * h * sizeof(float));
                free(depth);
                if (!referenceMode) {
                    expectedColour = colourHash; expectedDepth = depthHash;
                    expectedPixels = realloc(expectedPixels, (size_t)w * h * 4);
                    memcpy(expectedPixels, pixels, (size_t)w * h * 4);
                    if (output) { fwrite(&colourHash, sizeof(colourHash), 1, output); fwrite(&depthHash, sizeof(depthHash), 1, output); }
                } else {
                    if (colourHash != expectedColour || depthHash != expectedDepth) {
                        for (size_t at = 0; at < (size_t)w * h * 4; at++) {
                            if (pixels[at] != expectedPixels[at]) {
                                fprintf(stderr, "first difference (%d,%d) channel %d: %d/%d\n", (int)(at / 4 % w), (int)(at / 4 / w), (int)(at % 4), expectedPixels[at], pixels[at]);
                                break;
                            }
                        }
                        fprintf(stderr, "scale %d scene %d: colour %llx/%llx, depth %llx/%llx\n", factor, scene,
                                (unsigned long long)expectedColour, (unsigned long long)colourHash,
                                (unsigned long long)expectedDepth, (unsigned long long)depthHash);
                    }
                    assert(colourHash == expectedColour && depthHash == expectedDepth);
                }
                assert(!RenderBackendError()[0]);
            }
            cases++;
        }
    }
    free(expectedPixels);
    if (output) fclose(output);
    printf("%d GPU colour/depth cases covering palette classes, updates, overlaps, scales, HD and wrapping: OK\n", cases);
    RenderStop();
    SDL_GL_DestroyContext(context); SDL_DestroyWindow(window); SDL_Quit();
    return 0;
}
