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
#define SET_FORMAT 1u
typedef struct {
    uint32_t magic, format;
    uint16_t bpm_x10;                    /* the internal tempo */
    uint8_t clk_out;                     /* 1: MIDI clock and START / STOP out while playing */
    uint8_t clk_in;                      /* 1: follow MIDI clock and transport in */
    uint8_t midi_ch;                     /* LINE's notes in: 1-16, 0 = off */
    uint8_t one_core;                    /* 1: the second core stays off (applies at the next power-on) */
    uint8_t drone_core2;                 /* 1: the drone renders on the second core */
    uint8_t out_loud;                    /* 1: the output 6 dB hotter (the codec's full scale) */
    uint8_t usb_full;                    /* 1: USB audio at full level, MASTER for the FM-1's own outputs */
    uint8_t rsv[7];
} settings_t;

extern settings_t cfg;
void settings_defaults(void);
int settings_load(void);                 /* 0 = loaded; else the defaults are in place */
int settings_save(void);                 /* 0 = saved */
void settings_apply(void);               /* into the engine and the clock */

void ui_init(void);
void ui_frame(void);
void ui_input_only(void);
void ui_say(const char *a, const char *b);
void ui_warn(const char *a, const char *b);   /* ui_say for what did not happen: a screen reader says it whole */
void ui_redraw(void);                    /* the whole screen drawn again at the next frame */
