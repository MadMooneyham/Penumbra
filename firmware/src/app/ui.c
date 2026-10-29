/* SPDX-License-Identifier: GPL-3.0-only */
/* Penumbra's user interface: HOME, where you play, and a page one press away for everything else.
 *
 *   HOME     the corona: the Lunar Eclipse Forge mark on the eclipsed moon, its corona pulsing with
 *            the music, a bead on the rim for each drone voice, a star for each note you play. Under
 *            it the scene, the looper, FREEZE. KNOBS 1-4: the macros DRONE (its filter and body), VOX
 *            (its brightness), SPACE, TONE.
 *   SHAPE    a voice's sound, four knobs each, ALGO its level (ENV again, or SELECT: the next page):
 *            DRONE filter, body, swarm, wave; VOX (the sound playing) bright, attack, release, shimmer;
 *            VOX 2 wave, filter, reso, glide; VOX 3 detune, B octave, B mix, env; PULSE drop, decay,
 *            tick, space; TEXTURE colour, sweep, rate, width. Saved with the journey
 *   SCENE    the journey's scenes, field by field (SELECT: the field, KNOB 1: the scene, 2-4: change
 *            it); SEL stores the moment as a new scene
 *   FX       the reverb and the delay (SELECT: which)
 *   JOURNEY  the 6 presets and 8 user slots: SEL opens one, SAVE stores the one playing in a slot
 *   SYSTEM   the settings, a list: SELECT moves, any knob (or SEL) changes the row
 *   BENCH    the benchmark: each stage of the render, average and worst block; SEL: the load test
 *
 * Buttons, one job each (they act when let go; held, they say what they do instead):
 *   HOME home, ENV SHAPE, EDIT SCENE, FX FX, LFO JOURNEY, GLO SYSTEM then BENCH, SEQ the clock (starts
 *   and stops, with a fade), SAVE saves now, SEL the page's action, OCT- / OCT+ the white keys' octave.
 *   The looper: REC records (again: it plays; again: a layer over it), PLAY plays or stops it (held:
 *   the last layer undone), REC with PLAY clears it. ARP: FREEZE, the notes held keep sounding.
 * Keys: white VOX in the scene's key; black 1-5 latch the drone voices (when let go; a SWELL voice:
 * played when pressed), 6 and 7 hit PULSE, 8-11 pick VOX's sound (held: shape it). ALGO: the tempo
 * (SHAPE: the voice's level). PRESET: the next scene, or the one before (on JOURNEY: the list).
 * SELECT on HOME: MORPH, the way to the next scene (or the one before) by hand. Every page but BENCH
 * goes home after 30 s idle.
 *
 * Holds: the knobs play something else while one is held, on any page.
 *   a drone key   that voice's LEVEL, TONE, DETUNE, OCTAVE, ALGO HELD or SWELL; it stays (the key
 *                 does not latch)
 *   FX            THROWS: SHIMMER, FEEDBACK, FREEZE, DIP; let go, they drift back
 *   a white key   BENDS VOX: BRIGHT, VIBRATO, SHIMMER, RELEASE; let go, they drift back
 * SAVE while holding FX or a white key keeps what the knobs did: the bends go into VOX's sound, the
 * throws stay where they are until FX is held again.
 *
 * The screen is three bands (header, main, knob strip), each drawn into Felucca's canvas and
 * blitted only where its pixels changed: SPI time is the scarce thing. */
#include "penumbra.h"
#include "../dsp/fastmath.h"

/* ================================================================ model === */
static const int8_t KEY_WHITE[NKEYS] = {0, -1, 1, -1, 2, -1, 3, 4, -1, 5, -1, 6, 7, -1, 8, -1, 9, -1, 10, 11, -1, 12, -1, 13, 14, -1, 15};
static const int8_t KEY_BLACK[NKEYS] = {-1, 0, -1, 1, -1, 2, -1, -1, 3, -1, 4, -1, -1, 5, -1, 6, -1, 7, -1, -1, 8, -1, 9, -1, -1, 10, -1};
static uint8_t BLACK_KEY[11], WHITE_KEY[16];

enum { V_HOME, V_SHAPE, V_SCENE, V_FX, V_JOURNEY, V_SYSTEM, V_BENCH, NVIEWS };
static const char *const VIEW_N[NVIEWS] = {"HOME", "SHAPE", "SCENE", "FX", "JOURNEY", "SYSTEM", "BENCH"};

#define HELP_MS 600u                       /* held this long, a button says what it does instead */
#define HELP_HOLD_MS 1500u                 /* (FX, a hold too: later, and not once the knobs have moved) */
#define IDLE_HOME_MS 30000u
#define BACK_S 2.0f                        /* a throw or a bend let go drifts back over this, from all the way */

enum { L_PAGE, L_DRONE, L_VSOUND, L_THROW, L_BEND };   /* what the knobs play: the page's, or a hold's */

typedef struct {
    uint8_t view;
    int8_t oct;                        /* the white keys' octave, -2..+2 */
    uint8_t row;                       /* SYSTEM's row */
    uint8_t vsel, ssel, srow, fxrow, jrow;
    int8_t sdest;                      /* the scene PRESET sent it to (-1: none) */
    uint32_t btn, keys;                /* held, as last seen */
    uint32_t press_t[NB], helped;      /* when each button went down; held long enough to say its job */
    char msg[28];
    uint32_t msg_until;
    uint32_t frame;
    uint8_t dirty;                     /* the settings or the journey changed since they were saved */
    uint32_t act_t;                    /* the last change (a save waits for quiet) */
    uint32_t touch_t;                  /* the last touch (pages go home when idle) */
    uint8_t stars;                     /* notes played since the last frame: a star each */
    /* the holds */
    uint8_t dheld, dturned;            /* drone keys held (bit v); a knob moved while they were */
    uint8_t vheld;                     /* VOX's sound keys held (bit s) */
    uint16_t wheld;                    /* white keys held (bit w) */
    uint32_t used;                     /* buttons held as a hold this press: they do not act when let go */
    float thr[4], bend[4];             /* THROWS 0..127 (PF_SHIMMER..), BENDS -64..64 (PF_BRIGHT..) */
    int16_t sent[PF_N];                /* what the engine has */
    uint8_t thr_kept, thr_back, bend_back;
    float pitch;                       /* PITCH (ALGO, a white key held): -127 .. 127 of the bend range */
    uint8_t pitch_kept, pitch_back;    /* SAVE: it stays until the next note */
    uint8_t sleep, sleep_step;         /* SLEEP's choice; how far its letting go has got */
    uint32_t sleep_at;                 /* when it is all gone */
    uint32_t lp_edits;                 /* the loop's, as last seen (a change: it is saved) */
    uint32_t perf_t;                   /* the last drift */
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
enum { D_SET = 1, D_JR = 2, D_LOOP = 4 };        /* what is not saved: the settings, the journey, the loop */
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
enum { R_CLKOUT, R_CLKIN, R_MIDICH, R_CORES, R_DRNCORE, R_OUTLOUD, R_USBFULL, R_AUTOPLAY, R_BEND, R_SLEEP, R_ABOUT,
       NROWS };
static const char *const ROW_N[NROWS] = {"MIDI CLOCK OUT", "FOLLOW CLOCK IN", "VOX MIDI CH", "2ND CORE",
                                         "DRONE CORE", "OUTPUT LEVEL", "USB AUDIO", "PLAY AT POWER-ON",
                                         "BEND RANGE", "SLEEP", "ABOUT"};
/* SLEEP: in so many minutes it lets everything go (this session's: not saved) */
#define SLEEPS 5
static const uint8_t SLEEP_MIN[SLEEPS] = {0, 20, 40, 60, 90};
static const char *const ONOFF_N[] = {"OFF", "ON"};
static const char *const CORE_N[] = {"1ST", "2ND", "SPLIT"};
static const char *const LOUD_N[] = {"NORMAL", "LOUD"};
static const char *const USB_N[] = {"MASTER", "FULL"};

static int row_n(int r)                           /* choices; 0: not a setting */
{
    return r == R_MIDICH ? 17 : r == R_DRNCORE ? 3 : r == R_BEND ? BEND_RANGES : r == R_SLEEP ? SLEEPS : r == R_ABOUT ? 0 : 2;
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
    case R_BEND: return cfg.bend % BEND_RANGES;
    case R_SLEEP: return ui.sleep;
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
    case R_BEND: put_s(put_i(b, BEND_SEMIS[v]), " SEMITONES"); break;
    case R_SLEEP:
        if (v)
            put_s(put_i(b, SLEEP_MIN[v]), " MIN");
        else
            put_s(b, "OFF");
        break;
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
    case R_BEND: cfg.bend = (uint8_t)v; break;
    case R_SLEEP:                                 /* (now: from now) */
        ui.sleep = (uint8_t)v;
        ui.sleep_at = plat_ms() + (uint32_t)SLEEP_MIN[v] * 60000u;
        ui.sleep_step = 0;
        break;
    default: break;
    }
    if (r != R_SLEEP) {
        settings_apply();
        changed();
    }
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
    int what = ui.dirty ? ui.dirty : D_SET | D_JR | D_LOOP, bad = 0;
    if (what & D_LOOP) {                          /* (recording: it waits for the next) */
        int r = loop_save_work();
        if (r == 0)
            ui.dirty &= (uint8_t)~D_LOOP;
        else if (r < 0)
            bad = 1;
    }
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
static const char *const MACRO_N[NMACROS] = {"DRONE", "VOX", "SPACE", "TONE"};

/* SCENE's fields */
enum { SF_KEY, SF_SCALE, SF_PEDAL, SF_VOICING, SF_BRIGHT, SF_REG, SF_SPACE, SF_WIND, SF_COLOUR, NSF };
static const char *const SF_N[NSF] = {"KEY", "SCALE", "DRONE ROOT", "VOICING", "VOX BRIGHTNESS", "VOX REGISTER",
                                      "SPACE", "WIND", "WIND COLOUR"};
static uint8_t *sf_ptr(scene_t *s, int f)
{
    switch (f) {
    case SF_KEY: return &s->root;
    case SF_SCALE: return &s->scale;
    case SF_PEDAL: return &s->pedal;
    case SF_VOICING: return &s->voicing;
    case SF_BRIGHT: return &s->bright;
    case SF_REG: return &s->reg;
    case SF_SPACE: return &s->space;
    case SF_WIND: return &s->tex;
    default: return &s->colour;
    }
}
static void sf_text(const scene_t *s, int f, char *b)
{
    switch (f) {
    case SF_KEY: put_s(b, jr_root_name(s->root)); break;
    case SF_SCALE: put_s(b, jr_scale_name(s->scale)); break;
    case SF_PEDAL: put_s(b, s->pedal ? jr_root_name(s->pedal - 1) : "THE KEY"); break;
    case SF_VOICING: put_s(b, jr_voicing_name(s->voicing)); break;
    case SF_REG: put_i(b, s->reg); break;
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
    case SF_PEDAL: *p = (uint8_t)wrap_i(*p + e, 13); break;   /* (a root not in the key: the key's) */
    case SF_VOICING: *p = (uint8_t)wrap_i(*p + e, NVOICINGS); break;
    case SF_REG: *p = (uint8_t)clamp_i(*p + e, 2, 6); break;
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

/* ------------------------------------------------------------ the looper --- */
/* (what it says is what it will be: the engine takes the command at its next block) */
static void rec_button(void)
{
    const looper_t *l = engine_looper_state();
    if (!eng_clock.playing)                       /* it plays on the clock: the clock starts */
        tclock_play(&eng_clock);
    engine_looper(LOOPER_REC);
    say(l->state == LS_EMPTY ? "LOOP: RECORDING" : l->state == LS_REC ? "LOOP: PLAYING" :
        l->state == LS_DUB ? "LAYER KEPT" : l->layers >= LP_LAYERS ? "LAYERS FULL" : "LOOP: OVERDUB", 0);
}
static void loop_play(void)
{
    const looper_t *l = engine_looper_state();
    if (l->state == LS_EMPTY) {
        warn("NO LOOP: REC FIRST", 0);
        return;
    }
    if (l->state == LS_STOP && !eng_clock.playing)
        tclock_play(&eng_clock);
    engine_looper(LOOPER_PLAY);
    say(l->state == LS_PLAY ? "LOOP STOPPED" : "LOOP: PLAYING", 0);
}
static void loop_text(char *b)                     /* HOME's: what it is doing, its layers */
{
    const looper_t *l = engine_looper_state();
    static const char *const LS_N[] = {"LOOP -", "REC", "LOOP", "DUB", "LOOP STOPPED"};
    char *q = put_s(b, LS_N[l->state % 5u]);
    if (l->state != LS_EMPTY && l->state != LS_REC)
        q = put_i(put_s(q, " x"), l->layers);
    if (l->state != LS_EMPTY && l->speed != LP_AS_PLAYED)
        put_s(q, l->speed == LP_HALF ? " HALF" : " DOUBLE");
}

static void scene_step(int e)                      /* PRESET: the next scene, or the one before */
{
    const jrun_t *r = engine_jr();
    int n = eng_jr.nscenes ? eng_jr.nscenes : 1, from, to;
    char b[40];
    from = ui.sdest >= 0 && (r->travelling || r->cur != ui.sdest) ? ui.sdest : r->cur;   /* (on the way: from there) */
    to = wrap_i(from + (e > 0 ? 1 : -1), n);
    ui.sdest = (int8_t)to;
    engine_travel(to);
    put_s(put_s(put_s(put_i(put_s(b, "TO SCENE "), to + 1), "  "), jr_root_name(eng_jr.scene[to].root)), " ");
    say(b, jr_scale_name(eng_jr.scene[to].scale));
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

/* ================================================================ the sound === */
static const char *const VOICE_N[EV_N] = {"DRONE", "VOX", "PULSE", "TEXTURE"};
/* SHAPE's pages: a voice each, VOX three, of the sound of its four playing (ENV, SELECT: the next) */
enum { PG_DRONE, PG_VOX, PG_VOX2, PG_VOX3, PG_PULSE, PG_TEX, NPG };
typedef struct {
    uint8_t voice;
    const char *name;
    const char *k[4];
    uint8_t f[4];                                 /* the fields: SH_* (VOX: VS_*) */
    uint8_t sgn;                                  /* the knobs that are an offset from as made (bit k): -64 .. +63 */
} spage_t;
static const spage_t PAGE[NPG] = {
    {EV_DRONE, "DRONE", {"FILTER", "BODY", "SWARM", "WAVE"}, {SH_K1, SH_K2, SH_K3, SH_K4}, 0},
    {EV_VOX, "VOX", {"BRIGHT", "ATTACK", "RELEASE", "SHIMMER"}, {VS_BRIGHT, VS_ATTACK, VS_RELEASE, VS_SHIMMER}, 0x9},
    {EV_VOX, "VOX 2", {"WAVE", "FILTER", "RESO", "GLIDE"}, {VS_WAVE, VS_FILTER, VS_RESO, VS_GLIDE}, 0},
    {EV_VOX, "VOX 3", {"DETUNE", "B OCT", "B MIX", "ENV"}, {VS_DETUNE, VS_BOCT, VS_BMIX, VS_ENV}, 0},
    {EV_PULSE, "PULSE", {"DROP", "DECAY", "TICK", "SPACE"}, {SH_K1, SH_K2, SH_K3, SH_K4}, 0x8},
    {EV_TEX, "TEXTURE", {"COLOUR", "SWEEP", "RATE", "WIDTH"}, {SH_K1, SH_K2, SH_K3, SH_K4}, 0x1}};
static uint8_t *vsound(void) { return eng_jr.vsound[eng_jr.vcur % JR_VSOUNDS]; }
static uint8_t *pg_ptr(int pg, int k)             /* k: a knob, or -1 the level */
{
    const spage_t *p = &PAGE[pg % NPG];
    if (p->voice == EV_VOX)
        return &vsound()[k < 0 ? VS_LEVEL : p->f[k]];
    return &eng_jr.shape[p->voice][k < 0 ? SH_LEVEL : p->f[k]];
}
static int pg_octave(int pg, int k) { return k >= 0 && PAGE[pg % NPG].voice == EV_VOX && PAGE[pg % NPG].f[k] == VS_BOCT; }
static void shape_text(int pg, int k, char *b)
{
    int x = *pg_ptr(pg, k);
    if (pg_octave(pg, k))
        put_s(b, x == 0 ? "-1" : x == 2 ? "+1" : "0");
    else if (k >= 0 && (PAGE[pg % NPG].sgn >> k & 1u))
        put_sgn(b, x - 64);
    else
        put_i(b, x);
}
static void shape_turn(int k, int e)
{
    char t[40], b[12];
    uint8_t *p = pg_ptr(ui.vsel, k);
    *p = (uint8_t)(pg_octave(ui.vsel, k) ? clamp_i(*p + (e > 0 ? 1 : -1), 0, 2) : clamp_i(*p + 2 * e, 0, 127));
    changed_jr();
    shape_text(ui.vsel, k, b);
    put_s(put_s(put_s(put_s(put_s(t, PAGE[ui.vsel].name), " "), k < 0 ? "LEVEL" : PAGE[ui.vsel].k[k]), " "), b);
    say(t, 0);
}

/* VOX's four sounds, black keys 8 .. 11: pressed, that one plays (mid-phrase too); held, the knobs
 * shape it (BRIGHT, WAVE, FILTER, RELEASE) */
#define VS_KEY0 (ENG_DRONES + 2)                  /* the first of their keys (black: the drones, PULSE's two) */
static const uint8_t VSH_F[4] = {VS_BRIGHT, VS_WAVE, VS_FILTER, VS_RELEASE};
static const char *const VSH_N[4] = {"BRIGHT", "WAVE", "FILTER", "RELEASE"};
static void vsound_pick(int s)
{
    eng_jr.vcur = (uint8_t)(s % JR_VSOUNDS);
    changed_jr();
    say("VOX ", jr_vsound_name(s));
}

/* ---------------------------------------------------------------- the holds --- */
static int layer(void)
{
    if (ui.dheld)
        return L_DRONE;
    if (ui.vheld)
        return L_VSOUND;
    if (ui.btn >> B_FX & 1u)
        return L_THROW;
    if (ui.wheld)
        return L_BEND;
    return L_PAGE;
}
static int lowest_bit(uint32_t m)
{
    int i;
    for (i = 0; i < 31 && !(m >> i & 1u); i++)
        ;
    return i;
}
static const char *const DVF_N[DV_N] = {"LEVEL", "TONE", "DETUNE", "OCTAVE"};
static const char *const THR_N[4] = {"SHIMMER", "FEEDBACK", "FREEZE", "DIP"};
static const char *const BEND_N[4] = {"BRIGHT", "VIBRATO", "SHIMMER", "RELEASE"};
static int round_i(float x) { return (int)(x + (x < 0.0f ? -0.5f : 0.5f)); }
static void dv_text(int v, int f, char *b)
{
    int x = eng_jr.dvoice[v][f];
    if (f == DV_OCT)
        put_s(b, x == 0 ? "-1" : x == 2 ? "+1" : "0");
    else if (f == DV_DETUNE)
        put_sgn(b, x - 64);
    else
        put_i(b, x);
}
static void drone_knob(int k, int e)              /* the drone voices held, each the same way */
{
    char t[40], b[8];
    int v, first = lowest_bit(ui.dheld);
    for (v = 0; v < ENG_DRONES; v++)
        if (ui.dheld >> v & 1u) {
            uint8_t *p = &eng_jr.dvoice[v][k];
            *p = (uint8_t)(k == DV_OCT ? clamp_i(*p + (e > 0 ? 1 : -1), 0, 2) : clamp_i(*p + 2 * e, 0, 127));
        }
    ui.dturned |= ui.dheld;
    changed_jr();
    dv_text(first, k, b);
    put_s(put_s(put_s(put_s(put_i(put_s(t, "DRONE "), first + 1), " "), DVF_N[k]), " "), b);
    say(t, 0);
}
static void vsound_knob(int k, int e)             /* a VOX sound key held: that sound */
{
    char t[40], b[8];
    uint8_t *p = &vsound()[VSH_F[k]];
    *p = (uint8_t)clamp_i(*p + 2 * e, 0, 127);
    changed_jr();
    if (k == 0)
        put_sgn(b, *p - 64);
    else
        put_i(b, *p);
    put_s(put_s(put_s(put_s(put_s(t, jr_vsound_name(eng_jr.vcur)), " "), VSH_N[k]), " "), b);
    say(t, 0);
}
static void drone_mode(int e)                     /* ALGO, a drone key held: HELD (left) or SWELL, comes and goes (right) */
{
    char t[40];
    int first = lowest_bit(ui.dheld);
    if (e > 0)
        eng_jr.dswell |= ui.dheld;
    else
        eng_jr.dswell &= (uint8_t)~ui.dheld;
    ui.dturned |= ui.dheld;
    changed_jr();
    put_s(put_i(put_s(t, "DRONE "), first + 1), e > 0 ? " SWELLS: COMES AND GOES" : " HELD");
    say(t, 0);
}
static void throw_knob(int k, int e)
{
    char t[40], b[8];
    ui.thr[k] = fm_clampf(ui.thr[k] + 4.0f * (float)e, 0.0f, 127.0f);
    ui.thr_kept = ui.thr_back = 0;               /* (a new throw: all of them drift back at the end, unless kept) */
    ui.used |= 1u << B_FX;
    put_i(b, round_i(ui.thr[k]));
    put_s(put_s(put_s(put_s(t, THR_N[k]), " THROW"), " "), b);
    say(t, 0);
}
static void bend_knob(int k, int e)
{
    char t[40], b[8];
    ui.bend[k] = fm_clampf(ui.bend[k] + 2.0f * (float)e, -64.0f, 64.0f);
    ui.bend_back = 0;
    put_sgn(b, round_i(ui.bend[k]));
    put_s(put_s(put_s(t, BEND_N[k]), " "), b);
    say(t, 0);
}
static void pitch_text(char *b)                   /* "+1.5" semitones */
{
    int c = round_i(ui.pitch * (float)BEND_SEMIS[cfg.bend % BEND_RANGES] * (10.0f / 127.0f)), a = c < 0 ? -c : c;
    put_d1(put_s(b, c < 0 ? "-" : c > 0 ? "+" : ""), (uint32_t)a);
}
static void pitch_knob(int e)                     /* ALGO, a white key held: every VOX note up or down */
{
    char t[24], b[12];
    ui.pitch = fm_clampf(ui.pitch + 8.0f * (float)e, -127.0f, 127.0f);
    ui.pitch_back = ui.pitch_kept = 0;
    pitch_text(b);
    put_s(put_s(t, "PITCH "), b);
    say(t, 0);
}
static void perf_send(void)                       /* the moves to the engine, when they change */
{
    int k;
    if (round_i(ui.pitch) != ui.sent[PF_PITCH])
        engine_perf(PF_PITCH, ui.sent[PF_PITCH] = (int16_t)round_i(ui.pitch));
    for (k = 0; k < 4; k++) {
        int a = round_i(ui.thr[k]), b = round_i(ui.bend[k]);
        if (a != ui.sent[PF_SHIMMER + k])
            engine_perf(PF_SHIMMER + k, ui.sent[PF_SHIMMER + k] = (int16_t)a);
        if (b != ui.sent[PF_BRIGHT + k])
            engine_perf(PF_BRIGHT + k, ui.sent[PF_BRIGHT + k] = (int16_t)b);
    }
}
static void perf_tick(void)                       /* let go: back, over BACK_S from all the way */
{
    uint32_t now = plat_ms();
    float dt = fm_minf((float)(now - ui.perf_t) * 0.001f, 0.1f);
    int k, left;
    ui.perf_t = now;
    if (ui.thr_back) {
        for (k = left = 0; k < 4; k++)
            left |= (ui.thr[k] = fm_maxf(ui.thr[k] - dt * (127.0f / BACK_S), 0.0f)) > 0.0f;
        ui.thr_back = (uint8_t)left;
    }
    if (ui.pitch_back) {
        float s = dt * (127.0f / BACK_S);
        ui.pitch = ui.pitch > s ? ui.pitch - s : ui.pitch < -s ? ui.pitch + s : 0.0f;
        ui.pitch_back = ui.pitch != 0.0f;
    }
    if (ui.bend_back) {
        for (k = left = 0; k < 4; k++) {
            float s = dt * (64.0f / BACK_S), b = ui.bend[k];
            ui.bend[k] = b > s ? b - s : b < -s ? b + s : 0.0f;
            left |= ui.bend[k] != 0.0f;
        }
        ui.bend_back = (uint8_t)left;
    }
    perf_send();
}
static int any_thrown(void) { return ui.thr[0] > 0.5f || ui.thr[1] > 0.5f || ui.thr[2] > 0.5f || ui.thr[3] > 0.5f; }
/* SAVE in a hold keeps it: a note's bends go into VOX's sound (its SHAPE, and its vibrato), the throws
 * stay where they are */
static int keep_hold(void)
{
    int k;
    if (layer() == L_BEND) {
        static const uint8_t TO[4] = {VS_BRIGHT, VS_VIB, VS_SHIMMER, VS_RELEASE};
        uint8_t *sh = vsound();
        for (k = 0; k < 4; k++) {
            sh[TO[k]] = (uint8_t)clamp_i(sh[TO[k]] + round_i(ui.bend[k]), 0, 127);
            ui.bend[k] = 0.0f;
        }
        ui.bend_back = 0;
        if (ui.pitch != 0.0f) {                   /* the pitch: bent until the next note */
            ui.pitch_kept = 1;
            ui.pitch_back = 0;
        }
        perf_send();
        changed_jr();
        say(ui.pitch_kept ? "KEPT, BENT TILL THE NEXT NOTE" : "KEPT IN VOX", 0);
        return 1;
    }
    if (layer() == L_THROW) {
        ui.thr_kept = 1;
        ui.thr_back = 0;
        ui.used |= 1u << B_FX;
        say(any_thrown() ? "THROWS KEPT" : "NOTHING THROWN", 0);
        return 1;
    }
    return 0;
}

/* ------------------------------------------------------------------ SLEEP --- */
/* the last three minutes: the loop stops and what is frozen lets go; then every 30 s a drone voice falls
 * away, the top first, into the reverb; at the end the clock stops (and it all fades) */
#define SLEEP_LAST_MS 180000u
static int sleep_left_ms(void) { return (int)(ui.sleep_at - plat_ms()); }
static void sleep_tick(void)
{
    int left, v;
    if (!ui.sleep)
        return;
    left = sleep_left_ms();
    if (left <= 0) {
        if (eng_clock.playing)
            tclock_stop(&eng_clock);
        ui.sleep = 0;
        say("GOOD NIGHT", 0);
        return;
    }
    if (left > (int)SLEEP_LAST_MS)
        return;
    if (ui.sleep_step == 0) {
        const looper_t *l = engine_looper_state();
        if (l->state == LS_PLAY || l->state == LS_DUB || l->state == LS_REC)
            engine_looper(LOOPER_PLAY);           /* (recording: it closes and plays; the next step stops it) */
        if (engine_frozen())
            engine_freeze();
        say("SLEEP: LETTING GO", 0);
        ui.sleep_step = 1;
        return;
    }
    if (ui.sleep_step == 1 && engine_looper_state()->state == LS_PLAY)
        engine_looper(LOOPER_PLAY);
    if (left <= (int)SLEEP_LAST_MS - 30000 * (int)ui.sleep_step) {   /* a drone voice, every 30 s */
        ui.sleep_step++;
        for (v = ENG_DRONES - 1; v >= 0; v--)
            if (engine_drone_on(v) && !(eng_jr.dswell >> v & 1u)) {
                engine_drone_key(v);
                break;
            }
    }
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
    [B_FX] = "FX. HELD, KNOBS: THROWS", [B_SEL] = "THE PAGE'S ACTION", [B_ENV] = "SHAPE: EACH VOICE",
    [B_LFO] = "JOURNEYS",           [B_EDIT] = "SCENE: KEY, DRONE", [B_GLO] = "SYSTEM, BENCH",
    [B_HOME] = "HOME",              [B_SAVE] = "SAVE. IN A HOLD: KEEP IT", [B_ARP] = "FREEZE THE NOTES HELD",
    [B_SEQ] = "CLOCK: START, STOP", [B_REC] = "LOOP: RECORD, LAYER",
    [B_OCTDN] = "WHITE KEYS DOWN",  [B_OCTUP] = "WHITE KEYS UP"};   /* (PLAY held: the undo) */

static void button(int i)
{
    switch (i) {
    case B_HOME: set_view(V_HOME); break;
    case B_ENV:
        if (ui.view == V_SHAPE) {                 /* again: the next voice */
            ui.vsel = (uint8_t)((ui.vsel + 1) % NPG);
            say(PAGE[ui.vsel].name, 0);
        } else {
            set_view(V_SHAPE);
        }
        break;
    case B_EDIT:
        if (ui.view != V_SCENE)
            ui.ssel = engine_jr()->cur;
        set_view(V_SCENE);
        break;
    case B_SEQ:
        if (eng_clock.playing)
            tclock_stop(&eng_clock);
        else
            tclock_play(&eng_clock);
        say(eng_clock.playing ? "STOP" : "PLAY", 0);
        break;
    case B_FX: set_view(V_FX); break;
    case B_LFO: set_view(V_JOURNEY); break;
    case B_GLO: set_view(ui.view == V_SYSTEM ? V_BENCH : V_SYSTEM); break;
    case B_ARP:
        say(engine_frozen() ? "LET GO" : ui.wheld ? "FROZEN" : "NOTHING HELD", 0);
        engine_freeze();
        break;
    case B_PLAY: loop_play(); break;
    case B_REC: rec_button(); break;
    case B_SAVE:
        if (!keep_hold())
            save_button();
        break;
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
    case V_HOME: {                                /* MORPH: by hand toward the next scene, or the one before */
        const jrun_t *r = engine_jr();
        int n = eng_jr.nscenes ? eng_jr.nscenes : 1;
        if (n < 2 || r->travelling) {
            warn(n < 2 ? "ONE SCENE: NOWHERE TO GO" : "ON THE WAY: WAIT", 0);
            break;
        }
        engine_morph(e);
        if (r->mamt <= 0.0f)                      /* (setting out) */
            say_n("MORPH TO SCENE ", wrap_i(r->cur + (e > 0 ? 1 : -1), n) + 1);
        break;
    }
    case V_SYSTEM:
        ui.row = (uint8_t)clamp_i(ui.row + e, 0, NROWS - 1);
        row_text(ui.row, v);
        put_s(put_s(put_s(t, ROW_N[ui.row]), " "), v);
        ann_say(t, 0, ANN_NOW);
        break;
    case V_SHAPE:
        ui.vsel = (uint8_t)clamp_i(ui.vsel + e, 0, NPG - 1);
        ann_say(PAGE[ui.vsel].name, 0, ANN_NOW);
        break;
    case V_SCENE:
        ui.srow = (uint8_t)clamp_i(ui.srow + e, 0, NSF - 1);
        sf_text(scene_sel(), ui.srow, v);
        put_s(put_s(put_s(t, SF_N[ui.srow]), " "), v);
        ann_say(t, 0, ANN_NOW);
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
        put_sgn(v, *m - 64);
        put_s(put_s(put_s(t, MACRO_N[k]), " "), v);
        say(t, 0);
        break;
    }
    case V_SHAPE: shape_turn(k, e); break;
    case V_SCENE:
        if (k == 0) {
            int n = eng_jr.nscenes ? eng_jr.nscenes : 1;
            ui.ssel = (uint8_t)clamp_i(ui.ssel + e, 0, n - 1);
            say_n("SCENE ", ui.ssel + 1);
        } else {
            sf_turn(ui.srow, e);
        }
        break;
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
                ui.wheld |= (uint16_t)(1u << w);
                if (ui.pitch_kept) {              /* a kept bend: the next note eases it back */
                    ui.pitch_kept = 0;
                    ui.pitch_back = 1;
                }
                engine_white_key(w, 1, ui.oct);
                if (ui.stars < 8u)
                    ui.stars++;
            } else if (bk >= 0 && bk < ENG_DRONES) {
                ui.dheld |= (uint8_t)(1u << bk);  /* (held: it latches when let go, unless the knobs shaped it) */
                if (eng_jr.dswell >> bk & 1u)
                    engine_drone_key(bk);         /* a voice that comes and goes: played, now */
            } else if (bk == ENG_DRONES || bk == ENG_DRONES + 1) {   /* the next two: PULSE's kick and tick */
                engine_pulse(bk - ENG_DRONES, 110);
            } else if (bk >= VS_KEY0 && bk < VS_KEY0 + JR_VSOUNDS) {  /* the last four: VOX's sounds */
                ui.vheld |= (uint8_t)(1u << (bk - VS_KEY0));
                vsound_pick(bk - VS_KEY0);
            }
        }
        if ((ku >> i & 1u) && w >= 0) {
            engine_white_key(w, 0, ui.oct);
            ui.wheld &= (uint16_t)~(1u << w);
            if (!ui.wheld && (ui.bend[0] != 0.0f || ui.bend[1] != 0.0f || ui.bend[2] != 0.0f || ui.bend[3] != 0.0f))
                ui.bend_back = 1;                 /* the note let go: its bends drift back */
            if (!ui.wheld && ui.pitch != 0.0f && !ui.pitch_kept)
                ui.pitch_back = 1;
        } else if ((ku >> i & 1u) && bk >= 0 && bk < ENG_DRONES) {
            if (!(ui.dturned >> bk & 1u) && !(eng_jr.dswell >> bk & 1u))
                engine_drone_key(bk);
            ui.dheld &= (uint8_t)~(1u << bk);
            ui.dturned &= (uint8_t)~(1u << bk);
        } else if ((ku >> i & 1u) && bk >= VS_KEY0 && bk < VS_KEY0 + JR_VSOUNDS) {
            ui.vheld &= (uint8_t)~(1u << (bk - VS_KEY0));
        }
    }
    for (i = 0; i < NB; i++) {                    /* a button acts when let go; held, it says its job */
        uint32_t bit = 1u << i;
        if (down & bit) {
            const uint32_t both = 1u << B_REC | 1u << B_PLAY;
            ui.press_t[i] = now;
            ui.helped &= ~bit;
            ui.used &= ~bit;
            if ((i == B_REC || i == B_PLAY) && (b & both) == both) {   /* REC with PLAY: the loop gone */
                engine_looper(LOOPER_CLEAR);
                ui.used |= both;
                say("LOOP CLEARED", 0);
            }
        } else if ((b & bit) && !(ui.helped & bit) && !(ui.used & bit) && i != B_PLAY &&
                   now - ui.press_t[i] >= (i == B_FX ? HELP_HOLD_MS : HELP_MS)) {
            ui.helped |= bit;
            if (HELP[i])
                say(HELP[i], 0);
        }
        if (up & bit) {
            if (i == B_PLAY && !(ui.used & bit) && now - ui.press_t[i] >= HELP_MS) {
                engine_looper(LOOPER_UNDO);       /* PLAY held (and ALGO not turned): the last layer undone */
                say(engine_looper_state()->layers > 1 ? "LAYER UNDONE" : "LOOP CLEARED", 0);
            } else if (!(ui.helped & bit) && !(ui.used & bit)) {
                button(i);
            }
            if (i == B_FX && (ui.used & bit) && !ui.thr_kept)
                ui.thr_back = 1;                  /* FX let go: the throws drift back */
            ui.used &= ~bit;
        }
    }
    if ((e = plat_enc(EN_SELECT)) != 0) {
        ui.touch_t = now;
        select_turn(e);
    }
    if ((e = plat_enc(EN_ALGO)) != 0) {           /* the tempo, wherever you are (SHAPE: the voice's level) */
        ui.touch_t = now;
        if (ui.btn >> B_PLAY & 1u) {              /* PLAY held: the loop's speed */
            const looper_t *l = engine_looper_state();
            int s = clamp_i((int)l->speed + (e > 0 ? 1 : -1), LP_HALF, LP_DOUBLE);
            ui.used |= 1u << B_PLAY;
            if (l->state == LS_EMPTY || l->state == LS_REC) {
                warn("NO LOOP YET", 0);
            } else {
                engine_looper(e > 0 ? LOOPER_FASTER : LOOPER_SLOWER);
                say(s == LP_HALF ? "LOOP HALF SPEED" : s == LP_DOUBLE ? "LOOP DOUBLE SPEED" : "LOOP AS PLAYED", 0);
            }
        } else if (layer() == L_DRONE) {
            drone_mode(e);
        } else if (layer() == L_BEND) {
            pitch_knob(e);
        } else if (ui.view == V_SHAPE) {
            shape_turn(-1, e);
        } else {
            int bpm = clamp_i(cfg.bpm_x10 / 10 + e, 20, 300);
            cfg.bpm_x10 = (uint16_t)(bpm * 10);
            settings_apply();
            changed();
            say_n("TEMPO ", bpm);
        }
    }
    if ((e = plat_enc(EN_PRESET)) != 0) {         /* the scenes (on JOURNEY: the list) */
        char v[24];
        ui.touch_t = now;
        if (ui.view == V_JOURNEY) {
            ui.jrow = (uint8_t)clamp_i(ui.jrow + e, 0, JROWS - 1);
            jrow_name(ui.jrow, v);
            say(v, 0);
        } else {
            scene_step(e);
        }
    }
    for (i = EN_K1; i <= EN_K4; i++)
        if ((e = plat_enc(i)) != 0) {
            ui.touch_t = now;
            switch (layer()) {
            case L_DRONE: drone_knob(i - EN_K1, e); break;
            case L_VSOUND: vsound_knob(i - EN_K1, e); break;
            case L_THROW: throw_knob(i - EN_K1, e); break;
            case L_BEND: bend_knob(i - EN_K1, e); break;
            default: knob_turn(i - EN_K1, e); break;
            }
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
static void header_logo(uint16_t c)               /* the mark, small, at the header's left: on every page */
{
    int x, y;
    for (y = 0; y < LOGO_T_H; y++)
        for (x = 0; x < LOGO_T_W; x++) {
            uint32_t a = LOGO_T_PX[y * ((LOGO_T_W + 1) / 2) + x / 2];
            a = (x & 1) ? (a & 15u) : (a >> 4);
            if (a)
                px_blend(3 + x, 1 + y, c, a + (a == 15u));
        }
}
static void draw_header(void)
{
    char b[16];
    cv_begin(240, HDR_H, C_BG);
    cv_rect(0, HDR_H - 1, 240, 1, C_LINE);
    if (ui.msg_until && (int32_t)(ui.msg_until - plat_ms()) > 0) {   /* what was just done */
        cv_rect(0, 0, 240, HDR_H - 1, dim(C_AMB, 4));
        cv_rect(0, HDR_H - 1, 240, 1, C_AMB);
        header_logo(C_WHITE);
        text_c(122, vc(&FONT_B, 0, HDR_H - 1), &FONT_B, ui.msg, C_WHITE);
        cv_commit(0, 0, 0);
        return;
    }
    ui.msg_until = 0;
    header_logo(RGB(226, 224, 236));              /* (in moonlight, as on HOME) */
    cv_text(8 + LOGO_T_W, vc(&FONT_B, 1, 17), &FONT_B, ui.view == V_HOME ? eng_jr.name : VIEW_N[ui.view], C_HI);
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
    const looper_t *l = engine_looper_state();
    char b[40], *q;
    int x;
    draw_corona();
    q = scene_line(b, r->mamt > 0.0f ? r->mbase : r->cur);
    if (r->travelling)                            /* on the way to the next */
        put_i(put_s(q, "  > "), r->dest + 1);
    else if (r->mamt > 0.0f)                      /* MORPH: how far to the next, by hand */
        put_i(put_s(q, "  ~ "), r->mto + 1);
    cv_text(6, 112, &FONT_XS, b, C_HI);
    if (r->mamt > 0.0f) {
        int w = text_w(&FONT_XS, b) + 10;
        cv_rect(w, 117, 40, 2, C_LINE);
        cv_rect(w, 117, (int)(r->mamt * 40.0f), 2, C_AMB);
    }
    if (ui.sleep) {                               /* SLEEP: the minutes left */
        char s[16];
        put_s(put_i(put_s(s, "SLEEP "), (sleep_left_ms() + 59999) / 60000), "M");
        text_r(234, 112, &FONT_XS, s, sleep_left_ms() <= (int)SLEEP_LAST_MS ? C_AMB : C_GRAY);
    } else {
        text_r(234, 112, &FONT_XS, eng_clock.playing ? "PLAYING" : "STOPPED", eng_clock.playing ? C_GRAY : C_DIM);
    }
    {   /* the looper: what it does, its layers, where it is in the loop */
        int rec = l->state == LS_REC || l->state == LS_DUB;
        uint16_t c = rec ? ((ui.frame >> 4) & 1u ? C_RED : dim(C_RED, 8)) : l->state == LS_PLAY ? C_AMB
                                                                                                  : C_DIM;
        loop_text(b);
        cv_text(6, 126, &FONT_XS, b, c);
        x = 8 + text_w(&FONT_XS, b);
        if (l->len && l->state != LS_EMPTY) {
            cv_rect(x, 131, 60, 2, C_LINE);
            cv_rect(x, 131, (int)((uint64_t)l->pos * 60u / l->len), 2, c);
        }
    }
    if (engine_frozen())                          /* notes frozen */
        cv_text(150, 126, &FONT_XS, "FROZEN", C_AMB);
    if (any_thrown())                             /* a throw kept (or drifting back) */
        text_r(234, 126, &FONT_XS, "THROW", ui.thr_kept ? C_AMB : C_DIM);
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

/* SHAPE: the voices across the top, the one shown lit (VOX: which of its pages, and its sound); its
 * four knobs as meters (an offset's from the middle); its level along the bottom, 100 marked */
static void draw_shape(void)
{
    const spage_t *pg = &PAGE[ui.vsel % NPG];
    char b[24];
    int v, k, lv = *pg_ptr(ui.vsel, -1);
    for (v = 0; v < EV_N; v++) {
        int cx = 30 + v * 60, sel = v == pg->voice;
        text_c(cx, 4, &FONT_XS, sel ? pg->name : VOICE_N[v], sel ? C_AMB : C_DIM);
        if (sel)
            cv_rect(cx - 22, 16, 44, 2, C_AMB);
    }
    for (k = 0; k < 4; k++) {
        int cx = 30 + k * 60, x = *pg_ptr(ui.vsel, k), top = 28, h = 66, fill;
        if (pg_octave(ui.vsel, k))
            x = x * 127 / 2;
        fill = x * h / 127;
        cv_rect(cx - 3, top, 6, h, C_LINE);
        if (pg->sgn >> k & 1u) {                  /* from the middle, up or down */
            int mid = top + h - 64 * h / 127;
            cv_rect(cx - 6, mid, 12, 1, C_DIM);
            if (top + h - fill < mid)
                cv_rect(cx - 3, top + h - fill, 6, mid - (top + h - fill), C_AMB);
            else
                cv_rect(cx - 3, mid, 6, top + h - fill - mid + 1, C_AMB);
        } else {
            cv_rect(cx - 3, top + h - fill, 6, fill, C_AMB);
        }
        text_c(cx, 98, &FONT_XS, pg->k[k], C_GRAY);
    }
    if (pg->voice == EV_VOX)                      /* (the sound of the four these pages shape) */
        put_s(put_s(b, jr_vsound_name(eng_jr.vcur)), " LEVEL");
    else
        put_s(b, "LEVEL");
    cv_text(6, 116, &FONT_XS, b, C_GRAY);
    cv_rect(90, 120, 110, 4, C_LINE);
    cv_rect(90, 120, lv * 110 / 127, 4, C_AMB);
    cv_rect(90 + 100 * 110 / 127, 117, 1, 10, C_DIM);   /* 100: as made */
    put_i(b, lv);
    text_r(232, 116, &FONT_XS, b, C_HI);
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
    cv_rect(6, 13 + ST_CORE1 * 10, 228, 1, C_LINE);  /* the stages above, each core's whole below */
    for (k = 0; k < ST_N; k++) {
        int total = k >= ST_CORE1, y = 12 + k * 10 + (total ? 3 : 0);
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
    case V_SHAPE: draw_shape(); break;
    case V_SCENE: draw_scene(); break;
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
    switch (layer()) {                            /* a hold: its knobs, on any page */
    case L_DRONE: {
        int v = lowest_bit(ui.dheld);
        for (k = 0; k < DV_N; k++) {
            dv_text(v, k, a);
            cell(k, DVF_N[k], a, C_AMB);
        }
        put_s(put_i(put_s(a, "DRONE "), v + 1), eng_jr.dswell >> v & 1u ? ": SWELL" : ": HELD");
        hint(a, "ALGO: HELD / SWELL");
        cv_commit(2, 0, KNOB_Y);
        return;
    }
    case L_VSOUND:
        for (k = 0; k < 4; k++) {
            int x = vsound()[VSH_F[k]];
            if (k == 0)
                put_sgn(a, x - 64);
            else
                put_i(a, x);
            cell(k, VSH_N[k], a, C_AMB);
        }
        put_s(put_s(a, "VOX "), jr_vsound_name(eng_jr.vcur));
        hint(a, "IT STAYS");
        cv_commit(2, 0, KNOB_Y);
        return;
    case L_THROW:
    case L_BEND: {
        int th = layer() == L_THROW;
        for (k = 0; k < 4; k++) {
            if (th)
                put_i(a, round_i(ui.thr[k]));
            else
                put_sgn(a, round_i(ui.bend[k]));
            cell(k, th ? (k == 1 ? "FEEDBK" : THR_N[k]) : (k == 1 ? "VIB" : BEND_N[k]), a, C_AMB);
        }
        if (!th) {
            char p[12];
            pitch_text(p);
            put_s(put_s(b, "PITCH "), p);         /* (ALGO's) */
        }
        hint(th ? "THROWS" : b, "SAVE: KEEP   LET GO: BACK");
        cv_commit(2, 0, KNOB_Y);
        return;
    }
    default: break;
    }
    switch (ui.view) {
    case V_HOME:
        for (k = 0; k < NMACROS; k++) {
            put_sgn(a, eng_jr.macro[k] - 64);     /* (each from as made, 0) */
            cell(k, MACRO_N[k], a, C_HI);
        }
        put_sgn(put_s(a, "OCTAVE "), ui.oct);
        hint("PRESET: SCENE  SELECT: MORPH", a);
        break;
    case V_SHAPE:
        for (k = 0; k < 4; k++) {
            shape_text(ui.vsel, k, a);
            cell(k, PAGE[ui.vsel].k[k], a, C_HI);
        }
        shape_text(ui.vsel, -1, b);
        put_s(put_s(c, "ALGO: LEVEL "), b);
        hint("ENV: NEXT PAGE", c);
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
    static const uint8_t VIEW_BTN[NVIEWS] = {B_HOME, B_ENV, B_EDIT, B_FX, B_LFO, B_GLO, B_GLO};
    const looper_t *l = engine_looper_state();
    uint32_t btn = 1u << VIEW_BTN[ui.view], keys = 0, kdim = 0;
    int i;
    if (eng_clock.playing)
        btn |= 1u << B_SEQ;
    if (l->state == LS_PLAY || l->state == LS_DUB)
        btn |= 1u << B_PLAY;
    if ((l->state == LS_REC || l->state == LS_DUB) && ((ui.frame >> 4) & 1u))
        btn |= 1u << B_REC;                       /* (blinking: recording) */
    if (engine_frozen())
        btn |= 1u << B_ARP;
    if (any_thrown())
        btn |= 1u << B_FX;                        /* (a throw out, held or kept) */
    kdim |= (1u << BLACK_KEY[ENG_DRONES]) | (1u << BLACK_KEY[ENG_DRONES + 1]);   /* PULSE's keys, dim */
    for (i = 0; i < JR_VSOUNDS; i++)              /* VOX's sounds: the one playing lit, the others dim */
        if (i == eng_jr.vcur)
            keys |= 1u << BLACK_KEY[VS_KEY0 + i];
        else
            kdim |= 1u << BLACK_KEY[VS_KEY0 + i];
    for (i = 0; i < ENG_DRONES; i++) {          /* the drone keys: lit while the voice sounds, dim else */
        if (engine_drone_on(i))
            keys |= 1u << BLACK_KEY[i];
        else
            kdim |= 1u << BLACK_KEY[i];
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
        loop_text(v);
        q = put_s(put_s(q, v), engine_frozen() ? ". FROZEN. DRONES ON:" : ". DRONES ON:");
        for (k = 0; k < ENG_DRONES; k++)
            if (engine_drone_on(k))
                q = put_i(put_s(q, " "), k + 1);
        break;
    case V_SYSTEM:
        row_text(ui.row, v);
        q = put_s(put_s(put_s(q, ROW_N[ui.row]), " "), v);
        break;
    case V_SHAPE:
        if (PAGE[ui.vsel].voice == EV_VOX)
            q = put_s(put_s(q, jr_vsound_name(eng_jr.vcur)), ". ");
        q = put_i(put_s(put_s(q, PAGE[ui.vsel].name), " LEVEL "), *pg_ptr(ui.vsel, -1));
        for (k = 0; k < 4; k++) {
            shape_text(ui.vsel, k, v);
            q = put_s(put_s(put_s(put_s(q, ". "), PAGE[ui.vsel].k[k]), " "), v);
        }
        break;
    case V_SCENE:
        sf_text(scene_sel(), ui.srow, v);
        q = put_s(put_s(put_s(put_s(put_i(put_s(q, "SCENE "), ui.ssel + 1), ". "), SF_N[ui.srow]), " "), v);
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
    ui.sdest = -1;
    ui.dirty = 0;                                 /* (the device starts from nothing; the simulator's reboot too) */
    ui.row = 0;
    ui.sleep = 0;
    ui.lp_edits = engine_looper_state()->edits;
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
    perf_tick();
    sleep_tick();
    if (engine_looper_state()->edits != ui.lp_edits) {   /* the loop changed: it is saved with the rest */
        ui.lp_edits = engine_looper_state()->edits;
        changed_as(D_LOOP);
    }
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
