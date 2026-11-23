# Penumbra 1.11

An ambient space-drone synth for the M-VAVE FM-1, by Lunar Eclipse Forge. 1.11 is about journeys of one's
own: which scenes play, for how long, in what order, gathered from any journey.

**Package:** `penumbra-1.11.fwsc`, identity `FM-1_73011100`. It carries the update loader, so leaving Penumbra
again, for another firmware or M-VAVE's own, is always possible. **Coming from 1.10 or earlier:** the journey
playing, its loop, the user slots, the sounds and the settings come over as they were, every scene playing as
before.

## A journey of one's own

SCENE (EDIT) has new fields and a new move:

- **STAYS**: how long the scene plays when the journey travels by itself: AS TRAVELS, or 4, 8, 16, 32, 64 or
  128 BARS of its own. A scene with its own STAYS moves on even with TRAVELS by hand.
- **PLAYS**: YES, or SKIPPED. A skipped scene stays in the journey, but travelling, PRESETS and MORPH pass it
  by. SCENE still opens it, and turning it back to YES brings it back.
- **BRING IN**: a scene of any journey, a preset's or a user slot's. K2 picks the journey, K3 its scene, and
  SEL twice brings it in over the scene shown.
- **SEL held, K1 turned** moves the scene shown earlier or later; the scene playing plays on.

Up to eight scenes from anywhere, in any order, each for its own time or skipped: SAVE on a JOURNEY user slot
keeps the journey, and a backup file carries it to another FM-1.

## Good to know

- SCENE's fields after SOUND A-B are two further on (STAYS and PLAYS come next; BRING IN is last).
- MIDI's program changes and SCENE CC still go to the scene they name, skipped or not.
- Journeys are format 12; 1.10 and earlier come over upgraded.

GPL-3.0-only. Built on Felucca (Leo Kuroshita, Hügelton Instruments) and X0X (Charles Vestal); see
LICENSING.md.
