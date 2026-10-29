/* SPDX-License-Identifier: GPL-3.0-only */
/* Penumbra's looper: VOX's notes, as played, round and round on the clock, in layers.
 *
 *   REC      empty: records from now. Recording: the loop closes here (its length rounded to the
 *            nearest beat) and plays. Playing: a new layer over it (overdub); again: it is kept.
 *   PLAY     playing: stops (what sounds lets go); stopped: plays from its start. Recording or
 *            overdubbing: closes that, and plays on.
 *   UNDO     the last layer gone (the first: the loop gone).
 *   CLEAR    the loop gone.
 *
 * Time is the clock's: LP_TPS ticks a 16th, counted by the caller (looper_advance with the ticks since
 * the last call, from the clock's steps and the frames since the last one). The loop plays through a
 * sink, so what it plays is the same as a key's. Notes only, at most LP_EVENTS; not saved.
 * One context (the audio ISR) calls everything. */
#pragma once
#include <stdint.h>

#define LP_TPS 96u                              /* ticks a 16th */
#define LP_BEAT (4u * LP_TPS)
#define LP_EVENTS 384
#define LP_LAYERS 8
#define LP_MAX_LEN (64u * 16u * LP_TPS)         /* 16 bars of 4/4 */

enum { LS_EMPTY, LS_REC, LS_PLAY, LS_DUB, LS_STOP };

typedef struct {
    uint32_t t;                                 /* ticks from the loop's start */
    uint8_t note, vel, layer, rsv;              /* vel 0: off */
} lp_ev_t;

typedef struct {
    void *ctx;
    void (*on)(void *ctx, int note, int vel);
    void (*off)(void *ctx, int note);
} lp_sink_t;

typedef struct {
    lp_ev_t ev[LP_EVENTS];
    uint16_t n;
    uint8_t state, layers;                      /* layers kept (the one being recorded is layers) */
    uint32_t len, pos;                          /* ticks; pos: where it is in it (recording first: since its start) */
    uint32_t sounding[4];                       /* notes it has on (bit note) */
    uint32_t held[4];                           /* the hands' notes on, while recording */
    uint8_t full;                               /* events were dropped */
} looper_t;

void looper_init(looper_t *l);
void looper_rec(looper_t *l, const lp_sink_t *o);
void looper_play(looper_t *l, const lp_sink_t *o);
void looper_undo(looper_t *l, const lp_sink_t *o);
void looper_clear(looper_t *l, const lp_sink_t *o);
void looper_quiet(looper_t *l, const lp_sink_t *o);           /* the clock stopped: what it has on, off */
void looper_restart(looper_t *l, const lp_sink_t *o);         /* the clock started: a playing loop from its start */
void looper_note(looper_t *l, int note, int vel);             /* the hands (vel 0: off): recorded while it records */
void looper_advance(looper_t *l, uint32_t ticks, const lp_sink_t *o);
