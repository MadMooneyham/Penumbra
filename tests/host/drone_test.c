/* SPDX-License-Identifier: GPL-3.0-only */
/* The drone (firmware/src/dsp/drone.c) rendered in parts as the engine renders it on two cores:
 * each block planned, its voices split between two buffers (a different split every block) and
 * summed, and the bus (filter, then SWARM) run on that sum a block later, after the next block has
 * been planned, as the second core runs it. Against drone_render, a block earlier: the two differ
 * only by float rounding in the order the voices are added. Through the overload guard's fade and
 * back, and voices let go and swelled.
 *   drone_test [OUT.f32]   OUT: drone_render's 20 s, stereo float (to compare with another build) */
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "../../firmware/src/dsp/drone.h"

static drone_t a, b;
static float swa[2][SWARM_LEN], swb[2][SWARM_LEN];
static int fails;
#define CHECK(c, what) do { if (c) printf("  ok   %s\n", what); else { printf("  FAIL %s\n", what); fails++; } } while (0)

static void setup(drone_t *d, float sw[2][SWARM_LEN])
{
    int v;
    drone_init(d);
    drone_swarm_buffer(d, sw[0], sw[1]);
    for (v = 0; v < DRONE_VOICES; v++) {
        drone_set(d, v, DN_STACK, v & 3);           /* every stack, two to four oscillators */
        drone_set(d, v, DN_NOTE, 24 + 5 * v);
        drone_trigger(d, v, 1.0f);
    }
    drone_set(d, DRONE_KIT, DB_SWARM, 127);
    drone_set(d, DRONE_KIT, DB_LFOCUT, 80);
    drone_set(d, DRONE_KIT, DB_SHCUT, 60);
    drone_set(d, DRONE_KIT, DB_WIND, 40);           /* the bus's own noise, too */
}

int main(int argc, char **argv)
{
    static float la[2][256], ra[2][256], bl[2][256], br[2][256], l2[256], r2[256];
    FILE *out = argc > 1 ? fopen(argv[1], "wb") : 0;
    double worst = 0.0, peak = 0.0;
    uint32_t blk, i, nb = 20u * 44100u / 256u, n_prev = 0;
    int plan_prev = 0;
    setup(&a, swa);
    setup(&b, swb);
    for (blk = 0; blk < nb; blk++) {
        uint32_t n = 256u - (blk % 3u) * 37u, p = blk & 1u;   /* blocks of varying length, as stretches */
        uint32_t mask = 1u + blk % ((1u << DRONE_VOICES) - 2u);   /* a different split every block */
        int pl;
        if (blk == nb / 4)
            drone_set_lite(&a, 1), drone_set_lite(&b, 1);
        if (blk == nb / 2)
            drone_set_lite(&a, 0), drone_set_lite(&b, 0);
        if (blk == nb * 3 / 4)                      /* two voices let go */
            for (i = 0; i < 2; i++)
                drone_trigger(&a, (int)i, 1.0f), drone_trigger(&b, (int)i, 1.0f);
        memset(la[p], 0, sizeof la[p]);
        memset(ra[p], 0, sizeof ra[p]);
        drone_render(&a, la[p], ra[p], (int)n);
        /* the engine's way: plan this block, render its two shares, then the last block's bus */
        pl = drone_plan(&b, (int)n);
        memset(bl[p], 0, sizeof bl[p]);
        memset(br[p], 0, sizeof br[p]);
        memset(l2, 0, sizeof l2);
        memset(r2, 0, sizeof r2);
        drone_voices(&b, mask, bl[p], br[p]);
        drone_voices(&b, ~mask & ((1u << DRONE_VOICES) - 1u), l2, r2);
        if (blk) {
            drone_bus_filter(&b, plan_prev, bl[p ^ 1u], br[p ^ 1u]);
            drone_bus_swarm(&b, plan_prev, bl[p ^ 1u], br[p ^ 1u]);
            for (i = 0; i < n_prev; i++) {          /* the last block, both ways */
                double e = fmax(fabs((double)la[p ^ 1u][i] - bl[p ^ 1u][i]), fabs((double)ra[p ^ 1u][i] - br[p ^ 1u][i]));
                worst = fmax(worst, e);
                peak = fmax(peak, fmax(fabs((double)la[p ^ 1u][i]), fabs((double)ra[p ^ 1u][i])));
            }
        }
        for (i = 0; i < n; i++) {
            bl[p][i] += l2[i];
            br[p][i] += r2[i];
            if (out) {
                float s[2] = {la[p][i], ra[p][i]};
                fwrite(s, sizeof s, 1, out);
            }
        }
        plan_prev = pl;
        n_prev = n;
    }
    if (out)
        fclose(out);
    printf("  peak %.3f, worst difference %.2e (%.1f dB under the peak)\n", peak, worst,
           20.0 * log10(peak / (worst > 1e-30 ? worst : 1e-30)));
    CHECK(peak > 0.05, "the drone sounds");
    CHECK(worst < peak * 1e-5, "two shares, the bus a block later = one render (within rounding, -100 dB)");
    printf(fails ? "%d FAILED\n" : "all passed\n", fails);
    return fails != 0;
}
