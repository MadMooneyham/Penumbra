/* SPDX-License-Identifier: GPL-3.0-only */
/* MELODY: the Solar-style lead voice of the browser prototype (drone-proto/drone.html); in the
 * rewrite it stands in for LINE until phase 2 (note on with accent and slide, note off).
 *
 * One mono voice: a saw <-> square oscillator with a sub-octave square, glide, vibrato and a
 * ring modulator, through a saturating low-pass SVF swept by an ADSR, out through the same
 * ADSR. A note that arrives while the last one still sounds glides to it (GLIDE; 0 = jump);
 * a slide step glides as well, and holds the envelope instead of restarting it.
 *
 * Mono, 44.1 kHz, float, no libm, no allocation. Every instance is self-contained. */
#pragma once
#include <stdint.h>
#include "param.h"

#define MELODY_CTRL 16                    /* control rate: every 16 samples */
#define MELODY_MAX_BLOCK 256

/* pots: pages of four, in order */
enum {
    MEL_CUTOFF, MEL_RESO, MEL_ENVMOD, MEL_ACCENT,
    MEL_ATTACK, MEL_DECAY, MEL_SUSTAIN, MEL_RELEASE,
    MEL_WAVE, MEL_SUB, MEL_GLIDE, MEL_VIB,
    MEL_RING, MEL_RINGHZ, MEL_TUNE, MEL_VOLUME,
    MEL_NPARAMS
};

typedef struct melody {
    uint8_t pot[MEL_NPARAMS];
    /* derived from the pots */
    float cutoff, reso, envmod, accent_amt;
    float atk_inc, dec_c, sus, rel_c;
    float wave, sub, glide_c, vib_c, ring, ring_inc, tune, volume;
    /* voice */
    int gate;                             /* a note is held */
    int idle;                             /* nothing sounding: render writes zeros */
    int stage;                            /* 0 attack, 1 decay / sustain */
    float env, acc;                       /* envelope; this note's accent (0 or ACCENT) */
    float note, note_tgt;                 /* pitch now and where it glides, MIDI notes */
    float ph, sub_ph, vib_ph, ring_ph;    /* turns */
    float inc;                            /* this control step's oscillator increment */
    float a1, a2, a3, ic1, ic2;           /* SVF */
    int ctrl_left;
} melody_t;

void melody_init(melody_t *m);
void melody_note_on(melody_t *m, int note, int accent, int slide);
void melody_note_off(melody_t *m);
void melody_all_off(melody_t *m);
void melody_render(melody_t *m, float *out, int n);    /* writes n samples */
int melody_nparams(void);
const param_t *melody_param(int i);
void melody_set(melody_t *m, int i, int value);
int melody_get(const melody_t *m, int i);
