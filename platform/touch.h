/* Touch controls (touch.cpp): the pad drawn over the picture for a touch screen. On the left a stick that
   appears where the thumb lands (the directional pad: steering and the menus' cursor); on the right, in a race,
   the race's actions (thrust, fire, the air brakes, drop, view) and in the menus the four face buttons; Start
   at the top, and a button that opens the debug console. In a race each button performs its action whatever
   button the player bound to it (as the keyboard's keys do), so the game's own prompts show the PlayStation
   pad's symbols. The settings say when they are shown: always, never, or (automatic) from the first touch until
   a key or gamepad is used (on a phone or tablet from the start). */
#ifndef PORT_TOUCH_H
#define PORT_TOUCH_H

#include <SDL3/SDL.h>

#ifdef __cplusplus
extern "C" {
#endif

/* PortSettings.touchControls */
enum { TOUCH_AUTO, TOUCH_ON, TOUCH_OFF };

void TouchInit(SDL_Window *window);
/* Every window event before the game sees it: fingers (and, while the controls are shown, the mouse's left
   button); 1 when the controls took it. A key or a gamepad button tells the automatic mode the player has
   gone back to them. */
int TouchEvent(const SDL_Event *event);
/* Whether the controls are shown (and take touches) now. */
int TouchShown(void);
/* Each time the events are read: lets go of quick taps' buttons once they have been held long enough. */
void TouchUpdate(void);
/* Lets go of every finger (the debug console opened, the window lost the focus). */
void TouchRelease(void);
/* Draws the controls, inside the debug console's ImGui frame (debug_ui.cpp). */
void TouchDraw(void);

#ifdef __cplusplus
}
#endif

#endif
