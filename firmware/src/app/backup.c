/* SPDX-License-Identifier: GPL-3.0-only */
/* Backups: the site's Backup page (web/backup/) reads what the unit keeps, and puts a backup back, over
 * USB-MIDI SysEx in Felucca's update framing (ota.c: F0, then 7-bit packed 00 59 CMD LEN24 BODY CHK, F7;
 * CHK = ~sum(BODY)). Part of the unity build after ui.c (and the host's, and the browser's).
 *
 *   0x50 HELLO    ()                         -> 1, version[8], journey format, settings format, N,
 *                                               N x (part, length lo, hi): what is saved now (what had
 *                                               changed is saved first)
 *   0x51 READ     part, off16, len16 (<=256) -> status, part, off16, total16, the bytes
 *   0x52 WRITE    part, off16, total16, bytes-> status, part, have16   (in order from 0, into RAM)
 *   0x53 COMMIT   part                       -> status, part           (checked, then saved)
 *   0x54 RESTART  ()                         -> status; the unit starts again with what was restored
 *   0x55 JOURNEY  dest, off16, total16, bytes-> status, dest, have16   (the journey editor's: one journey_t, in
 *                                               order from 0; the last chunk in, it goes into user slot dest
 *                                               (0..7, saved) or, dest 0x7F, plays now; no restart)
 * status: 0 ok, 1 not a part, 2 nothing saved, 3 out of order, 4 not one this firmware takes, 5 not saved.
 * The parts: the settings, the journey playing (and its A/B), the user journeys, the loop, the sounds; never
 * the panel's calibration (the unit's own). From the first WRITE nothing is saved over the restore; 20 s
 * after the last command with something restored the unit starts again by itself. */
#include "penumbra.h"

static int bk_wire_send(const uint8_t *p, uint32_t n);   /* the platform's: one F0..F7 message out */
static void bk_restart(void);                            /* the platform's: start again from flash */

#define BK_PROTO 1u
#define BK_CHUNK 256u
#define BK_IDLE_MS 20000u
enum { BK_HELLO = 0x50, BK_READ, BK_WRITE, BK_COMMIT, BK_RESTART, BK_JOURNEY };
#define BK_JPLAY 0x7Fu
enum { BS_OK, BS_PART, BS_EMPTY, BS_ORDER, BS_BAD, BS_FLASH };
static const uint8_t BK_PARTS[] = {OBJ_SET, OBJ_WORK, OBJ_USER, OBJ_LOOP, OBJ_SOUNDS};
#define BK_NPARTS (sizeof BK_PARTS)

static uint8_t bk_buf[PLAT_STORE_MAX] PEN_POOL;          /* the part read, or the part coming in */
static union { journey_t j; uint8_t b[sizeof(journey_t)]; } bk_j;   /* JOURNEY's, coming in */
static struct { int dest; uint32_t have; } bkj = {-1, 0};
static uint8_t bk_msg[8 + 8 + BK_CHUNK], bk_out[2 + (8 + 8 + BK_CHUNK) * 8 / 7 + 2], bk_in[560];
static struct {
    int cached;                                          /* the part in bk_buf as read (-1: none) */
    int len;                                             /* its length (< 0: nothing saved) */
    int stage;                                           /* the part coming in (-1: none) */
    uint32_t total, have;
    uint8_t hold, restored, restart;
    uint32_t last, restart_at;
} bk = {-1, -1, -1, 0, 0, 0, 0, 0, 0, 0};

static int bk_part(uint32_t p)
{
    uint32_t i;
    for (i = 0; i < BK_NPARTS; i++)
        if (BK_PARTS[i] == p)
            return 1;
    return 0;
}

static uint32_t bk_pack7(const uint8_t *in, uint32_t n, uint8_t *out)   /* 8-to-7, LSB first (ota.c's) */
{
    uint32_t acc = 0, nb = 0, o = 0;
    while (n--) {
        acc |= (uint32_t)*in++ << nb;
        nb += 8u;
        while (nb >= 7u) {
            out[o++] = (uint8_t)(acc & 0x7Fu);
            acc >>= 7;
            nb -= 7u;
        }
    }
    if (nb)
        out[o++] = (uint8_t)(acc & 0x7Fu);
    return o;
}

static uint32_t bk_unpack7(const uint8_t *in, uint32_t n, uint8_t *out, uint32_t max)
{
    uint32_t acc = 0, nb = 0, o = 0;
    while (n--) {
        acc |= (uint32_t)(*in++ & 0x7Fu) << nb;
        nb += 7u;
        if (nb >= 8u) {
            if (o < max)
                out[o] = (uint8_t)acc;
            o++;
            acc >>= 8;
            nb -= 8u;
        }
    }
    return o;
}

static void bk_send(uint32_t cmd, const uint8_t *head, uint32_t nh, const uint8_t *data, uint32_t nd)
{
    uint32_t i, s = 0, n = nh + nd, w;
    bk_msg[0] = 0;
    bk_msg[1] = 0x59;
    bk_msg[2] = (uint8_t)cmd;
    bk_msg[3] = (uint8_t)n;
    bk_msg[4] = (uint8_t)(n >> 8);
    bk_msg[5] = 0;
    for (i = 0; i < n; i++) {
        uint8_t b = i < nh ? head[i] : data[i - nh];
        bk_msg[6 + i] = b;
        s += b;
    }
    bk_msg[6 + n] = (uint8_t)~s;
    bk_out[0] = 0xF0;
    w = 1u + bk_pack7(bk_msg, 7u + n, bk_out + 1);
    bk_out[w++] = 0xF7;
    bk_wire_send(bk_out, w);
}

static void bk_reply(uint32_t cmd, uint32_t status, uint32_t part, uint32_t v)   /* status, part, v16 */
{
    uint8_t h[4] = {(uint8_t)status, (uint8_t)part, (uint8_t)v, (uint8_t)(v >> 8)};
    bk_send(cmd, h, cmd == BK_RESTART ? 1u : cmd == BK_COMMIT ? 2u : 4u, 0, 0);
}

static void bk_hello(void)
{
    uint8_t h[12 + 3 * BK_NPARTS];
    const char *v = PEN_VERSION;
    uint32_t i, k = 0;
    if (!bk.hold) {                                      /* what changed, and what was never saved: in the backup */
        ui_flush();
        if (plat_store_load(OBJ_SET, bk_buf, sizeof bk_buf) < 0)
            settings_save();
        if (plat_store_load(OBJ_WORK, bk_buf, sizeof bk_buf) < 0)
            journey_save_work();
        if (plat_store_load(OBJ_LOOP, bk_buf, sizeof bk_buf) < 0)
            loop_save_work();
    }
    bk.cached = -1;
    h[k++] = BK_PROTO;
    for (i = 0; i < 8u; i++)
        h[k++] = (uint8_t)(*v ? *v++ : 0);
    h[k++] = JR_FORMAT;
    h[k++] = SET_FORMAT;
    h[k++] = (uint8_t)BK_NPARTS;
    for (i = 0; i < BK_NPARTS; i++) {
        int n = plat_store_load(BK_PARTS[i], bk_buf, sizeof bk_buf);
        if (n < 0)
            n = 0;
        h[k++] = BK_PARTS[i];
        h[k++] = (uint8_t)n;
        h[k++] = (uint8_t)(n >> 8);
    }
    bk_send(BK_HELLO, h, k, 0, 0);
    ui_say("BACKUP", "CONNECTED");
}

static void bk_read(const uint8_t *b, uint32_t n)
{
    uint32_t part, off, len;
    uint8_t h[6];
    if (n < 5u)
        return;
    part = b[0];
    off = b[1] | (uint32_t)b[2] << 8;
    len = b[3] | (uint32_t)b[4] << 8;
    if (!bk_part(part)) {
        bk_reply(BK_READ, BS_PART, part, off);
        return;
    }
    if (bk.stage >= 0)                                   /* (a restore half in: it starts again) */
        bk.stage = -1;
    if (bk.cached != (int)part) {
        bk.len = plat_store_load(part, bk_buf, sizeof bk_buf);
        bk.cached = (int)part;
    }
    if (bk.len < 0) {
        bk_reply(BK_READ, BS_EMPTY, part, off);
        return;
    }
    if (len > BK_CHUNK)
        len = BK_CHUNK;
    if (off > (uint32_t)bk.len)
        off = (uint32_t)bk.len;
    if (off + len > (uint32_t)bk.len)
        len = (uint32_t)bk.len - off;
    h[0] = BS_OK;
    h[1] = (uint8_t)part;
    h[2] = (uint8_t)off;
    h[3] = (uint8_t)(off >> 8);
    h[4] = (uint8_t)bk.len;
    h[5] = (uint8_t)(bk.len >> 8);
    bk_send(BK_READ, h, sizeof h, bk_buf + off, len);
}

static void bk_write(const uint8_t *b, uint32_t n)
{
    uint32_t part, off, total, k;
    if (n < 5u)
        return;
    part = b[0];
    off = b[1] | (uint32_t)b[2] << 8;
    total = b[3] | (uint32_t)b[4] << 8;
    n -= 5u;
    if (!bk_part(part)) {
        bk_reply(BK_WRITE, BS_PART, part, 0);
        return;
    }
    if (!bk.hold) {
        bk.hold = 1;
        ui_say("RESTORING", "A BACKUP");
    }
    if (off == 0) {
        bk.stage = (int)part;
        bk.total = total;
        bk.have = 0;
        bk.cached = -1;
    }
    if (bk.stage != (int)part || off != bk.have || total != bk.total || total > sizeof bk_buf ||
        off + n > total) {
        bk_reply(BK_WRITE, BS_ORDER, part, bk.stage == (int)part ? bk.have : 0);
        return;
    }
    for (k = 0; k < n; k++)
        bk_buf[off + k] = b[5 + k];
    bk.have += n;
    bk_reply(BK_WRITE, BS_OK, part, bk.have);
}

static void bk_commit(const uint8_t *b, uint32_t n)
{
    uint32_t part = n ? b[0] : 0xFFu;
    if (!bk_part(part)) {
        bk_reply(BK_COMMIT, BS_PART, part, 0);
        return;
    }
    if (bk.stage != (int)part || bk.have != bk.total) {
        bk_reply(BK_COMMIT, BS_ORDER, part, 0);
        return;
    }
    bk.stage = -1;
    if (store_check(part, bk_buf, bk.total)) {
        bk_reply(BK_COMMIT, BS_BAD, part, 0);
        return;
    }
    if (plat_store_save(part, bk_buf, bk.total)) {
        bk_reply(BK_COMMIT, BS_FLASH, part, 0);
        return;
    }
    bk.restored = 1;
    bk_reply(BK_COMMIT, BS_OK, part, 0);
}

static void bk_journey(const uint8_t *b, uint32_t n)
{
    uint32_t dest, off, total, k;
    int r;
    if (n < 5u)
        return;
    dest = b[0];
    off = b[1] | (uint32_t)b[2] << 8;
    total = b[3] | (uint32_t)b[4] << 8;
    n -= 5u;
    if (dest != BK_JPLAY && dest >= JR_USER) {
        bk_reply(BK_JOURNEY, BS_PART, dest, 0);
        return;
    }
    if (off == 0) {
        bkj.dest = (int)dest;
        bkj.have = 0;
    }
    if (bkj.dest != (int)dest || off != bkj.have || (total != sizeof bk_j.b && total != JR_SIZE_12) || off + n > total) {
        bk_reply(BK_JOURNEY, total != sizeof bk_j.b && total != JR_SIZE_12 ? BS_BAD : BS_ORDER, dest,
                 bkj.dest == (int)dest ? bkj.have : 0);
        return;
    }
    for (k = 0; k < n; k++)
        bk_j.b[off + k] = b[5 + k];
    bkj.have += n;
    if (bkj.have < total) {
        bk_reply(BK_JOURNEY, BS_OK, dest, bkj.have);
        return;
    }
    bkj.dest = -1;
    r = journey_put(dest == BK_JPLAY ? -1 : (int)dest, bk_j.b, total);
    bk_reply(BK_JOURNEY, r == 0 ? BS_OK : r == 1 ? BS_BAD : BS_FLASH, dest, total);
    if (r == 0) {
        if (dest == BK_JPLAY) {
            ui_say("JOURNEY IN", "PLAYING NOW");
        } else {
            char t[8] = "USER ";
            t[5] = (char)('1' + dest);
            t[6] = 0;
            ui_say("JOURNEY IN", t);
        }
    }
}

int bk_frame(const uint8_t *p, uint32_t n)
{
    uint32_t d, s = 0, i, len;
    if (n < 8u || p[0] != 0x00)                          /* (00 59 packed: 00 32 ..; HELLO's 7 bytes in 8) */
        return 0;
    d = bk_unpack7(p, n, bk_in, sizeof bk_in);
    if (d < 7u || d > sizeof bk_in || bk_in[1] != 0x59 || bk_in[2] < BK_HELLO || bk_in[2] > BK_JOURNEY)
        return 0;
    len = bk_in[3] | (uint32_t)bk_in[4] << 8 | (uint32_t)bk_in[5] << 16;
    if (len + 7u != d)
        return 0;
    for (i = 6; i + 1u < d; i++)
        s += bk_in[i];
    if ((uint8_t)~s != bk_in[d - 1u])
        return 1;                                        /* (ours, broken on the way: the page asks again) */
    bk.last = plat_ms();
    switch (bk_in[2]) {
    case BK_HELLO: bk_hello(); break;
    case BK_READ: bk_read(bk_in + 6, len); break;
    case BK_WRITE: bk_write(bk_in + 6, len); break;
    case BK_COMMIT: bk_commit(bk_in + 6, len); break;
    case BK_JOURNEY: bk_journey(bk_in + 6, len); break;
    default:
        bk_reply(BK_RESTART, BS_OK, 0, 0);
        bk.restart = 1;
        bk.restart_at = plat_ms() + 200u;                /* (the answer out first) */
        ui_say("RESTORED", "STARTING AGAIN");
        break;
    }
    return 1;
}

void bk_tick(uint32_t now)
{
    if (bk.hold && !bk.restart && now - bk.last > BK_IDLE_MS) {
        if (bk.restored) {                               /* (the page went before it said: start again anyway) */
            bk.restart = 1;
            bk.restart_at = now;
        } else {
            bk.hold = 0;                                 /* nothing came in: as it was */
            bk.stage = -1;
        }
    }
    if (bk.restart && (int32_t)(now - bk.restart_at) >= 0) {
        bk.restart = bk.hold = bk.restored = 0;
        bk.stage = bk.cached = -1;
        bk_restart();
    }
}

int bk_holding(void) { return bk.hold; }
