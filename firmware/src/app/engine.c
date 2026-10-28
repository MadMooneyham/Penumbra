/* SPDX-License-Identifier: GPL-3.0-only */
/* Penumbra's audio engine; see engine.h. */
#include "engine.h"
#include "plat.h"
#include "../dsp/drone.h"
#include "../dsp/melody.h"
#include "../dsp/fxbus.h"
#include "../dsp/master.h"
#include "../dsp/fastmath.h"

#define POOL PEN_POOL

/* ---------------------------------------------------------------- state --- */
tclock_t eng_clock;
volatile uint16_t eng_level;
volatile uint32_t eng_steps;
uint32_t eng_out_clips;

static drone_t dn POOL;
static melody_t line POOL;                       /* LINE, for now the kept melody voice */
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
enum { P_DRONE, P_LINE, NPARTS };

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
static const char *const ST_NAME[ST_N] = {"CTRL", "DRONE 1", "DRONE 2", "DRN FILT", "SWARM", "LINE", "FX WET",
                                          "WAIT", "MIX", "MASTER", "CORE 1", "CORE 2"};
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
enum { C_DRONE_KEY, C_LINE_ON, C_LINE_OFF, C_TEST, C_MIDI_CH };
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
void engine_line_on(int note) { cq_put(C_LINE_ON, 0, (uint32_t)note, 0); }
void engine_line_off(int note) { cq_put(C_LINE_OFF, 0, (uint32_t)note, 0); }
static volatile uint8_t testing;
void engine_load_test(int on) { cq_put(C_TEST, (uint32_t)(on != 0), 0, 0); }
int engine_load_testing(void) { return testing; }
void engine_midi_ch(int ch) { cq_put(C_MIDI_CH, (uint32_t)ch, 0, 0); }
int engine_drone_on(int voice) { return voice >= 0 && voice < DRONE_VOICES && dn.v[voice].on; }
float engine_drone_level(int voice)
{
    return voice >= 0 && voice < DRONE_VOICES && dn.v[voice].level > 0.0f ? fm_minf(dn.v[voice].amp / dn.v[voice].level, 1.0f) : 0.0f;
}

/* --------------------------------------------------------------- the scene --- */
/* the test scene, after the prototype's "Cinematic" (drone-proto/drone.html): an A minor drone (root,
 * fifth, octave, minor third, high octave on DR1-DR5; the prototype's second fifth went with the sixth
 * voice), each voice a little off true like a Solar tuned by ear, spread across the field; LINE and the drones into a long shimmering reverb
 * and a worn ping-pong tape delay, dotted quarters */
static float mix_level[NPARTS] = {1.0f, 1.0f}, mix_rev[NPARTS], mix_dly[NPARTS];
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
    for (i = 0; i < fxbus_nparams(); i++)
        fxbus_set(&fx, i, fxbus_param(i)->def);
    for (i = 0; i < fxbus_ext_nparams(); i++)
        fxbus_ext_set(&fx, i, fxbus_ext_param(i)->def);
    fxbus_set(&fx, FX_VOLUME, 127);              /* unity */
    fxbus_set(&fx, FX_DL_TYPE, 1);               /* tape */
    fxbus_set(&fx, FX_DL_TIME, 10);              /* 1/4. */
    fxbus_set(&fx, FX_DL_FDBK, 82);
    fxbus_set(&fx, FX_DL_TONE, 57);
    fxbus_set(&fx, FX_DL_WEAR, 44);
    fxbus_set(&fx, FX_DL_PING, 1);
    for (v = 0; v < NPARTS; v++) {
        mix_rev[v] = 64.0f / 127.0f;
        mix_dly[v] = 41.0f / 127.0f;
    }
}

/* the load test: the heaviest the voices get. Every drone voice latched on its thickest stack
 * (Min9, four oscillators) with SWARM full, LINE playing eighths, the reverb long and shimmering.
 * Off, the scene comes back and the drones are latched as they were */
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
        test_left = test_i = 0;
        guard_enabled = 0;
    } else {
        scene_default();
        for (v = 0; v < DRONE_VOICES; v++)
            if (dn.v[v].on != (int32_t)((test_latched >> v) & 1u))
                drone_trigger(&dn, v, 1.0f);
        melody_note_off(&line);
        guard_enabled = 1;
    }
    testing = (uint8_t)on;
}
static void test_tick(uint32_t n)                /* LINE's eighths, up and down A minor */
{
    static const uint8_t ARP[8] = {57, 60, 64, 67, 69, 67, 64, 60};
    if (!testing)
        return;
    if (test_left > n) {
        test_left -= n;
        return;
    }
    melody_note_on(&line, ARP[test_i & 7u], (test_i & 3u) == 0u, 0);   /* (an accent each half bar) */
    test_i++;
    test_left = 44100u / 4u;
}

/* ------------------------------------------------------------- LINE keys --- */
/* a mono voice: the newest held note sounds; let go, it glides back to the one held before */
#define LHELD 8
static uint8_t lheld[LHELD], nlheld;
static void line_on(int note)
{
    int i;
    for (i = 0; i < nlheld; i++)
        if (lheld[i] == note)
            return;
    if (nlheld == LHELD) {                       /* full: the oldest goes */
        for (i = 1; i < LHELD; i++)
            lheld[i - 1] = lheld[i];
        nlheld--;
    }
    melody_note_on(&line, note, 0, nlheld > 0);
    lheld[nlheld++] = (uint8_t)note;
}
static void line_off(int note)
{
    int i, j;
    for (i = 0; i < nlheld && lheld[i] != note; i++)
        ;
    if (i == nlheld)
        return;
    for (j = i + 1; j < nlheld; j++)
        lheld[j - 1] = lheld[j];
    nlheld--;
    if (!nlheld)
        melody_note_off(&line);
    else if (i == nlheld)                        /* the one sounding went: back to the newest left */
        melody_note_on(&line, lheld[nlheld - 1], 0, 1);
}

/* ------------------------------------------------------------- MIDI, clock --- */
static uint8_t midi_ch = 1;                      /* LINE's notes in: 1..16, 0 off */
static void clk_midi(void *x, uint8_t st)
{
    (void)x;
    plat_midi_out(0x0Fu | ((uint32_t)st << 8));
}
static void clk_step(void *x, uint32_t n)
{
    (void)x;
    eng_steps = n + 1u;
}
static void clk_stopped(void *x)
{
    (void)x;
    drone_end_swells(&dn);
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
    if (ty == 0x90)
        line_on(d1);
    else if (ty == 0x80)
        line_off(d1);
}

static void drain(void)
{
    uint32_t r = cq_r, pkt;
    int guard = 64;
    while (r != cq_w) {
        uint32_t c = cq[r % CQ], type = c >> 28, a = (c >> 20) & 0xFFu, b = (c >> 12) & 0xFFu;
        r++;
        if (type == C_DRONE_KEY && !testing)
            drone_trigger(&dn, (int)a, 1.0f);
        else if (type == C_LINE_ON)
            line_on((int)b);
        else if (type == C_LINE_OFF)
            line_off((int)b);
        else if (type == C_TEST)
            test_set((int)a);
        else if (type == C_MIDI_CH)
            midi_ch = (uint8_t)(a <= 16u ? a : 0u);
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
static glide_t mix_glide[NPARTS][GL_N] = {{{-1, 0}, {-1, 0}, {-1, 0}, {-1, 0}}, {{-1, 0}, {-1, 0}, {-1, 0}, {-1, 0}}};
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
static float line_buf[256] POOL, mono[256] POOL;
static float out_l[256] POOL, out_r[256] POOL, rev[256] POOL, dly[256] POOL;
static uint8_t line_was_idle, bus_par;
static uint8_t bus_due, bus_plan;               /* the last block planned voices: its bus runs now, on its plan */
static uint32_t bus_n;

/* whose each drone voice is this block: bit v set, the second core's. SPLIT gives the second core
 * voices until it holds half the sounding oscillators: with the bus and LINE beside them it then
 * carries about what the first does with its effects returns */
static uint32_t drone_mask2(void)
{
    uint32_t m = 0;
    int v, total = 0, two = 0;
    if (drone_core == DRONE_CORE1)
        return 0;
    if (drone_core == DRONE_CORE2)
        return (1u << DRONE_VOICES) - 1u;
    for (v = 0; v < DRONE_VOICES; v++)
        total += drone_cost(&dn, v);
    for (v = 0; v < DRONE_VOICES && 2 * two < total; v++)
        if (drone_cost(&dn, v)) {
            two += drone_cost(&dn, v);
            m |= 1u << v;
        }
    return m;
}

/* the second core's job (or the first's, with one core): with arg bit 15 the drone's bus on the last
 * block's voices (DRN FILT, SWARM), its drone voices (arg bits 9..13, the voices' mask) into dn2, and
 * LINE. It times itself on the 24 MHz timer, which both cores read, and touches only the bus, its own
 * voices and buffers: nothing here is mixed */
#define C2_BUS 0x8000u
static void core2_job(uint32_t arg)
{
    uint32_t n = arg & 0xFFu ? arg & 0xFFu : 256u, mask = (arg >> 9) & 0x1Fu, t0 = plat_ticks24(), t = t0, i;
    if (arg & C2_BUS) {
        float *bl = dbus[bus_par ^ 1u][0], *br = dbus[bus_par ^ 1u][1];
        drone_bus_filter(&dn, bus_plan, bl, br);
        st_t[ST_DRNFILT] = plat_ticks24() - t;
        t = plat_ticks24();
        drone_bus_swarm(&dn, bus_plan, bl, br);
        st_t[ST_SWARM] = plat_ticks24() - t;
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
    line_was_idle = (uint8_t)line.idle;
    melody_render(&line, line_buf, (int)n);
    st_t[ST_LINE] = plat_ticks24() - t;
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
    if (drone_active(&dn)) {                     /* before either core's voices */
        planned = 1;
        plan = drone_plan(&dn, (int)n);
        mask2 = drone_mask2();
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
    mix_targets(P_LINE);
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
    st_t[ST_MIX] = plat_ticks24() - t;
    t = plat_ticks24();
    /* MASTER, then the codec's scale */
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
    st_t[ST_MASTER] = plat_ticks24() - t;
    st_t[ST_CORE1] = plat_ticks24() - t0;
    if (!on2)
        st_t[ST_CORE2] = 0;                      /* (its job ran on the first core: counted there) */
    bench_block(n);
}

void engine_init(void)
{
    drone_init(&dn);
    drone_swarm_buffer(&dn, swarm_l, swarm_r);
    melody_init(&line);
    fxbus_init(&fx, dly_buf, DLY_LEN);
    fxbus_reverb_buffers(&fx, rv_buf, rv_ps);
    fxbus_set_lead(&fx, FX_LEAD);
    master_init(&mst);
    tclock_init(&eng_clock);
    scene_default();
}
