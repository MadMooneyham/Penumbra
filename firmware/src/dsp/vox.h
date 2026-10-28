/* SPDX-License-Identifier: GPL-3.0-only */
/* VOX: Penumbra's played voice, a four-voice oscillator synth in the Solar 42's manner: two
 * band-limited saws a voice (the second detuned, or an octave away), a clean 12 dB low-pass whose
 * cutoff the envelope opens and the key follows, a slow ADSR, one gentle vibrato for all, the voices
 * spread across the field.
 *
 * Notes take a free voice, else the one released longest ago, else the oldest held. A taken voice
 * carries on from its level and its phases (its envelope starts its attack from where it is), so a
 * steal never clicks. A silent voice costs nothing.
 *
 * Stereo, 44.1 kHz, float, no libm, no allocation. vox_note_on / off and vox_set between render
 * blocks, from the context that renders. */
#pragma once
#include <stdint.h>
#include "param.h"

#define VOX_VOICES 4
#define VOX_CTRL 16                       /* control rate: every 16 samples */
#define VOX_MAX_BLOCK 256

enum { VX_DETUNE, VX_OCTB, VX_MIXB, VX_CUTOFF, VX_RESO, VX_ENVAMT, VX_KEYTRK, VX_ATTACK, VX_DECAY,
       VX_SUSTAIN, VX_RELEASE, VX_VIB, VX_SPREAD, VX_LEVEL, VX_NPARAMS };

typedef struct {
    int32_t note;                         /* MIDI note, -1 never used */
    int32_t gate;                         /* held */
    uint32_t age;                         /* when it was taken (gate on) or let go (gate off) */
    float ph[2], inc[2];                  /* the saws, turns */
    float env;                            /* 0 .. 1 */
    int32_t stage;                        /* 0 attack, 1 decay / sustain, 2 release */
    float ic1, ic2, a1, a2, a3, k;        /* TPT SVF */
    float pan_l, pan_r;
    float vel;
} vox_voice_t;

typedef struct {
    uint8_t pot[VX_NPARAMS];
    float det, octb, mixb, cutoff, reso, envamt, keytrk, atk_c, dec_c, sus, rel_c, vib, spread, level;
    vox_voice_t v[VOX_VOICES];
    float vib_ph;
    uint32_t clock;                       /* notes so far, for the voices' ages */
    int32_t ctrl_left;
} vox_t;

void vox_init(vox_t *x);
void vox_note_on(vox_t *x, int note, float vel);
void vox_note_off(vox_t *x, int note);
void vox_all_off(vox_t *x);
int vox_active(const vox_t *x);           /* any voice sounding */
int vox_held(const vox_t *x, int note);   /* (tests) a voice holds the note */
/* adds the voices into l and r (the caller clears them) */
void vox_render(vox_t *x, float *l, float *r, int n);
int vox_nparams(void);
const param_t *vox_param(int i);
void vox_set(vox_t *x, int i, int value);
int vox_get(const vox_t *x, int i);
