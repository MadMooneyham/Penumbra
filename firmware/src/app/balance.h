/* SPDX-License-Identifier: GPL-3.0-only */
/* Sharing work between the two cores (engine.c's SPLIT): of every way to give the second core a set
 * of items (bit i of the mask: item i is the second core's), the one whose busier core is least busy,
 * from what each core does anyway and what each item costs. The share in use is kept unless another
 * is better by more than `hyst`, so items do not hop between cores on noise. Pure: the engine feeds it
 * its own timings; tests/host/balance_test.c feeds it the FM-1's. */
#pragma once
#include <stdint.h>

static inline uint32_t balance_pick(float fix1, float fix2, const float *cost, int n, uint32_t keep, float hyst)
{
    uint32_t m, best = 0, all = (1u << n) - 1u;
    float best_worst = 1e30f, keep_worst = 1e30f;
    int i;
    for (m = 0; m <= all; m++) {
        float c1 = fix1, c2 = fix2, worst;
        for (i = 0; i < n; i++)
            if (m >> i & 1u)
                c2 += cost[i];
            else
                c1 += cost[i];
        worst = c1 > c2 ? c1 : c2;
        if (m == (keep & all))
            keep_worst = worst;
        if (worst < best_worst) {
            best_worst = worst;
            best = m;
        }
    }
    return keep_worst > best_worst + hyst ? best : keep & all;
}
