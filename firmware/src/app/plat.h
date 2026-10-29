/* SPDX-License-Identifier: GPL-3.0-only */
/* Penumbra's platform interface: everything the app needs from the hardware.
 *
 * The device implementation (plat_fm1.c) wraps Felucca's HAL and panel map; the host
 * implementation (host/penumbra_host.c) is a simulator fed by a script, which is what lets the
 * whole app (UI, engine, every voice) run and be tested off the device. Drawing goes through
 * Felucca's lcd_fill / lcd_blit (lcd.c on the device, a framebuffer on the host) and gfx.c. */
#pragma once
#include <stdint.h>

/* big zero-initialised buffers go in the .pool RAM region on the device */
#ifdef PEN_HOST
#define PEN_POOL
#else
#define PEN_POOL __attribute__((section(".pool")))
#endif

/* panel controls, by printed label (Felucca's panel.c map turns these into matrix ids; Felucca
 * calls the second button SCL, the panel prints SEL) */
enum { B_FX, B_SEL, B_ENV, B_LFO, B_EDIT, B_GLO, B_HOME, B_SAVE, B_ARP, B_SEQ, B_PLAY, B_REC,
       B_OCTDN, B_OCTUP, NB };
enum { EN_SELECT, EN_ALGO, EN_PRESET, EN_K1, EN_K2, EN_K3, EN_K4, NE };

#define NKEYS 27                       /* F3 .. G5: 16 white, 11 black */

uint32_t plat_ms(void);
uint32_t plat_buttons(void);           /* held: bit B_* */
uint32_t plat_keys(void);              /* held: bit k = key k (0 = F3) */
int32_t plat_enc(int role);            /* detents since the last call, + = clockwise; EN_* */
uint32_t plat_master(void);            /* MASTER pot, 0..4096 gain Q12 */
int plat_battery(void);                /* 0..3 bars, 4 = powered by a USB host, -1 = not read yet */
void plat_leds(uint32_t buttons, uint32_t keys);   /* lit: bit B_*, bit key */
void plat_glow(int on);                /* the unlit buttons glow dimly, readable in the dark (call before plat_leds) */
void plat_keys_dim(uint32_t keys);     /* these keys light dim (a second level; call before plat_leds) */
void plat_play_red(int red);           /* PLAY lights green; red (its other LED) instead (before plat_leds) */

/* MIDI over USB and the TRS jack, 4-byte USB-MIDI event packets (cable 0) */
int plat_midi_in(uint32_t *pkt);       /* 1 = got one (called from the audio ISR) */
void plat_midi_out(uint32_t pkt);      /* queue one (audio ISR or main loop) */

/* persistent storage (Felucca storage.c objects: A/B sectors, CRC, torn-write safe). An object's
 * number is written into its flash record, so these numbers never change: OBJ_PANEL is 4 as in
 * X0X and Felucca, so a unit's calibration carries over. */
int plat_store_load(uint32_t obj, void *dst, uint32_t max);   /* bytes loaded, < 0 = none / bad */
int plat_store_save(uint32_t obj, const void *src, uint32_t len);   /* 0 = ok */
#define PLAT_STORE_MAX 3840u           /* bytes per object */
enum { OBJ_SET = 0, OBJ_PANEL = 4, OBJ_WORK = 8, OBJ_USER = 9, OBJ_LOOP = 10 };
/* flash: OBJ_SET 0xFC000, OBJ_PANEL 0xDC000, OBJ_WORK 0x97000 (the journey playing), OBJ_USER
 * 0x99000 (the 8 user journeys), OBJ_LOOP 0x9B000 (the loop) (storage.c). 0x9D000..0x9EFFF and 0xDE000
 * are free. OBJ_WORK and OBJ_USER are numbers X0X never used, so its old records there never load as
 * journeys; the loop has its own magic too. */

/* the audio render's load, in % of its budget, and the blocks it missed */
uint32_t plat_cpu_pct(void);
uint32_t plat_cpu_peak_pct(void);      /* the highest since the last call */
uint32_t plat_xruns(void);
/* the second core: a job of the render runs there while the first renders the rest (engine.c) */
int plat_cpu2_set(int on);             /* main loop: 0 = done; -1 = it did not start (one core) */
int plat_cpu2_on(void);
int plat_cpu2_run(void (*fn)(uint32_t), uint32_t arg);   /* ISR: 1 = the second core has it */
int plat_cpu2_wait(void);              /* ISR: 0 = done; -1 = it failed (and is off): do the job here */
int plat_cpu2_gave_up(void);           /* why it is off: 0 no, 1 it read a wrong job (fm1-x0x#10), 2 one did not finish */
uint32_t plat_cpu2_bad(void);          /* wrong jobs it saw (0 on a unit that runs both cores) */
int plat_cpu2_barred(void);            /* 1: since power-on it was given up, or core 0 crashed beside it */
int plat_last_crash(uint32_t v[5]);    /* 1: a crash since power-on (vec, pc, emu, dbg, rets as its red screen) */

/* timing for the benchmark: the 24 MHz timer (the same on both cores), and what the CPU's own
 * clock runs at, measured against it (0 = not known: no cycle counter) */
uint32_t plat_ticks24(void);
/* (24 MHz / 44.1 kHz = 80000 / 147: n * 24000000 passes 32 bits at n = 179, n * 80000 never does) */
#define PLAT_TICKS_PER_BLOCK(n) ((uint32_t)(n) * 80000u / 147u)
uint32_t plat_cpu_mhz10(void);         /* x 10 */
uint32_t plat_irq_stack_used(void);    /* the deepest the interrupts' stack has gone, bytes (0: unknown) */
uint32_t plat_irq_stack_size(void);
