/* Mesh holes are console pixels at every render scale and do not occlude what is behind. */
#define main HudProbeMain
#include "test_gpu_hud.c"
#undef main
int main(void)
{
    assert(SDL_Init(SDL_INIT_VIDEO));
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_Window *win = SDL_CreateWindow("Saturn mesh", 320, 240, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
    SDL_GLContext ctx = SDL_GL_CreateContext(win); assert(ctx);
    portSettings.renderScale = 1; portSettings.consoleDepth = 0; assert(RenderStart(win)); assert(GpuInit());
    if (SDL_getenv("WIPEOUT_TEST_METAL")) {
        assert(RenderRequestBackend(RENDER_METAL)); assert(RenderApplyBackend());
    }
    for (int factor = 1; factor <= 3; factor += 2) {
        SetScale(factor); GpuSetDrawArea(0, 0, 320, 240, 0, 0);
        GpuFill(0, 0, 8, 8, 96, 96, 96);
        GpuVertex quad[6] = {
            {.x=0,.y=0}, {.x=8,.y=0}, {.x=0,.y=8},
            {.x=8,.y=0}, {.x=0,.y=8}, {.x=8,.y=8}
        };
        for (int i = 0; i < 6; i++) { quad[i].w=100; quad[i].r=200; quad[i].g=180; quad[i].a=255; }
        GpuDrawState mesh = {.semi=4}; GpuTriangles(&mesh, quad, 6);
        /* A farther opaque object drawn afterward must show through only the holes. */
        for (int i = 0; i < 6; i++) { quad[i].w=200; quad[i].r=0; quad[i].g=0; quad[i].b=240; }
        GpuDrawState opaque = {.semi=-1}; GpuTriangles(&opaque, quad, 6);
        int w,h; uint8_t *pixels=GpuReadArea(0,0,8,8,&w,&h);
        assert(w==8*factor && h==8*factor);
        int ink=0, holes=0;
        for (int y=0;y<h;y++) for (int x=0;x<w;x++) {
            const uint8_t *p=pixels+(y*w+x)*4;
            if (((x/factor+y/factor)&1)==0) { assert(p[0]==200 && p[1]==180 && p[2]==0); ink++; }
            else { assert(p[0]==0 && p[1]==0 && p[2]==240); holes++; }
        }
        assert(ink==32*factor*factor && holes==ink);
    }
    puts("Saturn mesh stays at console resolution; only its drawn pixels occlude: OK");
    SDL_GL_DestroyContext(ctx); SDL_DestroyWindow(win); SDL_Quit();
}
