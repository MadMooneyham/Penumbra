# Penumbra 1.2

A space-drone instrument for the M-VAVE FM-1, by Lunar Eclipse Forge. 1.2 opens the stereo field: every part
has a place in it and can sway across it, on the tide or at a rate of its own. Penumbra now plays well with
a DAW, and each journey has a sound of its own.

**Package:** `penumbra-1.2.fwsc`, identity `FM-1_73010200`. It carries the update loader: you can always
leave Penumbra again, for another firmware or M-VAVE's own. **Coming from 1.1 or 1.0:** your journey, its
loop, your user slots, your sounds and your settings come over, and sound as they did (nothing sways until
you ask it to).

Play it in the browser first: https://madmooneyham.github.io/Penumbra/emu/ · cheat sheet:
https://madmooneyham.github.io/Penumbra/cheatsheet/

## New

**PAN** (SHAPE, after TIDES)
- **PAN**: K1-K4 put DRONE, VOX, PULSE (with BLOOM) and TEXTURE anywhere from left to right. They glide
  there, so you can move them while you play. ALGO is RATE: how fast the parts sway, with the tide or on a
  steady cycle of their own, from 60 seconds down to half a second.
- **PAN 2**: how far each part sways, from still to all the way out (VOX's echoes follow it half as far).
  ALGO is OFFSET: every part in sync, or each one following the last, up to mirrored.
- The screen shows each part in its lane, moving as it sways.

**With a DAW** (USB MIDI; the TRS jack takes MIDI in too)
- **A tight clock.** Each tick is timed as it arrives, so Penumbra holds the DAW's tempo to about a tenth of
  a BPM, and the delay's echoes stay put.
- **Song position.** When the DAW locates or loops, the clock and a playing loop carry on from there.
- **CCs both ways.** 44 controls: HOME's knobs, the throws, TIDES, DRONE 2, the levels, PAN and the whole FX
  page. Automate them from the DAW, or record your moves: changing one on the FM-1 sends its CC.
- **Notes both ways.** The white keys go out as VOX's notes. The black keys go out on the next channel, one
  note each, and those notes coming in press the keys, so the DAW can play the drone, the kick, BLOOM and
  VOX's sounds back as you did.
- **Program changes both ways**: the journey's scenes, the presets, the user journeys.
- SYSTEM: MIDI CHANNEL, MIDI NOTES OUT, MIDI CC OUT. The CC table is in the manual.

**Journeys**
- Each preset journey has a sound of its own now, as a place to start from: its drone, its tide, its stereo
  field and how it sways, the drone voices that come and go, and the VOX sound it starts on. ECLIPSE keeps
  Penumbra's sound as made.

## Fixed

- **Turning the tempo made a noise.** The delay's echo time follows the tempo, and each detent of ALGO bent
  every echo in it by a few semitones. A new tempo (or delay time) is now taken once you stop turning, the
  echoes crossfading to it, on pitch. WARP still bends them, as it should.
- Following an external clock, the echoes no longer warble with its small wanderings.

GPL-3.0-only. Built on Felucca (Leo Kuroshita, Hügelton Instruments) and X0X (Charles Vestal); see
LICENSING.md.
