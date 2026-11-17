# Penumbra 1.8

An ambient space-drone synth for the M-VAVE FM-1, by Lunar Eclipse Forge. 1.8 is about the kick: shaped
by hand, kept in the key, given weight, and able to play by itself.

**Package:** `penumbra-1.8.fwsc`, identity `FM-1_73010800`. It carries the update loader, so leaving Penumbra
again, for another firmware or M-VAVE's own, is always possible. **Coming from 1.7 or earlier:** the journey
playing, its loop, the user slots, the sounds and the settings come over as they were, and backups from 1.7
restore as they are; the kick sounds as it did until it is changed.

## The kick

**Its key held** (black 6) puts the kick under the knobs, as the other black keys' holds do:

| K1 | K2 | K3 | K4 | ALGO |
| --- | --- | --- | --- | --- |
| DROP | DECAY | CLICK | TUNE | the kick by itself |

- **TUNE** keeps it in the scene's key: on the root, the fourth, the fifth, or an octave up, following each
  key change.
- **By itself**, on the clock: A HEARTBEAT (a soft double beat each bar), EVERY BAR, EVERY HALF BAR or EVERY
  2 BARS. It plays only while the clock runs, is not recorded by the looper, and is saved with the journey.

**PULSE 2**, a new SHAPE page after PULSE: TUNE, DRIVE, DUCK and BY ITSELF, ALGO the level.

- **DRIVE** takes the kick from a soft round thud to a heavier, grittier hit that cuts through a thick drone.
- **DUCK:** each kick dips the drone, VOX and the wind for a moment, and they swell back over about a third
  of a second. A little gives the drone a slow breath; a lot, a deep throb. The kick and BLOOM stay whole.

**LANDS**, a new field on SCENE (the whole journey's, like TRAVELS): A KICK lands as each new scene's key
comes in, by PRESET, MORPH or travelling by itself.

**From MIDI**, on the channel after VOX's, the kick (note 41) and BLOOM (note 42) hit as hard as they are
played.

## Fixed

- **PAN 2 kept on upgrade.** Upgrading a journey from 1.2-1.7 could run a conversion meant only for 1.2's
  betas and change its PAN 2 settings. Now only those betas' journeys are converted.

## Good to know

- SHAPE's pages after PULSE are one further on (PULSE 2 comes between PULSE and TEXTURE).
- Journeys are format 11; 1.7 and earlier come over upgraded.

GPL-3.0-only. Built on Felucca (Leo Kuroshita, Hügelton Instruments) and X0X (Charles Vestal); see
LICENSING.md.
