/* SPDX-License-Identifier: GPL-3.0-only */
/* VOX; see vox.h. */
#include "vox.h"
#include "fastmath.h"

#define SR 44100.0f
#define INV_SR (1.0f / 44100.0f)
#define VOX_SCALE 0.22f                   /* a voice's loudness: four held under the drones' level */

static const char *const OCTB_N[3] = {"-12", "0", "+12"};
static const param_t VOX_P[VX_NPARAMS] = {
    [VX_DETUNE] = {"Detune", 127, 40, 0},    [VX_OCTB] = {"Oct B", 2, 1, OCTB_N},
    [VX_MIXB] = {"Mix B", 127, 64, 0},       [VX_CUTOFF] = {"Cutoff", 127, 60, 0},
    [VX_RESO] = {"Reso", 127, 25, 0},        [VX_ENVAMT] = {"EnvAmt", 127, 50, 0},
    [VX_KEYTRK] = {"KeyTrk", 127, 64, 0},    [VX_ATTACK] = {"Attack", 127, 80, 0},
    [VX_DECAY] = {"Decay", 127, 80, 0},      [VX_SUSTAIN] = {"Sustn", 127, 90, 0},
    [VX_RELEASE] = {"Releas", 127, 90, 0},   [VX_VIB] = {"Vib", 127, 20, 0},
    [VX_SPREAD] = {"Spread", 127, 70, 0},    [VX_LEVEL] = {"Level", 127, 100, 0},
    [VX_WAVE] = {"Wave", 127, 0, 0},         [VX_FMODE] = {"Filter", 127, 0, 0},
    [VX_GLIDE] = {"Glide", 127, 0, 0}};

int vox_nparams(void) { return VX_NPARAMS; }
const param_t *vox_param(int i) { return i >= 0 && i < VX_NPARAMS ? &VOX_P[i] : 0; }
int vox_get(const vox_t *x, int i) { return i >= 0 && i < VX_NPARAMS ? x->pot[i] : 0; }

static float pot01(int p) { return (float)p * (1.0f / 127.0f); }
static float one_pole(float seconds) { return 1.0f - fm_expf(-1.0f / (seconds * SR)); }
/* lo x 2^(octaves x pot / 127) */
static float pot_exp(int p, float lo, float octaves) { return lo * fm_exp2f(octaves * pot01(p)); }

static void voice_pans(vox_t *x)
{
    static const float AT[VOX_VOICES] = {-1.0f, 0.333f, -0.333f, 1.0f};   /* neighbours apart */
    int i;
    for (i = 0; i < VOX_VOICES; i++) {
        int pot = 64 + (int)(AT[i] * x->spread * 63.0f + (AT[i] > 0.0f ? 0.5f : -0.5f));
        pan_gains(pot < 0 ? 0 : pot > 127 ? 127 : pot, &x->v[i].pan_l, &x->v[i].pan_r);
    }
}

void vox_set(vox_t *x, int i, int value)
{
    const param_t *p = vox_param(i);
    if (!p)
        return;
    value = value < 0 ? 0 : value > p->max ? p->max : value;
    x->pot[i] = (uint8_t)value;
    switch (i) {
    case VX_DETUNE: x->det = pot01(value) * 25.0f; break;                   /* cents, B against A */
    case VX_OCTB: x->octb = 12.0f * (float)(value - 1); break;
    case VX_MIXB: x->mixb = pot01(value); break;
    case VX_CUTOFF: x->cutoff = pot_exp(value, 40.0f, 9.0f); break;         /* 40 Hz .. 20 kHz */
    case VX_RESO: x->reso = pot01(value) * 0.85f; break;
    case VX_ENVAMT: x->envamt = pot01(value) * 5.0f; break;                 /* octaves at full */
    case VX_KEYTRK: x->keytrk = pot01(value); break;                        /* octaves an octave */
    case VX_ATTACK: x->atk_c = one_pole(pot_exp(value, 0.002f, 11.0f)); break;   /* 2 ms .. 4 s */
    case VX_DECAY: x->dec_c = one_pole(pot_exp(value, 0.005f, 10.6f)); break;    /* 5 ms .. 8 s */
    case VX_SUSTAIN: x->sus = pot01(value); break;
    case VX_RELEASE: x->rel_c = one_pole(pot_exp(value, 0.005f, 10.6f)); break;
    case VX_VIB: x->vib = pot01(value) * 20.0f; break;                      /* cents */
    case VX_SPREAD: x->spread = pot01(value); voice_pans(x); break;
    case VX_LEVEL: x->level = pot01(value) * pot01(value) * 1.6129f; break; /* 100 = unity, square law */
    case VX_WAVE:                                                           /* saw .. square, then narrower */
        x->wmix = value >= 64 ? 1.0f : (float)value * (1.0f / 64.0f);
        x->pw = value <= 64 ? 0.5f : 0.5f - 0.4f * (float)(value - 64) * (1.0f / 63.0f);
        break;
    case VX_FMODE: x->bp = pot01(value); break;
    case VX_GLIDE: x->glide_c = value ? 1.0f - fm_expf(-(float)VOX_CTRL / (pot_exp(value, 0.01f, 7.2f) * SR)) : 1.0f; break;
    default: break;
    }
}

void vox_init(vox_t *x)
{
    uint8_t *z = (uint8_t *)x;
    uint32_t i;
    int k;
    for (i = 0; i < sizeof *x; i++)
        z[i] = 0;
    for (k = 0; k < VOX_VOICES; k++) {
        x->v[k].note = -1;
        x->v[k].stage = 2;
        x->v[k].ph[0] = 0.13f * (float)k;           /* (the voices' saws start apart) */
        x->v[k].ph[1] = 0.61f + 0.07f * (float)k;
    }
    for (k = 0; k < VX_NPARAMS; k++)
        vox_set(x, k, VOX_P[k].def);
    x->last_note = -1.0f;
}

/* silent: let go and 80 dB down (about -93 dBFS out, under any tail): it stops costing anything */
static int voice_idle(const vox_voice_t *v) { return v->stage == 2 && v->env < 1e-4f; }
int vox_active(const vox_t *x)
{
    int i;
    for (i = 0; i < VOX_VOICES; i++)
        if (!voice_idle(&x->v[i]))
            return 1;
    return 0;
}
int vox_held(const vox_t *x, int note)
{
    int i;
    for (i = 0; i < VOX_VOICES; i++)
        if (x->v[i].gate && x->v[i].note == note)
            return 1;
    return 0;
}

/* the voice a new note takes: the one with that note already, else a silent one, else the one let
 * go longest ago, else the oldest held */
static int pick(const vox_t *x, int note)
{
    int i, best = -1, best_rank = 99;
    uint32_t best_age = 0;
    for (i = 0; i < VOX_VOICES; i++)
        if (x->v[i].note == note && !voice_idle(&x->v[i]))
            return i;
    for (i = 0; i < VOX_VOICES; i++) {
        const vox_voice_t *v = &x->v[i];
        int rank = voice_idle(v) ? 0 : !v->gate ? 1 : 2;
        if (rank < best_rank || (rank == best_rank && v->age < best_age)) {
            best = i;
            best_rank = rank;
            best_age = v->age;
        }
    }
    return best;
}

void vox_note_on(vox_t *x, int note, float vel)
{
    vox_voice_t *v = &x->v[pick(x, note)];
    if (x->glide_c < 1.0f && x->last_note >= 0.0f)  /* GLIDE: from the last note played */
        v->pitch = x->last_note;
    else if (v->note != note || voice_idle(v))
        v->pitch = (float)note;
    x->last_note = (float)note;
    v->note = note;
    v->gate = 1;
    v->stage = 0;                                   /* the attack, from wherever the level is */
    v->age = ++x->clock;
    v->vel = vel < 0.0f ? 0.0f : vel > 1.0f ? 1.0f : vel;
    x->ctrl_left = 0;                               /* its pitch now */
}

void vox_note_off(vox_t *x, int note)
{
    int i;
    for (i = 0; i < VOX_VOICES; i++)
        if (x->v[i].gate && x->v[i].note == note) {
            x->v[i].gate = 0;
            x->v[i].stage = 2;
            x->v[i].age = ++x->clock;
        }
}

void vox_all_off(vox_t *x)
{
    int i;
    for (i = 0; i < VOX_VOICES; i++)
        if (x->v[i].gate)
            vox_note_off(x, x->v[i].note);
}

/* ---------------------------------------------------------------- render --- */
static float mtof_inc(float midi) { return 440.0f * fm_exp2f((midi - 69.0f) * (1.0f / 12.0f)) * INV_SR; }

static void control(vox_t *x)
{
    float vib;
    int i;
    x->vib_ph += 5.2f * (float)VOX_CTRL * INV_SR;
    if (x->vib_ph >= 1.0f)
        x->vib_ph -= 1.0f;
    vib = fm_sin_turns(x->vib_ph) * x->vib * 0.01f;
    for (i = 0; i < VOX_VOICES; i++) {
        vox_voice_t *v = &x->v[i];
        float n, fc, g, k;
        if (voice_idle(v))
            continue;
        v->pitch += ((float)v->note - v->pitch) * x->glide_c;
        n = v->pitch + vib;
        v->inc[0] = mtof_inc(n - x->det * 0.005f);
        v->inc[1] = mtof_inc(n + x->octb + x->det * 0.005f);
        fc = x->cutoff * fm_exp2f(x->envamt * v->env + x->keytrk * ((float)v->note - 60.0f) * (1.0f / 12.0f));
        fc = fm_clampf(fc, 20.0f, SR * 0.45f);
        g = fm_tanf(FM_PI * fc * INV_SR);
        k = 2.0f - 2.0f * x->reso;
        v->a1 = 1.0f / (1.0f + g * (g + k));
        v->a2 = g * v->a1;
        v->a3 = g * v->a2;
        v->bpk = k;
    }
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

static inline float saw(float t, float dt) { return 2.0f * t - 1.0f - blep(t, dt); }
/* a pulse of width pw: two saws pw apart, one taken from the other (no DC: it averages 0) */
static inline float pulse(float t, float dt, float pw)
{
    float t2 = t + pw;
    if (t2 >= 1.0f)
        t2 -= 1.0f;
    return saw(t, dt) - saw(t2, dt);
}

static void voice_chunk(vox_t *x, vox_voice_t *v, float *l, float *r, int n)
{
    const float ga = 1.0f - x->mixb, gb = x->mixb, sus = x->sus, wm = x->wmix, pw = x->pw;
    const float gain = VOX_SCALE * x->level * (0.4f + 0.6f * v->vel);
    const float pl = v->pan_l * gain, pr = v->pan_r * gain;
    const float a1 = v->a1, a2 = v->a2, a3 = v->a3, lpg = 1.0f - x->bp, bpg = x->bp * v->bpk;
    float ta = v->ph[0], tb = v->ph[1], da = v->inc[0], db = v->inc[1];
    float env = v->env, ic1 = v->ic1, ic2 = v->ic2;
    int stage = v->stage, i;
    for (i = 0; i < n; i++) {
        float s, v1, v2, v3;
        if (wm <= 0.0f)                             /* (the saws alone: as before, sample for sample) */
            s = ga * saw(ta, da) + gb * saw(tb, db);
        else
            s = ga * (saw(ta, da) + wm * (0.5f * pulse(ta, da, pw) - saw(ta, da))) +
                gb * (saw(tb, db) + wm * (0.5f * pulse(tb, db, pw) - saw(tb, db)));
        ta += da;
        if (ta >= 1.0f) ta -= 1.0f;
        tb += db;
        if (tb >= 1.0f) tb -= 1.0f;
        if (stage == 0) {                           /* toward past the top, so it gets there */
            env += (1.25f - env) * x->atk_c;
            if (env >= 1.0f) {
                env = 1.0f;
                stage = 1;
            }
        } else if (stage == 1) {
            env += (sus - env) * x->dec_c;
        } else {
            env -= env * x->rel_c;
        }
        v3 = s - ic2;
        v1 = a1 * ic1 + a2 * v3;
        v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        s = (lpg * v2 + bpg * v1) * env;            /* low-pass .. band-pass (FMODE) */
        l[i] += s * pl;
        r[i] += s * pr;
    }
    v->ph[0] = ta;
    v->ph[1] = tb;
    v->env = fm_flush(env);
    v->ic1 = fm_flush(ic1);
    v->ic2 = fm_flush(ic2);
    v->stage = stage;
}

void vox_render(vox_t *x, float *l, float *r, int n)
{
    int done = 0, i;
    while (done < n) {
        int m;
        if (x->ctrl_left <= 0) {
            control(x);
            x->ctrl_left = VOX_CTRL;
        }
        m = n - done < x->ctrl_left ? n - done : x->ctrl_left;
        for (i = 0; i < VOX_VOICES; i++)
            if (!voice_idle(&x->v[i]))
                voice_chunk(x, &x->v[i], l + done, r + done, m);
        x->ctrl_left -= m;
        done += m;
    }
}
