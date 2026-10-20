#ifndef SKELETON_H
#define SKELETON_H

typedef struct MATRIX { short m[3][3]; int t[3]; } MATRIX;

typedef struct Skeleton
{
    MATRIX           relative;
    MATRIX           absolute;
    short            update;
    struct Skeleton* super;
    struct Skeleton* sub;
    struct Skeleton* next;
} Skeleton;                     /* 0x50 bytes */

short rsin(short a);   /* sin_1 */
short rcos(short a);   /* rcos */
void  MulMatrix0(MATRIX* m0, MATRIX* m1, MATRIX* m2); /* MulRotMatrix0 */
void  SetTransMatrix(MATRIX* m); /* SetTransMatrix */
void  SetRotMatrix(MATRIX* m); /* SetRotMatrix */

#endif
