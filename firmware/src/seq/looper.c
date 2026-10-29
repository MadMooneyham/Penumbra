/* SPDX-License-Identifier: GPL-3.0-only */
/* Penumbra's looper; see looper.h. */
#include "looper.h"

#define BIT(a, n) ((a)[(n) >> 5 & 3u] >> ((n) & 31u) & 1u)
#define SET(a, n) ((a)[(n) >> 5 & 3u] |= 1u << ((n) & 31u))
#define CLR(a, n) ((a)[(n) >> 5 & 3u] &= ~(1u << ((n) & 31u)))

static void zero(void *p, uint32_t n)
{
    uint8_t *z = (uint8_t *)p;
    while (n--)
        *z++ = 0;
}

void looper_init(looper_t *l) { zero(l, sizeof *l); }

void looper_quiet(looper_t *l, const lp_sink_t *o)
{
    uint32_t k;
    for (k = 0; k < 128u; k++)
        if (BIT(l->sounding, k)) {
            CLR(l->sounding, k);
            o->off(o->ctx, (int)k);
        }
}

static void add(looper_t *l, uint32_t t, int note, int vel)
{
    lp_ev_t *e;
    if (l->n >= LP_EVENTS) {
        l->full = 1;
        return;
    }
    e = &l->ev[l->n++];
    e->t = t;
    e->note = (uint8_t)(note & 127);
    e->vel = (uint8_t)(vel & 127);
    e->layer = l->layers;
    e->rsv = 0;
}

/* the hands still down when a recording closes: their notes end just before where it is (a note held
 * round the whole loop sounds the whole loop) */
static void close_held(looper_t *l, uint32_t at)
{
    uint32_t k;
    for (k = 0; k < 128u; k++)
        if (BIT(l->held, k)) {
            add(l, at, (int)k, 0);
            CLR(l->held, k);
        }
}

static void close_first(looper_t *l)
{
    uint32_t len = (l->pos + LP_BEAT / 2u) / LP_BEAT * LP_BEAT, i;
    if (len < LP_BEAT)
        len = LP_BEAT;
    if (len > LP_MAX_LEN)
        len = LP_MAX_LEN;
    l->len = len;
    close_held(l, (l->pos + len - 1u) % len);
    for (i = 0; i < l->n; i++)                   /* played after the rounded end: at the start again */
        l->ev[i].t %= len;
    l->pos %= len;
    l->layers = 1;
    l->state = LS_PLAY;
}

static void close_dub(looper_t *l)
{
    close_held(l, (l->pos + l->len - 1u) % l->len);
    l->layers++;
    l->state = LS_PLAY;
}

static void fire(looper_t *l, uint32_t from, uint32_t to, int incl_from, const lp_sink_t *o)
{
    uint32_t i;
    for (i = 0; i < l->n; i++) {
        const lp_ev_t *e = &l->ev[i];
        if ((e->t > from || (incl_from && e->t == from)) && e->t <= to) {
            if (e->vel) {
                if (BIT(l->sounding, e->note))
                    o->off(o->ctx, e->note);
                SET(l->sounding, e->note);
                o->on(o->ctx, e->note, e->vel);
            } else if (BIT(l->sounding, e->note)) {
                CLR(l->sounding, e->note);
                o->off(o->ctx, e->note);
            }
        }
    }
}

static void from_start(looper_t *l, const lp_sink_t *o)
{
    l->pos = 0;
    fire(l, 0, 0, 1, o);
}

void looper_restart(looper_t *l, const lp_sink_t *o)
{
    if (l->state == LS_PLAY || l->state == LS_DUB) {
        looper_quiet(l, o);
        from_start(l, o);
    }
}

void looper_rec(looper_t *l, const lp_sink_t *o)
{
    switch (l->state) {
    case LS_EMPTY:
        zero(l, sizeof *l);
        l->state = LS_REC;
        break;
    case LS_REC: close_first(l); break;
    case LS_PLAY:
        if (l->layers < LP_LAYERS) {
            l->state = LS_DUB;
            zero(l->held, sizeof l->held);
        }
        break;
    case LS_DUB: close_dub(l); break;
    default:                                     /* stopped: from the start, recording over it */
        if (l->layers < LP_LAYERS) {
            from_start(l, o);
            l->state = LS_DUB;
            zero(l->held, sizeof l->held);
        }
        break;
    }
}

void looper_play(looper_t *l, const lp_sink_t *o)
{
    switch (l->state) {
    case LS_REC: close_first(l); break;
    case LS_DUB: close_dub(l); break;
    case LS_PLAY:
        l->state = LS_STOP;
        looper_quiet(l, o);
        break;
    case LS_STOP:
        l->state = LS_PLAY;
        from_start(l, o);
        break;
    default: break;
    }
}

void looper_clear(looper_t *l, const lp_sink_t *o)
{
    looper_quiet(l, o);
    zero(l, sizeof *l);
}

void looper_undo(looper_t *l, const lp_sink_t *o)
{
    uint32_t i, k = 0, gone;
    if (l->state == LS_EMPTY || l->state == LS_REC || (l->layers <= 1 && l->state != LS_DUB)) {
        looper_clear(l, o);
        return;
    }
    gone = l->state == LS_DUB ? l->layers : l->layers - 1u;   /* (overdubbing: that layer) */
    for (i = 0; i < l->n; i++)
        if (l->ev[i].layer != gone)
            l->ev[k++] = l->ev[i];
    l->n = (uint16_t)k;
    l->full = 0;
    if (l->state == LS_DUB)
        l->state = LS_PLAY;
    else
        l->layers--;
    looper_quiet(l, o);                          /* (what the rest has on comes back round) */
}

void looper_note(looper_t *l, int note, int vel)
{
    if (l->state != LS_REC && l->state != LS_DUB)
        return;
    note &= 127;
    if (vel)
        SET(l->held, (uint32_t)note);
    else if (!BIT(l->held, (uint32_t)note) && l->state == LS_DUB)
        return;                                  /* (a note from before the layer: its off is not this layer's) */
    else
        CLR(l->held, (uint32_t)note);
    add(l, l->pos, note, vel);
}

void looper_advance(looper_t *l, uint32_t ticks, const lp_sink_t *o)
{
    uint32_t to;
    if (!ticks)
        return;
    if (l->state == LS_REC) {
        l->pos += ticks;
        if (l->pos >= LP_MAX_LEN)                /* as long as it gets: it closes and plays */
            close_first(l);
        return;
    }
    if ((l->state != LS_PLAY && l->state != LS_DUB) || !l->len)
        return;
    if (ticks > l->len)
        ticks = l->len;
    to = l->pos + ticks;
    if (to < l->len) {
        fire(l, l->pos, to, 0, o);
        l->pos = to;
    } else {                                     /* round the end, to the start */
        fire(l, l->pos, l->len - 1u, 0, o);
        to -= l->len;
        fire(l, 0, to, 1, o);
        l->pos = to;
    }
}
