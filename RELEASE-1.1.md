# Penumbra 1.1

A space-drone instrument for the M-VAVE FM-1, by Lunar Eclipse Forge. 1.1 makes it move: the drone and the
space breathe on their own, each scene has a room of its own, the effects have more to play, and the
sound itself can be kept, blended and brought back.

**Package:** `penumbra-1.1.fwsc`, identity `FM-1_73010100`. It carries the update loader: you can always
leave Penumbra again, for another firmware or M-VAVE's own. **Coming from 1.0:** your journey, its loop,
your user slots and your settings come over (the new settings start as made).

Play it in the browser first: https://madmooneyham.github.io/Penumbra/emu/ · cheat sheet:
https://madmooneyham.github.io/Penumbra/cheatsheet/

## New

**Movement**
- **TIDES** (the last SHAPE page): one slow swell under everything, 8 seconds to 4 minutes a tide. It opens
  and closes the drone's filter, breathes the space in and out and brightens VOX, each as far as you set.
  ALGO sets its shape, from an even sine to a wandering, uneven tide. As made it moves gently, about once
  every 50 seconds.
- **DRONE 2** (SHAPE): the drone's own movement on the panel at last: RATE, SWEEP, WIDTH and DRIFT.
- **ROOM and HALO** (SCENE): each scene's own reverb size and decay, and its shimmer. They glide with
  PRESET and MORPH, so a journey travels from a small room to a vast hall. Every preset has its own spaces.

**Effects**
- **FX has four rows.** REVERB 2: MOTION, LOW CUT, INTERVAL (the shimmer an octave, a fifth or a twelfth
  up), LEVEL. DELAY 2: LEVEL, LOW CUT, PING-PONG, and WARP's range.
- **DIP goes both ways** (FX held, KNOB 4): left closes a low-pass, right opens a high-pass.
- **WARP** (FX held, turn ALGO): the tape delay's time bends, the echoes diving or rising, and springs back.
- A change of the reverb's size now glides: the room breathes, it never clicks.

**The sound**
- **A/B** (hold ENV): OCT- keeps the sound as A, OCT+ as B, SELECT walks between them, every value
  gliding. Saved with the journey playing.
- **SOUNDS** (LFO, then LFO again): a library of eight sounds apart from the journeys. SAVE stores the
  sound playing, SEL brings one into any journey, gliding in over two seconds.

**Playing it**
- ENV opens the page of what a hold last shaped: hold a drone key and turn, let go, ENV, and you are on
  DRONE.
- Every value reads as a percentage (an offset from as made as -/+%), times in seconds.
- SHAPE has tabs across its top; SEL twice resets each page, the new ones too.
- The screen reader says the scene line as "scene 1 of 4", and its words are published with the site.

## Changed under the hood

- About 2,000 lines X0X and Felucca left behind and Penumbra never used are gone (a compressor, a kick
  pump, a distortion, a plain delay, two fonts, a site builder, debug tools): the sound was checked sample
  for sample against 1.0 at every step.
- The web installer only calls a beta a beta.

GPL-3.0-only. Built on Felucca (Leo Kuroshita, Hügelton Instruments) and X0X (Charles Vestal); see
LICENSING.md.
