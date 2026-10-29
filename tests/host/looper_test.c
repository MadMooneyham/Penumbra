/* SPDX-License-Identifier: GPL-3.0-only */
/* The looper (firmware/src/seq/looper.c) alone: a phrase recorded plays back as played, round after
 * round, its length the nearest whole beat; an overdub adds a layer, UNDO takes it away again, CLEAR
 * the lot; a note held as a recording closes is let go round the loop, never stuck; STOP lets
 * everything go, PLAY starts from the start. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "../../firmware/src/seq/looper.h"

static int fails;
#define CHECK(c, what) do { if (c) printf("  ok   %s\n", what); else { printf("  FAIL %s\n", what); fails++; } } while (0)

static looper_t L;
static struct { int on[128], ons, offs; uint32_t at[64]; int nat; } S;
static uint32_t now;
static void s_on(void *x, int n, int v)
{
    (void)x;
    (void)v;
    S.on[n & 127]++;
    S.ons++;
    if (S.nat < 64)
        S.at[S.nat++] = now;
}
static void s_off(void *x, int n) { (void)x; S.on[n & 127]--; S.offs++; }
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
    looper_note(&L, note, 100);
    run(held);
    looper_note(&L, note, 0);
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
    looper_note(&L, 72, 100);                      /* held as the layer closes */
    run(40u);
    looper_rec(&L, &O);
    looper_note(&L, 72, 0);
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
        static uint8_t buf[16 + LP_EVENTS * 8];
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
        CHECK(n == 16u + (uint32_t)before * 8u && looper_load(&L, buf, n) == 0 && L.n == before && L.len == len &&
                  L.state == LS_PLAY && L.speed == LP_DOUBLE, "saved and loaded: the same loop, playing, its speed");
        buf[0] ^= 1u;
        CHECK(looper_load(&L, buf, n) != 0, "something else: not loaded");
        looper_rec(&L, &O);
        CHECK(looper_save(&L, buf, sizeof buf) == 0, "overdubbing: not saved (not now)");
    }
    printf(fails ? "%d FAILED\n" : "all passed\n", fails);
    return fails != 0;
}
