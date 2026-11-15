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
 * back through a sink. A note on carries a tag (0 .. 15), the caller's too (Penumbra's: the VOX sound it
 * was played with, 1 .. 4; 0, a loop from before 1.5: the sound in hand). At most LP_EVENTS. One context
 * (the audio ISR) calls everything.
 *
 * Each layer has settings of its own (lp_layer_t): its LEVEL (100: as played), a sound instead of the ones
 * its notes were played with, an octave down or up, a mute, and four the sink applies to its notes (its
 * place in the field and how far it drifts round it, its tone and its swell), and a length of its own: it
 * plays round in that many beats, shorter cutting its end, longer leaving a rest, so it drifts against the
 * others. A layer can be taken out from anywhere
 * (looper_layer_delete): the ones over it move down. The layers sound apart: the same note in two of them
 * is two notes. */
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
enum { LL_LEVEL, LL_SOUND, LL_OCT, LL_MUTE, LL_PAN, LL_DRIFT, LL_TONE, LL_SWELL, LL_BEATS, LL_N };   /* (looper_layer_set) */

typedef struct {
    uint8_t level;                              /* 0 .. 127: its notes' and hits' loudness, 100 as played */
    uint8_t snd;                                /* 0: each note the tag it was played with; else this tag (1 .. 15) */
    uint8_t oct;                                /* 0 an octave down, 1 as played, 2 an octave up (the sink's to do) */
    uint8_t mute;                               /* 1: it does not sound */
    uint8_t pan;                                /* its place, over its notes' (64: the middle) */
    uint8_t drift;                              /* 0 still .. 127: how far it sways round its place (the sink's) */
    uint8_t tone;                               /* 64 as its sound: darker .. brighter (the sink's) */
    uint8_t swell;                              /* 64 as its sound: shorter, plucked .. slower in, longer out */
    uint8_t beats;                              /* its own length: 0 the loop's, else 1 .. 64 beats */
    uint8_t rsv[3];
} lp_layer_t;

typedef struct {
    uint32_t t;                                 /* ticks from the loop's start */
    uint8_t note, vel, layer, kind;             /* vel 0: off. kind: LK_* in its low four bits, the tag above */
} lp_ev_t;

typedef struct {
    void *ctx;
    /* tag: the note's (or its layer's) in the low four bits, the layer's octave (0, 1, 2) above them */
    void (*on)(void *ctx, int note, int vel, int tag, int layer);
    void (*off)(void *ctx, int note, int layer);
    void (*hit)(void *ctx, int which, int vel);
} lp_sink_t;

typedef struct {
    lp_ev_t ev[LP_EVENTS];
    uint16_t n;
    uint8_t state, layers;                      /* layers kept (the one being recorded is layers) */
    uint32_t len, pos;                          /* ticks; pos: where it is in it (recording first: since its start) */
    uint32_t sounding[LP_LAYERS][8];            /* notes each layer has on (bit note: 0..255) */
    lp_layer_t lay[LP_LAYERS];
    uint32_t lpos[LP_LAYERS];                   /* a layer of its own length: where it is in it (ticks) */
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
/* the clock put at a place (a DAW's SONG POSITION): a playing loop where it would be, had it played from
 * the downbeat (ticks since it), silently there (what starts at that tick plays) */
void looper_seek(looper_t *l, uint32_t ticks, const lp_sink_t *o);
void looper_note(looper_t *l, int note, int vel, int tag);    /* the hands (vel 0: off): recorded while it records */
void looper_hit(looper_t *l, int which, int vel);             /* a hit, the same */
void looper_advance(looper_t *l, uint32_t ticks, const lp_sink_t *o);
/* a layer's setting (LL_*; 0 .. layers - 1: the kept ones), and a layer taken out (the only one: the loop gone) */
void looper_layer_set(looper_t *l, int layer, int field, int value, const lp_sink_t *o);
void looper_layer_delete(looper_t *l, int layer, const lp_sink_t *o);
/* saving: what is worth keeping, n bytes of it (0: nothing, or not now: recording); and back */
#define LP_SAVE_HDR (16u + LP_LAYERS * 12u)
#define LP_SAVE_MAX (LP_SAVE_HDR + LP_EVENTS * 8u)
uint32_t looper_save(const looper_t *l, void *dst, uint32_t max);
int looper_load(looper_t *l, const void *src, uint32_t n);    /* 0: loaded (stopped, or playing as it was) */
