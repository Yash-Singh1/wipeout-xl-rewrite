/* Frozen exhaustive resolver for differential tests of optimized lookup. */
static unsigned ReferenceResolvePrecise(const uint32_t *xy, const void *const *addresses, int count, int32_t *chosen)
{
    int32_t candidates[GTE_RESOLVE_MAX][PRECISE_CANDIDATES];
    int found[GTE_RESOLVE_MAX], i, j, anchors = 0, any = 0, bound = 0;
    int32_t sorted[GTE_RESOLVE_MAX], middle;
    unsigned mask = 0;

    resolvedBound = 0;
    for (i = 0; addresses && i < count; i++) {
        if (BoundFlat(addresses[i])) {
            return 0;
        }
    }
    for (i = 0; i < count; i++) {
        int32_t record = addresses ? BoundRecord(addresses[i], xy[i]) : -1;

        if (record >= 0) {
            /* bound by the renderer that built the packet: exact */
            found[i] = 1;
            candidates[i][0] = record;
            chosen[i] = record;
            anchors++;
            bound++;
            continue;
        }
        found[i] = Candidates(xy[i], candidates[i]);
        chosen[i] = found[i] ? candidates[i][0] : -1;
        anchors += found[i] == 1;
        any |= found[i] > 1;
    }
    if (any) {
        if (anchors) {
            /* each ambiguous vertex: the projection nearest to an unambiguous one */
            for (i = 0; i < count; i++) {
                int32_t best = -1, bestDistance = 0x7FFFFFFF;
                int k;
                if (found[i] < 2) {
                    continue;
                }
                for (k = 0; k < found[i]; k++) {
                    for (j = 0; j < count; j++) {
                        if (found[j] == 1 && Distance(candidates[i][k], chosen[j]) < bestDistance) {
                            bestDistance = Distance(candidates[i][k], chosen[j]);
                            best = candidates[i][k];
                        }
                    }
                }
                chosen[i] = best;
            }
        } else {
            /* none unambiguous: around each candidate of the first ambiguous
               vertex, the nearest of the others; the tightest group wins */
            int first = 0, k, bestSpread = 0x7FFFFFFF;
            int32_t group[GTE_RESOLVE_MAX];

            while (found[first] < 2) {
                first++;
            }
            for (k = 0; k < found[first]; k++) {
                int32_t low = candidates[first][k], high = low;
                for (i = 0; i < count; i++) {
                    int32_t best = -1;
                    int m;
                    for (m = 0; m < found[i]; m++) {
                        if (best < 0 || Distance(candidates[i][m], candidates[first][k]) <
                                            Distance(best, candidates[first][k])) {
                            best = candidates[i][m];
                        }
                    }
                    group[i] = best;
                    if (best >= 0) {
                        low = best < low ? best : low;
                        high = best > high ? best : high;
                    }
                }
                if (high - low < bestSpread) {
                    bestSpread = high - low;
                    memcpy(chosen, group, sizeof(group));
                }
            }
        }
    }

    /* a projection far from the primitive's others is a stranger at the same pixel */
    j = 0;
    for (i = 0; i < count; i++) {
        if (chosen[i] >= 0) {
            int k = j++;
            while (k > 0 && sorted[k - 1] > chosen[i]) {
                sorted[k] = sorted[k - 1];
                k--;
            }
            sorted[k] = chosen[i];
        }
    }
    if (!j) {
        return 0;
    }
    if (!bound && j < count) {
        return 0; /* only some corners of an unbound primitive match: by chance (a 2D shape) */
    }
    resolvedBound = bound == count;
    middle = sorted[(j - 1) / 2];
    for (i = 0; i < count; i++) {
        if (chosen[i] >= 0 && (j == 1 || Distance(chosen[i], middle) <= PRECISE_SPREAD)) {
            mask |= 1u << i;
        }
    }
    return mask;
}

