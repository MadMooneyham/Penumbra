/* SPDX-License-Identifier: GPL-3.0-only */
/* Penumbra's settings: one storage object (OBJ_SET), defaults when it is missing or another
 * firmware's. Part of the unity build (and the host's). */
#include "penumbra.h"

settings_t cfg;

void settings_defaults(void)
{
    uint8_t *z = (uint8_t *)&cfg;
    uint32_t i;
    for (i = 0; i < sizeof cfg; i++)
        z[i] = 0;
    cfg.magic = SET_MAGIC;
    cfg.format = SET_FORMAT;
    cfg.bpm_x10 = 900;
    cfg.clk_out = 1;
    cfg.clk_in = 1;
    cfg.midi_ch = 1;
}

int settings_load(void)
{
    settings_t s;
    if (plat_store_load(OBJ_SET, &s, sizeof s) != (int)sizeof s || s.magic != SET_MAGIC || s.format != SET_FORMAT) {
        settings_defaults();
        return -1;
    }
    if (s.bpm_x10 < 200 || s.bpm_x10 > 3000)
        s.bpm_x10 = 900;
    if (s.midi_ch > 16)
        s.midi_ch = 1;
    cfg = s;
    return 0;
}

int settings_save(void) { return plat_store_save(OBJ_SET, &cfg, sizeof cfg); }

void settings_apply(void)
{
    eng_clock.bpm = (float)cfg.bpm_x10 / 10.0f;
    eng_clock.send_clock = cfg.clk_out;
    eng_clock.follow = cfg.clk_in;
    engine_midi_ch(cfg.midi_ch);
    engine_out_loud(cfg.out_loud);
    engine_usb_full(cfg.usb_full);
    engine_drone_core2(cfg.drone_core2);
}
