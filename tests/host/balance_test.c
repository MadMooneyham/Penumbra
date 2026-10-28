/* SPDX-License-Identifier: GPL-3.0-only */
/* The cores' share of the drone voices (firmware/src/app/balance.h), fed the FM-1's own BENCH numbers
 * (0.10-beta's load test, % of the block): core 1 does CTRL 4.2 + FX WET 21.2 anyway, core 2 the drone
 * bus 11.1 + VOX 12 + LINE 6.4, and a drone oscillator costs 35.4 / 20 (DRONE 1 + DRONE 2 over the five
 * voices' four oscillators). 0.10 gave core 2 three voices (50.2 against 40); the best is two. */
#include <stdio.h>
#include <stdint.h>
#include "../../firmware/src/app/balance.h"

static int fails;
#define CHECK(c, what) do { if (c) printf("  ok   %s\n", what); else { printf("  FAIL %s\n", what); fails++; } } while (0)

static int bits(uint32_t m)
{
    int n = 0;
    for (; m; m &= m - 1u)
        n++;
    return n;
}
static float worst(float f1, float f2, const float *c, int n, uint32_t m)
{
    float a = f1, b = f2;
    int i;
    for (i = 0; i < n; i++)
        if (m >> i & 1u)
            b += c[i];
        else
            a += c[i];
    return a > b ? a : b;
}

int main(void)
{
    const float f1 = 4.2f + 21.2f, f2 = 11.1f + 12.0f + 6.4f, osc = 35.4f / 20.0f;
    float c[5] = {4 * osc, 4 * osc, 4 * osc, 4 * osc, 4 * osc};
    uint32_t m;
    m = balance_pick(f1, f2, c, 5, 0x07u, 3.0f);   /* from 0.10's share (three voices on core 2) */
    printf("  load test: core 2 gets %d voices, the busier core %.1f %% (0.10: %.1f %%)\n", bits(m),
           (double)worst(f1, f2, c, 5, m), (double)worst(f1, f2, c, 5, 0x07u));
    CHECK(bits(m) == 2, "the FM-1's load test: two voices to core 2, three to core 1");
    CHECK(worst(f1, f2, c, 5, m) < 47.0f, "the busier core under 47 % before the mix (0.10: 50.7)");
    /* drones alone, two-oscillator stacks, VOX and LINE silent: core 2 has only the bus */
    {
        float d[5] = {2 * osc, 2 * osc, 2 * osc, 2 * osc, 2 * osc};
        uint32_t q = balance_pick(f1, 11.1f, d, 5, 0, 3.0f);
        printf("  drones alone: core 2 gets %d voices, the busier core %.1f %%\n", bits(q), (double)worst(f1, 11.1f, d, 5, q));
        CHECK(bits(q) >= 3, "drones alone: core 2 takes most of them (core 1 has the effects)");
    }
    /* hysteresis: a share within the margin of the best is kept */
    {
        uint32_t a = balance_pick(f1, f2, c, 5, 0x03u, 3.0f), b = balance_pick(f1, f2, c, 5, 0x05u, 3.0f);
        CHECK(a == 0x03u && b == 0x05u, "a share as good as the best is kept (no hopping)");
        CHECK(balance_pick(f1, f2, c, 5, 0x1Fu, 3.0f) != 0x1Fu, "a share much worse than the best is left");
    }
    /* silent voices (cost 0) never decide anything */
    {
        float e[5] = {4 * osc, 0, 4 * osc, 0, 0};
        uint32_t q = balance_pick(f1, f2, e, 5, 0, 0.0f);
        CHECK(worst(f1, f2, e, 5, q) <= worst(f1, f2, e, 5, 0) && worst(f1, f2, e, 5, q) <= worst(f1, f2, e, 5, 0x05u),
              "two voices sounding: the share is the best of theirs");
    }
    printf(fails ? "%d FAILED\n" : "all passed\n", fails);
    return fails != 0;
}
