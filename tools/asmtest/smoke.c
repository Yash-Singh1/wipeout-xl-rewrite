/* Smoke test of the interpreter: retail library routines against the platform's translations. */
#include <stdio.h>
#include <stdlib.h>
#include "mips.h"
#include "../../platform/psx/gte.h"
#include "psyq.h"

int main(int argc, char **argv)
{
    int i, bad = 0;

    if (MipsLoadExe(argv[1])) { fprintf(stderr, "cannot load\n"); return 2; }
    for (i = -2000; i < 2000000; i += 1237) {
        int a = i * 977 + i % 13;
        uint32_t r = MipsCall(0x8007F9E8 /*SquareRoot0*/, (uint32_t)(a < 0 ? -a : a), 0, 0, 0, 0, 0);
        int e = SquareRoot0(a < 0 ? -a : a);
        if ((int)r != e) { if (bad++ < 5) printf("SquareRoot0(%d): retail %d, platform %d\n", a, (int)r, e); }
    }
    for (i = 0; i < 4096; i += 7) {
        uint32_t r = MipsCall(0x8007EF1C /*rsin*/, (uint32_t)i, 0, 0, 0, 0, 0);
        int e = rsin(i);
        if ((int)r != e) { if (bad++ < 5) printf("rsin(%d): retail %d, platform %d\n", i, (int)r, e); }
    }
    printf("%d mismatches\n", bad);
    return bad != 0;
}
