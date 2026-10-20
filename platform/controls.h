/* What the game tells the platform about the player's controls, and what the platform answers
   (controls.c, glyphs.c). While the player uses a PlayStation pad, or has not pressed anything yet,
   every answer leaves the console's behaviour as it is. */
#ifndef PORT_CONTROLS_H
#define PORT_CONTROLS_H

#include "psyq.h"

/* Every frame of a race: whether the player is steering the ship (racing, not paused), and the
   buttons the digital pad's six actions are bound to (the game's keyDef[]). The keyboard follows:
   its race keys press those buttons, and outside a steered race its keys drive the menus. */
void PortRaceControls(int steering, const unsigned short *bindings);

/* Button glyphs. DoButton (menus.c) asks for the glyph of the player's device in place of the
   console's icon: `icon` is the console icon it was going to draw (0 circle, 1 cross, 2 square,
   3 triangle, 4/5 left/right arrow, 6-9 the NegCon's face buttons), `poly` the quad it built at
   (x, y). When the device has another glyph the quad is rewritten and the width the glyph
   takes beyond the console's 16 pixels is returned (the label moves right by it); otherwise the
   quad stays and 0 is returned. */
int PortButtonGlyph(POLY_FT4 *poly, int icon, int x, int y);

/* The controller configuration shows the key of each action where the console shows the button
   bound to it: DoButton icon PORT_ICON_ACTION + row, the rows being thrust, fire, view, drop
   weapon, left and right air brake. */
#define PORT_ICON_ACTION 16
int PortShowsKeys(void);

/* The password symbols are the face buttons as shapes, not prompts: they stay the console's. */
void PortConsoleGlyphs(int forced);

/* Names of buttons in the game's text. The console's own text is returned for a PlayStation pad
   and when there is nothing better; otherwise the name of the key or button of the device. */
typedef enum { PORT_BUTTON_START, PORT_BUTTON_L1, PORT_BUTTON_L2, PORT_BUTTON_R1, PORT_BUTTON_R2 } PortButton;
char *PortButtonLabel(PortButton button, char *console);

#endif
