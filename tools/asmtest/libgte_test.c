/* The platform's libgte (platform/psx/libgte.c, libgte_more.c) against the retail library code: each
   routine the game calls runs both ways on many inputs, edge values included, and the results are
   compared. The retail code's GTE instructions run on gte.c, so this tests the library translations,
   not the GTE. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mips.h"
#include "../../platform/psx/gte.h"
#include "psyq.h"

#define RATAN2 0x80081BE0
#define RCOS 0x8007F024
#define RSIN 0x8007EF1C
#define SQUAREROOT0 0x8007F9E8
#define VECTORNORMAL 0x8007F858
#define OUTERPRODUCT12 0x8008125C
#define ROTTRANS 0x80080EC0
#define ROTTRANSPERS 0x80080E40
#define ROTTRANSPERS3 0x80080E6C
#define MULMATRIX0 0x8007FE30
#define SETROTMATRIX 0x800808D8
#define SETTRANSMATRIX 0x80080968

#define AREA 0x80180000u /* emulated RAM for the arguments */

static uint32_t seed = 12345;
static int bad;

static uint32_t Random(void)
{
    seed = seed * 1103515245u + 12345u;
    return seed >> 1;
}

/* Values of every magnitude, both signs, and the edges. */
static int32_t Value(int bits)
{
    static const int32_t edges[] = {0, 1, -1, 2, -2, 4095, 4096, -4096, 32767, -32768, 65535, 0x7FFFFFFF, (int32_t)0x80000000};
    uint32_t r = Random();

    if (r % 16 == 0) {
        return edges[(r >> 4) % (sizeof(edges) / sizeof(edges[0]))];
    }
    bits = 1 + (int)(Random() % (uint32_t)bits);
    return (int32_t)(Random() % (1u << (bits > 31 ? 31 : bits))) * ((r & 32) ? -1 : 1);
}

static void Mismatch(const char *what, long long input, long long retail, long long port)
{
    static const char *last;
    static int shown;

    if (what != last) {
        last = what;
        shown = 0;
    }
    bad++;
    if (shown++ < 4) {
        printf("%s(%lld): retail %lld, port %lld\n", what, input, retail, port);
    }
}

static void PutVector(uint32_t address, const VECTOR *v)
{
    MipsStore32(address, (uint32_t)v->vx);
    MipsStore32(address + 4, (uint32_t)v->vy);
    MipsStore32(address + 8, (uint32_t)v->vz);
}

static void PutSvector(uint32_t address, const SVECTOR *v)
{
    MipsStore16(address, (uint16_t)v->vx);
    MipsStore16(address + 2, (uint16_t)v->vy);
    MipsStore16(address + 4, (uint16_t)v->vz);
    MipsStore16(address + 6, 0);
}

static void PutMatrix(uint32_t address, const MATRIX *m)
{
    int i, j;

    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) {
            MipsStore16(address + (uint32_t)(i * 6 + j * 2), (uint16_t)m->m[i][j]);
        }
    }
    MipsStore32(address + 20, (uint32_t)m->t[0]);
    MipsStore32(address + 24, (uint32_t)m->t[1]);
    MipsStore32(address + 28, (uint32_t)m->t[2]);
}

static void RandomMatrix(MATRIX *m, int bits)
{
    int i, j;

    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) {
            m->m[i][j] = (short)Value(bits);
        }
        m->t[i] = Value(20);
    }
}

static void TestTrig(void)
{
    int i;

    for (i = -20000; i <= 20000; i++) {
        int32_t r = (int32_t)MipsCall(RSIN, (uint32_t)i, 0, 0, 0, 0, 0);
        int32_t c = (int32_t)MipsCall(RCOS, (uint32_t)i, 0, 0, 0, 0, 0);
        if (r != rsin(i)) {
            Mismatch("rsin", i, r, rsin(i));
        }
        if (c != rcos(i)) {
            Mismatch("rcos", i, c, rcos(i));
        }
    }
}

static void TestRatan2(void)
{
    int i;

    for (i = 0; i < 400000; i++) {
        int32_t y = Value(i < 200000 ? 17 : 31), x = Value(i < 200000 ? 17 : 31);
        int32_t r = (int32_t)MipsCall(RATAN2, (uint32_t)y, (uint32_t)x, 0, 0, 0, 0);
        int32_t p;

        if (mipsTrapped) {
            continue; /* the console divides by zero there */
        }
        p = ratan2(y, x);
        if (r != p) {
            Mismatch("ratan2 y<<32|x", ((long long)y << 32) | (uint32_t)x, r, p);
        }
    }
}

static void TestSquareRoot(void)
{
    int i;

    for (i = 0; i < 400000; i++) {
        int32_t a = Value(31);
        int32_t r = (int32_t)MipsCall(SQUAREROOT0, (uint32_t)a, 0, 0, 0, 0, 0);
        int32_t p = SquareRoot0(a);
        if (r != p) {
            Mismatch("SquareRoot0", a, r, p);
        }
    }
}

static void TestVectors(void)
{
    int i;

    for (i = 0; i < 100000; i++) {
        VECTOR v0, v1, v2, out;
        int32_t r, p;

        /* the game normalises vectors of up to a few times 4096 (cross products of unit vectors) */
        v0.vx = Value(i % 2 ? 13 : 15);
        v0.vy = Value(i % 2 ? 13 : 15);
        v0.vz = Value(i % 2 ? 13 : 15);
        PutVector(AREA, &v0);
        r = (int32_t)MipsCall(VECTORNORMAL, AREA, AREA + 16, 0, 0, 0, 0);
        p = VectorNormal(&v0, &out);
        if (r != p || (int32_t)MipsLoad32(AREA + 16) != out.vx || (int32_t)MipsLoad32(AREA + 20) != out.vy ||
            (int32_t)MipsLoad32(AREA + 24) != out.vz) {
            Mismatch("VectorNormal x", v0.vx, (int32_t)MipsLoad32(AREA + 16), out.vx);
        }

        v0.vx = Value(13);
        v0.vy = Value(13);
        v0.vz = Value(13);
        v1.vx = Value(13);
        v1.vy = Value(13);
        v1.vz = Value(13);
        PutVector(AREA, &v0);
        PutVector(AREA + 16, &v1);
        MipsCall(OUTERPRODUCT12, AREA, AREA + 16, AREA + 32, 0, 0, 0);
        OuterProduct12(&v0, &v1, &v2);
        if ((int32_t)MipsLoad32(AREA + 32) != v2.vx || (int32_t)MipsLoad32(AREA + 36) != v2.vy ||
            (int32_t)MipsLoad32(AREA + 40) != v2.vz) {
            Mismatch("OuterProduct12 x", v0.vx, (int32_t)MipsLoad32(AREA + 32), v2.vx);
        }
    }
}

static void TestTransforms(void)
{
    int i;

    for (i = 0; i < 100000; i++) {
        MATRIX m, a, b, out;
        SVECTOR v[3];
        int sxy[3], p, flag, z;
        int32_t r;
        int k;

        RandomMatrix(&m, i % 3 ? 13 : 16);
        for (k = 0; k < 3; k++) {
            v[k].vx = (short)Value(15);
            v[k].vy = (short)Value(15);
            v[k].vz = (short)Value(15);
            PutSvector(AREA + 64 + (uint32_t)k * 8, &v[k]);
        }
        PutMatrix(AREA, &m);

        /* RotTransPers */
        MipsCall(SETROTMATRIX, AREA, 0, 0, 0, 0, 0);
        MipsCall(SETTRANSMATRIX, AREA, 0, 0, 0, 0, 0);
        {
            uint32_t stack[1] = {AREA + 112};
            r = (int32_t)MipsCall(ROTTRANSPERS, AREA + 64, AREA + 96, AREA + 104, AREA + 108, NULL, 0);
            (void)stack;
        }
        SetRotMatrix(&m);
        SetTransMatrix(&m);
        z = RotTransPers(&v[0], &sxy[0], &p, &flag);
        if (r != (int32_t)z || (int32_t)MipsLoad32(AREA + 96) != (int32_t)sxy[0] ||
            (int32_t)MipsLoad32(AREA + 104) != (int32_t)p || (int32_t)MipsLoad32(AREA + 108) != (int32_t)flag) {
            Mismatch("RotTransPers z/sxy", i, ((long long)r << 32) | MipsLoad32(AREA + 96),
                     ((long long)z << 32) | (uint32_t)sxy[0]);
        }

        /* RotTrans */
        MipsCall(SETROTMATRIX, AREA, 0, 0, 0, 0, 0);
        MipsCall(SETTRANSMATRIX, AREA, 0, 0, 0, 0, 0);
        MipsCall(ROTTRANS, AREA + 64, AREA + 128, AREA + 144, 0, 0, 0);
        {
            VECTOR t;
            SetRotMatrix(&m);
            SetTransMatrix(&m);
            RotTrans(&v[0], &t, &flag);
            if ((int32_t)MipsLoad32(AREA + 128) != t.vx || (int32_t)MipsLoad32(AREA + 132) != t.vy ||
                (int32_t)MipsLoad32(AREA + 136) != t.vz || (int32_t)MipsLoad32(AREA + 144) != (int32_t)flag) {
                Mismatch("RotTrans x", i, (int32_t)MipsLoad32(AREA + 128), t.vx);
            }
        }

        /* MulMatrix0 */
        RandomMatrix(&a, 13);
        RandomMatrix(&b, 13);
        PutMatrix(AREA + 160, &a);
        PutMatrix(AREA + 192, &b);
        MipsCall(MULMATRIX0, AREA + 160, AREA + 192, AREA + 224, 0, 0, 0);
        MulMatrix0(&a, &b, &out);
        {
            int x, y;
            for (x = 0; x < 3; x++) {
                for (y = 0; y < 3; y++) {
                    int16_t rv = (int16_t)MipsLoad16(AREA + 224 + (uint32_t)(x * 6 + y * 2));
                    if (rv != out.m[x][y]) {
                        Mismatch("MulMatrix0 m", x * 3 + y, rv, out.m[x][y]);
                    }
                }
            }
        }
    }
}

int main(int argc, char **argv)
{
    if (argc < 2 || MipsLoadExe(argv[1])) {
        fprintf(stderr, "usage: libgte_test SCUS_943.51\n");
        return 2;
    }
    mipsMaxInstructions = 1000000;
    TestTrig();
    TestRatan2();
    TestSquareRoot();
    TestVectors();
    TestTransforms();
    printf("libgte: %d mismatches\n", bad);
    return bad != 0;
}
