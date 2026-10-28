/* SPDX-License-Identifier: GPL-3.0-only */
/* The transport clock (firmware/src/seq/clock.c) alone: internal tempo and clock out at the sample,
 * START / STOP out, an external clock followed (tempo, the downbeat after START) and given up, and
 * ignored when it is not to be followed. Time moves in 256-sample blocks split at events, as the
 * engine moves it. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "../../firmware/src/seq/clock.h"

static int fails;
#define CHECK(c, what) do { if (c) printf("  ok   %s\n", what); else { printf("  FAIL %s\n", what); fails++; } } while (0)

static uint32_t now, n_f8, n_fa, n_fc, n_steps, n_stopped, step_at[64];
static void s_midi(void *x, uint8_t st)
{
    (void)x;
    n_f8 += st == 0xF8;
    n_fa += st == 0xFA;
    n_fc += st == 0xFC;
}
static void s_step(void *x, uint32_t n)
{
    (void)x;
    if (n < 64u)
        step_at[n] = now;
    n_steps++;
}
static void s_stopped(void *x) { (void)x; n_stopped++; }
static const tclock_sink_t SINK = {0, s_midi, s_step, s_stopped};

static void block(tclock_t *c, uint32_t n)       /* as engine_render */
{
    uint32_t done, k;
    tclock_advance(c, 0, &SINK);
    for (done = 0; done < n; done += k) {
        k = tclock_until(c, n - done);
        now += k;                                 /* (the events it fires are due at the stretch's end) */
        tclock_advance(c, k, &SINK);
    }
}
static void run(tclock_t *c, uint32_t samples)
{
    uint32_t end = now + samples;
    while (now < end)
        block(c, 256);
}
static void reset(void)
{
    now = n_f8 = n_fa = n_fc = n_steps = n_stopped = 0;
    memset(step_at, 0, sizeof step_at);
}

int main(void)
{
    tclock_t c;
    /* internal, 120 BPM: a 16th is 5512.5 samples, a clock 918.75 */
    reset();
    tclock_init(&c);
    c.bpm = 120.0f;
    run(&c, 44100);
    CHECK(n_f8 == 0 && n_steps == 0, "stopped: no clock, no steps");
    tclock_play(&c);
    run(&c, 44100);
    CHECK(n_fa == 1, "PLAY sends START once");
    CHECK(n_f8 >= 47 && n_f8 <= 49, "120 BPM: 48 clocks a second");
    CHECK(n_steps >= 8 && n_steps <= 9, "120 BPM: 8 16ths a second");
    {
        int ok = 1, i;
        for (i = 1; i < 8; i++) {
            int32_t d = (int32_t)(step_at[i] - step_at[i - 1]);
            ok &= d >= 5512 && d <= 5513;
        }
        CHECK(ok, "16ths land on their sample (5512 or 5513 apart)");
    }
    tclock_stop(&c);
    run(&c, 4410);
    CHECK(n_fc == 1 && n_stopped == 1 && !c.playing, "STOP sends STOP, stops once");
    {
        uint32_t f8 = n_f8;
        run(&c, 44100);
        CHECK(n_f8 == f8, "stopped: the clock out stops too");
    }
    /* send_clock off: steps, no MIDI */
    reset();
    tclock_init(&c);
    c.send_clock = 0;
    tclock_play(&c);
    run(&c, 44100);
    CHECK(n_fa == 0 && n_f8 == 0 && n_steps >= 5, "clock out off: steps run, nothing sent");
    /* external: 24 ticks a quarter at 100 BPM (1102.5 samples a tick), START first */
    reset();
    tclock_init(&c);
    tclock_play(&c);                              /* as MIDI START would */
    {
        double t = 0.0, tick = 44100.0 * 60.0 / (100.0 * 24.0);
        uint32_t end = 3u * 44100u;
        while (now < end) {
            while (t <= (double)now + 256.0 && t < (double)end) {
                tclock_ext_tick(&c);
                t += tick;
            }
            block(&c, 256);
        }
    }
    CHECK(c.ext == 1, "an external clock is followed");
    CHECK(tclock_tempo(&c) > 99.0f && tclock_tempo(&c) < 101.0f, "its tempo is measured (100 BPM)");
    CHECK(n_fa == 0, "following: nothing sent (START came from outside)");
    CHECK(n_steps >= 19 && n_steps <= 21, "its 16ths: one every 6 ticks (20 in 3 s)");
    run(&c, 30000);
    CHECK(c.ext == 0, "half a second without ticks: internal again");
    /* not following: ticks ignored */
    reset();
    tclock_init(&c);
    c.follow = 0;
    {
        int i;
        for (i = 0; i < 200; i++) {
            tclock_ext_tick(&c);
            block(&c, 256);
        }
    }
    CHECK(c.ext == 0, "follow off: an external clock is ignored");
    printf(fails ? "%d FAILED\n" : "all passed\n", fails);
    return fails != 0;
}
