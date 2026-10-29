# Penumbra

A space-drone instrument for the M-VAVE FM-1, by Lunar Eclipse Forge. Switch it on and the drone swells in;
you play over it, loop it, shape it and move it from place to place. The sound it aims for is the ELTA Solar
42's: thick, slowly beating drones, an oscillator voice far back in the shimmer, deep soft pulses, a long wash
of delay. The space between the sounds is part of the sound.

Penumbra is a rewrite of its first, X0X-based version (kept at the tag `penumbra-0.6-shelved`). The design, its
phases and their gates are in the document "Penumbra: rewrite design". This tree has:

- the platform carried over from X0X and Felucca: boot and crash guards, flash storage, the update path and web
  installer, USB MIDI and audio, MIDI on the TRS jack, the second core, panel calibration, the screen and fonts,
  the screen reader;
- four voices: DRONE (five voices, SWARM), VOX (a four-voice oscillator synth: two oscillators from saw to a
  narrow pulse, a filter from low-pass to band-pass, glide), PULSE (a deep kick tuned to the key, a soft tick)
  and TEXTURE (a bed of space wind), into the shimmer reverb, the tape delay and the master stage;
- journeys: banks of scenes, each a key and scale, the drone's root and voicing, VOX's brightness and register,
  the space and the wind; with the sound (SHAPE's, the drone voices' own, VOX's four sounds). Six presets, eight
  user slots;
- the drone is the backbone: a pedal held through the whole scene. A new scene moves only the notes it must,
  one voice at a time, a few seconds apart, sliding slowly; each preset keeps its drone's root from scene to
  scene. A drone voice can come and go instead (SWELL);
- the looper: VOX's notes, on the clock, in layers;
- storage: the journey playing carries on after a power cut;
- MIDI clock in and out (24 ppqn, START / STOP), followed or sent as SYSTEM sets it;
- the benchmark: BENCH shows each stage of the render, average and worst block of the last second, as a share
  of the real-time budget, per core; SEL runs the load test (the voices at their heaviest).

Nothing plays by itself but the drone you latch (and a SWELL voice now and then).

## Playing it

The clock starts at power-on and the scene's drones swell in (SYSTEM > PLAY AT POWER-ON).

| Control | What it does |
| --- | --- |
| White keys | VOX, in the scene's key and scale |
| Black keys 1-5 | the drone voices: latched when let go; a SWELL voice: played when pressed |
| Black keys 6, 7 | PULSE's kick and tick |
| Black keys 8-11 | VOX's four sounds: GLASS, REED, HALO, LOW (mid-phrase too) |
| KNOBS 1-4 on HOME | DRONE (its filter and body), VOX (its brightness), SPACE (reverb and delay), TONE (everything brighter or darker) |
| SELECT on HOME | MORPH: by hand toward the next scene (right) or the one before (left); past the middle its key, all the way there |
| PRESET | the next scene, or the one before: it glides there (12 s) |
| ALGO | the tempo (on SHAPE: the voice's level) |
| SEQ | the clock: start and stop, with a fade (MIDI clock out, if on) |
| REC | the looper: record (the clock starts if it is not); again: it plays (its length the nearest beat); again: a layer over it; again: kept |
| PLAY | the loop: stop, or play from its start; held: undo the last layer |
| REC with PLAY | clear the loop |
| ARP | FREEZE: the notes held keep sounding when let go; again: they ring out (and what is held now freezes) |
| ENV | SHAPE (ENV again: the next page): DRONE filter, body, swarm, wave; VOX bright, attack, release, shimmer; VOX 2 wave, filter, reso, glide; VOX 3 detune, B octave, B mix, envelope; PULSE drop, decay, tick, space; TEXTURE colour, sweep, rate, width. VOX's pages shape the sound playing |
| EDIT | SCENE: the scenes' key, scale, drone root, voicing and the rest; SEL stores the moment as a new scene |
| FX | the reverb and the delay |
| LFO | JOURNEY: SEL opens a preset or a user journey; SAVE stores the one playing in a user slot |
| GLO | SYSTEM (the settings), then BENCH |
| HOME | back to the corona |
| SAVE | save now (it saves by itself once the sound is quiet, so after STOP); in a hold: keep what the knobs did |
| OCT- / OCT+ | the white keys' octave |
| Any button held | says what it does, and does nothing else (PLAY held: the undo) |
| OCT- + OCT+ held 5 s | the chip's update mode |

Pages go back to HOME after 30 s untouched (not BENCH).

Holds: while one is held, the four knobs play something else, on any page.

| Hold | The knobs | Let go |
| --- | --- | --- |
| A drone key (black 1-5) | that voice's level, tone, detune, octave; ALGO: HELD (left) or SWELL (right) | it stays as shaped (and the key does not latch) |
| A VOX sound key (black 8-11) | that sound's brightness, wave, filter, release | it stays as shaped |
| FX | throws: shimmer, delay feedback, freeze (the reverb holds), dip (the mix's low-pass) | they drift back over a couple of seconds; SAVE while holding keeps them, until FX is held and turned again |
| A white key | bend VOX: brightness, vibrato, shimmer, release | they drift back; SAVE while holding puts them into VOX's sound |

A drone voice is HELD (its key latches it; the journey holds it on the pedal) or SWELL: it comes and goes,
swelled in every few bars while the clock runs, and its key plays it, a swell in and out each press. Each
voice is set apart; the choice is saved with the journey.

The loop is VOX's notes, played back through VOX as they were played (up to 16 bars, 8 layers); it is not saved.
Opening a preset keeps the sound you have; a user journey brings its own.

Building and testing: BUILDING.md. Licences and credits: LICENSING.md.
