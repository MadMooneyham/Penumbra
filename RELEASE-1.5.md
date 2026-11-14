# Penumbra 1.5

A space-drone instrument for the M-VAVE FM-1, by Lunar Eclipse Forge. It plays itself until you do: 1.5 adds a
journey that travels on its own, a voice that sings, loops built from several sounds with a page for their
layers, and journeys that each sound like themselves.

**Package:** `penumbra-1.5.fwsc`, identity `FM-1_73010500`. It carries the update loader, so leaving Penumbra
again, for another firmware or M-VAVE's own, is always possible. **Coming from 1.4 or earlier:** the journey
playing, its loop, the user slots, the sounds and the settings come over as they were. TOTALITY is in the
JOURNEY list (LFO); a new unit starts on it.

## It plays itself

- **TOTALITY**, a seventh journey: an eclipse in six scenes on a pedal D, from first contact through the
  diamond ring and totality to the afterglow. It moves to its next scene every 16 bars, its sound opens up
  toward the middle and closes again, every part sways in turn round the field, and HALO sings a phrase now
  and then. Every key and knob works while it does.
- **Any journey can travel by itself.** SCENE (EDIT) has TRAVELS: BY HAND, or on to the next scene every 8, 16,
  32 or 64 bars. PRESET and MORPH still move it by hand.
- **The sound can travel with the scenes.** With A and B kept, SCENE's SOUND A-B gives each scene a place
  between them, and the sound glides there as the scene arrives. Neither of these writes to the FM-1's memory
  as it moves, so a journey can run all night.
- **VOX sings.** With one of VOX's sound keys (black 8-11) held, ALGO turned right sets that sound singing by
  itself: a few soft, slow notes in the scene's key, RARELY, NOW AND THEN or OFTEN. It keeps singing with that
  sound while the white keys play another.

## Journeys that sound like themselves

- **A journey opened brings its own sound,** gliding in over two seconds: its drone, its tide, its movement,
  the voices that come and go. Until now the six presets brought their scenes and kept the sound playing, so
  FAR SIDE's vast four-minute tide and PERIGEE's quick one were never heard.
- **SOUND: LOCKED.** Any knob turned right on JOURNEY locks the sound playing, so journeys opened bring their
  scenes alone. It is saved with the settings.

## Loops

- **Each note keeps its sound.** A looped note plays with the VOX sound it was played with, whichever sound is
  in hand later: a layer of GLASS, one of REED and one of HALO play together. The loop still follows the key
  from scene to scene.
- **LOOP, the layers' page.** SEL on HOME opens it: a row for each layer, and its own LEVEL, SOUND (as played,
  or one of the four for the whole layer), OCTAVE and MUTE. SEL twice takes a layer out, the ones over it
  moving down. The layers sound apart, so the same phrase recorded twice, one an octave up in another sound,
  is two parts.
- Loops saved by 1.4 load as they were; their notes play with the sound in hand, as they did.

## Fixed

- **SHAPE no longer jumps to its first page.** A key played while a knob turned on a SHAPE page took the knob
  for that key's hold and sent SHAPE to that part's page (a drone key: DRONE, the first). On SHAPE the knobs
  now stay the page's, so keys can be played while a sound is shaped. The holds are as they were on every
  other page.

## Good to know

- VOX has four voices, shared by the hands, the loop and a sound that sings. The shimmer and VOX's place in
  the field are shared by all its notes.
- With SOUND not locked, opening a journey replaces the sound playing. A sound worth keeping fits in SOUNDS
  or in A first.
- MIDI program change 14 opens TOTALITY (8-13 the other presets, as before).

GPL-3.0-only. Built on Felucca (Leo Kuroshita, Hügelton Instruments) and X0X (Charles Vestal); see
LICENSING.md.
