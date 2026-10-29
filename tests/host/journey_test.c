/* SPDX-License-Identifier: GPL-3.0-only */
/* The journeys (firmware/src/seq/journey.c) alone, against a sink that only counts: every preset
 * plays the same music from the same seed; every note it plays is in its scene's key (the lines on the
 * pentatonic when the scene asks); the director goes round the moon and through every scene in turn,
 * and a white key in DRIFT takes it where it says; STOP leaves nothing sounding; GUIDE's held keys are
 * the notes the lines favour (the generator's strong steps keep to the chord, the rest follow the
 * hands); a captured loop plays back what was heard; VOX recorded from the keys
 * plays back what was played. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "../../firmware/src/seq/journey.h"

static int fails;
#define CHECK(c, what) do { if (c) printf("  ok   %s\n", what); else { printf("  FAIL %s\n", what); fails++; } } while (0)

static const uint8_t SCALE[NSCALES][7] = {
    {0, 2, 4, 5, 7, 9, 11}, {0, 2, 3, 5, 7, 9, 10}, {0, 1, 3, 5, 7, 8, 10}, {0, 2, 4, 6, 7, 9, 11},
    {0, 2, 4, 5, 7, 9, 10}, {0, 2, 3, 5, 7, 8, 10}, {0, 2, 3, 5, 7, 8, 11}};

static jrun_t R;
static journey_t J;
static struct {
    uint32_t hash, line, vox, kicks, ticks, out_key, out_penta, drone_out;
    int vox_held[128], line_on;
    int line_seq[4096], nline;
} S;
static int in_key(int midi, const scene_t *s)
{
    int pc = ((midi - s->root) % 12 + 12) % 12, i;
    for (i = 0; i < 7; i++)
        if (SCALE[s->scale][i] == pc)
            return 1;
    return 0;
}
static int in_penta(int midi, const scene_t *s)
{
    static const uint8_t SKIP[NSCALES] = {0x48, 0x22, 0x22, 0x48, 0x48, 0x22, 0x22};
    int pc = ((midi - s->root) % 12 + 12) % 12, i;
    for (i = 0; i < 7; i++)
        if (SCALE[s->scale][i] == pc)
            return !(SKIP[s->scale] >> i & 1u);
    return 0;
}
static void mix(uint32_t a, uint32_t b) { S.hash = (S.hash ^ (a * 2654435761u + b)) * 16777619u; }
static const scene_t *now(void) { return &J.scene[R.cur]; }
static void d_note(void *x, int v, int m)
{
    (void)x;
    mix(1, (uint32_t)(v << 8 | m));
    S.drone_out += !in_key(m, now());
}
static void d_latch(void *x, uint32_t m) { (void)x; mix(2, m); }
static void d_rel(void *x) { (void)x; mix(3, 0); }
static void l_on(void *x, int m, int a, int s)
{
    (void)x;
    mix(4, (uint32_t)(m << 2 | a << 1 | s));
    S.line++;
    S.line_on = 1;
    if (!in_key(m, now()))
        printf("  (line %d out of key: scene %d, root %d, scale %d)\n", m, R.cur, now()->root, now()->scale);
    S.out_key += !in_key(m, now());
    S.out_penta += now()->penta && !in_penta(m, now());
    if (S.nline < 4096)
        S.line_seq[S.nline++] = m;
}
static void l_off(void *x) { (void)x; mix(5, 0); S.line_on = 0; }
static void v_on(void *x, int m, int v)
{
    (void)x;
    mix(6, (uint32_t)(m << 8 | v));
    S.vox++;
    S.vox_held[m & 127]++;
    S.out_key += !in_key(m, now());
}
static void v_off(void *x, int m) { (void)x; mix(7, (uint32_t)m); S.vox_held[m & 127]--; }
static void p_hit(void *x, int w, int v)
{
    (void)x;
    mix(8, (uint32_t)(w << 8 | v));
    if (w)
        S.ticks++;
    else
        S.kicks++;
}
static const jr_sink_t SINK = {0, d_note, d_latch, d_rel, l_on, l_off, v_on, v_off, p_hit};

#define STEP_FRAMES 7350u                          /* a 16th at 90 BPM */
static uint32_t step_n;
static void play_steps(uint32_t n)                  /* each 16th: the block's director, then the step */
{
    uint32_t i;
    for (i = 0; i < n; i++, step_n++) {
        uint32_t f;
        for (f = 0; f < STEP_FRAMES; f += 256u)
            jr_tick(&R, 256u, &SINK);
        jr_step(&R, step_n, STEP_FRAMES, &SINK);
    }
}
static void start(int preset, int mode, int drift)
{
    memset(&S, 0, sizeof S);
    jr_preset(preset, &J);
    J.mode = (uint8_t)mode;
    J.macro[MAC_DRIFT] = (uint8_t)drift;
    jr_init(&R, &J);
    step_n = 0;
    jr_start(&R, &SINK);
}
static int sounding_vox(void)
{
    int k, n = 0;
    for (k = 0; k < 128; k++)
        n += S.vox_held[k] > 0;
    return n;
}

int main(void)
{
    int p;
    char t[96];
    for (p = 0; p < JR_PRESETS; p++) {             /* each preset, 20 minutes in GUIDE (VOX too), DRIFT up */
        uint32_t h, seen = 0, moons = 0;
        int last_moon = -1, order = 1, last_scene = 0;
        start(p, MODE_GUIDE, 100);
        while (step_n < 20u * 60u * 6u) {          /* (6 16ths a second at 90 BPM) */
            play_steps(64);                        /* (7200 = 112.5 x 64: it plays 7232) */
            seen |= 1u << R.cur;
            if (R.moon != last_moon)
                moons++, last_moon = R.moon;
            if (R.cur != last_scene) {
                order &= R.cur == (last_scene + 1) % J.nscenes;
                last_scene = R.cur;
            }
        }
        h = S.hash;
        printf("  %-12s line %u, vox %u, kicks %u, ticks %u, scenes seen %x, moon changes %u\n", jr_preset_name(p), S.line,
               S.vox, S.kicks, S.ticks, seen, moons);
        snprintf(t, sizeof t, "%s: every note in its scene's key (lines, VOX, the drone)", jr_preset_name(p));
        CHECK(S.out_key == 0 && S.drone_out == 0, t);
        snprintf(t, sizeof t, "%s: pentatonic scenes' lines on the pentatonic", jr_preset_name(p));
        CHECK(S.out_penta == 0, t);
        snprintf(t, sizeof t, "%s: plays (lines, VOX), and the moon goes round", jr_preset_name(p));
        CHECK(S.line > 200 && S.vox > 20 && moons >= 8, t);
        snprintf(t, sizeof t, "%s: the director visits every scene, in order", jr_preset_name(p));
        CHECK(seen == (1u << J.nscenes) - 1u && order, t);
        jr_stop(&R, &SINK);
        snprintf(t, sizeof t, "%s: STOP leaves no note sounding", jr_preset_name(p));
        CHECK(!S.line_on && sounding_vox() == 0, t);
        start(p, MODE_GUIDE, 100);
        play_steps(113u * 64u);
        snprintf(t, sizeof t, "%s: the same seed, the same music", jr_preset_name(p));
        CHECK(S.hash == h, t);
    }
    {   /* DRIFT 0: it stays where it is */
        start(0, MODE_PLAY, 0);
        play_steps(6u * 600u);
        CHECK(R.cur == 0 && R.moon == MOON_WAX, "DRIFT 0: ten minutes in the first scene, the moon still");
        CHECK(S.vox == 0, "PLAY: the generator leaves VOX to the keys");
        jr_travel(&R, 2);
        play_steps(6u * 25u);
        CHECK(R.cur == 2 && !R.travelling, "a white key in DRIFT: there in 20 s (frozen, it takes 20 s)");
    }
    {   /* GUIDE: held keys are the notes the lines favour */
        int k, near_free = 0, near_held = 0;
        start(1, MODE_GUIDE, 0);
        J.macro[MAC_DENSITY] = 127;
        play_steps(6u * 120u);
        for (k = 0; k < S.nline; k++)
            near_free += (S.line_seq[k] % 12) == 9;   /* A: degree 4 of D dorian (on its pentatonic) */
        jr_guide(&R, 8, 1);                         /* the white key 8: degree 8 - 4 = 4 over the register's root */
        S.nline = 0;
        play_steps(6u * 120u);
        for (k = 0; k < S.nline; k++)
            near_held += (S.line_seq[k] % 12) == 9;
        printf("  guide: A in %d of the free lines' notes, %d of %d with it held\n", near_free, near_held, S.nline);
        CHECK(near_held * 3 > S.nline && near_held > 2 * near_free, "GUIDE: a held key's note is a third of LINE's, twice as often");
        jr_guide(&R, 8, 0);
    }
    {   /* a capture plays back what was heard */
        int i, n, same = 1;
        start(0, MODE_GUIDE, 0);
        J.macro[MAC_DENSITY] = 127;
        play_steps(6u * 60u);
        jr_capture(&R, 1u << LP_LINE);
        n = J.loop[LP_LINE].len * J.loop[LP_LINE].div;
        play_steps((uint32_t)(n - (int)(step_n % (uint32_t)n)));   /* (to the loop's start) */
        S.nline = 0;
        play_steps((uint32_t)(4 * n));
        for (i = 0; i + S.nline / 4 < S.nline; i++)
            same &= S.line_seq[i] == S.line_seq[i + S.nline / 4];
        CHECK(J.loop[LP_LINE].on && S.nline >= 4 && same, "a captured LINE repeats, round after round");
        jr_release(&R, 1u << LP_LINE);
        CHECK(!J.loop[LP_LINE].on, "released: the generator again");
    }
    {   /* VOX recorded from the keys: a bar, then it plays it */
        static const int NOTES[3] = {57, 64, 60};
        int i, ok = 1, got = 0;
        start(0, MODE_PLAY, 0);
        play_steps(3);
        jr_record(&R);
        play_steps(17u - (step_n & 15u));          /* the next bar line: it starts there */
        CHECK(R.rec == 2, "REC: recording from the bar line");
        for (i = 0; i < 16; i++) {
            if (i % 4 == 0)
                jr_vox_played(&R, NOTES[(i / 4) % 3], 1);
            if (i % 4 == 2)
                jr_vox_played(&R, NOTES[(i / 4) % 3], 0);
            play_steps(1);
        }
        CHECK(R.rec == 0 && J.loop[LP_VOX].on, "after a bar it plays");
        memset(S.vox_held, 0, sizeof S.vox_held);
        S.vox = 0;
        S.nline = 0;
        for (i = 0; i < 16; i++) {
            uint32_t before = S.vox, pos = step_n & 15u;
            play_steps(1);
            if (S.vox != before) {
                got++;
                ok &= pos % 4u == 0;
            }
        }
        printf("  record: %d notes played back\n", got);
        CHECK(ok && got == 4, "what was played, on its steps");
    }
    printf(fails ? "%d FAILED\n" : "all passed\n", fails);
    return fails != 0;
}
