/* SPDX-License-Identifier: GPL-3.0-only */
/* PULSE; see pulse.h. */
#include "pulse.h"
#include "fastmath.h"

#define SR 44100.0f
#define INV_SR (1.0f / 44100.0f)

static const param_t PULSE_P[PL_NPARAMS] = {
    [PL_TUNE] = {"Tune", 24, 12, 0},         [PL_PITCH] = {"Pitch", 127, 50, 0},
    [PL_PDECAY] = {"P.Dcy", 127, 64, 0},     [PL_DECAY] = {"Decay", 127, 80, 0},
    [PL_CLICK] = {"Click", 127, 30, 0},      [PL_DRIVE] = {"Drive", 127, 20, 0},
    [PL_LEVEL] = {"Level", 127, 100, 0},     [PL_BLOOM] = {"Bloom", 127, 90, 0},
    [PL_BLEN] = {"B.Len", 127, 64, 0},       [PL_BHEIGHT] = {"B.Hgt", 2, 1, 0},
    [PL_BFIFTH] = {"B.5th", 127, 64, 0}};

int pulse_nparams(void) { return PL_NPARAMS; }
const param_t *pulse_param(int i) { return i >= 0 && i < PL_NPARAMS ? &PULSE_P[i] : 0; }
int pulse_get(const pulse_t *p, int i) { return i >= 0 && i < PL_NPARAMS ? p->pot[i] : 0; }

static float pot01(int v) { return (float)v * (1.0f / 127.0f); }
static float one_pole(float seconds) { return 1.0f - fm_expf(-1.0f / (seconds * SR)); }
static float pot_exp(int v, float lo, float octaves) { return lo * fm_exp2f(octaves * pot01(v)); }

static void retune(pulse_t *p)
{
    float note = p->root + (float)((int)p->pot[PL_TUNE] - 12);
    p->base_inc = 440.0f * fm_exp2f((note - 69.0f) * (1.0f / 12.0f)) * INV_SR;
}

void pulse_set(pulse_t *p, int i, int value)
{
    const param_t *d = pulse_param(i);
    if (!d)
        return;
    value = value < 0 ? 0 : value > d->max ? d->max : value;
    p->pot[i] = (uint8_t)value;
    switch (i) {
    case PL_TUNE: retune(p); break;
    case PL_PITCH: p->pitch_r = fm_exp2f(4.0f * pot01(value)) - 1.0f; break;     /* 0 .. 4 octaves over the note */
    case PL_PDECAY: p->pdec_c = one_pole(pot_exp(value, 0.005f, 5.32f)); break;  /* 5 .. 200 ms */
    case PL_DECAY: p->dec_c = one_pole(pot_exp(value, 0.05f, 5.9f)); break;      /* 50 ms .. 3 s */
    case PL_CLICK: p->click = pot01(value) * 0.5f; break;
    case PL_DRIVE:
        p->drive = 1.0f + 3.0f * pot01(value);
        p->drive_n = 1.0f / fm_tanhf(p->drive);
        break;
    case PL_LEVEL: p->level = pot01(value) * pot01(value) * 1.6129f; break;      /* 100 = unity */
    case PL_BLOOM: p->bloom = pot01(value) * pot01(value) * 0.45f; break;
    case PL_BLEN:                                                                  /* in 1 .. 4 s, out 3 .. 14 s */
        p->brise_c = one_pole(pot_exp(value, 0.6f, 1.7f));
        p->bfall_c = one_pole(pot_exp(value, 1.0f, 2.2f));
        break;
    case PL_BHEIGHT: p->bmul = (float)(1 << value); break;                       /* the root, an octave, two */
    case PL_BFIFTH: p->bfifth = pot01(value) * 0.7f; break;                       /* (64: 0.35, as made) */
    default: break;
    }
}

void pulse_init(pulse_t *p)
{
    uint8_t *z = (uint8_t *)p;
    uint32_t i;
    int k;
    for (i = 0; i < sizeof *p; i++)
        z[i] = 0;
    p->root = 33.0f;                             /* A1 */
    p->rng = 0x6C8E9CF5u;
    for (k = 0; k < PL_NPARAMS; k++)
        pulse_set(p, k, PULSE_P[k].def);
}

void pulse_root(pulse_t *p, int midi_note)
{
    p->root = (float)midi_note;
    retune(p);
}

void pulse_hit(pulse_t *p, int which, float vel)
{
    vel = vel < 0.0f ? 0.0f : vel > 1.0f ? 1.0f : vel;
    if (which == PULSE_BLOOM) {                  /* (from where it is: a bloom over a bloom grows) */
        p->bvel = vel > p->bvel || p->benv < 0.05f ? vel : p->bvel;
        p->brise = 1;
        return;
    }
    p->vel = vel;                                /* the phase carries on: no click on a sounding kick */
    p->amp_tgt = vel;
    p->rise = 1;
    p->penv = 1.0f;
    p->cenv = 1.0f;
}

/* idle: 70 dB down (about -80 dBFS out, under any tail) */
#define PL_QUIET 3e-4f
int pulse_blooming(const pulse_t *p) { return p->brise || p->benv > PL_QUIET; }
int pulse_active(const pulse_t *p) { return p->amp > PL_QUIET || p->rise || p->cenv > PL_QUIET || pulse_blooming(p); }

static float noise(uint32_t *s)
{
    uint32_t x = *s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *s = x;
    return (float)(int32_t)x * (1.0f / 2147483648.0f);
}

void pulse_render(pulse_t *p, float *l, float *r, float *bl, float *br, int n)
{
    const float out = p->level * 0.5f, dn = p->drive_n, drv = p->drive;
    float ph = p->ph, penv = p->penv, amp = p->amp, cenv = p->cenv, clp = p->clp;
    const int kick = amp > PL_QUIET || p->rise || cenv > PL_QUIET, bloom = pulse_blooming(p);
    int i;
    if (kick)
        for (i = 0; i < n; i++) {
            float s, c;
            if (p->rise) {                           /* up to the hit's level in ~0.1 ms, no step */
                amp += (p->amp_tgt - amp) * 0.25f;
                if (amp >= p->amp_tgt * 0.999f)
                    p->rise = 0;
            } else {
                amp -= amp * p->dec_c;
            }
            s = fm_sin_turns(ph) * amp;
            ph += p->base_inc * (1.0f + p->pitch_r * penv);
            if (ph >= 1.0f)
                ph -= 1.0f;
            penv -= penv * p->pdec_c;
            clp += 0.35f * (noise(&p->rng) - clp);   /* the click: noise under ~3 kHz, ~2 ms */
            c = clp * cenv * p->click * p->vel;
            cenv *= 0.9895f;
            s = fm_tanhf((s + c) * drv) * dn * out;
            l[i] += s;
            r[i] += s;
        }
    if (bloom) {                                 /* an octave over the root, 4 cents either side; the fifth over */
        const float inc = p->base_inc * p->bmul, il = inc * 0.99769f, ir = inc * 1.00231f, i5 = inc * 1.49831f, f5 = p->bfifth;
        const float g = p->bloom * p->level;
        float a = p->bph[0], b = p->bph[1], c = p->bph[2], env = p->benv;
        int rise = p->brise;
        for (i = 0; i < n; i++) {
            float s5, e;
            if (rise) {                              /* toward past the top, so it gets there */
                env += (1.15f - env) * p->brise_c;
                if (env >= 1.0f) {
                    env = 1.0f;
                    rise = 0;
                }
            } else {
                env -= env * p->bfall_c;
            }
            e = env * env * g * p->bvel;             /* (squared: it blooms, slowly at first) */
            s5 = fm_sin_turns(c) * f5;
            bl[i] += (fm_sin_turns(a) + s5) * e;
            br[i] += (fm_sin_turns(b) + s5) * e;
            a += il;
            if (a >= 1.0f) a -= 1.0f;
            b += ir;
            if (b >= 1.0f) b -= 1.0f;
            c += i5;
            if (c >= 1.0f) c -= 1.0f;
        }
        p->bph[0] = a;
        p->bph[1] = b;
        p->bph[2] = c;
        p->benv = fm_flush(env);
        p->brise = rise;
    }
    p->ph = ph;
    p->penv = fm_flush(penv);
    p->amp = fm_flush(amp);
    p->cenv = fm_flush(cenv);
    p->clp = fm_flush(clp);
}
