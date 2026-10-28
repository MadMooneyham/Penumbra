/* SPDX-License-Identifier: GPL-3.0-only */
/* The drone (firmware/src/dsp/drone.c) rendered in parts as the engine renders it on two cores: its
 * voices split between two buffers (any split), summed, then the bus, against drone_render. The sums
 * differ only by float rounding in the order the voices are added. Then the overload guard's fade and
 * a swell, through the same comparison.
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
}

int main(int argc, char **argv)
{
    static float la[256], ra[256], l1[256], r1[256], l2[256], r2[256];
    FILE *out = argc > 1 ? fopen(argv[1], "wb") : 0;
    double worst = 0.0, peak = 0.0;
    uint32_t blk, i, nb = 20u * 44100u / 256u;
    setup(&a, swa);
    setup(&b, swb);
    for (blk = 0; blk < nb; blk++) {
        uint32_t n = 256u - (blk % 3u) * 37u;      /* blocks of varying length, as stretches */
        uint32_t mask = 1u + blk % 62u;             /* a different split every block */
        if (blk == nb / 4)
            drone_set_lite(&a, 1), drone_set_lite(&b, 1);
        if (blk == nb / 2)
            drone_set_lite(&a, 0), drone_set_lite(&b, 0);
        if (blk == nb * 3 / 4)                      /* two voices let go, one swells */
            for (i = 0; i < 2; i++)
                drone_trigger(&a, (int)i, 1.0f), drone_trigger(&b, (int)i, 1.0f);
        memset(la, 0, sizeof la);
        memset(ra, 0, sizeof ra);
        drone_render(&a, la, ra, (int)n);
        memset(l1, 0, sizeof l1);
        memset(r1, 0, sizeof r1);
        memset(l2, 0, sizeof l2);
        memset(r2, 0, sizeof r2);
        drone_plan(&b, (int)n);
        drone_voices(&b, mask, l1, r1);
        drone_voices(&b, ~mask & ((1u << DRONE_VOICES) - 1u), l2, r2);
        for (i = 0; i < n; i++) {
            l1[i] += l2[i];
            r1[i] += r2[i];
        }
        drone_bus(&b, l1, r1);
        for (i = 0; i < n; i++) {
            double e = fmax(fabs((double)la[i] - l1[i]), fabs((double)ra[i] - r1[i]));
            worst = fmax(worst, e);
            peak = fmax(peak, fmax(fabs((double)la[i]), fabs((double)ra[i])));
            if (out) {
                float s[2] = {la[i], ra[i]};
                fwrite(s, sizeof s, 1, out);
            }
        }
    }
    if (out)
        fclose(out);
    printf("  peak %.3f, worst split difference %.2e (%.1f dB under the peak)\n", peak, worst,
           20.0 * log10(peak / (worst > 1e-30 ? worst : 1e-30)));
    CHECK(peak > 0.05, "the drone sounds");
    CHECK(worst < peak * 1e-5, "split across two buffers = one render (within rounding, -100 dB)");
    printf(fails ? "%d FAILED\n" : "all passed\n", fails);
    return fails != 0;
}
