/* SPDX-License-Identifier: GPL-3.0-only */
/* Penumbra: the settings (everything saved, for now) and the app's entry points.
 * Included by the unity build after gfx.c, so drawing helpers are in scope. */
#pragma once
#include <stdint.h>
#include "plat.h"
#include "engine.h"

#ifndef PEN_VERSION                      /* a release build passes its own (tools/build.py --release) */
#define PEN_VERSION "DEV"
#endif
#define FW_NAME "PENUMBRA"               /* the name on the screen and in the reader's HELLO answer */

#define SET_MAGIC 0x31534E50u            /* "PNS1": another firmware's settings load as defaults */
#define SET_FORMAT 4u                    /* 3 (1.0): the FX page's first 8 at byte 40 (settings.c converts it);
                                          * the betas' 1 and 2 load as defaults */
typedef struct {
    uint32_t magic, format;
    uint16_t bpm_x10;                    /* the internal tempo */
    uint8_t clk_out;                     /* 1: MIDI clock and START / STOP out while playing */
    uint8_t clk_in;                      /* 1: follow MIDI clock and transport in */
    uint8_t midi_ch;                     /* MIDI's channel (VOX's notes, the CCs; the black keys on the next): 1-16, 0 = off */
    uint8_t one_core;                    /* 1: the second core stays off (applies at the next power-on) */
    uint8_t drone_core;                  /* the drone voices: 0 the first core, 1 the second, 2 split (engine.h) */
    uint8_t out_loud;                    /* 1: the output 6 dB hotter (the codec's full scale) */
    uint8_t usb_full;                    /* 1: USB audio at full level, MASTER for the FM-1's own outputs; 2: USB ONLY,
                                          * the same, the FM-1's own outputs silent while a computer takes its audio */
    uint8_t autoplay;                    /* 1: the journey plays at power-on */
    uint8_t bend;                        /* PITCH's range either way: 0 2, 1 5, 2 7, 3 12 semitones (BEND_SEMIS) */
    uint8_t midi_quiet;                  /* what does not go out: bit 0 the notes, bit 1 the CCs and program changes
                                          * (a spare byte in 1.1: 0, everything out) */
    uint8_t sound_lock;                  /* 1: a journey opened brings its scenes, and the sound playing stays
                                          * (0, and a spare byte to 1.4: it brings its own sound, gliding in) */
    uint8_t home_page;                   /* HOME's knob page (ALGO there): 0 DRONE, VOX, MOTION, SPACE, MIX, 5 ALIVE
                                          * (a spare byte to 1.6: DRONE) */
    uint8_t knobs;                       /* 1: CONTROLLER, on HOME the seven knobs send fixed CCs (ui.c CTL_CC) and
                                          * leave Penumbra be; 0 (a spare byte to 1.8): PENUMBRA, as ever */
    uint8_t rsv[1];
    uint8_t fx[EF_N];                   /* the FX page (engine.h EF_*) */
    uint8_t ctl[7];                      /* CONTROLLER: each knob's value, 0..127 (K1-K4, SELECT, ALGO, PRESET) */
    uint8_t rsv2[28 - EF_N - 7];
} settings_t;

extern settings_t cfg;
#define BEND_RANGES 4
extern const uint8_t BEND_SEMIS[BEND_RANGES];
void settings_defaults(void);
int settings_load(void);                 /* 0 = loaded; else the defaults are in place */
int settings_save(void);                 /* 0 = saved */
void settings_apply(void);               /* into the engine and the clock */
void settings_fx_default(void);          /* the FX page as made (then settings_apply) */

/* journeys: the one playing (eng_jr) is saved with the settings (OBJ_WORK), so it carries on after a
 * power cut; 8 user slots (OBJ_USER) beside the 6 presets */
int journey_load_work(void);             /* at power-on, after engine_init: 0 = the saved one is playing */
int journey_save_work(void);             /* 0 = saved */
int loop_load_work(void);                /* at power-on, after journey_load_work: 0 = the loop is back */
int loop_save_work(void);                /* 0 = saved, 1 = not now (recording), else failed */
/* A/B (ENV held): two sounds kept with the journey playing (saved after it in OBJ_WORK) and how far the
 * sound is between them */
typedef struct {
    uint8_t have;                        /* bit 0: A kept, bit 1: B */
    uint8_t t;                           /* the blend, 0 (A) .. 127 (B) */
    uint8_t rsv[2];
    jr_sound_t a, b;
} sound_ab_t;
extern sound_ab_t sound_ab;
/* the sound library (SOUNDS, LFO from JOURNEY): 8 slots of sounds, beside the journeys */
#define SND_SLOTS 8
int sound_slots_load(void);
int sound_slot_used(int slot);
const jr_sound_t *sound_slot(int slot);          /* 0: empty */
int sound_slot_save(int slot);           /* the sound playing into the slot; 0 = saved */
int sound_slots_clear(void);             /* every slot empty (RESET SOUND), saved */
int journey_slots_clear(void);           /* every user journey slot empty (RESET SOUND), saved */
int journey_slots_load(void);
int journey_slot_used(int slot);
const char *journey_slot_name(int slot);
const journey_t *journey_slot(int slot);  /* a user slot's journey (0: empty) */
int journey_slot_save(int slot);         /* the one playing into the slot; 0 = saved */
int journey_slot_open(int slot);         /* the slot's journey plays; 0 = done */
int journey_put(int slot, const journey_t *j);   /* the site's journey editor's: into a user slot, or (slot < 0)
                                          * playing now; 0 done, 1 not one this firmware takes, 2 not saved */
void journey_open_preset(int i);          /* the preset plays, all of it: its scenes and its own sound */
extern uint8_t journey_first;            /* the preset a new unit plays */

/* backups (backup.c: the site's Backup page, over SysEx) */
int store_check(uint32_t obj, const void *src, uint32_t n);   /* 0: a part this firmware takes */
int bk_frame(const uint8_t *p, uint32_t n);      /* a SysEx (between F0 and F7): 1 = a backup's, answered */
void bk_tick(uint32_t now);                      /* (each UI frame) */
int bk_holding(void);                            /* 1: a restore is coming in: nothing saved over it */
void ui_flush(void);                             /* what changed, saved now */

void ui_init(void);
void ui_start(void);                     /* once, after boot: PLAY AT POWER-ON */
void ui_frame(void);
void ui_input_only(void);
void ui_say(const char *a, const char *b);
