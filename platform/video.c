/* The window, its OpenGL context and the event pump. */
#include <SDL3/SDL.h>
#include "port_gl.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "debug_ui.h"
#include "link.h"
#include "platform.h"
#include "psx/gpu.h"
#include "psx/psx_io.h"
#include "settings.h"
#include "touch.h"
#include "trace.h"
#ifdef PORT_SECOND_GAME
#include "second/second.h"
#endif

static SDL_Window *window;
static SDL_GLContext context;

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
/* The web: the browser shows the canvas at its next display refresh after the game gives it its turn. A
   picture ends the game's turn without waiting for that refresh (the game's time must not follow the
   display's), and is marked unseen until then. */
EM_ASYNC_JS(void, ShowPicture, (void), {
    Module.pictureUnseen = true;
    requestAnimationFrame(function () { Module.pictureUnseen = false; });
    await new Promise(function (resolve) { setTimeout(resolve, 0); });
});
/* ... and a picture between the game's frames waits until the one before it has been shown (in a hidden page,
   which shows nothing, it does not wait). */
EM_ASYNC_JS(void, WaitPictureShown, (void), {
    if (Module.pictureUnseen && !document.hidden) {
        await new Promise(requestAnimationFrame);
        await new Promise(function (resolve) { setTimeout(resolve, 0); });
    }
});
#endif

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

#ifdef PORT_SECOND_GAME
    if (portConfig.secondPlayer) {
        /* the second player's game: it draws with the hidden window the first game made for it (windows are the
           main thread's), on its own thread */
        window = PortSecondGameWindow();
        context = PortSecondGameContext();
        if (!window || !context || !SDL_GL_MakeCurrent(window, context)) {
            SDL_Log("second player: %s", SDL_GetError());
            return 0;
        }
        PortSwapInterval(0);
        return GpuInit();
    }
#endif
    if (portConfig.hidden) {
        flags |= SDL_WINDOW_HIDDEN;
    } else if (portConfig.fullscreen) {
        flags |= SDL_WINDOW_FULLSCREEN;
    }
#if defined(__EMSCRIPTEN__) || defined(PORT_MOBILE)
#ifdef __EMSCRIPTEN__
    flags |= SDL_WINDOW_FILL_DOCUMENT; /* the web: the whole page, following the browser window's size */
#else
    flags |= SDL_WINDOW_FULLSCREEN; /* a phone or tablet: the whole screen */
#endif
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3); /* WebGL 2, OpenGL ES 3 */
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
#else
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
#endif
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
#ifdef PORT_MOBILE
    SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight"); /* (the game is wide, whichever way up) */
#endif
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
#ifdef SDL_PLATFORM_IOS
    GpuSetWindowFramebuffer((unsigned int)SDL_GetNumberProperty(SDL_GetWindowProperties(window),
                                                                SDL_PROP_WINDOW_UIKIT_OPENGL_FRAMEBUFFER_NUMBER, 0));
#endif
    TouchInit(window);
    if (!portConfig.hidden || SDL_getenv("WIPEOUT_DEBUG_UI")) {
        DebugUiInit(window, context); /* WIPEOUT_DEBUG_UI: also in a hidden test run */
    }
    PortSwapInterval(1);
    return GpuInit();
}

SDL_Window *PortWindow(void)
{
    return window;
}

/* The game paces itself to the console's field rate (libetc.c). On screen each picture also waits for the
   display's refresh, so it never tears (adaptive where the driver offers it), unless the frame rate is
   uncapped or capped (the settings); tests run unthrottled. */
void PortSwapInterval(int vsync)
{
    static int current = -2;

    if (portConfig.hidden || portConfig.fixedStep) {
        vsync = 0;
    }
    if (vsync == current) {
        return;
    }
    current = vsync;
    if (!vsync || !SDL_GL_SetSwapInterval(-1)) {
        SDL_GL_SetSwapInterval(vsync);
    }
}

void PortVideoShutdown(void)
{
    DebugUiShutdown();
#ifdef PORT_SECOND_GAME
    if (portConfig.secondPlayer) {
        return; /* (its window and drawing context are the first game's to close: host.c) */
    }
#endif
    if (context) {
        SDL_GL_DestroyContext(context);
    }
    if (window) {
        SDL_DestroyWindow(window);
    }
}

int PortWindowFullscreen(void)
{
    return window && (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0;
}

void PortWindowSize(int *width, int *height)
{
    SDL_GetWindowSize(window, width, height);
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
    /* RGBA, the one format every OpenGL (ES too) reads back */
    pixels = SDL_malloc((size_t)width * height * 4);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    if (SDL_getenv("WIPEOUT_ALPHA_CHECK")) {
        /* tests: pixels the window would show see-through */
        long low = 0, k;

        for (k = 0; k < (long)width * height; k++) {
            low += pixels[k * 4 + 3] < 255;
        }
        PortLog("alpha: frame %d has %ld see-through pixels\n", shown, low);
    }
    SDL_snprintf(path, sizeof(path), "%s/frame_%06d.ppm", portConfig.captureDir, shown);
    file = fopen(path, "wb");
    if (file) {
        fprintf(file, "P6\n%d %d\n255\n", width, height);
        size_t k;

        for (k = 0; k < (size_t)width * height; k++) { /* RGBA to RGB, in place */
            memmove(pixels + k * 3, pixels + k * 4, 3);
        }
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
#ifdef __EMSCRIPTEN__
    ShowPicture();
#endif
}

/* Before a picture between the game's frames: on the desktop the swap's VSync (the settings) paces them; the
   web's pictures are paced here, one per display refresh at most. */
void PortVideoBetweenFrames(void)
{
#ifdef __EMSCRIPTEN__
    WaitPictureShown();
#endif
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

int PortFocusPause(void)
{
    return !portSettings.noFocusPause && !PortWindowFocused();
}

int PortSurround(void)
{
    return portSettings.surround || PortTracing();
}

void PortAutoSaveAtExit(void); /* (game/src/portsave.c) */

void PortQuit(void)
{
    SDL_Event event;

    if (portConfig.secondPlayer) {
        exit(0); /* the second player's game ends alone (its thread: platform/second/) */
    }
    SDL_zero(event);
    event.type = SDL_EVENT_QUIT;
    SDL_PushEvent(&event); /* (the next PortPumpEvents closes the game) */
}

int PortPumpEvents(void)
{
    SDL_Event event;

    PortInputScript(PsxFieldCount());
    if (PortLinkRole() == LINK_SLAVE) {
        /* the second player's game: the window and its events are the first's */
        return !PortLinkQuitRequested();
    }
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) {
            PortAutoSaveAtExit(); /* what changed since the last save (game/src/portsave.c) */
            return 0;
        }
        /* Alt+Enter and F11 switch between window and full screen */
        if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat &&
            (event.key.scancode == SDL_SCANCODE_F11 ||
             (event.key.scancode == SDL_SCANCODE_RETURN && (event.key.mod & SDL_KMOD_ALT)))) {
            SDL_SetWindowFullscreen(window, !(SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN));
            continue;
        }
#if !defined(__EMSCRIPTEN__) && !defined(PORT_MOBILE)
        /* the next start is full screen or not as the window is now (F11, Alt+Enter or the title bar's button) */
        if ((event.type == SDL_EVENT_WINDOW_ENTER_FULLSCREEN || event.type == SDL_EVENT_WINDOW_LEAVE_FULLSCREEN) &&
            portSettings.fullscreen != (event.type == SDL_EVENT_WINDOW_ENTER_FULLSCREEN)) {
            portSettings.fullscreen = event.type == SDL_EVENT_WINDOW_ENTER_FULLSCREEN;
            PortSettingsSave();
        }
#endif
        if (DebugUiEvent(&event)) {
            continue; /* the debug console's (the ~ key) */
        }
        if (TouchEvent(&event)) {
            continue; /* the touch controls' */
        }
        if (portConfig.hidden && (event.type == SDL_EVENT_WINDOW_FOCUS_LOST || event.type == SDL_EVENT_WINDOW_FOCUS_GAINED)) {
            scriptedFocus = event.type == SDL_EVENT_WINDOW_FOCUS_GAINED;
        }
        PsxInputEvent(&event);
    }
    TouchUpdate();
    return 1;
}
