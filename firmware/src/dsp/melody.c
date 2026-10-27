/* SPDX-License-Identifier: GPL-3.0-only */
/* MELODY part; see melody.h. */
#include "melody.h"
#include "fastmath.h"

#define SR 44100.0f
#define INV_SR (1.0f / 44100.0f)
#define VIB_HZ 5.2f

static const char *const TUNE_N[25] = {"-12", "-11", "-10", "-9", "-8", "-7", "-6", "-5", "-4", "-3", "-2", "-1",
                                       "0", "+1", "+2", "+3", "+4", "+5", "+6", "+7", "+8", "+9", "+10", "+11",
                                       "+12"};

static const x0x_param_t MEL_P[MEL_NPARAMS] = {
    [MEL_CUTOFF] = {"Cutoff", 127, 84, 0},  [MEL_RESO] = {"Reso", 127, 40, 0},
    [MEL_ENVMOD] = {"EnvMod", 127, 32, 0},  [MEL_ACCENT] = {"Accent", 127, 64, 0},
    [MEL_ATTACK] = {"Attack", 127, 50, 0},  [MEL_DECAY] = {"Decay", 127, 91, 0},
    [MEL_SUSTAIN] = {"Sus", 127, 70, 0},    [MEL_RELEASE] = {"Rel", 127, 100, 0},
    [MEL_WAVE] = {"Wave", 127, 25, 0},      [MEL_SUB] = {"Sub", 127, 25, 0},
    [MEL_GLIDE] = {"Glide", 127, 36, 0},    [MEL_VIB] = {"Vib", 127, 13, 0},
    [MEL_RING] = {"Ring", 127, 0, 0},       [MEL_RINGHZ] = {"RingHz", 127, 74, 0},
    [MEL_TUNE] = {"Tune", 24, 12, TUNE_N},  [MEL_VOLUME] = {"Volume", 127, 100, 0}};

int melody_nparams(void) { return MEL_NPARAMS; }
const x0x_param_t *melody_param(int i) { return i >= 0 && i < MEL_NPARAMS ? &MEL_P[i] : 0; }
int melody_get(const melody_t *m, int i) { return i >= 0 && i < MEL_NPARAMS ? m->pot[i] : 0; }

static float pot_exp(int pot, float lo, float oct) { return lo * fm_exp2f(oct * (float)pot * (1.0f / 127.0f)); }
static float pot_lin(int pot) { return (float)pot * (1.0f / 127.0f); }
static float one_pole(float seconds) { return 1.0f - fm_expf(-1.0f / (seconds * SR)); }

void melody_set(melody_t *m, int i, int value)
{
    float x;
    if (i < 0 || i >= MEL_NPARAMS)
        return;
    if (value < 0) value = 0;
    if (value > MEL_P[i].max) value = MEL_P[i].max;
    m->pot[i] = (uint8_t)value;
    x = pot_lin(value);
    switch (i) {
    case MEL_CUTOFF: m->cutoff = pot_exp(value, 80.0f, 7.23f); break;
    case MEL_RESO: m->reso = x * 0.95f; break;
    case MEL_ENVMOD: m->envmod = 6.0f * x; break;
    case MEL_ACCENT: m->accent_amt = x; break;
    case MEL_ATTACK: m->atk_inc = 1.0f / (pot_exp(value, 0.002f, 11.0f) * SR); break;
    case MEL_DECAY: m->dec_c = one_pole(pot_exp(value, 0.02f, 8.23f)); break;
    case MEL_SUSTAIN: m->sus = x; break;
    case MEL_RELEASE: m->rel_c = one_pole(pot_exp(value, 0.02f, 8.64f)); break;
    case MEL_WAVE: m->wave = x; break;
    case MEL_SUB: m->sub = x; break;
    case MEL_GLIDE:                                /* 0 .. 1.5 s, square law: fine at the short end */
        m->glide_c = value ? 1.0f - fm_expf(-(float)MELODY_CTRL / (1.5f * x * x * SR)) : 1.0f;
        break;
    case MEL_VIB: m->vib_c = 60.0f * x; break;
    case MEL_RING: m->ring = x; break;
    case MEL_RINGHZ: m->ring_inc = pot_exp(value, 10.0f, 7.64f) * INV_SR; break;
    case MEL_TUNE: m->tune = (float)(value - 12); break;
    case MEL_VOLUME: {
        float v = (float)value * (1.0f / 100.0f);
        m->volume = v * v;
        break;
    }
    default: break;
    }
}

void melody_init(melody_t *m)
{
    uint8_t *z = (uint8_t *)m;
    int i;
    for (i = 0; i < (int)sizeof *m; i++)
        z[i] = 0;
    for (i = 0; i < MEL_NPARAMS; i++)
        melody_set(m, i, MEL_P[i].def);
    m->idle = 1;
    m->note = m->note_tgt = 57.0f;
}

void melody_note_on(melody_t *m, int note, int accent, int slide)
{
    const float tgt = (float)note + m->tune;
    const int sounding = m->gate || m->env > 0.002f;
    if (!sounding)
        m->note = tgt;                             /* from silence: no glide in */
    m->note_tgt = tgt;
    m->acc = accent ? m->accent_amt : 0.0f;
    if (!(slide && m->gate))
        m->stage = 0;                              /* a slide holds the envelope; else it attacks again */
    m->gate = 1;
    m->idle = 0;
    m->ctrl_left = 0;
}

void melody_note_off(melody_t *m) { m->gate = 0; }

void melody_all_off(melody_t *m)
{
    m->gate = 0;
    m->env = 0.0f;
    m->idle = 1;
}

static void control(melody_t *m)
{
    float f, fc, g, k;
    m->note += (m->note_tgt - m->note) * m->glide_c;
    m->vib_ph += VIB_HZ * (float)MELODY_CTRL * INV_SR;
    if (m->vib_ph >= 1.0f) m->vib_ph -= 1.0f;
    f = m->note + fm_sin_turns(m->vib_ph) * m->vib_c * 0.01f;
    m->inc = 440.0f * fm_exp2f((f - 69.0f) * (1.0f / 12.0f)) * INV_SR;
    fc = m->cutoff * fm_exp2f(m->env * m->envmod * (1.0f + m->acc));
    fc = fm_clampf(fc, 20.0f, SR * 0.45f);
    g = fm_tanf(FM_PI * fc * INV_SR);
    k = 2.0f - 2.0f * m->reso;
    m->a1 = 1.0f / (1.0f + g * (g + k));
    m->a2 = g * m->a1;
    m->a3 = g * m->a2;
}

static inline float blep(float t, float dt)
{
    if (t < dt) {
        t /= dt;
        return t + t - t * t - 1.0f;
    }
    if (t > 1.0f - dt) {
        t = (t - 1.0f) / dt;
        return t * t + t + t + 1.0f;
    }
    return 0.0f;
}

static void chunk(melody_t *m, float *out, int n)
{
    int i;
    const float dt = m->inc, sdt = dt * 0.5f, gain = m->volume * 0.35f * (1.0f + 0.5f * m->acc);
    for (i = 0; i < n; i++) {
        float t = m->ph, t2 = t + 0.5f, s = m->sub_ph, s2 = s + 0.5f, sw, sq, sub, y, v3, v1, v2;
        if (m->gate) {
            if (m->stage == 0) {
                m->env += m->atk_inc;
                if (m->env >= 1.0f) {
                    m->env = 1.0f;
                    m->stage = 1;
                }
            } else
                m->env += (m->sus - m->env) * m->dec_c;
        } else
            m->env -= m->env * m->rel_c;
        if (t2 >= 1.0f) t2 -= 1.0f;
        if (s2 >= 1.0f) s2 -= 1.0f;
        sw = 2.0f * t - 1.0f - blep(t, dt);
        sq = (t < 0.5f ? 1.0f : -1.0f) + blep(t, dt) - blep(t2, dt);
        sub = (s < 0.5f ? 1.0f : -1.0f) + blep(s, sdt) - blep(s2, sdt);
        y = sw + m->wave * (sq - sw) + m->sub * sub;
        t += dt;
        if (t >= 1.0f) t -= 1.0f;
        s += sdt;
        if (s >= 1.0f) s -= 1.0f;
        m->ph = t;
        m->sub_ph = s;
        y = fm_tanhf(y * 0.8f);                     /* the SVF, band state soft-clipped */
        v3 = y - m->ic2;
        v1 = m->a1 * m->ic1 + m->a2 * v3;
        v2 = m->ic2 + m->a2 * m->ic1 + m->a3 * v3;
        m->ic1 = fm_tanhf(2.0f * v1 - m->ic1);
        m->ic2 = 2.0f * v2 - m->ic2;
        y = v2;
        if (m->ring > 0.0f) {
            m->ring_ph += m->ring_inc;
            if (m->ring_ph >= 1.0f) m->ring_ph -= 1.0f;
            y = y * (1.0f - m->ring) + y * fm_sin_turns(m->ring_ph) * m->ring * 1.4f;
        }
        out[i] = y * m->env * gain;
    }
    m->env = fm_flush(m->env);
    m->ic1 = fm_flush(m->ic1);
    m->ic2 = fm_flush(m->ic2);
}

void melody_render(melody_t *m, float *out, int n)
{
    int done = 0, i;
    if (m->idle) {
        for (i = 0; i < n; i++)
            out[i] = 0.0f;
        return;
    }
    while (done < n) {
        int k;
        if (m->ctrl_left <= 0) {
            control(m);
            m->ctrl_left = MELODY_CTRL;
        }
        k = n - done < m->ctrl_left ? n - done : m->ctrl_left;
        chunk(m, out + done, k);
        m->ctrl_left -= k;
        done += k;
    }
    if (!m->gate && m->env < 1e-4f) {              /* faded out: stop computing */
        m->env = 0.0f;
        m->idle = 1;
    }
}
