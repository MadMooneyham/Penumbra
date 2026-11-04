/* SPDX-License-Identifier: GPL-3.0-only */
/* Penumbra's transport clock: the tempo, PLAY / STOP, and MIDI sync both ways (24 ppqn).
 *
 * Internal: the tempo is bpm; while playing, MIDI clock goes out (with START / STOP) when
 * send_clock is set. External: MIDI clock ticks (USB or the TRS jack) take over as soon as
 * they arrive (when follow is set); START, CONTINUE and STOP from outside run the transport, and
 * SONG POSITION puts it at a 16th (a DAW's locate and loop: the next tick plays there). Half a
 * second without a tick: back to internal.
 *
 * The tempo: ticks are counted at block edges (5.8 ms apart), too coarse to time them by. Each one
 * carries the time it arrived (TCLOCK_STAMP_HZ, taken where it is received), and the tempo is the
 * span of the last TCLOCK_SPAN of them: a DAW's 120 to within ~0.1 BPM. A tick without a time (the
 * browser's) falls back to its period in samples, smoothed.
 *
 * Time moves in samples. The audio ISR asks how far it may render before the next event
 * (tclock_until), renders that far, then advances (tclock_advance), which fires the events
 * due: MIDI out through the sink and a step every 16th note. Everything here runs in the
 * audio ISR; the main loop only sets requests (tclock_play / tclock_stop) and reads. */
#pragma once
#include <stdint.h>

#define TCLOCK_FS 44100u
#define TCLOCK_STAMP_HZ 5859.375f               /* a tick's time: 24 MHz / 4096, 16 bits (11 s round) */
#define TCLOCK_SPAN 48u                         /* the tempo over the last two beats' ticks */

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
    uint16_t stamp[TCLOCK_SPAN + 1u];   /* the last ticks' times, a ring */
    uint8_t stamp_n, stamp_at;          /* how many (to TCLOCK_SPAN + 1), the next one's place */
    uint8_t ext_stamped;                /* the ticks carry their times: the tempo is theirs */
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
void tclock_ext_tick_at(tclock_t *c, uint16_t stamp);   /* one with the time it came (TCLOCK_STAMP_HZ) */
void tclock_song_pos(tclock_t *c, uint32_t sixteenths); /* SONG POSITION (F2): the next tick plays there */
/* audio ISR: samples (1..n) to render before the next event; then move time by k */
uint32_t tclock_until(const tclock_t *c, uint32_t n);
void tclock_advance(tclock_t *c, uint32_t k, const tclock_sink_t *o);
