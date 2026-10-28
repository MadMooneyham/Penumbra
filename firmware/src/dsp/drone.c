/* SPDX-License-Identifier: GPL-3.0-only */
/* DRONE part; see drone.h. */
#include "drone.h"
#include "fastmath.h"

#define SR 44100.0f
#define INV_SR (1.0f / 44100.0f)

/* the stacks: semitones of each oscillator over the voice's note, and how many it has (the
 * cluster's unisons and the octaves' doublings are SWARM's now; Min9 leaves out the fifth) */
static const int8_t STACK[4][DRONE_OSC] = {{0, 0}, {0, 7, -12}, {0, 12, -12}, {0, 3, 10, 14}};
static const uint8_t NOSC[4] = {2, 3, 3, 4};
/* where each oscillator sits in the SPREAD (x the spread's cents) */
static const float SPREAD_AT[4][DRONE_OSC] = {{-0.5f, 0.5f}, {0.0f, -1.0f, 1.0f}, {0.0f, -1.0f, 1.0f},
                                              {0.0f, -1.0f, 1.0f, -0.5f}};

/* ---------------------------------------------------------------- params --- */
static const char *const NOTE_N[61] = {
    "-24", "-23", "-22", "-21", "-20", "-19", "-18", "-17", "-16", "-15", "-14", "-13", "-12", "-11", "-10",
    "-9", "-8", "-7", "-6", "-5", "-4", "-3", "-2", "-1", "0", "+1", "+2", "+3", "+4", "+5", "+6", "+7",
    "+8", "+9", "+10", "+11", "+12", "+13", "+14", "+15", "+16", "+17", "+18", "+19", "+20", "+21", "+22",
    "+23", "+24", "+25", "+26", "+27", "+28", "+29", "+30", "+31", "+32", "+33", "+34", "+35", "+36"};
static const char *const ROOT_N[25] = {"A0", "A#0", "B0", "C1", "C#1", "D1", "D#1", "E1", "F1", "F#1", "G1", "G#1",
                                       "A1", "A#1", "B1", "C2", "C#2", "D2", "D#2", "E2", "F2", "F#2", "G2",
                                       "G#2", "A2"};
static const char *const STACK_N[4] = {"Clust", "5ths", "Oct", "Min9"};
static const char *const MODE_N[2] = {"Hold", "Swell"};
static const char *const FMODE_N[2] = {"LP", "BP"};

static const param_t VOICE_P[DN_NPARAMS] = {
    [DN_NOTE] = {"Note", 60, 24, NOTE_N},     [DN_FINE] = {"Fine", 127, 64, 0},
    [DN_SPREAD] = {"Spread", 127, 20, 0},     [DN_STACK] = {"Stack", 3, 0, STACK_N},
    [DN_WAVE] = {"Wave", 127, 44, 0},         [DN_TONE] = {"Tone", 127, 75, 0},
    [DN_ATTACK] = {"Rise", 127, 98, 0},       [DN_RELEASE] = {"Fall", 127, 98, 0},
    [DN_HOLD] = {"Hold", 127, 81, 0},         [DN_MODE] = {"Mode", 1, DN_MODE_HOLD, MODE_N},
    [DN_LEVEL] = {"Level", 127, 106, 0},      [DN_PAN] = {"Pan", 127, 64, 0}};
static const param_t BUS_P[DB_NPARAMS] = {
    [DB_ROOT] = {"Root", 24, 12, ROOT_N},     [DB_DRIFT] = {"Drift", 127, 25, 0},
    [DB_CUTOFF] = {"Cutoff", 127, 81, 0},     [DB_RESO] = {"Reso", 127, 45, 0},
    [DB_DRIVE] = {"Drive", 127, 15, 0},       [DB_FMODE] = {"Filter", 1, 0, FMODE_N},
    [DB_LFORATE] = {"LfoRt", 127, 41, 0},     [DB_LFOCUT] = {"LfoCut", 127, 25, 0},
    [DB_LFOPITCH] = {"LfoPit", 127, 10, 0},   [DB_LFOPAN] = {"LfoPan", 127, 38, 0},
    [DB_SHRATE] = {"S&H Rt", 127, 75, 0},     [DB_SHCUT] = {"S&HCut", 127, 0, 0},
    [DB_WIND] = {"Wind", 127, 0, 0},          [DB_GAIN] = {"Gain", 127, 100, 0},
    [DB_SWARM] = {"Swarm", 127, 80, 0},       [DB_SWMOTION] = {"Motion", 127, 64, 0}};

int drone_nparams(int track)
{
    if (track >= 0 && track < DRONE_VOICES)
        return DN_NPARAMS;
    return track == DRONE_KIT ? DB_NPARAMS : 0;
}

const param_t *drone_param(int track, int i)
{
    if (i < 0 || i >= drone_nparams(track))
        return 0;
    return track == DRONE_KIT ? &BUS_P[i] : &VOICE_P[i];
}

const char *drone_track_name(int track)
{
    static const char *const N[DRONE_VOICES + 1] = {"DR1", "DR2", "DR3", "DR4", "DR5", "BUS"};
    return track >= 0 && track <= DRONE_KIT ? N[track] : "";
}

/* lo * 2^(oct * pot / 127): the exponential pots */
static float pot_exp(int pot, float lo, float oct) { return lo * fm_exp2f(oct * (float)pot * (1.0f / 127.0f)); }
static float pot_lin(int pot) { return (float)pot * (1.0f / 127.0f); }
static float one_pole(float seconds) { return 1.0f - fm_expf(-1.0f / (seconds * SR)); }

static void voice_derive(drone_voice_t *v, int i)
{
    const uint8_t *p = v->pot;
    switch (i) {
    case DN_WAVE: v->wave = pot_lin(p[DN_WAVE]); break;
    case DN_TONE: v->tone_c = 1.0f - fm_expf(-FM_TWO_PI * pot_exp(p[DN_TONE], 80.0f, 6.97f) * INV_SR); break;
    case DN_ATTACK: v->atk_c = one_pole(pot_exp(p[DN_ATTACK], 0.02f, 9.0f)); break;
    case DN_RELEASE: v->rel_c = one_pole(pot_exp(p[DN_RELEASE], 0.02f, 9.0f)); break;
    case DN_LEVEL: {
        float l = pot_lin(p[DN_LEVEL]);
        v->level = l * l;
        if (v->on)
            v->amp_tgt = v->level;
        break;
    }
    case DN_PAN: v->pan = (float)p[DN_PAN]; break;
    case DN_MODE:
        if (p[DN_MODE] == DN_MODE_HOLD)
            v->hold_left = 0;                     /* a latched voice has no countdown */
        break;
    default: break;                               /* pitch: read at each control step */
    }
}

static void bus_derive(drone_t *d, int i)
{
    const uint8_t *p = d->bus;
    switch (i) {
    case DB_DRIFT: d->drift_c = (float)p[DB_DRIFT] * (30.0f / 127.0f); break;
    case DB_CUTOFF: d->cutoff = pot_exp(p[DB_CUTOFF], 60.0f, 7.64f); break;
    case DB_RESO: d->reso = pot_lin(p[DB_RESO]) * 0.98f; break;
    case DB_DRIVE:
        d->drive = 1.0f + 5.0f * pot_lin(p[DB_DRIVE]);
        d->drive_post = fm_rsqrtf(d->drive);
        break;
    case DB_FMODE: d->bp = p[DB_FMODE]; break;
    case DB_LFORATE: d->lfo_inc = pot_exp(p[DB_LFORATE], 0.01f, 8.64f) * (float)DRONE_CTRL * INV_SR; break;
    case DB_LFOCUT: d->lfo_cut = 4.0f * pot_lin(p[DB_LFOCUT]); break;
    case DB_LFOPITCH: d->lfo_pitch = 40.0f * pot_lin(p[DB_LFOPITCH]); break;
    case DB_LFOPAN: d->lfo_pan = pot_lin(p[DB_LFOPAN]); break;
    case DB_SHRATE: d->sh_inc = pot_exp(p[DB_SHRATE], 0.1f, 7.32f) * (float)DRONE_CTRL * INV_SR; break;
    case DB_SHCUT: d->sh_cut = 3.0f * pot_lin(p[DB_SHCUT]); break;
    case DB_WIND: d->wind = pot_lin(p[DB_WIND]) * 1.5f; break;
    case DB_GAIN: {
        float g = (float)p[DB_GAIN] * (1.0f / 100.0f);
        d->gain = g * g;
        break;
    }
    case DB_SWARM: d->sw.amt = pot_lin(p[DB_SWARM]); break;
    case DB_SWMOTION: d->sw.motion = pot_lin(p[DB_SWMOTION]); break;
    default: break;
    }
}

void drone_set(drone_t *d, int track, int i, int value)
{
    const param_t *p = drone_param(track, i);
    if (!p)
        return;
    if (value < 0) value = 0;
    if (value > p->max) value = p->max;
    if (track == DRONE_KIT) {
        d->bus[i] = (uint8_t)value;
        bus_derive(d, i);
    } else {
        d->v[track].pot[i] = (uint8_t)value;
        voice_derive(&d->v[track], i);
    }
}

int drone_get(const drone_t *d, int track, int i)
{
    if (!drone_param(track, i))
        return 0;
    return track == DRONE_KIT ? d->bus[i] : d->v[track].pot[i];
}

/* ------------------------------------------------------------------ init --- */
static uint32_t rng_next(uint32_t *s)
{
    uint32_t x = *s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return *s = x;
}
static float rng_unit(uint32_t *s) { return (float)(rng_next(s) >> 8) * (1.0f / 16777216.0f); }   /* [0, 1) */

void drone_init(drone_t *d)
{
    uint8_t *z = (uint8_t *)d;
    int i, k, j;
    for (i = 0; i < (int)sizeof *d; i++)
        z[i] = 0;
    d->rng = 0x9E3779B9u;
    d->bus_rng = 0x2545F491u;
    d->lite_g = 1.0f;
    for (i = 0; i < DRONE_VOICES; i++) {
        drone_voice_t *v = &d->v[i];
        for (k = 0; k < DRONE_OSC; k++) {
            v->ph[k] = rng_unit(&d->rng);
            for (j = 0; j < 2; j++) {               /* each drifts at its own 0.03..0.28 Hz */
                v->drift_ph[k][j] = rng_unit(&d->rng);
                v->drift_inc[k][j] = (0.03f + 0.25f * rng_unit(&d->rng)) * (float)DRONE_CTRL * INV_SR;
            }
        }
        for (k = 0; k < DN_NPARAMS; k++)
            drone_set(d, i, k, VOICE_P[k].def);
        v->pan_l = v->pan_r = 1.0f;
    }
    for (k = 0; k < DB_NPARAMS; k++)
        drone_set(d, DRONE_KIT, k, BUS_P[k].def);
}

void drone_set_lite(drone_t *d, int on) { d->lite = on != 0; }

/* ----------------------------------------------------------------- notes --- */
void drone_trigger(drone_t *d, int track, float vel)
{
    drone_voice_t *v;
    (void)vel;                                    /* a drone has no velocity: LEVEL is its loudness */
    if (track < 0 || track >= DRONE_VOICES)
        return;
    v = &d->v[track];
    if (v->pot[DN_MODE] == DN_MODE_HOLD) {
        v->on = !v->on;                           /* the latching key */
    } else {
        v->on = 1;                                /* a swell: in over RISE, HOLD, then out over FALL */
        v->hold_left = (int32_t)(pot_exp(v->pot[DN_HOLD], 0.05f, 8.32f) * SR);
    }
    v->amp_tgt = v->on ? v->level : 0.0f;
    d->ctrl_left = 0;                             /* its pitch now, not at the next control step */
}

void drone_end_swells(drone_t *d)
{
    int i;
    for (i = 0; i < DRONE_VOICES; i++)
        if (d->v[i].hold_left > 0)
            d->v[i].hold_left = 1;                /* out over FALL at the next control step */
}

void drone_all_off(drone_t *d)
{
    int i;
    for (i = 0; i < DRONE_VOICES; i++) {
        d->v[i].on = 0;
        d->v[i].amp_tgt = 0.0f;
        d->v[i].hold_left = 0;
    }
}

static int voice_live(const drone_voice_t *v) { return v->amp_tgt > 0.0f || v->amp > 1e-5f; }

int drone_active(const drone_t *d)
{
    int i;
    if (d->wind > 0.0f)
        return 1;
    for (i = 0; i < DRONE_VOICES; i++)
        if (voice_live(&d->v[i]))
            return 1;
    return 0;
}

/* ----------------------------------------------------------------- swarm --- */
#define SW_BASE_MS 7.0f                   /* the taps sit 7, 10, 13 ms back, +-3 ms */
#define SW_DEPTH_MS 3.0f

void drone_swarm_buffer(drone_t *d, float *l, float *r)
{
    int s, i;
    d->sw.buf[0] = l;
    d->sw.buf[1] = r;
    for (s = 0; s < 2; s++)
        if (d->sw.buf[s])
            for (i = 0; i < SWARM_LEN; i++)
                d->sw.buf[s][i] = 0.0f;
    for (s = 0; s < 2; s++)
        for (i = 0; i < SWARM_TAPS; i++)
            d->sw.dl[s][i] = (SW_BASE_MS + 3.0f * (float)i) * (SR / 1000.0f);
}

/* each tap's delay at the next control step: an ensemble's sines, or a random walk, by MOTION. Only
 * the targets: the bus pass takes each tap's per-sample step from where it really is (swarm_steps) */
static void swarm_targets(drone_t *d, float tgt_out[2][SWARM_TAPS])
{
    drone_swarm_t *w = &d->sw;
    const float m = w->motion, depth = SW_DEPTH_MS * (SR / 1000.0f);
    int s, i;
    w->slow_ph += 0.63f * (float)DRONE_CTRL * INV_SR;     /* the ensemble: 0.63 Hz chorus, 5.7 Hz vibrato */
    w->fast_ph += 5.7f * (float)DRONE_CTRL * INV_SR;
    if (w->slow_ph >= 1.0f) w->slow_ph -= 1.0f;
    if (w->fast_ph >= 1.0f) w->fast_ph -= 1.0f;
    for (s = 0; s < 2; s++)
        for (i = 0; i < SWARM_TAPS; i++) {
            const float off = (float)i * (1.0f / 3.0f) + (float)s * (1.0f / 6.0f);
            float ens = 0.8f * fm_sin_turns(w->slow_ph + off) + 0.08f * fm_sin_turns(w->fast_ph + off), tgt;
            /* the walk: a new heading every ~0.25 .. 0.5 s, smoothed twice (~0.3 s each) so the pitch
             * it bends (its slope) moves without steps, within about +-15 cents */
            if (rng_unit(&d->rng) < 0.0015f * (1.0f + 0.5f * (float)i))
                w->walk_tgt[s][i] = rng_unit(&d->rng) * 2.0f - 1.0f;
            w->walk[s][i] += (w->walk_tgt[s][i] - w->walk[s][i]) * 0.0025f;
            w->walk2[s][i] += (w->walk[s][i] - w->walk2[s][i]) * 0.0025f;
            tgt = (SW_BASE_MS + 3.0f * (float)i) * (SR / 1000.0f) +
                  depth * (ens + m * (w->walk2[s][i] * 2.5f - ens));
            tgt_out[s][i] = fm_clampf(tgt, 1.0f * (SR / 1000.0f), (float)(SWARM_LEN - 4));
        }
}
static void swarm_steps(drone_t *d, const float tgt[2][SWARM_TAPS])
{
    drone_swarm_t *w = &d->sw;
    int s, i;
    for (s = 0; s < 2; s++)
        for (i = 0; i < SWARM_TAPS; i++)
            w->step[s][i] = (tgt[s][i] - w->dl[s][i]) * (1.0f / (float)DRONE_CTRL);
}

static void swarm_chunk(drone_t *d, float *l, float *r, int n)
{
    drone_swarm_t *w = &d->sw;
    const float wet = w->amt * (0.577f * 1.2f), dry = 1.0f - 0.55f * w->amt;
    int i, s, k;
    if (!w->buf[0] || !w->buf[1])
        return;
    for (i = 0; i < n; i++) {
        const int32_t p = w->w;
        float x[2];
        x[0] = l[i];
        x[1] = r[i];
        for (s = 0; s < 2; s++) {
            float *b = w->buf[s], sum = 0.0f;
            b[p] = x[s];
            for (k = 0; k < SWARM_TAPS; k++) {
                float rp = (float)p - w->dl[s][k], fr;
                int32_t i0;
                w->dl[s][k] += w->step[s][k];
                if (rp < 0.0f) rp += (float)SWARM_LEN;
                i0 = (int32_t)rp;
                fr = rp - (float)i0;
                sum += b[i0 & (SWARM_LEN - 1)] + (b[(i0 + 1) & (SWARM_LEN - 1)] - b[i0 & (SWARM_LEN - 1)]) * fr;
            }
            x[s] = x[s] * dry + sum * wet;
        }
        l[i] = x[0];
        r[i] = x[1];
        w->w = (p + 1) & (SWARM_LEN - 1);
    }
}

/* --------------------------------------------------------------- control --- */
static float mtof_inc(float midi) { return 440.0f * fm_exp2f((midi - 69.0f) * (1.0f / 12.0f)) * INV_SR; }

static void control(drone_t *d)
{
    int i, k;
    float fc, g, kk;
    d->lfo_ph += d->lfo_inc;
    if (d->lfo_ph >= 1.0f) d->lfo_ph -= 1.0f;
    d->lfo_now = fm_sin_turns(d->lfo_ph);
    d->sh_ph += d->sh_inc;
    if (d->sh_ph >= 1.0f) {
        d->sh_ph -= 1.0f;
        d->sh_val = rng_unit(&d->rng) * 2.0f - 1.0f;
    }
    d->sh_sm += (d->sh_val - d->sh_sm) * 0.5f;
    for (i = 0; i < DRONE_VOICES; i++) {
        drone_voice_t *v = &d->v[i];
        float base, pan;
        int pp;
        if (v->hold_left > 0 && (v->hold_left -= DRONE_CTRL) <= 0) {   /* a swell's hold ran out */
            v->hold_left = 0;
            v->on = 0;
            v->amp_tgt = 0.0f;
        }
        if (!voice_live(v))
            continue;
        base = (float)(21 + d->bus[DB_ROOT]) + (float)(v->pot[DN_NOTE] - 24) +
               ((float)v->pot[DN_FINE] - 64.0f) * (1.0f / 64.0f) + d->lfo_now * d->lfo_pitch * 0.01f;
        for (k = 0; k < NOSC[v->pot[DN_STACK]]; k++) {
            float dr, cents;
            v->drift_ph[k][0] += v->drift_inc[k][0];
            v->drift_ph[k][1] += v->drift_inc[k][1];
            if (v->drift_ph[k][0] >= 1.0f) v->drift_ph[k][0] -= 1.0f;
            if (v->drift_ph[k][1] >= 1.0f) v->drift_ph[k][1] -= 1.0f;
            dr = 0.6f * fm_sin_turns(v->drift_ph[k][0]) + 0.4f * fm_sin_turns(v->drift_ph[k][1]);
            cents = SPREAD_AT[v->pot[DN_STACK]][k] * (float)v->pot[DN_SPREAD] * (60.0f / 127.0f) + dr * d->drift_c;
            v->inc[k] = mtof_inc(base + (float)STACK[v->pot[DN_STACK]][k] + cents * 0.01f);
        }
        pan = v->pan + d->lfo_now * d->lfo_pan * 63.0f * ((i & 1) ? 1.0f : -1.0f);   /* neighbours swing apart */
        pp = (int)(pan + 0.5f);
        pan_gains(pp < 0 ? 0 : pp > 127 ? 127 : pp, &v->pan_l, &v->pan_r);
    }
    fc = d->cutoff * fm_exp2f(d->lfo_now * d->lfo_cut + d->sh_sm * d->sh_cut);
    fc = fm_clampf(fc, 20.0f, SR * 0.45f);
    g = fm_tanf(FM_PI * fc * INV_SR);
    kk = 2.0f - 2.0f * d->reso;
    d->a1 = 1.0f / (1.0f + g * (g + kk));
    d->a2 = g * d->a1;
    d->a3 = g * d->a2;
}

/* ---------------------------------------------------------------- render --- */
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

/* TPT state-variable filter (its stretch's coefficients); the band state soft-clipped (the raw,
 * singing resonance) */
static inline float svf(drone_svf_t *s, const drone_seg_t *g, int bp, float x)
{
    float v3 = x - s->ic2, v1 = g->a1 * s->ic1 + g->a2 * v3, v2 = s->ic2 + g->a2 * s->ic1 + g->a3 * v3;
    s->ic1 = fm_tanhf(2.0f * v1 - s->ic1);
    s->ic2 = 2.0f * v2 - s->ic2;
    return bp ? v1 : v2;
}

/* square -> saw from two band-limited saws half a turn apart: square = saw(t + 1/2) - saw(t), so
 * (1 - w) square + w saw = (1 - w) saw(t + 1/2) + (2w - 1) saw(t) (two BLEPs, not three) */
static void voice_chunk(drone_voice_t *v, const drone_seg_t *g, int vi, float *l, float *r)
{
    int i, k;
    const int nosc = g->nosc[vi], n = g->len;
    const float *inc = g->inc[vi], xg = g->xg;
    const float ga = 1.0f - v->wave, gb = 2.0f * v->wave - 1.0f, tc = v->tone_c;
    const float pl = g->pan_l[vi] * g->scale[vi], pr = g->pan_r[vi] * g->scale[vi];
    const float up = v->atk_c, down = v->rel_c, tgt = v->amp_tgt;
    float amp = v->amp, lp = v->lp;
    for (i = 0; i < n; i++) {
        float s = 0.0f, sx = 0.0f;                    /* sx: the oscillators past the second, x xg */
        for (k = 0; k < nosc; k++) {
            float t = v->ph[k], dt = inc[k], t2 = t + 0.5f, o;
            if (t2 >= 1.0f) t2 -= 1.0f;
            o = ga * (2.0f * t2 - 1.0f - blep(t2, dt)) + gb * (2.0f * t - 1.0f - blep(t, dt));
            if (k < 2)
                s += o;
            else
                sx += o;
            t += dt;
            if (t >= 1.0f) t -= 1.0f;
            v->ph[k] = t;
        }
        s += xg * sx;
        lp += tc * (s - lp);
        amp += (tgt - amp) * (tgt > amp ? up : down);
        s = lp * amp;
        l[i] += s * pl;
        r[i] += s * pr;
    }
    v->amp = fm_flush(amp);
    v->lp = fm_flush(lp);
}

/* the block's control steps, before any voice renders: each stretch between two of them gets what
 * the step set (pitches, pans, the filter, SWARM's targets) and the guard's fade, as one render made
 * them stretch by stretch. Whether a voice sounds is decided at the block's start (a voice falling
 * silent inside a block renders to its end, far under hearing) */
int drone_plan(drone_t *d, int n)
{
    /* a voice's loudness, by its oscillators: 0.09 x sqrt(5 / n), as five made it */
    static const float SCALE[DRONE_OSC + 1] = {0.0f, 0.2012f, 0.1423f, 0.1162f, 0.1006f, 0.09f};
    int done = 0, sg = 0, i, k, p = d->cur ^= 1;
    if (n > DRONE_MAX_BLOCK)
        n = DRONE_MAX_BLOCK;
    while (done < n && sg < DRONE_SEGS) {
        drone_seg_t *g = &d->seg[p][sg++];
        int m;
        g->ctrl = d->ctrl_left <= 0;
        if (g->ctrl) {
            control(d);
            swarm_targets(d, g->sw_tgt);
            d->ctrl_left = DRONE_CTRL;
        }
        m = n - done < d->ctrl_left ? n - done : d->ctrl_left;
        /* the overload guard thins the voices to two oscillators by fading the others out (and back),
         * the loudness following: a sudden change of the stack is a click in a held drone */
        d->lite_g = d->lite ? fm_maxf(d->lite_g - 0.012f, 0.0f) : fm_minf(d->lite_g + 0.012f, 1.0f);
        g->xg = d->lite_g;
        g->a1 = d->a1;
        g->a2 = d->a2;
        g->a3 = d->a3;
        for (i = 0; i < DRONE_VOICES; i++) {
            const drone_voice_t *v = &d->v[i];
            int nosc = NOSC[v->pot[DN_STACK]];
            float sc = SCALE[nosc];
            if (!voice_live(v)) {
                g->nosc[i] = 0;
                continue;
            }
            if (nosc > 2) {
                sc = SCALE[2] + (SCALE[nosc] - SCALE[2]) * g->xg;
                if (g->xg <= 0.0f)
                    nosc = 2;
            }
            g->nosc[i] = (uint8_t)nosc;
            g->scale[i] = sc;
            for (k = 0; k < nosc; k++)
                g->inc[i][k] = v->inc[k];
            g->pan_l[i] = v->pan_l;
            g->pan_r[i] = v->pan_r;
        }
        g->len = (int16_t)m;
        d->ctrl_left -= m;
        done += m;
    }
    d->nseg[p] = sg;
    d->plan_n = done;
    return p;
}

int drone_cost(const drone_t *d, int voice)
{
    return voice >= 0 && voice < DRONE_VOICES && d->nseg[d->cur] ? d->seg[d->cur][0].nosc[voice] : 0;
}

void drone_voices(drone_t *d, uint32_t mask, float *l, float *r)
{
    int sg, i, off = 0;
    for (sg = 0; sg < d->nseg[d->cur]; sg++) {
        const drone_seg_t *g = &d->seg[d->cur][sg];
        for (i = 0; i < DRONE_VOICES; i++)
            if ((mask >> i & 1u) && g->nosc[i])
                voice_chunk(&d->v[i], g, i, l + off, r + off);
        off += g->len;
    }
}

void drone_bus_filter(drone_t *d, int p, float *l, float *r)
{
    const float drv = d->drive, post = d->drive_post * d->gain;
    const int bp = d->bp;
    int sg, i, off = 0;
    for (sg = 0; sg < d->nseg[p & 1]; sg++) {
        const drone_seg_t *g = &d->seg[p & 1][sg];
        float *gl = l + off, *gr = r + off;
        const int n = g->len;
        if (d->wind > 0.0f)
            for (i = 0; i < n; i++) {
                d->nb_l += 0.03f * ((rng_unit(&d->bus_rng) * 2.0f - 1.0f) - d->nb_l);
                d->nb_r += 0.03f * ((rng_unit(&d->bus_rng) * 2.0f - 1.0f) - d->nb_r);
                gl[i] += d->nb_l * d->wind;
                gr[i] += d->nb_r * d->wind;
            }
        for (i = 0; i < n; i++) {
            gl[i] = svf(&d->fl, g, bp, fm_tanhf(gl[i] * drv)) * post;
            gr[i] = svf(&d->fr, g, bp, fm_tanhf(gr[i] * drv)) * post;
        }
        off += n;
    }
}

void drone_bus_swarm(drone_t *d, int p, float *l, float *r)
{
    int sg, off = 0;
    for (sg = 0; sg < d->nseg[p & 1]; sg++) {
        const drone_seg_t *g = &d->seg[p & 1][sg];
        if (g->ctrl)
            swarm_steps(d, g->sw_tgt);
        if (d->sw.amt > 0.0f)
            swarm_chunk(d, l + off, r + off, g->len);
        off += g->len;
    }
}

void drone_render(drone_t *d, float *l, float *r, int n)
{
    int done = 0;
    while (done < n) {                            /* (blocks longer than a plan holds: in plans) */
        int p = drone_plan(d, n - done);
        drone_voices(d, (1u << DRONE_VOICES) - 1u, l + done, r + done);
        drone_bus_filter(d, p, l + done, r + done);
        drone_bus_swarm(d, p, l + done, r + done);
        done += d->plan_n;
    }
}
