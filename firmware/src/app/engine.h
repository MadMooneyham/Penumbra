/* SPDX-License-Identifier: GPL-3.0-only */
/* Penumbra's audio engine. engine_render() runs in the audio ISR; everything else is called from
 * the main loop and reaches the ISR only through the command queue, so the voices' state is only
 * ever touched by one context (and the second core's job, while the first waits for it).
 *
 * The voices (docs: "Penumbra: rewrite design"): DRONE (five voices, SWARM; black keys 1-5 latch
 * them, the journey holds them on its pedal), VOX (four voices: the white keys in PLAY, MIDI
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
#include "../seq/looper.h"

extern tclock_t eng_clock;
#define ENG_DRONES 5                           /* the drone voices (dsp/drone.h): black keys 1-5 */

/* the journey that plays (seq/journey.h): the main loop writes it a byte at a time (and loads a
 * whole one, then engine_journey_loaded); the ISR reads it, and writes its loops and its scene */
extern journey_t eng_jr;
const jrun_t *engine_jr(void);                 /* (main loop, read only) the director, the generator */

/* the sound, saved with the journey (eng_jr.shape, eng_jr.dvoice), read by the ISR each block (conduct).
 * SHAPE: each voice's LEVEL (100 = as made) and four knobs (64 = as made where it is an offset):
 *   DRONE    FILTER, BODY (resonance), SWARM (the crowd, and the voices' detune with it), WAVE (soft .. reedy)
 *   VOX      BRIGHT (over the scene's), ATTACK, RELEASE, SHIMMER (how far into the reverb)
 *   PULSE    DROP (the kick's fall of pitch), DECAY, TICK, SPACE
 *   TEXTURE  COLOUR (over the scene's), SWEEP, RATE, WIDTH
 * SH_HIDE is set only by a held note's SAVE: VOX's vibrato. Each drone voice, its black key held:
 * LEVEL, TONE, DETUNE (64: none), OCTAVE (0 down, 1 as the journey has it, 2 up) */
enum { EV_DRONE, EV_VOX, EV_PULSE, EV_TEX, EV_N };
enum { SH_LEVEL, SH_K1, SH_K2, SH_K3, SH_K4, SH_HIDE, SH_N };
enum { DV_LEVEL, DV_TONE, DV_DETUNE, DV_OCT, DV_N };
_Static_assert(EV_N == JR_VOICES && SH_N == JR_SHAPE && DV_N == JR_DVOICE, "the journey keeps the sound");
/* VOX's sounds (eng_jr.vsound[vcur]: VOX's row of shape is unused): its SHAPE pages are VOX (BRIGHT,
 * ATTACK, RELEASE, SHIMMER), VOX 2 (WAVE, FILTER: low-pass .. band-pass, RESO, GLIDE) and VOX 3 (DETUNE,
 * B OCTAVE, B MIX, ENV: how far its envelope opens the filter); VIBRATO a held note's SAVE sets */
enum { VS_LEVEL, VS_BRIGHT, VS_ATTACK, VS_RELEASE, VS_SHIMMER, VS_VIB, VS_WAVE, VS_FILTER, VS_RESO, VS_GLIDE,
       VS_DETUNE, VS_BOCT, VS_BMIX, VS_ENV, VS_N };
_Static_assert(VS_N <= JR_VS, "a VOX sound fits its bytes");
/* the hands' moves, on top of the sound and gone when they spring back (the UI glides them):
 *   THROWS (FX held)        SHIMMER, FEEDBACK (the delay's), FREEZE (the reverb holds, the sends close),
 *                           DIP (the whole mix's low-pass): 0 none .. 127 all the way
 *   BENDS (a white key held) VOX's BRIGHT, VIBRATO, SHIMMER, RELEASE: -64 .. +64 */
enum { PF_SHIMMER, PF_FDBK, PF_FREEZE, PF_DIP, PF_BRIGHT, PF_VIB, PF_VSEND, PF_VREL, PF_N };
void engine_perf(int i, int value);
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
extern volatile uint32_t eng_vox_notes;                     /* VOX's notes so far (the hands', the looper's) */
extern uint32_t eng_out_clips;                              /* output samples past full scale, since power-on */

void engine_init(void);
void engine_render(int32_t *out_lr, uint32_t n);   /* ISR: n stereo frames (<= 256), 24-bit in int32 */
void engine_volume(int32_t *out_lr, uint32_t n);   /* ISR, after engine_render (and the USB tap) */

/* main loop -> ISR */
void engine_white_key(int w, int down, int oct);   /* a white key (0..15): VOX in the scene's key */
void engine_looper(int op);                        /* seq/looper.h: REC, PLAY, UNDO, CLEAR */
enum { LOOPER_REC, LOOPER_PLAY, LOOPER_UNDO, LOOPER_CLEAR };
const looper_t *engine_looper_state(void);         /* (main loop, read only) */
void engine_freeze(void);                          /* the notes held keep sounding (again: they let go) */
int engine_frozen(void);                           /* (main loop) notes frozen */
void engine_travel(int scene);                     /* the scene glides there */
void engine_morph(int detents);                    /* MORPH: toward the next scene (+), the one before (-) */
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
