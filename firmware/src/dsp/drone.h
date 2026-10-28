/* SPDX-License-Identifier: GPL-3.0-only */
/* DRONE part: six drone voices in the spirit of the Solar 42's drone blocks, ported from the
 * browser prototype (drone-proto/drone.html).
 *
 * Each voice is a cluster of five polyBLEP oscillators tuned around its note (a stack of
 * intervals plus a detune spread and a slow per-oscillator drift), morphing square -> saw,
 * through a one-pole tone filter and a slow amplitude envelope. The voices are summed in
 * stereo by their pans, with optional "wind" noise, into a pair of saturating SVFs
 * (low-pass or band-pass) swept by an LFO and a sample & hold.
 *
 * The part sits where X0X's 909 was: eleven tracks, the first DRONE_VOICES of them voices
 * (the rest have no parameters and make no sound), and the kit (DRONE_KIT) the bus. A hit on
 * a track (a step, a key) plays its voice by its MODE:
 *   HOLD  the hit turns the voice on, the next turns it off (the Solar's latching keys)
 *   SWELL the hit swells it in, it stays for HOLD, then fades out
 *
 * Stereo, 44.1 kHz, float, no libm, no allocation. Threading as drum808: drone_trigger
 * and drone_set between render blocks, from the context that renders. */
#pragma once
#include <stdint.h>
#include "x0x_param.h"

#define DRONE_VOICES 6
#define DRONE_OSC 5
#define DRONE_TRACKS 11                   /* the pattern's drum tracks (NDRUM) */
#define DRONE_KIT DRONE_TRACKS            /* the bus's parameters */
#define DRONE_CTRL 32                     /* control rate: every 32 samples */
#define DRONE_MAX_BLOCK 256

/* a voice's pots */
enum { DN_NOTE, DN_FINE, DN_SPREAD, DN_STACK, DN_WAVE, DN_TONE, DN_ATTACK, DN_RELEASE,
       DN_HOLD, DN_MODE, DN_LEVEL, DN_PAN, DN_NPARAMS };
enum { DN_MODE_HOLD, DN_MODE_SWELL };
/* the bus's pots */
enum { DB_ROOT, DB_DRIFT, DB_CUTOFF, DB_RESO, DB_DRIVE, DB_FMODE, DB_LFORATE, DB_LFOCUT, DB_LFOPITCH,
       DB_LFOPAN, DB_SHRATE, DB_SHCUT, DB_WIND, DB_GAIN, DB_NPARAMS };

typedef struct {
    float ic1, ic2;                       /* TPT SVF state */
} drone_svf_t;

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
    uint32_t rng;
    int32_t lite;                         /* overload guard: three oscillators a voice */
    int32_t ctrl_left;                    /* samples to the next control step */
};
typedef struct drone drone_t;

void drone_init(drone_t *d);
void drone_trigger(drone_t *d, int track, float vel);   /* a hit: by the voice's MODE */
void drone_end_swells(drone_t *d);                       /* STOP: swells fall now (latched voices stay) */
void drone_all_off(drone_t *d);
/* adds the part's stereo output into l and r (the caller clears them) */
void drone_render(drone_t *d, float *l, float *r, int n);
int drone_active(const drone_t *d);
void drone_set_lite(drone_t *d, int on);
int drone_nparams(int track);
const x0x_param_t *drone_param(int track, int i);
const char *drone_track_name(int track);
void drone_set(drone_t *d, int track, int i, int value);
int drone_get(const drone_t *d, int track, int i);
