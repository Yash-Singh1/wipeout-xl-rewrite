/* The window, its OpenGL context and the event pump. */
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <stdarg.h>
#include <stdio.h>

#include "platform.h"
#include "psx/gpu.h"

static SDL_Window *window;
static SDL_GLContext context;

/* Input events go to the pad emulation (platform/psx), which defines this. */
void PsxInputEvent(const SDL_Event *event);

void PortLog(const char *format, ...)
{
    static int enabled = -1;
    va_list arguments;

    if (enabled < 0) {
        enabled = SDL_getenv("WIPEOUT_DEBUG") != NULL;
    }
    if (enabled) {
        va_start(arguments, format);
        vfprintf(stderr, format, arguments);
        va_end(arguments);
    }
}

int PortVideoInit(void)
{
    SDL_WindowFlags flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;

    if (portConfig.hidden) {
        flags |= SDL_WINDOW_HIDDEN;
    } else if (portConfig.fullscreen) {
        flags |= SDL_WINDOW_FULLSCREEN;
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    window = SDL_CreateWindow("Wipeout XL", portConfig.windowWidth, portConfig.windowHeight, flags);
    if (!window) {
        SDL_Log("window: %s", SDL_GetError());
        return 0;
    }
    context = SDL_GL_CreateContext(window);
    if (!context) {
        SDL_Log("OpenGL: %s", SDL_GetError());
        return 0;
    }
    SDL_GL_MakeCurrent(window, context);
    /* The game paces itself to the console's field rate; no vsync wait here. */
    SDL_GL_SetSwapInterval(0);
    return GpuInit();
}

void PortVideoShutdown(void)
{
    if (context) {
        SDL_GL_DestroyContext(context);
    }
    if (window) {
        SDL_DestroyWindow(window);
    }
}

void PortDrawableSize(int *width, int *height)
{
    SDL_GetWindowSizeInPixels(window, width, height);
}

static void Capture(void)
{
    static int shown;
    int width, height;
    unsigned char *pixels;
    char path[1024];
    FILE *file;
    int y;

    shown++;
    if (!portConfig.captureDir || portConfig.captureEvery <= 0 || shown % portConfig.captureEvery) {
        return;
    }
    PortDrawableSize(&width, &height);
    pixels = SDL_malloc((size_t)width * height * 3);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels);
    SDL_snprintf(path, sizeof(path), "%s/frame_%06d.ppm", portConfig.captureDir, shown);
    file = fopen(path, "wb");
    if (file) {
        fprintf(file, "P6\n%d %d\n255\n", width, height);
        for (y = height - 1; y >= 0; y--) {
            fwrite(pixels + (size_t)y * width * 3, 1, (size_t)width * 3, file);
        }
        fclose(file);
    }
    SDL_free(pixels);
}

void PortVideoPresent(void)
{
    Capture();
    SDL_GL_SwapWindow(window);
}

int PortPumpEvents(void)
{
    SDL_Event event;

    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) {
            return 0;
        }
        PsxInputEvent(&event);
    }
    return 1;
}
