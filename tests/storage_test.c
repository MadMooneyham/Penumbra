/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Host test of firmware/src/storage.c against a simulated NOR flash:
 * erase -> 0xFF, program can only clear bits, page writes must not wrap. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static uint8_t nor[0x100000];
static int fail_after = -1;            /* torn-write injection: stop after N programs */

static int st_read(uint32_t off, void *dst, uint32_t n) { memcpy(dst, nor + off, n); return 0; }
static int st_erase(uint32_t off) { memset(nor + off, 0xFF, 4096); return 0; }
static int st_prog(uint32_t off, const void *src, uint32_t n)
{
    const uint8_t *s = src;
    uint32_t i;
    if (fail_after == 0)
        return -9;
    if (fail_after > 0)
        fail_after--;
    if ((off & 0xFFu) + n > 256u) {
        printf("page wrap at %#x\n", off);
        exit(1);
    }
    for (i = 0; i < n; i++)
        nor[off + i] &= s[i];
    return 0;
}
#include "../firmware/src/app/plat.h"     /* the object map */
#include "../firmware/src/storage.c"
#define OBJ_DATA OBJ_PANEL                      /* the test's data object */
#define OBJ_SETTINGS OBJ_SET

static int check(const char *what, int ok)
{
    printf("%-46s %s\n", what, ok ? "ok" : "FAIL");
    return ok ? 0 : 1;
}

int main(void)
{
    char a[600], b[600], got[600];
    int bad = 0, n;
    memset(nor, 0xFF, sizeof nor);
    memset(a, 'A', sizeof a);
    memset(b, 'B', sizeof b);
    bad += check("empty flash loads nothing", st_load(OBJ_DATA, got, sizeof got) < 0);
    bad += check("save A", st_save(OBJ_DATA, a, sizeof a) == 0);
    n = st_load(OBJ_DATA, got, sizeof got);
    bad += check("load returns A", n == (int)sizeof a && !memcmp(got, a, sizeof a));
    bad += check("save B", st_save(OBJ_DATA, b, sizeof b) == 0);
    n = st_load(OBJ_DATA, got, sizeof got);
    bad += check("load returns B (newer seq)", n == (int)sizeof b && !memcmp(got, b, sizeof b));
    fail_after = 1;                                  /* payload page 1 written, the rest torn */
    st_save(OBJ_DATA, a, sizeof a);
    fail_after = -1;
    n = st_load(OBJ_DATA, got, sizeof got);
    bad += check("torn save keeps B", n == (int)sizeof b && !memcmp(got, b, sizeof b));
    fail_after = 3;                                  /* all payload pages, header torn */
    st_save(OBJ_DATA, a, sizeof a);
    fail_after = -1;
    n = st_load(OBJ_DATA, got, sizeof got);
    bad += check("header not written keeps B", n == (int)sizeof b && !memcmp(got, b, sizeof b));
    bad += check("save A again", st_save(OBJ_DATA, a, sizeof a) == 0);
    n = st_load(OBJ_DATA, got, sizeof got);
    bad += check("load returns A", n == (int)sizeof a && !memcmp(got, a, sizeof a));
    nor[st_sector(OBJ_DATA, 0) + ST_PAYLOAD_OFF + 10] ^= 0x01;   /* bit rot in one copy */
    nor[st_sector(OBJ_DATA, 1) + ST_PAYLOAD_OFF + 10] ^= 0x01;
    n = st_load(OBJ_DATA, got, sizeof got);
    bad += check("both copies corrupt -> nothing", n < 0);
    bad += check("other objects untouched", st_load(OBJ_SETTINGS, got, sizeof got) < 0);
    bad += check("no such object: not saved", st_save(1, a, 16) < 0 && st_load(1, got, sizeof got) < 0);
    bad += check("settings save/load",
                 st_save(OBJ_SETTINGS, "hello", 5) == 0 && st_load(OBJ_SETTINGS, got, 5) == 5 && !memcmp(got, "hello", 5));
    bad += check("CRC-32 is zlib's (check value 0xCBF43926)", st_crc32("123456789", 9) == 0xCBF43926u);
    memset(nor, 0xFF, sizeof nor);
    st_save(OBJ_DATA, a, sizeof a);
    st_save(OBJ_DATA, b, sizeof b);              /* B is newer, in the other copy */
    {
        st_hdr_t h;
        int cur = st_current(OBJ_DATA, &h);
        nor[st_sector(OBJ_DATA, (uint32_t)cur) + ST_PAYLOAD_OFF + 300] ^= 0x10;   /* rot in the newer copy */
    }
    n = st_load(OBJ_DATA, got, sizeof got);
    bad += check("newer copy rotten -> the older one loads", n == (int)sizeof a && !memcmp(got, a, sizeof a));
    bad += check("save over the rotten copy", st_save(OBJ_DATA, b, sizeof b) == 0);
    n = st_load(OBJ_DATA, got, sizeof got);
    bad += check("... loads the new data", n == (int)sizeof b && !memcmp(got, b, sizeof b));
    nor[st_sector(OBJ_DATA, 0) + 8] ^= 0x01;    /* both headers broken */
    nor[st_sector(OBJ_DATA, 1) + 8] ^= 0x01;
    bad += check("both headers broken -> nothing", st_load(OBJ_DATA, got, sizeof got) < 0);
    {   /* the map: each object in a data region, its two copies side by side, no overlaps */
        static const uint32_t O[2] = {OBJ_SET, OBJ_PANEL};
        uint32_t o, ok = 1;
        for (o = 0; o < 2; o++) {
            uint32_t a0 = st_sector(O[o], 0), a1 = st_sector(O[o], 1) + 4096;
            int in_data = (a0 >= 0xDC000 && a1 <= 0xE0000) || (a0 >= 0xFC000 && a1 <= 0xFF000);
            if (!in_data || a1 - a0 != 8192)
                ok = 0;
        }
        if (st_sector(OBJ_SET, 0) < st_sector(OBJ_PANEL, 1) + 4096 && st_sector(OBJ_PANEL, 0) < st_sector(OBJ_SET, 1) + 4096)
            ok = 0;
        bad += check("objects in data regions, apart", (int)ok);
    }
    printf("%s\n", bad ? "STORAGE TEST FAILED" : "storage test passed");
    return bad != 0;
}
