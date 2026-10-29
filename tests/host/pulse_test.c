/* SPDX-License-Identifier: GPL-3.0-only */
/* PULSE (firmware/src/dsp/pulse.c) alone: silent until hit; the kick starts high and falls to its note
 * (A1, 55 Hz, then an octave up with TUNE +12); hits on a sounding kick carry on without a step; it falls
 * to silence and stops costing anything. The bloom, on its own pair: it swells in slowly (nothing like a
 * hit), its sides apart (beating), smooth, then falls to silence and goes idle; the kick's pair stays
 * silent under it.
 *   pulse_test [OUT.wav]   OUT: the test's sound, to listen to */
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "../../firmware/src/dsp/pulse.h"

static pulse_t p;
static int fails;
#define CHECK(c, what) do { if (c) printf("  ok   %s\n", what); else { printf("  FAIL %s\n", what); fails++; } } while (0)
#define MAXN (44100 * 50)
static float L[MAXN], R[MAXN], BL[MAXN], BR[MAXN];
static uint32_t at;

static void run(float seconds)
{
    uint32_t end = at + (uint32_t)(seconds * 44100.0f);
    while (at < end) {
        uint32_t n = end - at < 256u ? end - at : 256u;
        pulse_render(&p, L + at, R + at, BL + at, BR + at, (int)n);
        at += n;
    }
}
static double peak(uint32_t a, uint32_t b)
{
    double m = 0.0;
    for (; a < b; a++)
        m = fmax(m, fmax(fabs(L[a]), fabs(R[a])));
    return m;
}
static double freq(uint32_t a, uint32_t b)          /* from rising zero crossings */
{
    uint32_t i, first = 0, last = 0, n = 0;
    for (i = a + 1; i < b; i++)
        if (L[i - 1] < 0.0f && L[i] >= 0.0f) {
            if (!n)
                first = i;
            last = i;
            n++;
        }
    return n > 1 ? 44100.0 * (double)(n - 1) / (double)(last - first) : 0.0;
}
static double step(uint32_t a, uint32_t b)
{
    double m = 0.0;
    for (a = a ? a : 1; a < b; a++)
        m = fmax(m, fabs(L[a] - L[a - 1]));
    return m;
}

int main(int argc, char **argv)
{
    uint32_t t;
    pulse_init(&p);
    pulse_set(&p, PL_CLICK, 0);                       /* (the pitch measures want the sine alone) */
    run(0.3f);
    CHECK(peak(0, at) == 0.0 && !pulse_active(&p), "silent and idle until hit");
    t = at;
    pulse_hit(&p, PULSE_KICK, 1.0f);
    run(1.5f);
    {
        double f0 = freq(t, t + 441u * 2u), f1 = freq(t + 13230u, t + 26460u);   /* first 20 ms; 0.3 .. 0.6 s */
        printf("  kick: %.0f Hz at its start, %.1f Hz settled, peak %.2f\n", f0, f1, peak(t, at));
        CHECK(fabs(f1 - 55.0) < 1.0, "the kick settles on A1, 55 Hz");
        CHECK(f0 > 1.5 * f1, "it starts well above its note (the pitch drop)");
        CHECK(peak(t, at) > 0.2 && peak(t, at) < 1.0, "it sounds, under full scale");
    }
    pulse_set(&p, PL_TUNE, 24);                       /* TUNE +12: an octave up */
    t = at;
    pulse_hit(&p, PULSE_KICK, 1.0f);
    run(1.0f);
    CHECK(fabs(freq(t + 13230u, t + 26460u) - 110.0) < 2.0, "TUNE +12: 110 Hz");
    pulse_set(&p, PL_TUNE, 12);
    pulse_set(&p, PL_CLICK, 30);
    {   /* eighths at 120 BPM over a sounding kick: no step bigger than a hit from silence makes */
        double ref, fast;
        run(4.0f);
        t = at;
        pulse_hit(&p, PULSE_KICK, 1.0f);
        run(0.25f);
        ref = step(t, at);
        t = at;
        for (int k = 0; k < 8; k++) {
            pulse_hit(&p, PULSE_KICK, 0.9f);
            run(0.25f);
        }
        fast = step(t, at);
        printf("  largest step: a hit from silence %.4f, hits on a sounding kick %.4f\n", ref, fast);
        CHECK(fast < ref * 1.3 + 1e-4, "hits on a sounding kick make no step (no click)");
    }
    run(7.0f);                                        /* (DECAY's default: ~0.66 s a step of 8.7 dB) */
    CHECK(peak(at - 4410u, at) < 3e-4 && !pulse_active(&p), "the kick falls to silence and goes idle");
    t = at;
    pulse_hit(&p, PULSE_BLOOM, 1.0f);
    run(25.0f);
    {
        double top = 0.0, early = 0.0, side = 0.0, jump = 0.0, kick = 0.0;
        uint32_t i, top_at = t;
        for (i = t; i < at; i++) {
            double a = fmax(fabs(BL[i]), fabs(BR[i]));
            if (a > top) {
                top = a;
                top_at = i;
            }
            if (i < t + 13230u)                       /* its first 0.3 s */
                early = fmax(early, a);
            side = fmax(side, fabs(BL[i] - BR[i]));
            if (i > t)
                jump = fmax(jump, fabs(BL[i] - BL[i - 1]));
            kick = fmax(kick, fmax(fabs(L[i]), fabs(R[i])));
        }
        printf("  bloom: peak %.3f at %.1f s, %.3f in its first 0.3 s; largest step %.4f\n", top,
               (double)(top_at - t) / 44100.0, early, jump);
        CHECK(top > 0.05 && top < 0.9, "the bloom sounds, under full scale");
        CHECK(early < 0.1 * top && top_at - t > 44100u, "it swells in slowly (its top after a second, not at once)");
        CHECK(side > 0.1 * top, "its sides apart: it beats");
        CHECK(jump < 0.02, "smooth: no step anywhere");
        CHECK(kick == 0.0, "the kick's pair silent under it");
        CHECK(!pulse_active(&p) && !pulse_blooming(&p), "it falls to silence and goes idle");
    }
    if (argc > 1) {
        FILE *f = fopen(argv[1], "wb");
        uint32_t i, bytes = at * 4u, v;
        uint16_t h16[4] = {1, 2, 4, 16};
        fwrite("RIFF", 1, 4, f);
        v = 36 + bytes;
        fwrite(&v, 4, 1, f);
        fwrite("WAVEfmt ", 1, 8, f);
        v = 16;
        fwrite(&v, 4, 1, f);
        fwrite(h16, 2, 2, f);
        v = 44100;
        fwrite(&v, 4, 1, f);
        v = 44100 * 4;
        fwrite(&v, 4, 1, f);
        fwrite(h16 + 2, 2, 2, f);
        fwrite("data", 1, 4, f);
        fwrite(&bytes, 4, 1, f);
        for (i = 0; i < at; i++) {
            int16_t s[2] = {(int16_t)(L[i] * 32767.0f), (int16_t)(R[i] * 32767.0f)};
            fwrite(s, 2, 2, f);
        }
        fclose(f);
    }
    printf(fails ? "%d FAILED\n" : "all passed\n", fails);
    return fails != 0;
}
