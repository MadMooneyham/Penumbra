/* SPDX-License-Identifier: GPL-3.0-only */
/* Penumbra's user interface, phase 1: three views and the panel.
 *
 *   HOME    the corona: the Lunar Eclipse Forge mark on the eclipsed moon, its corona pulsing with
 *           the music, a bead on the rim for each drone voice (lit while it sounds), a star for each
 *           LINE note, the bar's 16ths underneath while the clock runs. KNOB 1: the tempo.
 *   SYSTEM  the settings, a list: SELECT moves, any knob (or SEL) changes the row.
 *   BENCH   the benchmark: each stage of the render, its average and its worst block over the last
 *           second, in % of the block's real-time budget. SEL runs the load test.
 *
 * Panel: HOME goes home, GLO opens SYSTEM and then BENCH, PLAY starts and stops the clock, SAVE
 * saves the settings now. Black keys 1-6 latch the six drone voices; the white keys play LINE in
 * A minor, OCT- and OCT+ move them.
 *
 * The screen is three bands (header, main, knob strip), each drawn into Felucca's canvas and
 * blitted only where its pixels changed: SPI time is the scarce thing. */
#include "penumbra.h"
#include "../dsp/fastmath.h"

/* ================================================================ model === */
static const int8_t KEY_WHITE[NKEYS] = {0, -1, 1, -1, 2, -1, 3, 4, -1, 5, -1, 6, 7, -1, 8, -1, 9, -1, 10, 11, -1, 12, -1, 13, 14, -1, 15};
static const int8_t KEY_BLACK[NKEYS] = {-1, 0, -1, 1, -1, 2, -1, -1, 3, -1, 4, -1, -1, 5, -1, 6, -1, 7, -1, -1, 8, -1, 9, -1, -1, 10, -1};
static uint8_t BLACK_KEY[11];

enum { V_HOME, V_SYSTEM, V_BENCH, NVIEWS };
static const char *const VIEW_N[NVIEWS] = {"HOME", "SYSTEM", "BENCH"};

/* LINE on the white keys: A natural minor from A3, two octaves and a bit */
static const uint8_t MINOR[7] = {0, 2, 3, 5, 7, 8, 10};
#define LINE_ROOT 57

typedef struct {
    uint8_t view;
    int8_t oct;                        /* the white keys' octave, -2..+2 */
    uint8_t row;                       /* SYSTEM's row */
    uint32_t btn, keys;                /* held, as last seen */
    uint8_t key_note[16];              /* the note each white key started (an octave change while held) */
    char msg[28];
    uint32_t msg_until;
    uint32_t frame;
    uint8_t dirty;                     /* the settings changed since they were saved */
    uint32_t act_t;                    /* the last change (a save waits for quiet) */
    uint8_t stars;                     /* white keys pressed since the last frame: a star each */
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

/* ================================================================ settings === */
/* SYSTEM's rows: a setting, its choices, and what a change does */
enum { R_CLKOUT, R_CLKIN, R_MIDICH, R_CORES, R_DRNCORE, R_OUTLOUD, R_USBFULL, R_ABOUT, NROWS };
static const char *const ROW_N[NROWS] = {"MIDI CLOCK OUT", "FOLLOW CLOCK IN", "LINE MIDI CH", "2ND CORE",
                                         "DRONE CORE", "OUTPUT LEVEL", "USB AUDIO", "ABOUT"};
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
    v = ((v % n) + n) % n;
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
    default: break;
    }
    settings_apply();
    ui.dirty = 1;
    ui.act_t = plat_ms();
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
 * the audio for a moment, so never under a sound. SAVE saves at once, whatever plays. */
#define SAVE_QUIET_MS 2000u
#define SAVE_SILENT_MS 1000u
static void save_now(void)
{
    if (settings_save() == 0) {
        ui.dirty = 0;
        say("SETTINGS SAVED", 0);
    } else {
        warn("NOT SAVED", 0);
    }
}
static void autosave(void)
{
    if (ui.dirty && plat_ms() - ui.act_t >= SAVE_QUIET_MS && engine_quiet_ms() >= SAVE_SILENT_MS)
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

static int white_note(int w)
{
    int n = LINE_ROOT + 12 * ui.oct + 12 * (w / 7) + MINOR[w % 7];
    return n < 0 ? 0 : n > 127 ? 127 : n;
}

static void input(void)
{
    uint32_t b = plat_buttons(), k = plat_keys(), down = b & ~ui.btn, kd = k & ~ui.keys, ku = ui.keys & ~k;
    int i, e;
    ui.btn = b;
    ui.keys = k;
    for (i = 0; i < NKEYS; i++) {
        int w = KEY_WHITE[i], bk = KEY_BLACK[i];
        if (kd >> i & 1u) {
            if (w >= 0) {
                ui.key_note[w] = (uint8_t)white_note(w);
                engine_line_on(ui.key_note[w]);
                if (ui.stars < 8u)
                    ui.stars++;
            } else if (bk >= 0 && bk < ENG_DRONES) {
                engine_drone_key(bk);
            }
        }
        if ((ku >> i & 1u) && w >= 0)
            engine_line_off(ui.key_note[w]);
    }
    if (down & (1u << B_HOME))
        set_view(V_HOME);
    if (down & (1u << B_GLO))
        set_view(ui.view == V_SYSTEM ? V_BENCH : V_SYSTEM);
    if (down & (1u << B_PLAY)) {
        if (eng_clock.playing)
            tclock_stop(&eng_clock);
        else
            tclock_play(&eng_clock);
        say(eng_clock.playing ? "STOP" : "PLAY", 0);
    }
    if (down & (1u << B_SAVE))
        save_now();
    if (down & ((1u << B_OCTDN) | (1u << B_OCTUP))) {
        char t[8];
        ui.oct = (int8_t)(ui.oct + ((down >> B_OCTUP) & 1u) - ((down >> B_OCTDN) & 1u));
        ui.oct = (int8_t)(ui.oct < -2 ? -2 : ui.oct > 2 ? 2 : ui.oct);
        put_i(put_s(t, ui.oct > 0 ? "+" : ""), ui.oct);
        say("LINE OCTAVE ", t);
    }
    if (down & (1u << B_SEL)) {
        if (ui.view == V_SYSTEM)
            row_set(ui.row, row_get(ui.row) + 1);
        else if (ui.view == V_BENCH) {
            int on = !engine_load_testing();      /* (the engine takes it at its next block) */
            engine_load_test(on);
            say(on ? "LOAD TEST ON" : "LOAD TEST OFF", 0);
        }
    }
    if ((e = plat_enc(EN_SELECT)) != 0 && ui.view == V_SYSTEM) {
        int r = ui.row + e;
        ui.row = (uint8_t)(r < 0 ? 0 : r >= NROWS ? NROWS - 1 : r);
        {
            char t[40], v[24];
            row_text(ui.row, v);
            put_s(put_s(put_s(t, ROW_N[ui.row]), " "), v);
            ann_say(t, 0, ANN_NOW);
        }
    }
    for (i = EN_ALGO; i < NE; i++) {
        if (i == EN_PRESET)
            continue;
        if ((e = plat_enc(i)) == 0)
            continue;
        if (ui.view == V_SYSTEM) {
            row_set(ui.row, row_get(ui.row) + (e > 0 ? 1 : -1));
        } else if (ui.view == V_HOME && i == EN_K1) {
            char t[8];
            int bpm = cfg.bpm_x10 / 10 + e;
            bpm = bpm < 20 ? 20 : bpm > 300 ? 300 : bpm;
            cfg.bpm_x10 = (uint16_t)(bpm * 10);
            settings_apply();
            ui.dirty = 1;
            ui.act_t = plat_ms();
            put_i(t, bpm);
            say("TEMPO ", t);
        }
    }
    plat_enc(EN_PRESET);                          /* (no job yet) */
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
    cv_text(14, vc(&FONT_B, 1, 17), &FONT_B, VIEW_N[ui.view], C_HI);
    if (ui.dirty)
        dot(100, 9, 2, C_AMB);                    /* settings not saved yet */
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
#define EC_CY 60
#define EC_RS 38                                   /* the moon's rim */
#define EC_CW 15                                   /* the corona's reach past it */
#define EC_NSTAR 14
static struct { int16_t x, y; uint8_t life; } ec_star[EC_NSTAR];
static uint8_t ec_star_w;
static float ec_lvl, ec_flare;
static uint32_t ec_rng = 0x2545F491u, ec_last_step;
static uint32_t ec_rand(void)
{
    ec_rng ^= ec_rng << 13;
    ec_rng ^= ec_rng >> 17;
    ec_rng ^= ec_rng << 5;
    return ec_rng;
}
static void ec_star_new(void)                     /* a LINE note: a star comes out */
{
    float a = (float)(ec_rand() & 1023u) * (FM_TWO_PI / 1024.0f);
    float r = (float)(EC_RS + EC_CW + 4) + (float)(ec_rand() % 40u);
    int x = EC_CX + (int)(fm_cosf(a) * r * 1.6f), y = EC_CY + (int)(fm_sinf(a) * r * 0.62f);
    ec_star[ec_star_w].x = (int16_t)(x < 6 ? 6 : x > 234 ? 234 : x);
    ec_star[ec_star_w].y = (int16_t)(y < 6 ? 6 : y > 108 ? 108 : y);
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
    if (eng_steps != ec_last_step) {              /* the clock's beats flare it, the bar's first most */
        ec_last_step = eng_steps;
        if (eng_steps && ((eng_steps - 1u) & 3u) == 0u)
            ec_flare = ((eng_steps - 1u) & 15u) == 0u ? 1.0f : 0.5f;
    }
    ec_flare *= 0.9f;
    breath = 0.5f + 0.5f * fm_sin_turns((float)(ui.frame & 255u) * (1.0f / 256.0f));   /* (quiet: it breathes) */
    amp = 0.22f + 0.08f * breath + 0.7f * ec_lvl + 0.35f * ec_flare;
    rot = (float)(ui.frame & 4095u) * (1.0f / 4096.0f);
    for (k = 0; k < 40; k++) {                    /* the sky: fixed stars, each twinkling its own way */
        uint32_t h = (uint32_t)k * 2654435761u;
        int sx = (int)(h % 236u) + 2, sy = (int)((h >> 12) % 106u) + 4;
        int tw8 = (int)(((h >> 20) + ui.frame / 6u) & 15u);
        cv_pset(sx, sy, blend(bg, C_GRAY, tw8 < 3 ? 12 : 5));
    }
    for (k = 0; k < EC_NSTAR; k++)                /* LINE's stars, fading */
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
    for (k = 0; k < ENG_DRONES; k++) {          /* the drones: six beads on the rim, from the top, clockwise */
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

static void draw_home(void)
{
    draw_corona();
    if (eng_clock.playing) {                      /* the bar's 16ths */
        int s, ph = (int)((eng_steps ? eng_steps - 1u : 0u) & 15u);
        char b[12];
        put_i(put_s(b, "BAR "), (int)((eng_steps ? eng_steps - 1u : 0u) / 16u) + 1);
        cv_text(6, 118, &FONT_XS, b, C_GRAY);
        for (s = 0; s < 16; s++) {
            int sx = 60 + s * 9 + (s >> 2) * 4;
            if (s == ph)
                dot(sx, 124, 2, C_WHITE);
            else
                dot(sx, 124, 1, (s & 3) == 0 ? C_GRAY : C_DIM);
        }
    } else {
        text_c(120, 118, &FONT_XS, "BLACK KEYS: DRONES   WHITE KEYS: LINE", C_DIM);
    }
}

/* ----------------------------------------------------------------- SYSTEM --- */
#define ROW_H 22
#define ROWS_SHOWN 6
static void draw_system(void)
{
    int top = ui.row < ROWS_SHOWN ? 0 : ui.row - ROWS_SHOWN + 1, r;
    for (r = top; r < NROWS && r < top + ROWS_SHOWN; r++) {
        int y = 2 + (r - top) * ROW_H, sel = r == ui.row;
        char v[24];
        if (sel)
            cv_rect(0, y, 240, ROW_H - 2, dim(C_AMB, 3));
        cv_text(10, vc(&FONT_S, y, ROW_H - 2), &FONT_S, ROW_N[r], sel ? C_HI : C_GRAY);
        row_text(r, v);
        text_r(230, vc(&FONT_S, y, ROW_H - 2), &FONT_S, v, sel ? C_AMB : C_HI);
    }
    if (ui.row == R_CORES && (!cfg.one_core) != plat_cpu2_on())
        text_c(120, 2 + ROWS_SHOWN * ROW_H, &FONT_XS, "CHANGES AT THE NEXT POWER-ON", C_AMB);
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
    cv_rect(6, 13 + ST_CORE1 * 11, 228, 1, C_LINE);  /* the stages above, each core's whole below */
    for (k = 0; k < ST_N; k++) {
        int total = k >= ST_CORE1, y = 13 + k * 11 + (total ? 3 : 0);
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
        text_r(210, y, &FONT_XS, b, bench.peak[k] >= 900 ? RGB(255, 90, 80) : bench.peak[k] >= 700 ? C_AMB : c);
    }
}

static void draw_main(void)
{
    cv_begin(240, MAIN_H, C_BG);
    if (ui.view == V_HOME)
        draw_home();
    else if (ui.view == V_SYSTEM)
        draw_system();
    else
        draw_bench();
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
static void draw_knobs(void)
{
    char a[12], b[12], c[12], d[12];
    cv_begin(240, KNOB_H, C_BG);
    cv_rect(0, 0, 240, 1, C_LINE);
    if (ui.view == V_HOME) {
        put_i(a, cfg.bpm_x10 / 10);
        cell(0, "TEMPO", a, eng_clock.ext ? C_DIM : C_HI);
        cell(1, "", "-", C_DIM);
        cell(2, "", "-", C_DIM);
        put_i(put_s(d, ui.oct > 0 ? "+" : ""), ui.oct);
        cell(3, "LINE OCT", d, C_HI);
    } else if (ui.view == V_SYSTEM) {
        text_c(120, 30, &FONT_XS, "SELECT: ROW    KNOB OR SEL: CHANGE", C_DIM);
        text_c(120, 46, &FONT_XS, "SAVE: SAVE NOW", C_DIM);
    } else {
        static uint32_t blk_peak, blk_t;
        if (plat_ms() - blk_t >= 1000u) {         /* the platform's whole half buffer, its worst each second */
            blk_t = plat_ms();
            blk_peak = plat_cpu_peak_pct();
        }
        put_s(put_i(put_s(put_i(a, (int)plat_cpu_pct()), "/"), (int)blk_peak), "");
        cell(0, "LOAD/PEAK", a, blk_peak >= 90u ? RGB(255, 90, 80) : C_HI);
        put_i(b, (int)plat_xruns());
        cell(1, "XRUNS", b, plat_xruns() ? RGB(255, 90, 80) : C_HI);
        put_i(c, (int)eng_guard_count);
        cell(2, "GUARD", c, engine_guard_active() ? C_AMB : C_HI);
        put_i(d, (int)eng_out_clips);
        cell(3, "CLIPS", d, eng_out_clips ? RGB(255, 90, 80) : C_HI);
        {
            uint32_t mhz = plat_cpu_mhz10();
            char *q = put_s(a, "CPU ");
            if (mhz)
                put_s(put_i(q, (int)(mhz / 10u)), " MHZ");
            else
                put_s(q, "? MHZ");
            cv_text(6, 64, &FONT_XS, a, C_DIM);
        }
        text_r(234, 64, &FONT_XS, engine_load_testing() ? "SEL: STOP LOAD TEST" : "SEL: LOAD TEST", C_DIM);
    }
    cv_commit(2, 0, KNOB_Y);
}

/* ------------------------------------------------------------------- lights --- */
static void leds(void)
{
    uint32_t btn = 0, keys = 0, kdim = 0;
    int i;
    if (ui.view == V_HOME)
        btn |= 1u << B_HOME;
    else
        btn |= 1u << B_GLO;
    if (eng_clock.playing)
        btn |= 1u << B_PLAY;
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
    int k;
    q = put_s(put_s(q, VIEW_N[ui.view]), ". ");
    if (ui.view == V_HOME) {
        q = put_s(q, eng_clock.playing ? "PLAYING. " : "STOPPED. ");
        q = put_s(put_i(put_s(q, "TEMPO "), cfg.bpm_x10 / 10), ". DRONES ON:");
        for (k = 0; k < ENG_DRONES; k++)
            if (engine_drone_on(k))
                q = put_i(put_s(q, " "), k + 1);
    } else if (ui.view == V_SYSTEM) {
        row_text(ui.row, v);
        q = put_s(put_s(put_s(q, ROW_N[ui.row]), " "), v);
    } else {
        for (k = ST_DRN1; k < ST_N && q < t + ANN_MAX - 32; k++)
            q = put_s(put_d1(put_s(put_s(put_s(q, engine_stage_name(k)), " PEAK "), ""), bench.peak[k]), ". ");
    }
    ann_say(t, 0, ANN_NOW);
}

/* ===================================================================== frame === */
void ui_init(void)
{
    int i;
    for (i = 0; i < NKEYS; i++)
        if (KEY_BLACK[i] >= 0)
            BLACK_KEY[KEY_BLACK[i]] = (uint8_t)i;
    ui.view = V_HOME;
    for (i = 0; i < 3; i++)
        blit_hash[i] = 0;
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
