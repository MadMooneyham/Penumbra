/* SPDX-License-Identifier: GPL-3.0-only */
/* TEXTURE: Penumbra's bed of space wind, always under the rest.
 *
 * Two noises (one a side, pinked), each through a band-pass whose centre (COLOUR) sweeps slowly:
 * a sine (RATE) and a random walk a side, SWEEP octaves either way, so the wind rises and falls and
 * never repeats. WIDTH moves the sides' sweeps apart (0: one wind in the middle). LEVEL glides
 * (~0.3 s), so the scene can bring it in and take it away. Silent, it costs nothing.
 *
 * Stereo, 44.1 kHz, float, no libm, no allocation. texture_set between render blocks, from the
 * context that renders. */
#pragma once
#include <stdint.h>
#include "param.h"

#define TEXTURE_CTRL 32                   /* control rate: every 32 samples */

enum { TX_LEVEL, TX_COLOUR, TX_SWEEP, TX_RATE, TX_WIDTH, TX_NPARAMS };

typedef struct {
    uint8_t pot[TX_NPARAMS];
    float level_tgt, level, colour, sweep, rate_inc, width;
    float ph, walk[2], walk_tgt[2];
    float pink[2], ic1[2], ic2[2];        /* the noise's one pole, each side's SVF */
    float a1[2], a2[2], norm[2];          /* this control step's band-pass, and its loudness */
    uint32_t rng[2], wrng;
    int32_t ctrl_left;
} texture_t;

void texture_init(texture_t *t);
int texture_active(const texture_t *t);
/* adds the wind into l and r (the caller clears them) */
void texture_render(texture_t *t, float *l, float *r, int n);
int texture_nparams(void);
const param_t *texture_param(int i);
void texture_set(texture_t *t, int i, int value);
int texture_get(const texture_t *t, int i);
