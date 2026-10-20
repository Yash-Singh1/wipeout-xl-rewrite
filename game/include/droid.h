#ifndef DROID_H
#define DROID_H

#include "psyq.h"

/* Program header read by LoadProg (136 bytes). */
typedef union Header {
    int  align;
    u_char bytes[136];
} Header;

/* Shape slot of the rescue droid model. */
#define RescueDroidModel 30

#endif
