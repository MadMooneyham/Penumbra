# Penumbra

A generative space-drone instrument for the M-VAVE FM-1, by Lunar Eclipse Forge: switch it on and it plays a
journey across space and time, and you steer it. The sound it aims for is the ELTA Solar 42's: thick, slowly
beating drones, oscillator lines that wander in key, deep soft pulses, a long wash of delay and shimmer.

Penumbra is a rewrite of its first, X0X-based version (kept at the tag `penumbra-0.6-shelved`). The design, its
phases and their gates are in the document "Penumbra: rewrite design". This tree has phases 1 to 5:

- the platform carried over from X0X and Felucca: boot and crash guards, flash storage, the update path and web
  installer, USB MIDI and audio, MIDI on the TRS jack, the second core, panel calibration, the screen and fonts,
  the screen reader;
- four voices: DRONE (five voices, SWARM), VOX (a four-voice oscillator synth, far back in the shimmer), PULSE
  (a deep kick tuned to the key, a soft tick) and TEXTURE (a bed of space wind), into the shimmer reverb, the
  tape delay and the master stage. The space between them is part of the sound;
- journeys: six presets, each a few scenes (key, scale, chords, the drone's root and voicing, density, space,
  wind, pulse). A director moves through the moon's phases (new, waxing, full, waning), travels to the next
  scene at each new moon and changes the chords every few bars; a generator writes VOX's phrases (one to three
  long notes over eight bars, then the silence they ring out into) and PULSE's pattern from the journey's
  seed, repeats them and changes them a little;
- the drone is the backbone: a pedal held through the whole scene. The chords move only VOX. A new scene moves
  only the drone notes it must, one voice at a time, a few seconds apart, sliding slowly; each preset keeps its
  drone's root from scene to scene;
- the sound is yours to shape: SHAPE has four knobs a voice, saved with the journey, and holds play it live
  (below);
- loops: what the generator plays can be captured and edited as 16 steps, VOX recorded from the keys;
- storage: the journey playing carries on after a power cut; 8 user journeys;
- MIDI clock in and out (24 ppqn, START / STOP), followed or sent as SYSTEM sets it;
- the benchmark: BENCH shows each stage of the render, average and worst block of the last second, as a share
  of the real-time budget, per core; SEL runs the load test (the voices at their heaviest).

## Playing it

It plays at power-on (SYSTEM > PLAY AT POWER-ON). The modes (ARP) say what the white keys do:

| Mode | The generator plays | The white keys |
| --- | --- | --- |
| PLAY | everything; its VOX waits while you play (and four bars after) | play VOX in the scene's key and scale |
| GUIDE | everything | held: the notes VOX favours; tapped: where the next phrase heads; low keys pull the register down, high keys up |
| DRIFT | everything | each key is a scene: the journey travels there |

| Control | What it does |
| --- | --- |
| Black keys 1-5 | latch the drone voices, when let go (the journey sets their notes) |
| Black keys 6, 7 | PULSE's kick and tick, by hand |
| KNOBS 1-4 on HOME | DRIFT (how fast the journey moves; 0 holds it still), DENSITY (how much plays), SPACE (reverb and delay together), TONE (brightness) |
| ALGO | the tempo (on SHAPE: the voice's level) |
| PRESET | the journeys (opens JOURNEY) |
| PLAY | start and stop, with a fade (MIDI clock out, if on) |
| REC | loop what plays: GUIDE and DRIFT capture VOX's phrase and PULSE; PLAY records a bar of VOX from the keys, from the next bar line. With loops on: let them all go |
| ARP | the mode: PLAY, GUIDE, DRIFT |
| ENV | SHAPE: a voice's sound (ENV again: the next voice). DRONE filter, body, swarm, wave; VOX bright, attack, release, shimmer; PULSE drop, decay, tick, space; TEXTURE colour, sweep, rate, width |
| EDIT | SCENE: the scenes' key, scale, chords, drone root and the rest; SEL stores the moment as a new scene |
| SEQ | LOOP: a loop's steps, VOX or PULSE (KNOB 1 the step, 2 its note, 3 on / tie, 4 MUTATE); SEL captures or lets go |
| FX | the reverb and the delay |
| LFO | JOURNEY: SEL opens a preset or a user journey; SAVE stores the one playing in a user slot |
| GLO | SYSTEM (the settings), then BENCH |
| HOME | back to the corona |
| SAVE | save now (it saves by itself once the sound is quiet, so after STOP); in a hold: keep what the knobs did |
| OCT- / OCT+ | the white keys' octave |
| Any button held | says what it does, and does nothing else |
| OCT- + OCT+ held 5 s | the chip's update mode |

Pages go back to HOME after 30 s untouched (not BENCH).

Holds: while one is held, the four knobs play something else, on any page.

| Hold | The knobs | Let go |
| --- | --- | --- |
| A drone key (black 1-5) | that voice's level, tone, detune, octave | it stays as shaped (and the key does not latch) |
| FX | throws: shimmer, delay feedback, freeze (the reverb holds), dip (the mix's low-pass) | they drift back over a couple of seconds; SAVE while holding keeps them, until FX is held and turned again |
| A white key (PLAY, GUIDE) | bend VOX: brightness, vibrato, shimmer, release | they drift back; SAVE while holding puts them into VOX's sound |

Opening a preset keeps the sound you have; a user journey brings its own.

Building and testing: BUILDING.md. Licences and credits: LICENSING.md.
