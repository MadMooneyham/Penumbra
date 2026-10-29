/* SPDX-License-Identifier: GPL-3.0-only */
/* Penumbra's audio engine. engine_render() runs in the audio ISR; everything else is called from
 * the main loop and reaches the ISR only through the command queue, so the voices' state is only
 * ever touched by one context (and the second core's job, while the first waits for it).
 *
 * The five voices (docs: "Penumbra: rewrite design"): DRONE (five voices, SWARM; black keys 1-5 latch
 * them, the journey's director sets their chords), VOX (four voices: the white keys in PLAY, MIDI
 * notes, and the generator's sparse phrases, far back in the shimmer), PULSE (a deep kick and a tick:
 * the generator, black keys 6 and 7) and TEXTURE (the wind), into the shimmer reverb, the tape delay
 * and the master stage. The journey (seq/journey.c) runs in CTRL, its steps on the clock's 16ths, and
 * its scene and the HOME macros set the voices each block (conduct). Every stage of every block is
 * timed on the 24 MHz timer, per core, published once a second for the BENCH page.
 *
 * The render, per block:
 *   core 1: CTRL (commands, MIDI in, the clock and the journey's steps, the drone's control steps)
 *   core 2: DRN BUS (the drone's bus: wind, drive and filter, then SWARM, on the last block's
 *           voices), DRONE 2 (its share of this block's drone voices), VOX, PULSE, TEXTURE
 *   core 1: DRONE 1 (the rest of the drone voices), FX WET (the reverb and delay returns of the last
 *           block's sends), then WAIT for core 2, OUTPUT (the voices' gains and sends, the distortion
 *           and glue compressor, the master's compressor, filter and limiter, the output's scale)
 * The effects and the drone's bus run a block behind (5.8 ms), so each core works while the other
 * does. DRONE CORE says whose the drone voices are: all the first's, all the second's, or SPLIT (the
 * default): each block, the share of the sounding voices that has the two cores finish closest
 * together, from what the blocks before measured (engine.c drone_mask2). */
#pragma once
#include <stdint.h>
#include "../seq/clock.h"
#include "../seq/journey.h"

extern tclock_t eng_clock;
#define ENG_DRONES 5                           /* the drone voices (dsp/drone.h): black keys 1-5 */

/* the journey that plays (seq/journey.h): the main loop writes it a byte at a time (and loads a
 * whole one, then engine_journey_loaded); the ISR reads it, and writes its loops and its scene */
extern journey_t eng_jr;
const jrun_t *engine_jr(void);                 /* (main loop, read only) the director, the generator */

/* the voices, as the VOICES page sets them: LEVEL (100 = as made), TONE (64 = the scene's), their
 * character (DRONE detune, VOX attack, PULSE decay, TEXTURE sweep) and SPACE (their reverb and delay
 * sends, 64 = as made) */
enum { EV_DRONE, EV_VOX, EV_PULSE, EV_TEX, EV_N };
enum { VF_LEVEL, VF_TONE, VF_CHAR, VF_SEND, VF_N };
void engine_voice(int voice, int field, int value);
/* the effects, as the FX page sets them */
enum { EF_RV_DECAY, EF_RV_SIZE, EF_RV_SHIMMER, EF_RV_TONE, EF_DL_TIME, EF_DL_FDBK, EF_DL_TONE, EF_DL_WEAR, EF_N };
void engine_fx(int i, int value);
int engine_fx_max(int i);
const char *engine_fx_value_name(int i, int value);   /* a switch's label (the delay's time), else 0 */

/* the benchmark: what each stage cost, in % of the block's real-time budget x 10 (so 1000 = the
 * whole block), averaged over a second and its worst block in that second */
enum { ST_CTRL, ST_DRN1, ST_DRN2, ST_DRNBUS, ST_VOX, ST_PULSE, ST_TEX, ST_FXWET, ST_WAIT, ST_OUT,
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
extern volatile uint32_t eng_kicks;                         /* PULSE's kicks so far (the corona flares on each) */
extern volatile uint32_t eng_vox_notes;                     /* the journey's VOX notes so far */
extern uint32_t eng_out_clips;                              /* output samples past full scale, since power-on */

void engine_init(void);
void engine_render(int32_t *out_lr, uint32_t n);   /* ISR: n stereo frames (<= 256), 24-bit in int32 */
void engine_volume(int32_t *out_lr, uint32_t n);   /* ISR, after engine_render (and the USB tap) */

/* main loop -> ISR */
void engine_white_key(int w, int down, int oct);   /* a white key (0..15): by the journey's mode */
void engine_loops(int op, uint32_t mask);          /* LOOP_CAPTURE, LOOP_RELEASE (bit LP_*), LOOP_RECORD */
enum { LOOP_CAPTURE, LOOP_RELEASE, LOOP_RECORD };
void engine_travel(int scene);                     /* the director goes there */
void engine_journey_loaded(void);                  /* eng_jr is a new journey: start it from its scene */
void engine_drone_key(int voice);                  /* a black key: the voice by its MODE (HOLD: latch) */
int engine_drone_on(int voice);                    /* (main loop) the voice is on: latched, or in its swell */
float engine_drone_level(int voice);               /* (main loop) how loud it is now, 0..1 of its LEVEL (it rises, falls) */
void engine_vox_on(int note, int vel);             /* VOX: a note (a white key, MIDI), vel 0..127 */
void engine_vox_off(int note);
void engine_pulse(int which, int vel);             /* PULSE_KICK (0) or PULSE_TICK (1), vel 0..127 */
int engine_vox_sounding(void);                     /* (main loop) a VOX voice sounds */
void engine_load_test(int on);                     /* every voice at its heaviest: the worst case */
int engine_load_testing(void);
void engine_out_loud(int on);                      /* 1: the output at the codec's full scale */
void engine_usb_full(int on);                      /* 1: USB audio at full level, MASTER after it */
void engine_drone_core(int mode);                  /* DRONE_CORE1, DRONE_CORE2, DRONE_SPLIT */
void engine_midi_ch(int ch);                       /* VOX's channel in: 1..16, 0 = off */
uint32_t engine_quiet_ms(void);                    /* how long the output has been silent */
