/* SPDX-License-Identifier: GPL-3.0-only */
/* Penumbra's settings (OBJ_SET) and journeys (OBJ_WORK: the one playing; OBJ_USER: 8 slots), with
 * defaults when one is missing or another firmware's. Part of the unity build (and the host's). */
#include "penumbra.h"

settings_t cfg;
const uint8_t BEND_SEMIS[BEND_RANGES] = {2, 5, 7, 12};

static const uint8_t FX_DEF[EF_N] = {91, 89, 25, 57, 10, 82, 57, 44};
_Static_assert(sizeof(settings_t) == 52u, "format 3's size: 1.0's settings load");

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
    cfg.drone_core = DRONE_SPLIT;
    cfg.autoplay = 1;
    settings_fx_default();
}

int settings_load(void)
{
    settings_t s;
    int n = plat_store_load(OBJ_SET, &s, sizeof s), i;
    if (n != (int)sizeof s || s.magic != SET_MAGIC || s.format != SET_FORMAT) {
        settings_defaults();                     /* none, or another firmware's */
        return -1;
    }
    if (s.bpm_x10 < 200 || s.bpm_x10 > 3000)
        s.bpm_x10 = 900;
    if (s.midi_ch > 16)
        s.midi_ch = 1;
    if (s.drone_core > DRONE_SPLIT)
        s.drone_core = DRONE_SPLIT;
    s.bend %= BEND_RANGES;
    for (i = 0; i < EF_N; i++)
        if (s.fx[i] > engine_fx_max(i))
            s.fx[i] = FX_DEF[i];
    cfg = s;
    return 0;
}

int settings_save(void) { return plat_store_save(OBJ_SET, &cfg, sizeof cfg); }

void settings_fx_default(void)
{
    uint32_t v;
    for (v = 0; v < EF_N; v++)
        cfg.fx[v] = FX_DEF[v];
}

void settings_apply(void)
{
    int v;
    eng_clock.bpm = (float)cfg.bpm_x10 / 10.0f;
    eng_clock.send_clock = cfg.clk_out;
    eng_clock.follow = cfg.clk_in;
    engine_midi_ch(cfg.midi_ch);
    engine_out_loud(cfg.out_loud);
    engine_usb_full(cfg.usb_full);
    engine_drone_core(cfg.drone_core);
    engine_bend_range(BEND_SEMIS[cfg.bend % BEND_RANGES]);
    for (v = 0; v < EF_N; v++)
        engine_fx(v, cfg.fx[v]);
}

/* ------------------------------------------------------------------ journeys --- */
static journey_t jslot[JR_USER] PEN_POOL;
_Static_assert(sizeof(journey_t) * JR_USER <= PLAT_STORE_MAX, "the user journeys fit one object");

static void journey_sane(journey_t *j)          /* a loaded one: every field in range */
{
    int i;
    for (i = 0; i < JR_NAME - 1; i++)
        if (j->name[i] && (j->name[i] < ' ' || j->name[i] > 'Z'))
            j->name[i] = ' ';
    j->name[JR_NAME - 1] = 0;
    j->at %= j->nscenes;
    for (i = 0; i < NMACROS; i++)
        j->macro[i] &= 0x7Fu;
    for (i = 0; i < JR_SCENES; i++)
        if (j->scene[i].pedal > 12)
            j->scene[i].pedal = 0;
    for (i = 0; i < EV_N * SH_N; i++)
        (&j->shape[0][0])[i] &= 0x7Fu;
    for (i = 0; i < ENG_DRONES * DV_N; i++)
        (&j->dvoice[0][0])[i] &= 0x7Fu;
    for (i = 0; i < JR_VSOUNDS * JR_VS; i++)
        (&j->vsound[0][0])[i] &= 0x7Fu;
    for (i = 0; i < JR_VSOUNDS; i++)
        if (j->vsound[i][VS_BOCT] > 2)
            j->vsound[i][VS_BOCT] = 1;
    j->vcur %= JR_VSOUNDS;
    for (i = 0; i < ENG_DRONES; i++)
        if (j->dvoice[i][DV_OCT] > 2)
            j->dvoice[i][DV_OCT] = 1;
    j->dswell &= (1u << ENG_DRONES) - 1u;
    for (i = 0; i < 3; i++)
        j->bloom[i] &= 0x7Fu;
    if (j->bloom[BL_HEIGHT] > 2)
        j->bloom[BL_HEIGHT] = 1;
}

int journey_load_work(void)
{
    journey_t j;
    int n = plat_store_load(OBJ_WORK, &j, sizeof j);
    if (n != (int)sizeof j || !jr_valid(&j))
        return -1;                                /* none, or not this format: the first preset plays */
    journey_sane(&j);
    eng_jr = j;                                   /* (before the audio starts, or between its blocks: bytes) */
    engine_journey_loaded();
    return 0;
}

/* the loop (OBJ_LOOP): kept with the journey playing, so a session survives a power cut */
static uint8_t lp_buf[16u + LP_EVENTS * 8u] __attribute__((aligned(4))) PEN_POOL;
_Static_assert(sizeof lp_buf <= PLAT_STORE_MAX, "the loop fits one object");
int loop_load_work(void)
{
    int n = plat_store_load(OBJ_LOOP, lp_buf, sizeof lp_buf);
    return n > 0 ? engine_looper_load(lp_buf, (uint32_t)n) : -1;
}
int loop_save_work(void)                          /* 1: not now (recording) */
{
    uint32_t n = engine_looper_save(lp_buf, sizeof lp_buf);
    if (!n)
        return 1;
    return plat_store_save(OBJ_LOOP, lp_buf, n);
}

int journey_save_work(void)
{
    journey_t j = eng_jr;
    return plat_store_save(OBJ_WORK, &j, sizeof j);
}

int journey_slots_load(void)
{
    int i, n = plat_store_load(OBJ_USER, jslot, sizeof jslot);
    uint8_t *z = (uint8_t *)jslot;
    uint32_t k;
    if (n != (int)sizeof jslot) {
        for (k = 0; k < sizeof jslot; k++)
            z[k] = 0;
        return -1;
    }
    for (i = 0; i < JR_USER; i++)
        if (jr_valid(&jslot[i]))
            journey_sane(&jslot[i]);
    return 0;
}

int journey_slot_used(int slot) { return slot >= 0 && slot < JR_USER && jr_valid(&jslot[slot]); }
const char *journey_slot_name(int slot) { return journey_slot_used(slot) ? jslot[slot].name : "EMPTY"; }

int journey_slot_save(int slot)
{
    if (slot < 0 || slot >= JR_USER)
        return -1;
    jslot[slot] = eng_jr;
    if (!jslot[slot].name[0] || !journey_slot_used(slot)) {
        jslot[slot].magic = JR_MAGIC;
        jslot[slot].format = JR_FORMAT;
        jslot[slot].size = (uint16_t)sizeof(journey_t);
    }
    {   /* named after its slot: USER 1 .. USER 8 */
        char *q = jslot[slot].name;
        const char *u = "USER ";
        int k = 0;
        while (*u)
            q[k++] = *u++;
        q[k++] = (char)('1' + slot);
        while (k < JR_NAME)
            q[k++] = 0;
    }
    return plat_store_save(OBJ_USER, jslot, sizeof jslot);
}

int journey_slot_open(int slot)
{
    if (!journey_slot_used(slot))
        return -1;
    eng_jr = jslot[slot];
    eng_jr.at = 0;
    engine_journey_loaded();
    return 0;
}

void journey_open_preset(int i)
{
    journey_t j;
    uint32_t k;
    jr_preset(i, &j);
    j.mode = eng_jr.mode;                         /* (the mode and the sound are how you are playing: they stay;
                                                   * a user journey brings its own) */
    for (k = 0; k < sizeof j.shape; k++)
        (&j.shape[0][0])[k] = (&eng_jr.shape[0][0])[k];
    for (k = 0; k < sizeof j.dvoice; k++)
        (&j.dvoice[0][0])[k] = (&eng_jr.dvoice[0][0])[k];
    j.dswell = eng_jr.dswell;
    for (k = 0; k < sizeof j.bloom; k++)
        j.bloom[k] = eng_jr.bloom[k];
    for (k = 0; k < sizeof j.vsound; k++)
        (&j.vsound[0][0])[k] = (&eng_jr.vsound[0][0])[k];
    j.vcur = eng_jr.vcur;
    eng_jr = j;
    engine_journey_loaded();
}
