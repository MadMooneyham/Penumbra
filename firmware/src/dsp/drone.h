/* SPDX-License-Identifier: GPL-3.0-only */
/* DRONE part: five drone voices in the spirit of the Solar 42's drone blocks, ported from the
 * browser prototype (drone-proto/drone.html). (Six before 0.9: SWARM makes the crowd a sixth added.)
 *
 * Each voice is a few polyBLEP oscillators tuned around its note (its stack's intervals, two to
 * four, plus a detune spread and a slow per-oscillator drift), morphing square -> saw, through a
 * one-pole tone filter and a slow amplitude envelope. The voices are summed in stereo by their
 * pans, with optional "wind" noise, into a pair of saturating SVFs (low-pass or band-pass) swept
 * by an LFO and a sample & hold.
 *
 * SWARM, after the filter: the crowd the oscillators no longer make. Three taps a side read a
 * short delay (7 .. 16 ms) that keeps moving: MOTION 0 moves them by slow and fast sines a third
 * of a turn apart (a string ensemble), MOTION 127 by each tap's own smoothed random walk (no two
 * alike, many players). SWARM sets how much of it is heard. Its lines are the caller's
 * (drone_swarm_buffer, .pool); without them it is off.
 *
 * Parameters are addressed by "track": 0 .. DRONE_VOICES-1 a voice, DRONE_KIT the bus. A hit
 * on a voice (a black key, the director) plays it by its MODE:
 *   HOLD  the hit turns the voice on, the next turns it off (the Solar's latching keys)
 *   SWELL the hit swells it in, it stays for HOLD, then fades out
 *
 * Stereo, 44.1 kHz, float, no libm, no allocation. drone_trigger and drone_set run between
 * render blocks, from the context that renders. */
#pragma once
#include <stdint.h>
#include "param.h"

#define DRONE_VOICES 5
#define DRONE_OSC 5
#define DRONE_KIT DRONE_VOICES            /* the bus's parameters */
#define DRONE_CTRL 32                     /* control rate: every 32 samples */
#define DRONE_MAX_BLOCK 256

/* a voice's pots */
enum { DN_NOTE, DN_FINE, DN_SPREAD, DN_STACK, DN_WAVE, DN_TONE, DN_ATTACK, DN_RELEASE,
       DN_HOLD, DN_MODE, DN_LEVEL, DN_PAN, DN_NPARAMS };
enum { DN_MODE_HOLD, DN_MODE_SWELL };
/* the bus's pots */
enum { DB_ROOT, DB_DRIFT, DB_CUTOFF, DB_RESO, DB_DRIVE, DB_FMODE, DB_LFORATE, DB_LFOCUT, DB_LFOPITCH,
       DB_LFOPAN, DB_SHRATE, DB_SHCUT, DB_WIND, DB_GAIN, DB_SWARM, DB_SWMOTION, DB_NPARAMS };

#define SWARM_TAPS 3                      /* a side */
#define SWARM_LEN 1024                    /* floats a side (power of two): 23 ms */

typedef struct {
    float ic1, ic2;                       /* TPT SVF state */
} drone_svf_t;

typedef struct {
    float *buf[2];                        /* 0: no swarm */
    int32_t w;
    float amt, motion;
    float dl[2][SWARM_TAPS], step[2][SWARM_TAPS];   /* each tap's delay now and its step a sample */
    float walk[2][SWARM_TAPS], walk2[2][SWARM_TAPS], walk_tgt[2][SWARM_TAPS];   /* (smoothed twice) */
    float slow_ph, fast_ph;
} drone_swarm_t;

/* a stretch of a planned block: what its control step set, so the voices can render on either core
 * from it (read only) while the bus waits for their sum */
#define DRONE_SEGS (DRONE_MAX_BLOCK / DRONE_CTRL + 2)
typedef struct {
    int16_t len, ctrl;                    /* samples; 1: a control step starts it */
    float xg;                             /* the overload guard's fade of the oscillators past the second */
    float a1, a2, a3;                     /* the bus filter */
    float sw_tgt[2][SWARM_TAPS];          /* where SWARM's taps head (ctrl stretches) */
    uint8_t nosc[DRONE_VOICES];           /* oscillators rendered (0: the voice is silent) */
    float scale[DRONE_VOICES];            /* its loudness by its oscillators */
    float inc[DRONE_VOICES][DRONE_OSC], pan_l[DRONE_VOICES], pan_r[DRONE_VOICES];
} drone_seg_t;

typedef struct {
    uint8_t pot[DN_NPARAMS];
    float ph[DRONE_OSC];                  /* oscillator phases, turns */
    float inc[DRONE_OSC];                 /* this control step's increments */
    float drift_ph[DRONE_OSC][2], drift_inc[DRONE_OSC][2];   /* two slow sines per oscillator, turns */
    float lp;                             /* tone filter state */
    float tone_c;
    float amp, amp_tgt;                   /* envelope: now, where it is going */
    float atk_c, rel_c;                   /* one-pole coefficients per sample */
    float wave, level, pan;               /* derived from the pots */
    float pan_l, pan_r;
    int32_t on;                           /* HOLD: latched on; SWELL: in the swell */
    int32_t hold_left;                    /* SWELL: samples until the release */
} drone_voice_t;

struct drone {
    drone_voice_t v[DRONE_VOICES];
    uint8_t bus[DB_NPARAMS];
    float drift_c;                        /* cents of drift at full swing */
    float cutoff, reso, drive, drive_post;
    int32_t bp;
    float lfo_ph, lfo_inc, lfo_cut, lfo_pitch, lfo_pan;
    float sh_ph, sh_inc, sh_val, sh_sm, sh_cut;
    float wind, gain;
    float lfo_now;                        /* this control step's LFO value */
    float a1, a2, a3;                     /* SVF coefficients, both sides */
    drone_svf_t fl, fr;
    float nb_l, nb_r;                     /* wind: brown noise */
    uint32_t rng;                         /* the control steps' (drone_plan) */
    uint32_t bus_rng;                     /* the wind's (drone_bus): the two may run on different cores */
    int32_t lite;                         /* overload guard: two oscillators a voice */
    float lite_g;                         /* the oscillators past the second: 1 sounding, 0 gone (glides, ~80 ms) */
    int32_t ctrl_left;                    /* samples to the next control step */
    drone_swarm_t sw;
    drone_seg_t seg[2][DRONE_SEGS];       /* two planned blocks: the bus may run on the last while the next is planned */
    int32_t nseg[2], plan_n, cur;         /* cur: the newest plan */
};
typedef struct drone drone_t;

void drone_init(drone_t *d);
void drone_swarm_buffer(drone_t *d, float *l, float *r);   /* SWARM_LEN each */
void drone_trigger(drone_t *d, int track, float vel);   /* a hit: by the voice's MODE */
void drone_end_swells(drone_t *d);                       /* STOP: swells fall now (latched voices stay) */
void drone_all_off(drone_t *d);
/* adds the part's stereo output into l and r (the caller clears them): drone_plan, every voice, the bus */
void drone_render(drone_t *d, float *l, float *r, int n);
/* the same in steps, so the work can run on two cores at once:
 *   drone_plan        the block's control steps (once, before its voices); returns the plan's number
 *   drone_voices      adds the voices in mask (bit v) of the newest plan into l, r; two calls with disjoint
 *                     masks may run at the same time (each touches only its own voices and buffers)
 *   drone_bus_filter  on the voices' sum of plan p, in place: wind, drive, the filter
 *   drone_bus_swarm   then SWARM on it; the bus (both) may run on plan p while plan p ^ 1, the next block,
 *                     is planned and its voices render (it touches only the bus's state)
 * drone_cost: a voice's oscillators in the newest plan (0: silent), for splitting the work */
int drone_plan(drone_t *d, int n);
int drone_cost(const drone_t *d, int voice);
void drone_voices(drone_t *d, uint32_t mask, float *l, float *r);
void drone_bus_filter(drone_t *d, int p, float *l, float *r);
void drone_bus_swarm(drone_t *d, int p, float *l, float *r);
int drone_active(const drone_t *d);
void drone_set_lite(drone_t *d, int on);
int drone_nparams(int track);
const param_t *drone_param(int track, int i);
const char *drone_track_name(int track);
void drone_set(drone_t *d, int track, int i, int value);
int drone_get(const drone_t *d, int track, int i);
