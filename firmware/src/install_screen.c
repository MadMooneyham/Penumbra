/* SPDX-License-Identifier: GPL-3.0-only */
/* The install screen: what an install is doing, from the first read to the restart. Drawn straight to the
 * panel (lcd_fill / lcd_blit), integers only, a glyph at a time from one small buffer, so the update loader
 * (firmware/loader/loader.c: no FPU, no canvas, every wait bounded) draws it as Penumbra does (penumbra.c
 * ota_show). The host renders it too (penumbra_host's 'install' command).
 *
 *   is_open(title)            the night, the mark, the title; the bar empty
 *   is_step(text)             the line under the title
 *   is_bar(done, total)       the bar and its percentage (only what moved is sent)
 *   is_note(text)             the line at the bottom
 *
 * Needs, before it is included: felucca_font_t and FONT_S / FONT_B / FONT_XS (gfx.c's felucca_font.h),
 * LOGO_S_* (logo.h), lcd_fill and lcd_blit. */
#ifndef IS_FONT_TITLE                              /* (the loader has room for one font: FONT_S for all three) */
#define IS_FONT_TITLE FONT_B
#define IS_FONT_NOTE FONT_XS
#endif
#define IS_BG 0x0042u                            /* RGB(7, 8, 16): the night (gfx.c C_BG) */
#define IS_MOON 0xE71Du                            /* RGB(226, 224, 236) */
#define IS_HI 0xEF5Eu                              /* RGB(236, 234, 246) */
#define IS_GRAY 0x8C75u                            /* RGB(138, 143, 174) */
#define IS_LINE 0x1906u                            /* RGB(30, 33, 52) */
#define IS_AMB 0xFCA8u                             /* RGB(255, 150, 70): the corona */
#define IS_BAR_X 30
#define IS_BAR_W 180
#define IS_BAR_Y 148
static uint16_t is_px[240 * 16];                   /* a line of text (glyphs overlap: a line at a time), or the mark */
static int is_shown = -1;                          /* the bar's filled width sent (-1: none yet) */
static int is_pct = -1;

static uint16_t is_mix(uint16_t a, uint16_t b, uint32_t t)   /* a to b, t 0..15 */
{
    uint32_t r = ((a >> 11) * (15u - t) + (b >> 11) * t) / 15u, g = (((a >> 5) & 63u) * (15u - t) + ((b >> 5) & 63u) * t) / 15u,
             bl = ((a & 31u) * (15u - t) + (b & 31u) * t) / 15u;
    uint32_t c = (r << 11) | (g << 5) | bl;
    return (uint16_t)(((c >> 8) & 0xFFu) | ((c & 0xFFu) << 8));   /* (the panel's byte order) */
}

static int is_text_w(const felucca_font_t *f, const char *s)
{
    int w = 0;
    for (; *s; s++)
        w += f->adv[((uint8_t)*s < f->first || (uint8_t)*s > f->last ? '?' : (uint8_t)*s) - f->first];
    return w;
}

/* s centred on the screen at y: the whole line, the screen's width, one transfer */
static void is_text(int y, const felucca_font_t *f, const char *s, uint16_t fg)
{
    const uint32_t h = f->h > 16u ? 16u : f->h, n = 240u * h;
    int x = (240 - is_text_w(f, s)) / 2;
    uint32_t i;
    lcd_sync();                                    /* (the last transfer may still read is_px) */
    for (i = 0; i < n; i++)
        is_px[i] = 0;                              /* (each pixel's coverage first, 0..15) */
    for (; *s; s++) {
        uint32_t ch = (uint8_t)*s < f->first || (uint8_t)*s > f->last ? '?' : (uint8_t)*s, gi = ch - f->first;
        uint32_t w = f->bw[gi], bpr = (w + 1u) / 2u, gx, gy;
        const uint8_t *gd = f->data + f->off[gi];
        for (gy = 0; gy < h; gy++)
            for (gx = 0; gx < w; gx++) {
                int px = x - f->pad + (int)gx;
                uint32_t a = gd[gy * bpr + gx / 2u];
                a = (gx & 1u) ? (a & 15u) : (a >> 4);
                if (px >= 0 && px < 240 && a > is_px[gy * 240u + (uint32_t)px])
                    is_px[gy * 240u + (uint32_t)px] = (uint16_t)a;
            }
        x += f->adv[gi];
    }
    for (i = 0; i < n; i++)
        is_px[i] = is_mix(IS_BG, fg, is_px[i]);
    lcd_blit(0, (uint32_t)y, 240, h, is_px);
}

static void is_open(const char *title)
{
    uint32_t x, y;
    lcd_fill(0, 0, 240, 240, IS_BG);
    lcd_sync();
    for (y = 0; y < LOGO_S_H; y++)                /* the Lunar Eclipse Forge mark, in moonlight */
        for (x = 0; x < LOGO_S_W; x++) {
            uint32_t a = LOGO_S_PX[y * ((LOGO_S_W + 1u) / 2u) + x / 2u];
            is_px[y * LOGO_S_W + x] = is_mix(IS_BG, IS_MOON, (x & 1u) ? (a & 15u) : (a >> 4));
        }
    lcd_blit((240u - LOGO_S_W) / 2u, 26, LOGO_S_W, LOGO_S_H, is_px);
    is_text(92, &IS_FONT_TITLE, title, IS_HI);
    lcd_fill(IS_BAR_X, IS_BAR_Y, IS_BAR_W, 4, IS_LINE);
    is_shown = 0;
    is_pct = -1;
}

static void is_step(const char *s) { is_text(116, &FONT_S, s, IS_GRAY); }
static void is_note(const char *s) { is_text(206, &IS_FONT_NOTE, s, IS_GRAY); }

static void is_bar(uint32_t done, uint32_t total)
{
    int w, pct;                                    /* (32 bits: done is under 1 MiB) */
    if (done > total)
        done = total;
    w = total ? (int)(done * IS_BAR_W / total) : 0;
    pct = total ? (int)(done * 100u / total) : 0;
    char b[6];
    int k = 0;
    if (w > IS_BAR_W)
        w = IS_BAR_W;
    if (pct > 100)
        pct = 100;
    if (is_shown < 0)
        is_shown = 0;
    if (w > is_shown)
        lcd_fill((uint32_t)(IS_BAR_X + is_shown), IS_BAR_Y, (uint32_t)(w - is_shown), 4, IS_AMB);
    else if (w < is_shown)
        lcd_fill((uint32_t)(IS_BAR_X + w), IS_BAR_Y, (uint32_t)(is_shown - w), 4, IS_LINE);
    is_shown = w;
    if (pct == is_pct)
        return;
    is_pct = pct;
    if (pct >= 100)
        b[k++] = '1', b[k++] = '0', b[k++] = '0';
    else if (pct >= 10)
        b[k++] = (char)('0' + pct / 10), b[k++] = (char)('0' + pct % 10);
    else
        b[k++] = (char)('0' + pct);
    b[k++] = '%';
    b[k] = 0;
    is_text(160, &FONT_S, b, IS_HI);
}
