/* The debug console (debug_ui.cpp): the ~ key (below Esc) opens it. */
#ifndef PORT_DEBUG_UI_H
#define PORT_DEBUG_UI_H

#include <SDL3/SDL.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void DebugUiInit(SDL_Window *window, void *context);
void DebugUiShutdown(void);
/* Every window event; returns 1 when the console took it (the game does not see it). */
int DebugUiEvent(const SDL_Event *event);
int DebugUiOpen(void);
void DebugUiSetOpen(int open); /* (the touch controls' console button) */
/* Opens the console at the frame recorder's tab (Tab freezing the game, or a test's WIPEOUT_FRAMECAP_FREEZE_AT). */
void DebugUiShowRecorder(void);
/* Tab, for the frame recorder (also a test's WIPEOUT_FRAMECAP_TAB_AT). */
void DebugUiTab(void);
/* Draws the console and the frame-rate overlay into the window's framebuffer, before it is shown. */
void DebugUiRender(void);
/* Before the game: shows a message across the window (and a progress bar for fraction 0 to 1). */
void DebugUiSplash(const char *text, float fraction);

/* trace.c: the game's frames so far (its Swap calls) */
uint64_t PortGameFrames(void);

#ifdef __cplusplus
}
#endif

#endif
