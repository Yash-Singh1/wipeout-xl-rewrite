/* The PsyQ libgte routines the game calls, each a direct translation of the
   library's own code in the retail executable (asm/6AE44.s of the decomp),
   running on the GTE in gte.c. */
#include <string.h>

#include "gte.h"
#include "psyq.h"

/* lwc2/swc2 of a 32-bit word of a structure */
static int32_t Word(const void *base, int offset)
{
    int32_t value;

    memcpy(&value, (const char *)base + offset, 4);
    return value;
}

static void StoreWord(void *base, int offset, int32_t value)
{
    memcpy((char *)base + offset, &value, 4);
}

void InitGeom(void)
{
    GteWriteControl(29, 0x155);       /* ZSF3 */
    GteWriteControl(30, 0x100);       /* ZSF4 */
    GteWriteControl(26, 0x3E8);       /* H */
    GteWriteControl(27, -0x1062);     /* DQA */
    GteWriteControl(28, 0x1400000);   /* DQB */
    GteWriteControl(24, 0);           /* OFX */
    GteWriteControl(25, 0);           /* OFY */
}

void SetGeomOffset(int ofx, int ofy)
{
    GteWriteControl(24, (int32_t)((uint32_t)ofx << 16));
    GteWriteControl(25, (int32_t)((uint32_t)ofy << 16));
}

void SetGeomScreen(int h)
{
    GteWriteControl(26, h);
}

static void SetMatrixRegisters(MATRIX *m, int first)
{
    int i;

    for (i = 0; i < 5; i++) {
        GteWriteControl(first + i, Word(m, i * 4));
    }
}

void SetRotMatrix(MATRIX *m)
{
    SetMatrixRegisters(m, 0);
}

void SetLightMatrix(MATRIX *m)
{
    SetMatrixRegisters(m, 8);
}

void SetColorMatrix(MATRIX *m)
{
    SetMatrixRegisters(m, 16);
}

void SetTransMatrix(MATRIX *m)
{
    GteWriteControl(5, Word(m, 0x14));
    GteWriteControl(6, Word(m, 0x18));
    GteWriteControl(7, Word(m, 0x1C));
}

void SetBackColor(int rbk, int gbk, int bbk)
{
    GteWriteControl(13, (int32_t)((uint32_t)rbk << 4));
    GteWriteControl(14, (int32_t)((uint32_t)gbk << 4));
    GteWriteControl(15, (int32_t)((uint32_t)bbk << 4));
}

MATRIX *TransMatrix(MATRIX *m, VECTOR *v)
{
    StoreWord(m, 0x14, Word(v, 0));
    StoreWord(m, 0x18, Word(v, 4));
    StoreWord(m, 0x1C, Word(v, 8));
    return m;
}

int RotTransPers(SVECTOR *v0, int *sxy, int *p, int *flag)
{
    GteWriteData(0, Word(v0, 0));
    GteWriteData(1, Word(v0, 4));
    GteExecute(0x4A180001); /* rtps */
    *sxy = GteReadData(14);
    *p = GteReadData(8);
    *flag = GteReadControl(31);
    return GteReadData(19) >> 2;
}

int RotTransPers3(SVECTOR *v0, SVECTOR *v1, SVECTOR *v2, int *sxy0, int *sxy1, int *sxy2, int *p, int *flag)
{
    GteWriteData(0, Word(v0, 0));
    GteWriteData(1, Word(v0, 4));
    GteWriteData(2, Word(v1, 0));
    GteWriteData(3, Word(v1, 4));
    GteWriteData(4, Word(v2, 0));
    GteWriteData(5, Word(v2, 4));
    GteExecute(0x4A280030); /* rtpt */
    *sxy0 = GteReadData(12);
    *sxy1 = GteReadData(13);
    *sxy2 = GteReadData(14);
    *p = GteReadData(8);
    *flag = GteReadControl(31);
    return GteReadData(19) >> 2;
}

/* n vertices: screen positions, depths (SZ3) and the upper flag bits. */
void RotTransPersN(SVECTOR *v0, DVECTOR *sxy, u_short *sz, u_short *flag, int n)
{
    do {
        GteWriteData(0, Word(v0, 0));
        GteWriteData(1, Word(v0, 4));
        GteExecute(0x4A180001); /* rtps */
        v0++;
        n--;
        StoreWord(sxy, 0, GteReadData(14));
        *sz = (u_short)GteReadData(19);
        *flag = (u_short)((uint32_t)GteReadControl(31) >> 12);
        sxy++;
        sz++;
        flag++;
    } while (n > 0);
}

void RotTrans(SVECTOR *v0, VECTOR *v1, int *flag)
{
    GteWriteData(0, Word(v0, 0));
    GteWriteData(1, Word(v0, 4));
    GteExecute(0x4A480012); /* mvmva: rotation * V0 + translation */
    StoreWord(v1, 0, GteReadData(25));
    StoreWord(v1, 4, GteReadData(26));
    StoreWord(v1, 8, GteReadData(27));
    *flag = GteReadControl(31);
}

int AverageZ3(int sz0, int sz1, int sz2)
{
    GteWriteData(17, sz0);
    GteWriteData(18, sz1);
    GteWriteData(19, sz2);
    GteExecute(0x4B58002D); /* avsz3 */
    return GteReadData(7);
}

int AverageZ4(int sz0, int sz1, int sz2, int sz3)
{
    GteWriteData(16, sz0);
    GteWriteData(17, sz1);
    GteWriteData(18, sz2);
    GteWriteData(19, sz3);
    GteExecute(0x4B68002E); /* avsz4 */
    return GteReadData(7);
}

/* m2 = m0 * m1, a column at a time through the GTE. */
MATRIX *MulMatrix0(MATRIX *m0, MATRIX *m1, MATRIX *m2)
{
    int32_t a3, a4, a5, a6, a7, a8, a9, a10;
    int i;

    for (i = 0; i < 5; i++) {
        GteWriteControl(i, Word(m0, i * 4));
    }
    GteWriteData(0, (Word(m1, 0) & 0xFFFF) | (Word(m1, 4) & (int32_t)0xFFFF0000));
    GteWriteData(1, Word(m1, 0xC));
    GteExecute(0x4A486012);
    a3 = GteReadData(9);
    a4 = GteReadData(10);
    a5 = GteReadData(11);
    GteWriteData(0, ((Word(m1, 0) >> 16) & 0xFFFF) | (int32_t)((uint32_t)Word(m1, 8) << 16));
    {
        int16_t m21;
        memcpy(&m21, (char *)m1 + 0xE, 2);
        GteWriteData(1, m21);
    }
    GteExecute(0x4A486012);
    a6 = GteReadData(9);
    a7 = GteReadData(10);
    a8 = GteReadData(11);
    GteWriteData(0, (Word(m1, 4) & 0xFFFF) | (Word(m1, 8) & (int32_t)0xFFFF0000));
    GteWriteData(1, Word(m1, 0x10));
    GteExecute(0x4A486012);
    StoreWord(m2, 0x0, (a3 & 0xFFFF) | (int32_t)((uint32_t)a6 << 16));
    StoreWord(m2, 0xC, (a5 & 0xFFFF) | (int32_t)((uint32_t)a8 << 16));
    a9 = GteReadData(9);
    a10 = GteReadData(10);
    StoreWord(m2, 0x4, (a9 & 0xFFFF) | (int32_t)((uint32_t)a4 << 16));
    StoreWord(m2, 0x8, (a7 & 0xFFFF) | (int32_t)((uint32_t)a10 << 16));
    StoreWord(m2, 0x10, GteReadData(11));
    return m2;
}

/* ---- the library's table math ---- */

static int Sin1(int a)
{
    if ((unsigned)a < 0x400) {
        return GteTable16(0x80091880 + a * 2);
    }
    if ((unsigned)(a - 0x400) < 0x400) {
        return GteTable16(0x80091880 + (0x7FF - a) * 2);
    }
    if ((unsigned)(a - 0x800) < 0x400) {
        return -GteTable16(0x80090880 + a * 2);
    }
    if ((unsigned)(a - 0xC00) < 0x400) {
        return -GteTable16(0x80091880 + (0xFFF - a) * 2);
    }
    return a; /* unreachable for 0..0xFFF: the library returns its argument */
}

int rsin(int a)
{
    if (a >= 0) {
        return Sin1(a & 0xFFF);
    }
    a = -a;
    /* the library rounds toward zero before taking the remainder */
    return -Sin1(a - ((a < 0 ? a + 0xFFF : a) >> 12) * 4096);
}

int rcos(int a)
{
    int v;

    if (a < 0) {
        a = -a;
    }
    v = ((a < 0 ? a + 0xFFF : a) >> 12) * 4096;
    a -= v;
    if ((unsigned)a < 0x400) {
        return GteTable16(0x80091880 + (0x3FF - a) * 2);
    }
    if ((unsigned)(a - 0x400) < 0x400) {
        return -GteTable16(0x80091080 + a * 2);
    }
    if ((unsigned)(a - 0x800) < 0x400) {
        return -GteTable16(0x80091880 + (0xBFF - a) * 2);
    }
    if ((unsigned)(a - 0xC00) < 0x400) {
        return GteTable16(0x80090080 + a * 2);
    }
    return a;
}

int SquareRoot0(int a)
{
    int32_t lz, shift, half, t4;
    int t2;

    GteWriteData(30, a);
    lz = GteReadData(31);
    if (lz == 0x20) {
        return 0;
    }
    t2 = lz & ~1;
    half = (0x1F - t2) >> 1;
    shift = t2 - 0x18;
    if (shift >= 0) {
        t4 = (int32_t)((uint32_t)a << shift);
    } else {
        t4 = a >> (0x18 - t2);
    }
    t4 = (t4 - 0x40) * 2;
    return (int)((uint32_t)(GteTable16(0x80092094 + t4) << half) >> 12);
}

int ratan2(int y, int x)
{
    int negativeX = 0, negativeY = 0;
    int quotient, v;

    if (x < 0) {
        negativeX = 1;
        x = -x;
    }
    if (y < 0) {
        negativeY = 1;
        y = -y;
    }
    if (x == 0 && y == 0) {
        return 0;
    }
    if (y < x) {
        if (y & 0x7FE00000) {
            quotient = y / (x >> 10);
        } else {
            quotient = (y << 10) / x;
        }
        v = GteTable16(0x800926B0 + quotient * 2);
    } else {
        if (x & 0x7FE00000) {
            quotient = x / (y >> 10);
        } else {
            quotient = (x << 10) / y;
        }
        v = 0x400 - GteTable16(0x800926B0 + quotient * 2);
    }
    if (negativeX) {
        v = 0x800 - v;
    }
    if (negativeY) {
        v = -v;
    }
    return v;
}
