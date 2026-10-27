/* SPDX-License-Identifier: GPL-3.0-only */
/* The send reverb; see spacevb.h. */
#include "spacevb.h"
#include "fastmath.h"

#define SR 44100.0f
#define INV_SR (1.0f / 44100.0f)
#define CTRL 32
#define MOD_MAX 14.0f                     /* samples of modulation at MOD 1 (x2 peak to peak) */
#define PS_WIN 2048                       /* shimmer grain, samples */
#define Q 8192.0f                         /* line scale: int16 / Q */

static const float MS[SV_LINES] = {29.7f, 37.1f, 41.1f, 43.7f, 53.3f, 59.9f, 67.7f, 73.1f};

static void recompute(spacevb_t *v)
{
    float gs = 0.0f;
    int j;
    for (j = 0; j < SV_LINES; j++) {
        v->d[j] = MS[j] * v->size * (SR / 1000.0f);
        v->g[j] = fm_exp2f(-9.9657843f * v->d[j] / (v->t60 * SR));   /* 10^(-3 d / (T60 sr)) */
        gs += v->g[j];
    }
    /* the tank sustains like 1 / (1 - g): this keeps the shimmer loop under unity */
    v->shim_gain = v->shimmer * (1.0f - gs * (1.0f / SV_LINES)) * 4.0f;
}

void spacevb_decay(spacevb_t *v, float x) { v->t60 = fm_exp2f(5.32f * x); recompute(v); }
void spacevb_tone(spacevb_t *v, float x) { v->damp_c = 0.05f + 0.95f * x; }
void spacevb_lowcut(spacevb_t *v, float hz) { v->hp_c = fm_expf(-FM_TWO_PI * hz * INV_SR); }
void spacevb_level(spacevb_t *v, float x) { v->level = x; }
void spacevb_size(spacevb_t *v, float x) { v->size = 0.5f + fm_clampf(x, 0.0f, 1.0f); recompute(v); }
void spacevb_mod(spacevb_t *v, float x) { v->mod = x; }
void spacevb_shimmer(spacevb_t *v, float x) { v->shimmer = 0.9f * x; recompute(v); }

void spacevb_init(spacevb_t *v, int16_t *buf, int16_t *ps)
{
    uint8_t *z = (uint8_t *)v;
    int32_t j, at = 0, i;
    for (i = 0; i < (int32_t)sizeof *v; i++)
        z[i] = 0;
    v->buf = buf;
    v->ps = ps;
    for (j = 0; j < SV_LINES; j++) {
        v->cap[j] = (int32_t)(MS[j] * SV_SIZE_MAX * (SR / 1000.0f)) + 2 * (int32_t)MOD_MAX + 38;
        v->base[j] = at;
        at += v->cap[j];
    }
    if (at > SV_TOTAL)                                   /* (SV_TOTAL is sized for these lines) */
        v->buf = 0;
    if (buf)
        for (i = 0; i < SV_TOTAL; i++)
            buf[i] = 0;
    if (ps)
        for (i = 0; i < SV_PS_LEN; i++)
            ps[i] = 0;
    v->t60 = 14.0f;
    v->size = 1.2f;
    v->shimmer = 0.18f;
    recompute(v);
    spacevb_tone(v, 0.5f);
    spacevb_lowcut(v, 120.0f);
    v->mod = 0.5f;
    v->level = 1.0f;
    v->quiet = SV_TOTAL + 1;
}

static inline float rd(const int16_t *b, int32_t cap, int32_t w, float dl)
{
    float rp = (float)w - dl, fr;
    int32_t i0, i1;
    if (rp < 0.0f)
        rp += (float)cap;
    i0 = (int32_t)rp;
    fr = rp - (float)i0;
    if (i0 >= cap)
        i0 -= cap;
    i1 = i0 + 1 >= cap ? 0 : i0 + 1;
    return ((float)b[i0] + ((float)b[i1] - (float)b[i0]) * fr) * (1.0f / Q);
}

static inline int16_t q16(float x, int32_t *loud)
{
    int32_t k = (int32_t)(x * Q);
    if (k > 32767) k = 32767;
    if (k < -32767) k = -32767;
    *loud |= k > 1 || k < -1;
    return (int16_t)k;
}

void spacevb_process(spacevb_t *v, const float *in, float *out_l, float *out_r, int n)
{
    int i, j;
    float y[SV_LINES];
    if (!v->buf || !v->ps)
        return;
    if (v->quiet > SV_TOTAL) {                           /* silent lines: only a send wakes it */
        int any = 0;
        for (i = 0; i < n; i++)
            any |= in[i] != 0.0f;
        if (!any)
            return;
    }
    for (i = 0; i < n; i++) {
        float x, xin, ol, orr, sh = 0.0f;
        int32_t loud = 0, h;
        if (v->ctrl_left <= 0) {                         /* the moving lines, at the control rate */
            const float md = v->mod * MOD_MAX;
            v->mod_ph += 0.31f * (float)CTRL * INV_SR;
            if (v->mod_ph >= 1.0f) v->mod_ph -= 1.0f;
            for (j = 0; j < SV_LINES; j++)
                v->dmod[j] = v->d[j] + (j < 4 ? md * (1.0f + fm_sin_turns(v->mod_ph + 0.27f * (float)j)) : 0.0f);
            v->ctrl_left = CTRL;
        }
        v->ctrl_left--;
        x = in[i];
        v->hp_y1 = v->hp_c * (v->hp_y1 + x - v->hp_x1);   /* the send's low cut */
        v->hp_x1 = x;
        xin = v->hp_y1 + fm_tanhf(v->shim_out * v->shim_gain);
        for (j = 0; j < SV_LINES; j++) {
            const float r = rd(v->buf + v->base[j], v->cap[j], v->w[j], v->dmod[j]);
            v->lp[j] += v->damp_c * (r - v->lp[j]);
            y[j] = v->lp[j];
        }
        ol = (y[0] - y[2] + y[4] - y[6]) * 0.35f;
        orr = (y[1] - y[3] + y[5] - y[7]) * 0.35f;
        for (j = 0; j < SV_LINES; j++)
            y[j] *= v->g[j];
        for (h = 1; h < SV_LINES; h <<= 1) {             /* Hadamard: three butterfly stages */
            int a, b;
            for (a = 0; a < SV_LINES; a += h << 1)
                for (b = a; b < a + h; b++) {
                    const float u = y[b], w = y[b + h];
                    y[b] = u + w;
                    y[b + h] = u - w;
                }
        }
        for (j = 0; j < SV_LINES; j++) {
            int32_t p = v->w[j];
            v->buf[v->base[j] + p] = q16(y[j] * 0.35355339f + ((j & 1) ? -xin : xin), &loud);
            v->w[j] = p + 1 >= v->cap[j] ? 0 : p + 1;
        }
        /* shimmer: two grains reading an octave up, crossfaded by sin^2 (they sum to one) */
        v->ps[v->ps_w] = q16((ol + orr) * 0.5f, &loud);
        v->ps_ph += 1.0f / (float)PS_WIN;
        if (v->ps_ph >= 1.0f) v->ps_ph -= 1.0f;
        for (h = 0; h < 2; h++) {
            float ph = v->ps_ph + 0.5f * (float)h, wv;
            if (ph >= 1.0f) ph -= 1.0f;
            wv = fm_sin_turns(ph * 0.5f);
            sh += rd(v->ps, SV_PS_LEN, v->ps_w, (1.0f - ph) * (float)PS_WIN + 2.0f) * wv * wv;
        }
        v->shim_out = sh;
        v->ps_w = (v->ps_w + 1) & (SV_PS_LEN - 1);
        out_l[i] += ol * v->level;
        out_r[i] += orr * v->level;
        v->quiet = (!loud && x == 0.0f) ? v->quiet + 1 : 0;
    }
    for (j = 0; j < SV_LINES; j++)
        v->lp[j] = fm_flush(v->lp[j]);
    v->hp_y1 = fm_flush(v->hp_y1);
}
