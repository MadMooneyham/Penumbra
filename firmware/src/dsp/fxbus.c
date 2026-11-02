/* SPDX-License-Identifier: GPL-3.0-only */
/* Send reverb (spacevb.c) and the synced tape delay, after 9W9's er99_dly_tick (GPL-3.0). See fxbus.h. */
#include "fxbus.h"

/* 1/32 1/16T 1/16 1/8T 1/16. 1/8 1/4T 1/8. 1/4 1/2T 1/4. 1/2 1/2. (in beats) */
static const float fx_beats[13] = {
    0.125f, 1.0f / 6.0f, 0.25f, 1.0f / 3.0f, 0.375f, 0.5f, 2.0f / 3.0f,
    0.75f, 1.0f, 4.0f / 3.0f, 1.5f, 2.0f, 3.0f
};

/* ---- parameters ---- */
enum { CV_LIN, CV_EXP, CV_SW };
typedef struct {
    param_t ui;
    uint8_t curve, exp;
    float lo, hi;
} fx_pspec_t;

static const char *const fx_div_names[13] = {
    "1/32", "1/16T", "1/16", "1/8T", "1/16.", "1/8", "1/4T", "1/8.", "1/4", "1/2T", "1/4.", "1/2", "1/2."
};
static const char *const fx_onoff_names[2] = { "OFF", "ON" };
static const char *const fx_interval_names[SV_INTERVALS] = { "OCTAVE", "FIFTH", "TWELFTH" };

/* pot defaults: the positions 9W9 seeds from its defaults */
static const fx_pspec_t fx_p[FX_NPARAMS] = {
    [FX_RV_DECAY] = { { "RvDec", 127, 91, 0 }, CV_LIN, 0, 0.0f, 1.0f },     /* 1 .. 40 s; 91 = 14 s */
    [FX_RV_TONE]  = { { "RvTone", 127, 57, 0 }, CV_LIN, 0, 0.0f, 1.0f },
    [FX_RV_HPF]   = { { "RvHPF", 127, 62, 0 }, CV_EXP, DSP_EXP_FX_HPF, 0.0f, 0.0f },
    [FX_RV_LEVEL] = { { "RvLvl", 127, 106, 0 }, CV_LIN, 0, 0.0f, 1.2f },
    [FX_DL_TIME]  = { { "DlTime", 12, 7, fx_div_names }, CV_SW, 0, 0.0f, 0.0f },
    [FX_DL_FDBK]  = { { "DlFdbk", 127, 52, 0 }, CV_LIN, 0, 0.0f, 0.85f },
    [FX_DL_TONE]  = { { "DlTone", 127, 51, 0 }, CV_LIN, 0, 0.0f, 1.0f },
    [FX_DL_LEVEL] = { { "DlLvl", 127, 85, 0 }, CV_LIN, 0, 0.0f, 1.2f },
    [FX_DL_HPF]   = { { "DlHPF", 127, 62, 0 }, CV_EXP, DSP_EXP_FX_HPF, 0.0f, 0.0f },
    [FX_DL_WEAR]  = { { "Wear", 127, 64, 0 }, CV_LIN, 0, 0.0f, 1.0f },
    [FX_DL_PING]  = { { "Ping", 1, 1, fx_onoff_names }, CV_SW, 0, 0.0f, 0.0f },
};

static const param_t fx_ext_p[FXX_NPARAMS] = {
    [FXX_RV_SIZE] = { "Size", 127, 89, 0 },           /* 0.5 .. 1.5; 89 = 1.2 */
    [FXX_RV_MOD] = { "Mod", 127, 64, 0 },
    [FXX_RV_SHIMMER] = { "Shimmr", 127, 25, 0 },
    [FXX_RV_INTERVAL] = { "Intvl", SV_INTERVALS - 1, SV_OCTAVE, fx_interval_names },
};

int fxbus_ext_nparams(void) { return FXX_NPARAMS; }
const param_t *fxbus_ext_param(int i) { return i >= 0 && i < FXX_NPARAMS ? &fx_ext_p[i] : 0; }

void fxbus_ext_set(fxbus_t *f, int i, int value)
{
    if (i < 0 || i >= FXX_NPARAMS)
        return;
    if (value < 0) value = 0;
    if (value > fx_ext_p[i].max) value = fx_ext_p[i].max;
    f->ext_pots[i] = (uint8_t)value;
    const float x = (float)value / 127.0f;
    switch (i) {
    case FXX_RV_SIZE: spacevb_size(&f->rv, x); break;
    case FXX_RV_MOD: spacevb_mod(&f->rv, x); break;
    case FXX_RV_SHIMMER: spacevb_shimmer(&f->rv, x); break;
    case FXX_RV_INTERVAL: spacevb_interval(&f->rv, value); break;
    default: break;
    }
}

int fxbus_nparams(void) { return FX_NPARAMS; }

const param_t *fxbus_param(int i)
{
    return i >= 0 && i < FX_NPARAMS ? &fx_p[i].ui : 0;
}

static void fx_retime(fxbus_t *f)
{
    int32_t i = f->dl_div;
    if (i < 0) i = 0;
    if (i > 12) i = 12;
    const float bpm = f->bpm > 20.0f ? f->bpm : 120.0f;
    float ms = fx_beats[i] * 60000.0f / bpm;
    const float max_ms = (float)(f->dlen - 256) / D9_SR * 1000.0f;
    while (ms > max_ms)                 /* longer than the line (1/2. under 90 BPM): half of it, */
        ms *= 0.5f;                     /* which is still on the beat; cutting it short is not */
    f->dl_time_ms = ms;
}

static void fx_apply(fxbus_t *f, int i, float v)
{
    switch (i) {
    case FX_RV_DECAY: spacevb_decay(&f->rv, v); break;
    case FX_RV_TONE: spacevb_tone(&f->rv, v); break;
    case FX_RV_HPF: spacevb_lowcut(&f->rv, v); break;
    case FX_RV_LEVEL: spacevb_level(&f->rv, v); break;
    case FX_DL_TIME: f->dl_div = (int32_t)v; fx_retime(f); break;
    case FX_DL_FDBK: f->dl_fdbk = v; break;
    case FX_DL_TONE: f->dl_tone = v; break;
    case FX_DL_LEVEL: f->dl_level = v; break;
    case FX_DL_HPF: d9_biquad_set(&f->dl_hp, D9_HP, v, 0.7071f); break;
    case FX_DL_WEAR: f->wear = v; break;
    case FX_DL_PING: f->dl_ping = (int32_t)v; break;
    default: break;
    }
}

void fxbus_set(fxbus_t *f, int i, int value)
{
    if (i < 0 || i >= FX_NPARAMS)
        return;
    const fx_pspec_t *s = &fx_p[i];
    if (value < 0) value = 0;
    if (value > s->ui.max) value = s->ui.max;
    float v;
    if (s->curve == CV_EXP)
        v = dsp_pot_exp[s->exp][value];
    else if (s->curve == CV_SW)
        v = (float)value;
    else
        v = s->lo + (s->hi - s->lo) * ((float)value / 127.0f);
    fx_apply(f, i, v);
}

void fxbus_set_bpm(fxbus_t *f, float bpm)
{
    if (bpm > 20.0f && bpm != f->bpm) {
        f->bpm = bpm;
        fx_retime(f);
    }
}

void fxbus_init(fxbus_t *f, int16_t *dly_buf, int dly_len)
{
    uint8_t *p = (uint8_t *)f;
    for (unsigned i = 0; i < sizeof(*f); ++i)
        p[i] = 0;
    for (int i = 0; i < dly_len; ++i)
        dly_buf[i] = 0;
    f->dbuf = dly_buf;
    f->dlen = dly_len;

    spacevb_init(&f->rv, 0, 0);                  /* silent until fxbus_reverb_buffers */

    f->dl_div = 7;
    f->bpm = 120.0f;
    f->dl_fdbk = 0.35f; f->dl_tone = 0.4f; f->dl_level = 0.8f;
    fx_retime(f);
    f->dcur = f->dl_time_ms * 0.001f * D9_SR;
    d9_biquad_set(&f->dl_hp, D9_HP, 150.0f, 0.7071f);
    f->dl_quiet = dly_len;
    f->wear = 64.0f / 127.0f;
    f->warp = 1.0f;
    f->dl_ping = 1;

    for (int i = FX_RV_DECAY; i <= FX_RV_LEVEL; ++i)   /* the reverb's pots into spacevb (its own defaults differ) */
        fxbus_set(f, i, fx_p[i].ui.def);
    for (int i = 0; i < FXX_NPARAMS; ++i)
        fxbus_ext_set(f, i, fx_ext_p[i].def);
}

void fxbus_reverb_buffers(fxbus_t *f, int16_t *rv, int16_t *ps)
{
    spacevb_t keep = f->rv;                      /* the settings; the lines start empty */
    spacevb_init(&f->rv, rv, ps);
    f->rv.t60 = keep.t60;
    f->rv.size = keep.size;
    f->rv.shimmer = keep.shimmer;
    f->rv.damp_c = keep.damp_c;
    f->rv.hp_c = keep.hp_c;
    f->rv.mod = keep.mod;
    f->rv.level = keep.level;
    f->rv.ps_inc = keep.ps_inc;
    spacevb_size(&f->rv, f->rv.size - 0.5f);     /* (recomputes the lines' gains) */
}

/* ===================================================================== */
/* Delay (after er99_dly_tick), adds its return into out_l and out_r      */
/* ===================================================================== */
static inline float fx_psin(uint32_t ph)                 /* parabolic sine of a turn */
{
    const float x = (float)(int32_t)ph * (1.0f / 2147483648.0f);
    return 4.0f * x * (1.0f - fm_fabsf(x));
}

/* the line at d samples behind w, interpolated (the ping-pong tap) */
static inline float dl_tap(const int16_t *buf, int32_t len, float flen, int32_t w, float d)
{
    float rp = (float)w - d;
    while (rp < 0.0f) rp += flen;
    int32_t i0 = (int32_t)rp;
    const float fr = rp - (float)i0;
    if (i0 >= len) i0 -= len;
    const int32_t i1 = i0 + 1 >= len ? 0 : i0 + 1;
    const float b0 = (float)buf[i0] * (1.0f / 2048.0f), b1 = (float)buf[i1] * (1.0f / 2048.0f);
    return b0 + (b1 - b0) * fr;
}

/* The left reads at the time and the right at twice it, the loop fed from the right: echoes alternate
 * L R L R, one time apart. The line holds both, so ping-pong needs twice the time to fit (a 2 s line:
 * 1 s); past that it plays centred. TAPE: wow (0.5 Hz) and flutter (6.1 Hz) on the read, a darker loop,
 * saturation and a 30 Hz DC block inside it, feedback up to 1.15 */
static void fx_delay(fxbus_t *f, const float *in, float *out, float *out_r, int n)
{
    /* the caller may hear the returns dl_lead samples late (engine.c runs them a block behind): the
     * echo is read that much sooner, so it still lands on time */
    const int32_t len = f->dlen;
    const float target = fm_minf(fm_maxf(f->dl_time_ms * 0.001f * D9_SR * f->warp - f->dl_lead, 1.0f),
                                 (float)len - 8.0f);   /* (WARP: never past the line) */
    if (f->dl_quiet >= len) {
        int any = 0;
        for (int i = 0; i < n; ++i)
            any |= in[i] != 0.0f;
        if (!any) {
            /* the line is all zeros: nothing to read, but time moves on */
            f->w = (f->w + n) % len;
            /* the time slew, sample by sample as 9W9 runs it: in float it stalls
             * ~1 sample short of the target (the step rounds away), and that
             * stalled value is where 9W9's echoes land. It stops at once
             * when it has stalled, so an idle line costs nothing. */
            {
                float dc = f->dcur;
                for (int i = 0; i < n; ++i) {
                    const float nd = dc + (target - dc) * 0.0008f;
                    if (nd == dc)
                        break;
                    dc = nd;
                }
                f->dcur = dc;
            }
            f->wow_ph += d9_inc(0.5f) * (uint32_t)n;
            f->flut_ph += d9_inc(6.1f) * (uint32_t)n;
            return;
        }
    }
    int16_t *buf = f->dbuf;
    const float flen = (float)len;
    const int ping = f->dl_ping && 2.0f * fm_maxf(f->dcur, target) + f->dl_lead + 4.0f < flen;
    const float lvl = f->dl_level;
    int32_t quiet = f->dl_quiet;
    int32_t w = f->w;
    float dcur = f->dcur, lp = f->lp;

    const float wear = f->wear;
    const float depth = 0.1f + 1.9f * wear;                  /* x nominal; 1 at wear ~0.5 */
    const float wow_a = 1.5f * D9_MS * depth, flut_a = 0.15f * D9_MS * depth;
    const uint32_t wow_i = d9_inc(0.5f), flut_i = d9_inc(6.1f);
    const float fdbk = f->dl_fdbk * (1.15f / 0.85f);
    const float tc = (0.06f + f->dl_tone * 0.6f) * (0.6f - 0.3f * wear);
    const float g = 1.0f + 1.5f * wear, inv_g = 1.0f / g;
    uint32_t wp = f->wow_ph, fp = f->flut_ph;
    float hp = f->tape_hp;
    for (int i = 0; i < n; ++i) {
        const float x = d9_biquad_tick(&f->dl_hp, in[i]);
        dcur += (target - dcur) * 0.0008f;
        wp += wow_i;
        fp += flut_i;
        const float dmod = dcur + wow_a * fx_psin(wp) + flut_a * fx_psin(fp);
        float rp = (float)w - dmod;
        while (rp < 0.0f) rp += flen;
        int32_t i0 = (int32_t)rp;
        const float fr = rp - (float)i0;
        if (i0 >= len) i0 -= len;
        const int32_t i1 = i0 + 1 >= len ? 0 : i0 + 1;
        const float b0 = (float)buf[i0] * (1.0f / 2048.0f), b1 = (float)buf[i1] * (1.0f / 2048.0f);
        const float y = b0 + (b1 - b0) * fr;
        const float y2 = ping ? dl_tap(buf, len, flen, w, 2.0f * dmod + f->dl_lead) : y;
        lp += (y2 * fdbk - lp) * tc;
        if (fm_fabsf(lp) < 1e-20f) lp = 0.0f;
        float s = d9_tanh(lp * g) * inv_g;                   /* tape saturation holds the loop */
        hp += (s - hp) * 0.0042725f;                          /* 30 Hz: no DC latch-up past unity */
        s -= hp;
        int32_t k = (int32_t)((x + s) * 2048.0f);
        if (k > 32767) k = 32767;
        if (k < -32767) k = -32767;
        buf[w] = (int16_t)k;
        if (++w >= len) w = 0;
        out[i] += y * lvl;
        out_r[i] += y2 * lvl;
        quiet = (k == 0 && in[i] == 0.0f) ? quiet + 1 : 0;
    }
    f->wow_ph = wp;
    f->flut_ph = fp;
    f->tape_hp = fm_flush(hp);
    f->w = w;
    f->dcur = dcur;
    f->lp = lp;
    f->dl_quiet = quiet;
}

void fxbus_wet_st(fxbus_t *f, const float *rev, const float *dly, float *wet_l, float *wet_r, int n)
{
    if (n > 256)
        n = 256;
    for (int i = 0; i < n; ++i)
        wet_l[i] = wet_r[i] = 0.0f;
    spacevb_process(&f->rv, rev, wet_l, wet_r, n);
    fx_delay(f, dly, wet_l, wet_r, n);
}

void fxbus_set_lead(fxbus_t *f, int samples) { f->dl_lead = (float)samples; }
void fxbus_set_warp(fxbus_t *f, float factor) { f->warp = fm_clampf(factor, 0.25f, 4.0f); }
