/* SPDX-License-Identifier: GPL-3.0-only */
/* TEXTURE (firmware/src/dsp/texture.c) alone: silent and idle at LEVEL 0; brought in, it glides up (no
 * step) to a quiet wind, about as loud at any COLOUR; its colour is where COLOUR says (more of it high
 * when high); WIDTH apart, the sides differ, at 0 they are one; taken out, it falls away and goes idle.
 *   texture_test [OUT.wav]   OUT: the test's sound, to listen to */
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "../../firmware/src/dsp/texture.h"

static texture_t t;
static int fails;
#define CHECK(c, what) do { if (c) printf("  ok   %s\n", what); else { printf("  FAIL %s\n", what); fails++; } } while (0)
#define MAXN (44100 * 40)
static float L[MAXN], R[MAXN];
static uint32_t at;

static void run(float seconds)
{
    uint32_t end = at + (uint32_t)(seconds * 44100.0f);
    while (at < end) {
        uint32_t n = end - at < 256u ? end - at : 256u, i;
        for (i = 0; i < n; i++)
            L[at + i] = R[at + i] = 0.0f;
        texture_render(&t, L + at, R + at, (int)n);
        at += n;
    }
}
static double rms(uint32_t a, uint32_t b)
{
    double s = 0.0;
    uint32_t i;
    for (i = a; i < b; i++)
        s += (double)L[i] * L[i] + (double)R[i] * R[i];
    return sqrt(s / (2.0 * (double)(b - a)) + 1e-30);
}
static double db(double x) { return 20.0 * log10(x + 1e-30); }
static double hi_share(uint32_t a, uint32_t b)   /* the first difference's share: high content */
{
    double hi = 0.0, all = 0.0;
    uint32_t i;
    for (i = a + 1; i < b; i++) {
        double d = L[i] - L[i - 1];
        hi += d * d * 0.5;
        all += (double)L[i] * L[i];
    }
    return hi / (all + 1e-30);
}
static double corr(uint32_t a, uint32_t b)
{
    double lr = 0.0, ll = 0.0, rr = 0.0;
    uint32_t i;
    for (i = a; i < b; i++) {
        lr += (double)L[i] * R[i];
        ll += (double)L[i] * L[i];
        rr += (double)R[i] * R[i];
    }
    return lr / sqrt(ll * rr + 1e-30);
}
static double step_max(uint32_t a, uint32_t b)
{
    double m = 0.0;
    uint32_t i;
    for (i = a + 1; i < b; i++)
        m = fmax(m, fabs(L[i] - L[i - 1]));
    return m;
}

int main(int argc, char **argv)
{
    uint32_t t0;
    double lo, hi, sl, sh;
    texture_init(&t);
    run(0.5f);
    CHECK(rms(0, at) < 1e-12 && !texture_active(&t), "LEVEL 0: silent, idle");
    texture_set(&t, TX_LEVEL, 100);
    t0 = at;
    run(3.0f);
    printf("  level 100: %.1f dBFS rms, first 10 ms %.1f dB\n", db(rms(at - 44100u, at)), db(rms(t0, t0 + 441u)));
    CHECK(db(rms(at - 44100u, at)) > -40.0 && db(rms(at - 44100u, at)) < -18.0, "brought in: a quiet wind (-40 .. -18 dBFS)");
    CHECK(db(rms(t0, t0 + 441u)) < db(rms(at - 44100u, at)) - 12.0, "it glides in, no step");
    texture_set(&t, TX_SWEEP, 0);
    texture_set(&t, TX_COLOUR, 10);
    run(2.0f);
    lo = db(rms(at - 44100u, at));
    sl = hi_share(at - 44100u, at);
    texture_set(&t, TX_COLOUR, 120);
    run(2.0f);
    hi = db(rms(at - 44100u, at));
    sh = hi_share(at - 44100u, at);
    printf("  colour 10: %.1f dB, high share %.3f; colour 120: %.1f dB, high share %.3f\n", lo, sl, hi, sh);
    CHECK(fabs(lo - hi) < 6.0, "as loud dark as bright (within 6 dB)");
    CHECK(sh > 4.0 * sl, "COLOUR high: brighter");
    texture_set(&t, TX_SWEEP, 70);
    texture_set(&t, TX_COLOUR, 60);
    texture_set(&t, TX_WIDTH, 127);
    run(2.0f);
    hi = corr(at - 44100u, at);
    texture_set(&t, TX_WIDTH, 0);
    run(2.0f);
    lo = corr(at - 44100u, at);
    printf("  correlation: width 127 %.2f, width 0 %.2f\n", hi, lo);
    CHECK(hi < 0.5 && lo > 0.99, "WIDTH: apart, the sides differ; at 0, one wind");
    texture_set(&t, TX_WIDTH, 90);
    run(1.0f);
    sl = step_max(at - 44100u, at);
    texture_set(&t, TX_LEVEL, 0);
    t0 = at;
    run(6.0f);
    printf("  out: %.1f dB after 0.1 s, idle %d after 6 s\n", db(rms(t0 + 4410u, t0 + 8820u)), !texture_active(&t));
    CHECK(step_max(t0, t0 + 4410u) <= sl * 1.2, "taken out: no step");
    CHECK(!texture_active(&t) && rms(at - 4410u, at) < 1e-12, "it falls away and goes idle");
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
