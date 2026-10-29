/* SPDX-License-Identifier: GPL-3.0-only */
/* Penumbra's user interface: HOME, where you play, and a page one press away for everything else.
 *
 *   HOME     the corona: the Lunar Eclipse Forge mark on the eclipsed moon, its corona pulsing with
 *            the music, a bead on the rim for each drone voice, a star for each note you play. Under
 *            it the scene, the moon's phase, the mode and the loops. KNOBS 1-4: the macros DRIFT,
 *            DENSITY, SPACE, TONE.
 *   VOICES   each voice's LEVEL, TONE, character and SPACE (SELECT: the voice)
 *   SCENE    the journey's scenes, field by field (SELECT: the field, KNOB 1: the scene, 2-4: change
 *            it); SEL stores the moment as a new scene
 *   LOOP     a captured loop's 16 steps (SELECT: LINE, VOX, PULSE; KNOB 1 the step, 2 its note, 3 on
 *            or tied, 4 MUTATE); SEL captures or lets go
 *   FX       the reverb and the delay (SELECT: which)
 *   JOURNEY  the 6 presets and 8 user slots: SEL opens one, SAVE stores the one playing in a slot
 *   SYSTEM   the settings, a list: SELECT moves, any knob (or SEL) changes the row
 *   BENCH    the benchmark: each stage of the render, average and worst block; SEL: the load test
 *
 * Buttons, one job each (they act when let go; held, they say what they do instead):
 *   HOME home, ENV VOICES, EDIT SCENE, SEQ LOOP, FX FX, LFO JOURNEY, GLO SYSTEM then BENCH,
 *   ARP the mode (PLAY, GUIDE, DRIFT), PLAY starts and stops (with a fade), REC loops what plays (PLAY
 *   mode: records VOX from the keys for a bar; pressed with loops on: lets them go), SAVE saves now,
 *   SEL the page's action, OCT- / OCT+ the white keys' octave.
 * Keys: black 1-5 latch the drone voices, 6 and 7 hit PULSE; the white keys follow the mode.
 * ALGO: the tempo. PRESET: the journeys (opens JOURNEY). Every page but BENCH goes home after 30 s idle.
 *
 * The screen is three bands (header, main, knob strip), each drawn into Felucca's canvas and
 * blitted only where its pixels changed: SPI time is the scarce thing. */
#include "penumbra.h"
#include "../dsp/fastmath.h"

/* ================================================================ model === */
static const int8_t KEY_WHITE[NKEYS] = {0, -1, 1, -1, 2, -1, 3, 4, -1, 5, -1, 6, 7, -1, 8, -1, 9, -1, 10, 11, -1, 12, -1, 13, 14, -1, 15};
static const int8_t KEY_BLACK[NKEYS] = {-1, 0, -1, 1, -1, 2, -1, -1, 3, -1, 4, -1, -1, 5, -1, 6, -1, 7, -1, -1, 8, -1, 9, -1, -1, 10, -1};
static uint8_t BLACK_KEY[11], WHITE_KEY[16];

enum { V_HOME, V_VOICES, V_SCENE, V_LOOP, V_FX, V_JOURNEY, V_SYSTEM, V_BENCH, NVIEWS };
static const char *const VIEW_N[NVIEWS] = {"HOME", "VOICES", "SCENE", "LOOP", "FX", "JOURNEY", "SYSTEM", "BENCH"};

#define HELP_MS 600u                       /* held this long, a button says what it does instead */
#define IDLE_HOME_MS 30000u

typedef struct {
    uint8_t view;
    int8_t oct;                        /* the white keys' octave, -2..+2 */
    uint8_t row;                       /* SYSTEM's row */
    uint8_t vsel, ssel, srow, lsel, lstep, fxrow, jrow;
    uint32_t btn, keys;                /* held, as last seen */
    uint32_t press_t[NB], helped;      /* when each button went down; held long enough to say its job */
    char msg[28];
    uint32_t msg_until;
    uint32_t frame;
    uint8_t dirty;                     /* the settings or the journey changed since they were saved */
    uint32_t act_t;                    /* the last change (a save waits for quiet) */
    uint32_t touch_t;                  /* the last touch (pages go home when idle) */
    uint8_t stars;                     /* notes played since the last frame: a star each */
} ui_t;
static ui_t ui;

/* ============================================================ text helpers === */
static char *put_s(char *b, const char *s)
{
    while (*s)
        *b++ = *s++;
    *b = 0;
    return b;
}
static char *put_i(char *b, int v)
{
    char t[12];
    int n = 0, neg = v < 0;
    uint32_t u = (uint32_t)(neg ? -v : v);
    do {
        t[n++] = (char)('0' + u % 10u);
        u /= 10u;
    } while (u);
    if (neg)
        *b++ = '-';
    while (n)
        *b++ = t[--n];
    *b = 0;
    return b;
}
static char *put_d1(char *b, uint32_t x10)        /* 123 -> "12.3" */
{
    b = put_i(b, (int)(x10 / 10u));
    *b++ = '.';
    *b++ = (char)('0' + x10 % 10u);
    *b = 0;
    return b;
}
static char *put_sgn(char *b, int v) { return put_i(put_s(b, v > 0 ? "+" : ""), v); }

static void say_as(const char *a, const char *b, uint32_t flags)
{
    char *q = put_s(ui.msg, a);
    if (b)
        put_s(q, b);
    ui.msg_until = plat_ms() + 1300u;
    ann_say(a, b, flags);                         /* and to a screen reader (announce.c) */
}
static void say(const char *a, const char *b) { say_as(a, b, ANN_NOW); }
static void warn(const char *a, const char *b) { say_as(a, b, ANN_NOW | ANN_KEEP); }
static void say_n(const char *a, int v)
{
    char t[12];
    put_i(t, v);
    say(a, t);
}
enum { D_SET = 1, D_JR = 2 };                    /* what is not saved: the settings, the journey */
static void changed_as(int what)
{
    ui.dirty |= (uint8_t)what;
    ui.act_t = plat_ms();
}
static void changed(void) { changed_as(D_SET); }
static void changed_jr(void) { changed_as(D_JR); }
static int clamp_i(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
static int wrap_i(int v, int n) { return ((v % n) + n) % n; }

/* ================================================================ settings === */
/* SYSTEM's rows: a setting, its choices, and what a change does */
enum { R_CLKOUT, R_CLKIN, R_MIDICH, R_CORES, R_DRNCORE, R_OUTLOUD, R_USBFULL, R_AUTOPLAY, R_ABOUT, NROWS };
static const char *const ROW_N[NROWS] = {"MIDI CLOCK OUT", "FOLLOW CLOCK IN", "VOX MIDI CH", "2ND CORE",
                                         "DRONE CORE", "OUTPUT LEVEL", "USB AUDIO", "PLAY AT POWER-ON", "ABOUT"};
static const char *const ONOFF_N[] = {"OFF", "ON"};
static const char *const CORE_N[] = {"1ST", "2ND", "SPLIT"};
static const char *const LOUD_N[] = {"NORMAL", "LOUD"};
static const char *const USB_N[] = {"MASTER", "FULL"};

static int row_n(int r)                           /* choices; 0: not a setting */
{
    return r == R_MIDICH ? 17 : r == R_DRNCORE ? 3 : r == R_ABOUT ? 0 : 2;
}
static int row_get(int r)
{
    switch (r) {
    case R_CLKOUT: return cfg.clk_out;
    case R_CLKIN: return cfg.clk_in;
    case R_MIDICH: return cfg.midi_ch;
    case R_CORES: return !cfg.one_core;
    case R_DRNCORE: return cfg.drone_core;
    case R_OUTLOUD: return cfg.out_loud;
    case R_USBFULL: return cfg.usb_full;
    case R_AUTOPLAY: return cfg.autoplay;
    default: return 0;
    }
}
static void row_text(int r, char *b)
{
    int v = row_get(r);
    switch (r) {
    case R_MIDICH:
        if (v)
            put_i(b, v);
        else
            put_s(b, "OFF");
        break;
    case R_DRNCORE: put_s(b, CORE_N[v]); break;
    case R_OUTLOUD: put_s(b, LOUD_N[v]); break;
    case R_USBFULL: put_s(b, USB_N[v]); break;
    case R_ABOUT: put_s(put_s(put_s(b, FW_NAME), " "), PEN_VERSION); break;
    default: put_s(b, ONOFF_N[v ? 1 : 0]); break;
    }
}
static void row_set(int r, int v)
{
    int n = row_n(r);
    char b[24];
    if (!n)
        return;
    v = wrap_i(v, n);
    if (v == row_get(r))
        return;
    switch (r) {
    case R_CLKOUT: cfg.clk_out = (uint8_t)v; break;
    case R_CLKIN: cfg.clk_in = (uint8_t)v; break;
    case R_MIDICH: cfg.midi_ch = (uint8_t)v; break;
    case R_CORES: cfg.one_core = (uint8_t)!v; break;
    case R_DRNCORE: cfg.drone_core = (uint8_t)v; break;
    case R_OUTLOUD: cfg.out_loud = (uint8_t)v; break;
    case R_USBFULL: cfg.usb_full = (uint8_t)v; break;
    case R_AUTOPLAY: cfg.autoplay = (uint8_t)v; break;
    default: break;
    }
    settings_apply();
    changed();
    row_text(r, b);
    if (r == R_CORES)
        say(v ? "2ND CORE ON" : "2ND CORE OFF", ": AT THE NEXT POWER-ON");
    else {
        char t[40];
        put_s(put_s(put_s(t, ROW_N[r]), " "), b);
        say(t, 0);
    }
}

/* saved when the output has been silent a second and nothing changed for two: a flash erase stops
 * the audio for a moment, so never under a sound (a journey playing never falls silent: SAVE, or
 * STOP, and it saves when its tails have gone). SAVE saves at once, whatever plays. */
#define SAVE_QUIET_MS 2000u
#define SAVE_SILENT_MS 1000u
static void save_now(void)                        /* what changed (SAVE with nothing changed: both) */
{
    int what = ui.dirty ? ui.dirty : D_SET | D_JR, bad = 0;
    if ((what & D_SET) && settings_save() == 0)
        ui.dirty &= (uint8_t)~D_SET;
    else if (what & D_SET)
        bad = 1;
    if ((what & D_JR) && journey_save_work() == 0)
        ui.dirty &= (uint8_t)~D_JR;
    else if (what & D_JR)
        bad = 1;
    if (bad) {
        ui.act_t = plat_ms();                     /* (and again after the next quiet, not every frame) */
        warn("NOT SAVED", 0);
    } else {
        say("SAVED", 0);
    }
}
static void autosave(void)
{
    if (ui.dirty && plat_ms() - ui.act_t >= SAVE_QUIET_MS && engine_quiet_ms() >= SAVE_SILENT_MS)
        save_now();
}

/* ============================================================ the journey === */
static const scene_t *scene_sel(void) { return &eng_jr.scene[ui.ssel % (eng_jr.nscenes ? eng_jr.nscenes : 1)]; }
static int loops_on(void) { return eng_jr.loop[LP_LINE].on || eng_jr.loop[LP_VOX].on || eng_jr.loop[LP_PULSE].on; }
static const char *const LOOP_N[JR_LOOPS] = {"LINE", "VOX", "PULSE"};
static const char *const MACRO_N[NMACROS] = {"DRIFT", "DENSITY", "SPACE", "TONE"};

/* SCENE's fields */
enum { SF_KEY, SF_SCALE, SF_CHORDS, SF_VOICING, SF_DENSITY, SF_PULSE, SF_BRIGHT, SF_SWARM, SF_SPACE, SF_WIND,
       SF_COLOUR, SF_REG, SF_PENTA, NSF };
static const char *const SF_N[NSF] = {"KEY", "SCALE", "CHORDS", "VOICING", "DENSITY", "PULSE", "BRIGHTNESS",
                                      "SWARM", "SPACE", "WIND", "WIND COLOUR", "REGISTER", "PENTATONIC"};
static uint8_t *sf_ptr(scene_t *s, int f)
{
    switch (f) {
    case SF_KEY: return &s->root;
    case SF_SCALE: return &s->scale;
    case SF_CHORDS: return &s->prog;
    case SF_VOICING: return &s->voicing;
    case SF_DENSITY: return &s->density;
    case SF_PULSE: return &s->pulse;
    case SF_BRIGHT: return &s->bright;
    case SF_SWARM: return &s->swarm;
    case SF_SPACE: return &s->space;
    case SF_WIND: return &s->tex;
    case SF_COLOUR: return &s->colour;
    case SF_REG: return &s->reg;
    default: return &s->penta;
    }
}
static void sf_text(const scene_t *s, int f, char *b)
{
    switch (f) {
    case SF_KEY: put_s(b, jr_root_name(s->root)); break;
    case SF_SCALE: put_s(b, jr_scale_name(s->scale)); break;
    case SF_CHORDS: put_s(b, jr_prog_name(s->prog)); break;
    case SF_VOICING: put_s(b, jr_voicing_name(s->voicing)); break;
    case SF_REG: put_i(b, s->reg); break;
    case SF_PENTA: put_s(b, ONOFF_N[s->penta ? 1 : 0]); break;
    default: put_i(b, *sf_ptr((scene_t *)s, f)); break;
    }
}
static void sf_turn(int f, int e)
{
    scene_t *s = &eng_jr.scene[ui.ssel % (eng_jr.nscenes ? eng_jr.nscenes : 1)];
    uint8_t *p = sf_ptr(s, f);
    char b[24], t[40];
    switch (f) {
    case SF_KEY: *p = (uint8_t)wrap_i(*p + e, 12); break;
    case SF_SCALE: *p = (uint8_t)wrap_i(*p + e, NSCALES); break;
    case SF_CHORDS: *p = (uint8_t)wrap_i(*p + e, jr_nprogs()); break;
    case SF_VOICING: *p = (uint8_t)wrap_i(*p + e, NVOICINGS); break;
    case SF_REG: *p = (uint8_t)clamp_i(*p + e, 2, 6); break;
    case SF_PENTA: *p = (uint8_t)(e > 0); break;
    default: *p = (uint8_t)clamp_i(*p + 2 * e, 0, 127); break;
    }
    changed_jr();
    sf_text(s, f, b);
    put_s(put_s(put_s(t, SF_N[f]), " "), b);
    say(t, 0);
}

static void store_scene(void)
{
    scene_t s;
    int n = eng_jr.nscenes;
    jr_scene_now(engine_jr(), &s);
    if (n < JR_SCENES) {
        eng_jr.scene[n] = s;
        eng_jr.nscenes = (uint8_t)(n + 1);
        ui.ssel = (uint8_t)n;
        say_n("STORED AS SCENE ", n + 1);
    } else {
        eng_jr.scene[ui.ssel] = s;
        say_n("STORED OVER SCENE ", ui.ssel + 1);
    }
    changed_jr();
}

static void rec_button(void)
{
    const jrun_t *r = engine_jr();
    if (loops_on() || r->rec) {
        engine_loops(LOOP_RELEASE, 7u);
        say("LOOPS LET GO", 0);
    } else if (!eng_clock.playing) {
        warn("PLAY FIRST", 0);
        return;
    } else if (eng_jr.mode == MODE_PLAY) {
        engine_loops(LOOP_RECORD, 0);
        say("RECORDING VOX: NEXT BAR", 0);
    } else {
        engine_loops(LOOP_CAPTURE, 7u);
        say("CAPTURED", 0);
    }
    changed_jr();
}

static void loop_sel(void)                         /* LOOP's SEL: the voice it shows, captured or let go */
{
    const int v = ui.lsel;
    if (eng_jr.loop[v].on || (v == LP_VOX && engine_jr()->rec)) {
        engine_loops(LOOP_RELEASE, 1u << v);
        say(LOOP_N[v], " LET GO");
    } else if (!eng_clock.playing) {
        warn("PLAY FIRST", 0);
        return;
    } else if (v == LP_VOX && eng_jr.mode == MODE_PLAY) {
        engine_loops(LOOP_RECORD, 0);
        say("RECORDING VOX: NEXT BAR", 0);
    } else {
        engine_loops(LOOP_CAPTURE, 1u << v);
        say(LOOP_N[v], " CAPTURED");
    }
    changed_jr();
}

static void loop_knob(int k, int e)
{
    loop_t *l = &eng_jr.loop[ui.lsel];
    uint8_t *st;
    if (k == 3) {
        l->mutate = (uint8_t)clamp_i(l->mutate + 2 * e, 0, 127);
        say_n("MUTATE ", l->mutate);
        changed_jr();
        return;
    }
    if (k == 0) {
        ui.lstep = (uint8_t)clamp_i(ui.lstep + e, 0, (l->len ? l->len : 16) - 1);
        say_n("STEP ", ui.lstep + 1);
        return;
    }
    if (!l->on) {
        warn("SEL: CAPTURE FIRST", 0);
        return;
    }
    st = &l->step[ui.lstep % JR_STEPS];
    if (ui.lsel == LP_PULSE) {
        uint8_t bit = k == 1 ? PS_KICK : PS_TICK;
        *st = (uint8_t)(e > 0 ? *st | bit : *st & ~bit);
        say(k == 1 ? "KICK " : "TICK ", *st & bit ? "ON" : "OFF");
    } else if (k == 1) {
        int d = (*st & ST_ON) ? ST_DEG(*st) + e : 0;
        *st = (uint8_t)((*st & ST_TIE) | ST_ON | ST_MK(clamp_i(d, -24, 39)));
        {
            char t[8];
            put_sgn(t, ST_DEG(*st));
            say("NOTE ", t);
        }
    } else {
        int s = !(*st & ST_ON) ? 0 : (*st & ST_TIE) ? 2 : 1;
        static const char *const SN[3] = {"OFF", "ON", "TIE"};
        int was_on = (*st & ST_ON) != 0;
        s = clamp_i(s + e, 0, 2);
        *st = (uint8_t)(s == 0 ? 0u : (uint8_t)(ST_ON | (s == 2 ? ST_TIE : 0u) | (was_on ? *st & 0x3Fu : ST_MK(0))));
        say(SN[s], 0);
    }
    changed_jr();
}

/* JOURNEY's rows: the presets, then the user slots */
#define JROWS (JR_PRESETS + JR_USER)
static void jrow_name(int r, char *b)
{
    if (r < JR_PRESETS)
        put_s(b, jr_preset_name(r));
    else
        put_s(put_i(put_s(b, "USER "), r - JR_PRESETS + 1), journey_slot_used(r - JR_PRESETS) ? "" : "  -");
}
static void journey_sel(void)
{
    char b[24];
    jrow_name(ui.jrow, b);
    if (ui.jrow < JR_PRESETS) {
        journey_open_preset(ui.jrow);
    } else if (journey_slot_open(ui.jrow - JR_PRESETS) != 0) {
        warn("EMPTY: SAVE INTO IT", 0);
        return;
    }
    ui.ssel = 0;
    say("OPENED ", b);
    changed_jr();
}
static void save_button(void)
{
    if (ui.view == V_JOURNEY) {
        if (ui.jrow < JR_PRESETS) {
            warn("PICK A USER SLOT", 0);
            return;
        }
        if (journey_slot_save(ui.jrow - JR_PRESETS) == 0)
            say_n("SAVED IN USER ", ui.jrow - JR_PRESETS + 1);
        else
            warn("NOT SAVED", 0);
        return;
    }
    save_now();
}

/* ================================================================== input === */
static void set_view(int v)
{
    if (v == ui.view)
        return;
    if (ui.view == V_BENCH && engine_load_testing())
        engine_load_test(0);                      /* the load test is the BENCH page's */
    ui.view = (uint8_t)v;
    ann_say(VIEW_N[v], 0, ANN_NOW);
}

static const char *const HELP[NB] = {
    [B_FX] = "FX: REVERB, DELAY",   [B_SEL] = "THE PAGE'S ACTION",  [B_ENV] = "VOICES: LEVEL, TONE",
    [B_LFO] = "JOURNEYS",           [B_EDIT] = "SCENE: KEY, CHORDS", [B_GLO] = "SYSTEM, BENCH",
    [B_HOME] = "HOME",              [B_SAVE] = "SAVE ALL NOW",       [B_ARP] = "MODE: PLAY GUIDE DRIFT",
    [B_SEQ] = "LOOP: CAPTURE, STEPS", [B_PLAY] = "START, STOP: FADES", [B_REC] = "LOOP WHAT PLAYS",
    [B_OCTDN] = "WHITE KEYS DOWN",  [B_OCTUP] = "WHITE KEYS UP"};

static void button(int i)
{
    switch (i) {
    case B_HOME: set_view(V_HOME); break;
    case B_ENV: set_view(V_VOICES); break;
    case B_EDIT:
        if (ui.view != V_SCENE)
            ui.ssel = engine_jr()->cur;
        set_view(V_SCENE);
        break;
    case B_SEQ: set_view(V_LOOP); break;
    case B_FX: set_view(V_FX); break;
    case B_LFO: set_view(V_JOURNEY); break;
    case B_GLO: set_view(ui.view == V_SYSTEM ? V_BENCH : V_SYSTEM); break;
    case B_ARP:
        eng_jr.mode = (uint8_t)((eng_jr.mode + 1) % NMODES);
        say("MODE ", jr_mode_name(eng_jr.mode));
        changed_jr();
        break;
    case B_PLAY:
        if (eng_clock.playing)
            tclock_stop(&eng_clock);
        else
            tclock_play(&eng_clock);
        say(eng_clock.playing ? "STOP" : "PLAY", 0);
        break;
    case B_REC: rec_button(); break;
    case B_SAVE: save_button(); break;
    case B_OCTDN:
    case B_OCTUP: {
        char t[8];
        ui.oct = (int8_t)clamp_i(ui.oct + (i == B_OCTUP ? 1 : -1), -2, 2);
        put_sgn(t, ui.oct);
        say("OCTAVE ", t);
        break;
    }
    case B_SEL:
        if (ui.view == V_SYSTEM)
            row_set(ui.row, row_get(ui.row) + 1);
        else if (ui.view == V_BENCH) {
            int on = !engine_load_testing();      /* (the engine takes it at its next block) */
            engine_load_test(on);
            say(on ? "LOAD TEST ON" : "LOAD TEST OFF", 0);
        } else if (ui.view == V_SCENE)
            store_scene();
        else if (ui.view == V_LOOP)
            loop_sel();
        else if (ui.view == V_JOURNEY)
            journey_sel();
        break;
    default: break;
    }
}

static void select_turn(int e)
{
    char t[40], v[24];
    switch (ui.view) {
    case V_SYSTEM:
        ui.row = (uint8_t)clamp_i(ui.row + e, 0, NROWS - 1);
        row_text(ui.row, v);
        put_s(put_s(put_s(t, ROW_N[ui.row]), " "), v);
        ann_say(t, 0, ANN_NOW);
        break;
    case V_VOICES: {
        static const char *const VN[EV_N] = {"DRONE", "LINE", "VOX", "PULSE", "TEXTURE"};
        ui.vsel = (uint8_t)clamp_i(ui.vsel + e, 0, EV_N - 1);
        ann_say(VN[ui.vsel], 0, ANN_NOW);
        break;
    }
    case V_SCENE:
        ui.srow = (uint8_t)clamp_i(ui.srow + e, 0, NSF - 1);
        sf_text(scene_sel(), ui.srow, v);
        put_s(put_s(put_s(t, SF_N[ui.srow]), " "), v);
        ann_say(t, 0, ANN_NOW);
        break;
    case V_LOOP:
        ui.lsel = (uint8_t)clamp_i(ui.lsel + e, 0, JR_LOOPS - 1);
        ui.lstep = 0;
        ann_say(LOOP_N[ui.lsel], 0, ANN_NOW);
        break;
    case V_FX:
        ui.fxrow = (uint8_t)clamp_i(ui.fxrow + e, 0, 1);
        ann_say(ui.fxrow ? "DELAY" : "REVERB", 0, ANN_NOW);
        break;
    case V_JOURNEY:
        ui.jrow = (uint8_t)clamp_i(ui.jrow + e, 0, JROWS - 1);
        jrow_name(ui.jrow, v);
        ann_say(v, 0, ANN_NOW);
        break;
    default: break;
    }
}

static const char *const VCHAR_N[EV_N] = {"DETUNE", "GLIDE", "ATTACK", "DECAY", "SWEEP"};
static const char *const FX_N[EF_N] = {"DECAY", "SIZE", "SHIMMER", "TONE", "TIME", "FEEDBACK", "TONE", "WEAR"};
static void fx_text(int i, char *b)
{
    const char *n = engine_fx_value_name(i, cfg.fx[i]);
    if (n)
        put_s(b, n);
    else
        put_i(b, cfg.fx[i]);
}

static void knob_turn(int k, int e)
{
    char t[40], v[16];
    switch (ui.view) {
    case V_SYSTEM: row_set(ui.row, row_get(ui.row) + (e > 0 ? 1 : -1)); break;
    case V_HOME: {
        uint8_t *m = &eng_jr.macro[k];
        *m = (uint8_t)clamp_i(*m + 2 * e, 0, 127);
        changed_jr();
        if (k == MAC_DRIFT && *m < 2)
            say("DRIFT FROZEN", 0);
        else
            say_n(k == MAC_DRIFT ? "DRIFT " : k == MAC_DENSITY ? "DENSITY " : k == MAC_SPACE ? "SPACE " : "TONE ", *m);
        break;
    }
    case V_VOICES: {
        static const char *const FN[VF_N] = {"LEVEL ", "TONE ", "", "SPACE "};
        uint8_t *p = &cfg.voice[ui.vsel][k];
        *p = (uint8_t)clamp_i(*p + 2 * e, 0, 127);
        engine_voice(ui.vsel, k, *p);
        changed();
        put_i(v, k == VF_TONE || k == VF_SEND ? *p - 64 : *p);
        put_s(put_s(put_s(t, k == VF_CHAR ? VCHAR_N[ui.vsel] : FN[k]), k == VF_CHAR ? " " : ""), v);
        say(t, 0);
        break;
    }
    case V_SCENE:
        if (k == 0) {
            int n = eng_jr.nscenes ? eng_jr.nscenes : 1;
            ui.ssel = (uint8_t)clamp_i(ui.ssel + e, 0, n - 1);
            say_n("SCENE ", ui.ssel + 1);
        } else {
            sf_turn(ui.srow, e);
        }
        break;
    case V_LOOP: loop_knob(k, e); break;
    case V_FX: {
        int i = ui.fxrow * 4 + k, step = engine_fx_max(i) < 127 ? 1 : 2;
        cfg.fx[i] = (uint8_t)clamp_i(cfg.fx[i] + step * e, 0, engine_fx_max(i));
        engine_fx(i, cfg.fx[i]);
        changed();
        fx_text(i, v);
        put_s(put_s(put_s(put_s(t, ui.fxrow ? "DELAY " : "REVERB "), FX_N[i]), " "), v);
        say(t, 0);
        break;
    }
    default: break;
    }
}

static void input(void)
{
    uint32_t b = plat_buttons(), k = plat_keys(), now = plat_ms();
    uint32_t down = b & ~ui.btn, up = ui.btn & ~b, kd = k & ~ui.keys, ku = ui.keys & ~k;
    int i, e;
    if (b || k || down || up)
        ui.touch_t = now;
    ui.btn = b;
    ui.keys = k;
    for (i = 0; i < NKEYS; i++) {
        int w = KEY_WHITE[i], bk = KEY_BLACK[i];
        if (kd >> i & 1u) {
            if (w >= 0) {
                engine_white_key(w, 1, ui.oct);
                if (eng_jr.mode == MODE_PLAY && ui.stars < 8u)
                    ui.stars++;
                if (eng_jr.mode == MODE_DRIFT && w < eng_jr.nscenes && w != engine_jr()->cur)
                    say_n("TO SCENE ", w + 1);
            } else if (bk >= 0 && bk < ENG_DRONES) {
                engine_drone_key(bk);
            } else if (bk == ENG_DRONES || bk == ENG_DRONES + 1) {   /* the next two: PULSE's kick and tick */
                engine_pulse(bk - ENG_DRONES, 110);
            }
        }
        if ((ku >> i & 1u) && w >= 0)
            engine_white_key(w, 0, ui.oct);
    }
    for (i = 0; i < NB; i++) {                    /* a button acts when let go; held, it says its job */
        uint32_t bit = 1u << i;
        if (down & bit) {
            ui.press_t[i] = now;
            ui.helped &= ~bit;
        } else if ((b & bit) && !(ui.helped & bit) && now - ui.press_t[i] >= HELP_MS) {
            ui.helped |= bit;
            if (HELP[i])
                say(HELP[i], 0);
        }
        if ((up & bit) && !(ui.helped & bit))
            button(i);
    }
    if ((e = plat_enc(EN_SELECT)) != 0) {
        ui.touch_t = now;
        select_turn(e);
    }
    if ((e = plat_enc(EN_ALGO)) != 0) {           /* the tempo, wherever you are */
        int bpm = clamp_i(cfg.bpm_x10 / 10 + e, 20, 300);
        ui.touch_t = now;
        cfg.bpm_x10 = (uint16_t)(bpm * 10);
        settings_apply();
        changed();
        say_n("TEMPO ", bpm);
    }
    if ((e = plat_enc(EN_PRESET)) != 0) {         /* the journeys */
        char v[24];
        ui.touch_t = now;
        if (ui.view != V_JOURNEY)
            set_view(V_JOURNEY);
        else
            ui.jrow = (uint8_t)clamp_i(ui.jrow + e, 0, JROWS - 1);
        jrow_name(ui.jrow, v);
        say(v, 0);
    }
    for (i = EN_K1; i <= EN_K4; i++)
        if ((e = plat_enc(i)) != 0) {
            ui.touch_t = now;
            knob_turn(i - EN_K1, e);
        }
    if (ui.view != V_HOME && ui.view != V_BENCH && now - ui.touch_t >= IDLE_HOME_MS)
        set_view(V_HOME);
}

/* ================================================================== drawing === */
#define HDR_H 20
#define MAIN_Y 20
#define MAIN_H 138
#define KNOB_Y 158
#define KNOB_H 82

static uint32_t blit_hash[3];
static uint32_t cv_hash(void)
{
    uint32_t i, h = 2166136261u, n = cv_w * cv_h;
    for (i = 0; i < n; i++)
        h = (h ^ cv_px[i]) * 16777619u;
    return h ^ (cv_w << 16) ^ cv_h;
}
static void cv_commit(int region, uint32_t x, uint32_t y)
{
    uint32_t h = cv_hash();
    if (h != blit_hash[region]) {
        blit_hash[region] = h;
        cv_blit(x, y);
    }
}

/* the main area sends only the rectangle that changed: the panel has no tearing-effect line the
 * FM-1 can see, so a transfer the panel's refresh overtakes shows half old, half new. Rows and
 * 8-pixel column blocks are hashed against the last frame sent; the changed rectangle is packed to
 * the front of the canvas and sent as one transfer. blit_hash[region] == 0 (a reset) sends it all. */
#define MAIN_COLB (240 / 8)
static uint32_t main_rowh[MAIN_H], main_colh[MAIN_COLB];
static void cv_commit_rect(int region, uint32_t x, uint32_t y)
{
    uint32_t rowh[MAIN_H], colh[MAIN_COLB], r, c, r0 = MAIN_H, r1 = 0, c0 = MAIN_COLB, c1 = 0, w, h, k;
    for (c = 0; c < MAIN_COLB; c++)
        colh[c] = 2166136261u;
    for (r = 0; r < MAIN_H; r++) {
        const uint16_t *q = cv_px + r * 240u;
        uint32_t hr = 2166136261u;
        for (c = 0; c < MAIN_COLB; c++) {
            uint32_t hc = colh[c];
            for (k = 0; k < 8u; k++) {
                hr = (hr ^ q[k]) * 16777619u;
                hc = (hc ^ q[k]) * 16777619u;
            }
            colh[c] = hc;
            q += 8;
        }
        rowh[r] = hr;
    }
    if (!blit_hash[region]) {
        r0 = c0 = 0;
        r1 = MAIN_H - 1;
        c1 = MAIN_COLB - 1;
    } else {
        for (r = 0; r < MAIN_H; r++)
            if (rowh[r] != main_rowh[r]) {
                if (r0 == MAIN_H)
                    r0 = r;
                r1 = r;
            }
        for (c = 0; c < MAIN_COLB; c++)
            if (colh[c] != main_colh[c]) {
                if (c0 == MAIN_COLB)
                    c0 = c;
                c1 = c;
            }
        if (r0 == MAIN_H || c0 == MAIN_COLB)
            return;
    }
    blit_hash[region] = 1;
    for (r = 0; r < MAIN_H; r++)
        main_rowh[r] = rowh[r];
    for (c = 0; c < MAIN_COLB; c++)
        main_colh[c] = colh[c];
    w = (c1 - c0 + 1u) * 8u;
    h = r1 - r0 + 1u;
    for (r = 0; r < h; r++) {                         /* pack: the destination never passes the source */
        const uint16_t *src = cv_px + (r0 + r) * 240u + c0 * 8u;
        uint16_t *dst = cv_px + r * w;
        for (k = 0; k < w; k++)
            dst[k] = src[k];
    }
    lcd_blit(x + c0 * 8u, y + r0, w, h, cv_px);
}

static uint16_t blend(uint16_t a, uint16_t b, int t)  /* a..b, t of 16 */
{
    int r = ((a >> 11) * (16 - t) + (b >> 11) * t) >> 4, g = (((a >> 5) & 63) * (16 - t) + ((b >> 5) & 63) * t) >> 4,
        bl = ((a & 31) * (16 - t) + (b & 31) * t) >> 4;
    return (uint16_t)((r << 11) | (g << 5) | bl);
}
static uint16_t dim(uint16_t c, int t) { return blend(C_BG, c, t); }   /* (toward the night, not black) */
#define C_RED RGB(255, 90, 80)

/* a font's capital line, measured once from 'H', so text centres on what the eye sees */
typedef struct { const felucca_font_t *f; int8_t top, bot; } capm_t;
static capm_t capm[6];
static const capm_t *caps(const felucca_font_t *f)
{
    int i;
    for (i = 0; i < 6 && capm[i].f; i++)
        if (capm[i].f == f)
            return &capm[i];
    if (i == 6)
        i = 5;
    {
        uint32_t gi = ((uint32_t)'H' <= f->last ? 'H' : '0') - f->first, w = f->bw[gi], bpr = (w + 1u) / 2u, x, y;
        const uint8_t *gd = f->data + f->off[gi];
        int top = -1, bot = 0;
        for (y = 0; y < f->h; y++)
            for (x = 0; x < w; x++) {
                uint32_t a = gd[y * bpr + x / 2u];
                a = (x & 1u) ? (a & 15u) : (a >> 4);
                if (a >= 8u) {
                    if (top < 0)
                        top = (int)y;
                    bot = (int)y;
                }
            }
        capm[i].f = f;
        capm[i].top = (int8_t)(top < 0 ? 0 : top);
        capm[i].bot = (int8_t)bot;
    }
    return &capm[i];
}
static int32_t vc(const felucca_font_t *f, int32_t y0, int32_t h)   /* f's capitals centred in [y0, y0 + h) */
{
    const capm_t *m = caps(f);
    return y0 + (h - (m->bot - m->top + 1) + 1) / 2 - m->top;
}
static void text_c(int32_t cx, int32_t y, const felucca_font_t *f, const char *s, uint16_t c)
{
    cv_text(cx - text_w(f, s) / 2, y, f, s, c);
}
static void text_r(int32_t rx, int32_t y, const felucca_font_t *f, const char *s, uint16_t c)
{
    cv_text(rx - text_w(f, s), y, f, s, c);
}

/* ---- anti-aliased shapes. Coverage is 0..16; a pixel is blended over what is there. ---- */
static void px_blend(int32_t x, int32_t y, uint16_t c, uint32_t a)
{
    uint16_t *q;
    uint32_t bg;
    y += cv_oy;
    if ((uint32_t)x >= cv_w || y < cv_y0 || y >= cv_y1 || !a)
        return;
    q = &cv_px[(uint32_t)y * cv_w + (uint32_t)x];
    if (a >= 16u) {
        *q = swap16(c);
        return;
    }
    bg = swap16(*q);
    *q = swap16((uint32_t)(((((c >> 11) * a + (bg >> 11) * (16u - a)) >> 4) << 11) |
                           (((((c >> 5) & 63u) * a + ((bg >> 5) & 63u) * (16u - a)) >> 4) << 5) |
                           (((c & 31u) * a + (bg & 31u) * (16u - a)) >> 4)));
}
static void disc(float cx, float cy, float r, uint16_t c)
{
    int32_t x0 = (int32_t)fm_floorf(cx - r - 1.0f), x1 = (int32_t)(cx + r + 1.0f);
    int32_t y0 = (int32_t)fm_floorf(cy - r - 1.0f), y1 = (int32_t)(cy + r + 1.0f), x, y;
    for (y = y0; y <= y1; y++)
        for (x = x0; x <= x1; x++) {
            float dx = (float)x + 0.5f - cx, dy = (float)y + 0.5f - cy, d = fm_sqrtf(dx * dx + dy * dy);
            float a = fm_clampf(r - d + 0.5f, 0.0f, 1.0f);
            px_blend(x, y, c, (uint32_t)(a * 16.0f + 0.5f));
        }
}
static void dot(int32_t x, int32_t y, int32_t r, uint16_t c) { disc((float)x + 0.5f, (float)y + 0.5f, (float)r + 0.3f, c); }

/* the moon's phase, small: lit on the right waxing, all of it full, on the left waning, a rim new */
static void moon_glyph(int cx, int cy, int r, int phase, uint16_t lit)
{
    int x, y;
    for (y = -r - 1; y <= r + 1; y++)
        for (x = -r - 1; x <= r + 1; x++) {
            float d = fm_sqrtf((float)(x * x + y * y)), a = fm_clampf((float)r - d + 0.5f, 0.0f, 1.0f);
            int on = phase == MOON_FULL || (phase == MOON_WAX && x >= 0) || (phase == MOON_WANE && x <= 0);
            if (a > 0.0f)
                px_blend(cx + x, cy + y, on ? lit : RGB(28, 30, 48), (uint32_t)(a * 16.0f + 0.5f));
        }
}

/* atan2 in turns, 0..1 from +x going clockwise on screen (y down) */
static float turns(float y, float x)
{
    float ax = fm_fabsf(x), ay = fm_fabsf(y), a = ax < ay ? ax / (ay + 1e-9f) : ay / (ax + 1e-9f), s = a * a, r;
    r = ((-0.0464964749f * s + 0.15931422f) * s - 0.327622764f) * s * a + a;   /* atan on [0, 1] */
    if (ay > ax)
        r = 1.57079637f - r;
    if (x < 0.0f)
        r = FM_PI - r;
    if (y < 0.0f)
        r = FM_TWO_PI - r;
    return r * (1.0f / FM_TWO_PI);
}

/* ------------------------------------------------------------------ header --- */
static void draw_header(void)
{
    char b[16];
    cv_begin(240, HDR_H, C_BG);
    cv_rect(0, HDR_H - 1, 240, 1, C_LINE);
    if (ui.msg_until && (int32_t)(ui.msg_until - plat_ms()) > 0) {   /* what was just done */
        cv_rect(0, 0, 240, HDR_H - 1, dim(C_AMB, 4));
        cv_rect(0, HDR_H - 1, 240, 1, C_AMB);
        text_c(120, vc(&FONT_B, 0, HDR_H - 1), &FONT_B, ui.msg, C_WHITE);
        cv_commit(0, 0, 0);
        return;
    }
    ui.msg_until = 0;
    dot(6, 9, 3, C_AMB);
    cv_text(14, vc(&FONT_B, 1, 17), &FONT_B, ui.view == V_HOME ? eng_jr.name : VIEW_N[ui.view], C_HI);
    if (ui.dirty)
        dot(132, 9, 2, C_AMB);                    /* not saved yet */
    {   /* following an external clock, the measured tempo wanders a little: the display averages it
         * over about a second, and moves only when that is half a BPM away */
        static float avg;
        static int shown;
        float t = tclock_tempo(&eng_clock);
        if (!eng_clock.ext || avg < 1.0f)
            avg = t;
        avg += (t - avg) * 0.03f;
        if (!eng_clock.ext || fm_fabsf(avg - (float)shown) > 0.6f)
            shown = (int)(avg + 0.5f);
        put_i(b, shown);
    }
    if (eng_clock.ext)
        cv_text(150, vc(&FONT_XS, 1, 17), &FONT_XS, "EXT", C_AMB);
    text_r(212, vc(&FONT_S, 1, 17), &FONT_S, b, eng_clock.ext ? C_AMB : C_HI);
    if (eng_clock.playing) {                      /* the play mark, bright on the beat */
        int i;
        uint16_t c = (eng_steps & 3u) == 1u ? C_WHITE : C_HI;
        for (i = 0; i < 9; i++)
            cv_rect(220 + i / 2, 5 + i / 2, 1, 10 - (i / 2) * 2, c);
    }
    cv_commit(0, 0, 0);
}

/* -------------------------------------------------------- HOME: the corona --- */
#define EC_CX 120
#define EC_CY 56
#define EC_RS 36                                   /* the moon's rim */
#define EC_CW 14                                   /* the corona's reach past it */
#define EC_NSTAR 14
static struct { int16_t x, y; uint8_t life; } ec_star[EC_NSTAR];
static uint8_t ec_star_w;
static float ec_lvl, ec_flare;
static uint32_t ec_rng = 0x2545F491u, ec_last_step, ec_last_kick;
static uint32_t ec_rand(void)
{
    ec_rng ^= ec_rng << 13;
    ec_rng ^= ec_rng >> 17;
    ec_rng ^= ec_rng << 5;
    return ec_rng;
}
static void ec_star_new(void)                     /* a VOX note: a star comes out */
{
    float a = (float)(ec_rand() & 1023u) * (FM_TWO_PI / 1024.0f);
    float r = (float)(EC_RS + EC_CW + 4) + (float)(ec_rand() % 40u);
    int x = EC_CX + (int)(fm_cosf(a) * r * 1.6f), y = EC_CY + (int)(fm_sinf(a) * r * 0.62f);
    ec_star[ec_star_w].x = (int16_t)(x < 6 ? 6 : x > 234 ? 234 : x);
    ec_star[ec_star_w].y = (int16_t)(y < 6 ? 6 : y > 100 ? 100 : y);
    ec_star[ec_star_w].life = 255;
    ec_star_w = (uint8_t)((ec_star_w + 1) % EC_NSTAR);
}

static void draw_corona(void)
{
    const uint16_t SUN = RGB(255, 244, 222), PALE = RGB(255, 222, 170), CORONA = C_AMB;
    const uint16_t FACE = RGB(12, 13, 24), MOON = RGB(226, 224, 236), bg = C_BG;
    const int R2 = EC_RS + EC_CW;
    float lvl = fm_minf((float)eng_level * (2.5f / 32768.0f), 1.0f), amp, rot, breath;
    int x, y, k;
    eng_level = (uint16_t)(eng_level - (eng_level >> 3));   /* the meter falls (~300 ms) */
    ec_lvl += (lvl - ec_lvl) * (lvl > ec_lvl ? 0.5f : 0.12f);   /* up fast, down slowly: a pulse */
    if (eng_kicks != ec_last_kick) {              /* each kick flares it */
        ec_last_kick = eng_kicks;
        ec_flare = 1.0f;
    } else if (eng_steps != ec_last_step) {       /* else the clock's beats, softly */
        ec_last_step = eng_steps;
        if (eng_steps && ((eng_steps - 1u) & 3u) == 0u)
            ec_flare = fm_maxf(ec_flare, 0.35f);
    }
    ec_flare *= 0.9f;
    breath = 0.5f + 0.5f * fm_sin_turns((float)(ui.frame & 255u) * (1.0f / 256.0f));   /* (quiet: it breathes) */
    amp = 0.22f + 0.08f * breath + 0.7f * ec_lvl + 0.35f * ec_flare;
    rot = (float)(ui.frame & 4095u) * (1.0f / 4096.0f);
    for (k = 0; k < 40; k++) {                    /* the sky: fixed stars, each twinkling its own way */
        uint32_t h = (uint32_t)k * 2654435761u;
        int sx = (int)(h % 236u) + 2, sy = (int)((h >> 12) % 98u) + 4;
        int tw8 = (int)(((h >> 20) + ui.frame / 6u) & 15u);
        cv_pset(sx, sy, blend(bg, C_GRAY, tw8 < 3 ? 12 : 5));
    }
    for (k = 0; k < EC_NSTAR; k++)                /* the notes' stars, fading */
        if (ec_star[k].life) {
            dot(ec_star[k].x, ec_star[k].y, ec_star[k].life > 160 ? 2 : 1, blend(bg, RGB(130, 176, 255), ec_star[k].life >> 4));
            ec_star[k].life = (uint8_t)(ec_star[k].life > 5 ? ec_star[k].life - 5 : 0);
        }
    /* the corona: a bright rim, glowing out into the dark in slowly turning rays; inside, the face */
    for (y = EC_CY - R2; y <= EC_CY + R2; y++)
        for (x = EC_CX - R2; x <= EC_CX + R2; x++) {
            float dx = (float)x + 0.5f - (float)EC_CX, dy = (float)y + 0.5f - (float)EC_CY, d2 = dx * dx + dy * dy, d, a;
            uint16_t col;
            if (d2 < (float)((EC_RS - 3) * (EC_RS - 3))) {
                cv_pset(x, y, FACE);
                continue;
            }
            if (d2 >= (float)(R2 * R2))
                continue;
            d = fm_sqrtf(d2);
            if (d < (float)EC_RS) {               /* the rim's inner edge, sharp, into the face */
                a = fm_clampf(1.0f - ((float)EC_RS - d) * 0.45f, 0.0f, 1.0f) * fm_minf(1.0f, 0.55f + amp);
                col = blend(FACE, blend(PALE, SUN, (int)(a * 16.0f)), (int)(a * 16.0f + 0.5f));
            } else {
                float w = 1.0f - (d - (float)EC_RS) / (float)EC_CW;
                float ray = 0.72f + 0.28f * fm_sin_turns(turns(dy, dx) * 7.0f + rot) * (0.4f + ec_lvl);
                a = fm_clampf(w * w * amp * ray * 1.3f, 0.0f, 1.0f);
                if (a < 0.03f)
                    continue;
                col = blend(bg, blend(CORONA, PALE, (int)(w * w * w * 16.0f)), (int)(a * 16.0f + 0.5f));
            }
            cv_pset(x, y, col);
        }
    {   /* the mark, in moonlight */
        const int ox = EC_CX - LOGO_S_W / 2, oy = EC_CY - LOGO_S_H / 2 + 1;
        for (y = 0; y < LOGO_S_H; y++)
            for (x = 0; x < LOGO_S_W; x++) {
                uint32_t a = LOGO_S_PX[y * ((LOGO_S_W + 1) / 2) + x / 2];
                a = (x & 1) ? (a & 15u) : (a >> 4);
                if (a)
                    px_blend(ox + x, oy + y, MOON, a + (a == 15u));
            }
    }
    for (k = 0; k < ENG_DRONES; k++) {          /* the drones: a bead each on the rim, from the top, clockwise */
        float a = ((float)k / (float)ENG_DRONES - 0.25f) * FM_TWO_PI;
        float bx = (float)EC_CX + fm_cosf(a) * ((float)EC_RS + 1.0f), by = (float)EC_CY + fm_sinf(a) * ((float)EC_RS + 1.0f);
        float lv = engine_drone_level(k);         /* lit as loud as it is: it rises in and falls away */
        disc(bx, by, 1.6f, C_DIM);
        if (lv > 0.01f) {
            disc(bx, by, 4.0f, blend(bg, CORONA, (int)(lv * (9.0f + 6.0f * breath))));
            disc(bx, by, 1.8f, blend(bg, SUN, (int)(lv * 16.0f)));
        }
    }
}

static char *scene_line(char *b, int s)           /* "2/4  A AEOLIAN" */
{
    const scene_t *sc = &eng_jr.scene[s % (eng_jr.nscenes ? eng_jr.nscenes : 1)];
    return put_s(put_s(put_s(put_s(put_i(put_s(put_i(b, s + 1), "/"), eng_jr.nscenes), "  "), jr_root_name(sc->root)), " "),
                 jr_scale_name(sc->scale));
}
static int same(const char *a, const char *b)
{
    while (*a && *a == *b)
        a++, b++;
    return *a == *b;
}

static void draw_home(void)
{
    const jrun_t *r = engine_jr();
    char b[40], *q;
    int v, x;
    draw_corona();
    q = scene_line(b, r->cur);
    if (r->travelling)                            /* on the way to the next */
        put_i(put_s(q, "  > "), r->dest + 1);
    cv_text(6, 112, &FONT_XS, b, C_HI);
    moon_glyph(222, 116, 5, r->moon, r->playing ? RGB(226, 224, 236) : C_DIM);
    text_r(213, 112, &FONT_XS, r->playing ? jr_moon_name(r->moon) : "STOPPED", r->playing ? C_GRAY : C_DIM);
    put_s(put_s(b, "MODE "), jr_mode_name(eng_jr.mode));
    cv_text(6, 126, &FONT_XS, b, eng_jr.mode == MODE_PLAY ? C_GRAY : C_AMB);
    for (v = 0, x = 120; v < JR_LOOPS; v++) {     /* the loops: lit when captured */
        int on = eng_jr.loop[v].on, recording = v == LP_VOX && r->rec;
        uint16_t c = recording ? ((ui.frame >> 4) & 1u ? C_RED : dim(C_RED, 6)) : on ? C_AMB : C_DIM;
        cv_text(x, 126, &FONT_XS, LOOP_N[v], c);
        x += text_w(&FONT_XS, LOOP_N[v]) + 10;
    }
}

/* ----------------------------------------------------------------- lists --- */
#define ROW_H 22
#define ROWS_SHOWN 6
static void list_row(int i, int sel, const char *name, const char *val, uint16_t vc_)
{
    int y = 2 + i * ROW_H;
    if (sel)
        cv_rect(0, y, 240, ROW_H - 2, dim(C_AMB, 3));
    cv_text(10, vc(&FONT_S, y, ROW_H - 2), &FONT_S, name, sel ? C_HI : C_GRAY);
    text_r(230, vc(&FONT_S, y, ROW_H - 2), &FONT_S, val, sel ? C_AMB : vc_);
}
static int list_top(int sel, int n, int shown)
{
    int top = sel < shown ? 0 : sel - shown + 1;
    return top + shown > n ? (n > shown ? n - shown : 0) : top;
}

static void draw_system(void)
{
    int top = list_top(ui.row, NROWS, ROWS_SHOWN), r;
    for (r = top; r < NROWS && r < top + ROWS_SHOWN; r++) {
        char v[24];
        row_text(r, v);
        list_row(r - top, r == ui.row, ROW_N[r], v, C_HI);
    }
    if (ui.row == R_CORES && (!cfg.one_core) != plat_cpu2_on())
        text_c(120, 2 + ROWS_SHOWN * ROW_H, &FONT_XS, "CHANGES AT THE NEXT POWER-ON", C_AMB);
}

static const char *const VOICE_N[EV_N] = {"DRONE", "LINE", "VOX", "PULSE", "TEXTURE"};
static void draw_voices(void)
{
    int v;
    for (v = 0; v < EV_N; v++) {
        int y = 6 + v * 26, sel = v == ui.vsel, lv = cfg.voice[v][VF_LEVEL];
        char b[8];
        if (sel)
            cv_rect(0, y - 2, 240, 24, dim(C_AMB, 3));
        cv_text(10, vc(&FONT_S, y, 20), &FONT_S, VOICE_N[v], sel ? C_HI : C_GRAY);
        cv_rect(100, y + 8, 100, 4, C_LINE);
        cv_rect(100, y + 8, lv * 100 / 127, 4, sel ? C_AMB : C_GRAY);
        cv_rect(100 + 100 * 100 / 127, y + 5, 1, 10, C_DIM);   /* 100: as made */
        put_i(b, lv);
        text_r(232, vc(&FONT_S, y, 20), &FONT_S, b, sel ? C_AMB : C_HI);
    }
}

static void draw_scene(void)
{
    const jrun_t *r = engine_jr();
    const scene_t *s = scene_sel();
    char b[40];
    int top, f;
    put_s(put_i(put_s(put_i(put_s(b, "SCENE "), ui.ssel + 1), " OF "), eng_jr.nscenes),
          ui.ssel == r->cur ? "  (PLAYING)" : "");
    cv_text(10, 2, &FONT_XS, b, ui.ssel == r->cur ? C_AMB : C_GRAY);
    top = list_top(ui.srow, NSF, 5);
    for (f = top; f < NSF && f < top + 5; f++) {
        int y = 16 + (f - top) * ROW_H, sel = f == ui.srow;
        sf_text(s, f, b);
        if (sel)
            cv_rect(0, y, 240, ROW_H - 2, dim(C_AMB, 3));
        cv_text(10, vc(&FONT_S, y, ROW_H - 2), &FONT_S, SF_N[f], sel ? C_HI : C_GRAY);
        text_r(230, vc(&FONT_S, y, ROW_H - 2), &FONT_S, b, sel ? C_AMB : C_HI);
    }
}

static void draw_loop(void)
{
    const jrun_t *r = engine_jr();
    const loop_t *l = &eng_jr.loop[ui.lsel];
    const loop_t *m = l->on ? l : &r->g[ui.lsel].m;
    char b[32];
    int i, len = m->len ? m->len : 16, div = m->div ? m->div : 1;
    int play = eng_clock.playing && eng_steps ? (int)(((eng_steps - 1u) / (uint32_t)div) % (uint32_t)len) : -1;
    put_s(put_s(b, LOOP_N[ui.lsel]), (ui.lsel == LP_VOX && r->rec) ? ": RECORDING" : l->on ? ": CAPTURED" : ": GENERATOR");
    cv_text(10, 2, &FONT_XS, b, (ui.lsel == LP_VOX && r->rec) ? C_RED : l->on ? C_AMB : C_GRAY);
    put_s(put_i(put_s(b, "MUTATE "), l->mutate), "");
    text_r(230, 2, &FONT_XS, b, C_DIM);
    for (i = 0; i < JR_STEPS; i++) {
        int x = 8 + i * 14, st = i < len ? m->step[i] : 0, cur = l->on && i == ui.lstep;
        uint16_t c = i >= len ? C_LINE : l->on ? C_AMB : C_GRAY;
        if (cur)
            cv_rect(x - 2, 16, 13, 116, dim(C_AMB, 3));
        if (i == play)
            cv_rect(x, 128, 9, 2, C_WHITE);
        if (ui.lsel == LP_PULSE) {
            dot(x + 4, 50, (st & PS_KICK) ? 4 : 1, (st & PS_KICK) ? c : C_DIM);
            dot(x + 4, 96, (st & PS_TICK) ? 2 : 1, (st & PS_TICK) ? c : C_DIM);
            continue;
        }
        if (st & ST_ON) {
            int d = clamp_i(ST_DEG(st), -10, 18), y = 110 - (d + 10) * 3;
            cv_rect(x, y, 9, 5, c);
            if (st & ST_TIE)
                cv_rect(x - 5, y + 2, 5, 1, c);
        } else {
            cv_rect(x + 3, 112, 3, 1, C_DIM);
        }
    }
    if (ui.lsel == LP_PULSE) {
        cv_text(10, 62, &FONT_XS, "KICK", C_DIM);
        cv_text(10, 104, &FONT_XS, "TICK", C_DIM);
    }
}

static void draw_fx(void)
{
    int row, k;
    for (row = 0; row < 2; row++) {
        int y = 4 + row * 66, sel = row == ui.fxrow;
        if (sel)
            cv_rect(0, y - 2, 240, 62, dim(C_AMB, 3));
        cv_text(10, y + 2, &FONT_S, row ? "DELAY" : "REVERB", sel ? C_HI : C_GRAY);
        for (k = 0; k < 4; k++) {
            char b[12];
            int i = row * 4 + k, cx = 30 + k * 60;
            fx_text(i, b);
            text_c(cx, y + 26, &FONT_XS, FX_N[i], C_DIM);
            text_c(cx, y + 40, &FONT_S, b, sel ? C_AMB : C_HI);
        }
    }
}

static void draw_journey(void)
{
    int top = list_top(ui.jrow, JROWS, ROWS_SHOWN), r;
    for (r = top; r < JROWS && r < top + ROWS_SHOWN; r++) {
        char n[24];
        int playing;
        jrow_name(r, n);
        playing = r < JR_PRESETS ? same(eng_jr.name, jr_preset_name(r))
                                 : journey_slot_used(r - JR_PRESETS) && same(eng_jr.name, journey_slot_name(r - JR_PRESETS));
        list_row(r - top, r == ui.jrow, n, playing ? "PLAYING" : r < JR_PRESETS ? "PRESET" : "", playing ? C_AMB : C_DIM);
    }
}

/* ------------------------------------------------------------------ BENCH --- */
static bench_t bench;
static uint32_t bench_serial;
static void draw_bench(void)
{
    char b[32];
    int k;
    if (engine_load_testing())                    /* how the second ran: the load test, the cores, the drone's */
        cv_text(6, 1, &FONT_XS, "LOAD TEST", C_AMB);
    else
        put_s(put_s(b, bench.two_cores ? "2 CORES, " : "1 CORE, "), bench.drone_core == DRONE_SPLIT ? "DRONE SPLIT" :
              bench.drone_core == DRONE_CORE2 ? "DRONE 2ND" : "DRONE 1ST"),
        cv_text(6, 1, &FONT_XS, b, C_GRAY);
    text_r(146, 1, &FONT_XS, "AVG %", C_DIM);
    text_r(210, 1, &FONT_XS, "PEAK %", C_DIM);
    cv_rect(6, 12 + ST_CORE1 * 9, 228, 1, C_LINE);   /* the stages above, each core's whole below */
    for (k = 0; k < ST_N; k++) {
        int total = k >= ST_CORE1, y = 11 + k * 9 + (total ? 3 : 0);
        uint16_t c = total ? C_HI : C_GRAY;
        cv_text(10, y, &FONT_XS, engine_stage_name(k), c);
        if (!bench_serial || (k == ST_CORE2 && !bench.two_cores)) {
            text_r(146, y, &FONT_XS, "-", C_DIM);
            text_r(210, y, &FONT_XS, "-", C_DIM);
            continue;
        }
        put_d1(b, bench.avg[k]);
        text_r(146, y, &FONT_XS, b, c);
        put_d1(b, bench.peak[k]);
        text_r(210, y, &FONT_XS, b, bench.peak[k] >= 900 ? C_RED : bench.peak[k] >= 700 ? C_AMB : c);
    }
}

static void draw_main(void)
{
    cv_begin(240, MAIN_H, C_BG);
    switch (ui.view) {
    case V_HOME: draw_home(); break;
    case V_VOICES: draw_voices(); break;
    case V_SCENE: draw_scene(); break;
    case V_LOOP: draw_loop(); break;
    case V_FX: draw_fx(); break;
    case V_JOURNEY: draw_journey(); break;
    case V_SYSTEM: draw_system(); break;
    default: draw_bench(); break;
    }
    cv_commit_rect(1, 0, MAIN_Y);
}

/* ------------------------------------------------------------ knob strip --- */
static void cell(int i, const char *name, const char *val, uint16_t c)
{
    int cx = i * 60 + 30;
    if (i)
        cv_rect(i * 60, 24, 1, 28, C_LINE);
    text_c(cx, 20, &FONT_XS, name, C_GRAY);
    text_c(cx, 38, &FONT_S, val, c);
}
static void hint(const char *l, const char *r)
{
    cv_text(6, 64, &FONT_XS, l, C_DIM);
    if (r)
        text_r(234, 64, &FONT_XS, r, C_DIM);
}
static void draw_knobs(void)
{
    char a[16], b[16], c[16], d[16];
    int k;
    cv_begin(240, KNOB_H, C_BG);
    cv_rect(0, 0, 240, 1, C_LINE);
    switch (ui.view) {
    case V_HOME:
        for (k = 0; k < NMACROS; k++) {
            if (k == MAC_DRIFT && eng_jr.macro[k] < 2)
                put_s(a, "STILL");
            else if (k == MAC_TONE)
                put_sgn(a, eng_jr.macro[k] - 64);
            else
                put_i(a, eng_jr.macro[k]);
            cell(k, MACRO_N[k], a, C_HI);
        }
        put_sgn(put_s(a, "OCTAVE "), ui.oct);
        hint(eng_jr.mode == MODE_PLAY ? "WHITE KEYS: VOX" : eng_jr.mode == MODE_GUIDE ? "WHITE KEYS: GUIDE"
                                                                                       : "WHITE KEYS: SCENES",
             a);
        break;
    case V_VOICES:
        put_i(a, cfg.voice[ui.vsel][VF_LEVEL]);
        put_sgn(b, cfg.voice[ui.vsel][VF_TONE] - 64);
        put_i(c, cfg.voice[ui.vsel][VF_CHAR]);
        put_sgn(d, cfg.voice[ui.vsel][VF_SEND] - 64);
        cell(0, "LEVEL", a, C_HI);
        cell(1, "TONE", b, C_HI);
        cell(2, VCHAR_N[ui.vsel], c, C_HI);
        cell(3, "SPACE", d, C_HI);
        hint("SELECT: VOICE", VOICE_N[ui.vsel]);
        break;
    case V_SCENE:
        put_i(put_s(put_i(a, ui.ssel + 1), "/"), eng_jr.nscenes);
        cell(0, "SCENE", a, C_HI);
        sf_text(scene_sel(), ui.srow, b);
        if (text_w(&FONT_S, b) > 110)
            b[8] = 0;
        cv_rect(60, 24, 1, 28, C_LINE);
        text_c(120, 20, &FONT_XS, SF_N[ui.srow], C_GRAY);
        text_c(120, 38, &FONT_S, b, C_AMB);
        cell(3, "SEL", "STORE", C_DIM);
        hint("SELECT: FIELD   KNOBS 2-4: CHANGE", 0);
        break;
    case V_LOOP: {
        const loop_t *l = &eng_jr.loop[ui.lsel];
        const int st = l->step[ui.lstep % JR_STEPS];
        put_i(a, ui.lstep + 1);
        cell(0, "STEP", a, l->on ? C_HI : C_DIM);
        if (ui.lsel == LP_PULSE) {
            cell(1, "KICK", st & PS_KICK ? "ON" : "OFF", l->on ? C_HI : C_DIM);
            cell(2, "TICK", st & PS_TICK ? "ON" : "OFF", l->on ? C_HI : C_DIM);
        } else {
            if (st & ST_ON)
                put_sgn(b, ST_DEG(st));
            else
                put_s(b, "-");
            cell(1, "NOTE", b, l->on ? C_HI : C_DIM);
            cell(2, "STATE", !(st & ST_ON) ? "OFF" : (st & ST_TIE) ? "TIE" : "ON", l->on ? C_HI : C_DIM);
        }
        put_i(d, l->mutate);
        cell(3, "MUTATE", d, C_HI);
        hint("SELECT: VOICE", l->on ? "SEL: LET GO" : ui.lsel == LP_VOX && eng_jr.mode == MODE_PLAY ? "SEL: RECORD" : "SEL: CAPTURE");
        break;
    }
    case V_FX:
        for (k = 0; k < 4; k++) {
            fx_text(ui.fxrow * 4 + k, a);
            cell(k, FX_N[ui.fxrow * 4 + k], a, C_HI);
        }
        hint("SELECT: REVERB, DELAY", 0);
        break;
    case V_JOURNEY:
        cv_text(6, 20, &FONT_XS, "PLAYING", C_GRAY);
        cv_text(6, 34, &FONT_S, eng_jr.name, C_AMB);
        hint("SEL: OPEN", "SAVE: INTO A USER SLOT");
        break;
    case V_SYSTEM:
        text_c(120, 30, &FONT_XS, "SELECT: ROW    KNOB OR SEL: CHANGE", C_DIM);
        text_c(120, 46, &FONT_XS, "SAVE: SAVE NOW", C_DIM);
        break;
    default: {
        static uint32_t blk_peak, blk_t;
        if (plat_ms() - blk_t >= 1000u) {         /* the platform's whole half buffer, its worst each second */
            blk_t = plat_ms();
            blk_peak = plat_cpu_peak_pct();
        }
        put_s(put_i(put_s(put_i(a, (int)plat_cpu_pct()), "/"), (int)blk_peak), "");
        cell(0, "LOAD/PEAK", a, blk_peak >= 90u ? C_RED : C_HI);
        put_i(b, (int)plat_xruns());
        cell(1, "XRUNS", b, plat_xruns() ? C_RED : C_HI);
        put_i(c, (int)eng_guard_count);
        cell(2, "GUARD", c, engine_guard_active() ? C_AMB : C_HI);
        put_i(d, (int)eng_out_clips);
        cell(3, "CLIPS", d, eng_out_clips ? C_RED : C_HI);
        {
            uint32_t mhz = plat_cpu_mhz10();
            char *q = put_s(a, "CPU ");
            if (mhz)
                put_s(put_i(q, (int)(mhz / 10u)), " MHZ");
            else
                put_s(q, "? MHZ");
            hint(a, engine_load_testing() ? "SEL: STOP LOAD TEST" : "SEL: LOAD TEST");
        }
        break;
    }
    }
    cv_commit(2, 0, KNOB_Y);
}

/* ------------------------------------------------------------------- lights --- */
static void leds(void)
{
    static const uint8_t VIEW_BTN[NVIEWS] = {B_HOME, B_ENV, B_EDIT, B_SEQ, B_FX, B_LFO, B_GLO, B_GLO};
    const jrun_t *r = engine_jr();
    uint32_t btn = 1u << VIEW_BTN[ui.view], keys = 0, kdim = 0;
    int i;
    if (eng_clock.playing)
        btn |= 1u << B_PLAY;
    if (eng_jr.mode != MODE_PLAY)
        btn |= 1u << B_ARP;
    if (loops_on() || (r->rec && ((ui.frame >> 4) & 1u)))
        btn |= 1u << B_REC;                       /* (blinking: recording, or waiting for the bar) */
    kdim |= (1u << BLACK_KEY[ENG_DRONES]) | (1u << BLACK_KEY[ENG_DRONES + 1]);   /* PULSE's keys, dim */
    for (i = 0; i < ENG_DRONES; i++) {          /* the drone keys: lit while the voice sounds, dim else */
        if (engine_drone_on(i))
            keys |= 1u << BLACK_KEY[i];
        else
            kdim |= 1u << BLACK_KEY[i];
    }
    if (eng_jr.mode == MODE_DRIFT)               /* the scenes: dim, the one playing lit, the next blinking */
        for (i = 0; i < eng_jr.nscenes && i < 16; i++) {
            if (i == r->cur || (r->travelling && i == r->dest && ((ui.frame >> 4) & 1u)))
                keys |= 1u << WHITE_KEY[i];
            else
                kdim |= 1u << WHITE_KEY[i];
        }
    keys |= ui.keys;
    plat_glow(1);
    plat_keys_dim(kdim);
    plat_play_red(0);
    plat_leds(btn, keys);
}

/* --------------------------------------------------------------- the reader --- */
/* WHERE: the screen, said whole */
static void speak_where(void)
{
    char t[ANN_MAX + 1], *q = t, v[24];
    const jrun_t *r = engine_jr();
    int k;
    q = put_s(put_s(q, VIEW_N[ui.view]), ". ");
    switch (ui.view) {
    case V_HOME:
        q = put_s(q, eng_clock.playing ? "PLAYING. " : "STOPPED. ");
        q = put_s(put_i(put_s(q, "TEMPO "), cfg.bpm_x10 / 10), ". ");
        q = put_s(put_s(q, eng_jr.name), ", SCENE ");
        scene_line(v, r->cur);
        q = put_s(put_s(q, v), ". ");
        if (r->playing)
            q = put_s(put_s(q, jr_moon_name(r->moon)), ". ");
        q = put_s(put_s(put_s(q, "MODE "), jr_mode_name(eng_jr.mode)), ". DRONES ON:");
        for (k = 0; k < ENG_DRONES; k++)
            if (engine_drone_on(k))
                q = put_i(put_s(q, " "), k + 1);
        break;
    case V_SYSTEM:
        row_text(ui.row, v);
        q = put_s(put_s(put_s(q, ROW_N[ui.row]), " "), v);
        break;
    case V_VOICES:
        q = put_i(put_s(put_s(q, VOICE_N[ui.vsel]), " LEVEL "), cfg.voice[ui.vsel][VF_LEVEL]);
        break;
    case V_SCENE:
        sf_text(scene_sel(), ui.srow, v);
        q = put_s(put_s(put_s(put_s(put_i(put_s(q, "SCENE "), ui.ssel + 1), ". "), SF_N[ui.srow]), " "), v);
        break;
    case V_LOOP:
        q = put_s(put_s(q, LOOP_N[ui.lsel]), eng_jr.loop[ui.lsel].on ? " CAPTURED" : " GENERATOR");
        break;
    case V_FX:
        q = put_s(q, ui.fxrow ? "DELAY" : "REVERB");
        break;
    case V_JOURNEY:
        jrow_name(ui.jrow, v);
        q = put_s(put_s(put_s(q, v), ". PLAYING "), eng_jr.name);
        break;
    default:
        for (k = ST_DRN1; k < ST_N && q < t + ANN_MAX - 32; k++)
            q = put_s(put_d1(put_s(put_s(put_s(q, engine_stage_name(k)), " PEAK "), ""), bench.peak[k]), ". ");
        break;
    }
    ann_say(t, 0, ANN_NOW);
}

/* ===================================================================== frame === */
void ui_init(void)
{
    int i;
    for (i = 0; i < NKEYS; i++) {
        if (KEY_BLACK[i] >= 0)
            BLACK_KEY[KEY_BLACK[i]] = (uint8_t)i;
        if (KEY_WHITE[i] >= 0)
            WHITE_KEY[KEY_WHITE[i]] = (uint8_t)i;
    }
    ui.view = V_HOME;
    ui.touch_t = plat_ms();
    for (i = 0; i < 3; i++)
        blit_hash[i] = 0;
}

void ui_start(void)
{
    journey_slots_load();
    if (cfg.autoplay && !eng_clock.playing)
        tclock_play(&eng_clock);
}

void ui_frame(void)
{
    input();
    for (; ui.stars; ui.stars--)
        ec_star_new();
    autosave();
    {
        uint32_t s = engine_bench(&bench);
        if (s)
            bench_serial = s;
    }
    draw_header();
    draw_main();
    draw_knobs();
    leds();
    if (ann_where())
        speak_where();
    ann_tick(plat_ms());
    ui.frame++;
}

void ui_input_only(void) { input(); }
void ui_say(const char *a, const char *b) { say(a, b); }
void ui_warn(const char *a, const char *b) { warn(a, b); }
void ui_redraw(void)
{
    int i;
    lcd_fill(0, 0, 240, 240, C_BG);
    for (i = 0; i < 3; i++)
        blit_hash[i] = 0;
}
