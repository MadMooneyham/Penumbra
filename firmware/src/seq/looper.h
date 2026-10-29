/* SPDX-License-Identifier: GPL-3.0-only */
/* Penumbra's looper: VOX's notes and PULSE's hits, as played, round and round on the clock, in layers.
 *
 *   REC      empty: records from now. Recording: the loop closes here (its length rounded to the
 *            nearest beat) and plays. Playing: a new layer over it (overdub); again: it is kept.
 *   PLAY     playing: stops (what sounds lets go); stopped: plays from its start. Recording or
 *            overdubbing: closes that, and plays on.
 *   UNDO     the last layer gone (the first: the loop gone).
 *   CLEAR    the loop gone.
 *   SPEED    half, as played, double: how fast it goes round (the first recording: as played).
 *
 * Time is the clock's: LP_TPS ticks a 16th, counted by the caller (looper_advance with the ticks since
 * the last call, from the clock's steps and the frames since the last one). A note is whatever the
 * caller says it is (Penumbra's: a scale degree, so a loop follows the scene's key): the loop plays it
 * back through a sink. At most LP_EVENTS. One context (the audio ISR) calls everything. */
#pragma once
#include <stdint.h>

#define LP_TPS 96u                              /* ticks a 16th */
#define LP_BEAT (4u * LP_TPS)
#define LP_EVENTS 384
#define LP_LAYERS 8
#define LP_MAX_LEN (64u * 16u * LP_TPS)         /* 16 bars of 4/4 */

enum { LS_EMPTY, LS_REC, LS_PLAY, LS_DUB, LS_STOP };
enum { LP_HALF, LP_AS_PLAYED, LP_DOUBLE };
enum { LK_NOTE, LK_HIT };                        /* an event's kind: a note (on, off), a hit (PULSE: on only) */

typedef struct {
    uint32_t t;                                 /* ticks from the loop's start */
    uint8_t note, vel, layer, kind;             /* vel 0: off */
} lp_ev_t;

typedef struct {
    void *ctx;
    void (*on)(void *ctx, int note, int vel);
    void (*off)(void *ctx, int note);
    void (*hit)(void *ctx, int which, int vel);
} lp_sink_t;

typedef struct {
    lp_ev_t ev[LP_EVENTS];
    uint16_t n;
    uint8_t state, layers;                      /* layers kept (the one being recorded is layers) */
    uint32_t len, pos;                          /* ticks; pos: where it is in it (recording first: since its start) */
    uint32_t sounding[8];                       /* notes it has on (bit note: 0..255) */
    uint32_t held[8];                           /* the hands' notes on, while recording */
    uint8_t full;                               /* events were dropped */
    uint8_t speed, frac;                        /* LP_*; half speed's odd tick */
    uint32_t edits;                             /* counts every change worth saving */
} looper_t;

void looper_init(looper_t *l);
void looper_rec(looper_t *l, const lp_sink_t *o);
void looper_play(looper_t *l, const lp_sink_t *o);
void looper_undo(looper_t *l, const lp_sink_t *o);
void looper_clear(looper_t *l, const lp_sink_t *o);
void looper_speed(looper_t *l, int dir);                      /* slower (-), faster (+) */
void looper_quiet(looper_t *l, const lp_sink_t *o);           /* the clock stopped: what it has on, off */
void looper_restart(looper_t *l, const lp_sink_t *o);         /* the clock started: a playing loop from its start */
void looper_note(looper_t *l, int note, int vel);             /* the hands (vel 0: off): recorded while it records */
void looper_hit(looper_t *l, int which, int vel);             /* a hit, the same */
void looper_advance(looper_t *l, uint32_t ticks, const lp_sink_t *o);
/* saving: what is worth keeping, n bytes of it (0: nothing, or not now: recording); and back */
uint32_t looper_save(const looper_t *l, void *dst, uint32_t max);
int looper_load(looper_t *l, const void *src, uint32_t n);    /* 0: loaded (stopped, or playing as it was) */
