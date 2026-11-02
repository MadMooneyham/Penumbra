/* SPDX-License-Identifier: GPL-3.0-only */
/* The master section, after the send FX: DIP (a low-pass or a high-pass over the whole mix), volume,
 * limiter. See master.c. */
#pragma once
#include <stdint.h>

enum { MST_OPEN, MST_LP, MST_HP };
typedef struct {
    uint8_t dip;                      /* 60 .. 68 = open; under 60 the low-pass closes, over 68 the high-pass */
    uint8_t mode;                     /* MST_* */
    /* the filter */
    float g, g_t, k;
    float a1;                         /* 1 / (1 + g (g + k)): recomputed only while g moves */
    float ic1, ic2;
    float ic1r, ic2r;                 /* the right side's state */
    float vol_cur;                    /* the volume, gliding to the knob (< 0: unset) */
    /* limiter: looks MST_LA samples ahead, so it can lower the gain before a peak arrives */
#define MST_LA 64
    float env, gain, inv_env;         /* inv_env = 1 / env, kept up as env moves (no divide a sample) */
    float la[MST_LA], la_r[MST_LA];
    int la_pos, hold;
} master_t;

void master_init(master_t *m);
/* DIP 0..127: 60 .. 68 open; under 60 a low-pass closing, from 18 kHz at 59 to ~50 Hz at 0; over 68 a
 * high-pass opening, from 30 Hz at 69 to ~10 kHz at 127 */
void master_dip(master_t *m, int v);
/* in place, stereo: the filter on each side, the volume (0..1+), the limiter on the louder side,
 * which moves both together */
void master_process_st(master_t *m, float *l, float *r, int n, float volume);
