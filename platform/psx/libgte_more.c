/* More PsyQ libgte routines the game calls, translated from the library's own code in the retail
   executable (asm/6AE44.s of the decomp) the way libgte.c does: each runs on the GTE of gte.c. */
#include <string.h>

#include "gte.h"
#include "psyq.h"

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

/* NCLIP of three screen points (x low half, y high half of each argument): the sign of the
   cross product, as MAC0. */
int NormalClip(int sxy0, int sxy1, int sxy2)
{
    GteWriteData(12, sxy0);
    GteWriteData(14, sxy2);
    GteWriteData(13, sxy1);
    GteExecute(0x4B400006); /* nclip */
    return GteReadData(24);
}

/* v2 = v0 x v1 (OP with the shift of 12): the diagonal of the rotation matrix holds v0. */
void OuterProduct12(VECTOR *v0, VECTOR *v1, VECTOR *v2)
{
    int32_t saved0 = GteReadControl(0);
    int32_t saved2 = GteReadControl(2);
    int32_t saved4 = GteReadControl(4);

    GteWriteControl(0, Word(v0, 0));
    GteWriteControl(2, Word(v0, 4));
    GteWriteControl(4, Word(v0, 8));
    GteWriteData(11, Word(v1, 8));
    GteWriteData(9, Word(v1, 0));
    GteWriteData(10, Word(v1, 4));
    GteExecute(0x4B78000C); /* op, sf = 1 */
    StoreWord(v2, 0, GteReadData(25));
    StoreWord(v2, 4, GteReadData(26));
    StoreWord(v2, 8, GteReadData(27));
    GteWriteControl(0, saved0);
    GteWriteControl(2, saved2);
    GteWriteControl(4, saved4);
}

/* The multiplier and shift of the last 1/sqrt the library computed.  VectorNormal reads them even when
   the last InvSquareRoot failed (a zero vector), so they live between calls as the library's two words do. */
static int32_t inverseMultiplier, inverseShift;

/* Leading-zero count of a (GTE LZCS/LZCR) gives the shift; the table at 0x80092214 gives 1/sqrt of the
   top bits.  Returns -1 for 0 and for a count of 0 or 32 (the value's sign bits fill the word). */
static int InvSquareRoot(int a, int32_t *multiplier, int32_t *shift)
{
    int32_t lz, odd, t2, half, t3, t4;

    GteWriteData(30, a);
    lz = GteReadData(31);
    if (lz == 0x20 || lz == 0) {
        return -1;
    }
    odd = lz & 1;
    (void)odd;
    t2 = lz & ~1;
    half = (0x1F - t2) >> 1;
    t3 = t2 - 0x18;
    if (t3 >= 0) {
        t4 = (int32_t)((uint32_t)a << t3);
    } else {
        t4 = a >> (0x18 - t2);
    }
    t4 -= 0x40;
    t4 <<= 1;
    *shift = half;
    *multiplier = GteTable16(0x80092214 + t4);
    return 1;
}

/* v1 = v0 scaled to a length of 4096; returns the squared length of v0. */
int VectorNormal(VECTOR *v0, VECTOR *v1)
{
    uint32_t x = (uint32_t)v0->vx, y = (uint32_t)v0->vy, z = (uint32_t)v0->vz;
    int32_t squared = (int32_t)(x * x + y * y + z * z);

    InvSquareRoot(squared, &inverseMultiplier, &inverseShift);
    v1->vx = (int32_t)((uint32_t)(x * (uint32_t)inverseMultiplier)) >> inverseShift;
    v1->vy = (int32_t)((uint32_t)(y * (uint32_t)inverseMultiplier)) >> inverseShift;
    v1->vz = (int32_t)((uint32_t)(z * (uint32_t)inverseMultiplier)) >> inverseShift;
    return squared;
}
