#ifndef SCENE_H
#define SCENE_H

/* GPU command for an unmodulated textured quadrilateral. */
#define GPU_FT4_RAW_TEXTURE 0x2D

#include "wipeout.h"

/* One entry of a scene (.PRM) file while it is walked: an object header, a skeleton,
   a vertex or normal list, or a primitive of any PRM type. */
typedef union Prm {
    char          *ptr;
    Object        *object;
    Skeleton      *skeleton;
    SVECTOR       *svector;
    Primitive     *primitive;
    F3            *f3;
    FT3           *ft3;
    F4            *f4;
    FT4           *ft4;
    G3            *g3;
    GT3           *gt3;
    G4            *g4;
    GT4           *gt4;
    LSF3          *lsf3;
    LSFT3         *lsft3;
    LSF4          *lsf4;
    LSFT4         *lsft4;
    LSG3          *lsg3;
    LSGT3         *lsgt3;
    LSG4          *lsg4;
    LSGT4         *lsgt4;
    SPR           *spr;
    Spline        *spline;
    PointLight    *pointLight;
    SpotLight     *spotLight;
    InfiniteLight *infiniteLight;
} Prm;

#endif
