# Penumbra

A generative space-drone instrument for the M-VAVE FM-1, by Lunar Eclipse Forge: switch it on and it plays a
journey across space and time, and you steer it. The sound it aims for is the ELTA Solar 42's: thick, slowly
beating drones, oscillator lines that wander in key, deep soft pulses, a long wash of delay and shimmer.

Penumbra is being rewritten from its first, X0X-based version (kept at the tag `penumbra-0.6-shelved`). The
design, its phases and their gates are in the document "Penumbra: rewrite design". This tree is phase 1, the clean
base:

- the platform carried over from X0X and Felucca: boot and crash guards, flash storage, the update path and web
  installer, USB MIDI and audio, MIDI on the TRS jack, the second core, panel calibration, the screen and fonts,
  the screen reader;
- the sound code that works: five drone voices with SWARM, the melody voice (LINE until phase 2), the shimmer
  reverb, the tape delay, the master stage;
- MIDI clock in and out (24 ppqn, START / STOP), followed or sent as SYSTEM sets it;
- the benchmark: BENCH shows each stage of the render, average and worst block of the last second, as a share
  of the real-time budget, per core; SEL runs the load test (the voices at their heaviest).

## Playing it (phase 1)

| Control | What it does |
| --- | --- |
| Black keys 1-5 | latch the five drone voices (an A minor drone) |
| White keys | play LINE in A minor; OCT- / OCT+ move them |
| PLAY | start and stop the clock (MIDI clock out, if on) |
| KNOB 1 on HOME | the tempo |
| GLO | SYSTEM (the settings), then BENCH |
| HOME | back to the corona |
| SAVE | save the settings now (they save by themselves once the sound is quiet) |
| OCT- + OCT+ held 5 s | the chip's update mode |

Building and testing: BUILDING.md. Licences and credits: LICENSING.md.
