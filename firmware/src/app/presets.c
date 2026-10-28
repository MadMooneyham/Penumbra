/* SPDX-License-Identifier: GPL-3.0-only */
/* Penumbra's preset songs: whole projects (a sound, four patterns and a song) to play with, named after
 * the moon's seas. GLO > PRESET SONGS loads one over the project: PLAY then loops its four patterns
 * (a chain, P1-P4), the SONG screen plays its arrangement, and any of it can be changed and saved.
 *
 * The drones are SWELL voices the DRONE part's steps start: a voice HOLDs a little longer than the bar
 * (the DRONE pattern, 64 steps), so a chord a pattern starts again sounds on, and one the next pattern
 * leaves out fades over FALL. A pattern is written as text, read when the preset loads:
 *   drone  "1:0 5:0 6:32"      voice:step pairs (voices 1-6, steps from 0)
 *   808    "x...x...X..."      one row per track; x a hit, X a hit on the accent row; the length the row's
 *   melody "A4 =*7 - C5! D5~"  one token a step: a note (C4 = 60), "-" a rest, "=" a tie; "!" accents,
 *                              "~" slides into the next step; "*N" repeats the token N times */
#include "x0x.h"
#include "../dsp/drone.h"                      /* DN_*, DB_*, DRONE_KIT */
#include "../dsp/melody.h"                     /* MEL_* */
#include "../dsp/drum808.h"                    /* D8_* (the 808's tracks) */

#define NPRESETS 6
#define PR_PATS 4
enum { MU_DRN = 1 << PART_909, MU_808 = 1 << PART_808, MU_MA = 1 << PART_303A, MU_MB = 1 << PART_303B };

typedef struct { uint8_t t, v, i, val; } pr_pot_t;
typedef struct {
    const char *drone;
    const char *kit[NDRUM];               /* the 808's tracks, in D8_* order */
    const char *mel[NBASS];               /* MEL A, MEL B */
} pr_pat_t;
typedef struct { uint8_t pat, mute, rep; } pr_bar_t;
typedef struct {
    const char *name;
    uint16_t bpm_x10;
    const pr_pot_t *pots;                 /* over the factory sound; ends with t = 0xFF */
    pr_pat_t pat[PR_PATS];
    const pr_bar_t *song;                 /* ends with rep = 0 */
} preset_t;

#define END {0xFF, 0, 0, 0}
#define DV(v, i, x) {T_909, (v), (i), (x)}            /* a drone voice (0-5) */
#define DB(i, x) {T_909, DRONE_KIT, (i), (x)}         /* the drone bus */
#define MA(i, x) {T_303, 0, (i), (x)}
#define MB(i, x) {T_303, 1, (i), (x)}
#define MIX(p, i, x) {T_MIX, (p), (i), (x)}
#define FX(i, x) {T_FX, 0, (i), (x)}
#define FXX(i, x) {T_FX, 2, (i), (x)}
/* the six drones' notes (24 = the root) and their envelope: every voice a swell */
#define NOTES(a, b, c, d, e, f) DV(0, DN_NOTE, a), DV(1, DN_NOTE, b), DV(2, DN_NOTE, c), DV(3, DN_NOTE, d), \
                                DV(4, DN_NOTE, e), DV(5, DN_NOTE, f)
#define SWELL1(v, rise, fall, hold) DV(v, DN_MODE, DN_MODE_SWELL), DV(v, DN_ATTACK, rise), DV(v, DN_RELEASE, fall), \
                                    DV(v, DN_HOLD, hold)
#define SWELL(rise, fall, hold) SWELL1(0, rise, fall, hold), SWELL1(1, rise, fall, hold), SWELL1(2, rise, fall, hold), \
                                SWELL1(3, rise, fall, hold), SWELL1(4, rise, fall, hold), SWELL1(5, rise, fall, hold)
/* RISE / FALL: 0.02 s * 2^(9 pot / 127) (88 = 1.5 s, 102 = 3 s, 112 = 5 s, 120 = 7 s);
 * HOLD: 0.05 s * 2^(8.32 pot / 127) (115 = 9.3 s, 119 = 11 s, 122 = 12.7 s, 127 = 16 s) */

/* ---- TRANQUILITY: A minor, 72 BPM. The factory sound: Am, F, C and Am7 swelling into one another under a
 * slow gliding line, a bass that holds the root, and a soft 808 heartbeat once it has all come in. */
static const pr_pot_t TRQ_POTS[] = {
    NOTES(24, 20, 27, 22, 31, 36),                    /* A1 F1 C2 G1 E2 A2 */
    SWELL(102, 112, 127),                             /* a bar is 13.3 s: each chord rings 2.7 s into the next */
    MA(MEL_GLIDE, 70), MA(MEL_ATTACK, 70), MA(MEL_RELEASE, 105), MA(MEL_VIB, 30), MA(MEL_VOLUME, 85),
    MB(MEL_TUNE, 12), MB(MEL_CUTOFF, 50), MB(MEL_SUB, 80), MB(MEL_ATTACK, 60), MB(MEL_SUSTAIN, 100),
    MB(MEL_VOLUME, 90),
    MIX(PART_808, MX_LEVEL, 70),
    END};
static const pr_bar_t TRQ_SONG[] = {
    {0, MU_808 | MU_MA | MU_MB, 1}, {1, MU_808 | MU_MA | MU_MB, 1}, {2, MU_808 | MU_MA, 1}, {3, MU_808 | MU_MA, 1},
    {0, MU_808, 1}, {1, MU_808, 1}, {2, MU_808, 1}, {3, MU_808, 1},
    {0, 0, 1}, {1, 0, 1}, {2, 0, 1}, {3, 0, 1},
    {0, MU_808 | MU_MB, 1}, {0, MU_808 | MU_MA | MU_MB, 1}, {0, 0, 0}};
#define TRQ_KIT {[D8_BD] = "x.........x.....", [D8_RS] = "....x.......x..x", [D8_CH] = "..x...x...x...x."}

/* ---- SERENITY: D dorian, 60 BPM. Band-passed drones breathing with the wind, bell notes falling into the
 * shimmer and the delay; no drums, no bass. */
static const pr_pot_t SER_POTS[] = {
    DB(DB_ROOT, 5),                                   /* D1 */
    NOTES(36, 43, 46, 50, 51, 41),                    /* D2 A2 C3 E3 F3 G2 */
    SWELL(112, 120, 118),                             /* started every 8 s, rings 10.6 s */
    DV(0, DN_STACK, 2), DV(3, DN_SPREAD, 35), DV(4, DN_SPREAD, 35),
    DB(DB_FMODE, 1), DB(DB_CUTOFF, 70), DB(DB_RESO, 70), DB(DB_LFORATE, 20), DB(DB_LFOCUT, 50),
    DB(DB_WIND, 40), DB(DB_DRIFT, 45), DB(DB_GAIN, 127),   /* the band-pass is quieter: the bus up */
    MA(MEL_ATTACK, 0), MA(MEL_DECAY, 60), MA(MEL_SUSTAIN, 0), MA(MEL_RELEASE, 70), MA(MEL_WAVE, 0),
    MA(MEL_RING, 60), MA(MEL_RINGHZ, 90), MA(MEL_CUTOFF, 100), MA(MEL_ENVMOD, 40), MA(MEL_RESO, 20),
    MA(MEL_GLIDE, 0), MA(MEL_VIB, 0), MA(MEL_SUB, 0), MA(MEL_VOLUME, 110),
    MIX(PART_303A, MX_REV, 90), MIX(PART_303A, MX_DLY, 90),
    FX(FX_RV_DECAY, 105), FXX(FXX_RV_SHIMMER, 90), FXX(FXX_RV_SIZE, 110),
    END};
static const pr_bar_t SER_SONG[] = {
    {0, MU_MA, 1}, {1, MU_MA, 1}, {2, 0, 1}, {3, 0, 1}, {0, 0, 2}, {1, 0, 1}, {2, 0, 1}, {3, 0, 1}, {0, 0, 0}};

/* ---- IMBRIUM: E phrygian, 96 BPM. Low drones under a sample & hold filter, a 303-like bass ostinato,
 * a half-time 808 and a high line with vibrato: the dark one. */
static const pr_pot_t IMB_POTS[] = {
    DB(DB_ROOT, 7),                                   /* E1 */
    NOTES(24, 25, 31, 36, 39, 32),                    /* E1 F1 B1 E2 G2 C2 */
    SWELL(88, 98, 119),                               /* a bar is 10 s */
    DV(0, DN_STACK, 1),
    DB(DB_CUTOFF, 60), DB(DB_RESO, 60), DB(DB_DRIVE, 50), DB(DB_SHRATE, 95), DB(DB_SHCUT, 45), DB(DB_LFOCUT, 30),
    MA(MEL_VIB, 40), MA(MEL_GLIDE, 70), MA(MEL_ATTACK, 60), MA(MEL_RELEASE, 95), MA(MEL_VOLUME, 75),
    MB(MEL_TUNE, 12), MB(MEL_CUTOFF, 50), MB(MEL_RESO, 95), MB(MEL_ENVMOD, 90), MB(MEL_ACCENT, 90),
    MB(MEL_ATTACK, 0), MB(MEL_DECAY, 50), MB(MEL_SUSTAIN, 0), MB(MEL_RELEASE, 20), MB(MEL_WAVE, 0),
    MB(MEL_SUB, 40), MB(MEL_GLIDE, 30), MB(MEL_VIB, 0), MB(MEL_VOLUME, 100),
    MIX(PART_303B, MX_REV, 25), MIX(PART_303B, MX_DLY, 20),
    END};
static const pr_bar_t IMB_SONG[] = {
    {0, MU_808 | MU_MA | MU_MB, 1}, {0, MU_808 | MU_MA, 1}, {1, MU_MA, 1}, {2, MU_MA, 1}, {3, MU_MA, 1},
    {0, 0, 1}, {1, 0, 1}, {2, 0, 1}, {3, 0, 1},
    {0, MU_808 | MU_MB, 1}, {0, MU_808 | MU_MA | MU_MB, 1}, {0, 0, 0}};
#define IMB_KIT {[D8_BD] = "X.........x.....", [D8_CP] = "........x.......", [D8_CH] = "x.x.x.x.x.x.x.x.", \
                 [D8_RS] = "...x......x..x.."}

/* ---- NUBIUM: C minor, 84 BPM. Arpeggios into the dotted ping-pong delay over held chords (Cm, Ab, Fm,
 * Bb), a bass in half notes, a light 808 with a clap: the sequencer one. */
static const pr_pot_t NUB_POTS[] = {
    DB(DB_ROOT, 3),                                   /* C1 */
    NOTES(36, 43, 51, 32, 34, 41),                    /* C2 G2 Eb3 Ab1 Bb1 F2 */
    SWELL(98, 105, 122),                              /* a bar is 11.4 s */
    MA(MEL_ATTACK, 0), MA(MEL_DECAY, 70), MA(MEL_SUSTAIN, 30), MA(MEL_RELEASE, 60), MA(MEL_CUTOFF, 80),
    MA(MEL_RESO, 50), MA(MEL_ENVMOD, 60), MA(MEL_WAVE, 64), MA(MEL_GLIDE, 0), MA(MEL_VIB, 0), MA(MEL_VOLUME, 85),
    MB(MEL_TUNE, 12), MB(MEL_CUTOFF, 45), MB(MEL_RESO, 30), MB(MEL_ENVMOD, 30), MB(MEL_ATTACK, 10),
    MB(MEL_DECAY, 80), MB(MEL_SUSTAIN, 80), MB(MEL_RELEASE, 60), MB(MEL_WAVE, 0), MB(MEL_SUB, 70),
    MB(MEL_GLIDE, 20), MB(MEL_VOLUME, 95),
    MIX(PART_303A, MX_DLY, 80), MIX(PART_303B, MX_REV, 30), MIX(PART_303B, MX_DLY, 0),
    END};
static const pr_bar_t NUB_SONG[] = {
    {0, MU_808 | MU_MB, 1}, {1, MU_808 | MU_MB, 1}, {2, MU_808, 1}, {3, MU_808, 1},
    {0, 0, 1}, {1, 0, 1}, {2, 0, 1}, {3, 0, 1},
    {0, MU_808 | MU_MA, 1}, {1, MU_808 | MU_MA, 1}, {2, MU_808 | MU_MA, 1}, {3, MU_808 | MU_MA, 1},
    {0, 0, 1}, {1, 0, 1}, {2, 0, 1}, {3, 0, 1}, {0, MU_808 | MU_MA | MU_MB, 1}, {0, 0, 0}};
#define NUB_KIT {[D8_BD] = "x.....x...x.....", [D8_CP] = "....x.......x...", [D8_CH] = "..x...x...x...xx"}

/* ---- CRISIUM: F# minor, 110 BPM. Minor-ninth drones, a fast sample & hold and drive, an eighth-note bass
 * pulse, a ring-modulated lead and a four-on-the-floor 808 with toms: the tense, driving one. */
static const pr_pot_t CRI_POTS[] = {
    DB(DB_ROOT, 9),                                   /* F#1 */
    NOTES(24, 36, 32, 39, 34, 31),                    /* F#1 F#2 D2 A2 E2 C#2 */
    SWELL(85, 90, 115),                               /* a bar is 8.7 s */
    DV(0, DN_STACK, 3),
    DB(DB_CUTOFF, 70), DB(DB_RESO, 80), DB(DB_DRIVE, 70), DB(DB_SHRATE, 110), DB(DB_SHCUT, 70), DB(DB_LFOPAN, 70),
    MA(MEL_RING, 50), MA(MEL_RINGHZ, 60), MA(MEL_VIB, 30), MA(MEL_GLIDE, 60), MA(MEL_CUTOFF, 90),
    MA(MEL_WAVE, 90), MA(MEL_VOLUME, 75),
    MB(MEL_TUNE, 12), MB(MEL_CUTOFF, 60), MB(MEL_RESO, 70), MB(MEL_ENVMOD, 70), MB(MEL_ACCENT, 80),
    MB(MEL_DECAY, 40), MB(MEL_SUSTAIN, 20), MB(MEL_RELEASE, 20), MB(MEL_WAVE, 20), MB(MEL_SUB, 50),
    MB(MEL_GLIDE, 10), MB(MEL_VOLUME, 95),
    MIX(PART_303B, MX_REV, 20), MIX(PART_303B, MX_DLY, 0), MIX(PART_808, MX_LEVEL, 85),
    END};
static const pr_bar_t CRI_SONG[] = {
    {0, MU_808 | MU_MA | MU_MB, 1}, {0, MU_808 | MU_MA, 1}, {1, MU_MA, 1}, {2, MU_MA, 1}, {3, MU_MA, 1},
    {0, 0, 1}, {1, 0, 1}, {2, 0, 1}, {3, 0, 1}, {0, MU_808 | MU_MB, 1},
    {0, 0, 1}, {1, 0, 1}, {2, 0, 1}, {3, 0, 1}, {0, MU_808 | MU_MA | MU_MB, 1}, {0, 0, 0}};
#define CRI_KIT {[D8_BD] = "X...x...X...x...", [D8_CP] = "....x.......x...", [D8_CH] = "..x...x...x...x.", \
                 [D8_LT] = ".......x......x.", [D8_MT] = "..........x....."}

/* ---- HUMORUM: G minor, 60 BPM. A bed to play over: G and D swelling on their own, DR3-DR6 (Bb, F, A, D)
 * left latching on their black keys, and the melodies an empty, slow pad for the white keys. */
static const pr_pot_t HUM_POTS[] = {
    DB(DB_ROOT, 10),                                  /* G1 */
    NOTES(24, 31, 39, 34, 38, 43),                    /* G1 D2 Bb2 F2 A2 D3 */
    SWELL1(0, 112, 120, 118), SWELL1(1, 112, 120, 118),   /* started every 8 s */
    DV(0, DN_STACK, 2), DV(5, DN_SPREAD, 40),
    DB(DB_CUTOFF, 55), DB(DB_RESO, 55), DB(DB_LFORATE, 15), DB(DB_LFOCUT, 50), DB(DB_WIND, 25), DB(DB_DRIFT, 50),
    MA(MEL_ATTACK, 90), MA(MEL_DECAY, 100), MA(MEL_SUSTAIN, 100), MA(MEL_RELEASE, 110), MA(MEL_GLIDE, 50),
    MA(MEL_VOLUME, 80),
    END};
static const pr_bar_t HUM_SONG[] = {{0, 0, 8}, {0, 0, 0}};

static const preset_t PRESETS[NPRESETS] = {
    {"TRANQUILITY", 720, TRQ_POTS, {
        {"1:0 5:0 6:0", TRQ_KIT, {"E5 =*11 D5 =*3 C5 =*15 -*8 A4 =*7 B4 =*7 C5 =*7",
                                  "A1 =*31 A1 =*23 E2 =*7"}},
        {"2:0 3:0 6:0", TRQ_KIT, {"A4 =*11 C5 =*3 F5 =*15 E5 =*7 -*8 C5 =*15",
                                  "F1 =*31 F1 =*23 C2 =*7"}},
        {"3:0 4:0 5:0", TRQ_KIT, {"G5 =*7 E5 =*7 D5 =*7 C5 =*7 -*16 E5 =*15",
                                  "C2 =*31 C2 =*23 G2 =*7"}},
        {"1:0 4:0 5:0", TRQ_KIT, {"D5 =*11 E5 =*3 A5 =*15 G5 =*7 E5 =*6 =~ -*16",
                                  "G1 =*31 G1 =*23 D2 =*7"}}}, TRQ_SONG},
    {"SERENITY", 600, SER_POTS, {
        {"1:0 2:0 4:0 1:32 2:32 4:32", {0}, {"A5 -*5 D6 -*9 F5 -*3 E5 -*11 C6 -*7 A5 -*23", 0}},
        {"1:0 3:0 5:0 1:32 3:32 5:32", {0}, {"D6 -*7 C6 -*3 A5 -*19 G5 -*5 F5 -*3 E5 -*21", 0}},
        {"6:0 3:0 1:0 6:32 3:32 1:32", {0}, {"G5 -*3 B5 -*3 D6 -*23 C6 -*7 A5 -*23", 0}},
        {"1:0 2:0 4:0 5:0 1:32 2:32 4:32 5:32", {0}, {"E6 -*5 D6 -*5 A5 -*13 F5 -*7 E5 -*3 D5 -*25", 0}}},
     SER_SONG},
    {"IMBRIUM", 960, IMB_POTS, {
        {"1:0 4:0", IMB_KIT, {"-*16 B4 =*15 -*8 G4 =*7 F4 =*15",
                              "E1! E1 E2~ E1 - E1 D2 E1 E1! - E2 E1~ F1 E1 - B1"}},
        {"1:0 4:0 5:0", IMB_KIT, {"E5 =*15 -*8 D5 =*7 B4 =*15 -*16",
                                  "E1! E1 E2~ E1 - E1 D2 E1 E1! - E2 E1~ F1 E1 - B1"}},
        {"6:0 5:0 4:0", IMB_KIT, {"G5 =*15 -*8 E5 =*7 C5 =*31",
                                  "C2! C2 C3~ C2 - C2 B1 C2 C2! - C3 C2~ D2 C2 - G1"}},
        {"2:0 3:0 4:0", IMB_KIT, {"F5 =*15 -*8 E5 =*7 B4 =*31",
                                  "F1! F1 F2~ F1 - F1 E2 F1 F1! - F2 F1~ E1 F1 - B1"}}}, IMB_SONG},
    {"NUBIUM", 840, NUB_POTS, {
        {"1:0 2:0 3:0", NUB_KIT, {"C4! G4 Eb5 G4 C5 G4 Eb5 D5 C4 G4 Eb5 G4 C5 G4 Bb4 G4",
                                  "C2 =*6 - C2 =*4 G1 = -"}},
        {"4:0 1:0 3:0", NUB_KIT, {"Ab3! Eb4 C5 Eb4 Ab4 Eb4 C5 Bb4 Ab3 Eb4 C5 Eb4 Ab4 Eb4 G4 Eb4",
                                  "Ab1 =*6 - Ab1 =*4 Eb2 = -"}},
        {"6:0 4:0 1:0", NUB_KIT, {"F3! C4 Ab4 C4 F4 C4 Ab4 G4 F3 C4 Ab4 C4 F4 C4 Eb4 C4",
                                  "F1 =*6 - F1 =*4 C2 = -"}},
        {"5:0 6:0 3:0", NUB_KIT, {"Bb3! F4 D5 F4 Bb4 F4 D5 C5 Bb3 F4 Eb5 F4 Bb4 F4 D5 F4",
                                  "Bb1 =*6 - Bb1 =*4 F1 = -"}}}, NUB_SONG},
    {"CRISIUM", 1100, CRI_POTS, {
        {"1:0 2:0", CRI_KIT, {"C#5 =*7 F#5 =*7 A5 =*15 G#5 =*7 E5 =*7 F#5 =*15",
                              "F#1! - F#1 - F#2 - F#1 - F#1! - F#1 - E2 - F#1 -"}},
        {"1:0 4:0", CRI_KIT, {"-*32 A5 =*7 C#6 =*7 B5 =*15",
                              "F#1! - F#1 - F#2 - F#1 - F#1! - F#1 - E2 - F#1 -"}},
        {"3:0 4:0 2:0", CRI_KIT, {"D5 =*7 F#5 =*7 A5 =*15 F#5 =*7 E5 =*7 D5 =*15",
                                  "D1! - D1 - D2 - D1 - D1! - D1 - C#2 - D1 -"}},
        {"5:0 6:0 4:0", CRI_KIT, {"E5 =*15 C#5 =*15 -*16 E5 =*7 F#5 =*7",
                                  "E1! - E1 - E2 - E1 - E1! - E1 - D2 - E1 -"}}}, CRI_SONG},
    {"HUMORUM", 600, HUM_POTS, {
        {"1:0 2:0 1:32 2:32", {0}, {0, 0}}, {"1:0 2:0 1:32 2:32", {0}, {0, 0}},
        {"1:0 2:0 1:32 2:32", {0}, {0, 0}}, {"1:0 2:0 1:32 2:32", {0}, {0, 0}}}, HUM_SONG},
};

int preset_count(void) { return NPRESETS; }
const char *preset_name(int k) { return k >= 0 && k < NPRESETS ? PRESETS[k].name : ""; }

/* "C#4", "Bb3", "A5": the MIDI note, -1 if it is not one */
static int pr_note(const char **s)
{
    static const int8_t PC[7] = {9, 11, 0, 2, 4, 5, 7};   /* A .. G */
    const char *p = *s;
    int n;
    if (*p < 'A' || *p > 'G')
        return -1;
    n = PC[*p++ - 'A'];
    if (*p == '#')
        n++, p++;
    else if (*p == 'b')
        n--, p++;
    if (*p < '0' || *p > '9')
        return -1;
    n += 12 * (*p++ - '0' + 1);
    *s = p;
    return n < 0 ? 0 : n > 127 ? 127 : n;
}

static int pr_int(const char **s)
{
    int n = 0;
    while (**s >= '0' && **s <= '9')
        n = n * 10 + *(*s)++ - '0';
    return n;
}

/* a melody line into a 303 part: its length is the number of steps written */
static void pr_line(bpart_t *b, const char *s)
{
    int n = 0, note = b->step[0].note;
    while (s && *s && n < NSTEPS) {
        int gate, flags = 0, rep = 1, k;
        while (*s == ' ')
            s++;
        if (!*s)
            break;
        if (*s == '-')
            gate = G_REST, s++;
        else if (*s == '=')
            gate = G_TIE, s++;
        else {
            int m = pr_note(&s);
            if (m < 0)
                break;                                /* (a typo in the table: the line stops there) */
            gate = G_NOTE;
            note = m;
        }
        while (*s && *s != ' ') {
            if (*s == '*') {
                s++;
                rep = pr_int(&s);
                continue;
            }
            flags |= *s == '!' ? BS_ACCENT : *s == '~' ? BS_SLIDE : 0;
            s++;
        }
        for (k = 0; k < rep && n < NSTEPS; k++, n++) {        /* the accent on the first, the slide on the last */
            b->step[n].note = (uint8_t)note;
            b->step[n].flags = (uint8_t)(gate | (k == 0 ? flags & BS_ACCENT : 0) |
                                         (k == rep - 1 ? flags & BS_SLIDE : 0));
        }
    }
    if (n)
        b->len = (uint8_t)n;
}

static void pr_pattern(pattern_t *pt, const pr_pat_t *src)
{
    const char *s = src->drone;
    int t, i;
    dpart_t *d = &pt->drum[0];
    d->len = NSTEPS;                                  /* the DRONE part: the bar, four of 4/4 */
    while (s && *s) {
        int v, st;
        while (*s == ' ')
            s++;
        v = pr_int(&s) - 1;
        if (*s++ != ':')
            break;
        st = pr_int(&s);
        if (v >= 0 && v < DRONE_VOICES && st < NSTEPS)
            sm_set(d->hit[v], st);
    }
    d = &pt->drum[1];
    for (t = 0; t < NDRUM; t++) {
        const char *r = src->kit[t];
        for (i = 0; r && r[i] && i < NSTEPS; i++) {
            if (r[i] == 'x' || r[i] == 'X')
                sm_set(d->hit[t], i);
            if (r[i] == 'X')
                sm_set(d->accent, i);
        }
        if (i)
            d->len = (uint8_t)i;
    }
    for (t = 0; t < NBASS; t++)
        pr_line(&pt->bass[t], src->mel[t]);
}

/* preset k over the project: its sound (the factory sound, then its own pots), its patterns (P1-P4; the
 * rest empty), its song and tempo. The settings otherwise stay as they are. */
void preset_load(int k)
{
    const preset_t *p = &PRESETS[k < 0 || k >= NPRESETS ? 0 : k];
    const pr_pot_t *q;
    int i, n;
    engine_sound_defaults(&proj.sound);
    for (q = p->pots; q->t != 0xFF; q++)
        proj.sound.v[q->t][q->v][q->i] = q->val;
    for (i = 0; i < NPAT; i++)
        pattern_init(&proj.pat[i], 0x3B0u + (uint32_t)i * 977u, 0x5A1u + (uint32_t)i * 613u);
    for (i = 0; i < PR_PATS; i++)
        pr_pattern(&proj.pat[i], &p->pat[i]);
    arrange_defaults();
    for (n = 0; p->song[n].rep && n < NSONG; n++) {
        song_bar_t *b = &proj.arr.song.bar[n];
        for (i = 0; i < NTRACKS; i++)
            b->pat[i] = p->song[n].pat;
        b->mute = (uint8_t)(p->song[n].mute | (1u << PART_BRK) | ((p->song[n].rep - 1u) << 5));
    }
    proj.arr.song.len = (uint16_t)n;
    proj.set.bpm_x10 = p->bpm_x10;
}
