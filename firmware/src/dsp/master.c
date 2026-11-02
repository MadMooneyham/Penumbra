/* SPDX-License-Identifier: GPL-3.0-only */
/* The master: DIP, volume, limiter. (X0X's master, less what Penumbra never used: its compressor, the
 * kick-keyed PUMP, the band- and high-pass and the mono path.)
 *
 * DIP: Zavalishin's TPT state-variable filter as a low-pass (stable at any cutoff); the coefficient
 * glides so a fast sweep does not zipper (ReBirth's PCF idea: sweep the whole mix).
 * Limiter: instant-attack peak follower, ~150 ms release, then a tanh knee above -1 dBFS. */
#include "master.h"
#include "fastmath.h"

#define FS 44100.0f
#define DIP_RESO 20                   /* Q 2.3: a little lift at the cutoff */

/* the low-pass's coefficients for a cutoff 0..127 (30 Hz .. 18 kHz) */
static void filt_coefs(master_t *m, int cutoff)
{
    float fc = 30.0f * fm_exp2f((float)cutoff * (9.23f / 127.0f));
    float q = 0.5f + (float)DIP_RESO * (11.5f / 127.0f);
    m->g_t = fm_tanf(FM_PI * fm_minf(fc, 19000.0f) / FS);
    m->k = 1.0f / q;
    m->a1 = 1.0f / (1.0f + m->g * (m->g + m->k));       /* the resonance may have changed */
}

void master_init(master_t *m)
{
    int i;
    uint8_t *z = (uint8_t *)m;
    for (i = 0; i < (int)sizeof *m; i++)
        z[i] = 0;
    m->dip = 64;
    m->gain = 1.0f;
    m->vol_cur = -1.0f;                               /* unset: the first block takes the volume as it is */
    filt_coefs(m, 127);
    m->g = m->g_t;
    m->a1 = 1.0f / (1.0f + m->g * (m->g + m->k));
}

void master_dip(master_t *m, int v)
{
    m->dip = (uint8_t)(v < 0 ? 0 : v > 127 ? 127 : v);
    m->lp = m->dip < 60;
    filt_coefs(m, m->lp ? 8 + m->dip * 119 / 59 : 127);   /* right of 0, the cutoff falls to the left */
}

/* the limiter's last stage: a soft knee over 0.89 */
static inline float soft_ceiling(float s)
{
    if (s > 0.89f || s < -0.89f)
        s = s > 0.0f ? 0.89f + 0.11f * fm_tanhf((s - 0.89f) * 9.0f) : -0.89f - 0.11f * fm_tanhf((-s - 0.89f) * 9.0f);
    return s;
}

void master_process_st(master_t *m, float *xl, float *xr, int n, float volume)
{
    int i;
    float k = m->k, vc = m->vol_cur < 0.0f ? volume : m->vol_cur;
    for (i = 0; i < n; i++) {
        float s = xl[i], sr = xr[i];
        if (m->lp) {
            float g = m->g, a1;
            if (g != m->g_t) {                               /* the cutoff moving: its coefficient too */
                g = m->g += (m->g_t - m->g) * 0.0625f;
                if (fm_fabsf(m->g_t - g) < 1e-6f * m->g_t)
                    g = m->g = m->g_t;
                m->a1 = 1.0f / (1.0f + g * (g + k));
            }
            a1 = m->a1;
            float v3 = s - m->ic2;
            float v1 = a1 * m->ic1 + g * a1 * v3;
            float v2 = m->ic2 + g * v1;
            m->ic1 = fm_flush(2.0f * v1 - m->ic1);
            m->ic2 = fm_flush(2.0f * v2 - m->ic2);
            s = v2;
            v3 = sr - m->ic2r;
            v1 = a1 * m->ic1r + g * a1 * v3;
            v2 = m->ic2r + g * v1;
            m->ic1r = fm_flush(2.0f * v1 - m->ic1r);
            m->ic2r = fm_flush(2.0f * v2 - m->ic2r);
            sr = v2;
        }
        if (vc != volume) {                          /* the MASTER knob glides (~10 ms): no zipper */
            vc += (volume - vc) * 0.0022651f;
            if (fm_fabsf(volume - vc) < 1e-5f)
                vc = volume;
        }
        s *= vc;
        sr *= vc;
        {
            /* look ahead MST_LA samples: a peak is seen when it enters, the gain eases down over the
             * MST_LA samples it takes to come out, and is held for them. (It used to halve the
             * distance every sample: a step in the waveform, a click on every limited kick.) */
            float a = fm_maxf(fm_fabsf(s), fm_fabsf(sr)), d, dr;
            if (a >= m->env) {
                m->env = a;
                m->hold = MST_LA;
                /* a divide only for a new peak over the ceiling. The compiler computes the divide
                 * before the test, so it must be safe on its own: a clamp INSIDE the branch is folded
                 * away (a > 0.8 there) and 0.2-beta crashed here on 1 / 0 (firmware/hal/fm1_irq.h). */
                float r = 1.0f / fm_maxf(a, 0.8f);
                m->inv_env = a > 0.8f ? r : 0.0f;
            } else if (m->hold) {
                m->hold--;
            } else {
                m->env *= 0.99985f;
                m->inv_env *= 1.0001500225f;                 /* 1 / 0.99985 */
            }
            {
                float want = m->env > 0.8f ? 0.8f * m->inv_env : 1.0f;
                m->gain += (want - m->gain) * (want < m->gain ? (5.0f / MST_LA) : 0.002f);
            }
            d = m->la[m->la_pos];
            dr = m->la_r[m->la_pos];
            m->la[m->la_pos] = s;
            m->la_r[m->la_pos] = sr;
            m->la_pos = (m->la_pos + 1) % MST_LA;
            xl[i] = soft_ceiling(d * m->gain);
            xr[i] = soft_ceiling(dr * m->gain);
        }
    }
    m->vol_cur = vc;
}
