# Penumbra 1.6

A space-drone instrument for the M-VAVE FM-1, by Lunar Eclipse Forge. 1.6 is about the loop and the voices
that play themselves: layers with their own place, tone and length, several sounds singing at once, BLOOM
swelling by itself, and twice the voices for all of it.

**Package:** `penumbra-1.6.fwsc`, identity `FM-1_73010600`. It carries the update loader, so leaving Penumbra
again, for another firmware or M-VAVE's own, is always possible. **Coming from 1.5 or earlier:** the journey
playing, its loop, the user slots, the sounds and the settings come over as they were. A 1.5 journey's
singing sound keeps singing; its loop's layers keep their level, sound, octave and mute.

## The loop's layers

LOOP (SEL on HOME) has more for each layer:

- **A second page** (ENV there): **PAN**, the layer's place in the field; **DRIFT**, how far it sways round
  that place on the tide, each layer a quarter tide behind the one before, so drifting layers circle the
  field; **TONE**, its filter, darker or brighter; **SWELL**, its envelope, short and plucked or slow in
  with a long tail.
- **A length of its own** (ALGO there): as the loop, or 1 to 64 beats. Shorter, its end is cut; longer, a
  rest follows it. Either way it drifts against the other layers and meets them differently each time round,
  like tape loops of unequal length.
- **Beads on HOME.** Each layer is a bead on the satellite's orbit at its place in the field, moving as it
  drifts, in its sound's colour (GLASS pale, REED amber, HALO blue, LOW red), lit while it sounds.

## Voices that play themselves

- **Several sounds sing at once.** Each of VOX's four sounds has its own singing setting (its key held, and
  ALGO), each with its own phrases.
- **VOX SINGS**, a new SHAPE page after VOX 3: for the sound playing, how often it SINGS, its PLACE in the
  field, how far it DRIFTs round it, and its OCTAVE. TOTALITY's HALO now wanders as it sings.
- **BLOOM by itself.** With BLOOM's key held, ALGO sets it swelling in now and then: RARELY, NOW AND THEN or
  OFTEN. TOTALITY blooms rarely.
- **Eight VOX voices** instead of four, shared by the hands, the loop and the sounds that sing. BENCH's load
  test holds all eight.

## Fixed

- **A key let go no longer ends the loop's same note.** When the hands and the loop played the same note in
  the same sound they shared a voice, so letting go of the key cut off the looped note. Each note now belongs
  to the hands, a singing sound or a loop layer, and only its owner ends it.
- **No octave jump with DETUNE at 0.** On some VOX voices the two oscillators started half a cycle apart,
  and with no DETUNE they cancelled the note's fundamental, so it sounded an octave high. They now start
  close together.

## Good to know

- SHAPE's pages after VOX 3 are one further on (VOX SINGS is the sixth).
- Singing sounds and BLOOM by itself are not recorded by the looper, and play only while the clock runs.

GPL-3.0-only. Built on Felucca (Leo Kuroshita, Hügelton Instruments) and X0X (Charles Vestal); see
LICENSING.md.
