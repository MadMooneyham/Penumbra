/* SPDX-License-Identifier: GPL-3.0-only */
/* The journeys (firmware/src/seq/journey.c) alone, against a sink that only counts: every preset
 * plays the same music from the same seed; every note it plays is in its scene's key (VOX's phrases on
 * the pentatonic when the scene asks); VOX is sparse, long notes with more silence than sound; the
 * director goes round the moon and through every scene in turn, and a white key in DRIFT takes it where
 * it says; STOP leaves nothing sounding; in PLAY the generator gives way while VOX is played by hand;
 * GUIDE's held keys are the notes VOX favours; a captured phrase plays back what was heard; VOX recorded
 * from the keys plays back what was played. */
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
    uint32_t hash, vox, kicks, ticks, out_key, out_penta, drone_out;
    uint32_t dmoves, dgap_min, dlast;               /* the drone's notes after the start, the least time between */
    int vox_held[128];
    int seq[4096], nseq;
} S;
static uint32_t frames_now, started, swells, latched;
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
    if (started) {
        if (S.dmoves && frames_now - S.dlast < S.dgap_min)
            S.dgap_min = frames_now - S.dlast;
        S.dmoves++;
        S.dlast = frames_now;
    }
}
static void d_latch(void *x, uint32_t m) { (void)x; mix(2, m); latched = m; }
static void d_rel(void *x) { (void)x; mix(3, 0); }
static void d_swell(void *x, int v) { (void)x; mix(4, (uint32_t)v); swells++; }
static void v_on(void *x, int m, int v)
{
    (void)x;
    mix(6, (uint32_t)(m << 8 | v));
    S.vox++;
    S.vox_held[m & 127]++;
    S.out_key += !in_key(m, now());
    S.out_penta += now()->penta && J.loop[LP_VOX].on != LOOP_RECORDED && !in_penta(m, now());
    if (S.nseq < 4096)
        S.seq[S.nseq++] = m;
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
static const jr_sink_t SINK = {0, d_note, d_latch, d_rel, d_swell, v_on, v_off, p_hit};

static int sounding_vox(void)
{
    int k, n = 0;
    for (k = 0; k < 128; k++)
        n += S.vox_held[k] > 0;
    return n;
}

#define STEP_FRAMES 7350u                          /* a 16th at 90 BPM */
#define PER_MIN (60u * 6u)                         /* 16ths a minute at 90 BPM */
static uint32_t step_n, quiet_steps, played_steps;
static void play_steps(uint32_t n)                  /* each 16th: the block's director, then the step */
{
    uint32_t i;
    for (i = 0; i < n; i++, step_n++) {
        uint32_t f;
        for (f = 0; f < STEP_FRAMES; f += 256u, frames_now += 256u)
            jr_tick(&R, 256u, &SINK);
        jr_step(&R, step_n, STEP_FRAMES, &SINK);
        played_steps++;
        quiet_steps += sounding_vox() == 0;
    }
}
static void start(int preset, int mode, int drift)
{
    memset(&S, 0, sizeof S);
    jr_preset(preset, &J);
    J.mode = (uint8_t)mode;
    J.macro[MAC_DRIFT] = (uint8_t)drift;
    jr_init(&R, &J);
    step_n = quiet_steps = played_steps = 0;
    started = 0;
    jr_start(&R, &SINK);
    started = 1;
    S.dgap_min = 0xFFFFFFFFu;
}

int main(void)
{
    int p;
    char t[112];
    for (p = 0; p < JR_PRESETS; p++) {             /* each preset, 20 minutes in GUIDE, DRIFT up */
        uint32_t h, seen = 0, moons = 0, travels = 0;
        int last_moon = -1, order = 1, last_scene = 0;
        float per_min, quiet;
        start(p, MODE_GUIDE, 100);
        while (step_n < 20u * PER_MIN) {
            play_steps(64);                        /* (7200 = 112.5 x 64: it plays 7232) */
            seen |= 1u << R.cur;
            if (R.moon != last_moon)
                moons++, last_moon = R.moon;
            if (R.cur != last_scene) {
                order &= R.cur == (last_scene + 1) % J.nscenes;
                last_scene = R.cur;
                travels++;
            }
        }
        h = S.hash;
        per_min = (float)S.vox / 20.0f;
        quiet = (float)quiet_steps / (float)played_steps;
        printf("  %-12s VOX %.1f notes a minute, silent %.0f%% of the time; kicks %u, ticks %u, scenes %x, moons %u;"
               " the drone moved %u times in %u scene changes\n",
               jr_preset_name(p), (double)per_min, (double)(quiet * 100.0f), S.kicks, S.ticks, seen, moons, S.dmoves, travels);
        snprintf(t, sizeof t, "%s: the drone holds through the chords, moves only with the scene, a voice at a time", jr_preset_name(p));
        CHECK(travels >= 2 && S.dmoves <= 3u * travels &&
                  (S.dmoves < 2 || S.dgap_min >= (uint32_t)(JR_DRONE_GAP * 44100.0f) - 512u), t);
        snprintf(t, sizeof t, "%s: every note in its scene's key (VOX, the drone)", jr_preset_name(p));
        CHECK(S.out_key == 0 && S.drone_out == 0, t);
        snprintf(t, sizeof t, "%s: pentatonic scenes' phrases on the pentatonic", jr_preset_name(p));
        CHECK(S.out_penta == 0, t);
        snprintf(t, sizeof t, "%s: VOX sparse (1 .. 10 notes a minute), silent a third of the time or more", jr_preset_name(p));
        CHECK(per_min >= 1.0f && per_min <= 10.0f && quiet >= 0.33f, t);
        snprintf(t, sizeof t, "%s: the moon goes round, the director visits every scene in order", jr_preset_name(p));
        CHECK(moons >= 8 && seen == (1u << J.nscenes) - 1u && order, t);
        jr_stop(&R, &SINK);
        snprintf(t, sizeof t, "%s: STOP leaves no note sounding", jr_preset_name(p));
        CHECK(sounding_vox() == 0, t);
        start(p, MODE_GUIDE, 100);
        play_steps(113u * 64u);
        snprintf(t, sizeof t, "%s: the same seed, the same music", jr_preset_name(p));
        CHECK(S.hash == h, t);
    }
    {   /* a drone voice that comes and goes: not latched, swelled in now and then, more as the moon fills */
        uint32_t bars;
        jr_preset(0, &J);
        J.dswell = 1u << 4;
        jr_init(&R, &J);
        swells = 0;
        step_n = 0;
        jr_start(&R, &SINK);
        play_steps(10u * PER_MIN);
        bars = step_n / 16u;
        printf("  swell: voice 5 swelled %u times in %u bars\n", swells, bars);
        CHECK(!(latched >> 4 & 1u) && swells * 20u >= bars && swells * 3u <= bars,
              "a swelling drone voice: not latched, in every 3 .. 20 bars");
    }
    {   /* DRIFT 0: it stays where it is; PLAY: the hands come first */
        uint32_t before;
        start(0, MODE_PLAY, 0);
        J.macro[MAC_DENSITY] = 127;
        play_steps(10u * PER_MIN);
        CHECK(R.cur == 0 && R.moon == MOON_WAX, "DRIFT 0: ten minutes in the first scene, the moon still");
        CHECK(S.vox > 10, "PLAY: the generator plays VOX while the keys are still");
        jr_vox_played(&R, 69, 1);                  /* a key: it waits four bars */
        before = S.vox;
        play_steps(JR_HANDS_STEPS - 1u);
        jr_vox_played(&R, 69, 0);
        CHECK(S.vox == before, "PLAY: a played note, and the generator waits");
        play_steps(3u * PER_MIN);
        CHECK(S.vox > before, "then it comes back");
        jr_travel(&R, 2);
        play_steps(6u * 25u);
        CHECK(R.cur == 2 && !R.travelling, "a white key in DRIFT: there in 20 s (frozen, it takes 20 s)");
    }
    {   /* GUIDE: held keys are the notes VOX favours */
        int k, near_free = 0, near_held = 0, n_free;
        start(1, MODE_GUIDE, 0);
        J.macro[MAC_DENSITY] = 127;
        play_steps(15u * PER_MIN);
        for (k = 0; k < S.nseq; k++)
            near_free += (S.seq[k] % 12) == 9;     /* A: degree 4 of D dorian (on its pentatonic) */
        n_free = S.nseq;
        jr_guide(&R, 8, 1);                         /* the white key 8: degree 8 - 4 = 4 over the register's root */
        S.nseq = 0;
        play_steps(15u * PER_MIN);
        for (k = 0; k < S.nseq; k++)
            near_held += (S.seq[k] % 12) == 9;
        printf("  guide: A in %d of %d free notes, %d of %d with it held\n", near_free, n_free, near_held, S.nseq);
        CHECK(near_held * 2 > S.nseq && near_held * n_free > 2 * near_free * S.nseq, "GUIDE: a held key's note is most of VOX's (any chord)");
        jr_guide(&R, 8, 0);
    }
    {   /* a capture plays back what was heard: the same phrase each time round (over the chords as they move) */
        int n;
        uint8_t kept[JR_STEPS];
        uint32_t first;
        start(0, MODE_GUIDE, 0);
        J.macro[MAC_DENSITY] = 127;
        play_steps(3u * PER_MIN);
        jr_capture(&R, 1u << LP_VOX);
        memcpy(kept, J.loop[LP_VOX].step, sizeof kept);
        n = J.loop[LP_VOX].len * J.loop[LP_VOX].div;
        play_steps((uint32_t)(n - (int)(step_n % (uint32_t)n)));   /* (to the loop's start) */
        S.vox = 0;
        play_steps((uint32_t)n);
        first = S.vox;
        play_steps((uint32_t)(3 * n));
        printf("  capture: %u notes a round, %u in the next three\n", first, S.vox - first);
        CHECK(J.loop[LP_VOX].on == LOOP_CAPTURED && first >= 1 && S.vox == 4u * first &&
                  !memcmp(kept, J.loop[LP_VOX].step, sizeof kept), "a captured phrase repeats, round after round");
        jr_release(&R, 1u << LP_VOX);
        CHECK(!J.loop[LP_VOX].on, "released: the generator again");
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
        CHECK(R.rec == 0 && J.loop[LP_VOX].on == LOOP_RECORDED, "after a bar it plays");
        memset(S.vox_held, 0, sizeof S.vox_held);
        S.vox = 0;
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
