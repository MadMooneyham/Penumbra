/* SPDX-License-Identifier: GPL-3.0-only */
/* PULSE: Penumbra's percussion, two sounds that stay out of the drones' way.
 *
 *   KICK  a deep sine, tuned to the key (TUNE semitones over the root, A1 = 55 Hz at 0): each hit
 *         starts PITCH octaves higher and falls to the note over PDECAY, sounds for DECAY, with a soft
 *         click on its front (filtered noise, CLICK) and a gentle DRIVE that rounds it
 *   TICK  a short high noise burst, quiet: the dust between the beats
 *
 * A hit on a sounding kick carries on from its level and its phase (the envelope rises from where it
 * is), so a fast pulse never clicks. A silent PULSE costs nothing.
 *
 * Stereo out (the kick in the middle, the tick a little right), 44.1 kHz, float, no libm, no
 * allocation. pulse_hit and pulse_set between render blocks, from the context that renders. */
#pragma once
#include <stdint.h>
#include "param.h"

enum { PL_TUNE, PL_PITCH, PL_PDECAY, PL_DECAY, PL_CLICK, PL_DRIVE, PL_LEVEL, PL_TICK, PL_TDECAY, PL_NPARAMS };
enum { PULSE_KICK, PULSE_TICK };

typedef struct {
    uint8_t pot[PL_NPARAMS];
    float root;                           /* the key's root, a MIDI note (the scene's) */
    float base_inc, pitch_r, pdec_c, dec_c, click, drive, drive_n, level, tick, tdec_c;
    /* the kick */
    float ph, penv, amp, amp_tgt, vel;
    int32_t rise;                         /* rising to the hit's level */
    float cenv, clp;                      /* the click: envelope, low-passed noise */
    /* the tick */
    float tenv, thp, tvel;
    uint32_t rng;
} pulse_t;

void pulse_init(pulse_t *p);
void pulse_root(pulse_t *p, int midi_note);   /* the key the kick is tuned to (A1 = 33) */
void pulse_hit(pulse_t *p, int which, float vel);
int pulse_active(const pulse_t *p);
/* adds the sound into l and r (the caller clears them) */
void pulse_render(pulse_t *p, float *l, float *r, int n);
int pulse_nparams(void);
const param_t *pulse_param(int i);
void pulse_set(pulse_t *p, int i, int value);
int pulse_get(const pulse_t *p, int i);
