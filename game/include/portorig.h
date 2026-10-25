/* Port: the original Wipeout's tracks raced as guests (portorig.c; platform/original.c). */
#ifndef PORTORIG_H
#define PORTORIG_H

#include "menus.h"

void  PortOriginalChoose(int index);
void  PortOriginalRaceBegins(void);
int   PortOriginalOffered(void);
int   PortClassMove(int current, int step, int classCount);
int   PortOriginalClassMenu(MenuSelection *selection, short *trackCount, int draw);
void  PortOriginalMenuRestore(MenuSelection *selection);
char *PortRaceTrackName(short actTrackNum);
void  PortOriginalModelsLoad(void);      /* the original's track models, with the menus' (in its reserve heap) */
Object *PortOriginalModel(int index);    /* ... the model of a WIPEOUT line's track, or NULL */
short PortOriginalLibraryBase(short loaded);
short PortOriginalLibraryPlaced(TexTemplate *tiles, short tileCount, short loaded);

#endif
