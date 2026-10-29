/* SPDX-License-Identifier: GPL-3.0-only */
/* Penumbra's looper; see looper.h. */
#include "looper.h"

#define BIT(a, n) ((a)[(n) >> 5 & 7u] >> ((n) & 31u) & 1u)
#define SET(a, n) ((a)[(n) >> 5 & 7u] |= 1u << ((n) & 31u))
#define CLR(a, n) ((a)[(n) >> 5 & 7u] &= ~(1u << ((n) & 31u)))

static void zero(void *p, uint32_t n)
{
    uint8_t *z = (uint8_t *)p;
    while (n--)
        *z++ = 0;
}

void looper_init(looper_t *l)
{
    zero(l, sizeof *l);
    l->speed = LP_AS_PLAYED;
}

void looper_quiet(looper_t *l, const lp_sink_t *o)
{
    uint32_t k;
    for (k = 0; k < 256u; k++)
        if (BIT(l->sounding, k)) {
            CLR(l->sounding, k);
            o->off(o->ctx, (int)k);
        }
}

static void add(looper_t *l, uint32_t t, int kind, int note, int vel)
{
    lp_ev_t *e;
    if (l->n >= LP_EVENTS) {
        l->full = 1;
        return;
    }
    e = &l->ev[l->n++];
    e->t = t;
    e->note = (uint8_t)(note & 255);
    e->vel = (uint8_t)(vel & 127);
    e->layer = l->layers;
    e->kind = (uint8_t)kind;
}

/* the hands still down when a recording closes: their notes end just before where it is (a note held
 * round the whole loop sounds the whole loop) */
static void close_held(looper_t *l, uint32_t at)
{
    uint32_t k;
    for (k = 0; k < 256u; k++)
        if (BIT(l->held, k)) {
            add(l, at, LK_NOTE, (int)k, 0);
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
    l->edits++;
}

static void close_dub(looper_t *l)
{
    close_held(l, (l->pos + l->len - 1u) % l->len);
    l->layers++;
    l->state = LS_PLAY;
    l->edits++;
}

static void fire(looper_t *l, uint32_t from, uint32_t to, int incl_from, const lp_sink_t *o)
{
    uint32_t i;
    for (i = 0; i < l->n; i++) {
        const lp_ev_t *e = &l->ev[i];
        if (!((e->t > from || (incl_from && e->t == from)) && e->t <= to))
            continue;
        if (e->kind == LK_HIT) {
            o->hit(o->ctx, e->note, e->vel);
        } else if (e->vel) {
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

static void from_start(looper_t *l, const lp_sink_t *o)
{
    l->pos = 0;
    l->frac = 0;
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
    case LS_EMPTY: {
        uint32_t e = l->edits;
        looper_init(l);                          /* (as played: a new loop) */
        l->edits = e;
        l->state = LS_REC;
        break;
    }
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
        l->edits++;
        break;
    case LS_STOP:
        l->state = LS_PLAY;
        from_start(l, o);
        l->edits++;
        break;
    default: break;
    }
}

void looper_clear(looper_t *l, const lp_sink_t *o)
{
    uint32_t e = l->edits;
    looper_quiet(l, o);
    looper_init(l);
    l->edits = e + 1u;
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
    l->edits++;
    looper_quiet(l, o);                          /* (what the rest has on comes back round) */
}

void looper_speed(looper_t *l, int dir)
{
    int s = (int)l->speed + (dir > 0 ? 1 : -1);
    if (s < LP_HALF || s > LP_DOUBLE || l->state == LS_EMPTY || l->state == LS_REC)
        return;
    l->speed = (uint8_t)s;
    l->frac = 0;
    l->edits++;
}

static void record(looper_t *l, int kind, int note, int vel)
{
    if (l->state != LS_REC && l->state != LS_DUB)
        return;
    note &= 255;
    if (kind == LK_NOTE) {
        if (vel)
            SET(l->held, (uint32_t)note);
        else if (!BIT(l->held, (uint32_t)note) && l->state == LS_DUB)
            return;                              /* (a note from before the layer: its off is not this layer's) */
        else
            CLR(l->held, (uint32_t)note);
    }
    add(l, l->pos, kind, note, vel);
}
void looper_note(looper_t *l, int note, int vel) { record(l, LK_NOTE, note, vel); }
void looper_hit(looper_t *l, int which, int vel) { record(l, LK_HIT, which, vel ? vel : 1); }

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
    if (l->speed == LP_DOUBLE)
        ticks *= 2u;
    else if (l->speed == LP_HALF) {              /* (the odd tick waits for the next) */
        ticks += l->frac;
        l->frac = (uint8_t)(ticks & 1u);
        ticks >>= 1;
        if (!ticks)
            return;
    }
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

/* ------------------------------------------------------------------- saving --- */
#define LP_MAGIC 0x314C4E50u                     /* "PNL1" */
#define LP_HDR 16u
typedef struct {
    uint32_t magic;
    uint16_t n, rsv;
    uint32_t len;
    uint8_t layers, state, speed, rsv2;
} lp_hdr_t;
_Static_assert(sizeof(lp_hdr_t) == LP_HDR && sizeof(lp_ev_t) == 8u, "the saved loop's layout");

uint32_t looper_save(const looper_t *l, void *dst, uint32_t max)
{
    lp_hdr_t h;
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s;
    uint32_t k, bytes = LP_HDR + (uint32_t)l->n * sizeof(lp_ev_t);
    if (l->state == LS_REC || l->state == LS_DUB || bytes > max)
        return 0;
    h.magic = LP_MAGIC;
    h.n = l->state == LS_EMPTY ? 0 : l->n;
    h.rsv = 0;
    h.len = l->state == LS_EMPTY ? 0 : l->len;
    h.layers = l->layers;
    h.state = l->state;
    h.speed = l->speed;
    h.rsv2 = 0;
    bytes = LP_HDR + (uint32_t)h.n * sizeof(lp_ev_t);
    s = (const uint8_t *)&h;
    for (k = 0; k < LP_HDR; k++)
        d[k] = s[k];
    s = (const uint8_t *)l->ev;
    for (k = 0; k < bytes - LP_HDR; k++)
        d[LP_HDR + k] = s[k];
    return bytes;
}

int looper_load(looper_t *l, const void *src, uint32_t n)
{
    const uint8_t *s = (const uint8_t *)src;
    lp_hdr_t h;
    uint8_t *d = (uint8_t *)&h;
    uint32_t k;
    if (n < LP_HDR)
        return -1;
    for (k = 0; k < LP_HDR; k++)
        d[k] = s[k];
    if (h.magic != LP_MAGIC || h.n > LP_EVENTS || n != LP_HDR + (uint32_t)h.n * sizeof(lp_ev_t) ||
        h.len > LP_MAX_LEN || h.layers > LP_LAYERS || h.speed > LP_DOUBLE)
        return -1;
    looper_init(l);
    if (!h.n || !h.len || (h.state != LS_PLAY && h.state != LS_STOP))
        return 0;                                /* (an empty one: empty) */
    d = (uint8_t *)l->ev;
    for (k = 0; k < (uint32_t)h.n * sizeof(lp_ev_t); k++)
        d[k] = s[LP_HDR + k];
    for (k = 0; k < h.n; k++) {                  /* (each where it can be) */
        l->ev[k].t %= h.len;
        l->ev[k].kind = l->ev[k].kind == LK_HIT ? LK_HIT : LK_NOTE;
    }
    l->n = h.n;
    l->len = h.len;
    l->layers = h.layers ? h.layers : 1u;
    l->speed = h.speed;
    l->state = h.state;                          /* (playing: it plays when the clock starts) */
    return 0;
}
