# Penumbra 1.13

An ambient space-drone synth for the M-VAVE FM-1, by Lunar Eclipse Forge. 1.13 puts the drone's voices on the
bar: when each one comes in, how it plays, how long it holds. And the knobs answer at once.

**Package:** `penumbra-1.13.fwsc`, identity `FM-1_73011300`. It carries the update loader, so leaving Penumbra
again, for another firmware or M-VAVE's own, is always possible. **Coming from 1.12 or earlier:** the journey
playing, its A/B, the loop, the user slots, the sounds and the settings come over as they were; every drone
voice plays as before.

## The drone voices, on the bar

**ALGO with a drone key held** now sets how that voice plays: HELD, AT TIMES (swelling in now and then, as
SWELL did), or EVERY 2, 4, 8, 16 or 32 bars, exactly on the bar line from when the clock starts. ENV, with the
key still held, opens DRONE VOICES on it.

**DRONE VOICES** (SHAPE's last page) shows all five voices:

| Control | What it does |
| --- | --- |
| K1 VOICE | The voice the other knobs set |
| K2 PLAYS | HELD, AT TIMES, EVERY 2 to 32 |
| K3 ENTERS | AT ONCE, or 1 to 32 bars after the clock starts: a held voice comes in on that bar, a swelling one starts there |
| K4 HOLDS | A swell: AS MADE, or up 1 to 32 bars from its start, falling on a bar line. A held voice: STAYS, or lets go that many bars after it came in |
| ALGO | The drone's level |

So a scene builds and breaks down in time with a band: voice 1 at once, voice 2 after 2 bars, voice 3 in after 4
and out 4 later, voice 4 swelling every 8 bars and holding 4. The bars follow the tempo, or the MIDI clock
coming in.

## The knobs

- An amount (a level, a filter) answers on the first click, even after a rest; only a detent's instant spring
  back is passed over. The quick-spin speed-up is as it was.
- A choice (a hold's ALGO, pages, rows, stepped settings) settles where it is left: a detent's spring back and
  its bounce are both passed over, and a steady turn back follows the hand.

## Good to know

- Journeys are format 13, and larger: room for the drone voices' timing and for what comes next. 1.12's and
  older journeys, user slots, backups and `.penumbra-journey` files come over widened, with nothing lost,
  1.11's own STAYS and skipped scenes included.
- The Journeys page now needs Penumbra 1.13 on the FM-1 (the Install page updates it); it still opens older
  journey files.

GPL-3.0-only. Built on Felucca (Leo Kuroshita, Hügelton Instruments) and X0X (Charles Vestal); see
LICENSING.md.
