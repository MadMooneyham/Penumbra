# Penumbra 1.7

An ambient space-drone synth for the M-VAVE FM-1, by Lunar Eclipse Forge. 1.7 is about shaping: HOME's
knobs now play six pages of the sound itself, every page has an eclipse rising behind it, installs show how
far they have got, and everything Penumbra keeps can be backed up to a file.

**Package:** `penumbra-1.7.fwsc`, identity `FM-1_73010700`. It carries the update loader, so leaving Penumbra
again, for another firmware or M-VAVE's own, is always possible. **Coming from 1.6 or earlier:** the journey
playing, its loop, the user slots, the sounds and the settings come over as they were.

## Shaping on HOME

**HOME's four knobs play six pages, and ALGO on HOME turns between them,** round and round. The page stays
chosen across a power-off.

| Page | K1 | K2 | K3 | K4 |
| --- | --- | --- | --- | --- |
| DRONE | FILTER | BODY | SWARM | WAVE |
| VOX | BRIGHT | FILTER | RESO | RELEASE |
| MOTION | SWEEP | RATE | DRIFT | TIDES |
| SPACE | REVERB | DECAY | SHIMMER | DELAY |
| MIX | DRONE | VOX | PULSE | TEXTURE |
| ALIVE | SINGS | BLOOMS | SWELLS | TRAVELS |

- **DRONE, VOX and MOTION** are SHAPE's own settings, brought where they can be played. VOX shapes the VOX
  sound playing; black keys 8-11 pick which.
- **SPACE** is the FX page's reverb and delay returns, its decay and shimmer.
- **ALIVE** sets how much plays by itself, a step a click: how often the sound playing sings, BLOOM by
  itself, how many drone voices come and go, and how often the journey travels on.
- **The knobs are arcs.** On HOME, FX and SHAPE each knob is a slice of the corona filled to its value, in
  the page's colour.
- **The knobs' speed.** Each click is one step, and a slow click from rest only wakes a knob, so a detent
  that snaps back or on by itself changes nothing. A knob that keeps spinning fast goes further: two steps a
  click after four quick clicks in a row, three after ten.

## The horizon

Behind every page but HOME an eclipsed moon rises from the bottom-right corner under a sky of stars, its
corona in the page's own colour: FX teal, JOURNEY violet, SCENE blue, SOUNDS pink, LOOP green, SHAPE amber.
The corona breathes with the tide and a few stars twinkle, slowly. The row chosen on a page is tinted in the
same colour.

## Installs show how far they have got

Penumbra shows each step while it reads a package and readies its installer, and the installer in the
package then shows its own screen while it writes, with a bar and a percentage, and says when it is done.
The installer's screen appears on every install of 1.7 or later, from any firmware; the first screen from
1.7 on. (Installing 1.7 from 1.6, the first part still looks as 1.6 drew it.)

## Backups

The **[Backup page](https://madmooneyham.github.io/Penumbra/backup/)** (Chrome or Edge, the FM-1 connected
by USB) copies everything Penumbra keeps to one readable file: the eight user journeys, the sound library,
the journey playing with its A and B, the loop and its layers, and the settings. The same file puts any of
them back, part by part, onto this FM-1 or another, and Penumbra starts again with them. Each part is checked
before it is saved: an older backup comes over upgraded, a part from a newer Penumbra is not taken. The
panel's calibration stays the unit's own. The emulator saves and restores the same file.

## USB ONLY

SYSTEM's **USB AUDIO** has a third choice, **USB ONLY**: full-level USB audio, and while a computer takes it
the FM-1's own outputs (the speaker, the headphones and the line out) fade to silence. Unplugged, or with no
computer taking the audio, the FM-1 plays as FULL.

## Also

- **TEXTURE's level as made is now 55%**, a little further back (journeys already saved keep their own).
- **The manual, README, cheat sheet and emulator page lead with shaping:** HOME's pages and the gestures
  first, the keys after.

## Good to know

- **The tempo** is ALGO on every page but HOME (and SHAPE and LOOP, as before).
- **The macros** (DRONE, VOX, SPACE, TONE) are no longer on HOME. They are still on MIDI CCs 14-17, and a
  journey's macros come over and sound as they did.

GPL-3.0-only. Built on Felucca (Leo Kuroshita, Hügelton Instruments) and X0X (Charles Vestal); see
LICENSING.md.
