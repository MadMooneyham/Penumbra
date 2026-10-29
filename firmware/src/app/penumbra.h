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

#define SET_MAGIC 0x31534E50u            /* "PNS1": X0X's settings ("0X0P") load as defaults */
#define SET_FORMAT 3u                    /* 1 (0.7 .. 0.12): the first 17 bytes, the rest takes its defaults;
                                          * 2 (0.13): five voices, LINE's second (settings.c converts both) */
typedef struct {
    uint32_t magic, format;
    uint16_t bpm_x10;                    /* the internal tempo */
    uint8_t clk_out;                     /* 1: MIDI clock and START / STOP out while playing */
    uint8_t clk_in;                      /* 1: follow MIDI clock and transport in */
    uint8_t midi_ch;                     /* VOX's notes in: 1-16, 0 = off */
    uint8_t one_core;                    /* 1: the second core stays off (applies at the next power-on) */
    uint8_t drone_core;                  /* the drone voices: 0 the first core, 1 the second, 2 split (engine.h) */
    uint8_t out_loud;                    /* 1: the output 6 dB hotter (the codec's full scale) */
    uint8_t usb_full;                    /* 1: USB audio at full level, MASTER for the FM-1's own outputs */
    uint8_t autoplay;                    /* 1: the journey plays at power-on */
    uint8_t bend;                        /* PITCH's range either way: 0 2, 1 5, 2 7, 3 12 semitones (BEND_SEMIS) */
    uint8_t rsv[5];
    uint8_t voice14[EV_N][4];            /* 0.14's VOICES page (level, tone, character, send): read once,
                                          * into an older journey's sound (settings.c), then unused */
    uint8_t fx[EF_N];                    /* the FX page (engine.h EF_*) */
    uint8_t rsv2[4];
} settings_t;

extern settings_t cfg;
#define BEND_RANGES 4
extern const uint8_t BEND_SEMIS[BEND_RANGES];
void settings_defaults(void);
int settings_load(void);                 /* 0 = loaded; else the defaults are in place */
int settings_save(void);                 /* 0 = saved */
void settings_apply(void);               /* into the engine and the clock */

/* journeys: the one playing (eng_jr) is saved with the settings (OBJ_WORK), so it carries on after a
 * power cut; 8 user slots (OBJ_USER) beside the 6 presets */
int journey_load_work(void);             /* at power-on, after engine_init: 0 = the saved one is playing */
int journey_save_work(void);             /* 0 = saved */
int journey_slots_load(void);
int journey_slot_used(int slot);
const char *journey_slot_name(int slot);
int journey_slot_save(int slot);         /* the one playing into the slot; 0 = saved */
int journey_slot_open(int slot);         /* the slot's journey plays; 0 = done */
void journey_open_preset(int i);

void ui_init(void);
void ui_start(void);                     /* once, after boot: PLAY AT POWER-ON */
void ui_frame(void);
void ui_input_only(void);
void ui_say(const char *a, const char *b);
void ui_warn(const char *a, const char *b);   /* ui_say for what did not happen: a screen reader says it whole */
void ui_redraw(void);                    /* the whole screen drawn again at the next frame */
