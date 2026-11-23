/* SPDX-License-Identifier: GPL-3.0-only */
/* Penumbra's journeys (docs: "Penumbra: rewrite design"): banks of places to play in, and the sound.
 * Nothing here plays on its own but the drone, and VOX's sounds set to sing (sing): else VOX is your
 * hands' (and the looper's).
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
#define JR_PRESETS 7
#define JR_TRAVEL_N 5                           /* travel's settings: not, every 8, 16, 32, 64 bars */
#define JR_USER 8
#define JR_DRONES 5
#define JR_MAGIC 0x314A4E50u                    /* "PNJ1" */
#define JR_STAY_N 7                             /* a scene's STAYS: as TRAVELS, 4, 8, 16, 32, 64, 128 bars */
#define JR_FORMAT 12u                           /* 11 (1.8 .. 1.10): no scene of its own length, none skipped (their
                                                 * bytes 0.15's: cleared); 10 (1.6, 1.7): no kick of its own (TUNE, DRIVE, DUCK, by itself,
                                                 * landing); 9 (1.2 .. 1.5): no singers' places, nor BLOOM by itself; 8 (1.2's
                                                 * second and third betas): one SWAY for all and PAN 2's
                                                 * switches, no RATE; 7 (its first): no PAN 2; 6 (1.1): nor PAN or
                                                 * SWAY; 5 (1.0): nor ROOM, HALO, drone motion or TIDES (settings.c
                                                 * converts them all); the betas' 1 .. 4 do not load */
#define JR_VSOUNDS 4                            /* VOX's sounds: black keys 8 .. 11 */
#define JR_VS 16                                /* a VOX sound's bytes (engine.h VS_*) */
#define JR_DRONE_GAP 3.0f                       /* seconds between the drone's voices moving to a new scene */
#define JR_TRAVEL_S 12.0f                       /* seconds a move to another scene glides */
#define JR_VOICES 4                             /* the sound: DRONE, VOX, PULSE, TEXTURE (engine.h EV_*) */
#define JR_SHAPE 6                              /* each one's level, the SHAPE page's four knobs, one more (engine.h SH_*) */
#define JR_DVOICE 4                             /* each drone voice's own, its black key held (engine.h DV_*) */
#define JR_DMOTION 4                            /* the drone's movement (engine.h DM_*) */
#define JR_TIDE 5                               /* TIDES (engine.h TD_*) */
#define JR_SING_N 4                             /* a sound's singing: not, rarely, now and then, often */
#define JR_PAN JR_VOICES                        /* each voice's place in the field (64: the middle) */
#define JR_KICK 4                               /* the kick's own: TUNE, DRIVE, DUCK, AUTO */
enum { JR_KK_TUNE, JR_KK_DRIVE, JR_KK_DUCK, JR_KK_AUTO };
#define JR_KTUNE_N 4                            /* the root, the fourth, the fifth, the octave */
enum { JR_KA_HAND, JR_KA_HEART, JR_KA_BAR, JR_KA_HALF, JR_KA_TWO, JR_KA_N };   /* the kick by itself: by hand,
                                                 * a heartbeat each bar, every bar, every half bar, every two bars */

enum { SC_IONIAN, SC_DORIAN, SC_PHRYGIAN, SC_LYDIAN, SC_MIXOLYDIAN, SC_AEOLIAN, SC_HARMONIC, NSCALES };
enum { VC_OPEN, VC_WARM, VC_HIGH, VC_CLUSTER, NVOICINGS };
enum { MAC_DRONE, MAC_VOX, MAC_SPACE, MAC_TONE, NMACROS };   /* HOME's knobs: 64 as made */

typedef struct {
    uint8_t root;                               /* pitch class: 0 C .. 11 B */
    uint8_t scale;                              /* SC_* */
    uint8_t stays;                              /* how long it stays, travelling by itself: 0 as TRAVELS, else
                                                 * JR_STAY_BARS[stays] bars (format 11 and before: 0.15's chords, unused) */
    uint8_t voicing;                            /* VC_*: how the drone's five voices stack */
    uint8_t room;                               /* the reverb's size and decay over the FX page's (64: as set) */
    uint8_t bright;
    uint8_t halo;                               /* the shimmer over the FX page's (64: as set) */
    uint8_t space, tex, colour, pulse;          /* 0..127 (pulse: unused) */
    uint8_t reg;                                /* VOX's octave, less one: 2..6 (4: its root an octave over middle C's) */
    uint8_t drones;                             /* the drone voices PLAY latches (bit v) */
    uint8_t skip;                               /* 1: SKIPPED, travelling, PRESET and MORPH pass it by (0: it plays)
                                                 * (format 11 and before: unused) */
    uint8_t pedal;                              /* the drone's root: 0 the key's, else 1 + a pitch class in the key */
    uint8_t blend;                              /* the sound's place between A and B in this scene: 0 it stays as
                                                 * it is; 1 (A) .. 127 (B): it glides there with the scene */
} scene_t;

typedef struct {
    uint32_t magic;
    uint16_t format, size;
    char name[JR_NAME];
    uint32_t seed;
    uint8_t nscenes, mode, at;                  /* (mode: unused) at: the scene it is in (saved: it carries on there) */
    uint8_t macro[NMACROS];
    uint8_t travel;                             /* it travels by itself: 0 not, else to the next scene after
                                                 * JR_TRAVEL_BARS[travel] bars in each */
    scene_t scene[JR_SCENES];
    uint8_t dmotion[JR_DMOTION];                /* the drone's own movement: RATE, SWEEP, WIDTH, DRIFT (engine.h DM_*) */
    uint8_t tide[JR_TIDE];                      /* TIDES: RATE, DRONE, SPACE, VOX, SHAPE (engine.h TD_*) */
    uint8_t pan[JR_PAN];                        /* PAN: DRONE, VOX, PULSE (its BLOOM too), TEXTURE; 0 left .. 127 right */
    uint8_t sway_rate;                          /* PAN's ALGO: 0 the sway on the tide, else its own cycle (60 s .. 0.5 s) */
    uint8_t sway[JR_PAN];                       /* PAN 2: how far each part sways, as pan[] (0 still; VOX: its echoes half) */
    uint8_t sway_off;                           /* PAN 2's ALGO: 0 all in sync .. 127 each half a cycle behind the one before */
    uint8_t sing[4][JR_VSOUNDS];                /* each VOX sound singing by itself (SHAPE's VOX SINGS): JR_SG_*,
                                                 * [JR_SG_HOW] 0 not, 1 rarely, 2 now and then, 3 often; [PLACE] its
                                                 * place (64 the middle, over VOX's); [DRIFT] 0 still .. 127 how far
                                                 * it sways round it on the tide; [OCT] 0 down, 1 as is, 2 up */
    uint8_t bloom_auto;                         /* BLOOM by itself, now and then: 0 not, 1 rarely .. 3 often */
    uint8_t kick[JR_KICK];                      /* the kick (its key held, SHAPE's PULSE 2): JR_KK_*: TUNE (0 the root,
                                                 * 1 the fourth, 2 the fifth, 3 the octave), DRIVE, DUCK (how far each
                                                 * kick dips the rest), AUTO (by itself: JR_KA_*) (format 10 and before:
                                                 * spare, set as made) */
    /* the sound (0..127; nothing here reads it but dswell: the engine does) */
    uint8_t shape[JR_VOICES][JR_SHAPE];
    uint8_t dvoice[JR_DRONES][JR_DVOICE];
    uint8_t dswell;                             /* the drone voices that come and go (bit v): swelled in now and
                                                 * then, a key plays one; the rest are held */
    uint8_t bloom[3];                           /* PULSE's BLOOM: LENGTH, HEIGHT (0..2), FIFTH (its level: PULSE's
                                                 * SH_HIDE) (engine.h BL_*) */
    uint8_t vsound[JR_VSOUNDS][JR_VS];          /* VOX's four sounds (shape[VOX] is unused: these are VOX's) */
    uint8_t vcur;                               /* the one playing */
    uint8_t vsing, vsing_snd;                   /* (1.5's: one sound singing, at that level; settings.c moves it
                                                 * into sing, and they stay 0) */
    uint8_t kick_land;                          /* 1: a kick lands as each new scene's key comes in */
} journey_t;
_Static_assert(JR_DMOTION + JR_TIDE + 2 * JR_PAN + 2 + 4 * JR_VSOUNDS + 1 + JR_KICK == 40, "the kick: in the spare bytes");
enum { JR_SG_HOW, JR_SG_PLACE, JR_SG_DRIFT, JR_SG_OCT };
#define JR_SING(j, s) ((int)(j)->sing[JR_SG_HOW][(s) % JR_VSOUNDS])

/* a journey's sound alone, without its scenes: what SHAPE, the holds and TIDES set (A/B and the sound
 * library keep these). All bytes, laid out as in journey_t */
typedef struct {
    uint8_t shape[JR_VOICES][JR_SHAPE];
    uint8_t dvoice[JR_DRONES][JR_DVOICE];
    uint8_t dswell;
    uint8_t bloom[3];
    uint8_t vsound[JR_VSOUNDS][JR_VS];
    uint8_t vcur;
    uint8_t dmotion[JR_DMOTION];
    uint8_t tide[JR_TIDE];
} jr_sound_t;
_Static_assert(sizeof(jr_sound_t) == 122, "a sound: bytes only");
void jr_sound_get(const journey_t *j, jr_sound_t *s);
void jr_sound_put(journey_t *j, const jr_sound_t *s);
/* the way from a to b, t of it (0 .. 1): each value glides; a switch (an octave, HELD or SWELL, the VOX
 * sound playing) changes at the middle */
void jr_sound_mix(jr_sound_t *out, const jr_sound_t *a, const jr_sound_t *b, float t);

typedef struct {
    void *ctx;
    void (*drone_note)(void *ctx, int voice, int midi);
    void (*drone_latch)(void *ctx, uint32_t mask);        /* these voices on (the others as they are) */
    void (*drone_release)(void *ctx);                     /* every voice falls away */
    void (*drone_swell)(void *ctx, int voice);            /* a voice that comes and goes: in, then out */
    void (*sing)(void *ctx, int midi, int vel, int snd);  /* VOX singing by itself: a note on (vel 0: off) */
    void (*bloom)(void *ctx);                             /* BLOOM by itself */
} jr_sink_t;

/* the scene as it sounds now, gliding while it travels (0..127) */
typedef struct {
    float bright, space, tex, colour, room, halo;
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
    /* VOX singing, each sound its own: beats to its next note, notes left in the phrase, beats its note has
     * left (0: none on), that note, the white key (degree) it is on */
    uint8_t sing_wait[JR_VSOUNDS], sing_left[JR_VSOUNDS], sing_hold[JR_VSOUNDS], sing_note[JR_VSOUNDS],
        sing_w[JR_VSOUNDS];
    uint8_t bloom_wait;                         /* BLOOM by itself: bars to the next */
    uint16_t bars_here;                         /* bars in this scene (it travels by itself: on at JR_TRAVEL_BARS) */
    uint32_t srng;
    float macro_s[NMACROS];
    /* MORPH: the way to the next scene (or the one before) by hand: mbase .. mto, mamt of the way */
    uint8_t mbase, mto;
    int8_t mdir;                                /* MORPH's way: +1 toward the next scene that plays, -1 the one before */
    float mamt;
    uint32_t sig;                               /* the scene's key, scale, pedal and voicing: a change moves the drone */
    jr_out_t out;
} jrun_t;

/* the journeys in the firmware */
void jr_preset(int i, journey_t *dst);
/* a preset with two sounds of its own (TOTALITY: its scenes move between them): 1, and they are in a and b;
 * such a one brings its whole sound when it is opened. 0: it has not */
int jr_preset_ab(int i, jr_sound_t *a, jr_sound_t *b);
extern const uint8_t JR_TRAVEL_BARS[JR_TRAVEL_N];
extern const uint8_t JR_STAY_BARS[JR_STAY_N];
int jr_plays(const journey_t *j, int scene);      /* 1: the scene plays (not skipped; or every one is) */
int jr_next_playing(const journey_t *j, int from, int dir);   /* the next scene that plays, dir +1 / -1 (from, if none) */
const char *jr_preset_name(int i);
void jr_sound_default(journey_t *j);             /* the sound as made (a preset's, an old journey's) */
void jr_sing_default(journey_t *j);              /* no sound singing, each in the middle, still, as is; no BLOOM by itself */
void jr_vsound_default(journey_t *j);            /* VOX's four sounds as made */
void jr_bloom_default(journey_t *j);             /* BLOOM as made */
void jr_kick_default(journey_t *j);              /* the kick's sound as made: the root, a little DRIVE, no DUCK (its
                                                  * playing by itself and landing: jr_sing_default's) */
void jr_motion_default(journey_t *j);            /* the drone's movement and TIDES as made */
void jr_pan_default(journey_t *j);               /* PAN as made: every voice in the middle, on the tide; PAN 2's too */
void jr_sway_default(journey_t *j);              /* PAN 2 as made: VOX sways a quarter of the way out, nothing else; in sync */
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
