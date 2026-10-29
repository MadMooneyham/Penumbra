/* SPDX-License-Identifier: GPL-3.0-only */
/* Penumbra's journeys (docs: "Penumbra: rewrite design", generative model): what plays when nobody
 * plays it, and how you steer it.
 *
 *   scene      a key and scale, a chord progression, the drone's pedal and how it voices it, how
 *              much plays, VOX's brightness, the space, the wind's level and colour, the pulse,
 *              VOX's register. Up to 8 a journey.
 *   director   moves through the moon's phases, NEW (drones only, nearly) -> WAXING -> FULL (the
 *              densest) -> WANING, each 30 s .. 4 min by the DRIFT macro (0: it stays where it is);
 *              at each NEW it travels to the next scene: everything glides over the phase, the key
 *              and chords change at its middle, in the quiet. The chord moves every 2 .. 8 bars.
 *   drone      the backbone: a pedal, the scene's voicing over its root (or the pedal it names, in
 *              its key), held the whole scene. The chords move only VOX. A new scene moves the voices
 *              that must move, to the nearest note of their new one (the rest stay where they are),
 *              one at a time, the top first and the bass last, JR_DRONE_GAP seconds apart. A voice
 *              may come and go instead (dswell): swelled in every few bars, more often as the moon fills.
 *   generator  writes from the journey's seed and the scene's scale: VOX's phrases (one to three long
 *              notes over eight bars, then the space they ring out into) and PULSE's pattern; repeats
 *              them, then changes them a little (a note a step) and, after a few changes, writes new
 *              ones. Notes are scale degrees over the chord's root, so a phrase follows the chords and
 *              the key. The same seed and the same hands give the same music. How much of a phrase
 *              plays follows the moon and DENSITY: at the new moon, the drones alone.
 *   loops      a captured phrase (or VOX recorded from the keys) plays instead of the generator until
 *              it is released; MUTATE lets it change a little each time round.
 *   modes      PLAY: the white keys play VOX; while they do (and 4 bars after), the generator's VOX
 *              waits. GUIDE: held white keys are the notes VOX favours, a tapped one is where the next
 *              phrase heads, low keys pull the register down, high up. DRIFT: each white key is a
 *              scene, and the director travels there.
 *
 * journey_t is the saved part (under 300 bytes): the main loop writes it, a byte at a time, and the
 * audio ISR reads it (one core: the ISR only interrupts, never overlaps). jrun_t is the ISR's alone.
 * Everything that sounds goes out through a jr_sink_t; nothing here knows the voices. */
#pragma once
#include <stdint.h>

#define JR_SCENES 8
#define JR_STEPS 16
#define JR_NAME 12
#define JR_PRESETS 6
#define JR_USER 8
#define JR_DRONES 5
#define JR_MAGIC 0x314A4E50u                    /* "PNJ1" */
#define JR_FORMAT 3u                            /* 2 (0.14): no sound; 1 (0.13): three loops, LINE's first, no
                                                 * sound (settings.c converts both) */
#define JR_HANDS_STEPS 64u                      /* PLAY: the generator's VOX waits 4 bars after a played note */
#define JR_DRONE_GAP 3.0f                       /* seconds between the drone's voices moving to a new scene */
#define JR_VOICES 4                             /* the sound: DRONE, VOX, PULSE, TEXTURE (engine.h EV_*) */
#define JR_SHAPE 6                              /* each one's level, the SHAPE page's four knobs, one more (engine.h SH_*) */
#define JR_DVOICE 4                             /* each drone voice's own, its black key held (engine.h DV_*) */

enum { SC_IONIAN, SC_DORIAN, SC_PHRYGIAN, SC_LYDIAN, SC_MIXOLYDIAN, SC_AEOLIAN, SC_HARMONIC, NSCALES };
enum { VC_OPEN, VC_WARM, VC_HIGH, VC_CLUSTER, NVOICINGS };
enum { MODE_PLAY, MODE_GUIDE, MODE_DRIFT, NMODES };
enum { MAC_DRIFT, MAC_DENSITY, MAC_SPACE, MAC_TONE, NMACROS };
enum { MOON_NEW, MOON_WAX, MOON_FULL, MOON_WANE, NMOONS };
enum { LP_VOX, LP_PULSE, JR_LOOPS };
enum { LOOP_OFF, LOOP_CAPTURED, LOOP_RECORDED };   /* loop_t.on: recorded plays as played (no pentatonic) */

/* a loop's step. VOX: ST_ON, ST_TIE (held on from the step before), the degree
 * over the chord's root (-24 .. +39). PULSE: PS_KICK, PS_TICK, PS_ACC */
#define ST_ON 0x80u
#define ST_TIE 0x40u
#define ST_DEG(s) ((int)((s) & 0x3Fu) - 24)
#define ST_MK(deg) ((uint8_t)(((deg) + 24) & 0x3F))
#define PS_KICK 1u
#define PS_TICK 2u
#define PS_ACC 4u

typedef struct {
    uint8_t root;                               /* pitch class: 0 C .. 11 B */
    uint8_t scale;                              /* SC_* */
    uint8_t prog;                               /* the chords (jr_prog_name) */
    uint8_t voicing;                            /* VC_*: how the drone's five voices stack each chord */
    uint8_t density, bright, swarm, space, tex, colour, pulse;   /* 0..127 */
    uint8_t reg;                                /* VOX's octave, less one: 2..6 (4: its root an octave over middle C's) */
    uint8_t drones;                             /* the drone voices PLAY latches (bit v) */
    uint8_t penta;                              /* 1: VOX's phrases keep to the scale's pentatonic */
    uint8_t pedal;                              /* the drone's root: 0 the key's, else 1 + a pitch class in the key */
    uint8_t rsv;
} scene_t;

typedef struct {
    uint8_t step[JR_STEPS];
    uint8_t len;                                /* 1..16 */
    uint8_t div;                                /* 16ths a step: 1, 2, 4, 8 */
    uint8_t on;                                 /* LOOP_*: it plays instead of the generator */
    uint8_t mutate;                             /* 0..127: how much it changes each time round */
} loop_t;

typedef struct {
    uint32_t magic;
    uint16_t format, size;
    char name[JR_NAME];
    uint32_t seed;
    uint8_t nscenes, mode, at;                  /* at: the scene it is in (saved: it carries on there) */
    uint8_t macro[NMACROS];
    uint8_t rsv;
    scene_t scene[JR_SCENES];
    loop_t loop[JR_LOOPS];
    /* the sound (0..127; nothing here reads it: the engine does) */
    uint8_t shape[JR_VOICES][JR_SHAPE];
    uint8_t dvoice[JR_DRONES][JR_DVOICE];
    uint8_t dswell;                             /* the drone voices that come and go (bit v): the director swells
                                                 * them in now and then, a key plays one; the rest are held */
    uint8_t rsv2[3];
} journey_t;

typedef struct {
    void *ctx;
    void (*drone_note)(void *ctx, int voice, int midi);
    void (*drone_latch)(void *ctx, uint32_t mask);        /* these voices on (the others as they are) */
    void (*drone_release)(void *ctx);                     /* every voice falls away */
    void (*drone_swell)(void *ctx, int voice);            /* a voice that comes and goes: in, then out */
    void (*vox_on)(void *ctx, int midi, int vel);         /* vel 0..127 */
    void (*vox_off)(void *ctx, int midi);
    void (*pulse)(void *ctx, int which, int vel);         /* 0 kick, 1 tick; vel 0..127 */
} jr_sink_t;

/* the scene as it sounds now, gliding while the director travels (0..127), and the moon */
typedef struct {
    float density, bright, swarm, space, tex, colour, pulse;
    float intensity;                            /* the moon's: 0.12 NEW .. 1 FULL */
    float eff;                                  /* how much plays: intensity x scene density x DENSITY */
    int root_midi;                              /* the key's root in octave 1 (24..35): PULSE's tuning */
    float fade;                                 /* 0..1: PLAY fades in, STOP out */
} jr_out_t;

typedef struct {
    loop_t m;                                   /* the motif */
    uint8_t rank[JR_STEPS];                     /* the order its steps join as it fills (0: first) */
    uint32_t rng;
    uint8_t rep, reps, muts, band;
    uint32_t count;                             /* motifs written: each one's seed */
} jr_gen_t;

typedef struct {
    journey_t *j;
    uint8_t playing;
    uint8_t cur, from, dest, travelling, switched;
    float xf, travel_s;
    uint8_t moon;
    float moon_t, moon_len;
    float intensity;
    uint8_t chord;
    uint32_t bar;
    int drone[JR_DRONES];                       /* the drone's notes (MIDI; 0: none yet) */
    int dtarget[JR_DRONES];                     /* where they are going */
    uint8_t dpend;                              /* the voices still to move (bit v) */
    float dwait;                                /* seconds to the next */
    uint8_t swell_wait[JR_DRONES];              /* the swelling voices: bars to the next */
    uint32_t srng;
    jr_gen_t g[JR_LOOPS];
    uint32_t lrng[JR_LOOPS];                    /* the captured loops' mutations */
    int vox_note;                               /* the generator's, sounding (-1: none) */
    uint8_t vox_ok;                             /* the last step played (a tie after a thinned step plays anew) */
    uint8_t hands;                              /* a VOX note was played since PLAY */
    uint32_t hand_n, now_n;                     /* the step it was played in; this step */
    /* GUIDE */
    uint32_t held;                              /* white keys held (bit w) */
    int8_t target, shift;                       /* a tapped key's degree (-128: none); the register's pull */
    /* VOX recorded from the keys (PLAY): 1 armed (from the next bar), 2 recording */
    uint8_t rec, rec_pos;
    int rec_held, rec_pending;
    uint32_t since_step, step_len;
    float macro_s[NMACROS];
    uint32_t sig;                               /* the scene's key, scale, pedal and voicing: a change moves the drone */
    jr_out_t out;
} jrun_t;

/* the journeys in the firmware */
void jr_preset(int i, journey_t *dst);
const char *jr_preset_name(int i);
void jr_sound_default(journey_t *j);             /* the sound as made (a preset's, an old journey's) */
void jr_blank(journey_t *j);                     /* the first preset, named NEW (the power-on journey) */
int jr_valid(const journey_t *j);                /* loaded from flash: is it one */

/* the music's tables, for the UI */
const char *jr_scale_name(int s);
const char *jr_root_name(int pc);
const char *jr_prog_name(int p);
int jr_nprogs(void);
const char *jr_voicing_name(int v);
const char *jr_moon_name(int m);
const char *jr_mode_name(int m);

/* audio ISR */
void jr_init(jrun_t *r, journey_t *j);
void jr_restart(jrun_t *r, const jr_sink_t *o);  /* a journey loaded: from its scene, as if PLAY started */
void jr_start(jrun_t *r, const jr_sink_t *o);
void jr_stop(jrun_t *r, const jr_sink_t *o);
void jr_tick(jrun_t *r, uint32_t frames, const jr_sink_t *o);   /* each block: the director, the glides */
void jr_step(jrun_t *r, uint32_t n, uint32_t step_frames, const jr_sink_t *o);   /* each 16th (n since PLAY) */
void jr_travel(jrun_t *r, int scene);             /* DRIFT's keys, the SCENE page: go there */
void jr_guide(jrun_t *r, int w, int down);        /* GUIDE: a white key (0..15) */
int jr_key_note(const jrun_t *r, int w, int oct); /* PLAY: a white key's note in the scene's key */
void jr_vox_played(jrun_t *r, int midi, int on);  /* a VOX note from the keys or MIDI (recording) */
void jr_record(jrun_t *r);                        /* arm VOX's recording (again: stop it) */
void jr_capture(jrun_t *r, uint32_t mask);        /* the generator's motifs (bit LP_*) become loops */
void jr_release(jrun_t *r, uint32_t mask);
void jr_scene_now(const jrun_t *r, scene_t *s);   /* the moment, as a scene (SCENE: store) */
