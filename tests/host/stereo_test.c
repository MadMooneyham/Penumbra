/* SPDX-License-Identifier: GPL-3.0-only */
/* X0X stereo: a centred mix must be the mono one exactly, so the stereo paths are held to the mono
 * ones (themselves held to 9W9 and Open303 references elsewhere):
 *  - master_process_st with l == r is master_process, bit for bit, compressor, PUMP, filter, limiter;
 *  - fxbus_process_st with dry l == r and no reverb is fxbus_process on both sides, bit for bit;
 *  - the reverb (spacevb): its mono return is the mean of its two sides; the sides are decorrelated and
 *    as loud; its tail is long; at full SHIMMER and DECAY it stays bounded (the shimmer loop < unity);
 *  - PING: an impulse into the delay comes back on the left one time later, on the right two. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../firmware/src/dsp/master.h"
#define X0X_DRUM_TABLES_DEFINE              /* no drum909.c in this link: the tables are defined here */
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

static master_t m1, m2;
static fxbus_t f1, f2;
static int16_t dl1[88200], dl2[88200];
static int16_t rv1[SV_TOTAL], rv2[SV_TOTAL], ps1[SV_PS_LEN], ps2[SV_PS_LEN];

int main(void)
{
    static float x[N], a[N], l[N], r[N], dry[N], rev[N], dly[N];
    int b, i, same = 1;
    master_init(&m1);
    master_init(&m2);
    {   /* everything on: comp, PUMP, a resonant filter, the limiter */
        static const int P[][2] = {{MST_THRESH, 20}, {MST_RATIO, 90}, {MST_PUMP, 80}, {MST_MODE, 1},
                                   {MST_CUTOFF, 70}, {MST_RESO, 90}, {MST_LIMIT, 1}, {MST_MAKEUP, 100}};
        for (i = 0; i < (int)(sizeof P / sizeof P[0]); i++) {
            master_set(&m1, P[i][0], P[i][1]);
            master_set(&m2, P[i][0], P[i][1]);
        }
    }
    for (b = 0; b < BLOCKS; b++) {
        if (b % 40 == 0) {
            master_key(&m1, 0, 1.0f);
            master_key(&m2, 0, 1.0f);
        }
        for (i = 0; i < N; i++)
            x[i] = 1.6f * rnd() * (float)(b % 7) / 6.0f;
        memcpy(a, x, sizeof x);
        memcpy(l, x, sizeof x);
        memcpy(r, x, sizeof x);
        master_process(&m1, a, N, 0.9f);
        master_process_st(&m2, l, r, N, 0.9f);
        same &= !memcmp(a, l, sizeof a) && !memcmp(a, r, sizeof a);
    }
    check(same, "master: centred stereo == mono, bit for bit (comp, PUMP, filter, limiter)");

    fxbus_init(&f1, dl1, 88200);
    fxbus_init(&f2, dl2, 88200);
    {
        static const int P[][2] = {{FX_DIST, 2}, {FX_DRIVE, 70}, {FX_COMP, 90}, {FX_DL_FDBK, 100}, {FX_DL_LEVEL, 110}};
        for (i = 0; i < (int)(sizeof P / sizeof P[0]); i++) {
            fxbus_set(&f1, P[i][0], P[i][1]);
            fxbus_set(&f2, P[i][0], P[i][1]);
        }
    }
    same = 1;
    for (b = 0; b < BLOCKS; b++) {
        for (i = 0; i < N; i++) {
            dry[i] = 0.5f * rnd();
            dly[i] = (b % 10 == 0) ? 0.5f * rnd() : 0.0f;
            rev[i] = 0.0f;
        }
        fxbus_process(&f1, dry, rev, dly, a, N);
        fxbus_process_st(&f2, dry, dry, rev, dly, l, r, N);
        same &= !memcmp(a, l, sizeof a) && !memcmp(a, r, sizeof a);
    }
    check(same, "fx: centred stereo, no reverb == mono, bit for bit (dist, delay, glue comp)");

    {   /* the reverb: a burst of noise in, the tail out */
        double sl = 0, sr = 0, slr = 0, late = 0;
        float worst = 0.0f;
        fxbus_init(&f1, dl1, 88200);
        fxbus_init(&f2, dl2, 88200);
        fxbus_reverb_buffers(&f1, rv1, ps1);
        fxbus_reverb_buffers(&f2, rv2, ps2);
        for (b = 0; b < BLOCKS; b++) {
            for (i = 0; i < N; i++) {
                rev[i] = b < 4 ? 0.5f * rnd() : 0.0f;
                dry[i] = dly[i] = 0.0f;
            }
            fxbus_process(&f1, dry, rev, dly, a, N);
            fxbus_process_st(&f2, dry, dry, rev, dly, l, r, N);
            for (i = 0; i < N; i++) {
                const float d = fabsf(a[i] - 0.5f * (l[i] + r[i]));
                worst = d > worst ? d : worst;
                sl += (double)l[i] * l[i];
                sr += (double)r[i] * r[i];
                slr += (double)l[i] * r[i];
                if (b >= BLOCKS - 40)                     /* ~2.1 s after the burst */
                    late += (double)l[i] * l[i];
            }
        }
        check(worst < 1e-6f, "reverb: the mono return is the mean of the two sides");
        printf("       L/R correlation %.2f, R/L level %.2f dB, tail at 2 s %.1f dB under the whole\n",
               slr / sqrt(sl * sr), 10 * log10(sr / sl), 10 * log10(sl / (late + 1e-30)));
        check(fabs(slr / sqrt(sl * sr)) < 0.5 && fabs(10 * log10(sr / sl)) < 3.0, "reverb: the sides are decorrelated, as loud");
        check(late > sl * 1e-3, "reverb: a long tail (more than -30 dB of it still there at 2 s)");
    }
    {   /* the shimmer loop at its worst: full SHIMMER, the longest DECAY, noise in for 20 s */
        float peak = 0.0f;
        int finite = 1;
        fxbus_init(&f2, dl2, 88200);
        fxbus_reverb_buffers(&f2, rv2, ps2);
        fxbus_set(&f2, FX_RV_DECAY, 127);
        fxbus_set(&f2, FX_RV_LEVEL, 127);
        fxbus_ext_set(&f2, FXX_RV_SHIMMER, 127);
        fxbus_ext_set(&f2, FXX_RV_SIZE, 127);
        for (b = 0; b < 3445; b++) {                      /* 20 s */
            for (i = 0; i < N; i++) {
                rev[i] = 0.3f * rnd();
                dry[i] = dly[i] = 0.0f;
            }
            fxbus_process_st(&f2, dry, dry, rev, dly, l, r, N);
            for (i = 0; i < N; i++) {
                finite &= isfinite(l[i]) && isfinite(r[i]);
                peak = fabsf(l[i]) > peak ? fabsf(l[i]) : peak;
                peak = fabsf(r[i]) > peak ? fabsf(r[i]) : peak;
            }
        }
        printf("       full shimmer and decay, 20 s of noise: peak %.2f\n", (double)peak);
        check(finite && peak < 4.0f, "reverb: the shimmer loop stays bounded at its worst");
    }
    {   /* PING: one impulse, echoes at T (left) and 2T (right) */
        int first_l = -1, first_r = -1, t;
        fxbus_init(&f2, dl2, 88200);
        fxbus_set(&f2, FX_DL_PING, 1);
        fxbus_set(&f2, FX_DL_FDBK, 0);
        fxbus_set(&f2, FX_DL_HPF, 0);
        fxbus_set_bpm(&f2, 120.0f);
        for (b = 0; b < BLOCKS; b++) {
            for (i = 0; i < N; i++) {
                dry[i] = rev[i] = 0.0f;
                dly[i] = (b == 2 && i == 0) ? 1.0f : 0.0f;
            }
            fxbus_process_st(&f2, dry, dry, rev, dly, l, r, N);
            for (i = 0; i < N; i++) {
                t = b * N + i - 2 * N;
                if (first_l < 0 && fabsf(l[i]) > 0.01f) first_l = t;
                if (first_r < 0 && fabsf(r[i]) > 0.01f) first_r = t;
            }
        }
        printf("       first echo: left at %d, right at %d samples (time %.0f)\n", first_l, first_r,
               (double)(f2.dl_time_ms * 0.001f * 44100.0f));
        check(first_l > 0 && first_r > first_l && abs(first_r - 2 * first_l) < 8, "ping: left at T, right at 2T");
    }
    printf(fails ? "stereo: %d FAILED\n" : "stereo: ok\n", fails);
    return fails != 0;
}
