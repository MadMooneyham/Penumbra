/* SPDX-License-Identifier: GPL-3.0-only */
/* Penumbra's audio engine; see engine.h. */
#include "engine.h"
#include "plat.h"
#include "../dsp/drone.h"
#include "../dsp/melody.h"
#include "../dsp/vox.h"
#include "../dsp/pulse.h"
#include "../dsp/texture.h"
#include "../dsp/fxbus.h"
#include "../dsp/master.h"
#include "../dsp/fastmath.h"
#include "balance.h"

#define POOL PEN_POOL

/* ---------------------------------------------------------------- state --- */
tclock_t eng_clock;
volatile uint16_t eng_level;
volatile uint32_t eng_steps;
uint32_t eng_out_clips;

static drone_t dn POOL;
static melody_t line POOL;                       /* LINE: the kept melody voice, set softer (scene_default) */
static vox_t vox POOL;
static pulse_t pulse POOL;
static texture_t tex POOL;
volatile uint32_t eng_kicks;
journey_t eng_jr;
static jrun_t jr;
const jrun_t *engine_jr(void) { return &jr; }
static fxbus_t fx POOL;
static master_t mst;
#define DLY_LEN 66150                            /* 1.5 s */
static int16_t dly_buf[DLY_LEN] POOL;
static int16_t rv_buf[SV_TOTAL] POOL, rv_ps[SV_PS_LEN] POOL;
static float swarm_l[SWARM_LEN] POOL, swarm_r[SWARM_LEN] POOL;

_Static_assert(ENG_DRONES == DRONE_VOICES, "engine.h names the drone voices");
/* a 256-frame block is 5.805 ms, 139,320 ticks of 24 MHz (the benchmark's 100 %; 0.7-beta's overflowed) */
_Static_assert(PLAT_TICKS_PER_BLOCK(256) == 139319u, "the block's budget in 24 MHz ticks");

/* the voices' places in the mix */
enum { P_DRONE, P_VOX, P_LINE, P_PULSE, P_TEX, NPARTS };

/* ------------------------------------------------------------ benchmark --- */
/* each stage's 24 MHz ticks this block; summed over a second (acc), then published (pub) for the
 * main loop, which copies it when the serial moves (and again if it moved while copying) */
static uint32_t st_t[ST_N];
static struct {
    float sum[ST_N], budget;
    uint16_t peak[ST_N];
    uint32_t blocks, frames;
} acc;
static bench_t pub;
static volatile uint32_t pub_serial;
static const char *const ST_NAME[ST_N] = {"CTRL", "DRONE 1", "DRONE 2", "DRN BUS", "VOX", "LINE", "PULSE",
                                          "TEXTURE", "FX WET", "WAIT", "OUTPUT", "CORE 1", "CORE 2"};
const char *engine_stage_name(int st) { return st >= 0 && st < ST_N ? ST_NAME[st] : ""; }

uint32_t engine_bench(bench_t *out)
{
    uint32_t s, tries;
    for (tries = 0; tries < 4u; tries++) {
        s = pub_serial;
        *out = pub;
        if (s == pub_serial)
            break;
    }
    return s;
}

static volatile uint8_t drone_core = DRONE_SPLIT; /* whose the drone voices are */
static void bench_block(uint32_t n)
{
    float budget = (float)PLAT_TICKS_PER_BLOCK(n);
    uint32_t k;
    for (k = 0; k < ST_N; k++) {
        uint32_t p10 = (uint32_t)((float)st_t[k] * 1000.0f / budget);
        acc.sum[k] += (float)st_t[k];
        if (p10 > acc.peak[k])
            acc.peak[k] = (uint16_t)(p10 > 65535u ? 65535u : p10);
    }
    acc.budget += budget;
    acc.blocks++;
    acc.frames += n;
    if (acc.frames >= 44100u) {                  /* a second: publish, start again */
        for (k = 0; k < ST_N; k++) {
            pub.avg[k] = (uint16_t)(acc.sum[k] * 1000.0f / acc.budget + 0.5f);
            pub.peak[k] = acc.peak[k];
            acc.sum[k] = 0.0f;
            acc.peak[k] = 0;
        }
        pub.blocks = acc.blocks;
        pub.two_cores = (uint8_t)plat_cpu2_on();
        pub.drone_core = drone_core;
        acc.budget = 0.0f;
        acc.blocks = acc.frames = 0;
        __asm__ volatile("" ::: "memory");
        pub_serial++;
    }
}

/* ------------------------------------------------------- overload guard --- */
/* past the CPU (a block over 92%, or 85% on average) the drone voices fade down to two oscillators
 * each; after 3 s back under 65% they return. Off during the load test, which measures the full cost */
#define GUARD_ON_LAST 92u
#define GUARD_ON_AVG 85u
#define GUARD_OFF 65u
#define GUARD_CALM_FRAMES (3u * 44100u)
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
}

/* ------------------------------------------------------- command queue --- */
/* uint32 commands, single producer (main loop) / single consumer (audio ISR): type:4 | a:8 | b:8 | c:12 */
enum { C_DRONE_KEY, C_VOX_ON, C_VOX_OFF, C_TEST, C_MIDI_CH, C_PULSE, C_WHITE, C_LOOPS, C_TRAVEL, C_LOADED,
       C_VOICE, C_FX };
#define CQ 256u
static uint32_t cq[CQ];
static volatile uint32_t cq_w, cq_r;
static void cq_put(uint32_t type, uint32_t a, uint32_t b, uint32_t c)
{
    uint32_t w = cq_w;
    if (w - cq_r >= CQ)
        return;                                  /* full: dropped (nothing queued here is a flood) */
    cq[w % CQ] = (type << 28) | ((a & 0xFFu) << 20) | ((b & 0xFFu) << 12) | (c & 0xFFFu);
    __asm__ volatile("" ::: "memory");
    cq_w = w + 1;
}
void engine_drone_key(int voice) { cq_put(C_DRONE_KEY, (uint32_t)voice, 0, 0); }
void engine_vox_on(int note, int vel) { cq_put(C_VOX_ON, 0, (uint32_t)note, (uint32_t)vel); }
void engine_vox_off(int note) { cq_put(C_VOX_OFF, 0, (uint32_t)note, 0); }
static volatile uint8_t vox_sounding;
int engine_vox_sounding(void) { return vox_sounding; }
static volatile uint8_t testing;
void engine_load_test(int on) { cq_put(C_TEST, (uint32_t)(on != 0), 0, 0); }
int engine_load_testing(void) { return testing; }
void engine_midi_ch(int ch) { cq_put(C_MIDI_CH, (uint32_t)ch, 0, 0); }
void engine_pulse(int which, int vel) { cq_put(C_PULSE, (uint32_t)which, 0, (uint32_t)vel); }
void engine_white_key(int w, int down, int oct) { cq_put(C_WHITE, (uint32_t)w, (uint32_t)(down != 0), (uint32_t)(oct + 8)); }
void engine_loops(int op, uint32_t mask) { cq_put(C_LOOPS, (uint32_t)op, mask, 0); }
void engine_travel(int scene) { cq_put(C_TRAVEL, (uint32_t)scene, 0, 0); }
void engine_journey_loaded(void) { cq_put(C_LOADED, 0, 0, 0); }
void engine_voice(int voice, int field, int value) { cq_put(C_VOICE, (uint32_t)voice, (uint32_t)field, (uint32_t)value); }
void engine_fx(int i, int value) { cq_put(C_FX, (uint32_t)i, 0, (uint32_t)value); }
static void hit(int which, float vel)            /* (ISR) a PULSE hit; the kicks are counted for the screen */
{
    pulse_hit(&pulse, which, vel);
    if (which == PULSE_KICK)
        eng_kicks++;
}
int engine_drone_on(int voice) { return voice >= 0 && voice < DRONE_VOICES && dn.v[voice].on; }
float engine_drone_level(int voice)
{
    return voice >= 0 && voice < DRONE_VOICES && dn.v[voice].level > 0.0f ? fm_minf(dn.v[voice].amp / dn.v[voice].level, 1.0f) : 0.0f;
}

/* --------------------------------------------------------------- the scene --- */
/* the test scene, after the prototype's "Cinematic" (drone-proto/drone.html): an A minor drone (root,
 * fifth, octave, minor third, high octave on DR1-DR5; the prototype's second fifth went with the sixth
 * voice), each voice a little off true like a Solar tuned by ear, spread across the field; VOX a soft
 * pad of two saws; all of it into a long shimmering reverb and a worn ping-pong tape delay, dotted
 * quarters */
static const float MIX_BASE[NPARTS] = {1.0f, 0.57f, 0.8f, 0.9f, 1.0f};   /* VOX 3 dB under 0.10's */
static float mix_level[NPARTS] = {1.0f, 0.57f, 0.8f, 0.9f, 1.0f}, mix_rev[NPARTS], mix_dly[NPARTS];
static float rev_base[NPARTS], dly_base[NPARTS], space_k = 1.0f;

/* the VOICES page's settings and the FX page's (settings.c keeps them; they come here as commands) */
static uint8_t voice_set[EV_N][VF_N] = {{100, 64, 20, 64}, {100, 64, 30, 64}, {100, 64, 80, 64}, {80, 64, 80, 64},
                                        {100, 64, 70, 64}};
static const uint8_t EF_PARAM[EF_N] = {FX_RV_DECAY, FXX_RV_SIZE, FXX_RV_SHIMMER, FX_RV_TONE, FX_DL_TIME, FX_DL_FDBK,
                                       FX_DL_TONE, FX_DL_WEAR};
static const uint8_t EF_EXT[EF_N] = {0, 1, 1, 0, 0, 0, 0, 0};
static uint8_t fx_set[EF_N] = {91, 89, 25, 57, 10, 82, 57, 44};
int engine_fx_max(int i) { return i == EF_DL_TIME ? fxbus_param(FX_DL_TIME)->max : 127; }
const char *engine_fx_value_name(int i, int value)
{
    const param_t *p = i == EF_DL_TIME ? fxbus_param(FX_DL_TIME) : 0;
    return p && p->names && value >= 0 && value <= p->max ? p->names[value] : 0;
}
static void fx_apply(int i)
{
    if (i < 0 || i >= EF_N)
        return;
    if (EF_EXT[i])
        fxbus_ext_set(&fx, EF_PARAM[i], fx_set[i]);
    else
        fxbus_set(&fx, EF_PARAM[i], fx_set[i]);
}
static void voice_level(int v)                   /* LEVEL into the mix: 100 is as made, the square law */
{
    static const uint8_t PART[EV_N] = {P_DRONE, P_LINE, P_VOX, P_PULSE, P_TEX};
    float l = (float)voice_set[v][VF_LEVEL] * (1.0f / 100.0f);
    mix_level[PART[v]] = MIX_BASE[PART[v]] * l * l;
}

static void scene_default(void)
{
    static const uint8_t NOTE[DRONE_VOICES] = {24, 31, 36, 39, 48};       /* 0 +7 +12 +15 +24 */
    static const uint8_t FINE[DRONE_VOICES] = {64, 61, 66, 60, 67};       /* 0 -4 +3 -7 +5 cents */
    static const uint8_t PAN[DRONE_VOICES] = {64, 38, 89, 26, 102};
    int v, i;
    for (v = 0; v < DRONE_VOICES; v++) {
        for (i = 0; i < DN_NPARAMS; i++)
            drone_set(&dn, v, i, drone_param(v, i)->def);
        drone_set(&dn, v, DN_NOTE, NOTE[v]);
        drone_set(&dn, v, DN_FINE, FINE[v]);
        drone_set(&dn, v, DN_PAN, PAN[v]);
    }
    drone_set(&dn, 4, DN_SPREAD, 30);            /* the high octave: a wider, shimmering cluster */
    for (i = 0; i < DB_NPARAMS; i++)
        drone_set(&dn, DRONE_KIT, i, drone_param(DRONE_KIT, i)->def);
    for (i = 0; i < MEL_NPARAMS; i++)
        melody_set(&line, i, melody_param(i)->def);
    melody_set(&line, MEL_CUTOFF, 72);           /* LINE: softer and rounder than the prototype's lead */
    melody_set(&line, MEL_RESO, 30);
    melody_set(&line, MEL_ENVMOD, 40);
    melody_set(&line, MEL_ATTACK, 40);
    melody_set(&line, MEL_SUSTAIN, 60);
    melody_set(&line, MEL_RELEASE, 96);
    melody_set(&line, MEL_WAVE, 18);
    melody_set(&line, MEL_SUB, 15);
    melody_set(&line, MEL_VIB, 18);
    for (i = 0; i < VX_NPARAMS; i++)
        vox_set(&vox, i, vox_param(i)->def);
    for (i = 0; i < PL_NPARAMS; i++)
        pulse_set(&pulse, i, pulse_param(i)->def);
    pulse_root(&pulse, 33);                      /* the kick on the drone's root, A1 */
    for (i = 0; i < TX_NPARAMS; i++)
        texture_set(&tex, i, texture_param(i)->def);
    for (i = 0; i < fxbus_nparams(); i++)
        fxbus_set(&fx, i, fxbus_param(i)->def);
    for (i = 0; i < fxbus_ext_nparams(); i++)
        fxbus_ext_set(&fx, i, fxbus_ext_param(i)->def);
    fxbus_set(&fx, FX_VOLUME, 127);              /* unity */
    fxbus_set(&fx, FX_DL_TYPE, 1);               /* tape */
    fxbus_set(&fx, FX_DL_PING, 1);
    for (i = 0; i < EF_N; i++)                   /* (1/4. delay, long and worn: the FX page's) */
        fx_apply(i);
    for (v = 0; v < NPARTS; v++) {
        rev_base[v] = 64.0f / 127.0f;
        dly_base[v] = 41.0f / 127.0f;
    }
    rev_base[P_VOX] = 80.0f / 127.0f;            /* VOX further back, in the shimmer */
    rev_base[P_PULSE] = 12.0f / 127.0f;          /* the kick dry and close, a breath of room; no echo */
    dly_base[P_PULSE] = 0.0f;
    rev_base[P_TEX] = 70.0f / 127.0f;            /* the wind far off */
    dly_base[P_TEX] = 20.0f / 127.0f;
    for (v = 0; v < NPARTS; v++) {
        mix_rev[v] = rev_base[v];
        mix_dly[v] = dly_base[v];
    }
    for (v = 0; v < EV_N; v++)
        voice_level(v);
}

/* ------------------------------------------------------------- the journey --- */
/* what the journey plays goes out to the voices here (audio ISR) */
static void hit(int which, float vel);
static void js_drone_note(void *x, int v, int midi) { (void)x; drone_set(&dn, v, DN_NOTE, midi - 9); }   /* (ROOT A1: NOTE 24 = 33) */
static void js_drone_latch(void *x, uint32_t m)
{
    int v;
    (void)x;
    for (v = 0; v < DRONE_VOICES; v++)
        if ((m >> v & 1u) && !dn.v[v].on)
            drone_trigger(&dn, v, 1.0f);
}
static void js_drone_release(void *x)
{
    int v;
    (void)x;
    for (v = 0; v < DRONE_VOICES; v++)
        if (dn.v[v].on)
            drone_trigger(&dn, v, 1.0f);         /* (latched: the key again lets it fall) */
}
volatile uint32_t eng_line_notes, eng_vox_notes;
static void js_line_on(void *x, int midi, int acc, int slide)
{
    (void)x;
    melody_note_on(&line, midi, acc, slide);
    eng_line_notes++;
}
static void js_line_off(void *x) { (void)x; melody_note_off(&line); }
static void js_vox_on(void *x, int midi, int vel)
{
    (void)x;
    vox_note_on(&vox, midi, (float)vel * (1.0f / 127.0f));
    eng_vox_notes++;
}
static void js_vox_off(void *x, int midi) { (void)x; vox_note_off(&vox, midi); }
static void js_pulse(void *x, int which, int vel) { (void)x; hit(which, (float)vel * (1.0f / 127.0f)); }
static const jr_sink_t JSINK = {0, js_drone_note, js_drone_latch, js_drone_release, js_line_on, js_line_off,
                                js_vox_on, js_vox_off, js_pulse};

/* the scene and the HOME macros into the voices, each block: TONE brightens everything (with each
 * voice's own TONE), SPACE scales every send, the wind follows the moon. Each pot is set only when
 * its value moves */
static int16_t cache[16];
enum { K_DCUT, K_SWARM, K_VCUT, K_LCUT, K_TLVL, K_TCOL, K_ROOT, K_SPREAD, K_GLIDE, K_VATK, K_PDEC, K_TSWEEP, NK };
static int moved(int k, int v)
{
    if (cache[k] == v)
        return 0;
    cache[k] = (int16_t)v;
    return 1;
}
static int clamp127(float x) { return x < 0.0f ? 0 : x > 127.0f ? 127 : (int)(x + 0.5f); }
static void conduct(void)
{
    const jr_out_t *o = &jr.out;
    const float tone = jr.macro_s[MAC_TONE] - 64.0f, bright = o->bright + tone;
    int v, x;
    if (moved(K_DCUT, x = clamp127(28.0f + bright * 0.66f + (float)(voice_set[EV_DRONE][VF_TONE] - 64))))
        drone_set(&dn, DRONE_KIT, DB_CUTOFF, x);
    if (moved(K_SWARM, x = clamp127(o->swarm)))
        drone_set(&dn, DRONE_KIT, DB_SWARM, x);
    if (moved(K_VCUT, x = clamp127(30.0f + bright * 0.47f + (float)(voice_set[EV_VOX][VF_TONE] - 64))))
        vox_set(&vox, VX_CUTOFF, x);
    if (moved(K_LCUT, x = clamp127(42.0f + bright * 0.47f + (float)(voice_set[EV_LINE][VF_TONE] - 64))))
        melody_set(&line, MEL_CUTOFF, x);
    if (moved(K_TLVL, x = clamp127(o->tex * o->fade * (0.6f + 0.4f * o->intensity))))
        texture_set(&tex, TX_LEVEL, x);
    if (moved(K_TCOL, x = clamp127(o->colour + tone * 0.5f + (float)(voice_set[EV_TEX][VF_TONE] - 64))))
        texture_set(&tex, TX_COLOUR, x);
    if (moved(K_ROOT, o->root_midi))
        pulse_root(&pulse, o->root_midi);
    if (moved(K_SPREAD, x = voice_set[EV_DRONE][VF_CHAR]))
        for (v = 0; v < DRONE_VOICES; v++)
            drone_set(&dn, v, DN_SPREAD, v == 4 ? x + 10 : x);
    if (moved(K_GLIDE, x = voice_set[EV_LINE][VF_CHAR]))
        melody_set(&line, MEL_GLIDE, x);
    if (moved(K_VATK, x = voice_set[EV_VOX][VF_CHAR]))
        vox_set(&vox, VX_ATTACK, x);
    if (moved(K_PDEC, x = voice_set[EV_PULSE][VF_CHAR]))
        pulse_set(&pulse, PL_DECAY, x);
    if (moved(K_TSWEEP, x = voice_set[EV_TEX][VF_CHAR]))
        texture_set(&tex, TX_SWEEP, x);
    {   /* SPACE: the scene's x the macro, 64 x 64 as made; each voice's own send on top */
        static const uint8_t EV_OF[NPARTS] = {EV_DRONE, EV_VOX, EV_LINE, EV_PULSE, EV_TEX};
        space_k = fm_clampf(o->space * jr.macro_s[MAC_SPACE] * (1.0f / (80.0f * 64.0f)), 0.0f, 1.8f);
        for (v = 0; v < NPARTS; v++) {
            float s = space_k * (float)voice_set[EV_OF[v]][VF_SEND] * (1.0f / 64.0f);
            mix_rev[v] = fm_minf(rev_base[v] * s, 1.0f);
            mix_dly[v] = fm_minf(dly_base[v] * s, 1.0f);
        }
    }
}
static void conduct_reset(void)
{
    int k;
    for (k = 0; k < 16; k++)
        cache[k] = -1;
}

/* the load test: the heaviest the voices get. Every drone voice latched on its thickest stack
 * (Min9, four oscillators) with SWARM full, VOX's four voices held (a new chord every two seconds,
 * taking the sounding voices), LINE playing eighths, the reverb long and shimmering. Off, the scene
 * comes back and the drones are latched as they were */
static uint8_t test_latched;                     /* the voices that were latched before the test */
static uint32_t test_left, test_i;
static void test_set(int on)
{
    int v;
    if (on == testing)
        return;
    if (on) {
        test_latched = 0;
        for (v = 0; v < DRONE_VOICES; v++) {
            if (dn.v[v].on)
                test_latched |= (uint8_t)(1u << v);
            drone_set(&dn, v, DN_MODE, DN_MODE_HOLD);
            drone_set(&dn, v, DN_STACK, 3);
            drone_set(&dn, v, DN_SPREAD, 60);
            if (!dn.v[v].on)
                drone_trigger(&dn, v, 1.0f);
        }
        drone_set(&dn, DRONE_KIT, DB_SWARM, 127);
        drone_set(&dn, DRONE_KIT, DB_WIND, 64);
        fxbus_ext_set(&fx, FXX_RV_SHIMMER, 127);
        texture_set(&tex, TX_LEVEL, 127);
        if (jr.playing) {                        /* the journey waits (the test plays its own) */
            jr.line_note = jr.vox_note = -1;
            jr.line_ok = jr.vox_ok = 0;
        }
        test_left = test_i = 0;
        guard_enabled = 0;
    } else {
        scene_default();
        conduct_reset();
        for (v = 0; v < DRONE_VOICES; v++)
            if (dn.v[v].on != (int32_t)((test_latched >> v) & 1u))
                drone_trigger(&dn, v, 1.0f);
        melody_note_off(&line);
        vox_all_off(&vox);
        jr_restart(&jr, &JSINK);                 /* the drone back in the journey's chord */
        guard_enabled = 1;
    }
    testing = (uint8_t)on;
}
static void test_tick(uint32_t n)                /* LINE's eighths, up and down A minor; VOX's chords */
{
    static const uint8_t ARP[8] = {57, 60, 64, 67, 69, 67, 64, 60};
    static const uint8_t CHORD[2][4] = {{57, 60, 64, 67}, {53, 57, 60, 64}};   /* Am7, Fmaj7 */
    int k;
    if (!testing)
        return;
    if (test_left > n) {
        test_left -= n;
        return;
    }
    melody_note_on(&line, ARP[test_i & 7u], (test_i & 3u) == 0u, 0);   /* (an accent each half bar) */
    hit(PULSE_TICK, 0.7f);                       /* PULSE: the tick each eighth, the kick each quarter */
    if (!(test_i & 1u))
        hit(PULSE_KICK, 1.0f);
    if ((test_i & 7u) == 0u) {
        vox_all_off(&vox);
        for (k = 0; k < 4; k++)
            vox_note_on(&vox, CHORD[(test_i >> 3) & 1u][k], 1.0f);
    }
    test_i++;
    test_left = 44100u / 4u;
}

/* ------------------------------------------------------------- MIDI, clock --- */
static uint8_t midi_ch = 1;                      /* VOX's notes in: 1..16, 0 off */
static void clk_midi(void *x, uint8_t st)
{
    (void)x;
    plat_midi_out(0x0Fu | ((uint32_t)st << 8));
}
/* the clock's 16ths are the journey's steps: PLAY (here, or a START from outside) starts it on the
 * downbeat, STOP lets it fall away */
static void clk_step(void *x, uint32_t n)
{
    (void)x;
    eng_steps = n + 1u;
    if (testing)
        return;                                  /* (the load test plays its own) */
    if (!jr.playing)
        jr_start(&jr, &JSINK);
    jr_step(&jr, n, (uint32_t)(44100.0f * 15.0f / fm_maxf(tclock_tempo(&eng_clock), 20.0f)), &JSINK);
}
static void clk_stopped(void *x)
{
    (void)x;
    drone_end_swells(&dn);
    if (jr.playing)
        jr_stop(&jr, &JSINK);
}
static const tclock_sink_t CLK_SINK = {0, clk_midi, clk_step, clk_stopped};

static void midi_in(uint32_t pkt)
{
    uint8_t st = (uint8_t)(pkt >> 8), d1 = (uint8_t)(pkt >> 16) & 0x7F, d2 = (uint8_t)(pkt >> 24) & 0x7F;
    uint8_t ty = st & 0xF0;
    switch (st) {
    case 0xF8: tclock_ext_tick(&eng_clock); return;
    case 0xFA: if (eng_clock.follow) tclock_play(&eng_clock); return;
    case 0xFB: if (eng_clock.follow) tclock_continue(&eng_clock); return;
    case 0xFC: if (eng_clock.follow) tclock_stop(&eng_clock); return;
    default: break;
    }
    if (!midi_ch || st >= 0xF0 || (st & 0x0F) != midi_ch - 1u)
        return;
    if (ty == 0x90 && d2 == 0)
        ty = 0x80;
    if (ty == 0x90) {
        vox_note_on(&vox, d1, (float)d2 * (1.0f / 127.0f));
        jr_vox_played(&jr, d1, 1);
    } else if (ty == 0x80) {
        vox_note_off(&vox, d1);
        jr_vox_played(&jr, d1, 0);
    }
}

/* a white key, by the journey's mode: PLAY plays VOX in the scene's key, GUIDE steers the lines,
 * DRIFT picks the scene to travel to. A key let go ends the note it started, whatever the mode is now */
static int16_t white_note[16] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
static void white_key(int w, int down, int oct)
{
    if (w < 0 || w > 15)
        return;
    if (!down) {
        if (white_note[w] >= 0) {
            vox_note_off(&vox, white_note[w]);
            jr_vox_played(&jr, white_note[w], 0);
            white_note[w] = -1;
        }
        jr_guide(&jr, w, 0);
        return;
    }
    if (eng_jr.mode == MODE_GUIDE) {
        jr_guide(&jr, w, 1);
    } else if (eng_jr.mode == MODE_DRIFT) {
        jr_travel(&jr, w);
    } else {
        int note = jr_key_note(&jr, w, oct);
        if (white_note[w] >= 0)
            vox_note_off(&vox, white_note[w]);
        white_note[w] = (int16_t)note;
        vox_note_on(&vox, note, 100.0f / 127.0f);
        jr_vox_played(&jr, note, 1);
    }
}

static void drain(void)
{
    uint32_t r = cq_r, pkt;
    int guard = 64;
    while (r != cq_w) {
        uint32_t c = cq[r % CQ], type = c >> 28, a = (c >> 20) & 0xFFu, b = (c >> 12) & 0xFFu;
        (void)c;
        r++;
        if (type == C_DRONE_KEY && !testing)
            drone_trigger(&dn, (int)a, 1.0f);
        else if (type == C_VOX_ON)
            vox_note_on(&vox, (int)b, (float)(c & 127u) * (1.0f / 127.0f));
        else if (type == C_VOX_OFF)
            vox_note_off(&vox, (int)b);
        else if (type == C_TEST)
            test_set((int)a);
        else if (type == C_PULSE)
            hit((int)a, (float)(c & 127u) * (1.0f / 127.0f));
        else if (type == C_MIDI_CH)
            midi_ch = (uint8_t)(a <= 16u ? a : 0u);
        else if (type == C_WHITE)
            white_key((int)a, (int)b, (int)(c & 0xFFFu) - 8);
        else if (type == C_LOOPS) {
            if (a == LOOP_CAPTURE)
                jr_capture(&jr, b);
            else if (a == LOOP_RELEASE)
                jr_release(&jr, b);
            else
                jr_record(&jr);
        } else if (type == C_TRAVEL)
            jr_travel(&jr, (int)a);
        else if (type == C_LOADED) {
            jr_restart(&jr, &JSINK);
            conduct_reset();
        } else if (type == C_VOICE && a < EV_N && b < VF_N) {
            voice_set[a][b] = (uint8_t)((c & 0xFFFu) > 127u ? 127u : (c & 0xFFFu));
            voice_level((int)a);
        } else if (type == C_FX && a < EF_N) {
            fx_set[a] = (uint8_t)((c & 0xFFFu) > 127u ? 127u : (c & 0xFFFu));
            if (!testing)
                fx_apply((int)a);
        }
    }
    cq_r = r;
    while (guard-- && plat_midi_in(&pkt))
        midi_in(pkt);
}

/* ------------------------------------------------------------------ output --- */
/* the output's scale: Felucca's -6 dBFS ceiling of the 24-bit codec (2^22), or its full scale (LOUD); the
 * clamp keeps an over from wrapping */
static volatile float out_scale = 4194303.0f, out_lim = 2.0f;
void engine_out_loud(int on)
{
    out_scale = on ? 8388607.0f : 4194303.0f;
    out_lim = on ? 1.0f : 2.0f;
}
/* USB AUDIO FULL: the render at full level, for the USB tap, and the MASTER knob after it, for the
 * speaker, headphones and line out (with the master's ~10 ms glide) */
static volatile int usb_full;
static float vol_after = -1.0f;
void engine_usb_full(int on) { usb_full = on; }
void engine_drone_core(int mode) { drone_core = (uint8_t)(mode >= DRONE_CORE1 && mode <= DRONE_SPLIT ? mode : DRONE_SPLIT); }
static volatile uint32_t eng_quiet_frames;
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

/* ------------------------------------------------------------------- mix --- */
/* a mix gain that glides to its target (one pole, ~10 ms) instead of stepping each block. cur < 0:
 * never used, takes the target at once; settled, cur IS the target */
typedef struct { float cur, tgt; } glide_t;
#define GLIDE_A 0.0022651f                       /* 1 - exp(-1 / (10 ms * 44.1 kHz)) */
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
enum { GL_L, GL_R, GL_REV, GL_DLY, GL_N };
static glide_t mix_glide[NPARTS][GL_N] = {{{-1, 0}, {-1, 0}, {-1, 0}, {-1, 0}}, {{-1, 0}, {-1, 0}, {-1, 0}, {-1, 0}},
                                          {{-1, 0}, {-1, 0}, {-1, 0}, {-1, 0}}, {{-1, 0}, {-1, 0}, {-1, 0}, {-1, 0}},
                                          {{-1, 0}, {-1, 0}, {-1, 0}, {-1, 0}}};
static void mix_targets(int p)
{
    glide_set(&mix_glide[p][GL_L], mix_level[p]);
    glide_set(&mix_glide[p][GL_R], mix_level[p]);
    glide_set(&mix_glide[p][GL_REV], mix_level[p] * mix_rev[p]);
    glide_set(&mix_glide[p][GL_DLY], mix_level[p] * mix_dly[p]);
}

/* ---------------------------------------------------------------- render --- */
/* the reverb and the delay run a block behind: each block keeps its sends (fx_send[fx_par]) and
 * hears the returns of the block before, which the first core works out while the second renders:
 * time it would otherwise spend waiting. The delay reads FX_LEAD sooner to make up (fxbus_set_lead) */
#define FX_LEAD 256
static float fx_send[2][2][256] POOL;           /* [block parity][rev, dly][sample] */
static float wet_l[256] POOL, wet_r[256] POOL;
static uint8_t fx_par;
/* the drone's bus runs a block behind too, on the second core: each block's voices (both cores'
 * shares) are summed into dbus[bus_par], and the next block's core 2 runs the bus on them in place
 * while core 1 renders that block's own share. The drone is heard 5.8 ms later; nothing else is */
static float dbus[2][2][256] POOL;              /* [block parity][L, R][sample] */
static float dn2_l[256] POOL, dn2_r[256] POOL;
static float line_buf[256] POOL, mono[256] POOL, vox_l[256] POOL, vox_r[256] POOL;
static float out_l[256] POOL, out_r[256] POOL, rev[256] POOL, dly[256] POOL;
static uint8_t line_was_idle, vox_was_active, pulse_was_active, bus_par;
static float pulse_l[256] POOL, pulse_r[256] POOL, tex_l[256] POOL, tex_r[256] POOL;
static uint8_t tex_was_active;
static uint8_t bus_due, bus_plan;               /* the last block planned voices: its bus runs now, on its plan */
static uint32_t bus_n;

/* SPLIT balances itself: what each core does besides the drone voices, and what one drone
 * oscillator costs, kept as running averages of the blocks' own timings (24 MHz ticks). Each block's
 * voices then go where the two cores finish closest together: of every way to share the sounding
 * voices, the one whose busier core is least busy. The split only changes when another is better by
 * more than BAL_HYST of the block, so voices do not hop from core to core on noise. Until the first
 * timings, the second core takes half the oscillators. */
#define BAL_A 0.0625f                           /* averaging: about 16 blocks (0.1 s) */
#define BAL_HYST 0.03f
static float bal_fix1, bal_fix2, bal_osc;       /* core 1's own work, core 2's, a drone oscillator */
static uint32_t bal_mask, bal_osc1, bal_osc2;   /* the split, and the oscillators each core rendered */
static uint32_t drone_mask2(uint32_t n)
{
    uint32_t all = (1u << DRONE_VOICES) - 1u;
    float cost[DRONE_VOICES];
    int v, total = 0;
    if (drone_core == DRONE_CORE1)
        return 0;
    if (drone_core == DRONE_CORE2)
        return all;
    for (v = 0; v < DRONE_VOICES; v++)
        total += drone_cost(&dn, v);
    if (bal_osc <= 0.0f) {                       /* no timings yet: half the oscillators */
        uint32_t m = 0;
        int two = 0;
        for (v = 0; v < DRONE_VOICES && 2 * two < total; v++)
            if (drone_cost(&dn, v)) {
                two += drone_cost(&dn, v);
                m |= 1u << v;
            }
        return bal_mask = m;
    }
    for (v = 0; v < DRONE_VOICES; v++)
        cost[v] = (float)drone_cost(&dn, v) * bal_osc;
    return bal_mask = balance_pick(bal_fix1, bal_fix2, cost, DRONE_VOICES, bal_mask,
                                   BAL_HYST * (float)PLAT_TICKS_PER_BLOCK(n));
}
static void balance_learn(void)                 /* after a block, from its stages' times */
{
    float osc = (float)(bal_osc1 + bal_osc2);
    bal_fix1 += ((float)(st_t[ST_CTRL] + st_t[ST_FXWET]) - bal_fix1) * BAL_A;
    bal_fix2 += ((float)(st_t[ST_DRNBUS] + st_t[ST_VOX] + st_t[ST_LINE] + st_t[ST_PULSE] + st_t[ST_TEX]) - bal_fix2) * BAL_A;
    if (osc > 0.0f) {
        float per = (float)(st_t[ST_DRN1] + st_t[ST_DRN2]) / osc;
        bal_osc = bal_osc > 0.0f ? bal_osc + (per - bal_osc) * BAL_A : per;
    }
}
static uint32_t mask_osc(uint32_t m)
{
    uint32_t o = 0;
    int v;
    for (v = 0; v < DRONE_VOICES; v++)
        if (m >> v & 1u)
            o += (uint32_t)drone_cost(&dn, v);
    return o;
}

/* the second core's job (or the first's, with one core): with arg bit 15 the drone's bus on the last
 * block's voices (DRN BUS), its drone voices (arg bits 9..13, the voices' mask) into dn2, VOX and
 * LINE and PULSE. It times itself on the 24 MHz timer, which both cores read, and touches only the bus, its own
 * voices and buffers: nothing here is mixed */
#define C2_BUS 0x8000u
static void core2_job(uint32_t arg)
{
    uint32_t n = arg & 0xFFu ? arg & 0xFFu : 256u, mask = (arg >> 9) & 0x1Fu, t0 = plat_ticks24(), t = t0, i;
    if (arg & C2_BUS) {
        float *bl = dbus[bus_par ^ 1u][0], *br = dbus[bus_par ^ 1u][1];
        drone_bus_filter(&dn, bus_plan, bl, br);
        drone_bus_swarm(&dn, bus_plan, bl, br);
        st_t[ST_DRNBUS] = plat_ticks24() - t;
        t = plat_ticks24();
    }
    if (mask) {
        uint32_t tv = t;
        for (i = 0; i < n; i++)
            dn2_l[i] = dn2_r[i] = 0.0f;
        drone_voices(&dn, mask, dn2_l, dn2_r);
        t = plat_ticks24();
        st_t[ST_DRN2] = t - tv;
    }
    vox_was_active = (uint8_t)vox_active(&vox);
    if (vox_was_active) {
        uint32_t tv = t;
        for (i = 0; i < n; i++)
            vox_l[i] = vox_r[i] = 0.0f;
        vox_render(&vox, vox_l, vox_r, (int)n);
        t = plat_ticks24();
        st_t[ST_VOX] = t - tv;
    }
    vox_sounding = vox_was_active;
    line_was_idle = (uint8_t)line.idle;
    melody_render(&line, line_buf, (int)n);
    st_t[ST_LINE] = plat_ticks24() - t;
    t = plat_ticks24();
    pulse_was_active = (uint8_t)pulse_active(&pulse);
    if (pulse_was_active) {
        for (i = 0; i < n; i++)
            pulse_l[i] = pulse_r[i] = 0.0f;
        pulse_render(&pulse, pulse_l, pulse_r, (int)n);
        st_t[ST_PULSE] = plat_ticks24() - t;
    }
    t = plat_ticks24();
    tex_was_active = (uint8_t)texture_active(&tex);
    if (tex_was_active) {
        for (i = 0; i < n; i++)
            tex_l[i] = tex_r[i] = 0.0f;
        texture_render(&tex, tex_l, tex_r, (int)n);
        st_t[ST_TEX] = plat_ticks24() - t;
    }
    st_t[ST_CORE2] = plat_ticks24() - t0;
}

void engine_render(int32_t *out_lr, uint32_t n)
{
    uint32_t t0 = plat_ticks24(), t, i, k, done, arg, mask2 = 0;
    float vol = usb_full ? 1.0f : (float)plat_master() / 4096.0f, pk = 0.0f;
    float *dl = dbus[bus_par][0], *dr = dbus[bus_par][1];
    int on2, planned = 0, plan = 0;
    if (n > 256u)
        n = 256u;
    for (k = 0; k < ST_N; k++)
        st_t[k] = 0;
    /* CTRL: commands, MIDI in, the clock (its events at their sample), the drone's control steps */
    guard_apply();
    drain();
    test_tick(n);
    tclock_advance(&eng_clock, 0, &CLK_SINK);
    for (done = 0; done < n; done += k) {
        k = tclock_until(&eng_clock, n - done);
        tclock_advance(&eng_clock, k, &CLK_SINK);
    }
    fxbus_set_bpm(&fx, tclock_tempo(&eng_clock));
    if (!testing) {                              /* the director, then the scene into the voices */
        jr_tick(&jr, n, &JSINK);
        conduct();
    }
    if (drone_active(&dn)) {                     /* before either core's voices */
        planned = 1;
        plan = drone_plan(&dn, (int)n);
        mask2 = drone_mask2(n);
    }
    t = plat_ticks24();
    st_t[ST_CTRL] = t - t0;
    /* core 2 starts (the last block's drone bus, its share of the voices, LINE); core 1 renders its
     * share of the voices and the effects returns meanwhile */
    arg = (n & 0xFFu) | (mask2 << 9) | (bus_due ? C2_BUS : 0u);
    on2 = plat_cpu2_run(core2_job, arg);
    t = plat_ticks24();                          /* (the simulator runs core 2's job inside the hand-off) */
    if (planned) {
        for (i = 0; i < n; i++)
            dl[i] = dr[i] = 0.0f;
        drone_voices(&dn, ~mask2 & ((1u << DRONE_VOICES) - 1u), dl, dr);
        st_t[ST_DRN1] = plat_ticks24() - t;
        t = plat_ticks24();
    }
    fxbus_wet_st(&fx, fx_send[fx_par ^ 1u][0], fx_send[fx_par ^ 1u][1], wet_l, wet_r, (int)n);
    st_t[ST_FXWET] = plat_ticks24() - t;
    t = plat_ticks24();
    if (!on2 || plat_cpu2_wait())
        core2_job(arg);                          /* one core (or the second failed): here */
    else
        st_t[ST_WAIT] = plat_ticks24() - t;
    t = plat_ticks24();
    bal_osc2 = planned && on2 ? mask_osc(mask2) : 0;   /* (one core: its job's voices ran here, untimed as DRONE 2) */
    bal_osc1 = planned ? mask_osc(~mask2 & ((1u << DRONE_VOICES) - 1u)) : 0;
    if (planned && mask2)                        /* this block's voices, both shares: the next block's bus */
        for (i = 0; i < n; i++) {
            dl[i] += dn2_l[i];
            dr[i] += dn2_r[i];
        }
    /* MIX: the voices by their gains into dry and the sends; out = dry + the returns, then the
     * distortion and the glue compressor */
    for (i = 0; i < n; i++)
        out_l[i] = out_r[i] = rev[i] = dly[i] = 0.0f;
    mix_targets(P_DRONE);
    mix_targets(P_VOX);
    mix_targets(P_LINE);
    mix_targets(P_PULSE);
    mix_targets(P_TEX);
    if (bus_due) {                               /* the drone: the last block's voices, through the bus */
        const float *bl = dbus[bus_par ^ 1u][0], *br = dbus[bus_par ^ 1u][1];
        uint32_t m = bus_n < n ? bus_n : n;      /* (the device's blocks are all 256: equal) */
        for (i = 0; i < m; i++)
            mono[i] = (bl[i] + br[i]) * 0.5f;
        add_glide(out_l, bl, &mix_glide[P_DRONE][GL_L], m);
        add_glide(out_r, br, &mix_glide[P_DRONE][GL_R], m);
        add_glide(rev, mono, &mix_glide[P_DRONE][GL_REV], m);
        add_glide(dly, mono, &mix_glide[P_DRONE][GL_DLY], m);
    }
    if (vox_was_active) {
        for (i = 0; i < n; i++)
            mono[i] = (vox_l[i] + vox_r[i]) * 0.5f;
        add_glide(out_l, vox_l, &mix_glide[P_VOX][GL_L], n);
        add_glide(out_r, vox_r, &mix_glide[P_VOX][GL_R], n);
        add_glide(rev, mono, &mix_glide[P_VOX][GL_REV], n);
        add_glide(dly, mono, &mix_glide[P_VOX][GL_DLY], n);
    }
    if (pulse_was_active) {
        for (i = 0; i < n; i++)
            mono[i] = (pulse_l[i] + pulse_r[i]) * 0.5f;
        add_glide(out_l, pulse_l, &mix_glide[P_PULSE][GL_L], n);
        add_glide(out_r, pulse_r, &mix_glide[P_PULSE][GL_R], n);
        add_glide(rev, mono, &mix_glide[P_PULSE][GL_REV], n);
    }
    if (tex_was_active) {
        for (i = 0; i < n; i++)
            mono[i] = (tex_l[i] + tex_r[i]) * 0.5f;
        add_glide(out_l, tex_l, &mix_glide[P_TEX][GL_L], n);
        add_glide(out_r, tex_r, &mix_glide[P_TEX][GL_R], n);
        add_glide(rev, mono, &mix_glide[P_TEX][GL_REV], n);
        add_glide(dly, mono, &mix_glide[P_TEX][GL_DLY], n);
    }
    if (!line_was_idle) {
        add_glide(out_l, line_buf, &mix_glide[P_LINE][GL_L], n);
        add_glide(out_r, line_buf, &mix_glide[P_LINE][GL_R], n);
        add_glide(rev, line_buf, &mix_glide[P_LINE][GL_REV], n);
        add_glide(dly, line_buf, &mix_glide[P_LINE][GL_DLY], n);
    }
    {
        float *sr = fx_send[fx_par][0], *sd = fx_send[fx_par][1];
        for (i = 0; i < n; i++) {
            sr[i] = rev[i];                      /* for the next block */
            sd[i] = dly[i];
            out_l[i] += wet_l[i];
            out_r[i] += wet_r[i];
        }
        for (i = n; i < 256u; i++)               /* (a shorter block: the rest sends nothing) */
            sr[i] = sd[i] = 0.0f;
    }
    fx_par ^= 1u;
    bus_due = (uint8_t)planned;                  /* this block's voices: the next block's bus */
    bus_plan = (uint8_t)plan;
    bus_n = n;
    bus_par ^= 1u;
    fxbus_post_st(&fx, out_l, out_r, (int)n);
    /* the master stage, then the codec's scale (OUTPUT: all of it, from the mix on) */
    master_process_st(&mst, out_l, out_r, (int)n, vol);
    for (i = 0; i < n; i++) {
        float l = out_l[i], r = out_r[i];
        eng_out_clips += (fm_fabsf(l) > out_lim) + (fm_fabsf(r) > out_lim);
        l = l > out_lim ? out_lim : l < -out_lim ? -out_lim : l;
        r = r > out_lim ? out_lim : r < -out_lim ? -out_lim : r;
        out_lr[2u * i] = (int32_t)(l * out_scale);
        out_lr[2u * i + 1u] = (int32_t)(r * out_scale);
        pk = fm_maxf(pk, fm_maxf(fm_fabsf(l), fm_fabsf(r)));
    }
    eng_quiet_frames = pk < 2.5e-4f ? (eng_quiet_frames + n > 0x7FFFFFFFu ? 0x7FFFFFFFu : eng_quiet_frames + n) : 0;
    {
        uint32_t q = (uint32_t)(fm_minf(pk, 1.0f) * 32767.0f);
        if (q > eng_level)
            eng_level = (uint16_t)q;
    }
    st_t[ST_OUT] = plat_ticks24() - t;
    st_t[ST_CORE1] = plat_ticks24() - t0;
    if (on2)
        balance_learn();                         /* (one core: there is nothing to balance) */
    if (!on2)
        st_t[ST_CORE2] = 0;                      /* (its job ran on the first core: counted there) */
    bench_block(n);
}

void engine_init(void)
{
    drone_init(&dn);
    drone_swarm_buffer(&dn, swarm_l, swarm_r);
    melody_init(&line);
    vox_init(&vox);
    pulse_init(&pulse);
    texture_init(&tex);
    fxbus_init(&fx, dly_buf, DLY_LEN);
    fxbus_reverb_buffers(&fx, rv_buf, rv_ps);
    fxbus_set_lead(&fx, FX_LEAD);
    master_init(&mst);
    tclock_init(&eng_clock);
    scene_default();
    conduct_reset();
    jr_preset(0, &eng_jr);                       /* (until settings.c loads the saved one: journey_load_work) */
    jr_init(&jr, &eng_jr);
    jr_restart(&jr, &JSINK);                     /* the drone in its key */
}
