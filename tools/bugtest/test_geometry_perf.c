/* Standalone CPU resolver benchmark. GTE_SOURCE can name an older implementation.
   Force cold resolution generations to model a newly built frame, without
   including fixture construction or GPU synchronization in the measurements. */
#ifndef GTE_SOURCE
#define GTE_SOURCE "../../platform/psx/gte.c"
#endif
#include GTE_SOURCE
#include <assert.h>
#include <stdio.h>
#include <time.h>

enum { PACKETS = 1024, SAMPLES = 128 };
static uint32_t words[PACKETS][4], handles[PACKETS][4];
static double Now(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1e9 + t.tv_nsec;
}
int main(void)
{
    puts("workload,sample,ns_per_primitive");
    for (int mode = 0; mode < 3; mode++) {
        GteClearPrecise();
        for (int p = 0; p < PACKETS; p++) for (int k = 0; k < 4; k++) {
            words[p][k] = (p % 64) * 4 + k;
            handles[p][k] = GteRecordProjection(words[p][k], p, k, 100 + p, 1);
        }
        for (int p = 0; p < PACKETS; p++) for (int k = 0; k < 4; k++)
            GteBindPrecise(&words[p][k], mode == 0 || (mode == 2 && k == 0) ? handles[p][k] : 0);
        volatile unsigned observed = 0;
        for (int sample = -8; sample < SAMPLES; sample++) {
            /* New packets/bindings invalidate cached choices at frame start. */
            bindingRevision++;
            double start = Now();
            for (int p = 0; p < PACKETS; p++) {
                const void *at[] = {&words[p][0], &words[p][1], &words[p][2], &words[p][3]};
                GtePrecise out[4];
                unsigned mask = GteResolvePrecise(words[p], at, 4, out);
                assert(mask & 1);
                observed += mask;
                if (out[0].z < 0) abort();
            }
            double elapsed = Now() - start;
            if (sample >= 0) printf("%s,%d,%.3f\n", mode == 0 ? "bound" : mode == 1 ? "ambiguous" : "mixed",
                                    sample, elapsed / PACKETS);
        }
        assert(observed != 0);
    }
}
