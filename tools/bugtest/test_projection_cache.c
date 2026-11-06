/* Compare repeated resolutions with a fresh search while packets, bindings and
   projections change. A cached record index must still read its current value. */
#ifndef GTE_SOURCE
#define GTE_SOURCE "../../platform/psx/gte.c"
#endif
#include GTE_SOURCE
#include <assert.h>

static uint32_t seed = 0xc012a9b3;
static uint32_t Next(void)
{
    seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
    return seed;
}

static void Compare(const uint32_t *xy, const void *const *addresses, int count)
{
    int32_t chosen[GTE_RESOLVE_MAX];
    GtePrecise expected[GTE_RESOLVE_MAX], actual[GTE_RESOLVE_MAX];
    memset(expected, 0x5a, sizeof(expected));
    memset(actual, 0x5a, sizeof(actual));
    unsigned mask = ResolvePrecise(xy, addresses, count, chosen);
    int bound = resolvedBound;
    for (int i = 0; i < count; i++) if (mask & (1u << i)) expected[i] = records[chosen[i]].value;
    for (int repeat = 0; repeat < 3; repeat++) {
        assert(GteResolvePrecise(xy, addresses, count, actual) == mask);
        assert(GteResolvedBound() == bound);
        assert(!memcmp(expected, actual, sizeof(expected)));
    }
}

int main(void)
{
    uint32_t packets[128] = {0}, handles[128] = {0};
    for (int step = 0; step < 20000; step++) {
        int slot = Next() % 128;
        switch (Next() % 8) {
        case 0:
            packets[slot] = Next() % 24;
            handles[slot] = GteRecordProjection(packets[slot], Next() % 320, Next() % 240, 100 + Next() % 900, 1.0f);
            GteBindPrecise(&packets[slot], handles[slot]);
            break;
        case 1: GteBindFlat(&packets[slot]); break;
        case 2: GteBindPrecise(&packets[slot], 0); break;
        case 3: GteBindPrecise(&packets[slot], handles[Next() % 128]); break;
        case 4: packets[slot] = Next() % 24; break;
        case 5: GteRecordProjection(Next() % 24, Next() % 320, Next() % 240, 100, 0.5f); break;
        case 6: GteRecordView(handles[slot], Next() % 1000, Next() % 1000, Next() % 1000); break;
        case 7: GteRecordAnchor(handles[slot], Next(), 10, 20, 30); break;
        }
        for (int probe = 0; probe < 8; probe++) {
            uint32_t xy[GTE_RESOLVE_MAX];
            const void *addresses[GTE_RESOLVE_MAX];
            int count = Next() % GTE_RESOLVE_MAX + 1;
            for (int k = 0; k < count; k++) {
                int at = (probe * 16 + k) % 128;
                xy[k] = packets[at]; addresses[k] = &packets[at];
            }
            Compare(xy, addresses, count);
            Compare(xy, NULL, count);
        }
        if (step % 101 == 100) GteClearPrecise();
    }
    preciseFrame = 0xfffffffeu;
    GteClearPrecise();
    assert(preciseFrame == 1);
    assert(GteResolvePrecise(NULL, NULL, 0, NULL) == 0 && !GteResolvedBound());
    puts("projection cache matches fresh searches through packet, binding and frame changes: OK");
}
