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
int   PortOriginalDrawPicture(int index, int x, int y); /* ... or its picture (1 when drawn) */
short PortOriginalLibraryBase(short loaded);
short PortOriginalLibraryPlaced(TexTemplate *tiles, short tileCount, short loaded);
int   PortOriginalLooks(void);         /* the race is on a track of the original Wipeout (its scenery's look) */
int   PortOriginalEightShips(void);    /* ... an arcade race there: eight ships race, as in the original */
void  PortOriginalPlaceSky(Object *sky);   /* ... a Wipeout 64 track's sky, as it is loaded */
void  PortOriginalSceneInit(Object **prm, short prmCount);
void  PortOriginalCount(int updateCount);
void  PortOriginalSceneUpdate(int paused);

#endif
