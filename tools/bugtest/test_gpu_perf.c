/* Paired synthetic renderer benchmark. Alternates conservative texture sampling and
   pass selection with the caches enabled, using the current submission path on both
   sides. This is not a comparison with a historical renderer or whole-game FPS.
   Run separately from regression tests; prints latency samples for each render scale. */
#define main PassRegressionMain
#include "test_gpu_passes.c"
#undef main

enum { TRIANGLES = 1024 };
static GpuDrawState states[TRIANGLES];
static GpuVertex vertices[TRIANGLES][3];

static uint64_t Picture(int conservative)
{
    referenceMode = conservative;
    hdEnabled = 0; /* The reference activates the conservative path without supplying HD art. */
    GpuSetDrawArea(0, 0, 320, 240, 0, 0);
    uint64_t start = SDL_GetTicksNS();
    GpuClearArea(0, 0, 320, 240, 60, 80, 100);
    GpuClearDepth(0, 0, 320, 240);
    GpuVramChanged(768, 400, 64, 1); /* Include palette-cache rebuilding, as in a game frame. */
    for (int i = 0; i < TRIANGLES; i++) GpuTriangles(&states[i], vertices[i], 3);
    GpuFlush();
    glFinish();
    return SDL_GetTicksNS() - start;
}

int main(void)
{
    assert(SDL_Init(SDL_INIT_VIDEO));
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_Window *window = SDL_CreateWindow("paired GPU benchmark", 320, 240, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
    assert(window);
    SDL_GLContext context = SDL_GL_CreateContext(window);
    assert(context);
    portSettings.renderScale = 1;
    assert(GpuInit());
    for (int y = 128; y < 384; y++) for (int x = 512; x < 768; x++) psxVram[y][x] = Next();
    GpuVramChanged(512, 128, 256, 256);
    for (int kind = 0; kind < 4; kind++) {
        for (int i = 0; i < 16; i++) {
            uint16_t colour = (Next() & 0x7fff) | 1;
            if (kind == 1 || (kind == 2 && i % 2)) colour |= 0x8000;
            if (i == 0 || kind == 3) colour = 0;
            psxVram[400][768 + kind * 16 + i] = colour;
        }
    }
    GpuVramChanged(768, 400, 64, 1);
    for (int i = 0; i < TRIANGLES; i++) {
        states[i].textured = states[i].raw = 1;
        states[i].semi = i % 5 < 3 ? 2 : i % 5 == 3 ? -1 : 1;
        states[i].depth = 0; states[i].pageX = 512; states[i].pageY = 128;
        states[i].clutX = 768 + (i % 10 < 6 ? 0 : i % 10 < 8 ? 1 : 2) * 16;
        states[i].clutY = 400;
        int x = (int)(Next() % 384) - 32, y = Next() % 240;
        for (int k = 0; k < 3; k++) {
            GpuVertex *v = &vertices[i][k];
            v->x = x + (k == 1 ? 20 : 0); v->y = y + (k == 2 ? 16 : 0);
            v->u = k == 1 ? 31 : 0; v->v = k == 2 ? 31 : 0;
            v->w = 100 + Next() % 500;
            v->r = v->g = v->b = 128; v->a = i % 7 ? 255 : 192;
        }
    }
    sceneWide = 1; SetWideMargin(32);
    const int factors[] = {1, 6, 12};
    puts("scale,pair,conservative_ms,optimized_ms");
    for (int f = 0; f < 3; f++) {
        if (factors[f] > GpuMaxScale()) continue;
        SetScale(factors[f]); SetWideMargin(32);
        for (int warmup = 0; warmup < 8; warmup++) Picture(warmup & 1);
        Picture(1);
        int w, h;
        uint64_t expected = Hash(GpuReadArea(0, 0, 320, 240, &w, &h), (size_t)320 * factors[f] * 240 * factors[f] * 4);
        Picture(0);
        assert(expected == Hash(GpuReadArea(0, 0, 320, 240, &w, &h), (size_t)320 * factors[f] * 240 * factors[f] * 4));
        for (int pair = 0; pair < 64; pair++) {
            uint64_t before, after;
            if (pair & 1) { after = Picture(0); before = Picture(1); }
            else { before = Picture(1); after = Picture(0); }
            printf("%d,%d,%.6f,%.6f\n", factors[f], pair, before / 1e6, after / 1e6);
        }
    }
    SDL_GL_DestroyContext(context); SDL_DestroyWindow(window); SDL_Quit();
}
