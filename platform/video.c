/* The window, its OpenGL context and the event pump. */
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <stdarg.h>
#include <stdio.h>

#include "debug_ui.h"
#include "link.h"
#include "platform.h"
#include "psx/gpu.h"
#include "psx/psx_io.h"

static SDL_Window *window;
static SDL_GLContext context;

/* Input events go to the pad emulation (platform/psx), which defines this. */

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
    if (!portConfig.hidden || SDL_getenv("WIPEOUT_DEBUG_UI")) {
        DebugUiInit(window, context); /* WIPEOUT_DEBUG_UI: also in a hidden test run */
    }
    /* The game paces itself to the console's field rate (libetc.c). On
       screen each picture also waits for the display's refresh, so it never
       tears (adaptive where the driver offers it); tests run unthrottled. */
    if (portConfig.hidden || portConfig.fixedStep || !SDL_GL_SetSwapInterval(-1)) {
        SDL_GL_SetSwapInterval(portConfig.hidden || portConfig.fixedStep ? 0 : 1);
    }
    return GpuInit();
}

void PortVideoShutdown(void)
{
    DebugUiShutdown();
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

static int shown;

int PortShownFrames(void)
{
    return shown;
}

static void Capture(void)
{
    int width, height;
    unsigned char *pixels;
    char path[1024];
    FILE *file;
    int y;

    shown++;
    if (!portConfig.captureDir || portConfig.captureEvery <= 0 || shown < portConfig.captureFrom ||
        (portConfig.captureTo && shown > portConfig.captureTo) ||
        shown % portConfig.captureEvery) {
        return;
    }
    PortDrawableSize(&width, &height);
    pixels = SDL_malloc((size_t)width * height * 3);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels);
    if (SDL_getenv("WIPEOUT_ALPHA_CHECK")) {
        /* tests: pixels the window would show see-through */
        unsigned char *rgba = SDL_malloc((size_t)width * height * 4);
        long low = 0, k;

        glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
        for (k = 0; k < (long)width * height; k++) {
            low += rgba[k * 4 + 3] < 255;
        }
        PortLog("alpha: frame %d has %ld see-through pixels\n", shown, low);
        SDL_free(rgba);
    }
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

/* Tests (WIPEOUT_VRAM_DUMP=file.ppm): all of video memory as a picture. */
void PortDumpVram(void)
{
    const char *path = SDL_getenv("WIPEOUT_VRAM_DUMP");
    FILE *file;
    int x, y;

    if (!path || !(file = fopen(path, "wb"))) {
        return;
    }
    GpuReadBack(0, 0, 320, VRAM_HEIGHT); /* the drawn part; the rest is only ever uploaded */
    fprintf(file, "P6\n%d %d\n255\n", VRAM_WIDTH, VRAM_HEIGHT);
    for (y = 0; y < VRAM_HEIGHT; y++) {
        for (x = 0; x < VRAM_WIDTH; x++) {
            uint16_t v = psxVram[y][x];

            fputc((v & 31) * 255 / 31, file);
            fputc((v >> 5 & 31) * 255 / 31, file);
            fputc((v >> 10 & 31) * 255 / 31, file);
        }
    }
    fclose(file);
}

void PortVideoPresent(void)
{
    /* The picture is opaque: blending (the subtractive modes, the fade) also changes the alpha the
       framebuffer holds, and the window system composites the window with it, so such pixels showed
       what is behind the window (black holes where ship shadows were). */
    glDisable(GL_SCISSOR_TEST); /* the presenters leave the window's framebuffer bound */
    DebugUiRender();
    glDisable(GL_SCISSOR_TEST);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    Capture();
    SDL_GL_SwapWindow(window);
}

/* The hidden test window never has the focus: it counts as focused unless its script says otherwise. */
static int scriptedFocus = 1;

int PortWindowFocused(void)
{
    if (portConfig.hidden) {
        return scriptedFocus;
    }
    return window && (SDL_GetWindowFlags(window) & SDL_WINDOW_INPUT_FOCUS);
}

int PortPumpEvents(void)
{
    SDL_Event event;

    PortInputScript(PsxFieldCount());
    if (PortLinkRole() == LINK_SLAVE && PortLinkQuitRequested()) {
        return 0; /* the first player's game closed the second's */
    }
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) {
            return 0;
        }
        /* Alt+Enter and F11 switch between window and full screen */
        if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat &&
            (event.key.scancode == SDL_SCANCODE_F11 ||
             (event.key.scancode == SDL_SCANCODE_RETURN && (event.key.mod & SDL_KMOD_ALT)))) {
            SDL_SetWindowFullscreen(window, !(SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN));
            continue;
        }
        if (DebugUiEvent(&event)) {
            continue; /* the debug console's (the backtick key) */
        }
        if (portConfig.hidden && (event.type == SDL_EVENT_WINDOW_FOCUS_LOST || event.type == SDL_EVENT_WINDOW_FOCUS_GAINED)) {
            scriptedFocus = event.type == SDL_EVENT_WINDOW_FOCUS_GAINED;
        }
        PsxInputEvent(&event);
    }
    return 1;
}
