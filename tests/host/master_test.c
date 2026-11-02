/* SPDX-License-Identifier: GPL-3.0-only */
/* The master: open, it is the input delayed by the limiter's look-ahead, exactly; DIP closes a low-pass;
 * the volume glides; the limiter holds a burst under its ceiling without a step in the gain, and moves
 * both sides together. */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "../../firmware/src/dsp/master.h"

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("  FAIL: "); printf(__VA_ARGS__); printf("\n"); } else { printf("  ok   "); printf(__VA_ARGS__); printf("\n"); } } while (0)
#define N 44100

static float bl[N], br[N];
static master_t M;

static void sine(float *x, int n, float hz, float amp)
{
    int i;
    for (i = 0; i < n; i++)
        x[i] = amp * sinf(2.0f * 3.14159265f * hz * (float)i / 44100.0f);
}

static void run(float *l, float *r, int n, float vol)
{
    int i;
    for (i = 0; i < n; i += 256)
        master_process_st(&M, l + i, r + i, n - i < 256 ? n - i : 256, vol);
}

static float peak(const float *x, int n)
{
    float p = 0;
    int i;
    for (i = 0; i < n; i++)
        if (fabsf(x[i]) > p)
            p = fabsf(x[i]);
    return p;
}

int main(void)
{
    static float ref[N];
    int i, same = 1;
    /* 1. open, at unity and under the ceiling: the input, MST_LA samples late, sample for sample */
    master_init(&M);
    sine(ref, N, 220.0f, 0.5f);
    memcpy(bl, ref, sizeof bl);
    memcpy(br, ref, sizeof br);
    run(bl, br, N, 1.0f);
    for (i = MST_LA; i < N; i++)
        same &= bl[i] == ref[i - MST_LA] && br[i] == ref[i - MST_LA];
    CHECK(same, "open at unity: the input %d samples late, exactly", MST_LA);

    /* 2. DIP: 64 leaves 5 kHz alone; 30 (about 900 Hz) takes it far down and keeps 100 Hz */
    {
        float open, shut, low;
        master_init(&M);
        sine(bl, N, 5000.0f, 0.3f);
        memcpy(br, bl, sizeof br);
        run(bl, br, N, 1.0f);
        open = peak(bl + N / 2, N / 2);
        master_init(&M);
        master_dip(&M, 30);
        sine(bl, N, 5000.0f, 0.3f);
        memcpy(br, bl, sizeof br);
        run(bl, br, N, 1.0f);
        shut = peak(bl + N / 2, N / 2);
        master_init(&M);
        master_dip(&M, 30);
        sine(bl, N, 100.0f, 0.3f);
        memcpy(br, bl, sizeof br);
        run(bl, br, N, 1.0f);
        low = peak(bl + N / 2, N / 2);
        CHECK(fabsf(20.0f * log10f(open / 0.3f)) < 0.1f, "DIP open: 5 kHz through (%.2f dB)", 20.0f * log10f(open / 0.3f));
        CHECK(20.0f * log10f(shut / 0.3f) < -20.0f, "DIP 30: 5 kHz down %.1f dB", -20.0f * log10f(shut / 0.3f));
        CHECK(fabsf(20.0f * log10f(low / 0.3f)) < 1.0f, "DIP 30: 100 Hz kept (%.2f dB)", 20.0f * log10f(low / 0.3f));
    }

    /* 3. the volume glides: a jump from 0.2 to 1.0 on DC moves at most 0.25%% of the way a sample */
    {
        float step = 0.0f;
        master_init(&M);
        for (i = 0; i < N; i++)
            bl[i] = br[i] = 0.5f;
        run(bl, br, 4410, 0.2f);
        run(bl + 4410, br + 4410, N - 4410, 1.0f);
        for (i = 4411 + MST_LA; i < N; i++)
            if (fabsf(bl[i] - bl[i - 1]) > step)
                step = fabsf(bl[i] - bl[i - 1]);
        CHECK(step < 0.0025f * 0.4f + 1e-6f && fabsf(bl[N - 1] - 0.5f) < 1e-3f,
              "volume: 0.2 to 1 glides (largest step %.5f), and arrives (%.4f)", step, bl[N - 1]);
    }

    /* 4. the limiter: a 60 Hz burst at 1.6, from its peak at once, comes out under it; the gain eases in
     *    over the look-ahead (it used to halve the distance each sample: a ~1 dB step, a click) */
    {
        float step = 0.0f, prev = 1.0f, pk = 0.0f;
        master_init(&M);
        for (i = 0; i < N; i++) {
            float x = i < 4410 ? 0.3f * sinf(2.0f * 3.14159265f * 60.0f * (float)i / 44100.0f)
                               : 1.6f * cosf(2.0f * 3.14159265f * 60.0f * (float)(i - 4410) / 44100.0f);
            float y = 0.1f * x, d;
            master_process_st(&M, &x, &y, 1, 1.0f);
            d = fabsf(20.0f * log10f(M.gain) - 20.0f * log10f(prev));
            prev = M.gain;
            if (d > step)
                step = d;
            if (i > 4410 + 2 * MST_LA && fabsf(x) > pk)
                pk = fabsf(x);
            bl[i] = x;
            br[i] = y;
        }
        CHECK(pk < 0.9f, "limiter: a 60 Hz burst at 1.6 comes out at %.2f (ceiling 0.8, soft above 0.89)", pk);
        CHECK(step < 0.5f, "limiter: the gain moves at most %.2f dB a sample", step);
        CHECK(fabsf(br[N - 1] - 0.1f * bl[N - 1]) < 1e-4f, "limiter: the quiet side follows the loud one's gain");
    }
    printf("master: %s\n", fails ? "FAIL" : "ok");
    return fails != 0;
}
