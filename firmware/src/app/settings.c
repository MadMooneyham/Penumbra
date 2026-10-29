/* SPDX-License-Identifier: GPL-3.0-only */
/* Penumbra's settings (OBJ_SET) and journeys (OBJ_WORK: the one playing; OBJ_USER: 8 slots), with
 * defaults when one is missing or another firmware's. Part of the unity build (and the host's). */
#include "penumbra.h"

settings_t cfg;

static const uint8_t VOICE_DEF[EV_N][VF_N] = {{100, 64, 20, 64}, {100, 64, 80, 64}, {80, 64, 80, 64},
                                              {100, 64, 70, 64}};   /* PULSE 2 dB down */
static const uint8_t FX_DEF[EF_N] = {91, 89, 25, 57, 10, 82, 57, 44};
#define SET_V1_LEN 24u                           /* format 1's size */
#define SET_V2_LEN 56u                           /* format 2's: 24, five voices of 4, the FX's 8, 4 spare */
_Static_assert(sizeof(settings_t) == SET_V2_LEN - VF_N, "format 3 is format 2 without LINE's voice");

static void defaults_after_v1(void)
{
    uint32_t v, f;
    cfg.autoplay = 1;
    for (v = 0; v < EV_N; v++)
        for (f = 0; f < VF_N; f++)
            cfg.voice[v][f] = VOICE_DEF[v][f];
    for (v = 0; v < EF_N; v++)
        cfg.fx[v] = FX_DEF[v];
}

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
    defaults_after_v1();
}

int settings_load(void)
{
    union { settings_t s; uint8_t b[SET_V2_LEN]; } u;
    settings_t s;
    int n = plat_store_load(OBJ_SET, u.b, sizeof u.b), i, f;
    s = u.s;
    if (n == (int)SET_V1_LEN && s.magic == SET_MAGIC && s.format == 1u) {   /* 0.7 .. 0.12: kept, the rest new */
        settings_defaults();
        for (i = 8; i < 17; i++)
            ((uint8_t *)&cfg)[i] = u.b[i];
        s = cfg;
    } else if (n == (int)SET_V2_LEN && s.magic == SET_MAGIC && s.format == 2u) {   /* 0.13: LINE's voice left out */
        static const uint8_t FROM[EV_N] = {0, 2, 3, 4};   /* (DRONE, LINE, VOX, PULSE, TEXTURE then) */
        for (i = 0; i < EV_N; i++)
            for (f = 0; f < VF_N; f++)
                s.voice[i][f] = u.b[24 + FROM[i] * VF_N + f];
        for (i = 0; i < EF_N; i++)
            s.fx[i] = u.b[24 + 5 * VF_N + i];
        for (i = 0; i < 4; i++)
            s.rsv2[i] = 0;
    } else if (n != (int)sizeof s || s.magic != SET_MAGIC || s.format != SET_FORMAT) {
        settings_defaults();
        return -1;
    }
    if (s.bpm_x10 < 200 || s.bpm_x10 > 3000)
        s.bpm_x10 = 900;
    if (s.midi_ch > 16)
        s.midi_ch = 1;
    if (s.drone_core > DRONE_SPLIT)
        s.drone_core = DRONE_SPLIT;
    for (i = 0; i < EV_N; i++)
        for (f = 0; f < VF_N; f++)
            if (s.voice[i][f] > 127)
                s.voice[i][f] = VOICE_DEF[i][f];
    for (i = 0; i < EF_N; i++)
        if (s.fx[i] > engine_fx_max(i))
            s.fx[i] = FX_DEF[i];
    s.format = SET_FORMAT;
    cfg = s;
    return 0;
}

int settings_save(void) { return plat_store_save(OBJ_SET, &cfg, sizeof cfg); }

void settings_apply(void)
{
    int v, f;
    eng_clock.bpm = (float)cfg.bpm_x10 / 10.0f;
    eng_clock.send_clock = cfg.clk_out;
    eng_clock.follow = cfg.clk_in;
    engine_midi_ch(cfg.midi_ch);
    engine_out_loud(cfg.out_loud);
    engine_usb_full(cfg.usb_full);
    engine_drone_core(cfg.drone_core);
    for (v = 0; v < EV_N; v++)
        for (f = 0; f < VF_N; f++)
            engine_voice(v, f, cfg.voice[v][f]);
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
    j->mode %= NMODES;
    j->at %= j->nscenes;
    for (i = 0; i < NMACROS; i++)
        j->macro[i] &= 0x7Fu;
    for (i = 0; i < JR_LOOPS; i++) {
        loop_t *l = &j->loop[i];
        l->len = (uint8_t)(l->len < 1 || l->len > JR_STEPS ? JR_STEPS : l->len);
        l->div = (uint8_t)(l->div == 1 || l->div == 2 || l->div == 4 || l->div == 8 ? l->div : 2);
        l->on = (uint8_t)(l->on > LOOP_RECORDED ? LOOP_CAPTURED : l->on);
        l->mutate &= 0x7Fu;
    }
}

/* a 0.13 journey (format 1) had three loops, LINE's first: the rest is as now, so it comes over less
 * that loop */
#define JR_V1_SIZE (sizeof(journey_t) + sizeof(loop_t))
#define JR_LOOPS_AT __builtin_offsetof(journey_t, loop)
static int journey_from_v1(const uint8_t *src, journey_t *j)
{
    uint8_t *d = (uint8_t *)j;
    uint32_t k;
    for (k = 0; k < JR_LOOPS_AT; k++)
        d[k] = src[k];
    for (k = 0; k < JR_LOOPS * sizeof(loop_t); k++)
        d[JR_LOOPS_AT + k] = src[JR_LOOPS_AT + sizeof(loop_t) + k];
    if (j->magic != JR_MAGIC || j->format != 1u || j->size != JR_V1_SIZE)
        return -1;
    j->format = JR_FORMAT;
    j->size = (uint16_t)sizeof *j;
    return jr_valid(j) ? 0 : -1;
}
static uint8_t jr_buf[JR_USER * JR_V1_SIZE] __attribute__((aligned(4))) PEN_POOL;

int journey_load_work(void)
{
    journey_t j;
    int n = plat_store_load(OBJ_WORK, jr_buf, JR_V1_SIZE);
    if (n == (int)JR_V1_SIZE) {
        if (journey_from_v1(jr_buf, &j) != 0)
            return -1;
    } else {
        uint8_t *d = (uint8_t *)&j;
        uint32_t k;
        if (n != (int)sizeof j)
            return -1;
        for (k = 0; k < sizeof j; k++)
            d[k] = jr_buf[k];
        if (!jr_valid(&j))
            return -1;
    }
    journey_sane(&j);
    eng_jr = j;                                   /* (before the audio starts, or between its blocks: bytes) */
    engine_journey_loaded();
    return 0;
}

int journey_save_work(void)
{
    journey_t j = eng_jr;
    return plat_store_save(OBJ_WORK, &j, sizeof j);
}

int journey_slots_load(void)
{
    int i, n = plat_store_load(OBJ_USER, jr_buf, sizeof jr_buf);
    uint8_t *z = (uint8_t *)jslot;
    uint32_t k;
    if (n == (int)sizeof jslot) {
        for (k = 0; k < sizeof jslot; k++)
            z[k] = jr_buf[k];
    } else if (n == (int)sizeof jr_buf) {         /* 0.13's: each converted (an empty slot stays empty) */
        for (i = 0; i < JR_USER; i++)
            if (journey_from_v1(jr_buf + (uint32_t)i * JR_V1_SIZE, &jslot[i]) != 0)
                for (k = 0; k < sizeof(journey_t); k++)
                    ((uint8_t *)&jslot[i])[k] = 0;
    } else {
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
    jr_preset(i, &j);
    j.mode = eng_jr.mode;                         /* (the mode is how you are playing: it stays) */
    eng_jr = j;
    engine_journey_loaded();
}
