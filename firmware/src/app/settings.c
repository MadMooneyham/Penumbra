/* SPDX-License-Identifier: GPL-3.0-only */
/* Penumbra's settings (OBJ_SET) and journeys (OBJ_WORK: the one playing; OBJ_USER: 8 slots), with
 * defaults when one is missing or another firmware's. Part of the unity build (and the host's). */
#include "penumbra.h"

settings_t cfg;
const uint8_t BEND_SEMIS[BEND_RANGES] = {2, 5, 7, 12};

static const uint8_t FX_DEF[EF_N] = {91, 89, 25, 57, 10, 82, 57, 44, 64, 62, 0, 106, 85, 62, 1, 64};
_Static_assert(sizeof(settings_t) == 52u, "format 3's size too: 1.0's settings load");
#define SET_V3_FX 40u                            /* 1.0's FX page (its 8): here */

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
    union { settings_t s; uint8_t b[sizeof(settings_t)]; } u;
    settings_t s;
    int n = plat_store_load(OBJ_SET, u.b, sizeof u.b), i;
    s = u.s;
    if (n == (int)sizeof s && s.magic == SET_MAGIC && s.format == 3u) {   /* 1.0's: its FX page moves, the new 8 as made */
        for (i = 0; i < EF_N; i++)
            s.fx[i] = i < 8 ? u.b[SET_V3_FX + i] : FX_DEF[i];
        for (i = 0; i < (int)sizeof s.rsv2; i++)
            s.rsv2[i] = 0;
        s.format = SET_FORMAT;
    } else if (n != (int)sizeof s || s.magic != SET_MAGIC || s.format != SET_FORMAT) {
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
    s.midi_quiet &= 3u;
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
    engine_midi_notes_out(!(cfg.midi_quiet & 1u));
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
    for (i = 0; i < JR_SCENES; i++) {
        if (j->scene[i].pedal > 12)
            j->scene[i].pedal = 0;
        j->scene[i].room &= 0x7Fu;
        j->scene[i].halo &= 0x7Fu;
    }
    for (i = 0; i < JR_DMOTION; i++)
        j->dmotion[i] &= 0x7Fu;
    for (i = 0; i < JR_TIDE; i++)
        j->tide[i] &= 0x7Fu;
    for (i = 0; i < JR_PAN; i++) {
        j->pan[i] &= 0x7Fu;
        j->sway[i] &= 0x7Fu;
    }
    j->sway_rate &= 0x7Fu;
    j->sway_off &= 0x7Fu;
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

/* 1.2's later betas' journey (format 8) kept one SWAY for every part and PAN 2's switches in what are now
 * RATE and each part's sway: the parts switched on sway by that SWAY, on the tide. Its first beta's (7) had
 * VOX alone swaying (PAN 2's bytes spare). A 1.1 one (6) less PAN and SWAY too, a 1.0 one (5) less ROOM,
 * HALO, the drone's movement and TIDES too (their bytes were spare): its scenes in the FX page's space, the
 * drone moving as it did, TIDES as made; every voice in the middle and nothing swaying. Each as it sounded */
static void journey_upgrade(journey_t *j)
{
    int k;
    if (j->magic != JR_MAGIC || j->format < 5u || j->format >= JR_FORMAT || j->size != sizeof *j)
        return;
    if (j->format == 5u) {
        for (k = 0; k < JR_SCENES; k++)
            j->scene[k].room = j->scene[k].halo = 64;
        jr_motion_default(j);
    }
    if (j->format <= 6u) {
        jr_pan_default(j);
        for (k = 0; k < JR_PAN; k++)
            j->sway[k] = 0;
    } else {
        const uint8_t all = j->sway_rate;        /* (the one SWAY) */
        for (k = 0; k < JR_PAN; k++)
            j->sway[k] = (j->format == 7u ? k == 1 : j->sway[k] != 0) ? all : 0;   /* (7: VOX alone) */
        if (j->format == 7u)
            j->sway_off = 0;
        j->sway_rate = 0;
    }
    for (k = 0; k < (int)sizeof j->rsv2; k++)
        j->rsv2[k] = 0;
    j->format = JR_FORMAT;
}

/* the journey playing (OBJ_WORK): the journey, then its A/B (1.0's: the journey alone, no A/B) */
sound_ab_t sound_ab;
typedef struct {
    journey_t j;
    sound_ab_t ab;
} work_t;
static work_t work_buf PEN_POOL;
_Static_assert(sizeof(work_t) <= PLAT_STORE_MAX, "the journey playing and its A/B fit one object");
static void sound_sane(jr_sound_t *s)              /* every value 0..127, the switches in range */
{
    uint8_t *p = (uint8_t *)s;
    uint32_t k;
    for (k = 0; k < sizeof *s; k++)
        p[k] &= 0x7Fu;
    for (k = 0; k < JR_DRONES; k++)
        if (s->dvoice[k][DV_OCT] > 2)
            s->dvoice[k][DV_OCT] = 1;
    for (k = 0; k < JR_VSOUNDS; k++)
        if (s->vsound[k][VS_BOCT] > 2)
            s->vsound[k][VS_BOCT] = 1;
    if (s->bloom[BL_HEIGHT] > 2)
        s->bloom[BL_HEIGHT] = 1;
    s->dswell &= (1u << ENG_DRONES) - 1u;
    s->vcur %= JR_VSOUNDS;
}

int journey_load_work(void)
{
    journey_t *j = &work_buf.j;
    int n = plat_store_load(OBJ_WORK, &work_buf, sizeof work_buf);
    uint8_t *z = (uint8_t *)&sound_ab;
    uint32_t k;
    for (k = 0; k < sizeof sound_ab; k++)
        z[k] = 0;
    if (n == (int)sizeof(journey_t) || n == (int)sizeof work_buf)
        journey_upgrade(j);
    if ((n != (int)sizeof(journey_t) && n != (int)sizeof work_buf) || !jr_valid(j))
        return -1;                                /* none, or not this format: the first preset plays */
    journey_sane(j);
    if (n == (int)sizeof work_buf) {              /* its A/B */
        sound_ab = work_buf.ab;
        sound_ab.have &= 3u;
        sound_ab.t &= 0x7Fu;
        sound_sane(&sound_ab.a);
        sound_sane(&sound_ab.b);
    }
    eng_jr = *j;                                  /* (before the audio starts, or between its blocks: bytes) */
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
    work_buf.j = eng_jr;
    work_buf.ab = sound_ab;
    return plat_store_save(OBJ_WORK, &work_buf, sizeof work_buf);
}

/* the sound library (OBJ_SOUNDS): 8 sounds, each one a journey's sound without its scenes */
#define SND_MAGIC 0x31444E53u                    /* "SND1" */
typedef struct {
    uint32_t magic;
    uint8_t used[SND_SLOTS];
    jr_sound_t s[SND_SLOTS];
} sound_lib_t;
static sound_lib_t slib PEN_POOL;
_Static_assert(sizeof(sound_lib_t) <= PLAT_STORE_MAX, "the sound library fits one object");
int sound_slots_load(void)
{
    int i, n = plat_store_load(OBJ_SOUNDS, &slib, sizeof slib);
    if (n != (int)sizeof slib || slib.magic != SND_MAGIC) {
        uint8_t *z = (uint8_t *)&slib;
        uint32_t k;
        for (k = 0; k < sizeof slib; k++)
            z[k] = 0;
        slib.magic = SND_MAGIC;
        return -1;
    }
    for (i = 0; i < SND_SLOTS; i++) {
        slib.used[i] = slib.used[i] == 1u;
        sound_sane(&slib.s[i]);
    }
    return 0;
}
int sound_slot_used(int slot) { return slot >= 0 && slot < SND_SLOTS && slib.used[slot]; }
const jr_sound_t *sound_slot(int slot) { return sound_slot_used(slot) ? &slib.s[slot] : 0; }
int sound_slot_save(int slot)
{
    if (slot < 0 || slot >= SND_SLOTS)
        return -1;
    slib.magic = SND_MAGIC;
    jr_sound_get(&eng_jr, &slib.s[slot]);
    slib.used[slot] = 1;
    return plat_store_save(OBJ_SOUNDS, &slib, sizeof slib);
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
        journey_upgrade(&jslot[i]);
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
    for (k = 0; k < JR_DMOTION; k++)
        j.dmotion[k] = eng_jr.dmotion[k];
    for (k = 0; k < JR_TIDE; k++)
        j.tide[k] = eng_jr.tide[k];
    eng_jr = j;
    engine_journey_loaded();
}
