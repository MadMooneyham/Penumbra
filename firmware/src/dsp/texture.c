/* SPDX-License-Identifier: GPL-3.0-only */
/* TEXTURE; see texture.h. */
#include "texture.h"
#include "fastmath.h"

#define SR 44100.0f
#define INV_SR (1.0f / 44100.0f)
#define TX_K 0.55f                               /* the band-pass's damping: a breathy, wide band */
#define TX_QUIET 1e-5f

static const param_t TEXTURE_P[TX_NPARAMS] = {
    [TX_LEVEL] = {"Level", 127, 0, 0},   [TX_COLOUR] = {"Colour", 127, 60, 0},
    [TX_SWEEP] = {"Sweep", 127, 70, 0},  [TX_RATE] = {"Rate", 127, 40, 0},
    [TX_WIDTH] = {"Width", 127, 90, 0}};

int texture_nparams(void) { return TX_NPARAMS; }
const param_t *texture_param(int i) { return i >= 0 && i < TX_NPARAMS ? &TEXTURE_P[i] : 0; }
int texture_get(const texture_t *t, int i) { return i >= 0 && i < TX_NPARAMS ? t->pot[i] : 0; }

static float pot01(int v) { return (float)v * (1.0f / 127.0f); }

void texture_set(texture_t *t, int i, int value)
{
    const param_t *d = texture_param(i);
    if (!d)
        return;
    value = value < 0 ? 0 : value > d->max ? d->max : value;
    t->pot[i] = (uint8_t)value;
    switch (i) {
    case TX_LEVEL: t->level_tgt = pot01(value) * 0.4f; break;   /* linear: 127 about -26 dBFS rms, 30 about -39 */
    case TX_COLOUR: t->colour = 120.0f * fm_exp2f(5.0f * pot01(value)); break;  /* 120 Hz .. 3.8 kHz */
    case TX_SWEEP: t->sweep = 2.0f * pot01(value); break;                       /* +-2 octaves */
    case TX_RATE: t->rate_inc = 0.005f * fm_exp2f(5.0f * pot01(value)) * (float)TEXTURE_CTRL * INV_SR; break;   /* 200 s .. 6 s a turn */
    case TX_WIDTH: t->width = pot01(value); break;
    default: break;
    }
}

void texture_init(texture_t *t)
{
    uint8_t *z = (uint8_t *)t;
    uint32_t i;
    int k;
    for (i = 0; i < sizeof *t; i++)
        z[i] = 0;
    t->rng[0] = 0x1B873593u;
    t->rng[1] = 0xCC9E2D51u;
    t->wrng = 0x85EBCA6Bu;
    for (k = 0; k < TX_NPARAMS; k++)
        texture_set(t, k, TEXTURE_P[k].def);
}

int texture_active(const texture_t *t) { return t->level_tgt > 0.0f || t->level > TX_QUIET; }

static uint32_t xs(uint32_t *s)
{
    uint32_t x = *s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return *s = x;
}
static float noise(uint32_t *s) { return (float)(int32_t)xs(s) * (1.0f / 2147483648.0f); }

static void control(texture_t *t)
{
    int s;
    t->ph += t->rate_inc;
    if (t->ph >= 1.0f)
        t->ph -= 1.0f;
    for (s = 0; s < 2; s++) {
        float off = s ? 0.5f * t->width : 0.0f, oct, fc, g;
        if ((xs(&t->wrng) & 1023u) < 6u)         /* the walk: a new heading every few seconds */
            t->walk_tgt[s] = noise(&t->wrng);
        t->walk[s] += (t->walk_tgt[s] - t->walk[s]) * 0.004f;
        oct = t->sweep * (0.65f * fm_sin_turns(t->ph + off) + 0.35f * t->walk[s] * (s ? t->width : 1.0f));
        fc = fm_clampf(t->colour * fm_exp2f(oct), 40.0f, 9000.0f);
        g = fm_tanf(FM_PI * fc * INV_SR);
        t->a1[s] = 1.0f / (1.0f + g * (g + TX_K));
        t->a2[s] = g * t->a1[s];
        t->norm[s] = TX_K * fm_rsqrtf(fc * TX_K * (1.0f / 600.0f));   /* noise through a band: as loud at any colour */
    }
}

void texture_render(texture_t *t, float *l, float *r, int n)
{
    int done = 0;
    if (!texture_active(t))
        return;
    while (done < n) {
        int k, i, s;
        if (t->ctrl_left <= 0) {
            control(t);
            t->ctrl_left = TEXTURE_CTRL;
        }
        k = n - done < t->ctrl_left ? n - done : t->ctrl_left;
        for (s = 0; s < 2; s++) {
            float p = t->pink[s], ic1 = t->ic1[s], ic2 = t->ic2[s], lv = t->level;
            const float a1 = t->a1[s], a2 = t->a2[s], g = a2 / a1, nm = t->norm[s], lt = t->level_tgt;
            float *o = s ? r + done : l + done;
            for (i = 0; i < k; i++) {
                float w = noise(&t->rng[s]), v3, v1, v2;
                p += 0.25f * (w - p);                /* soften the hiss: the wind, not the static */
                v3 = p + 0.3f * w - ic2;
                v1 = a1 * ic1 + a2 * v3;
                v2 = ic2 + g * v1;
                ic1 = 2.0f * v1 - ic1;
                ic2 = 2.0f * v2 - ic2;
                lv += (lt - lv) * 0.00008f;          /* ~0.3 s */
                o[i] += v1 * nm * lv;
            }
            t->pink[s] = p;
            t->ic1[s] = fm_flush(ic1);
            t->ic2[s] = fm_flush(ic2);
            if (s)
                t->level = lv < TX_QUIET && lt == 0.0f ? 0.0f : lv;
        }
        {   /* WIDTH 0: the two winds meet in the middle */
            const float m = 0.5f * (1.0f - t->width);
            if (m > 0.0f)
                for (i = 0; i < k; i++) {
                    float a = l[done + i], b = r[done + i];
                    l[done + i] = a + (b - a) * m;
                    r[done + i] = b + (a - b) * m;
                }
        }
        t->ctrl_left -= k;
        done += k;
    }
}
