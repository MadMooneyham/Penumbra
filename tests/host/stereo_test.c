/* SPDX-License-Identifier: GPL-3.0-only */
/* The send FX in stereo (fxbus_wet_st):
 *  - the reverb (spacevb): its sides are decorrelated and as loud; its tail is long; at full SHIMMER and
 *    DECAY it stays bounded (the shimmer loop < unity);
 *  - the delay's ping-pong: an impulse comes back on the left one time later, on the right two;
 *  - silent in, silent out: the effects idle on zeros. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../firmware/src/dsp/fxbus.h"

#define N 256
#define BLOCKS 400
static int fails;
static void check(int ok, const char *what)
{
    printf("  %s %s\n", ok ? "ok  " : "FAIL", what);
    fails += !ok;
}
static unsigned rs = 12345u;
static float rnd(void) { rs = rs * 1664525u + 1013904223u; return (float)(int)(rs >> 9) / 4194304.0f - 1.0f; }

static fxbus_t f;
static int16_t dl[88200];
static int16_t rv[SV_TOTAL], ps[SV_PS_LEN];

int main(void)
{
    static float l[N], r[N], rev[N], dly[N];
    int b, i;
    {   /* the reverb: a burst of noise in, the tail out */
        double sl = 0, sr = 0, slr = 0, late = 0;
        fxbus_init(&f, dl, 88200);
        fxbus_reverb_buffers(&f, rv, ps);
        for (b = 0; b < BLOCKS; b++) {
            for (i = 0; i < N; i++) {
                rev[i] = b < 4 ? 0.5f * rnd() : 0.0f;
                dly[i] = 0.0f;
            }
            fxbus_wet_st(&f, rev, dly, l, r, N);
            for (i = 0; i < N; i++) {
                sl += (double)l[i] * l[i];
                sr += (double)r[i] * r[i];
                slr += (double)l[i] * r[i];
                if (b >= BLOCKS - 40)                     /* ~2.1 s after the burst */
                    late += (double)l[i] * l[i];
            }
        }
        printf("       L/R correlation %.2f, R/L level %.2f dB, tail at 2 s %.1f dB under the whole\n",
               slr / sqrt(sl * sr), 10 * log10(sr / sl), 10 * log10(sl / (late + 1e-30)));
        check(fabs(slr / sqrt(sl * sr)) < 0.5 && fabs(10 * log10(sr / sl)) < 3.0, "reverb: the sides are decorrelated, as loud");
        check(late > sl * 1e-3, "reverb: a long tail (more than -30 dB of it still there at 2 s)");
    }
    {   /* the shimmer loop at its worst: full SHIMMER, the longest DECAY, noise in for 20 s */
        float peak = 0.0f;
        int finite = 1;
        fxbus_init(&f, dl, 88200);
        fxbus_reverb_buffers(&f, rv, ps);
        fxbus_set(&f, FX_RV_DECAY, 127);
        fxbus_set(&f, FX_RV_LEVEL, 127);
        fxbus_ext_set(&f, FXX_RV_SHIMMER, 127);
        fxbus_ext_set(&f, FXX_RV_SIZE, 127);
        for (b = 0; b < 3445; b++) {                      /* 20 s */
            for (i = 0; i < N; i++) {
                rev[i] = 0.3f * rnd();
                dly[i] = 0.0f;
            }
            fxbus_wet_st(&f, rev, dly, l, r, N);
            for (i = 0; i < N; i++) {
                finite &= isfinite(l[i]) && isfinite(r[i]);
                peak = fabsf(l[i]) > peak ? fabsf(l[i]) : peak;
                peak = fabsf(r[i]) > peak ? fabsf(r[i]) : peak;
            }
        }
        printf("       full shimmer and decay, 20 s of noise: peak %.2f\n", (double)peak);
        check(finite && peak < 4.0f, "reverb: the shimmer loop stays bounded at its worst");
    }
    {   /* ping-pong: one impulse, echoes at T (left) and 2T (right) */
        int first_l = -1, first_r = -1, t;
        fxbus_init(&f, dl, 88200);
        fxbus_set(&f, FX_DL_FDBK, 0);
        fxbus_set(&f, FX_DL_HPF, 0);
        fxbus_set(&f, FX_DL_WEAR, 0);                     /* the least wow and flutter: the times hold */
        fxbus_set_bpm(&f, 120.0f);
        for (b = 0; b < BLOCKS; b++) {
            for (i = 0; i < N; i++) {
                rev[i] = 0.0f;
                dly[i] = (b == 2 && i == 0) ? 1.0f : 0.0f;
            }
            fxbus_wet_st(&f, rev, dly, l, r, N);
            for (i = 0; i < N; i++) {
                t = b * N + i - 2 * N;
                if (first_l < 0 && fabsf(l[i]) > 0.01f) first_l = t;
                if (first_r < 0 && fabsf(r[i]) > 0.01f) first_r = t;
            }
        }
        printf("       first echo: left at %d, right at %d samples (time %.0f)\n", first_l, first_r,
               (double)(f.dl_time_ms * 0.001f * 44100.0f));
        check(first_l > 0 && first_r > first_l && abs(first_r - 2 * first_l) < 16, "ping-pong: left at T, right at 2T");
    }
    {   /* silence: nothing comes out of an empty line and an empty reverb */
        int quiet = 1;
        fxbus_init(&f, dl, 88200);
        fxbus_reverb_buffers(&f, rv, ps);
        for (b = 0; b < 40; b++) {
            memset(rev, 0, sizeof rev);
            memset(dly, 0, sizeof dly);
            fxbus_wet_st(&f, rev, dly, l, r, N);
            for (i = 0; i < N; i++)
                quiet &= l[i] == 0.0f && r[i] == 0.0f;
        }
        check(quiet, "silence in, silence out");
    }
    printf(fails ? "stereo: %d FAILED\n" : "stereo: ok\n", fails);
    return fails != 0;
}
