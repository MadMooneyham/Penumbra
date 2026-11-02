/* SPDX-License-Identifier: GPL-3.0-only */
/* The send reverb: the browser prototype's (drone-proto/drone.html, in the history up to 1.0) eight-line feedback delay
 * network, for long, slowly moving washes, with an octave-up shimmer fed back into it.
 *
 *  - Eight delay lines (29.7 .. 73.1 ms at SIZE 1, up to x1.5), mixed by an 8x8 Hadamard
 *    matrix, each with its own gain for the DECAY (seconds to -60 dB) and a one-pole damping
 *    (TONE) in the loop; four lines read with a slow modulation (MOD) to keep it from ringing.
 *  - Left out: lines 0, 4 minus 2, 6; right: 1, 5 minus 3, 7.
 *  - Shimmer: a two-head granular pitch shifter, an octave up (or a fifth, or a twelfth: INTERVAL), on
 *    the output, fed back into the input. Its return is scaled by (1 - the tank's mean gain), so the
 *    loop stays below unity at any DECAY, and soft-clipped.
 *  - SIZE moves: the lines' lengths slew to the new size (at most SV_SLEW samples a sample, a few per
 *    cent of pitch while they move: the room breathes, it never clicks); silent, they jump.
 *
 * The lines are int16 (x 1/8192: +-4 full scale) in a buffer the caller passes in (.pool), the
 * shimmer's in another. Idle (no per-sample work) once its input and its lines are silent.
 *
 * Penumbra: the network runs at half the rate (22050 Hz, SV_DS), between a 39-tap half-band
 * decimator and interpolator (flat to 8 kHz, -80 dB from 14 kHz: nothing folds back as grit).
 * Half the time and half the lines' memory; the shimmer keeps its air up to 11 kHz. */
#pragma once
#include <stdint.h>

#define SV_LINES 8
#define SV_SIZE_MAX 1.5f
#define SV_DS 2                           /* the network's rate: 44100 / SV_DS */
#define SV_TOTAL 14000                    /* int16 samples: every line at SIZE_MAX, + modulation room */
#define SV_PS_LEN 2048                    /* the shimmer's line (power of two) */
#define SV_HB_N 39                        /* the half-band's taps (19 on each side, 10 of them non-zero) */
#define SV_HB_Y 20                        /* the interpolator's history: one output per even tap */
#define SV_SLEW 0.03f                     /* a line's length moves at most this many samples a sample */
enum { SV_OCTAVE, SV_FIFTH, SV_TWELFTH, SV_INTERVALS };

typedef struct {
    int16_t *buf, *ps;                    /* 0: no reverb (the effect outputs nothing) */
    int32_t base[SV_LINES], cap[SV_LINES], w[SV_LINES];
    float d[SV_LINES], g[SV_LINES], lp[SV_LINES], dmod[SV_LINES];
    float dcur[SV_LINES];                 /* the lengths now, slewing to d */
    float ps_inc;                         /* the shimmer's grain phase a sample: its interval */
    float t60, size, damp_c, mod, shimmer, level, shim_gain;
    float hp_c, hp_x1, hp_y1;             /* the send's low cut */
    float mod_ph;
    int32_t ps_w;
    float ps_ph, shim_out;
    int32_t quiet, ctrl_left;
    int32_t ds_ph, hw, yw;                /* the rate change: which half of a pair; the histories' heads */
    float hx[2 * SV_HB_N];                /* the send, the last SV_HB_N samples (twice: read in one run) */
    float hyl[2 * SV_HB_Y], hyr[2 * SV_HB_Y];   /* the return at the network's rate, likewise */
    float odd_l, odd_r;                   /* the second sample of the pair the last tick made */
    float pz[4];                          /* the shimmer feed's low-pass (two biquads, TDF-II) */
} spacevb_t;

void spacevb_init(spacevb_t *v, int16_t *buf, int16_t *ps);   /* buf: SV_TOTAL, ps: SV_PS_LEN */
/* settings, each 0..1 unless said */
void spacevb_decay(spacevb_t *v, float x);    /* 1 .. 40 s */
void spacevb_tone(spacevb_t *v, float x);     /* dark .. bright */
void spacevb_lowcut(spacevb_t *v, float hz);
void spacevb_level(spacevb_t *v, float x);    /* the return, 0 .. 1.2 */
void spacevb_size(spacevb_t *v, float x);     /* 0.5 .. 1.5 */
void spacevb_mod(spacevb_t *v, float x);
void spacevb_shimmer(spacevb_t *v, float x);
void spacevb_interval(spacevb_t *v, int i);   /* SV_OCTAVE, SV_FIFTH, SV_TWELFTH */
/* adds the return into out_l / out_r */
void spacevb_process(spacevb_t *v, const float *in, float *out_l, float *out_r, int n);
