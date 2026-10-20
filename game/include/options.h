#ifndef OPTIONS_H
#define OPTIONS_H

#include "psyq.h"

/* Background box and its semi-transparent blend mode, one pair per screen. */
struct SelectBox {
    POLY_F4 box[2];
    DR_MODE trans[2];
};

/* A menu entry's text, also copied as words. */
typedef union MenuText {
    int align[4];
    char text[16];
} MenuText;

#endif
