/* SPDX-License-Identifier: GPL-3.0-only */
/* The send FX, shared by every voice: the reverb and a tempo-synced tape delay. The delay is a port
 * of 9W9's (GPL-3.0), the reverb the drone firmware's own:
 *
 *  - Reverb: spacevb.h, the long modulated network with its shimmer (it replaced 9W9's
 *    four combs). Its SIZE, MOD and SHIMMER are the "ext" pots.
 *  - Delay: a note division of the tempo. A new tempo or division is taken once it has settled
 *    (80 ms), crossfading from the old time to the new (46 ms): the echoes stay on pitch. WARP
 *    instead slews the time, so its echoes bend like tape. Read with wow and flutter; feedback through a darkening one-pole, saturating inside the loop
 *    and running slightly past unity into self-oscillation that the saturation holds; WEAR scales
 *    all of it. 12-bit write, HPF on the send. PING-PONG (on as made): the left reads at the time,
 *    the right at twice it; off, both sides hear the same echo.
 *
 * The delay line is int16 (9W9 writes it at 12 bits, so nothing is lost; values
 * past +-16 full scale saturate) and is passed in by the caller (.pool), as are
 * the reverb's lines (fxbus_reverb_buffers; without them the reverb is silent).
 * Both effects go idle (no per-sample work) once their input and their lines are
 * silent. (X0X's master stage after them, 9W9's distortion and glue compressor, and its plain
 * delay are gone: Penumbra never used them.) */
#pragma once
#include <stdint.h>
#include "param.h"
#include "fxkit.h"
#include "spacevb.h"

enum { FX_RV_DECAY, FX_RV_TONE, FX_RV_HPF, FX_RV_LEVEL,
       FX_DL_TIME, FX_DL_FDBK, FX_DL_TONE, FX_DL_LEVEL,
       FX_DL_HPF, FX_DL_WEAR, FX_DL_PING, FX_NPARAMS };
/* the reverb's further pots (INTERVAL: spacevb.h SV_*) */
enum { FXX_RV_SIZE, FXX_RV_MOD, FXX_RV_SHIMMER, FXX_RV_INTERVAL, FXX_NPARAMS };

struct fxbus {
    /* reverb */
    spacevb_t rv;
    uint8_t ext_pots[FXX_NPARAMS];
    /* delay */
    float dl_time_ms, dl_fdbk, dl_tone, dl_level, bpm, wear;
    float dl_use_ms;                     /* the time the echoes are on (dl_time_ms once it has settled) */
    float dl_xold;                       /* the old read offset, while crossfading from it */
    int32_t dl_xf, dl_settle;            /* samples of the crossfade left; since the time last moved */
    float dl_gl, dl_gr, dl_tl, dl_tr;    /* the echoes' balance now, and where it is going (fxbus_set_balance) */
    float dl_lead;                       /* samples the caller hears the returns late (fxbus_set_lead) */
    float warp;                          /* the time's factor now (WARP: fxbus_set_warp) */
    int32_t dl_div, dl_ping;
    d9_biquad_t dl_hp;
    int16_t *dbuf;
    int32_t dlen, w, dl_quiet;
    float dcur, lp, tape_hp;
    uint32_t wow_ph, flut_ph;
};
typedef struct fxbus fxbus_t;

void fxbus_init(fxbus_t *f, int16_t *dly_buf, int dly_len);
/* the reverb's lines: rv SV_TOTAL samples, ps SV_PS_LEN (after fxbus_init) */
void fxbus_reverb_buffers(fxbus_t *f, int16_t *rv, int16_t *ps);
int fxbus_ext_nparams(void);
const param_t *fxbus_ext_param(int i);
void fxbus_ext_set(fxbus_t *f, int i, int value);
void fxbus_set_bpm(fxbus_t *f, float bpm);
/* the returns of the sends (rev, dly: mono) alone, into wet_l / wet_r */
void fxbus_wet_st(fxbus_t *f, const float *rev, const float *dly, float *wet_l, float *wet_r, int n);
void fxbus_set_lead(fxbus_t *f, int samples);   /* the returns heard this late: the delay reads sooner */
/* WARP: the delay's time x factor (1: as set; 0.25 .. 4), reached as the time always moves, gliding: the
 * echoes bend up as it shortens and dive as it grows, like a tape slowing and speeding */
void fxbus_set_warp(fxbus_t *f, float factor);
/* the echoes' balance (param.h's law: 1 and 1 the middle), reached across the next block */
void fxbus_set_balance(fxbus_t *f, float l, float r);
int fxbus_nparams(void);
const param_t *fxbus_param(int i);
void fxbus_set(fxbus_t *f, int i, int value);
