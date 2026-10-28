/* SPDX-License-Identifier: GPL-3.0-only */
/* X0X audio engine; see engine.h. */
#include "engine.h"
#include "plat.h"
#include "../dsp/drone.h"
#include "../dsp/drum808.h"
#include "../dsp/melody.h"
#include "../dsp/breaks.h"
#include "../dsp/fxbus.h"
#include "../dsp/master.h"
#include "../dsp/fastmath.h"

#define POOL X0X_POOL

/* time per stage of the render, in plat_cycles() units (the CPU's cycles, or the 24 MHz timer):
 * running totals (wrapping; the PERF screen takes differences) and the frames rendered. */
volatile uint32_t eng_prof[ENG_PROF_N], eng_prof_frames;
#define PROF_T(v) uint32_t v = plat_cycles()
#define PROF_ADD(k, t0) (eng_prof[k] += plat_cycles() - (t0))

/* ---------------------------------------------------------------- state --- */
seq_t seq;
volatile uint8_t eng_step[NTRACKS];
volatile uint16_t eng_peak[NPARTS];
volatile uint8_t eng_hit[NKIT][NDRUM];
int16_t eng_scope[SCOPE_N];
volatile uint32_t eng_scope_w;

/* the parts: DRONE in the 909's place (PART_909, T_909), the 808, MELODY A and B in the 303s'
 * (PART_303A/B, T_303); the break part is kept in the project but no longer sounds */
static drone_t dn POOL;
static drum808_t d808 POOL;
static melody_t mel[NBASS] POOL;
static breaks_t brk POOL;
static fxbus_t fx POOL;
static master_t mst;
#define DLY_LEN 66150                  /* 1.5 s (2 s before the reverb's lines needed the room) */
static int16_t dly_buf[DLY_LEN] POOL;
static int16_t rv_buf[SV_TOTAL] POOL, rv_ps[SV_PS_LEN] POOL;
static float swarm_l[SWARM_LEN] POOL, swarm_r[SWARM_LEN] POOL;
static volatile uint8_t brk_hold;      /* main loop is rewriting the break loops: ISR leaves it alone */
static motion_t mot;
static const sound_t *mot_base_snd;    /* the knobs' own values (the project's mirror) */
static uint8_t mot_playing;
static volatile uint8_t mot_on = 1;
static uint8_t mot_was_on = 1;
void engine_motion_enable(int on) { mot_on = (uint8_t)(on != 0); }   /* the ISR releases the knobs */
static volatile uint8_t mot_release_req;
void engine_motion_reset(void) { mot_release_req = 1; }   /* lanes changed under it (undo): knobs back */

/* overload guard: past the CPU (a block over 92%, or 85% on average) the drone voices fade down to two
 * oscillators each (a thinner cluster) and the 808's voices end at -70 dB instead of -90;
 * after 3 s back under 65% both return. The platform reports each
 * block's load after rendering it (engine_load, same context as engine_render); the change is
 * made at the start of the next block. Off during the PERF test, which measures the full cost. */
#define GUARD_ON_LAST 92u
#define GUARD_ON_AVG 85u
#define GUARD_OFF 65u
#define GUARD_CALM_FRAMES (3u * 44100u)
extern float drum808_quiet;
static uint8_t guard_on, guard_applied, guard_enabled = 1;
static uint32_t guard_avg_q8, guard_calm;
volatile uint32_t eng_guard_count;
void engine_guard_enable(int on) { guard_enabled = (uint8_t)(on != 0); }
int engine_guard_active(void) { return guard_on; }
void engine_load(uint32_t pct, uint32_t frames)
{
    guard_avg_q8 = (guard_avg_q8 * 15u + pct * 256u) / 16u;
    if (!guard_enabled) {
        guard_on = 0;
        guard_calm = 0;
    } else if (!guard_on) {
        if (pct > GUARD_ON_LAST || guard_avg_q8 > GUARD_ON_AVG * 256u) {
            guard_on = 1;
            guard_calm = 0;
            eng_guard_count++;
        }
    } else if (pct >= GUARD_OFF || guard_avg_q8 >= GUARD_OFF * 256u) {
        guard_calm = 0;
    } else if ((guard_calm += frames) >= GUARD_CALM_FRAMES) {
        guard_on = 0;
    }
}
static void guard_apply(void)
{
    if (guard_on == guard_applied)
        return;
    guard_applied = guard_on;
    drone_set_lite(&dn, guard_on);
    drum808_quiet = guard_on ? 3.2e-4f : 3.2e-5f;      /* Penumbra: -70 dB, not -50 (a tail cut that high is heard) */
}

/* mixer: per part Level, Rev send, Dly send (a drum machine's add to its voices' own sends) */
static const x0x_param_t MIX_P[MX_NPARAMS] = {{"LEVEL", 127, 100, 0}, {"REV", 127, 0, 0}, {"DLY", 127, 0, 0},
                                               {"PAN", 127, 64, 0}};
static const char *const PART_NAMES[NPARTS] = {"DRONE", "808", "MEL A", "MEL B", "BREAK"};
static float mix_level[NPARTS], mix_rev[NPARTS], mix_dly[NPARTS];
/* pan: a balance law. Centre is full level on both sides (a centred mix is the mono one exactly);
 * turning toward a side fades the other out along a quarter cosine, the near side stays at 1 */
static float mix_pl[NPARTS] = {1, 1, 1, 1, 1}, mix_pr[NPARTS] = {1, 1, 1, 1, 1};

/* ------------------------------------------------------- command queue --- */
/* uint32 commands, single producer (main loop) / single consumer (audio ISR):
 * type:4 | a:8 | b:8 | c:12 */
enum { C_PARAM_T0 = 0, C_DRUM = 8, C_BON, C_BOFF, C_BRK, C_NOP };
#define CQ 1024u
static uint32_t cq[CQ];
static volatile uint32_t cq_w, cq_r;

static void cq_put(uint32_t type, uint32_t a, uint32_t b, uint32_t c)
{
    uint32_t w = cq_w;
    if (w - cq_r >= CQ)
        return;                        /* full: drop (a param flood; the mirror still has it) */
    cq[w % CQ] = (type << 28) | ((a & 0xFFu) << 20) | ((b & 0xFFu) << 12) | (c & 0xFFFu);
    __asm__ volatile("" ::: "memory");
    cq_w = w + 1;
}

/* ------------------------------------------------------------ params --- */
int engine_nvoices(int t)
{
    switch (t) {
    case T_909: return DRONE_TRACKS + 1;
    case T_808: return D8_NUM + 1;
    case T_303: return NBASS;
    case T_MIX: return NPARTS;
    case T_FX: return 3;                                 /* 0: fxbus, 1: the UI's TAPE knob (not sent), 2: the reverb's ext */
    default: return 1;
    }
}

int engine_nparams(int t, int v)
{
    switch (t) {
    case T_909: return drone_nparams(v);
    case T_808: return drum808_nparams(v);
    case T_303: return melody_nparams();
    case T_BRK: return 3;                                /* Level, Pitch, Filter (sends: the mixer's; the rest: the pattern's) */
    case T_FX: return v == 0 ? fxbus_nparams() : v == 2 ? fxbus_ext_nparams() : 0;
    case T_MIX: return MX_NPARAMS;                       /* every part: Level, Rev send, Dly send */
    case T_MST: return master_nparams();
    default: return 0;
    }
}

const x0x_param_t *engine_param(int t, int v, int i)
{
    switch (t) {
    case T_909: return drone_param(v, i);
    case T_808: return drum808_param(v, i);
    case T_303: return melody_param(i);
    case T_BRK: return breaks_param(BRK_NSET + i);
    case T_FX: return v == 0 ? fxbus_param(i) : v == 2 ? fxbus_ext_param(i) : 0;
    case T_MIX: return (i >= 0 && i < MX_NPARAMS) ? &MIX_P[i] : 0;
    case T_MST: return master_param(i);
    default: return 0;
    }
}

static const char *const N808[D8_NUM + 1] = {"BD", "SD", "LT", "MT", "HT", "RS", "CP", "CB", "CY", "OH", "CH", "KIT"};

const char *engine_voice_name(int t, int v)
{
    switch (t) {
    case T_909: return drone_track_name(v);
    case T_808: return (v >= 0 && v <= D8_NUM) ? N808[v] : "";
    case T_303: return v ? "MEL B" : "MEL A";
    case T_MIX: return (v >= 0 && v < NPARTS) ? PART_NAMES[v] : "";
    case T_BRK: return "BREAK";
    case T_FX: return "FX";
    case T_MST: return "MASTER";
    default: return "";
    }
}

static void apply_param(int t, int v, int i, int val)    /* ISR */
{
    switch (t) {
    case T_909: drone_set(&dn, v, i, val); break;
    case T_808: drum808_set(&d808, v, i, val); break;
    case T_303: if (v >= 0 && v < NBASS) melody_set(&mel[v], i, val); break;
    case T_BRK: breaks_set(&brk, BRK_NSET + i, val); break;
    case T_FX:
        if (v == 0)
            fxbus_set(&fx, i, val);
        else if (v == 2)
            fxbus_ext_set(&fx, i, val);
        break;
    case T_MIX:
        if (v < 0 || v >= NPARTS)
            break;
        if (i == MX_LEVEL)
            mix_level[v] = (float)val * (float)val / (100.0f * 100.0f);     /* 100 = unity, square law */
        else if (i == MX_REV)
            mix_rev[v] = (float)val / 127.0f;
        else if (i == MX_DLY)
            mix_dly[v] = (float)val / 127.0f;
        else if (i == MX_PAN)
            x0x_pan_gains(val, &mix_pl[v], &mix_pr[v]);
        break;
    case T_MST: master_set(&mst, i, val); break;
    default: break;
    }
}

void engine_set(int t, int v, int i, int val) { cq_put((uint32_t)t, (uint32_t)v, (uint32_t)i, (uint32_t)val); }

/* the output's scale: Felucca's -6 dBFS ceiling of the 24-bit codec (2^22), or its full scale (LOUD); the
 * clamp keeps an over from wrapping (normal: the codec's own full scale, as before) */
static volatile float out_scale = 4194303.0f, out_lim = 2.0f;
void engine_out_loud(int on)
{
    out_scale = on ? 8388607.0f : 4194303.0f;
    out_lim = on ? 1.0f : 2.0f;
}

/* USB AUDIO FULL (#12): the render at full level, for the USB tap, and the MASTER knob after it, for
 * the speaker, headphones and line out (with the master's ~10 ms glide). The limiter then works at
 * full level too: turned down, the FM-1 plays what it sends, quieter. */
static volatile int usb_full;
static float vol_after = -1.0f;
void engine_usb_full(int on) { usb_full = on; }
/* where the drone renders, read once per stretch (render_sub): the first core by default. Some
 * FM-1s' second core misreads memory now and then (fm1_cpu1.h); the more it does, the likelier
 * a misread lands in the sound, so it keeps the 808 alone unless asked */
static volatile uint8_t drone_on2;
void engine_drone_core2(int on) { drone_on2 = (uint8_t)(on != 0); }
static volatile uint32_t eng_quiet_frames;   /* engine_render: frames since the output last made a sound */
uint32_t eng_out_clips;                       /* output samples past full scale (hard-limited), since power-on */
uint32_t engine_quiet_ms(void) { return eng_quiet_frames / 44u; }

void engine_volume(int32_t *out_lr, uint32_t n)
{
    float vol = (float)plat_master() / 4096.0f, v = vol_after < 0.0f ? vol : vol_after;
    uint32_t i;
    if (!usb_full) {
        vol_after = -1.0f;
        return;
    }
    for (i = 0; i < n; i++) {
        if (v != vol) {
            v += (vol - v) * 0.0022651f;
            if (fm_fabsf(vol - v) < 1e-5f)
                v = vol;
        }
        out_lr[2u * i] = (int32_t)((float)out_lr[2u * i] * v);
        out_lr[2u * i + 1u] = (int32_t)((float)out_lr[2u * i + 1u] * v);
    }
    vol_after = v;
}

void engine_sound_defaults(sound_t *s)
{
    int t, v, i;
    uint8_t *z = (uint8_t *)s;
    for (i = 0; i < (int)sizeof *s; i++)
        z[i] = 0;
    for (t = 0; t < NTARGETS; t++)
        for (v = 0; v < engine_nvoices(t) && v < NVOICES_MAX; v++)
            for (i = 0; i < engine_nparams(t, v) && i < NPARAMS_MAX; i++) {
                const x0x_param_t *p = engine_param(t, v, i);
                s->v[t][v][i] = p ? p->def : 0;
            }
    /* the factory sound: the prototype's "Cinematic" (drone-proto/drone.html). An A minor drone
     * (root, fifth, octave on; minor third, fifth and a high octave ready on DR4-DR6), each voice
     * a little off true like a Solar tuned by ear, spread across the field; the melodies and the
     * drones into a long shimmering reverb and a worn ping-pong tape delay, dotted quarters. */
    {
        static const uint8_t NOTE[DRONE_VOICES] = {24, 31, 36, 39, 43, 48};   /* 0 +7 +12 +15 +19 +24 */
        static const uint8_t FINE[DRONE_VOICES] = {64, 61, 66, 60, 64, 67};   /* 0 -4 +3 -7 0 +5 cents */
        static const uint8_t PAN[DRONE_VOICES] = {64, 38, 89, 26, 102, 64};
        for (v = 0; v < DRONE_VOICES; v++) {
            s->v[T_909][v][DN_NOTE] = NOTE[v];
            s->v[T_909][v][DN_FINE] = FINE[v];
            s->v[T_909][v][DN_PAN] = PAN[v];
        }
        s->v[T_909][5][DN_SPREAD] = 30;              /* the high octave: a wider, shimmering cluster */
    }
    s->v[T_MIX][PART_909][MX_REV] = 64;
    s->v[T_MIX][PART_909][MX_DLY] = 41;
    s->v[T_MIX][PART_808][MX_LEVEL] = 90;
    s->v[T_MIX][PART_808][MX_REV] = 30;
    s->v[T_808][D8_KIT][0] = 127;                    /* kit level */
    for (v = 0; v < NBASS; v++) {
        s->v[T_MIX][PART_303A + v][MX_REV] = 64;
        s->v[T_MIX][PART_303A + v][MX_DLY] = 41;
    }
    s->v[T_303][1][MEL_TUNE] = 0;                    /* MEL B an octave down: a bass line under A */
    s->v[T_MIX][PART_BRK][MX_LEVEL] = 0;             /* (silent anyway) */
    s->v[T_FX][0][FX_VOLUME] = 127;                  /* unity: 9W9's 0.35 (-9 dB) balanced its hot kit, not drones */
    s->v[T_FX][0][FX_DL_TYPE] = 1;                   /* tape */
    s->v[T_FX][0][FX_DL_TIME] = 10;                  /* 1/4. */
    s->v[T_FX][0][FX_DL_FDBK] = 82;
    s->v[T_FX][0][FX_DL_TONE] = 57;
    s->v[T_FX][0][FX_DL_WEAR] = 44;
    s->v[T_FX][0][FX_DL_PING] = 1;
    s->v[T_FX][1][0] = (uint8_t)(16 + s->v[T_FX][0][FX_DL_WEAR] * 111 / 127);   /* TAPE (ui.c R_TAPE): on, its wear */
}

void engine_sound_centre_drum_pans(sound_t *s)
{
    int t, v;
    for (t = T_909; t <= T_808; t++)
        for (v = 0; v < NDRUM && v < NVOICES_MAX; v++) {
            int i = engine_nparams(t, v) - 1;           /* PAN is every voice's last pot */
            const x0x_param_t *p = engine_param(t, v, i);
            if (p && p->def == 64 && p->max == 127)
                s->v[t][v][i] = 64;
        }
}

void engine_apply_sound(const sound_t *s)
{
    int t, v, i;
    engine_set(T_MST, 0, MST_COMP1, s->v[T_MST][0][MST_COMP1]);   /* the macros first: the pots they set */
    engine_set(T_MST, 0, MST_DJF, s->v[T_MST][0][MST_DJF]);       /* follow as saved (a project's own sound) */
    for (t = 0; t < NTARGETS; t++)
        for (v = 0; v < engine_nvoices(t) && v < NVOICES_MAX; v++)
            for (i = 0; i < engine_nparams(t, v) && i < NPARAMS_MAX; i++)
                if (t != T_MST || i < MST_COMP1)
                    engine_set(t, v, i, s->v[t][v][i]);
}

void engine_drum(int kit, int voice, float vel)
{
    cq_put(C_DRUM, (uint32_t)kit, (uint32_t)voice, (uint32_t)(vel * 4095.0f));
}
void engine_bass_on(int part, int note, int accent, int slide)
{
    cq_put(C_BON, (uint32_t)part, (uint32_t)note, (uint32_t)((accent ? 1 : 0) | (slide ? 2 : 0)));
}
void engine_bass_off(int part) { cq_put(C_BOFF, (uint32_t)part, 0, 0); }
void engine_brk_live(int key, int down) { cq_put(C_BRK, (uint32_t)key, (uint32_t)(down != 0), 0); }

/* ------------------------------------------------------- sequencer sink --- */
/* probability: each part's (and drum track's) chance on the step playing now, set by its T_PROB lane
 * before the step fires (motion_step runs first); 100 = always */
static uint8_t prob[NPARTS][NDRUM];
static uint32_t prob_rng = 0x2545F491u;
static int prob_skip(int part, int v)                 /* ISR: 1 = this hit does not play */
{
    uint32_t p = prob[part][v];
    if (p > 100u && p < PROB_COND + PROB_NCOND) {      /* A:B: the A-th of every B passes (seq_track_t.bars) */
        int a, b;
        prob_cond((int)p - PROB_COND, &a, &b);
        return (int)(seq.t[part].bars % (uint32_t)b) != a - 1;
    }
    if (p >= 100u)
        return 0;
    prob_rng ^= prob_rng << 13;
    prob_rng ^= prob_rng >> 17;
    prob_rng ^= prob_rng << 5;
    return prob_rng % 100u >= p;
}
static void s_drum(void *x, int kit, int v, float vel);
static int s_drum_seq(void *x, int kit, int v, float vel)
{
    if (prob_skip(kit, v))
        return 0;
    s_drum(x, kit, v, vel);
    return 1;
}
static void s_drum(void *x, int kit, int v, float vel)
{
    (void)x;
    if (v == 0 && kit == 1)              /* the 808's BD: the master's PUMP key (a drone is no kick) */
        master_key(&mst, kit, vel);
    if ((unsigned)kit < NKIT && (unsigned)v < NDRUM)   /* MIX's drum pages: the track's meter jumps */
        eng_hit[kit][v] = (uint8_t)(vel >= 1.0f ? 255 : (int)(vel * 255.0f));
    if (kit == 0)
        drone_trigger(&dn, v, vel);      /* by the voice's MODE: latch on / off, or a swell */
    else                                 /* the 808's unaccented hit sits at D8_VEL_NORMAL, not at our 909 level */
        drum808_trigger(&d808, v, vel >= 0.999f ? 1.0f : vel * (D8_VEL_NORMAL / (88.0f / 127.0f)));
}
static void s_bon(void *x, int p, int note, int acc, int sl)
{
    (void)x;
    if (prob_skip(PART_303A + p, 0))                  /* skipped: a rest */
        melody_note_off(&mel[p]);
    else
        melody_note_on(&mel[p], note, acc, sl);
}
static void s_boff(void *x, int p) { (void)x; melody_note_off(&mel[p]); }
static uint8_t brk_applied[BRK_NSET];  /* the pattern settings the break was last given */
static void s_brk(void *x, int s16, int bar, float spb, int en)
{
    const brkpart_t *bp = &seq.pat[seq.ppat[TRK_BRK]].brk;
    int i;
    if (en && prob_skip(PART_BRK, 0))
        en = 0;
    (void)x;
    if (brk_hold)
        return;
    breaks_set_quiet(&brk, bp->steps == 0 || (seq.mute & (1u << MUTE_BRK)));   /* unheard: stop decoding */
    for (i = 0; i < BRK_NSET; i++)       /* follow the playing pattern's settings (12 compares) */
        if (bp->set[i] != brk_applied[i]) {
            brk_applied[i] = bp->set[i];
            breaks_set(&brk, i, bp->set[i]);
        }
    breaks_pin(&brk, (int)bp->slice[s16 & 15] - 1);   /* the step's own slice, if it has one */
    breaks_step(&brk, s16, bar, spb, en);
}
static void s_brkstop(void *x)            /* the sequencer stopped */
{
    (void)x;
    breaks_stop(&brk);
    drone_end_swells(&dn);
}
static void s_midi(void *x, uint8_t st, uint8_t d1, uint8_t d2)
{
    uint32_t cin = st >= 0xF0 ? 0x0Fu : (uint32_t)(st >> 4);
    (void)x;
    plat_midi_out(cin | ((uint32_t)st << 8) | ((uint32_t)d1 << 16) | ((uint32_t)d2 << 24));
}
static void s_step(void *x, int t, int p)
{
    (void)x;
    eng_step[t] = (uint8_t)p;
    if (mot_on)
        motion_step(&mot, t, seq.ppat[t], p, seq_next_step(&seq, t, p), seq_part_len(&seq, t));
}

/* --------------------------------------------------------------- motion --- */
static int mot_base(void *x, int t, int v, int i)
{
    (void)x;
    if (t >= NTARGETS)
        return 100;                                   /* T_PROB: always */
    return mot_base_snd ? mot_base_snd->v[t][v][i] : 0;
}
static void mot_apply(void *x, int t, int v, int i, int val)
{
    (void)x;
    if (t == T_PROB) {
        if ((v >> 4) < NPARTS && (v & 15) < NDRUM)
            prob[v >> 4][v & 15] = (uint8_t)val;
    } else if (t < NTARGETS)
        apply_param(t, v, i, val);
}

int engine_motion_part(int t, int v)
{
    switch (t) {
    case T_909: return PART_909;
    case T_808: return PART_808;
    case T_303: return PART_303A + (v & 1);
    case T_BRK: return PART_BRK;
    case T_MIX: return v < NPARTS ? v : PART_909;
    case T_PROB: return (v >> 4) < NPARTS ? v >> 4 : PART_909;
    default: return PART_909;                     /* FX, master: the bar */
    }
}
int engine_motion_value(int k) { return motion_value(&mot, k); }
void engine_motion_rec(int k) { motion_req_rec(&mot, k); }
void engine_motion_hold(int k) { motion_req_hold(&mot, k); }
static const seq_sink_t SINK = {s_drum_seq, s_bon, s_boff, s_brk, s_brkstop, s_midi, s_step, 0};

/* ---------------------------------------------------------------- MIDI in --- */
static uint8_t bass_held[NBASS];       /* note held from MIDI, for legato = slide */

static void midi_in(uint32_t pkt)
{
    uint8_t st = (uint8_t)(pkt >> 8), d1 = (uint8_t)(pkt >> 16) & 0x7F, d2 = (uint8_t)(pkt >> 24) & 0x7F;
    uint8_t ch = st & 0x0F, ty = st & 0xF0;
    int k, v;
    switch (st) {
    case 0xF8: seq_ext_clock(&seq); return;
    case 0xFA: seq_start(&seq); return;
    case 0xFB: seq_continue(&seq); return;
    case 0xFC: seq_stop(&seq); return;
    default: break;
    }
    if (ty == 0x90 && d2 == 0)
        ty = 0x80;
    for (k = 0; k < NKIT; k++)                          /* each part on its own channel (GLO): the drum */
        if (seq.ch[k] == ch && ty == 0x90) {             /* machines on GM drum notes */
            static const uint8_t GM[NDRUM] = {36, 38, 41, 45, 50, 37, 39, 42, 46, 49, 51};
            static const uint8_t GM8[NDRUM] = {36, 38, 41, 45, 50, 37, 39, 56, 49, 46, 42};
            for (v = 0; v < NDRUM; v++)
                if ((k ? GM8 : GM)[v] == d1)
                    s_drum(0, k, v, (float)d2 / 127.0f);
        }
    for (k = 0; k < NBASS; k++)                         /* the melodies: overlapping notes slide */
        if (seq.ch[TRK_BASS0 + k] == ch) {
            if (ty == 0x90) {
                melody_note_on(&mel[k], d1, d2 >= 100, bass_held[k] != 0);
                bass_held[k] = d1;
            } else if (ty == 0x80 && bass_held[k] == d1) {
                melody_note_off(&mel[k]);
                bass_held[k] = 0;
            }
        }
    if (seq.ch[TRK_BRK] == ch && (ty == 0x90 || ty == 0x80) && d1 >= 36 && d1 <= 43)   /* the break's slices */
        breaks_live(&brk, d1 - 36, ty == 0x90);
}

/* ---------------------------------------------------------------- render --- */
static void drain(void)
{
    uint32_t r = cq_r, pkt;
    int guard = 64;
    while (r != cq_w) {
        uint32_t c = cq[r % CQ], type = c >> 28, a = (c >> 20) & 0xFFu, b = (c >> 12) & 0xFFu, v = c & 0xFFFu;
        r++;
        if (type < 8u)
            apply_param((int)type, (int)a, (int)b, (int)v);
        else if (type == C_DRUM)
            s_drum(0, (int)a, (int)b, (float)v / 4095.0f);
        else if (type == C_BON && a < NBASS)
            melody_note_on(&mel[a], (int)b, (int)(v & 1u), (int)((v >> 1) & 1u));
        else if (type == C_BOFF && a < NBASS)
            melody_note_off(&mel[a]);
        else if (type == C_BRK && !brk_hold)
            breaks_live(&brk, (int)a, (int)b);
    }
    cq_r = r;
    while (guard-- && plat_midi_in(&pkt))
        midi_in(pkt);
}

static float part_peak[NPARTS];
__attribute__((noinline)) static void meter(int p, const float *x, float g, uint32_t n)
{
    uint32_t i;
    float pk = part_peak[p];
    for (i = 0; i < n; i++) {
        float a = fm_fabsf(x[i]) * g;
        if (a > pk)
            pk = a;
    }
    part_peak[p] = pk;
}

/* dry into L and R by the part's pan; the sends stay mono (the reverb and delay make the width) */
/* a mix gain that glides to its target (one pole, ~10 ms) instead of stepping each block, which
 * zippered on quick turns of LEVEL, PAN or a send. cur < 0: never used, takes the target at once
 * (no fade-in at boot); settled, cur IS the target, so the product is the unsmoothed one exactly */
typedef struct { float cur, tgt; } glide_t;
#define GLIDE_A 0.0022651f                                /* 1 - exp(-1 / (10 ms * 44.1 kHz)) */
static void glide_set(glide_t *g, float t)
{
    g->tgt = t;
    if (g->cur < 0.0f)
        g->cur = t;
}
__attribute__((noinline)) static void add_glide(float *dst, const float *src, glide_t *g, uint32_t n)
{
    uint32_t i;
    float c = g->cur, t = g->tgt;
    if (c == t) {
        if (c != 0.0f)
            for (i = 0; i < n; i++)
                dst[i] += src[i] * c;
        return;
    }
    for (i = 0; i < n; i++) {
        c += (t - c) * GLIDE_A;
        dst[i] += src[i] * c;
    }
    if (fm_fabsf(t - c) < 1e-5f)
        c = t;
    g->cur = c;
}
/* each part's gains: dry left and right, its reverb and delay sends, and (drums) the voices' own sends */
enum { GL_L, GL_R, GL_REV, GL_DLY, GL_VREV, GL_VDLY, GL_N };
static glide_t mix_glide[NPARTS][GL_N] = {
    {{-1, 0}, {-1, 0}, {-1, 0}, {-1, 0}, {-1, 0}, {-1, 0}}, {{-1, 0}, {-1, 0}, {-1, 0}, {-1, 0}, {-1, 0}, {-1, 0}},
    {{-1, 0}, {-1, 0}, {-1, 0}, {-1, 0}, {-1, 0}, {-1, 0}}, {{-1, 0}, {-1, 0}, {-1, 0}, {-1, 0}, {-1, 0}, {-1, 0}},
    {{-1, 0}, {-1, 0}, {-1, 0}, {-1, 0}, {-1, 0}, {-1, 0}}};
static void mix_targets(int p, float g)                     /* g: the part level */
{
    glide_set(&mix_glide[p][GL_L], g * mix_pl[p]);
    glide_set(&mix_glide[p][GL_R], g * mix_pr[p]);
    glide_set(&mix_glide[p][GL_REV], g * mix_rev[p]);
    glide_set(&mix_glide[p][GL_DLY], g * mix_dly[p]);
    glide_set(&mix_glide[p][GL_VREV], g);
    glide_set(&mix_glide[p][GL_VDLY], g);
}

#if X0X_DEBUG
/* development: what one block cost, for the worst-block record (main_fm1.c render_block) */
volatile uint32_t eng_dbg_wait, eng_dbg_nsub;  /* 24 MHz ticks waiting for core 1, stretches */
#endif

/* the mix of one stretch between two events: dry left and right, the reverb and delay sends;
 * t0..tr the 808's scratch, mono the drone's send */
static float dry_l[256], dry_r[256], rev[256], dly[256], t0[256], t1[256], t2[256], tr[256], mono[256];

/* a glide applied in place to a buffer that started at zero: buf * c is 0 + buf * c exactly, so this
 * is add_glide(zeros, buf) to the bit (the same steps of c) */
__attribute__((noinline)) static void scale_glide(float *buf, glide_t *g, uint32_t n)
{
    uint32_t i;
    float c = g->cur, t = g->tgt;
    if (c == t) {
        for (i = 0; i < n; i++)
            buf[i] *= c;
        return;
    }
    for (i = 0; i < n; i++) {
        c += (t - c) * GLIDE_A;
        buf[i] *= c;
    }
    if (fm_fabsf(t - c) < 1e-5f)
        c = t;
    g->cur = c;
}

/* the second core's job (or the first's, after its own part, with one core): the 808 into t0..tr,
 * and with arg bit 9 the drone into dn_l, dn_r (GLO > DRONE CORE 2ND); arg bit 8: not timed (the
 * cycle counter is the first core's). Nothing here is mixed: the first core does that afterwards, in
 * one core's order. (The break no longer renders.) drone_trigger and drone_set run between
 * stretches, never during this. */
static uint8_t d808_was_active, dn_was_active;
static float dn_l[256] POOL, dn_r[256] POOL;
static void drone_job(uint32_t n)
{
    uint32_t i;
    dn_was_active = (uint8_t)drone_active(&dn);
    if (dn_was_active) {
        for (i = 0; i < n; i++)
            dn_l[i] = dn_r[i] = 0.0f;
        drone_render(&dn, dn_l, dn_r, (int)n);
    }
}
static void core2_job(uint32_t arg)
{
    uint32_t n = arg & 0xFFu ? arg & 0xFFu : 256u, i, timed = !(arg & 0x100u), pt = timed ? plat_cycles() : 0;
    if (arg & 0x200u) {
        drone_job(n);
        if (timed) {
            PROF_ADD(PR_909, pt);
            pt = plat_cycles();
        }
    }
    d808_was_active = (uint8_t)drum808_active(&d808);
    if (d808_was_active) {                                /* silent, it does nothing (and is not mixed) */
        for (i = 0; i < n; i++)
            t0[i] = t1[i] = t2[i] = tr[i] = 0.0f;
        drum808_render_st(&d808, t0, tr, t1, t2, (int)n);
    } else
        drum808_pan_settle(&d808);
    if (timed)
        PROF_ADD(PR_808, pt);
}

/* a drum machine's mix, from its render (L, R, voice sends; mono, its kit send) */
__attribute__((noinline)) static void kit_sends(int b, const float *m, uint32_t n)
{
    add_glide(rev, m, &mix_glide[b][GL_REV], n);
    add_glide(dly, m, &mix_glide[b][GL_DLY], n);
    meter(b, m, mix_level[b], n);
}

/* Penumbra: the reverb and the delay run a block behind. Each stretch keeps its sends (fx_cur, at
 * its offset in the block) and adds the returns of the same stretch of the block before (fx_prev),
 * which the first core works out while the second renders the 808: time it spent waiting. The
 * returns come 5.8 ms (FX_LEAD) later; the delay reads that much sooner (fxbus_set_lead). */
#define FX_LEAD 256
static float fx_send[2][2][256] POOL;           /* [block parity][rev, dly][sample] */
static float wet_l[256] POOL, wet_r[256] POOL;
static uint8_t fx_par;                          /* fx_send[fx_par]: this block's */
static uint32_t sub_off;                        /* this stretch's offset in the block */

/* one stretch: the melodies (and the drone, unless it is the second core's) here while the second
 * core renders the 808; then those are mixed in, in the order one core always used (drone, 808,
 * MEL A, MEL B): the same sums either way */
static void render_sub(float *out_l, float *out_r, uint32_t n)
{
    float *bb[NBASS] = {out_l, out_r};
    int idle[NBASS], b;
    uint32_t arg = (n & 0xFFu) | (drone_on2 ? 0x200u : 0u), i, pt;
    int on2 = plat_cpu2_run(core2_job, arg | 0x100u);
    for (i = 0; i < n; i++)
        dry_l[i] = dry_r[i] = rev[i] = dly[i] = 0.0f;
    if (!(arg & 0x200u)) {
        PROF_T(p9);
        drone_job(n);
        PROF_ADD(PR_909, p9);
    }
    for (b = 0; b < NBASS; b++) {
        PROF_T(pb);
        idle[b] = mel[b].idle;                            /* idle: the render writes zeros */
        melody_render(&mel[b], bb[b], (int)n);
        PROF_ADD(2 + b, pb);
    }
    {   /* the last block's returns for this stretch, while the 808 renders */
        PROF_T(pf);
        fxbus_wet_st(&fx, fx_send[fx_par ^ 1u][0] + sub_off, fx_send[fx_par ^ 1u][1] + sub_off, wet_l, wet_r, (int)n);
        PROF_ADD(5, pf);
    }
    pt = plat_cycles();
#if X0X_DEBUG
    eng_dbg_nsub++;
    {
        uint32_t w0 = plat_ticks24();
        int failed = on2 && plat_cpu2_wait();
        eng_dbg_wait += plat_ticks24() - w0;
        if (failed)
            on2 = 0;
    }
    if (!on2)
#else
    if (!on2 || plat_cpu2_wait())
#endif
        core2_job(arg);                                   /* one core (or the second failed): here */
    else
        PROF_ADD(PR_808, pt);                             /* what the first core waited for the second */
    if (dn_was_active) {   /* the drone, into the mix (L, R), then its sends from both sides and its level and pan */
        for (i = 0; i < n; i++) {
            dry_l[i] = dn_l[i];
            dry_r[i] = dn_r[i];
            mono[i] = (dn_l[i] + dn_r[i]) * 0.5f;
        }
        mix_targets(PART_909, mix_level[PART_909]);
        scale_glide(dry_l, &mix_glide[PART_909][GL_L], n);
        scale_glide(dry_r, &mix_glide[PART_909][GL_R], n);
        kit_sends(PART_909, mono, n);
    }
    if (d808_was_active) {
        mix_targets(PART_808, mix_level[PART_808]);
        add_glide(dry_l, t0, &mix_glide[PART_808][GL_L], n);
        add_glide(dry_r, tr, &mix_glide[PART_808][GL_R], n);
        add_glide(rev, t1, &mix_glide[PART_808][GL_VREV], n);
        add_glide(dly, t2, &mix_glide[PART_808][GL_VDLY], n);
        for (i = 0; i < n; i++)
            t0[i] = (t0[i] + tr[i]) * 0.5f;
        kit_sends(PART_808, t0, n);
    }
    for (b = 0; b < NBASS; b++) {
        int p = PART_303A + b;
        if (idle[b])
            continue;
        mix_targets(p, mix_level[p]);
        add_glide(dry_l, bb[b], &mix_glide[p][GL_L], n);
        add_glide(dry_r, bb[b], &mix_glide[p][GL_R], n);
        add_glide(rev, bb[b], &mix_glide[p][GL_REV], n);
        add_glide(dly, bb[b], &mix_glide[p][GL_DLY], n);
        meter(p, bb[b], mix_level[p], n);
    }
    {
        PROF_T(pf);
        float *sr = fx_send[fx_par][0] + sub_off, *sd = fx_send[fx_par][1] + sub_off;
        for (i = 0; i < n; i++) {
            sr[i] = rev[i];                               /* for the next block */
            sd[i] = dly[i];
            out_l[i] = dry_l[i] + wet_l[i];
            out_r[i] = dry_r[i] + wet_r[i];
        }
        fxbus_post_st(&fx, out_l, out_r, (int)n);
        PROF_ADD(5, pf);
    }
}

void engine_render(int32_t *out_lr, uint32_t n)
{
    static float out_l[256], out_r[256];
    uint32_t done = 0, i;
    float vol = usb_full ? 1.0f : (float)plat_master() / 4096.0f;   /* (FULL: engine_volume) */
    int p;
    if (n > 256u)
        n = 256u;
    eng_prof_frames += n;
    if ((mot_was_on && !mot_on) || mot_release_req) {   /* motion off, or its lanes replaced: knobs back */
        mot_release_req = 0;
        motion_release(&mot);
    }
    mot_was_on = mot_on;
    guard_apply();
    {
        PROF_T(pt);
        drain();
        if (mot_on)
            motion_tick(&mot, n);
        fxbus_set_bpm(&fx, seq_tempo(&seq));
        seq_advance(&seq, 0, &SINK);
        PROF_ADD(7, pt);
    }
    while (done < n) {
        uint32_t k = seq_until_event(&seq, n - done);
        sub_off = done;
        render_sub(out_l + done, out_r + done, k);
        {
            PROF_T(pt);
            seq_advance(&seq, k, &SINK);
            PROF_ADD(7, pt);
        }
        done += k;
    }
    for (i = n; i < 256u; i++)                            /* (a shorter block: the rest sends nothing) */
        fx_send[fx_par][0][i] = fx_send[fx_par][1][i] = 0.0f;
    fx_par ^= 1u;
    if (mot_playing && !seq.playing)                     /* stopped: every knob back to its own value */
        motion_release(&mot);
    mot_playing = seq.playing;
    {
        PROF_T(pt);
        master_process_st(&mst, out_l, out_r, (int)n, vol);
        PROF_ADD(6, pt);
    }
    {
        float pk = 0.0f;
        for (i = 0; i < n; i++) {
            float l = out_l[i], r = out_r[i];            /* (out_scale: NORMAL 2^22, Felucca's; LOUD 2^23) */
            eng_out_clips += (fm_fabsf(l) > out_lim) + (fm_fabsf(r) > out_lim);
            l = l > out_lim ? out_lim : l < -out_lim ? -out_lim : l;
            r = r > out_lim ? out_lim : r < -out_lim ? -out_lim : r;
            out_lr[2u * i] = (int32_t)(l * out_scale);
            out_lr[2u * i + 1u] = (int32_t)(r * out_scale);
            if (i & 1u)
                eng_scope[eng_scope_w++ & (SCOPE_N - 1u)] = (int16_t)((out_l[i] + out_r[i]) * 16383.5f);
            pk = fm_maxf(pk, fm_maxf(fm_fabsf(l), fm_fabsf(r)));
        }
        /* Penumbra: how long the output has been silent (under -72 dBFS), for AUTOSAVE: a flash
         * erase stops the audio, and drones and their reverb ring on with the sequencer stopped */
        eng_quiet_frames = pk < 2.5e-4f ? (eng_quiet_frames + n > 0x7FFFFFFFu ? 0x7FFFFFFFu : eng_quiet_frames + n) : 0;
    }
    for (p = 0; p < NPARTS; p++) {                      /* meters: peak hold, ~300 ms decay at the UI */
        uint32_t q = (uint32_t)(fm_minf(part_peak[p], 1.0f) * 32767.0f);
        if (q > eng_peak[p])
            eng_peak[p] = (uint16_t)q;
        part_peak[p] = 0.0f;
    }
}

/* -------------------------------------------------------------- loops --- */
/* a user loop's length in bars: 1, 2, 4 or 8, whichever puts its tempo nearest 120 BPM
 * inside 80..180 (breaks are recorded at their own tempo; the player stretches to ours) */
static uint8_t loop_bars(uint32_t ns, uint32_t rate)
{
    float sec = (float)ns / (float)(rate ? rate : 22050u), best = 1e9f;
    uint8_t bars = 1, b;
    for (b = 1; b <= 8; b = (uint8_t)(b * 2)) {
        float bpm = (float)b * 240.0f / sec, d = bpm > 120.0f ? bpm - 120.0f : 120.0f - bpm;
        if (bpm >= 80.0f && bpm <= 180.0f && d < best) {
            best = d;
            bars = b;
        }
    }
    return bars;
}

#include "x0x_break_bank.h"              /* tools/gen_break_bank.py: the built-in loops */

static const brk_loop_t *brk_builtin(int k)
{
    static brk_loop_t L[2];
    static int which;
    brk_loop_t *l = &L[which ^= 1];      /* two: A and B are set one after the other */
    if (k < 0 || k >= X0X_NBREAKS)
        return 0;
    l->adpcm = X0X_BREAKS[k].adpcm;
    l->nsamples = X0X_BREAKS[k].n;
    l->rate = X0X_BREAK_RATE;
    l->bars = X0X_BREAKS[k].bars;
    return l;
}

/* the LOOP choices: the built-in bank, then every loop (zone) of the user slots, in order.
 * The list follows uploads: engine_brk_loops() rescans it. */
#define NLOOPS_MAX (X0X_NBREAKS + PLAT_NSLOTS * PLAT_SLOT_ZONES)
static const char *loop_names[NLOOPS_MAX];
static char user_names[PLAT_NSLOTS * PLAT_SLOT_ZONES][8];
static uint8_t loop_slot[NLOOPS_MAX], loop_zone[NLOOPS_MAX];
static int nloops;

static void scan_loops(void)
{
    int i, k, z, u = 0;
    nloops = 0;
    for (i = 0; i < X0X_NBREAKS; i++)
        loop_names[nloops++] = X0X_BREAKS[i].name;
    for (k = 0; k < PLAT_NSLOTS; k++) {                 /* yours: BR1, BR2 ... in order, whatever slot */
        int nz = plat_slot_zones(k);                    /* holds them (the break page packs the list) */
        for (z = 0; z < nz && z < PLAT_SLOT_ZONES; z++) {
            char *n = user_names[k * PLAT_SLOT_ZONES + z];
            int j = 2;
            u++;
            n[0] = 'B';
            n[1] = 'R';
            if (u >= 10)
                n[j++] = (char)('0' + u / 10);
            n[j++] = (char)('0' + u % 10);
            n[j] = 0;
            loop_slot[nloops] = (uint8_t)k;
            loop_zone[nloops] = (uint8_t)z;
            loop_names[nloops++] = n;
        }
    }
}

int engine_brk_nslots(void)
{
    if (!nloops)
        scan_loops();
    return nloops;
}
const char *const *engine_brk_slot_names(void)
{
    if (!nloops)
        scan_loops();
    return loop_names;
}

int engine_brk_slice(void) { return brk.running ? brk.st_slice : -1; }

void engine_brk_state(int *slice, int *bank, int *div, int *running)
{
    *slice = brk.st_slice;
    *bank = brk.st_bank;
    *div = brk.st_div;
    *running = brk.running;
}

/* the loop's outline for the screen: n columns of peak level 0..255 (main loop: decodes the
 * whole loop once, ~20k IMA steps; called when the loop changes, never per frame) */
static const int16_t ISTEP[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97,
    107, 118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
    876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428,
    4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350,
    22385, 24623, 27086, 29794, 32767};
static const int8_t IIDX[8] = {-1, -1, -1, -1, 2, 4, 6, 8};
int engine_brk_outline(int which, uint8_t *peaks, int n)
{
    const brk_bank_t *bk = &brk.bank[which & 1];
    uint32_t i, ns = bk->n;
    int32_t pred = 0, idx = 0, col = 0, pk = 0;
    uint32_t per;
    if (!bk->adpcm || !ns || n <= 0)
        return 0;
    per = (ns + (uint32_t)n - 1u) / (uint32_t)n;       /* samples per column (32-bit: no __udivdi3) */
    for (i = 0; i < ns; i++) {
        uint32_t code = (bk->adpcm[i >> 1] >> ((i & 1u) * 4u)) & 15u;
        int32_t st = ISTEP[idx], vd = st >> 3, c;
        if (code & 4u) vd += st;
        if (code & 2u) vd += st >> 1;
        if (code & 1u) vd += st >> 2;
        pred += (code & 8u) ? -vd : vd;
        pred = pred < -32768 ? -32768 : pred > 32767 ? 32767 : pred;
        idx += IIDX[code & 7u];
        idx = idx < 0 ? 0 : idx > 88 ? 88 : idx;
        c = (int32_t)(i / per);
        if (c != col) {
            peaks[col] = (uint8_t)(pk >> 7);
            col = c;
            pk = 0;
        }
        if ((pred < 0 ? -pred : pred) > pk)
            pk = pred < 0 ? -pred : pred;
    }
    peaks[col] = (uint8_t)(pk >> 7);
    for (col++; col < n; col++)
        peaks[col] = 0;
    return 1;
}
float engine_gr_db(void) { return mst.gr_view; }
void engine_master_format(int i, char *buf) { master_format(&mst, i, buf); }

void engine_brk_loops(void)
{
    const brkpart_t *bp = &seq.pat[seq.ppat[TRK_BRK]].brk;
    brk_loop_t L[2];
    int w;
    scan_loops();                                       /* an upload may have changed the user loops */
    brk_hold = 1;                                       /* the ISR skips the break while we rewrite it */
    for (w = 0; w < 2; w++) {
        int slot = w ? bp->slot_b : bp->slot_a;
        uint32_t ns = 0, rate = 0;
        char name[9];
        const uint8_t *d = (slot >= X0X_NBREAKS && slot < nloops)
                               ? plat_slot(loop_slot[slot], loop_zone[slot], &ns, &rate, name) : 0;
        if (d) {
            L[w].adpcm = d;
            L[w].nsamples = ns;
            L[w].rate = rate;
            L[w].bars = loop_bars(ns, rate);
            breaks_set_loop(&brk, w, &L[w]);
        } else {
            breaks_set_loop(&brk, w, brk_builtin(slot < X0X_NBREAKS ? slot : 0));   /* built-in (or an empty user slot: the first) */
        }
    }
    brk_hold = 0;
}

void engine_init(pattern_t *patterns, song_t *song, lane_t *lanes, const sound_t *base)
{
    int p;
    for (p = 0; p < NPARTS * NDRUM; p++)
        prob[p / NDRUM][p % NDRUM] = 100;             /* every step plays, until a lane says */
    drone_init(&dn);
    drone_swarm_buffer(&dn, swarm_l, swarm_r);
    drum808_init(&d808);
    melody_init(&mel[0]);
    melody_init(&mel[1]);
    breaks_init(&brk);
    fxbus_init(&fx, dly_buf, DLY_LEN);
    fxbus_reverb_buffers(&fx, rv_buf, rv_ps);
    fxbus_set_lead(&fx, FX_LEAD);
    master_init(&mst);
    seq_init(&seq, patterns);
    seq.song = song;
    motion_init(&mot, lanes);
    mot.base = mot_base;
    mot.apply = mot_apply;
    mot_base_snd = base;
    for (p = 0; p < NPARTS; p++) {
        mix_level[p] = 1.0f;
        mix_rev[p] = mix_dly[p] = 0.0f;
    }
}
