/* SPDX-License-Identifier: GPL-3.0-only */
/* Penumbra's journeys; see journey.h. */
#include "journey.h"

#define FS 44100.0f

/* ------------------------------------------------------------- the music --- */
static const uint8_t SCALE[NSCALES][7] = {
    {0, 2, 4, 5, 7, 9, 11}, {0, 2, 3, 5, 7, 9, 10}, {0, 1, 3, 5, 7, 8, 10}, {0, 2, 4, 6, 7, 9, 11},
    {0, 2, 4, 5, 7, 9, 10}, {0, 2, 3, 5, 7, 8, 10}, {0, 2, 3, 5, 7, 8, 11}};
static const char *const SCALE_N[NSCALES] = {"IONIAN", "DORIAN", "PHRYGIAN", "LYDIAN", "MIXOLYDIAN", "AEOLIAN",
                                             "HARM MINOR"};
static const char *const ROOT_N[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
/* the drone's five voices over its root, in scale degrees (root, fifth, octave, ...) */
static const int8_t VOICING[NVOICINGS][JR_DRONES] = {{0, 4, 7, 11, 14}, {0, 4, 7, 9, 14}, {0, 7, 11, 14, 16},
                                                     {0, 4, 7, 8, 11}};
static const char *const VOICING_N[NVOICINGS] = {"OPEN", "WARM", "HIGH", "CLUSTER"};

const char *jr_scale_name(int s) { return SCALE_N[(unsigned)s % NSCALES]; }
const char *jr_root_name(int pc) { return ROOT_N[(unsigned)pc % 12u]; }
const char *jr_voicing_name(int v) { return VOICING_N[(unsigned)v % NVOICINGS]; }

/* --------------------------------------------------------------- presets --- */
/* each journey keeps its drone's root from scene to scene: where the key moves, the pedal stays (a
 * scene's pedal is 1 + its pitch class), so the drone changes only its voicing's colour. (The unused
 * fields keep their old values: a journey saved by 0.15 reads the same.) */
#define SCN(root, scale, prog, voc, dens, bri, swm, spc, tex, col, pul, reg, dr, pen, ped) \
    {root, scale, prog, voc, dens, bri, swm, spc, tex, col, pul, reg, dr, pen, ped, 0}
typedef struct { const char *name; uint32_t seed; uint8_t n; scene_t s[4]; } preset_t;
static const preset_t PRESET[JR_PRESETS] = {
    {"ECLIPSE", 0x5EC1u, 4, {SCN(9, SC_AEOLIAN, 7, VC_WARM, 60, 60, 80, 80, 40, 50, 0, 4, 0x07, 1, 0),
                             SCN(9, SC_AEOLIAN, 1, VC_OPEN, 85, 72, 95, 70, 30, 70, 40, 4, 0x1F, 0, 0),
                             SCN(5, SC_LYDIAN, 4, VC_HIGH, 70, 80, 70, 90, 50, 85, 20, 4, 0x1D, 0, 10),
                             SCN(9, SC_DORIAN, 2, VC_WARM, 45, 55, 90, 85, 60, 45, 0, 4, 0x0B, 1, 0)}},
    {"PENUMBRA", 0x9E7Bu, 3, {SCN(2, SC_DORIAN, 2, VC_WARM, 70, 65, 85, 75, 35, 55, 50, 4, 0x1F, 1, 0),
                              SCN(2, SC_DORIAN, 9, VC_OPEN, 90, 75, 90, 70, 25, 65, 70, 4, 0x1F, 0, 0),
                              SCN(7, SC_MIXOLYDIAN, 3, VC_WARM, 60, 70, 70, 80, 40, 70, 30, 4, 0x0F, 1, 3)}},
    {"SELENE", 0x5E1Eu, 3, {SCN(4, SC_PHRYGIAN, 4, VC_CLUSTER, 35, 40, 100, 95, 70, 35, 0, 3, 0x13, 0, 0),
                            SCN(4, SC_PHRYGIAN, 9, VC_OPEN, 55, 48, 110, 90, 60, 40, 25, 4, 0x1F, 1, 0),
                            SCN(0, SC_LYDIAN, 0, VC_HIGH, 30, 60, 80, 100, 80, 60, 0, 4, 0x11, 0, 5)}},
    {"TRANQUILITY", 0x7A9Cu, 3, {SCN(0, SC_LYDIAN, 4, VC_HIGH, 55, 85, 70, 90, 30, 90, 0, 4, 0x1F, 0, 0),
                                 SCN(7, SC_IONIAN, 8, VC_WARM, 70, 80, 75, 85, 25, 80, 20, 4, 0x1F, 1, 1),
                                 SCN(4, SC_AEOLIAN, 7, VC_WARM, 60, 70, 80, 85, 35, 70, 10, 4, 0x1F, 1, 1)}},
    {"PERIGEE", 0x9E21u, 3, {SCN(6, SC_AEOLIAN, 7, VC_OPEN, 95, 70, 75, 60, 20, 60, 85, 4, 0x1F, 1, 0),
                             SCN(6, SC_DORIAN, 2, VC_WARM, 110, 80, 80, 55, 20, 70, 100, 4, 0x1F, 0, 0),
                             SCN(2, SC_LYDIAN, 4, VC_HIGH, 80, 85, 70, 70, 25, 80, 60, 4, 0x1D, 1, 7)}},
    {"FAR SIDE", 0xFA75u, 4, {SCN(7, SC_HARMONIC, 1, VC_WARM, 50, 50, 95, 100, 75, 30, 0, 4, 0x1F, 0, 0),
                              SCN(3, SC_LYDIAN, 0, VC_HIGH, 30, 55, 90, 110, 85, 50, 0, 4, 0x15, 0, 8),
                              SCN(0, SC_AEOLIAN, 7, VC_WARM, 70, 60, 85, 90, 50, 45, 30, 4, 0x1F, 1, 8),
                              SCN(7, SC_HARMONIC, 9, VC_CLUSTER, 40, 45, 110, 100, 70, 35, 0, 3, 0x13, 0, 0)}}};

const char *jr_preset_name(int i) { return PRESET[(unsigned)i % JR_PRESETS].name; }

/* the sound as made, the engine's meaning (engine.h SH_*, DV_*): each voice's level (100), its SHAPE
 * knobs and the held-only one; each drone voice's level, tone, detune (64: none) and octave (1: as is) */
void jr_sound_default(journey_t *j)
{
    static const uint8_t SHAPE[JR_VOICES][JR_SHAPE] = {
        {100, 75, 45, 80, 44, 0},                /* DRONE: FILTER, BODY, SWARM, WAVE */
        {100, 64, 80, 90, 64, 20},               /* VOX: BRIGHT, ATTACK, RELEASE, SHIMMER; VIBRATO */
        {80, 50, 80, 40, 64, 0},                 /* PULSE: DROP, DECAY, TICK, SPACE (2 dB down) */
        {100, 64, 70, 40, 90, 0}};               /* TEXTURE: COLOUR, SWEEP, RATE, WIDTH */
    static const uint8_t DVOICE[JR_DVOICE] = {106, 75, 64, 1};
    int v, f;
    for (v = 0; v < JR_VOICES; v++)
        for (f = 0; f < JR_SHAPE; f++)
            j->shape[v][f] = SHAPE[v][f];
    for (v = 0; v < JR_DRONES; v++)
        for (f = 0; f < JR_DVOICE; f++)
            j->dvoice[v][f] = DVOICE[f];
    j->dswell = 0;
    jr_vsound_default(j);
}

/* VOX's sounds (engine.h VS_*): LEVEL, BRIGHT (64: the scene's), ATTACK, RELEASE, SHIMMER (64: as made),
 * VIBRATO, WAVE, FILTER (low-pass .. band-pass), RESO, GLIDE, DETUNE, B OCTAVE (0 down, 1, 2 up), B MIX, ENV */
static const char *const VSOUND_N[JR_VSOUNDS] = {"GLASS", "REED", "HALO", "LOW"};
const char *jr_vsound_name(int i) { return VSOUND_N[(unsigned)i % JR_VSOUNDS]; }
void jr_vsound_default(journey_t *j)
{
    static const uint8_t VS[JR_VSOUNDS][JR_VS] = {
        {100, 64, 80, 90, 64, 20, 0, 0, 25, 0, 40, 1, 64, 50},      /* GLASS: the two saws, as VOX was */
        {92, 60, 64, 82, 56, 14, 96, 92, 64, 0, 18, 1, 30, 44},     /* REED: a narrow pulse, band-pass, closer */
        {88, 70, 98, 112, 92, 28, 24, 12, 20, 0, 78, 2, 70, 30},    /* HALO: wide, an octave over, long, far back */
        {106, 40, 70, 86, 48, 10, 64, 0, 36, 44, 28, 0, 60, 36}};   /* LOW: square, an octave under, gliding */
    int s, f;
    for (s = 0; s < JR_VSOUNDS; s++)
        for (f = 0; f < JR_VS; f++)
            j->vsound[s][f] = VS[s][f];
    j->vcur = 0;
}

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
    for (v = 0; v < NMACROS; v++)
        dst->macro[v] = 64;
    for (v = 0; v < p->n; v++)
        dst->scene[v] = p->s[v];
    jr_sound_default(dst);
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
static int reg_of(const scene_t *s) { return clampi(s->reg, 2, 6); }

/* a scale degree's semitones over the root (any degree, below 0 too) */
static int deg_semi(int scale, int d)
{
    int o = d >= 0 ? d / 7 : -((6 - d) / 7);
    return 12 * o + SCALE[scale][d - 7 * o];
}

int jr_key_note(const jrun_t *r, int w, int oct)
{
    const scene_t *s = sc(r);
    return clampi(12 * reg_of(s) + s->root % 12 + deg_semi(scale_of(s), w + 7 * oct), 0, 127);
}

/* ------------------------------------------------------------------ the drone --- */
#define DRONE_LO 21                              /* its range (dsp/drone.h NOTE: 9 .. 69) */
#define DRONE_HI 69
/* the scene's pedal as a degree of its key (not in the key, or none: the key's root) */
static int pedal_deg(const scene_t *s)
{
    int scale = scale_of(s), pc, i;
    if (!s->pedal)
        return 0;
    pc = ((s->pedal - 1 - s->root) % 12 + 12) % 12;
    for (i = 0; i < 7; i++)
        if (SCALE[scale][i] == pc)
            return i;
    return 0;
}
static uint32_t drone_sig(const jrun_t *r)
{
    const scene_t *s = sc(r);
    return (uint32_t)s->root | (uint32_t)s->scale << 4 | (uint32_t)s->pedal << 8 | (uint32_t)s->voicing << 16 | 1u << 31;
}
static int pc_of(int m) { return ((m % 12) + 12) % 12; }
static int nearest_oct(int m, int cur)           /* m's octave nearest cur, in the drone's range */
{
    int k, best = m, bd = 999;
    for (k = -36; k <= 36; k += 12) {
        int c = m + k, d = c > cur ? c - cur : cur - c;
        if (c >= DRONE_LO && c <= DRONE_HI && d < bd) {
            bd = d;
            best = c;
        }
    }
    return best;
}
/* the scene's voicing over its pedal, the bass in octave 1. now: every voice there at once (the start).
 * Else the bass to the pedal, and of the others, a voice whose note the new voicing has (in any
 * octave) stays where it is; the rest take the notes still missing, each at its nearest octave, and
 * move one at a time (jr_tick). OPEN and HIGH, say, are the same notes: nothing moves */
static void drone_to_scene(jrun_t *r, int now, const jr_sink_t *o)
{
    const scene_t *s = sc(r);
    int v, i, pd = pedal_deg(s), voc = s->voicing % NVOICINGS, base = 24 + s->root % 12, want[JR_DRONES];
    uint32_t have = 0, need = 0, kept = 0;
    while (base + deg_semi(scale_of(s), pd) > 35)
        base -= 12;
    for (v = 0; v < JR_DRONES; v++) {
        int m = base + deg_semi(scale_of(s), pd + VOICING[voc][v]);
        while (m > DRONE_HI)
            m -= 12;
        want[v] = m;
        need |= 1u << pc_of(m);
    }
    r->sig = drone_sig(r);
    r->dpend = 0;
    r->dwait = 0.0f;                             /* (the first goes now) */
    for (v = 0; v < JR_DRONES; v++)
        r->dtarget[v] = want[v];
    if (!now && r->drone[0] > 0) {
        r->dtarget[0] = nearest_oct(want[0], r->drone[0]);
        have = 1u << pc_of(want[0]);
        for (v = 1; v < JR_DRONES; v++)
            if (need >> pc_of(r->drone[v]) & 1u) {
                kept |= 1u << v;
                have |= 1u << pc_of(r->drone[v]);
                r->dtarget[v] = r->drone[v];
            }
        for (v = 1; v < JR_DRONES; v++) {
            int m = want[v];
            if (kept >> v & 1u)
                continue;
            for (i = 1; i < JR_DRONES; i++)       /* a note still missing, else its own */
                if (!(have >> pc_of(want[i]) & 1u)) {
                    m = want[i];
                    break;
                }
            have |= 1u << pc_of(m);
            r->dtarget[v] = nearest_oct(m, r->drone[v]);
        }
    }
    for (v = 0; v < JR_DRONES; v++) {
        if (now || r->drone[v] <= 0) {
            r->drone[v] = r->dtarget[v];
            if (o)
                o->drone_note(o->ctx, v, r->drone[v]);
        } else if (r->dtarget[v] != r->drone[v]) {
            r->dpend |= (uint8_t)(1u << v);
        }
    }
}
static void drone_moves(jrun_t *r, float dt, const jr_sink_t *o)
{
    int v;
    if (!r->dpend || (r->dwait -= dt) > 0.0f)
        return;
    for (v = JR_DRONES - 1; v >= 0; v--)        /* the top first, the bass last */
        if (r->dpend >> v & 1u) {
            r->dpend &= (uint8_t)~(1u << v);
            r->drone[v] = r->dtarget[v];
            o->drone_note(o->ctx, v, r->drone[v]);
            r->dwait = JR_DRONE_GAP;
            return;
        }
}

/* ------------------------------------------------------------------ the journey --- */
void jr_init(jrun_t *r, journey_t *j)
{
    uint8_t *z = (uint8_t *)r;
    uint32_t i;
    for (i = 0; i < sizeof *r; i++)
        z[i] = 0;
    r->j = j;
    for (i = 0; i < NMACROS; i++)
        r->macro_s[i] = (float)j->macro[i];
    r->srng = hash3(j->seed, 0x200u, 0);
    r->cur = r->from = r->dest = (uint8_t)(j->at % nsc(j));
    jr_tick(r, 0, 0);
}

void jr_start(jrun_t *r, const jr_sink_t *o)
{
    const scene_t *s = sc(r);
    int v;
    r->playing = 1;
    jr_tick(r, 0, o);
    drone_to_scene(r, 1, o);
    o->drone_latch(o->ctx, (s->drones & 0x1Fu ? s->drones & 0x1Fu : 0x07u) & ~(uint32_t)r->j->dswell);
    for (v = 0; v < JR_DRONES; v++)              /* the ones that come and go: the first soon */
        r->swell_wait[v] = (uint8_t)(1 + v + rndn(&r->srng, 3));
}

void jr_stop(jrun_t *r, const jr_sink_t *o)
{
    o->drone_release(o->ctx);
    r->playing = 0;
}

void jr_restart(jrun_t *r, const jr_sink_t *o)
{
    r->cur = r->from = r->dest = (uint8_t)(r->j->at % nsc(r->j));
    r->travelling = 0;
    r->xf = 0.0f;
    r->srng = hash3(r->j->seed, 0x200u, 0);
    if (r->playing) {
        jr_start(r, o);
        return;
    }
    drone_to_scene(r, 1, o);                     /* (the black keys play in its key) */
}

void jr_travel(jrun_t *r, int scene)
{
    int n = nsc(r->j);
    if (scene < 0 || scene >= n || (scene == r->cur && !r->travelling) || scene == r->dest)
        return;
    r->from = r->travelling ? r->dest : r->cur; /* (on the way already: from where it is heading) */
    r->dest = (uint8_t)scene;
    r->xf = 0.0f;
    r->switched = 0;
    r->travelling = 1;
}

static float lerp_u8(uint8_t a, uint8_t b, float t) { return (float)a + ((float)b - (float)a) * t; }

void jr_tick(jrun_t *r, uint32_t frames, const jr_sink_t *o)
{
    const journey_t *j = r->j;
    const float dt = (float)frames * (1.0f / FS);
    const scene_t *a, *b;
    float t;
    int k;
    for (k = 0; k < NMACROS; k++)
        r->macro_s[k] += ((float)j->macro[k] - r->macro_s[k]) * (frames ? 0.06f : 1.0f);
    r->out.fade = clampf(r->out.fade + (r->playing ? dt : -dt) * 0.5f, 0.0f, 1.0f);   /* 2 s in, out */
    if (r->travelling && frames) {
        r->xf += dt * (1.0f / JR_TRAVEL_S);
        if (r->xf >= 0.5f && !r->switched) {      /* the middle: the new key (the drone: below) */
            r->switched = 1;
            r->cur = r->dest;
            r->j->at = r->cur;
        }
        if (r->xf >= 1.0f) {
            r->travelling = 0;
            r->from = r->cur;
            r->xf = 0.0f;
        }
    }
    if (o) {                                     /* a new scene, or this one edited (SCENE): the drone moves */
        if (drone_sig(r) != r->sig)
            drone_to_scene(r, 0, o);
        drone_moves(r, dt, o);
    }
    a = &j->scene[(r->travelling ? r->from : r->cur) % nsc(j)];
    b = &j->scene[(r->travelling ? r->dest : r->cur) % nsc(j)];
    t = r->travelling ? r->xf * r->xf * (3.0f - 2.0f * r->xf) : 0.0f;
    r->out.bright = lerp_u8(a->bright, b->bright, t);
    r->out.space = lerp_u8(a->space, b->space, t);
    r->out.tex = lerp_u8(a->tex, b->tex, t);
    r->out.colour = lerp_u8(a->colour, b->colour, t);
    r->out.root_midi = 24 + sc(r)->root % 12;
}

void jr_step(jrun_t *r, uint32_t n, const jr_sink_t *o)
{
    int v;
    if (!r->playing || (n & 15u))
        return;
    for (v = 0; v < JR_DRONES; v++)              /* a bar line: the voices that come and go, every 3 .. 18 bars */
        if (r->j->dswell >> v & 1u) {
            if (r->swell_wait[v]) {
                r->swell_wait[v]--;
                continue;
            }
            o->drone_swell(o->ctx, v);
            r->swell_wait[v] = (uint8_t)(3 + rndn(&r->srng, 16));
        }
}

void jr_scene_now(const jrun_t *r, scene_t *s)
{
    *s = *sc(r);
    s->bright = (uint8_t)(r->out.bright + 0.5f);
    s->space = (uint8_t)(r->out.space + 0.5f);
    s->tex = (uint8_t)(r->out.tex + 0.5f);
    s->colour = (uint8_t)(r->out.colour + 0.5f);
}
