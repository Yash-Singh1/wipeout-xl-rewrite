#ifndef TTF_H
#define TTF_H

/* One texture entry of a .ttf file: 21 shorts (16-, 4- and 1-entry blocks). */
struct TexTemplate {
    short tex16[16];
    short tex4[4];
    short tex1[1];
};

#endif
