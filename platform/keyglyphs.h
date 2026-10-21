/* The keyboard glyphs of the Windows release (its HUD2 art; tools/gen_keyglyphs.py makes
   keyglyphs.c): ESC, ENTER and the two arrows as that release draws them, and caps in the same
   style for the other keys the port shows. */
#ifndef KEYGLYPHS_H
#define KEYGLYPHS_H

#include <stdint.h>

typedef struct KeyArt {
    const char *name;
    int width, height;      /* texels; the Windows release draws them at half size in console pixels */
    const uint32_t *pixels; /* RGBA, red in the low byte; alpha 0 where the art is transparent */
} KeyArt;

enum {
    KEY_ART_ESC,
    KEY_ART_ENTER,
    KEY_ART_LEFT,
    KEY_ART_RIGHT,
    KEY_ART_SPACE,
    KEY_ART_ALT,
    KEY_ART_S,
    KEY_ART_F1,
    KEY_ART_A,
    KEY_ART_D,
    KEY_ART_COUNT
};

extern const KeyArt keyArt[KEY_ART_COUNT];

#endif
