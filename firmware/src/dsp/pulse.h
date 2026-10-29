/* SPDX-License-Identifier: GPL-3.0-only */
/* PULSE: Penumbra's percussion, two sounds that stay out of the drones' way.
 *
 *   KICK  a deep sine, tuned to the key (TUNE semitones over the root, A1 = 55 Hz at 0): each hit
 *         starts PITCH octaves higher and falls to the note over PDECAY, sounds for DECAY, with a soft
 *         click on its front (filtered noise, CLICK) and a gentle DRIVE that rounds it
 *   BLOOM a deep, slow swell in the key: two sines an octave over the root, a few cents apart (one a
 *         side, beating), and the fifth over them, softly; in over ~2 s, out over LENGTH. Out on its
 *         own pair (the caller sends it far into the reverb; the kick stays close)
 *
 * A hit on a sounding kick (or bloom) carries on from its level and its phase (the envelope rises
 * from where it is), so nothing clicks. A silent PULSE costs nothing.
 *
 * Stereo out, 44.1 kHz, float, no libm, no allocation. pulse_hit and pulse_set between render blocks,
 * from the context that renders. */
#pragma once
#include <stdint.h>
#include "param.h"

/* BLOOM: its level, LENGTH (in and out together), HEIGHT (0 on the root, 1 an octave over, 2 two),
 * FIFTH (how much of the fifth over it) */
enum { PL_TUNE, PL_PITCH, PL_PDECAY, PL_DECAY, PL_CLICK, PL_DRIVE, PL_LEVEL, PL_BLOOM, PL_BLEN, PL_BHEIGHT, PL_BFIFTH,
       PL_NPARAMS };
enum { PULSE_KICK, PULSE_BLOOM };

typedef struct {
    uint8_t pot[PL_NPARAMS];
    float root;                           /* the key's root, a MIDI note (the scene's) */
    float base_inc, pitch_r, pdec_c, dec_c, click, drive, drive_n, level, bloom, brise_c, bfall_c, bmul, bfifth;
    /* the kick */
    float ph, penv, amp, amp_tgt, vel;
    int32_t rise;                         /* rising to the hit's level */
    float cenv, clp;                      /* the click: envelope, low-passed noise */
    /* the bloom */
    float bph[3], benv, bvel;             /* its sines (left, right, the fifth); its envelope */
    int32_t brise;                        /* 1: swelling in */
    uint32_t rng;
} pulse_t;

void pulse_init(pulse_t *p);
void pulse_root(pulse_t *p, int midi_note);   /* the key the kick is tuned to (A1 = 33) */
void pulse_hit(pulse_t *p, int which, float vel);
int pulse_active(const pulse_t *p);
/* adds the kick into l and r, the bloom into bl and br (the caller clears them) */
void pulse_render(pulse_t *p, float *l, float *r, float *bl, float *br, int n);
int pulse_blooming(const pulse_t *p);     /* the bloom sounds (its pair has something) */
int pulse_nparams(void);
const param_t *pulse_param(int i);
void pulse_set(pulse_t *p, int i, int value);
int pulse_get(const pulse_t *p, int i);
