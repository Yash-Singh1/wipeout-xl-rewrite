#ifndef OBJECT2_H
#define OBJECT2_H

#include "psyq.h"

/* One double-buffer slot: the draw and display environments for one of the two screens. */
typedef struct DB {
    DRAWENV *draw;
    DISPENV *disp;
} DB;

/* Current primitive-buffer pointer, updated once per frame. */
typedef struct PrimPtr {
    char *ptr;
} PrimPtr;

#endif
