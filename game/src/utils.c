#include "functions.h"
#include "ships.h"
#include "control.h"

extern int attractShip;
extern short arccos_tab[];


int abs(int value)
{
    int result;

    result = value;
    if (result < 0) {
        result = -result;
    }
    return result;
}
int GetAng(VECTOR vec1, VECTOR vec2)
{
    int product;
    int mag;

    product = ScalarProduct(vec1, vec2);
    mag = (GetMagnitude(vec1) * GetMagnitude(vec2)) >> 12;
    if (mag == 0) {
        product = 4095;
    } else {
        product = product / mag;
    }
    if (product > 4095) {
        product = 4095;
    } else if (product < -4095) {
        product = -4095;
    }
    return arccos(product);
}


int scalar_product(int *a, int *b)
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}


int ScalarProduct(VECTOR v, VECTOR w)
{
    return v.vx * w.vx + v.vy * w.vy + v.vz * w.vz;
}


void CrossProduct(VECTOR *out, VECTOR a, VECTOR b)
{
    out->vx = (a.vy * b.vz - a.vz * b.vy) >> 9;
    out->vy = (a.vz * b.vx - a.vx * b.vz) >> 9;
    out->vz = (a.vx * b.vy - a.vy * b.vx) >> 9;
}


void MinusVector(VECTOR v1, VECTOR v2, VECTOR *result)
{
    result->vx = v2.vx - v1.vx;
    result->vy = v2.vy - v1.vy;
    result->vz = v2.vz - v1.vz;
}


int GetMagnitude(VECTOR vec1)
{
    int mag;

    mag = ((vec1.vx * vec1.vx) + (vec1.vy * vec1.vy)) + (vec1.vz * vec1.vz);
    mag = SquareRoot0(mag);
    return mag;
}


int arccos(int value)
{
    int angle;
    int absValue;

    if (value < 0) {
        absValue = -value;
    } else {
        absValue = value;
    }
    angle = arccos_tab[absValue];
    if (value < 0) {
        angle = 8192 + (8192 - angle);
    }
    return angle;
}


int sar(int a, int n)
{
    if (a > 0) {
        return a >> n;
    } else {
        a = (-a) >> n;
        return -a;
    }
}


int ang(int angle)
{
    angle &= 0xFFF;
    if (angle < 0x801) {
        return angle;
    }
    return angle - 0x1000;
}


void assign(VECTOR *out, VECTOR v)
{
    out->vx = v.vx;
    out->vy = v.vy;
    out->vz = v.vz;
}


int newRand(void)
{
    int seed;

    seed = (attractShip >> 1) ^ (attractShip * 8);
    attractShip = seed;
    return seed & 0x7FFF;
}
