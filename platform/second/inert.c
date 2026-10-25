/* The second player's game (platform/second/): the debug console and the touch controls are the first game's, in
   its window; the second game's copy has neither (they are C++, which the second game's names would not suit). */
#include <SDL3/SDL.h>

#include "debug_ui.h"
#include "touch.h"

void DebugUiInit(SDL_Window *window, void *context)
{
    (void)window;
    (void)context;
}

void DebugUiShutdown(void) {}

int DebugUiEvent(const SDL_Event *event)
{
    (void)event;
    return 0;
}

int DebugUiOpen(void)
{
    return 0;
}

void DebugUiSetOpen(int open)
{
    (void)open;
}

void DebugUiShowRecorder(void) {}

void DebugUiTab(void) {}

void DebugUiRender(void) {}

void DebugUiSplash(const char *text, float fraction)
{
    (void)text;
    (void)fraction;
}

void TouchInit(SDL_Window *window)
{
    (void)window;
}

int TouchEvent(const SDL_Event *event)
{
    (void)event;
    return 0;
}

int TouchShown(void)
{
    return 0;
}

void TouchUpdate(void) {}

void TouchRelease(void) {}

void TouchDraw(void) {}
