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
 * scene's pedal is 1 + its pitch class), so the drone changes only its voicing's colour. Each scene has
 * a space of its own (ROOM, HALO): a journey travels from one room to the next. (The unused fields keep
 * their old values.) */
#define SCN(root, scale, prog, voc, room, bri, halo, spc, tex, col, pul, reg, dr, pen, ped) \
    {root, scale, prog, voc, room, bri, halo, spc, tex, col, pul, reg, dr, pen, ped, 0}
typedef struct { const char *name; uint32_t seed; uint8_t n; scene_t s[4]; } preset_t;
static const preset_t PRESET[JR_PRESETS] = {
    {"ECLIPSE", 0x5EC1u, 4, {SCN(9, SC_AEOLIAN, 7, VC_WARM, 64, 60, 64, 80, 40, 50, 0, 4, 0x07, 1, 0),
                             SCN(9, SC_AEOLIAN, 1, VC_OPEN, 82, 72, 74, 70, 30, 70, 40, 4, 0x1F, 0, 0),
                             SCN(5, SC_LYDIAN, 4, VC_HIGH, 100, 80, 92, 90, 50, 85, 20, 4, 0x1D, 0, 10),
                             SCN(9, SC_DORIAN, 2, VC_WARM, 72, 55, 60, 85, 60, 45, 0, 4, 0x0B, 1, 0)}},
    {"PENUMBRA", 0x9E7Bu, 3, {SCN(2, SC_DORIAN, 2, VC_WARM, 70, 65, 64, 75, 35, 55, 50, 4, 0x1F, 1, 0),
                              SCN(2, SC_DORIAN, 9, VC_OPEN, 86, 75, 82, 70, 25, 65, 70, 4, 0x1F, 0, 0),
                              SCN(7, SC_MIXOLYDIAN, 3, VC_WARM, 60, 70, 70, 80, 40, 70, 30, 4, 0x0F, 1, 3)}},
    {"SELENE", 0x5E1Eu, 3, {SCN(4, SC_PHRYGIAN, 4, VC_CLUSTER, 92, 40, 82, 95, 70, 35, 0, 3, 0x13, 0, 0),
                            SCN(4, SC_PHRYGIAN, 9, VC_OPEN, 76, 48, 64, 90, 60, 40, 25, 4, 0x1F, 1, 0),
                            SCN(0, SC_LYDIAN, 0, VC_HIGH, 112, 60, 104, 100, 80, 60, 0, 4, 0x11, 0, 5)}},
    {"TRANQUILITY", 0x7A9Cu, 3, {SCN(0, SC_LYDIAN, 4, VC_HIGH, 74, 85, 92, 90, 30, 90, 0, 4, 0x1F, 0, 0),
                                 SCN(7, SC_IONIAN, 8, VC_WARM, 64, 80, 70, 85, 25, 80, 20, 4, 0x1F, 1, 1),
                                 SCN(4, SC_AEOLIAN, 7, VC_WARM, 82, 70, 76, 85, 35, 70, 10, 4, 0x1F, 1, 1)}},
    {"PERIGEE", 0x9E21u, 3, {SCN(6, SC_AEOLIAN, 7, VC_OPEN, 50, 70, 50, 60, 20, 60, 85, 4, 0x1F, 1, 0),
                             SCN(6, SC_DORIAN, 2, VC_WARM, 56, 80, 60, 55, 20, 70, 100, 4, 0x1F, 0, 0),
                             SCN(2, SC_LYDIAN, 4, VC_HIGH, 74, 85, 78, 70, 25, 80, 60, 4, 0x1D, 1, 7)}},
    {"FAR SIDE", 0xFA75u, 4, {SCN(7, SC_HARMONIC, 1, VC_WARM, 98, 50, 78, 100, 75, 30, 0, 4, 0x1F, 0, 0),
                              SCN(3, SC_LYDIAN, 0, VC_HIGH, 118, 55, 98, 110, 85, 50, 0, 4, 0x15, 0, 8),
                              SCN(0, SC_AEOLIAN, 7, VC_WARM, 84, 60, 70, 90, 50, 45, 30, 4, 0x1F, 1, 8),
                              SCN(7, SC_HARMONIC, 9, VC_CLUSTER, 106, 45, 86, 100, 70, 35, 0, 3, 0x13, 0, 0)}}};

const char *jr_preset_name(int i) { return PRESET[(unsigned)i % JR_PRESETS].name; }

/* each journey's own sound, over the one as made (engine.h's meanings): the drone's FILTER, BODY, SWARM and
 * WAVE; its movement (DM_*); TIDES (TD_*); PAN (each part's place) and its RATE (0: the tide); how far each
 * part sways and their OFFSET; the drone voices that come and go (bit v); the VOX sound it starts on.
 *   ECLIPSE      as made, VOX on a gentle tide; its five drone voices all come and go (sparse, and the black
 *                keys play them).
 *   PENUMBRA     a reedy drone; VOX (REED) and the wind mirrored, swaying every 16 s.
 *   SELENE       an uneasy, wandering tide; the high voices come and go; VOX (HALO) and the drone a quarter
 *                tide apart on it.
 *   TRANQUILITY  soft and still: a three-minute even tide; VOX and the wind drifting together every 40 s.
 *   PERIGEE      close and quick: a short tide, VOX (LOW) on the left, the kick and the wind swinging round it
 *                every 4 s.
 *   FAR SIDE     vast: a four-minute wandering tide, three voices coming and going, every part taking its turn
 *                round the field every 24 s. */
typedef struct {
    uint8_t drone[4], dm[JR_DMOTION], td[JR_TIDE], pan[JR_PAN], rate, sway[JR_PAN], sway_off, dswell, vcur;
} character_t;
static const character_t CHARACTER[JR_PRESETS] = {
    {{75, 45, 80, 44}, {41, 25, 38, 25}, {64, 24, 20, 0, 64}, {64, 64, 64, 64}, 0, {0, 32, 0, 0}, 0, 0x1F, 0},
    {{70, 50, 70, 64}, {50, 30, 60, 30}, {84, 40, 30, 20, 30}, {64, 52, 64, 76}, 36, {0, 48, 0, 40}, 127, 0x00, 1},
    {{60, 62, 96, 30}, {30, 40, 50, 52}, {50, 50, 40, 30, 120}, {64, 76, 64, 44}, 0, {30, 44, 0, 0}, 64, 0x18, 2},
    {{86, 35, 60, 18}, {62, 15, 48, 14}, {112, 20, 30, 12, 0}, {64, 60, 64, 68}, 12, {0, 30, 0, 24}, 0, 0x00, 0},
    {{70, 56, 70, 72}, {24, 46, 44, 28}, {18, 30, 16, 40, 40}, {64, 36, 64, 64}, 72, {0, 0, 60, 70}, 90, 0x02, 3},
    {{60, 50, 100, 50}, {36, 30, 80, 40}, {127, 46, 50, 26, 90}, {64, 34, 92, 64}, 25, {50, 90, 70, 80}, 64, 0x16, 2}};

static void jr_character(journey_t *j, int i)
{
    const character_t *c = &CHARACTER[(unsigned)i % JR_PRESETS];
    int k;
    for (k = 0; k < 4; k++)
        j->shape[0][1 + k] = c->drone[k];        /* (DRONE's SH_K1 .. SH_K4) */
    for (k = 0; k < JR_DMOTION; k++)
        j->dmotion[k] = c->dm[k];
    for (k = 0; k < JR_TIDE; k++)
        j->tide[k] = c->td[k];
    for (k = 0; k < JR_PAN; k++) {
        j->pan[k] = c->pan[k];
        j->sway[k] = c->sway[k];
    }
    j->sway_rate = c->rate;
    j->sway_off = c->sway_off;
    j->dswell = c->dswell;
    j->vcur = c->vcur;
}

/* the sound as made, the engine's meaning (engine.h SH_*, DV_*): each voice's level (100), its SHAPE
 * knobs and the held-only one; each drone voice's level, tone, detune (64: none) and octave (1: as is) */
void jr_sound_default(journey_t *j)
{
    static const uint8_t SHAPE[JR_VOICES][JR_SHAPE] = {
        {100, 75, 45, 80, 44, 0},                /* DRONE: FILTER, BODY, SWARM, WAVE */
        {100, 64, 80, 90, 64, 20},               /* VOX: BRIGHT, ATTACK, RELEASE, SHIMMER; VIBRATO */
        {80, 50, 80, 30, 64, 90},                /* PULSE: DROP, DECAY, CLICK, SPACE (2 dB down); BLOOM's level */
        {100, 64, 70, 40, 90, 0}};               /* TEXTURE: COLOUR, SWEEP, RATE, WIDTH */
    static const uint8_t DVOICE[JR_DVOICE] = {106, 75, 64, 1};
    int v, f;
    for (v = 0; v < JR_VOICES; v++)
        for (f = 0; f < JR_SHAPE; f++)
            j->shape[v][f] = SHAPE[v][f];
    for (v = 0; v < JR_DRONES; v++)
        for (f = 0; f < JR_DVOICE; f++)
            j->dvoice[v][f] = DVOICE[f];
    j->dswell = 0x1F;                            /* (all five come and go, as ECLIPSE: since 1.4) */
    jr_vsound_default(j);
    jr_bloom_default(j);
    jr_motion_default(j);
    jr_pan_default(j);
}

/* each voice in the middle (the drone's own voices spread across it, as they always were); the tide
 * carrying VOX and the echoes a quarter of the way out */
void jr_pan_default(journey_t *j)
{
    int k;
    for (k = 0; k < JR_PAN; k++)
        j->pan[k] = 64;
    j->sway_rate = 0;
    jr_sway_default(j);
}
void jr_sway_default(journey_t *j)
{
    int k;
    for (k = 0; k < JR_PAN; k++)
        j->sway[k] = k == 1 ? 32 : 0;            /* (VOX: engine.h EV_VOX) */
    j->sway_off = 0;
}

/* the drone's movement (engine.h DM_*): its LFO's RATE (~14 s), SWEEP (over its filter), WIDTH (the voices
 * swinging apart), DRIFT (their tuning wandering), as the drone has always moved. TIDES (TD_*): one slow
 * swell, ~50 s, through the drone's filter and the space, a little irregular (SHAPE 64) */
void jr_motion_default(journey_t *j)
{
    static const uint8_t DM[JR_DMOTION] = {41, 25, 38, 25}, TD[JR_TIDE] = {64, 24, 20, 0, 64};
    int k;
    for (k = 0; k < JR_DMOTION; k++)
        j->dmotion[k] = DM[k];
    for (k = 0; k < JR_TIDE; k++)
        j->tide[k] = TD[k];
}

void jr_bloom_default(journey_t *j)              /* level 90, LENGTH 64, an octave over, FIFTH 64 */
{
    j->shape[2][5] = 90;                         /* (PULSE's SH_HIDE: engine.h) */
    j->bloom[0] = 64;
    j->bloom[1] = 1;
    j->bloom[2] = 64;
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

/* ---------------------------------------------------------------- the sound --- */
void jr_sound_get(const journey_t *j, jr_sound_t *s)
{
    uint32_t k;
    for (k = 0; k < sizeof s->shape; k++) (&s->shape[0][0])[k] = (&j->shape[0][0])[k];
    for (k = 0; k < sizeof s->dvoice; k++) (&s->dvoice[0][0])[k] = (&j->dvoice[0][0])[k];
    s->dswell = j->dswell;
    for (k = 0; k < sizeof s->bloom; k++) s->bloom[k] = j->bloom[k];
    for (k = 0; k < sizeof s->vsound; k++) (&s->vsound[0][0])[k] = (&j->vsound[0][0])[k];
    s->vcur = j->vcur;
    for (k = 0; k < JR_DMOTION; k++) s->dmotion[k] = j->dmotion[k];
    for (k = 0; k < JR_TIDE; k++) s->tide[k] = j->tide[k];
}
void jr_sound_put(journey_t *j, const jr_sound_t *s)
{
    uint32_t k;
    for (k = 0; k < sizeof s->shape; k++) (&j->shape[0][0])[k] = (&s->shape[0][0])[k];
    for (k = 0; k < sizeof s->dvoice; k++) (&j->dvoice[0][0])[k] = (&s->dvoice[0][0])[k];
    j->dswell = s->dswell;
    for (k = 0; k < sizeof s->bloom; k++) j->bloom[k] = s->bloom[k];
    for (k = 0; k < sizeof s->vsound; k++) (&j->vsound[0][0])[k] = (&s->vsound[0][0])[k];
    j->vcur = s->vcur;
    for (k = 0; k < JR_DMOTION; k++) j->dmotion[k] = s->dmotion[k];
    for (k = 0; k < JR_TIDE; k++) j->tide[k] = s->tide[k];
}
/* (the switches, by their place in jr_sound_t: each drone voice's OCTAVE (engine.h DV_OCT = 3), BLOOM's
 * HEIGHT (1), each VOX sound's B OCTAVE (VS_BOCT = 11), then dswell and vcur whole) */
#define SND_OFF(f) ((uint32_t)__builtin_offsetof(jr_sound_t, f))
static int snd_switch(uint32_t i)
{
    if (i >= SND_OFF(dvoice) && i < SND_OFF(dswell))
        return (i - SND_OFF(dvoice)) % JR_DVOICE == 3u;
    if (i == SND_OFF(dswell) || i == SND_OFF(vcur) || i == SND_OFF(bloom) + 1u)
        return 1;
    if (i >= SND_OFF(vsound) && i < SND_OFF(vcur))
        return (i - SND_OFF(vsound)) % JR_VS == 11u;
    return 0;
}
void jr_sound_mix(jr_sound_t *out, const jr_sound_t *a, const jr_sound_t *b, float t)
{
    const uint8_t *pa = (const uint8_t *)a, *pb = (const uint8_t *)b;
    uint8_t *po = (uint8_t *)out;
    uint32_t i;
    t = t < 0.0f ? 0.0f : t > 1.0f ? 1.0f : t;
    for (i = 0; i < sizeof *out; i++)
        po[i] = snd_switch(i) ? (t < 0.5f ? pa[i] : pb[i])
                              : (uint8_t)((float)pa[i] + ((float)pb[i] - (float)pa[i]) * t + 0.5f);
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
    jr_character(dst, i);
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

#define CODE_DEG 42                              /* degrees either way: six octaves (the code 3 a degree) */
int jr_note_code(const jrun_t *r, int midi)
{
    const scene_t *s = sc(r);
    int scale = scale_of(s), semi = midi - (12 * reg_of(s) + s->root % 12), o, pc, i = 6, d;
    while (semi < -12 * 6)                       /* (past six octaves: an octave in) */
        semi += 12;
    while (semi >= 12 * 6)
        semi -= 12;
    o = semi >= 0 ? semi / 12 : -((11 - semi) / 12);
    pc = semi - 12 * o;
    while (i > 0 && SCALE[scale][i] > pc)        /* the degree at or under it */
        i--;
    d = clampi(7 * o + i, -CODE_DEG, CODE_DEG - 1);
    return (d + CODE_DEG) * 3 + clampi(pc - SCALE[scale][i], 0, 2);
}
int jr_code_note(const jrun_t *r, int code)
{
    const scene_t *s = sc(r);
    int d = code / 3 - CODE_DEG, over = code % 3;
    return clampi(12 * reg_of(s) + s->root % 12 + deg_semi(scale_of(s), d) + over, 0, 127);
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
    r->cur = r->from = r->dest = r->mbase = r->mto = (uint8_t)(j->at % nsc(j));
    jr_tick(r, 0, 0);
}

void jr_start(jrun_t *r, const jr_sink_t *o)
{
    const scene_t *s = sc(r);
    const uint32_t held = (s->drones & 0x1Fu ? s->drones & 0x1Fu : 0x07u) & ~(uint32_t)r->j->dswell;
    int v;
    r->playing = 1;
    jr_tick(r, 0, o);
    drone_to_scene(r, 1, o);
    o->drone_latch(o->ctx, held);
    for (v = 0; v < JR_DRONES; v++)              /* the ones that come and go: the first soon */
        r->swell_wait[v] = (uint8_t)(1 + v + rndn(&r->srng, 3));
    if (!held)                                   /* nothing held: the lowest one that comes and goes, at once */
        for (v = 0; v < JR_DRONES; v++)
            if (r->j->dswell >> v & 1u) {
                o->drone_swell(o->ctx, v);
                r->swell_wait[v] = (uint8_t)(3 + rndn(&r->srng, 16));
                break;
            }
}

void jr_stop(jrun_t *r, const jr_sink_t *o)
{
    o->drone_release(o->ctx);
    r->playing = 0;
}

void jr_restart(jrun_t *r, const jr_sink_t *o)
{
    r->cur = r->from = r->dest = r->mbase = r->mto = (uint8_t)(r->j->at % nsc(r->j));
    r->travelling = 0;
    r->xf = r->mamt = 0.0f;
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
    r->mamt = 0.0f;                              /* (a MORPH under way: let go, the glide takes over) */
    r->mbase = r->mto = r->cur;
}

void jr_morph(jrun_t *r, int detents)
{
    int n = nsc(r->j), dir;
    if (!detents || n < 2 || r->travelling)
        return;
    if (r->mamt <= 0.0f) {                       /* setting out: this way */
        r->mbase = r->cur;
        r->mto = (uint8_t)((r->cur + (detents > 0 ? 1 : n - 1)) % n);
    }
    dir = r->mto == (r->mbase + 1) % n ? 1 : -1;
    if (n == 2)                                  /* (two scenes: either way is the other) */
        dir = detents > 0 ? 1 : -1;
    r->mamt += (float)(detents * dir) * (1.0f / JR_MORPH_STEPS);
    if (r->mamt <= 0.0f) {
        r->mamt = 0.0f;
        r->mto = r->mbase;
    }
    if (r->mamt >= 1.0f) {                       /* there */
        r->mamt = 0.0f;
        r->mbase = r->mto;
    }
    r->cur = r->mamt >= 0.5f ? r->mto : r->mbase;   /* (the key: past the middle, the new one's) */
    r->j->at = r->cur;
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
            r->from = r->mbase = r->mto = r->cur;
            r->xf = 0.0f;
        }
    }
    if (o) {                                     /* a new scene, or this one edited (SCENE): the drone moves */
        if (drone_sig(r) != r->sig)
            drone_to_scene(r, 0, o);
        drone_moves(r, dt, o);
    }
    if (r->travelling) {
        a = &j->scene[r->from % nsc(j)];
        b = &j->scene[r->dest % nsc(j)];
        t = r->xf * r->xf * (3.0f - 2.0f * r->xf);
    } else if (r->mamt > 0.0f) {                 /* MORPH: as far as the hand has it */
        a = &j->scene[r->mbase % nsc(j)];
        b = &j->scene[r->mto % nsc(j)];
        t = r->mamt;
    } else {
        a = b = &j->scene[r->cur % nsc(j)];
        t = 0.0f;
    }
    r->out.bright = lerp_u8(a->bright, b->bright, t);
    r->out.space = lerp_u8(a->space, b->space, t);
    r->out.tex = lerp_u8(a->tex, b->tex, t);
    r->out.colour = lerp_u8(a->colour, b->colour, t);
    r->out.room = lerp_u8(a->room, b->room, t);
    r->out.halo = lerp_u8(a->halo, b->halo, t);
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
    s->room = (uint8_t)(r->out.room + 0.5f);
    s->halo = (uint8_t)(r->out.halo + 0.5f);
}
