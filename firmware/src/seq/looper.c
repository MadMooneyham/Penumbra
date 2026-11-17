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

static void layer_default(lp_layer_t *y)
{
    y->level = 100;
    y->snd = 0;
    y->oct = 1;
    y->mute = 0;
    y->pan = y->tone = y->swell = 64;
    y->drift = y->beats = 0;
    y->rsv[0] = y->rsv[1] = y->rsv[2] = 0;
}
static uint32_t layer_len(const looper_t *l, int y)   /* its own length, else the loop's */
{
    const uint32_t b = l->lay[y % LP_LAYERS].beats;
    return b ? b * LP_BEAT : l->len;
}
static int own_len(const looper_t *l, int y) { return l->lay[y % LP_LAYERS].beats && layer_len(l, y) != l->len; }

void looper_init(looper_t *l)
{
    int k;
    zero(l, sizeof *l);
    l->speed = LP_AS_PLAYED;
    for (k = 0; k < LP_LAYERS; k++)
        layer_default(&l->lay[k]);
}

static void quiet_layer(looper_t *l, int y, const lp_sink_t *o)
{
    uint32_t k;
    for (k = 0; k < 256u; k++)
        if (BIT(l->sounding[y], k)) {
            CLR(l->sounding[y], k);
            o->off(o->ctx, (int)k, y);
        }
}
void looper_quiet(looper_t *l, const lp_sink_t *o)
{
    int y;
    for (y = 0; y < LP_LAYERS; y++)
        quiet_layer(l, y, o);
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
    e->layer = (uint8_t)(l->layers < LP_LAYERS ? l->layers : LP_LAYERS - 1);
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

/* the events in (from, to] (and at from, incl_from) of layer only (-1: the layers of the loop's own length) */
static void fire(looper_t *l, uint32_t from, uint32_t to, int incl_from, const lp_sink_t *o, int only)
{
    uint32_t i;
    for (i = 0; i < l->n; i++) {
        const lp_ev_t *e = &l->ev[i];
        const int y = e->layer % LP_LAYERS;
        if (only < 0 ? own_len(l, y) : y != only)
            continue;
        if (e->t >= layer_len(l, y))
            continue;                            /* (a layer made shorter: its end cut) */
        const lp_layer_t *ly = &l->lay[y];
        uint32_t *on = l->sounding[y];
        int vel;
        if (!((e->t > from || (incl_from && e->t == from)) && e->t <= to))
            continue;
        vel = ((int)e->vel * (int)ly->level + 50) / 100;
        vel = vel < 1 ? 1 : vel > 127 ? 127 : vel;
        if ((e->kind & 15u) == LK_HIT) {
            if (!ly->mute && ly->level)
                o->hit(o->ctx, e->note, vel);
        } else if (e->vel) {
            if (ly->mute || !ly->level)
                continue;
            if (BIT(on, e->note))
                o->off(o->ctx, e->note, y);
            SET(on, e->note);
            o->on(o->ctx, e->note, vel, (ly->snd ? ly->snd : e->kind >> 4) | ly->oct << 4, y);
        } else if (BIT(on, e->note)) {
            CLR(on, e->note);
            o->off(o->ctx, e->note, y);
        }
    }
}

static void from_start(looper_t *l, const lp_sink_t *o)
{
    int y;
    l->pos = 0;
    l->frac = 0;
    fire(l, 0, 0, 1, o, -1);
    for (y = 0; y < LP_LAYERS; y++) {
        l->lpos[y] = 0;
        if (y < (int)l->layers && own_len(l, y))
            fire(l, 0, 0, 1, o, y);
    }
}

void looper_restart(looper_t *l, const lp_sink_t *o)
{
    if (l->state == LS_PLAY || l->state == LS_DUB) {
        looper_quiet(l, o);
        from_start(l, o);
    }
}

void looper_seek(looper_t *l, uint32_t ticks, const lp_sink_t *o)
{
    if ((l->state != LS_PLAY && l->state != LS_DUB) || !l->len)
        return;                                  /* (as looper_restart: a stopped loop stays stopped) */
    looper_quiet(l, o);
    if (l->speed == LP_DOUBLE)                   /* (where it would be, had it run from the downbeat) */
        ticks *= 2u;
    else if (l->speed == LP_HALF)
        ticks >>= 1;
    l->pos = ticks % l->len;
    l->frac = 0;
    fire(l, l->pos, l->pos, 1, o, -1);           /* (what starts there, plays) */
    {
        int y;
        for (y = 0; y < (int)l->layers && y < LP_LAYERS; y++)
            if (own_len(l, y)) {
                l->lpos[y] = ticks % layer_len(l, y);
                fire(l, l->lpos[y], l->lpos[y], 1, o, y);
            }
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
    layer_default(&l->lay[gone % LP_LAYERS]);
    l->edits++;
    looper_quiet(l, o);                          /* (what the rest has on comes back round) */
}

void looper_layer_set(looper_t *l, int y, int field, int value, const lp_sink_t *o)
{
    lp_layer_t *ly;
    if (y < 0 || y >= (int)l->layers || y >= LP_LAYERS || l->state == LS_EMPTY || l->state == LS_REC)
        return;
    ly = &l->lay[y];
    switch (field) {
    case LL_LEVEL: ly->level = (uint8_t)(value < 0 ? 0 : value > 127 ? 127 : value); break;
    case LL_SOUND: ly->snd = (uint8_t)(value < 0 ? 0 : value > 15 ? 15 : value); break;
    case LL_OCT: ly->oct = (uint8_t)(value < 0 ? 0 : value > 2 ? 2 : value); break;
    case LL_PAN:
    case LL_DRIFT:
    case LL_TONE:
    case LL_SWELL: (&ly->pan)[field - LL_PAN] = (uint8_t)(value < 0 ? 0 : value > 127 ? 127 : value); break;
    case LL_BEATS:                               /* its own length: from where the loop is, in it */
        ly->beats = (uint8_t)(value < 0 ? 0 : value > 64 ? 64 : value);
        quiet_layer(l, y, o);
        l->lpos[y] = l->pos % layer_len(l, y);
        break;
    case LL_MUTE:
        ly->mute = (uint8_t)(value != 0);
        if (ly->mute)
            quiet_layer(l, y, o);                /* (what it has on: let go now) */
        break;
    default: return;
    }
    l->edits++;
}

void looper_layer_delete(looper_t *l, int y, const lp_sink_t *o)
{
    uint32_t i, k = 0;
    int j;
    if (y < 0 || y >= (int)l->layers || l->state == LS_EMPTY || l->state == LS_REC || l->state == LS_DUB)
        return;                                  /* (overdubbing: PLAY held undoes that layer) */
    if (l->layers <= 1) {
        looper_clear(l, o);
        return;
    }
    looper_quiet(l, o);
    for (i = 0; i < l->n; i++)
        if (l->ev[i].layer != y) {
            l->ev[k] = l->ev[i];
            if (l->ev[k].layer > y)
                l->ev[k].layer--;                /* the ones over it: down one */
            k++;
        }
    l->n = (uint16_t)k;
    l->full = 0;
    for (j = y; j < LP_LAYERS - 1; j++)
        l->lay[j] = l->lay[j + 1], l->lpos[j] = l->lpos[j + 1];
    layer_default(&l->lay[LP_LAYERS - 1]);
    l->layers--;
    l->edits++;
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

static void record(looper_t *l, int kind, int note, int vel, int tag)
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
    add(l, l->pos, kind | (tag & 15) << 4, note, vel);
}
void looper_note(looper_t *l, int note, int vel, int tag) { record(l, LK_NOTE, note, vel, vel ? tag : 0); }
void looper_hit(looper_t *l, int which, int vel) { record(l, LK_HIT, which, vel ? vel : 1, 0); }

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
        fire(l, l->pos, to, 0, o, -1);
        l->pos = to;
    } else {                                     /* round the end, to the start */
        fire(l, l->pos, l->len - 1u, 0, o, -1);
        to -= l->len;
        fire(l, 0, to, 1, o, -1);
        l->pos = to;
    }
    {   /* the layers of their own length: each round in its own time; what one has on is let go at its end */
        int y;
        for (y = 0; y < (int)l->layers && y < LP_LAYERS; y++) {
            uint32_t yl, p, t2;
            if (!own_len(l, y))
                continue;
            yl = layer_len(l, y);
            p = l->lpos[y] % yl;
            t2 = p + ticks;
            if (t2 < yl) {
                fire(l, p, t2, 0, o, y);
                l->lpos[y] = t2;
            } else {
                fire(l, p, yl - 1u, 0, o, y);
                quiet_layer(l, y, o);
                t2 = (t2 - yl) % yl;
                fire(l, 0, t2, 1, o, y);
                l->lpos[y] = t2;
            }
        }
    }
}

/* ------------------------------------------------------------------- saving --- */
#define LP_MAGIC1 0x314C4E50u                    /* "PNL1" (to 1.4): the header, then the events */
#define LP_MAGIC2 0x324C4E50u                    /* "PNL2" (1.5): the header, four bytes a layer, the events */
#define LP_MAGIC 0x334C4E50u                     /* "PNL3": the header, the layers' settings, then the events */
#define LP_HDR 16u
typedef struct {
    uint32_t magic;
    uint16_t n, rsv;
    uint32_t len;
    uint8_t layers, state, speed, rsv2;
} lp_hdr_t;
_Static_assert(sizeof(lp_hdr_t) == LP_HDR && sizeof(lp_ev_t) == 8u && sizeof(lp_layer_t) == 12u &&
               LP_SAVE_HDR == LP_HDR + sizeof(((looper_t *)0)->lay), "the saved loop's layout");

uint32_t looper_save(const looper_t *l, void *dst, uint32_t max)
{
    lp_hdr_t h;
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s;
    uint32_t k, bytes = LP_SAVE_HDR + (uint32_t)l->n * sizeof(lp_ev_t);
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
    bytes = LP_SAVE_HDR + (uint32_t)h.n * sizeof(lp_ev_t);
    s = (const uint8_t *)&h;
    for (k = 0; k < LP_HDR; k++)
        d[k] = s[k];
    s = (const uint8_t *)l->lay;
    for (k = 0; k < sizeof l->lay; k++)
        d[LP_HDR + k] = s[k];
    s = (const uint8_t *)l->ev;
    for (k = 0; k < bytes - LP_SAVE_HDR; k++)
        d[LP_SAVE_HDR + k] = s[k];
    return bytes;
}

static int lp_head(const void *src, uint32_t n, lp_hdr_t *h, uint32_t *hdr)   /* 0: a saved loop's head, whole */
{
    const uint8_t *s = (const uint8_t *)src;
    uint8_t *d = (uint8_t *)h;
    uint32_t k;
    if (n < LP_HDR)
        return -1;
    for (k = 0; k < LP_HDR; k++)
        d[k] = s[k];
    *hdr = h->magic == LP_MAGIC ? LP_SAVE_HDR : h->magic == LP_MAGIC2 ? LP_HDR + LP_LAYERS * 4u
                                                                  : LP_HDR;   /* (1.4's: its layers as played) */
    if ((h->magic != LP_MAGIC && h->magic != LP_MAGIC2 && h->magic != LP_MAGIC1) || h->n > LP_EVENTS ||
        n != *hdr + (uint32_t)h->n * sizeof(lp_ev_t) || h->len > LP_MAX_LEN || h->layers > LP_LAYERS ||
        h->speed > LP_DOUBLE)
        return -1;
    return 0;
}

int looper_check(const void *src, uint32_t n)
{
    lp_hdr_t h;
    uint32_t hdr;
    return lp_head(src, n, &h, &hdr);
}

int looper_load(looper_t *l, const void *src, uint32_t n)
{
    const uint8_t *s = (const uint8_t *)src;
    lp_hdr_t h;
    uint8_t *d;
    uint32_t k, hdr;
    if (lp_head(src, n, &h, &hdr))
        return -1;
    looper_init(l);
    if (!h.n || !h.len || (h.state != LS_PLAY && h.state != LS_STOP))
        return 0;                                /* (an empty one: empty) */
    if (hdr != LP_HDR) {                         /* (1.5's: the first four of each, the rest as made) */
        const uint32_t each = h.magic == LP_MAGIC ? sizeof(lp_layer_t) : 4u;
        for (k = 0; k < LP_LAYERS * each; k++)
            ((uint8_t *)&l->lay[k / each])[k % each] = s[LP_HDR + k];
        for (k = 0; k < LP_LAYERS; k++) {        /* (each in range) */
            lp_layer_t *y = &l->lay[k];
            y->level &= 0x7Fu;
            y->snd &= 15u;
            y->oct = (uint8_t)(y->oct > 2 ? 1 : y->oct);
            y->mute = (uint8_t)(y->mute != 0);
            y->pan &= 0x7Fu;
            y->drift &= 0x7Fu;
            y->tone &= 0x7Fu;
            y->swell &= 0x7Fu;
            y->beats = (uint8_t)(y->beats > 64 ? 0 : y->beats);
        }
    }
    d = (uint8_t *)l->ev;
    for (k = 0; k < (uint32_t)h.n * sizeof(lp_ev_t); k++)
        d[k] = s[hdr + k];
    for (k = 0; k < h.n; k++) {                  /* (each where it can be) */
        l->ev[k].t %= h.len;
        l->ev[k].layer %= LP_LAYERS;
        l->ev[k].kind = (l->ev[k].kind & 15u) == LK_HIT ? LK_HIT : (uint8_t)(LK_NOTE | (l->ev[k].kind & 0xF0u));
    }
    l->n = h.n;
    l->len = h.len;
    l->layers = h.layers ? h.layers : 1u;
    l->speed = h.speed;
    l->state = h.state;                          /* (playing: it plays when the clock starts) */
    return 0;
}
