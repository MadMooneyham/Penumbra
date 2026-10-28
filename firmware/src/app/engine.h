/* SPDX-License-Identifier: GPL-3.0-only */
/* Penumbra's audio engine. engine_render() runs in the audio ISR; everything else is called from
 * the main loop and reaches the ISR only through the command queue, so the voices' state is only
 * ever touched by one context (and the second core's job, while the first waits for it).
 *
 * Phase 1 (docs: "Penumbra: rewrite design"): the kept sound code on a fixed test scene. DRONE
 * (five voices, SWARM) on the black keys, the melody voice standing in for LINE on the white keys
 * and MIDI notes, the shimmer reverb, the tape delay and the master stage. Its real job is the
 * benchmark: every stage of every block timed on the 24 MHz timer, per core, published once a
 * second for the BENCH page.
 *
 * The render, per block:
 *   core 1: CTRL (commands, MIDI in, the clock, the drone's control steps for the block)
 *   core 2: DRN FILT and SWARM (the drone's bus: wind, drive and filter, then SWARM, on the last
 *           block's voices), DRONE 2 (its share of this block's drone voices), LINE
 *   core 1: DRONE 1 (the rest of the drone voices), FX WET (the reverb and delay returns of the last
 *           block's sends), then WAIT for core 2, MIX (the voices' gains and sends, the distortion and
 *           glue compressor), MASTER (compressor, filter, limiter, the output's scale)
 * The effects and the drone's bus run a block behind (5.8 ms), so each core works while the other
 * does. DRONE CORE says whose the drone voices are: all the first's, all the second's, or SPLIT (the
 * default): the second core takes half the sounding oscillators, which with the bus and LINE beside
 * them balances the first's effects returns. */
#pragma once
#include <stdint.h>
#include "../seq/clock.h"

extern tclock_t eng_clock;
#define ENG_DRONES 5                           /* the drone voices (dsp/drone.h): black keys 1-5 */

/* the benchmark: what each stage cost, in % of the block's real-time budget x 10 (so 1000 = the
 * whole block), averaged over a second and its worst block in that second */
enum { ST_CTRL, ST_DRN1, ST_DRN2, ST_DRNFILT, ST_SWARM, ST_LINE, ST_FXWET, ST_WAIT, ST_MIX, ST_MASTER,
       ST_CORE1, ST_CORE2, ST_N };
enum { DRONE_CORE1, DRONE_CORE2, DRONE_SPLIT };
typedef struct {
    uint16_t avg[ST_N], peak[ST_N];
    uint32_t blocks;                   /* blocks in the window */
    uint8_t two_cores, drone_core;     /* how the window ran */
} bench_t;
uint32_t engine_bench(bench_t *out);   /* main loop: the last second's numbers; returns their serial (0: none yet) */
const char *engine_stage_name(int st);

/* overload guard: the platform reports each block's load after rendering it */
void engine_load(uint32_t pct, uint32_t frames);           /* ISR, after engine_render */
void engine_guard_enable(int on);                           /* off: full quality always (the load test) */
int engine_guard_active(void);
extern volatile uint32_t eng_guard_count;                   /* times it engaged */
extern volatile uint16_t eng_level;                         /* the output's peak, Q15, held for the UI to fall */
extern volatile uint32_t eng_steps;                         /* 16ths since PLAY (the clock's), for the screen */
extern uint32_t eng_out_clips;                              /* output samples past full scale, since power-on */

void engine_init(void);
void engine_render(int32_t *out_lr, uint32_t n);   /* ISR: n stereo frames (<= 256), 24-bit in int32 */
void engine_volume(int32_t *out_lr, uint32_t n);   /* ISR, after engine_render (and the USB tap) */

/* main loop -> ISR */
void engine_drone_key(int voice);                  /* a black key: the voice by its MODE (HOLD: latch) */
int engine_drone_on(int voice);                    /* (main loop) the voice is on: latched, or in its swell */
float engine_drone_level(int voice);               /* (main loop) how loud it is now, 0..1 of its LEVEL (it rises, falls) */
void engine_line_on(int note);                     /* LINE: a note (a white key, MIDI); legato glides */
void engine_line_off(int note);
void engine_load_test(int on);                     /* every drone latched thick, LINE running: the worst case */
int engine_load_testing(void);
void engine_out_loud(int on);                      /* 1: the output at the codec's full scale */
void engine_usb_full(int on);                      /* 1: USB audio at full level, MASTER after it */
void engine_drone_core(int mode);                  /* DRONE_CORE1, DRONE_CORE2, DRONE_SPLIT */
void engine_midi_ch(int ch);                       /* LINE's channel in: 1..16, 0 = off */
uint32_t engine_quiet_ms(void);                    /* how long the output has been silent */
