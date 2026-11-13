# Penumbra 1.4

A space-drone instrument for the M-VAVE FM-1, by Lunar Eclipse Forge. In 1.4 the first journey starts sparse,
and the guides describe what Penumbra does in its first few minutes.

**Package:** `penumbra-1.4.fwsc`, identity `FM-1_73010400`. It carries the update loader, so leaving Penumbra
again, for another firmware or M-VAVE's own, is always possible. **Coming from 1.3, 1.2.1, 1.2, 1.1 or 1.0:**
the journey playing, its loop, the user slots, the sounds and the settings come over as they were, so a sound
already saved keeps its drone voices as they were set.

## A sparser start

- **ECLIPSE's drone voices all come and go.** Instead of five voices held from the start, they drift in and
  out on their own every few bars, with space around them, and black keys 1-5 each call one in to swell and
  fade. The lowest voice swells in as soon as the clock starts, so there's sound from the first moment.
- **The other journeys are as they were,** most of their voices held.
- **The sound as made** (RESET SOUND) has all five voices coming and going too, as ECLIPSE does.
- Holding a drone key and turning ALGO still switches that voice between HELD and SWELL.

## The first few minutes

The README, the manual, the browser version's page and the cheat sheet now open with what Penumbra does in its
first few minutes: the drone breathing on its own, the black keys calling voices in, the white keys always in
the key, a loop, and travelling to the next scene with the loop following into the new key. A list of what
else it can do follows: throws, a frozen chord, bends, MORPH, held voices, other journeys, A/B and SLEEP.

## Fixed

- At power-on, the clock's first beat came before the drone voices were set to come and go, so a swell the
  journey asked for then was missed. The journey's setting now counts from the first beat.

GPL-3.0-only. Built on Felucca (Leo Kuroshita, Hügelton Instruments) and X0X (Charles Vestal); see
LICENSING.md.
