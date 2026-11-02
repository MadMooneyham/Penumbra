/* SPDX-License-Identifier: GPL-3.0-only */
/* The effects' building blocks (fxbus.c), ported from 9W9 (er99 webaudio.h / er99_circuit.h, GPL-3.0)
 * to the FM-1: float only, no libm, no double, all static inline. The d9_ prefix keeps their origin.
 *
 * What changed against 9W9 and why, so a reader can check the numbers:
 *  - Oscillator phase: 9W9 keeps a double; here a uint32 turn (2^32 = one cycle),
 *    which is exact modulo the increment's float rounding.
 *  - tanh: a 1025-point table with linear interpolation (abs error < 6e-6),
 *    generated offline (tools/gen_dsp_tables.py); the tape delay's saturation reads it.
 *  - Biquad coefficients: cos(w0) as 1 - 2 sin^2(w0/2), which keeps the
 *    low-frequency filters (20 Hz DC blocker) as accurate as libm's cosf. */
#pragma once
#include <stdint.h>
#include "fastmath.h"
#include "dsp_tables.h"

#define D9_SR 44100.0f
#define D9_MS (0.001f * D9_SR)                 /* samples per ms, as 9W9's `ms` */
#define D9_PHASE_PER_HZ 97391.548752834467f   /* 2^32 / 44100 */

/* ===================================================================== */
/* Biquad (Web Audio / RBJ), direct form 1 as 9W9                         */
/* ===================================================================== */
enum { D9_LP = 0, D9_HP, D9_BP };

typedef struct {
    float b0, b1, b2, a1, a2;
    float x1, x2, y1, y2;
} d9_biquad_t;

static inline void d9_biquad_set(d9_biquad_t *f, int type, float freq, float q)
{
    float fc = freq / (D9_SR * 0.5f);
    if (fc >= 1.0f) fc = 0.9999f;
    if (fc <= 0.0f) fc = 0.0001f;
    const float w0 = FM_PI * fc;
    const float sh = fm_sinf(0.5f * w0);
    const float cosw0 = 1.0f - 2.0f * sh * sh;
    const float sinw0 = fm_sinf(w0);
    if (q < 0.0001f) q = 0.0001f;
    const float alpha = sinw0 / (2.0f * q);
    float b0, b1, b2;
    const float a0 = 1.0f + alpha, a1 = -2.0f * cosw0, a2 = 1.0f - alpha;
    if (type == D9_LP) {
        b0 = (1.0f - cosw0) * 0.5f; b1 = 1.0f - cosw0; b2 = (1.0f - cosw0) * 0.5f;
    } else if (type == D9_HP) {
        b0 = (1.0f + cosw0) * 0.5f; b1 = -(1.0f + cosw0); b2 = (1.0f + cosw0) * 0.5f;
    } else {
        b0 = alpha; b1 = 0.0f; b2 = -alpha;
    }
    const float inv = 1.0f / a0;
    f->b0 = b0 * inv; f->b1 = b1 * inv; f->b2 = b2 * inv;
    f->a1 = a1 * inv; f->a2 = a2 * inv;
}

static inline void d9_biquad_reset(d9_biquad_t *f) { f->x1 = f->x2 = f->y1 = f->y2 = 0.0f; }

static inline float d9_biquad_tick(d9_biquad_t *f, float in)
{
    const float y = f->b0 * in + f->b1 * f->x1 + f->b2 * f->x2 - f->a1 * f->y1 - f->a2 * f->y2;
    f->x2 = f->x1; f->x1 = in;
    f->y2 = f->y1; f->y1 = y;
    return y;
}

/* ===================================================================== */
/* A uint32 phase increment (2^32 = one cycle)                            */
/* ===================================================================== */
static inline uint32_t d9_inc(float hz) { return (uint32_t)(hz * D9_PHASE_PER_HZ); }

/* ===================================================================== */
/* tanh from the table                                                    */
/* ===================================================================== */
static inline float d9_tanh(float u)
{
    const float a = fm_fabsf(u) * DSP_TANH_SCALE;
    float t;
    if (a >= (float)DSP_TANH_N) {
        t = 1.0f;
    } else {
        const int32_t i = (int32_t)a;
        const float fr = a - (float)i;
        t = dsp_tanh_tab[i] + (dsp_tanh_tab[i + 1] - dsp_tanh_tab[i]) * fr;
    }
    return u < 0.0f ? -t : t;
}

