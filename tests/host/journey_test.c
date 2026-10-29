/* SPDX-License-Identifier: GPL-3.0-only */
/* The journeys (firmware/src/seq/journey.c) alone, against a sink that only counts: every preset's
 * drone is in its scene's key; it holds while a scene plays, and a move to another scene moves only
 * the notes it must, a voice at a time; a move glides there in JR_TRAVEL_S; a drone voice that comes
 * and goes is not latched and swells in every few bars; STOP lets the drone go. */
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
    uint32_t out_key, moves, gap_min, last, swells, releases, latched;
} S;
static uint32_t frames_now, started;
static int in_key(int midi, const scene_t *s)
{
    int pc = ((midi - s->root) % 12 + 12) % 12, i;
    for (i = 0; i < 7; i++)
        if (SCALE[s->scale][i] == pc)
            return 1;
    return 0;
}
static void d_note(void *x, int v, int m)
{
    (void)x;
    (void)v;
    S.out_key += !in_key(m, &J.scene[R.cur]);
    if (started) {
        if (S.moves && frames_now - S.last < S.gap_min)
            S.gap_min = frames_now - S.last;
        S.moves++;
        S.last = frames_now;
    }
}
static void d_latch(void *x, uint32_t m) { (void)x; S.latched = m; }
static void d_rel(void *x) { (void)x; S.releases++; }
static void d_swell(void *x, int v) { (void)x; (void)v; S.swells++; }
static const jr_sink_t SINK = {0, d_note, d_latch, d_rel, d_swell};

#define STEP_FRAMES 7350u                          /* a 16th at 90 BPM */
#define PER_MIN (60u * 6u)                         /* 16ths a minute at 90 BPM */
static uint32_t step_n;
static void play_steps(uint32_t n)                  /* each 16th: the block's glides, then the step */
{
    uint32_t i, f;
    for (i = 0; i < n; i++, step_n++) {
        for (f = 0; f < STEP_FRAMES; f += 256u, frames_now += 256u)
            jr_tick(&R, 256u, &SINK);
        jr_step(&R, step_n, &SINK);
    }
}
static void start(int preset)
{
    memset(&S, 0, sizeof S);
    jr_preset(preset, &J);
    jr_init(&R, &J);
    step_n = 0;
    started = 0;
    jr_start(&R, &SINK);
    started = 1;
    S.gap_min = 0xFFFFFFFFu;
}

int main(void)
{
    int p;
    char t[112];
    for (p = 0; p < JR_PRESETS; p++) {             /* each preset: a few minutes in each scene, round twice */
        int k, n;
        start(p);
        n = J.nscenes;
        play_steps(2u * PER_MIN);
        snprintf(t, sizeof t, "%s: the drone holds while a scene plays", jr_preset_name(p));
        CHECK(S.moves == 0, t);
        for (k = 1; k <= 2 * n; k++) {
            jr_travel(&R, k % n);
            play_steps(2u * PER_MIN);
        }
        printf("  %-12s the drone moved %u notes in %d scene changes\n", jr_preset_name(p), S.moves, 2 * n);
        snprintf(t, sizeof t, "%s: the drone always in its scene's key", jr_preset_name(p));
        CHECK(S.out_key == 0, t);
        snprintf(t, sizeof t, "%s: it moves only with the scene, the notes it must, a voice at a time", jr_preset_name(p));
        CHECK(S.moves <= 3u * (uint32_t)(2 * n) && (S.moves < 2 || S.gap_min >= (uint32_t)(JR_DRONE_GAP * 44100.0f) - 512u), t);
        snprintf(t, sizeof t, "%s: round the scenes, back in the first", jr_preset_name(p));
        CHECK(R.cur == 0 && !R.travelling, t);
        jr_stop(&R, &SINK);
        snprintf(t, sizeof t, "%s: STOP lets the drone go", jr_preset_name(p));
        CHECK(S.releases == 1 && !R.playing, t);
    }
    {   /* a move glides over JR_TRAVEL_S; the key changes at the middle */
        uint32_t steps = (uint32_t)(JR_TRAVEL_S * 44100.0f / (float)STEP_FRAMES);
        start(0);
        jr_travel(&R, 2);
        play_steps(steps / 2u - 2u);
        CHECK(R.cur == 0 && R.travelling, "a move: the key still the first's before the middle");
        play_steps(4u);
        CHECK(R.cur == 2 && R.travelling, "the new key at the middle");
        play_steps(steps / 2u);
        CHECK(R.cur == 2 && !R.travelling, "there in JR_TRAVEL_S");
    }
    {   /* MORPH: by hand, the key past the middle, back is back, all the way is there */
        int k;
        start(0);
        for (k = 0; k < JR_MORPH_STEPS / 2 - 1; k++)
            jr_morph(&R, 1);
        play_steps(16);
        CHECK(R.cur == 0 && R.mamt > 0.4f && R.mto == 1, "MORPH: on the way to the next, the key still this one's");
        jr_morph(&R, 1);
        play_steps(16);
        CHECK(R.cur == 1, "MORPH: past the middle, the next one's key");
        jr_morph(&R, -2);
        play_steps(16);
        CHECK(R.cur == 0 && R.mamt > 0.0f, "MORPH back: this one's again");
        jr_morph(&R, JR_MORPH_STEPS);
        play_steps(16u * 12u);
        CHECK(R.cur == 1 && R.mamt == 0.0f && R.mbase == 1 && S.out_key == 0, "MORPH all the way: there, the drone in its key");
        jr_morph(&R, -1);
        CHECK(R.mto == 0 && R.mbase == 1, "and the next turn heads on (here: back to the first)");
    }
    {   /* a drone voice that comes and goes: not latched, swelled in now and then */
        uint32_t bars;
        jr_preset(0, &J);
        J.dswell = 1u << 4;
        jr_init(&R, &J);
        memset(&S, 0, sizeof S);
        step_n = 0;
        jr_start(&R, &SINK);
        play_steps(10u * PER_MIN);
        bars = step_n / 16u;
        printf("  swell: voice 5 swelled %u times in %u bars\n", S.swells, bars);
        CHECK(!(S.latched >> 4 & 1u) && S.swells * 20u >= bars && S.swells * 3u <= bars,
              "a swelling drone voice: not latched, in every 3 .. 20 bars");
    }
    printf(fails ? "%d FAILED\n" : "all passed\n", fails);
    return fails != 0;
}
