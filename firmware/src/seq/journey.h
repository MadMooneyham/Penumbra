/* SPDX-License-Identifier: GPL-3.0-only */
/* Penumbra's journeys (docs: "Penumbra: rewrite design"): banks of places to play in, and the sound.
 * Nothing here plays on its own but the drone you latch: VOX is your hands' (and the looper's).
 *
 *   scene      a key and scale, the drone's pedal and how it voices it, VOX's brightness and register,
 *              the space, the wind's level and colour. Up to 8 a journey. PRESET moves to the next or
 *              the one before: everything glides there over JR_TRAVEL_S, the key changes at the middle.
 *   drone      the backbone: a pedal, the scene's voicing over its root (or the pedal it names, in
 *              its key), held the whole scene. A new scene moves the voices that must move, to the
 *              nearest note of their new one (the rest stay where they are), one at a time, the top
 *              first and the bass last, JR_DRONE_GAP seconds apart. A voice may come and go instead
 *              (dswell): swelled in every few bars while the clock runs, and played by its key.
 *
 * journey_t is the saved part (under 300 bytes): the main loop writes it, a byte at a time, and the
 * audio ISR reads it (one core: the ISR only interrupts, never overlaps). jrun_t is the ISR's alone.
 * Everything that sounds goes out through a jr_sink_t; nothing here knows the voices. */
#pragma once
#include <stdint.h>

#define JR_SCENES 8
#define JR_NAME 12
#define JR_PRESETS 6
#define JR_USER 8
#define JR_DRONES 5
#define JR_MAGIC 0x314A4E50u                    /* "PNJ1" */
#define JR_FORMAT 5u                            /* 4 (0.16, 0.17): no BLOOM; 3 (0.15): one VOX sound; 2 (0.14): no
                                                 * sound; 1 (0.13): three loops, LINE's first, no sound
                                                 * (settings.c converts them all) */
#define JR_VSOUNDS 4                            /* VOX's sounds: black keys 8 .. 11 */
#define JR_VS 16                                /* a VOX sound's bytes (engine.h VS_*) */
#define JR_DRONE_GAP 3.0f                       /* seconds between the drone's voices moving to a new scene */
#define JR_TRAVEL_S 12.0f                       /* seconds a move to another scene glides */
#define JR_VOICES 4                             /* the sound: DRONE, VOX, PULSE, TEXTURE (engine.h EV_*) */
#define JR_SHAPE 6                              /* each one's level, the SHAPE page's four knobs, one more (engine.h SH_*) */
#define JR_DVOICE 4                             /* each drone voice's own, its black key held (engine.h DV_*) */
#define JR_OLD_LOOPS 40                         /* (0.15's two captured loops: unused bytes now) */

enum { SC_IONIAN, SC_DORIAN, SC_PHRYGIAN, SC_LYDIAN, SC_MIXOLYDIAN, SC_AEOLIAN, SC_HARMONIC, NSCALES };
enum { VC_OPEN, VC_WARM, VC_HIGH, VC_CLUSTER, NVOICINGS };
enum { MAC_DRONE, MAC_VOX, MAC_SPACE, MAC_TONE, NMACROS };   /* HOME's knobs: 64 as made */

typedef struct {
    uint8_t root;                               /* pitch class: 0 C .. 11 B */
    uint8_t scale;                              /* SC_* */
    uint8_t prog;                               /* (0.15's chords: unused) */
    uint8_t voicing;                            /* VC_*: how the drone's five voices stack */
    uint8_t density, bright, swarm, space, tex, colour, pulse;   /* 0..127 (density, swarm, pulse: unused) */
    uint8_t reg;                                /* VOX's octave, less one: 2..6 (4: its root an octave over middle C's) */
    uint8_t drones;                             /* the drone voices PLAY latches (bit v) */
    uint8_t penta;                              /* (unused) */
    uint8_t pedal;                              /* the drone's root: 0 the key's, else 1 + a pitch class in the key */
    uint8_t rsv;
} scene_t;

typedef struct {
    uint32_t magic;
    uint16_t format, size;
    char name[JR_NAME];
    uint32_t seed;
    uint8_t nscenes, mode, at;                  /* (mode: unused) at: the scene it is in (saved: it carries on there) */
    uint8_t macro[NMACROS];
    uint8_t rsv;
    scene_t scene[JR_SCENES];
    uint8_t old_loops[JR_OLD_LOOPS];
    /* the sound (0..127; nothing here reads it but dswell: the engine does) */
    uint8_t shape[JR_VOICES][JR_SHAPE];
    uint8_t dvoice[JR_DRONES][JR_DVOICE];
    uint8_t dswell;                             /* the drone voices that come and go (bit v): swelled in now and
                                                 * then, a key plays one; the rest are held */
    uint8_t bloom[3];                           /* PULSE's BLOOM: LENGTH, HEIGHT (0..2), FIFTH (its level: PULSE's
                                                 * SH_HIDE) (engine.h BL_*) */
    uint8_t vsound[JR_VSOUNDS][JR_VS];          /* VOX's four sounds (shape[VOX] is unused: these are VOX's) */
    uint8_t vcur;                               /* the one playing */
    uint8_t rsv3[3];
} journey_t;

typedef struct {
    void *ctx;
    void (*drone_note)(void *ctx, int voice, int midi);
    void (*drone_latch)(void *ctx, uint32_t mask);        /* these voices on (the others as they are) */
    void (*drone_release)(void *ctx);                     /* every voice falls away */
    void (*drone_swell)(void *ctx, int voice);            /* a voice that comes and goes: in, then out */
} jr_sink_t;

/* the scene as it sounds now, gliding while it travels (0..127) */
typedef struct {
    float bright, space, tex, colour;
    int root_midi;                              /* the key's root in octave 1 (24..35): PULSE's tuning */
    float fade;                                 /* 0..1: PLAY fades in, STOP out */
} jr_out_t;

typedef struct {
    journey_t *j;
    uint8_t playing;
    uint8_t cur, from, dest, travelling, switched;
    float xf;
    int drone[JR_DRONES];                       /* the drone's notes (MIDI; 0: none yet) */
    int dtarget[JR_DRONES];                     /* where they are going */
    uint8_t dpend;                              /* the voices still to move (bit v) */
    float dwait;                                /* seconds to the next */
    uint8_t swell_wait[JR_DRONES];              /* the swelling voices: bars to the next */
    uint32_t srng;
    float macro_s[NMACROS];
    /* MORPH: the way to the next scene (or the one before) by hand: mbase .. mto, mamt of the way */
    uint8_t mbase, mto;
    float mamt;
    uint32_t sig;                               /* the scene's key, scale, pedal and voicing: a change moves the drone */
    jr_out_t out;
} jrun_t;

/* the journeys in the firmware */
void jr_preset(int i, journey_t *dst);
const char *jr_preset_name(int i);
void jr_sound_default(journey_t *j);             /* the sound as made (a preset's, an old journey's) */
void jr_vsound_default(journey_t *j);            /* VOX's four sounds as made */
void jr_bloom_default(journey_t *j);             /* BLOOM as made */
const char *jr_vsound_name(int i);               /* GLASS, REED, HALO, LOW */
int jr_valid(const journey_t *j);                /* loaded from flash: is it one */

/* the music's tables, for the UI */
const char *jr_scale_name(int s);
const char *jr_root_name(int pc);
const char *jr_voicing_name(int v);

/* audio ISR */
void jr_init(jrun_t *r, journey_t *j);
void jr_restart(jrun_t *r, const jr_sink_t *o);  /* a journey loaded: from its scene, as if PLAY started */
void jr_start(jrun_t *r, const jr_sink_t *o);    /* the clock starts: the scene's drones swell in */
void jr_stop(jrun_t *r, const jr_sink_t *o);     /* it stops: they fall away */
void jr_tick(jrun_t *r, uint32_t frames, const jr_sink_t *o);   /* each block: the glides, the drone's moves */
void jr_step(jrun_t *r, uint32_t n, const jr_sink_t *o);        /* each 16th (n since PLAY): the swells */
void jr_travel(jrun_t *r, int scene);             /* PRESET: go there */
/* MORPH (SELECT on HOME): detents toward the next scene (+) or the one before (-), JR_MORPH_STEPS the
 * way; past the middle its key, all the way: there (and the next turn heads on). Back: back */
#define JR_MORPH_STEPS 32
void jr_morph(jrun_t *r, int detents);
int jr_key_note(const jrun_t *r, int w, int oct); /* a white key's note in the scene's key */
/* the looper's notes: a played note as a degree of the scene's key over VOX's register (and the
 * semitones over that degree, 0..2: a note out of the key), 0..251; and back, in the scene playing now.
 * So a loop goes where the scene goes */
int jr_note_code(const jrun_t *r, int midi);
int jr_code_note(const jrun_t *r, int code);
void jr_scene_now(const jrun_t *r, scene_t *s);   /* the moment, as a scene (SCENE: store) */
