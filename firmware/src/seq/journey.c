/* SPDX-License-Identifier: GPL-3.0-only */
/* Penumbra's journeys; see journey.h. */
#include "journey.h"

#define FS 44100.0f

/* ------------------------------------------------------------- the music --- */
static const uint8_t SCALE[NSCALES][7] = {
    {0, 2, 4, 5, 7, 9, 11}, {0, 2, 3, 5, 7, 9, 10}, {0, 1, 3, 5, 7, 8, 10}, {0, 2, 4, 6, 7, 9, 11},
    {0, 2, 4, 5, 7, 9, 10}, {0, 2, 3, 5, 7, 8, 10}, {0, 2, 3, 5, 7, 8, 11}};
/* the degrees each scale's pentatonic leaves out: the minor ones their 2nd and 6th, the major their 4th
 * and 7th (lydian its #4) */
static const uint8_t PENTA_SKIP[NSCALES] = {0x48, 0x22, 0x22, 0x48, 0x48, 0x22, 0x22};
static const char *const SCALE_N[NSCALES] = {"IONIAN", "DORIAN", "PHRYGIAN", "LYDIAN", "MIXOLYDIAN", "AEOLIAN",
                                             "HARM MINOR"};
static const char *const ROOT_N[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
/* the drone's five voices over the chord's root, in scale degrees (root, fifth, octave, ...) */
static const int8_t VOICING[NVOICINGS][JR_DRONES] = {{0, 4, 7, 11, 14}, {0, 4, 7, 9, 14}, {0, 7, 11, 14, 16},
                                                     {0, 4, 7, 8, 11}};
static const char *const VOICING_N[NVOICINGS] = {"OPEN", "WARM", "HIGH", "CLUSTER"};
typedef struct { const char *name; uint8_t n; int8_t deg[4]; } prog_t;
static const prog_t PROG[] = {
    {"I", 1, {0}},              {"I VI", 2, {0, 5}},         {"I IV", 2, {0, 3}},
    {"I VII", 2, {0, 6}},       {"I II", 2, {0, 1}},         {"I III", 2, {0, 2}},
    {"I V", 2, {0, 4}},         {"I VI III VII", 4, {0, 5, 2, 6}}, {"I IV VI V", 4, {0, 3, 5, 4}},
    {"I VII VI VII", 4, {0, 6, 5, 6}}, {"VI IV I V", 4, {5, 3, 0, 4}}, {"I II IV II", 4, {0, 1, 3, 1}}};
#define NPROG ((int)(sizeof PROG / sizeof PROG[0]))
static const char *const MOON_N[NMOONS] = {"NEW", "WAXING", "FULL", "WANING"};
static const char *const MODE_N[NMODES] = {"PLAY", "GUIDE", "DRIFT"};

const char *jr_scale_name(int s) { return SCALE_N[(unsigned)s % NSCALES]; }
const char *jr_root_name(int pc) { return ROOT_N[(unsigned)pc % 12u]; }
const char *jr_prog_name(int p) { return PROG[(unsigned)p % (unsigned)NPROG].name; }
int jr_nprogs(void) { return NPROG; }
const char *jr_voicing_name(int v) { return VOICING_N[(unsigned)v % NVOICINGS]; }
const char *jr_moon_name(int m) { return MOON_N[(unsigned)m % NMOONS]; }
const char *jr_mode_name(int m) { return MODE_N[(unsigned)m % NMODES]; }

/* --------------------------------------------------------------- presets --- */
#define SCN(root, scale, prog, voc, dens, bri, swm, spc, tex, col, pul, reg, dr, pen) \
    {root, scale, prog, voc, dens, bri, swm, spc, tex, col, pul, reg, dr, pen, {0, 0}}
typedef struct { const char *name; uint32_t seed; uint8_t n; scene_t s[4]; } preset_t;
static const preset_t PRESET[JR_PRESETS] = {
    {"ECLIPSE", 0x5EC1u, 4, {SCN(9, SC_AEOLIAN, 7, VC_WARM, 60, 60, 80, 80, 40, 50, 0, 4, 0x07, 1),
                             SCN(9, SC_AEOLIAN, 1, VC_OPEN, 85, 72, 95, 70, 30, 70, 40, 4, 0x1F, 0),
                             SCN(5, SC_LYDIAN, 4, VC_HIGH, 70, 80, 70, 90, 50, 85, 20, 4, 0x1D, 0),
                             SCN(2, SC_DORIAN, 2, VC_WARM, 45, 55, 90, 85, 60, 45, 0, 4, 0x0B, 1)}},
    {"PENUMBRA", 0x9E7Bu, 3, {SCN(2, SC_DORIAN, 2, VC_WARM, 70, 65, 85, 75, 35, 55, 50, 4, 0x1F, 1),
                              SCN(2, SC_DORIAN, 9, VC_OPEN, 90, 75, 90, 70, 25, 65, 70, 4, 0x1F, 0),
                              SCN(7, SC_MIXOLYDIAN, 3, VC_WARM, 60, 70, 70, 80, 40, 70, 30, 4, 0x0F, 1)}},
    {"SELENE", 0x5E1Eu, 3, {SCN(4, SC_PHRYGIAN, 4, VC_CLUSTER, 35, 40, 100, 95, 70, 35, 0, 3, 0x13, 0),
                            SCN(4, SC_PHRYGIAN, 9, VC_OPEN, 55, 48, 110, 90, 60, 40, 25, 4, 0x1F, 1),
                            SCN(0, SC_LYDIAN, 0, VC_HIGH, 30, 60, 80, 100, 80, 60, 0, 4, 0x11, 0)}},
    {"TRANQUILITY", 0x7A9Cu, 3, {SCN(0, SC_LYDIAN, 4, VC_HIGH, 55, 85, 70, 90, 30, 90, 0, 4, 0x1F, 0),
                                 SCN(7, SC_IONIAN, 8, VC_WARM, 70, 80, 75, 85, 25, 80, 20, 4, 0x1F, 1),
                                 SCN(4, SC_AEOLIAN, 7, VC_WARM, 60, 70, 80, 85, 35, 70, 10, 4, 0x1F, 1)}},
    {"PERIGEE", 0x9E21u, 3, {SCN(6, SC_AEOLIAN, 7, VC_OPEN, 95, 70, 75, 60, 20, 60, 85, 4, 0x1F, 1),
                             SCN(6, SC_DORIAN, 2, VC_WARM, 110, 80, 80, 55, 20, 70, 100, 4, 0x1F, 0),
                             SCN(2, SC_LYDIAN, 4, VC_HIGH, 80, 85, 70, 70, 25, 80, 60, 4, 0x1D, 1)}},
    {"FAR SIDE", 0xFA75u, 4, {SCN(7, SC_HARMONIC, 1, VC_WARM, 50, 50, 95, 100, 75, 30, 0, 4, 0x1F, 0),
                              SCN(3, SC_LYDIAN, 0, VC_HIGH, 30, 55, 90, 110, 85, 50, 0, 4, 0x15, 0),
                              SCN(0, SC_AEOLIAN, 7, VC_WARM, 70, 60, 85, 90, 50, 45, 30, 4, 0x1F, 1),
                              SCN(7, SC_HARMONIC, 9, VC_CLUSTER, 40, 45, 110, 100, 70, 35, 0, 3, 0x13, 0)}}};

const char *jr_preset_name(int i) { return PRESET[(unsigned)i % JR_PRESETS].name; }

void jr_preset(int i, journey_t *dst)
{
    const preset_t *p = &PRESET[(unsigned)i % JR_PRESETS];
    uint8_t *z = (uint8_t *)dst;
    uint32_t k;
    int v;
    for (k = 0; k < sizeof *dst; k++)
        z[k] = 0;
    dst->magic = JR_MAGIC;
    dst->format = JR_FORMAT;
    dst->size = (uint16_t)sizeof *dst;
    for (k = 0; p->name[k] && k < JR_NAME - 1u; k++)
        dst->name[k] = p->name[k];
    dst->seed = p->seed;
    dst->nscenes = p->n;
    dst->mode = MODE_PLAY;
    for (v = 0; v < NMACROS; v++)
        dst->macro[v] = 64;
    for (v = 0; v < p->n; v++)
        dst->scene[v] = p->s[v];
    for (v = 0; v < JR_LOOPS; v++) {
        dst->loop[v].len = 16;
        dst->loop[v].div = v == LP_VOX ? 1 : 2;
    }
}

int jr_valid(const journey_t *j)
{
    return j->magic == JR_MAGIC && j->format == JR_FORMAT && j->size == sizeof *j && j->nscenes >= 1 &&
           j->nscenes <= JR_SCENES;
}

/* ------------------------------------------------------------------ helpers --- */
static uint32_t rnd(uint32_t *s)
{
    uint32_t x = *s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return *s = x;
}
static int rndn(uint32_t *s, int n) { return (int)((rnd(s) >> 8) % (uint32_t)n); }
static float rndf(uint32_t *s) { return (float)(rnd(s) >> 8) * (1.0f / 16777216.0f); }
static uint32_t hash3(uint32_t a, uint32_t b, uint32_t c)
{
    uint32_t h = a * 0x9E3779B1u ^ (b + 0x7F4A7C15u) * 0x85EBCA6Bu ^ (c + 1u) * 0xC2B2AE35u;
    h ^= h >> 15;
    h *= 0x2C1B3C6Du;
    h ^= h >> 12;
    return h ? h : 1u;
}
static int clampi(int x, int lo, int hi) { return x < lo ? lo : x > hi ? hi : x; }
static float clampf(float x, float lo, float hi) { return x < lo ? lo : x > hi ? hi : x; }

static int nsc(const journey_t *j) { return j->nscenes < 1 ? 1 : j->nscenes > JR_SCENES ? JR_SCENES : j->nscenes; }
static const scene_t *sc(const jrun_t *r) { return &r->j->scene[r->cur % nsc(r->j)]; }
static int scale_of(const scene_t *s) { return s->scale % NSCALES; }
static const prog_t *prog_of(const scene_t *s) { return &PROG[s->prog % NPROG]; }
static int reg_of(const scene_t *s) { return clampi(s->reg, 2, 6); }

/* a scale degree's semitones over the root (any degree, below 0 too) */
static int deg_semi(int scale, int d)
{
    int o = d >= 0 ? d / 7 : -((6 - d) / 7);
    return 12 * o + SCALE[scale][d - 7 * o];
}
/* the chord's root as a degree, kept near the key's root (VII is the degree under it) */
static int chord_root(const jrun_t *r)
{
    const prog_t *p = prog_of(sc(r));
    int d = p->deg[r->chord % p->n];
    return d > 3 ? d - 7 : d;
}
/* a phrase's note: deg over the chord's root, oct octaves over the register */
static int mel_note(const jrun_t *r, int deg, int oct, int penta)
{
    const scene_t *s = sc(r);
    int scale = scale_of(s), d = chord_root(r) + deg + r->shift;
    int m;
    if (penta && s->penta && (PENTA_SKIP[scale] >> (((d % 7) + 7) % 7) & 1u))
        d++;
    m = 12 * (reg_of(s) + oct) + s->root % 12 + deg_semi(scale, d);
    while (m < 24)                               /* (out of range: an octave in, still in the key) */
        m += 12;
    while (m > 108)
        m -= 12;
    return m;
}
/* the other way: a played note as a degree over the chord's root (VOX's register, nearest in the scale) */
static int note_deg(const jrun_t *r, int midi)
{
    const scene_t *s = sc(r);
    int scale = scale_of(s), semi = midi - (12 * (reg_of(s) + 1) + s->root % 12), o, pc, i, best = 0, bd = 99;
    o = semi >= 0 ? semi / 12 : -((11 - semi) / 12);
    pc = semi - 12 * o;
    for (i = 0; i < 7; i++) {
        int dd = pc - SCALE[scale][i];
        dd = dd < 0 ? -dd : dd;
        if (dd < bd) {
            bd = dd;
            best = i;
        }
    }
    if (12 - pc < bd) {                          /* nearer the octave over */
        best = 0;
        o++;
    }
    return clampi(7 * o + best - chord_root(r) - r->shift, -24, 39);
}

int jr_key_note(const jrun_t *r, int w, int oct)
{
    const scene_t *s = sc(r);
    return clampi(12 * reg_of(s) + s->root % 12 + deg_semi(scale_of(s), w + 7 * oct), 0, 127);
}

static void drones_to_chord(jrun_t *r, const jr_sink_t *o)
{
    const scene_t *s = sc(r);
    int v, cd = chord_root(r), voc = s->voicing % NVOICINGS;
    for (v = 0; v < JR_DRONES; v++) {
        int m = 24 + s->root % 12 + deg_semi(scale_of(s), cd + VOICING[voc][v]);
        while (m > 69)                           /* the drone's top (dsp/drone.h NOTE) */
            m -= 12;
        r->drone[v] = m;
        o->drone_note(o->ctx, v, m);
    }
}

/* --------------------------------------------------------------- generator --- */
/* the order a motif's steps join as it fills: the strong ones (each beat) first, then the 8ths */
static void gen_rank(jr_gen_t *g)
{
    uint8_t key[JR_STEPS];
    int i, k, n = g->m.len;
    for (i = 0; i < n; i++)
        key[i] = (uint8_t)(((i & 3) == 0 ? 0 : (i & 1) == 0 ? 64 : 128) + rndn(&g->rng, 64));
    for (i = 0; i < n; i++) {
        int rk = 0;
        for (k = 0; k < n; k++)
            rk += key[k] < key[i] || (key[k] == key[i] && k < i);
        g->rank[i] = (uint8_t)rk;
    }
}
/* GUIDE's held keys: the degrees they stand for, over the chord's root (key w: w - 4 over the register's) */
static int held_deg(const jrun_t *r, int w) { return w - 4 - chord_root(r) - r->shift; }
static int guide_pick(const jrun_t *r, int d, uint32_t *rng)
{
    int w, best = d, bd = 99;
    if (!r->held || rndf(rng) > 0.85f)
        return d;
    for (w = 0; w < 16; w++)
        if (r->held >> w & 1u) {
            int h = held_deg(r, w), dd = h - d;
            dd = dd < 0 ? -dd : dd;
            if (dd < bd) {
                bd = dd;
                best = h;
            }
        }
    return best;
}
static void gen_seed(jrun_t *r, int v)
{
    jr_gen_t *g = &r->g[v];
    g->rng = hash3(r->j->seed, (uint32_t)(r->cur << 8 | v), g->count++);
    g->rep = g->muts = 0;
    g->reps = (uint8_t)(2 + rndn(&g->rng, 3));
}
static void head_for_target(jrun_t *r, loop_t *m)
{
    int i;
    if (r->target == -128)
        return;
    for (i = m->len - 1; i >= 0; i--)
        if (m->step[i] & ST_ON) {
            m->step[i] = (uint8_t)(ST_ON | ST_MK(clampi(held_deg(r, r->target), -24, 39)));
            break;
        }
}
/* VOX's phrase: one to three long notes, then the space they ring out into. Eight steps of a bar
 * (eight bars), or of a half note when the moon is full: a note holds for one to three steps, its
 * neighbour a small step away; a breath may come before the first. The notes are the chord's tones
 * and the colours over them (its 7th, 9th, 6th), high up in the shimmer, sometimes an octave lower.
 * As it fills, the first note comes first: a thin moon plays only it, a full one the whole phrase.
 * Its rank is the note's place in the phrase x 3 (a note and its held steps share it) */
static void gen_vox(jrun_t *r)
{
    static const int8_t TONE[] = {0, 2, 4, 4, 6, 7, 8, 9, 11};
    static const int8_t MOVE[] = {-2, -1, -1, 1, 1, 2, 3, -3, 4};
    jr_gen_t *g = &r->g[LP_VOX];
    loop_t *m = &g->m;
    const float e = r->out.eff;
    int i, k, d, notes, low, idx = 0;
    gen_seed(r, LP_VOX);
    m->len = 8;
    m->div = (uint8_t)(e > 0.7f && rndn(&g->rng, 3) == 0 ? 8 : 16);
    for (i = 0; i < JR_STEPS; i++) {
        m->step[i] = 0;
        g->rank[i] = 0;
    }
    notes = 1 + (rndf(&g->rng) < 0.35f + 0.4f * e) + (rndf(&g->rng) < 0.15f + 0.35f * e);
    low = rndn(&g->rng, 10) < 3 ? -7 : 0;
    i = rndn(&g->rng, 4) == 0 ? 1 : 0;
    d = TONE[rndn(&g->rng, (int)sizeof TONE)];
    for (k = 0; k < notes && i < m->len; k++, idx++) {
        int hold = 1 + rndn(&g->rng, 3), h;
        d = guide_pick(r, d, &g->rng);
        m->step[i] = (uint8_t)(ST_ON | ST_MK(clampi(d + low, -24, 39)));
        g->rank[i] = (uint8_t)(idx * 3);
        for (h = 1; h <= hold && i + h < m->len - 1; h++) {   /* (the last step stays a rest: the space) */
            m->step[i + h] = (uint8_t)(ST_ON | ST_TIE | ST_MK(clampi(d + low, -24, 39)));
            g->rank[i + h] = (uint8_t)(idx * 3);
        }
        i += h;
        d = clampi(d + MOVE[rndn(&g->rng, (int)sizeof MOVE)], -3, 11);
    }
    head_for_target(r, m);
}
/* PULSE's pattern: two bars of 8ths, fuller with the band (the scene's pulse, the moon, DENSITY) */
static int pulse_band(const jrun_t *r)
{
    float a = clampf(r->out.pulse * (1.0f / 127.0f) * r->out.eff * 1.6f, 0.0f, 1.0f);
    return a < 0.15f ? 0 : a < 0.4f ? 1 : a < 0.7f ? 2 : 3;
}
static void gen_pulse(jrun_t *r)
{
    static const uint8_t EXTRA[] = {6, 11, 14, 3};
    jr_gen_t *g = &r->g[LP_PULSE];
    loop_t *m = &g->m;
    int i, b = pulse_band(r);
    gen_seed(r, LP_PULSE);
    g->band = (uint8_t)b;
    m->len = 16;
    m->div = 2;
    for (i = 0; i < JR_STEPS; i++)
        m->step[i] = 0;
    if (b >= 1)
        m->step[0] = PS_KICK | PS_ACC;
    if (b >= 2) {
        m->step[8] = PS_KICK;
        if (rndn(&g->rng, 2)) {
            m->step[6] |= PS_TICK;
            m->step[14] |= PS_TICK;
        }
    }
    if (b >= 3) {
        m->step[EXTRA[rndn(&g->rng, 4)]] |= PS_KICK;
        for (i = 1; i < 16; i += 2)
            m->step[i] |= PS_TICK;
    }
    gen_rank(g);
}
static void gen(jrun_t *r, int v)
{
    if (v == LP_VOX)
        gen_vox(r);
    else
        gen_pulse(r);
}

/* a change of a loop, small. VOX: a note (and the steps it holds) a step or two away, so the phrase
 * keeps its shape and its space; PULSE: a tick */
static void mutate(loop_t *m, uint32_t *rng, int pulse)
{
    int t, i;
    if (m->len < 1)
        return;
    if (pulse) {
        i = 1 + 2 * rndn(rng, 8);
        if (i < m->len)
            m->step[i] ^= PS_TICK;
        return;
    }
    for (t = 0; t < 8; t++) {
        i = rndn(rng, m->len);
        if ((m->step[i] & ST_ON) && !(m->step[i] & ST_TIE)) {
            int d = clampi(ST_DEG(m->step[i]) + (rndn(rng, 2) ? 1 : -1) * (rndn(rng, 3) ? 1 : 2), -24, 39), k;
            m->step[i] = (uint8_t)(ST_ON | ST_MK(d));
            for (k = i + 1; k < m->len && (m->step[k] & ST_TIE); k++)
                m->step[k] = (uint8_t)(ST_ON | ST_TIE | ST_MK(d));
            return;
        }
    }
}

/* how many of a motif's steps play, of len: its rank must be under this */
static int keep_limit(const loop_t *m, float e, float lo, float hi)
{
    float k = clampf((e - lo) / (hi - lo), 0.0f, 1.0f);
    return (int)(k * (float)m->len + 0.999f);
}

/* ------------------------------------------------------------------ playing --- */
static void vox_quiet(jrun_t *r, const jr_sink_t *o)
{
    if (r->vox_note >= 0)
        o->vox_off(o->ctx, r->vox_note);
    r->vox_note = -1;
    r->vox_ok = 0;
}
static int play_vox(jrun_t *r, const loop_t *m, const uint8_t *rank, uint32_t n, const jr_sink_t *o)
{
    uint32_t div = m->div ? m->div : 1u, pos;
    int lim, note, st;
    if (n % div || !m->len)
        return 0;
    pos = (n / div) % m->len;
    st = m->step[pos];
    lim = rank ? keep_limit(m, r->out.eff, 0.15f, 0.8f) : JR_STEPS;
    if (!(st & ST_ON) || (rank && rank[pos] >= lim)) {
        vox_quiet(r, o);
        return pos == m->len - 1u;
    }
    {   /* GUIDE: a generated note near a held key's is that note, whatever the chord is now */
        int d = ST_DEG(st);
        if (rank && r->held) {
            int w, best = 99;
            for (w = 0; w < 16; w++)
                if (r->held >> w & 1u) {
                    int h = held_deg(r, w), dd = h - d;
                    for (; dd > 3; dd -= 7)       /* (any octave of it) */
                        ;
                    for (; dd < -3; dd += 7)
                        ;
                    if ((dd < 0 ? -dd : dd) < (best < 0 ? -best : best))
                        best = dd;
                }
            if (best != 99 && (best < 0 ? -best : best) <= 2)
                d += best;
        }
        note = mel_note(r, d, 1, m->on != LOOP_RECORDED);   /* (what you recorded plays as you played it) */
    }
    if (!((st & ST_TIE) && r->vox_ok && r->vox_note == note)) {
        vox_quiet(r, o);
        o->vox_on(o->ctx, note, rank ? 62 + (int)(26.0f * r->out.eff) : 96);
        r->vox_note = note;
    }
    r->vox_ok = 1;
    return pos == m->len - 1u;
}
static int play_pulse(jrun_t *r, const loop_t *m, const uint8_t *rank, uint32_t n, const jr_sink_t *o)
{
    uint32_t div = m->div ? m->div : 1u, pos;
    int st, lim;
    float a = 0.7f + 0.3f * clampf(r->out.pulse * (1.0f / 127.0f), 0.0f, 1.0f);
    if (n % div || !m->len)
        return 0;
    pos = (n / div) % m->len;
    st = m->step[pos];
    lim = rank ? keep_limit(m, r->out.eff, 0.2f, 0.9f) : JR_STEPS;
    if (st & PS_KICK)
        o->pulse(o->ctx, 0, (int)((st & PS_ACC ? 118.0f : 96.0f) * a));
    if ((st & PS_TICK) && (!rank || rank[pos] < lim))
        o->pulse(o->ctx, 1, (int)(56.0f * a));
    return pos == m->len - 1u;
}

/* after a generator's motif went round: again, or changed a little, or (after a few changes) new */
static void gen_round(jrun_t *r, int v)
{
    jr_gen_t *g = &r->g[v];
    if (v == LP_PULSE && pulse_band(r) != g->band) {
        gen(r, v);
        return;
    }
    if (++g->rep < g->reps)
        return;
    g->rep = 0;
    if (++g->muts > (v == LP_PULSE ? 6 : 4)) {
        gen(r, v);
        return;
    }
    mutate(&g->m, &g->rng, v == LP_PULSE);
}
static void loop_round(jrun_t *r, int v)
{
    loop_t *m = &r->j->loop[v];
    if (m->mutate && rndf(&r->lrng[v]) < (float)m->mutate * (0.7f / 127.0f))
        mutate(m, &r->lrng[v], v == LP_PULSE);
}

/* ------------------------------------------------------------------ director --- */
void jr_init(jrun_t *r, journey_t *j)
{
    uint8_t *z = (uint8_t *)r;
    uint32_t i;
    for (i = 0; i < sizeof *r; i++)
        z[i] = 0;
    r->j = j;
    r->vox_note = r->rec_held = r->rec_pending = -1;
    r->target = -128;
    r->intensity = 0.12f;
    r->moon = MOON_WAX;
    r->moon_len = 85.0f;
    r->step_len = 7350u;
    for (i = 0; i < NMACROS; i++)
        r->macro_s[i] = (float)j->macro[i];
    for (i = 0; i < JR_LOOPS; i++)
        r->lrng[i] = hash3(j->seed, 0x100u + i, 0);
    r->cur = r->from = r->dest = (uint8_t)(j->at % nsc(j));
    jr_tick(r, 0, 0);
}

static void regen_all(jrun_t *r)
{
    int v;
    for (v = 0; v < JR_LOOPS; v++) {
        r->g[v].count = 0;
        gen(r, v);
    }
}

void jr_start(jrun_t *r, const jr_sink_t *o)
{
    const scene_t *s;
    r->cur = r->from = r->dest = (uint8_t)(r->j->at % nsc(r->j));
    r->travelling = 0;
    r->xf = 0.0f;
    r->moon = MOON_WAX;                          /* in from the quiet, a third of the way up */
    r->moon_t = r->moon_len * 0.33f;
    r->intensity = 0.25f;
    r->chord = 0;
    r->vox_ok = 0;
    r->hands = 0;
    r->playing = 1;
    jr_tick(r, 0, o);
    regen_all(r);
    drones_to_chord(r, o);
    s = sc(r);
    o->drone_latch(o->ctx, s->drones & 0x1Fu ? s->drones & 0x1Fu : 0x07u);
}

void jr_stop(jrun_t *r, const jr_sink_t *o)
{
    vox_quiet(r, o);
    o->drone_release(o->ctx);
    r->playing = 0;
    r->rec = 0;
}

void jr_restart(jrun_t *r, const jr_sink_t *o)
{
    int v;
    for (v = 0; v < JR_LOOPS; v++)
        r->lrng[v] = hash3(r->j->seed, 0x100u + (uint32_t)v, 0);
    if (r->playing) {
        vox_quiet(r, o);
        jr_start(r, o);
        return;
    }
    r->cur = r->from = r->dest = (uint8_t)(r->j->at % nsc(r->j));
    r->travelling = 0;
    r->chord = 0;
    drones_to_chord(r, o);                       /* (the black keys play in its key) */
}

void jr_travel(jrun_t *r, int scene)
{
    int n = nsc(r->j);
    if (scene < 0 || scene >= n || (scene == r->cur && !r->travelling) || scene == r->dest)
        return;
    r->from = r->cur;
    r->dest = (uint8_t)scene;
    r->xf = 0.0f;
    r->switched = 0;
    r->travelling = 1;
    r->travel_s = r->macro_s[MAC_DRIFT] < 2.0f ? 20.0f : clampf(r->moon_len, 20.0f, 240.0f);
}

static float lerp_u8(uint8_t a, uint8_t b, float t) { return (float)a + ((float)b - (float)a) * t; }

void jr_tick(jrun_t *r, uint32_t frames, const jr_sink_t *o)
{
    const journey_t *j = r->j;
    const float dt = (float)frames * (1.0f / FS);
    const scene_t *a, *b;
    float t, dm;
    int k;
    for (k = 0; k < NMACROS; k++)
        r->macro_s[k] += ((float)j->macro[k] - r->macro_s[k]) * (frames ? 0.06f : 1.0f);
    r->since_step += frames;
    r->out.fade = clampf(r->out.fade + (r->playing ? dt : -dt) * 0.5f, 0.0f, 1.0f);   /* 2 s in, out */
    if (r->playing && frames) {
        const float drift = r->macro_s[MAC_DRIFT];
        float p, tgt;
        r->moon_len = 240.0f * (drift < 2.0f ? 1.0f : 1.0f / (1.0f + drift * (7.0f / 127.0f)));   /* 240 .. 30 s */
        if (drift >= 2.0f && (r->moon_t += dt) >= r->moon_len) {
            r->moon_t = 0.0f;
            r->moon = (uint8_t)((r->moon + 1) % NMOONS);
            if (r->moon == MOON_NEW && nsc(j) > 1 && !r->travelling)
                jr_travel(r, (r->cur + 1) % nsc(j));
        }
        p = clampf(r->moon_t / r->moon_len, 0.0f, 1.0f);
        tgt = r->moon == MOON_NEW ? 0.12f : r->moon == MOON_WAX ? 0.12f + 0.88f * p : r->moon == MOON_FULL ? 1.0f
                                                                                                         : 1.0f - 0.88f * p;
        r->intensity += (tgt - r->intensity) * clampf(dt * (1.0f / 3.0f), 0.0f, 1.0f);
        if (r->travelling) {
            r->xf += dt / r->travel_s;
            if (r->xf >= 0.5f && !r->switched) {  /* the middle: the new key, its chords, new motifs */
                r->switched = 1;
                r->cur = r->dest;
                r->j->at = r->cur;
                r->chord = 0;
                if (o) {
                    regen_all(r);
                    drones_to_chord(r, o);
                }
            }
            if (r->xf >= 1.0f) {
                r->travelling = 0;
                r->from = r->cur;
                r->xf = 0.0f;
            }
        }
    }
    if (o) {                                     /* the scene edited (SCENE page): the drone follows now */
        const scene_t *s = sc(r);
        uint32_t sig = (uint32_t)s->root | (uint32_t)s->scale << 4 | (uint32_t)s->prog << 8 | (uint32_t)s->voicing << 16 |
                       (uint32_t)r->cur << 24;
        if (sig != r->sig) {
            r->sig = sig;
            r->chord = (uint8_t)(r->chord % prog_of(s)->n);
            drones_to_chord(r, o);
        }
    }
    a = &j->scene[(r->travelling ? r->from : r->cur) % nsc(j)];
    b = &j->scene[(r->travelling ? r->dest : r->cur) % nsc(j)];
    t = r->travelling ? r->xf * r->xf * (3.0f - 2.0f * r->xf) : 0.0f;
    r->out.density = lerp_u8(a->density, b->density, t);
    r->out.bright = lerp_u8(a->bright, b->bright, t);
    r->out.swarm = lerp_u8(a->swarm, b->swarm, t);
    r->out.space = lerp_u8(a->space, b->space, t);
    r->out.tex = lerp_u8(a->tex, b->tex, t);
    r->out.colour = lerp_u8(a->colour, b->colour, t);
    r->out.pulse = lerp_u8(a->pulse, b->pulse, t);
    r->out.intensity = r->intensity;
    dm = r->macro_s[MAC_DENSITY] * (1.0f / 64.0f);
    r->out.eff = clampf(r->intensity * r->out.density * (1.4f / 127.0f) * dm, 0.0f, 1.0f);
    r->out.root_midi = 24 + sc(r)->root % 12;
}

/* ---------------------------------------------------------------- the steps --- */
static void rec_step(jrun_t *r)
{
    loop_t *m = &r->j->loop[LP_VOX];
    uint8_t st = 0;
    if (r->rec_pending >= 0) {
        st = (uint8_t)(ST_ON | ST_MK(note_deg(r, r->rec_pending)));
        r->rec_held = r->rec_pending;
        r->rec_pending = -1;
    } else if (r->rec_held >= 0) {
        st = (uint8_t)(ST_ON | ST_TIE | ST_MK(note_deg(r, r->rec_held)));
    }
    m->step[r->rec_pos] = st;
    if (++r->rec_pos >= JR_STEPS) {               /* the bar is in: it plays */
        r->rec = 0;
        m->len = JR_STEPS;
        m->div = 1;
        m->on = LOOP_RECORDED;
    }
}

void jr_step(jrun_t *r, uint32_t n, uint32_t step_frames, const jr_sink_t *o)
{
    journey_t *j = r->j;
    int v;
    r->since_step = 0;
    r->step_len = step_frames ? step_frames : r->step_len;
    r->now_n = n;
    if (!r->playing)
        return;
    if ((n & 15u) == 0u) {                       /* a bar line */
        uint32_t bar = n >> 4, bpc = r->intensity > 0.7f ? 2u : r->intensity > 0.35f ? 4u : 8u;
        const prog_t *p = prog_of(sc(r));
        if (bar && bar % bpc == 0u && p->n > 1) {
            r->chord = (uint8_t)((r->chord + 1) % p->n);
            drones_to_chord(r, o);
        }
        if (r->rec == 1) {                       /* recording starts on the bar */
            int i;
            r->rec = 2;
            r->rec_pos = 0;
            r->rec_pending = -1;
            j->loop[LP_VOX].on = 0;
            for (i = 0; i < JR_STEPS; i++)
                j->loop[LP_VOX].step[i] = 0;
        }
    }
    if (r->rec == 2)
        rec_step(r);
    for (v = 0; v < JR_LOOPS; v++) {
        loop_t *lp = &j->loop[v];
        int cap = lp->on, done;
        const loop_t *m = cap ? lp : &r->g[v].m;
        const uint8_t *rank = cap ? 0 : r->g[v].rank;
        if (v == LP_VOX && !cap && (r->rec || (j->mode == MODE_PLAY && r->hands && n - r->hand_n < JR_HANDS_STEPS))) {
            if (r->vox_ok || r->vox_note >= 0)
                vox_quiet(r, o);                 /* PLAY: while you play VOX, the generator listens */
            continue;
        }
        done = v == LP_VOX ? play_vox(r, m, rank, n, o) : play_pulse(r, m, rank, n, o);
        if (done) {
            if (cap)
                loop_round(r, v);
            else
                gen_round(r, v);
        }
    }
}

/* ------------------------------------------------------------------ the hands --- */
void jr_guide(jrun_t *r, int w, int down)
{
    if (w < 0 || w > 15)
        return;
    if (down) {
        r->held |= 1u << w;
        r->target = (int8_t)w;
        r->g[LP_VOX].muts = 99;                  /* the next phrase heads there */
        r->g[LP_VOX].rep = r->g[LP_VOX].reps;
    } else {
        r->held &= ~(1u << w);
    }
    if (r->held) {                               /* low keys pull the register down, high keys up */
        int k, sum = 0, cnt = 0;
        for (k = 0; k < 16; k++)
            if (r->held >> k & 1u) {
                sum += k;
                cnt++;
            }
        r->shift = (int8_t)clampi((2 * sum - 15 * cnt) * 2 / (5 * cnt), -3, 3);
    }
}

void jr_vox_played(jrun_t *r, int midi, int on)
{
    if (on) {                                    /* the hands are on VOX: the generator gives way */
        r->hands = 1;
        r->hand_n = r->now_n;
    }
    if (r->rec != 2)
        return;
    if (on) {
        if (r->rec_pos > 0 && r->since_step < r->step_len / 2u) {   /* early in its step: that step */
            r->j->loop[LP_VOX].step[r->rec_pos - 1] = (uint8_t)(ST_ON | ST_MK(note_deg(r, midi)));
            r->rec_held = midi;
        } else {
            r->rec_pending = midi;
        }
    } else if (midi == r->rec_held) {
        r->rec_held = -1;
    }
}

void jr_record(jrun_t *r)
{
    if (r->rec) {
        r->rec = 0;
        return;
    }
    r->rec = 1;
    r->rec_held = r->rec_pending = -1;
}

void jr_capture(jrun_t *r, uint32_t mask)
{
    int v, i;
    for (v = 0; v < JR_LOOPS; v++) {
        loop_t *lp = &r->j->loop[v];
        const jr_gen_t *g = &r->g[v];
        int lim;
        if (!(mask >> v & 1u) || (v == LP_VOX && r->j->mode == MODE_PLAY))
            continue;
        lim = v == LP_VOX ? keep_limit(&g->m, r->out.eff, 0.15f, 0.8f) : JR_STEPS;
        for (i = 0; i < JR_STEPS; i++)            /* what is heard: the steps not playing yet are rests */
            lp->step[i] = i < g->m.len && g->rank[i] < lim ? g->m.step[i] : (v == LP_PULSE ? g->m.step[i] & PS_KICK : 0u);
        lp->len = g->m.len ? g->m.len : 16;
        lp->div = g->m.div ? g->m.div : 2;
        lp->on = LOOP_CAPTURED;
    }
}

void jr_release(jrun_t *r, uint32_t mask)
{
    int v;
    for (v = 0; v < JR_LOOPS; v++)
        if (mask >> v & 1u)
            r->j->loop[v].on = 0;
    if (mask >> LP_VOX & 1u)
        r->rec = 0;
}

void jr_scene_now(const jrun_t *r, scene_t *s)
{
    *s = *sc(r);
    s->density = (uint8_t)(r->out.density + 0.5f);
    s->bright = (uint8_t)(r->out.bright + 0.5f);
    s->swarm = (uint8_t)(r->out.swarm + 0.5f);
    s->space = (uint8_t)(r->out.space + 0.5f);
    s->tex = (uint8_t)(r->out.tex + 0.5f);
    s->colour = (uint8_t)(r->out.colour + 0.5f);
    s->pulse = (uint8_t)(r->out.pulse + 0.5f);
}
