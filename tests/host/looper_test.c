/* SPDX-License-Identifier: GPL-3.0-only */
/* The looper (firmware/src/seq/looper.c) alone: a phrase recorded plays back as played, round after
 * round, its length the nearest whole beat; an overdub adds a layer, UNDO takes it away again, CLEAR
 * the lot; a note held as a recording closes is let go round the loop, never stuck; STOP lets
 * everything go, PLAY starts from the start. A note's tag (Penumbra's: the VOX sound it was played with)
 * comes back with it, each layer's its own, saved and loaded too. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "../../firmware/src/seq/looper.h"

static int fails;
#define CHECK(c, what) do { if (c) printf("  ok   %s\n", what); else { printf("  FAIL %s\n", what); fails++; } } while (0)

static looper_t L;
static struct { int on[128], ons, offs, tags[16], octs[3], lays[LP_LAYERS], vel; uint32_t at[64]; int nat; } S;
static uint32_t now;
static void s_on(void *x, int n, int v, int tag, int layer)
{
    (void)x;
    S.vel = v;
    S.tags[tag & 15]++;
    S.octs[(tag >> 4) % 3]++;
    S.lays[layer % LP_LAYERS]++;
    S.on[n & 127]++;
    S.ons++;
    if (S.nat < 64)
        S.at[S.nat++] = now;
}
static void s_off(void *x, int n, int layer) { (void)x; (void)layer; S.on[n & 127]--; S.offs++; }
static int hits;
static void s_hit(void *x, int w, int v) { (void)x; (void)w; (void)v; hits++; }
static const lp_sink_t O = {0, s_on, s_off, s_hit};
static int sounding(void)
{
    int k, n = 0;
    for (k = 0; k < 128; k++)
        n += S.on[k] > 0;
    return n;
}
static int near(uint32_t a, uint32_t b) { return (a > b ? a - b : b - a) <= 13u; }
static void run(uint32_t ticks)                     /* in blocks of 13 ticks (about a block at 90 BPM) */
{
    while (ticks) {
        uint32_t t = ticks < 13u ? ticks : 13u;
        looper_advance(&L, t, &O);
        now += t;
        ticks -= t;
    }
}
static void hand(int note, uint32_t held, uint32_t after)   /* a note played: on, held, off */
{
    looper_note(&L, note, 100, 0);
    run(held);
    looper_note(&L, note, 0, 0);
    run(after);
}

int main(void)
{
    looper_init(&L);
    memset(&S, 0, sizeof S);
    looper_rec(&L, &O);
    CHECK(L.state == LS_REC, "REC: recording");
    hand(60, 2u * LP_BEAT, LP_BEAT);
    hand(64, LP_BEAT, LP_BEAT - 20u);              /* 5 beats less 20 ticks: it rounds to 5 */
    looper_rec(&L, &O);
    CHECK(L.state == LS_PLAY && L.len == 5u * LP_BEAT && L.layers == 1, "REC again: it plays, five beats long");
    S.ons = 0;
    S.nat = 0;
    run(L.len - L.pos);                            /* to the start, then three rounds */
    run(3u * L.len);
    printf("  played back %d notes in 3 rounds (+ the start's)\n", S.ons);
    CHECK(S.ons == 7 && near(S.at[1] - S.at[0], 3u * LP_BEAT) && near(S.at[3] - S.at[1], 5u * LP_BEAT),
          "round after round, each note where it was played (to the block)");
    looper_rec(&L, &O);
    CHECK(L.state == LS_DUB, "REC while playing: an overdub");
    hand(67, 60u, 60u);
    looper_rec(&L, &O);
    CHECK(L.state == LS_PLAY && L.layers == 2 && L.n == 6, "REC again: the layer kept");
    S.ons = 0;
    run(2u * L.len);
    CHECK(S.ons == 6, "both layers play");
    looper_undo(&L, &O);
    S.ons = 0;
    run(2u * L.len);
    CHECK(L.layers == 1 && L.n == 4 && S.ons == 4, "UNDO: the last layer gone, the first plays on");
    looper_rec(&L, &O);
    looper_note(&L, 72, 100, 0);                    /* held as the layer closes */
    run(40u);
    looper_rec(&L, &O);
    looper_note(&L, 72, 0, 0);
    run(3u * L.len);
    CHECK(S.on[72] == 0, "a note held as a layer closes: let go round the loop, not stuck");
    looper_play(&L, &O);
    CHECK(L.state == LS_STOP && sounding() == 0, "PLAY: stopped, nothing sounding");
    S.nat = 0;
    looper_play(&L, &O);
    CHECK(L.state == LS_PLAY && S.nat == 1 && L.pos == 0, "PLAY again: from the start (its first note at once)");
    run(L.len / 2u);
    looper_clear(&L, &O);
    CHECK(L.state == LS_EMPTY && L.n == 0 && sounding() == 0, "CLEAR: gone, nothing sounding");
    looper_undo(&L, &O);
    CHECK(L.state == LS_EMPTY, "UNDO with nothing: nothing");
    {   /* PULSE's hits loop too; the speed; and the loop saved and back */
        static uint8_t buf[LP_SAVE_MAX];
        uint32_t n, len;
        int before;
        looper_rec(&L, &O);
        looper_hit(&L, 0, 110);
        run(LP_BEAT);
        hand(62, LP_BEAT, LP_BEAT);
        looper_rec(&L, &O);
        len = L.len;
        hits = 0;
        S.ons = 0;
        run(2u * len);                             /* (two rounds from anywhere: each event twice) */
        CHECK(hits == 2 && S.ons == 2, "a PULSE hit loops with the notes");
        looper_speed(&L, -1);
        hits = 0;
        run(2u * len);
        CHECK(L.speed == LP_HALF && hits == 1, "half speed: once round in twice the time");
        looper_speed(&L, 1);
        looper_speed(&L, 1);
        hits = 0;
        run(2u * len);
        CHECK(L.speed == LP_DOUBLE && hits == 4, "double: twice round in the time");
        n = looper_save(&L, buf, sizeof buf);
        before = L.n;
        looper_clear(&L, &O);
        CHECK(n == LP_SAVE_HDR + (uint32_t)before * 8u && looper_load(&L, buf, n) == 0 && L.n == before && L.len == len &&
                  L.state == LS_PLAY && L.speed == LP_DOUBLE, "saved and loaded: the same loop, playing, its speed");
        buf[0] ^= 1u;
        CHECK(looper_load(&L, buf, n) != 0, "something else: not loaded");
        looper_rec(&L, &O);
        CHECK(looper_save(&L, buf, sizeof buf) == 0, "overdubbing: not saved (not now)");
    }
    {   /* tags: a layer played with tag 2, one over it with tag 4; each note comes back with its own */
        static uint8_t buf[LP_SAVE_MAX];
        uint32_t n;
        looper_clear(&L, &O);
        looper_rec(&L, &O);
        looper_note(&L, 60, 100, 2);
        run(LP_BEAT);
        looper_note(&L, 60, 0, 2);
        run(LP_BEAT);
        looper_rec(&L, &O);
        looper_rec(&L, &O);
        looper_note(&L, 64, 100, 4);
        run(LP_BEAT / 2u);
        looper_note(&L, 64, 0, 4);
        looper_rec(&L, &O);
        memset(S.tags, 0, sizeof S.tags);
        run(2u * L.len);
        CHECK(S.tags[2] == 2 && S.tags[4] == 2 && S.tags[0] == 0, "each layer's notes come back with their own tag");
        n = looper_save(&L, buf, sizeof buf);
        looper_clear(&L, &O);
        memset(S.tags, 0, sizeof S.tags);
        CHECK(looper_load(&L, buf, n) == 0, "a tagged loop loads");
        run(2u * L.len);
        CHECK(S.tags[2] == 2 && S.tags[4] == 2 && S.tags[0] == 0, "saved and loaded: the tags kept");
    }
    {   /* the layers' own settings: three layers (notes 60, 64, 67, the first tagged 2); the second muted is
         * silent and what it has on is let go; a level scales its notes; a sound and an octave go out with
         * them; the same note in two layers is two notes; the middle one taken out, the third is the second,
         * with its settings; saved and loaded, they are kept; a loop saved by 1.4 loads, as played */
        static uint8_t buf[LP_SAVE_MAX], old[LP_SAVE_MAX];
        uint32_t n, k, m;
        looper_clear(&L, &O);
        memset(&S, 0, sizeof S);
        looper_rec(&L, &O);
        looper_note(&L, 60, 100, 2);
        run(LP_BEAT);
        looper_note(&L, 60, 0, 0);
        run(LP_BEAT);
        looper_rec(&L, &O);
        for (k = 0; k < 2; k++) {
            looper_rec(&L, &O);
            looper_note(&L, k ? 67 : 64, 100, 3);
            run(LP_BEAT / 2u);
            looper_note(&L, k ? 67 : 64, 0, 0);
            looper_rec(&L, &O);
            run(L.len - L.pos % L.len);
        }
        CHECK(L.layers == 3 && L.lay[1].level == 100 && L.lay[1].oct == 1 && !L.lay[1].mute && !L.lay[1].snd,
              "three layers, each as played");
        memset(&S, 0, sizeof S);
        run(2u * L.len);
        CHECK(S.lays[0] == 2 && S.lays[1] == 2 && S.lays[2] == 2 && S.octs[1] == 6, "each layer's notes say whose they are");
        looper_layer_set(&L, 1, LL_MUTE, 1, &O);
        memset(&S, 0, sizeof S);
        run(2u * L.len);
        CHECK(S.lays[1] == 0 && S.lays[0] == 2 && S.lays[2] == 2 && S.on[64] <= 0, "a muted layer is silent, the others play");
        looper_layer_set(&L, 1, LL_MUTE, 0, &O);
        looper_layer_set(&L, 0, LL_MUTE, 1, &O);
        looper_layer_set(&L, 1, LL_MUTE, 1, &O);
        looper_layer_set(&L, 2, LL_LEVEL, 50, &O);
        looper_layer_set(&L, 2, LL_SOUND, 4, &O);
        looper_layer_set(&L, 2, LL_OCT, 2, &O);
        memset(&S, 0, sizeof S);
        run(2u * L.len);
        CHECK(S.vel == 50 && S.tags[4] == 2 && S.tags[3] == 0 && S.octs[2] == 2,
              "a layer's level, sound and octave go out with its notes");
        looper_layer_set(&L, 0, LL_MUTE, 0, &O);
        looper_layer_set(&L, 1, LL_MUTE, 0, &O);
        looper_layer_delete(&L, 1, &O);
        memset(&S, 0, sizeof S);
        run(2u * L.len);
        CHECK(L.layers == 2 && S.lays[0] == 2 && S.lays[1] == 2 && S.lays[2] == 0 && S.tags[4] == 2 && S.tags[2] == 2 &&
                  L.lay[1].level == 50 && L.lay[2].level == 100 && sounding() <= 2,
              "the middle layer taken out: the one over it moves down, with its settings");
        n = looper_save(&L, buf, sizeof buf);
        /* (1.4's file: the header with its magic, then the events, no layers' settings) */
        memcpy(old, buf, 16);
        old[0] = 'P', old[1] = 'N', old[2] = 'L', old[3] = '1';
        memcpy(old + 16, buf + LP_SAVE_HDR, n - LP_SAVE_HDR);
        m = n - (LP_SAVE_HDR - 16u);
        looper_clear(&L, &O);
        CHECK(looper_load(&L, buf, n) == 0 && L.layers == 2 && L.lay[1].level == 50 && L.lay[1].snd == 4 && L.lay[1].oct == 2,
              "saved and loaded: the layers' settings kept");
        looper_clear(&L, &O);
        CHECK(looper_load(&L, old, m) == 0 && L.layers == 2 && L.lay[1].level == 100 && L.lay[1].oct == 1 && L.n == 4,
              "a loop saved by 1.4 loads, its layers as played");
        {   /* PAN, DRIFT, TONE, SWELL: kept for the sink, as made 64, 0, 64, 64; they move down with their layer;
             * saved; a loop saved by 1.5 (four bytes a layer) loads with them as made */
            static uint8_t two[LP_SAVE_MAX];
            uint32_t j, n2;
            CHECK(L.lay[0].pan == 64 && L.lay[0].drift == 0 && L.lay[0].tone == 64 && L.lay[0].swell == 64,
                  "a layer's place, drift, tone and swell as made");
            looper_layer_set(&L, 1, LL_PAN, 20, &O);
            looper_layer_set(&L, 1, LL_DRIFT, 90, &O);
            looper_layer_set(&L, 1, LL_TONE, 100, &O);
            looper_layer_set(&L, 1, LL_SWELL, 300, &O);
            CHECK(L.lay[1].pan == 20 && L.lay[1].drift == 90 && L.lay[1].tone == 100 && L.lay[1].swell == 127,
                  "set, each in range");
            n = looper_save(&L, buf, sizeof buf);
            looper_clear(&L, &O);
            CHECK(looper_load(&L, buf, n) == 0 && L.lay[1].pan == 20 && L.lay[1].drift == 90 && L.lay[1].tone == 100,
                  "saved and loaded: kept");
            /* (1.5's file: "PNL2", the header, four bytes a layer, the events) */
            memcpy(two, buf, 16);
            two[0] = 'P', two[1] = 'N', two[2] = 'L', two[3] = '2';
            for (j = 0; j < LP_LAYERS; j++)
                memcpy(two + 16 + 4 * j, buf + 16 + 8 * j, 4);
            memcpy(two + 16 + 4 * LP_LAYERS, buf + LP_SAVE_HDR, n - LP_SAVE_HDR);
            n2 = n - 4u * LP_LAYERS;
            looper_clear(&L, &O);
            CHECK(looper_load(&L, two, n2) == 0 && L.layers == 2 && L.lay[1].level == 100 && L.lay[1].oct == 1 &&
                      L.lay[1].pan == 64 && L.lay[1].drift == 0 && L.lay[1].tone == 64,
                  "a loop saved by 1.5 loads: its settings, the new ones as made");
            looper_load(&L, buf, n);
            looper_layer_delete(&L, 0, &O);
            CHECK(L.layers == 1 && L.lay[0].pan == 20 && L.lay[0].tone == 100 && L.lay[1].pan == 64,
                  "the first taken out: the second's place and tone move down with it");
        }
        looper_layer_delete(&L, 0, &O);
        CHECK(L.state == LS_EMPTY && sounding() <= 0, "the last layer taken out: the loop gone");
    }
    printf(fails ? "%d FAILED\n" : "all passed\n", fails);
    return fails != 0;
}
