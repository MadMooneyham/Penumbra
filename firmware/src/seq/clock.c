/* SPDX-License-Identifier: GPL-3.0-only */
/* Penumbra's transport clock; see clock.h. The external-clock smoothing and the clock-out timing
 * are X0X's sequencer's (seq/sequencer.c at penumbra-0.6-shelved), which held sync on hardware. */
#include "clock.h"

#define TICKS_PER_STEP 6u                       /* 24 ppqn: six to a 16th */
#define NEVER 0xFFFFFFFFu

void tclock_init(tclock_t *c)
{
    uint8_t *z = (uint8_t *)c;
    uint32_t i;
    for (i = 0; i < sizeof *c; i++)
        z[i] = 0;
    c->bpm = 90.0f;
    c->ext_bpm = 90.0f;
    c->send_clock = 1;
    c->follow = 1;
    c->step_to_next = NEVER;
}

float tclock_tempo(const tclock_t *c)
{
    float b = c->ext ? c->ext_bpm : c->bpm;
    return b < 20.0f ? 20.0f : b > 300.0f ? 300.0f : b;
}

static uint32_t step_q8(const tclock_t *c)      /* samples per 16th, Q8 */
{
    return (uint32_t)((float)TCLOCK_FS * 60.0f * 256.0f / (4.0f * tclock_tempo(c)));
}

void tclock_play(tclock_t *c) { c->req_start = 1; }
void tclock_continue(tclock_t *c) { c->req_cont = 1; }
void tclock_stop(tclock_t *c) { c->req_stop = 1; }
void tclock_ext_tick(tclock_t *c) { c->ext_pending++; }

uint32_t tclock_until(const tclock_t *c, uint32_t n)
{
    uint32_t m = n, e;
    if (!c->playing || c->ext)
        return n;                               /* (external steps come with the ticks, at block edges) */
    if (c->send_clock && (e = c->clk_to_next >> 8) < m)
        m = e;
    if ((e = c->step_to_next >> 8) < m)
        m = e;
    return m ? m : 1;
}

static void out(const tclock_sink_t *o, uint8_t st)
{
    if (o->midi)
        o->midi(o->ctx, st);
}

static void do_start(tclock_t *c, int from_top, const tclock_sink_t *o)
{
    if (from_top)
        c->steps = 0;
    c->playing = 1;
    c->clk_to_next = 0;
    c->step_to_next = c->ext ? NEVER : 0;
    c->ext_ticks = TICKS_PER_STEP - 1u;         /* the first tick after START is the downbeat */
    if (!c->ext && c->send_clock)
        out(o, from_top ? 0xFA : 0xFB);
}

static void do_stop(tclock_t *c, const tclock_sink_t *o)
{
    c->playing = 0;
    c->step_to_next = NEVER;
    if (!c->ext && c->send_clock)
        out(o, 0xFC);
    if (o->stopped)
        o->stopped(o->ctx);
}

static void fire_step(tclock_t *c, const tclock_sink_t *o)
{
    if (o->step)
        o->step(o->ctx, c->steps);
    c->steps++;
}

static void ext_ticks(tclock_t *c, const tclock_sink_t *o)
{
    uint32_t n = c->ext_pending;
    c->ext_pending = 0;
    for (; n; n--) {
        if (c->ext_since > 0 && c->ext_since < TCLOCK_FS) {   /* tempo: the smoothed tick period */
            uint32_t q = c->ext_since << 8;
            c->ext_period = c->ext_period ? c->ext_period - (c->ext_period >> 3) + (q >> 3) : q;
            c->ext_bpm = (float)TCLOCK_FS * 60.0f * 256.0f / (24.0f * (float)c->ext_period);
        }
        c->ext_since = 0;
        if (c->playing && ++c->ext_ticks >= TICKS_PER_STEP) {
            c->ext_ticks = 0;
            fire_step(c, o);
        }
    }
}

void tclock_advance(tclock_t *c, uint32_t k, const tclock_sink_t *o)
{
    uint32_t dq = k << 8, guard;
    c->ext_since = c->ext_since + k < c->ext_since ? NEVER : c->ext_since + k;
    if (c->ext_pending && !c->follow)
        c->ext_pending = 0;                     /* not following: ticks are ignored */
    if (c->ext_pending) {
        c->ext = 1;
    } else if (c->ext && c->ext_since > TCLOCK_FS / 2u) {   /* clock gone for 0.5 s: back to internal */
        c->ext = 0;
        if (c->playing)
            c->step_to_next = 0;
    }
    if (c->req_stop) {
        c->req_stop = c->req_start = c->req_cont = 0;
        if (c->playing)
            do_stop(c, o);
    }
    if (c->req_start) {
        c->req_start = 0;
        do_start(c, 1, o);
    }
    if (c->req_cont) {
        c->req_cont = 0;
        do_start(c, 0, o);
    }
    if (c->ext_pending)
        ext_ticks(c, o);
    if (!c->playing || c->ext)
        return;
    c->clk_to_next = c->clk_to_next > dq ? c->clk_to_next - dq : 0;
    c->step_to_next = c->step_to_next > dq ? c->step_to_next - dq : 0;
    for (guard = 0; guard < 4u && c->send_clock && c->clk_to_next < 256u; guard++) {
        out(o, 0xF8);
        c->clk_to_next += step_q8(c) / TICKS_PER_STEP;
    }
    for (guard = 0; guard < 4u && c->step_to_next < 256u; guard++) {
        fire_step(c, o);
        c->step_to_next += step_q8(c);
    }
}
