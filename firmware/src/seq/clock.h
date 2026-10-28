/* SPDX-License-Identifier: GPL-3.0-only */
/* Penumbra's transport clock: the tempo, PLAY / STOP, and MIDI sync both ways (24 ppqn).
 *
 * Internal: the tempo is bpm; while playing, MIDI clock goes out (with START / STOP) when
 * send_clock is set. External: MIDI clock ticks (USB or the TRS jack) take over as soon as
 * they arrive (when follow is set), their period smoothed into the tempo; START, CONTINUE and
 * STOP from outside run the transport. Half a second without a tick: back to internal.
 *
 * Time moves in samples. The audio ISR asks how far it may render before the next event
 * (tclock_until), renders that far, then advances (tclock_advance), which fires the events
 * due: MIDI out through the sink and a step every 16th note. Everything here runs in the
 * audio ISR; the main loop only sets requests (tclock_play / tclock_stop) and reads. */
#pragma once
#include <stdint.h>

#define TCLOCK_FS 44100u

typedef struct {
    void *ctx;
    void (*midi)(void *ctx, uint8_t status);   /* a real-time byte out: F8, FA, FB, FC */
    void (*step)(void *ctx, uint32_t n);       /* the n-th 16th since PLAY (0 = the downbeat) */
    void (*stopped)(void *ctx);                /* the transport stopped */
} tclock_sink_t;

typedef struct {
    float bpm;                          /* internal tempo */
    uint8_t send_clock;                 /* internal: MIDI clock and transport out */
    uint8_t follow;                     /* external clock in is followed */
    volatile uint8_t playing;
    volatile uint8_t ext;               /* following an external clock now */
    volatile uint8_t req_start, req_cont, req_stop;
    volatile uint32_t ext_pending;      /* ticks received, not yet counted */
    uint32_t ext_since;                 /* samples since the last tick */
    uint32_t ext_period;                /* smoothed tick period, samples Q8 */
    float ext_bpm;
    uint32_t clk_to_next;               /* internal: samples to the next clock out, Q8 */
    uint32_t step_to_next;              /* internal: samples to the next 16th, Q8 */
    uint8_t ext_ticks;                  /* external: ticks into this 16th */
    uint32_t steps;                     /* 16ths since PLAY */
} tclock_t;

void tclock_init(tclock_t *c);
float tclock_tempo(const tclock_t *c);
/* main loop or MIDI: transport requests (taken at the next block) */
void tclock_play(tclock_t *c);
void tclock_continue(tclock_t *c);
void tclock_stop(tclock_t *c);
void tclock_ext_tick(tclock_t *c);      /* one incoming 24-ppqn tick (F8) */
/* audio ISR: samples (1..n) to render before the next event; then move time by k */
uint32_t tclock_until(const tclock_t *c, uint32_t n);
void tclock_advance(tclock_t *c, uint32_t k, const tclock_sink_t *o);
