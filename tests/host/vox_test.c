/* SPDX-License-Identifier: GPL-3.0-only */
/* VOX (firmware/src/dsp/vox.c) alone: silent until played; a chord sounds, in both channels; a fifth
 * note takes the voice let go longest ago, else the oldest held, and the take is smooth (no step in
 * the waveform bigger than the playing itself makes); released, it falls to silence and stops
 * costing anything; a note's pitch is its own (the spectrum's peak where the note is).
 *   vox_test [OUT.wav]   OUT: the test's sound, to listen to */
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "../../firmware/src/dsp/vox.h"

static vox_t x;
static int fails;
#define CHECK(c, what) do { if (c) printf("  ok   %s\n", what); else { printf("  FAIL %s\n", what); fails++; } } while (0)
#define MAXN (44100 * 30)
static float L[MAXN], R[MAXN];
static uint32_t at;

static void run(float seconds)
{
    uint32_t end = at + (uint32_t)(seconds * 44100.0f);
    while (at < end) {
        uint32_t n = end - at < 256u ? end - at : 256u;
        vox_render(&x, L + at, R + at, (int)n);
        at += n;
    }
}
static double peak(uint32_t a, uint32_t b)
{
    double p = 0.0;
    for (; a < b; a++)
        p = fmax(p, fmax(fabs(L[a]), fabs(R[a])));
    return p;
}
static double step(uint32_t a, uint32_t b)          /* the biggest sample-to-sample move */
{
    double p = 0.0;
    for (a = a ? a : 1; a < b; a++)
        p = fmax(p, fmax(fabs(L[a] - L[a - 1]), fabs(R[a] - R[a - 1])));
    return p;
}

int main(int argc, char **argv)
{
    uint32_t t;
    vox_init(&x);
    run(0.5f);
    CHECK(peak(0, at) == 0.0 && !vox_active(&x), "silent and idle until played");
    t = at;
    vox_note_on(&x, 57, 1.0f);
    vox_note_on(&x, 60, 1.0f);
    vox_note_on(&x, 64, 1.0f);
    vox_note_on(&x, 67, 1.0f);
    run(2.0f);
    {
        double pl = 0.0, pr = 0.0;
        uint32_t i;
        for (i = t; i < at; i++) {
            pl = fmax(pl, fabs(L[i]));
            pr = fmax(pr, fabs(R[i]));
        }
        printf("  chord peak L %.3f R %.3f\n", pl, pr);
        CHECK(pl > 0.05 && pr > 0.05 && pl < 1.0 && pr < 1.0, "a four-note chord sounds in both channels, under full scale");
    }
    vox_note_off(&x, 60);                            /* let go of one: the fifth note takes that voice */
    run(0.3f);
    t = at;
    vox_note_on(&x, 69, 1.0f);
    CHECK(vox_held(&x, 57) && vox_held(&x, 64) && vox_held(&x, 67) && vox_held(&x, 69) && !vox_held(&x, 60),
          "a fifth note takes the voice let go");
    vox_note_on(&x, 72, 1.0f);                       /* all held: the oldest (57) goes */
    CHECK(!vox_held(&x, 57) && vox_held(&x, 72), "all held: the oldest held is taken");
    run(1.0f);
    {
        double before = step(t - 22050u, t), after = step(t, t + 4410u);
        printf("  largest step: playing %.4f, at the takes %.4f\n", before, after);
        CHECK(after < before * 1.6 + 1e-4, "taking a sounding voice makes no step (no click)");
    }
    vox_all_off(&x);
    run(12.0f);
    CHECK(peak(at - 4410u, at) < 1e-4 && !vox_active(&x), "let go: it falls to silence and goes idle");
    {   /* the pitch: one note, open filter, the spectrum's strongest bin within a few Hz of 220 */
        uint32_t i, n = 16384, best = 0;
        double bestv = 0.0;
        vox_set(&x, VX_DETUNE, 0);
        vox_set(&x, VX_CUTOFF, 127);
        t = at;
        vox_note_on(&x, 57, 1.0f);
        run(1.0f);
        for (i = 20; i < 200; i++) {                 /* (DFT bins around the fundamental) */
            double re = 0.0, im = 0.0, f = (double)i * 44100.0 / (double)n;
            uint32_t k;
            for (k = 0; k < n; k++) {
                double a = 2.0 * M_PI * f * (double)k / 44100.0;
                re += L[at - n + k] * cos(a);
                im += L[at - n + k] * sin(a);
            }
            if (re * re + im * im > bestv) {
                bestv = re * re + im * im;
                best = i;
            }
        }
        printf("  strongest at %.1f Hz\n", (double)best * 44100.0 / (double)n);
        CHECK(fabs((double)best * 44100.0 / (double)n - 220.0) < 3.0, "A3 sounds at 220 Hz");
        vox_all_off(&x);
        run(8.0f);
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
