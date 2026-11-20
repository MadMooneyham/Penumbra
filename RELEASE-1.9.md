# Penumbra 1.9

An ambient space-drone synth for the M-VAVE FM-1, by Lunar Eclipse Forge. 1.9 is about working with a DAW:
the FM-1's knobs as a MIDI controller, and everything Penumbra does on HOME recordable and playable back.

**Package:** `penumbra-1.9.fwsc`, identity `FM-1_73010900`. It carries the update loader, so leaving Penumbra
again, for another firmware or M-VAVE's own, is always possible. **Coming from 1.8 or earlier:** the journey
playing, its loop, the user slots, the sounds and the settings come over as they were.

## The knobs as a controller

SYSTEM has a new row, **KNOBS**: PENUMBRA as ever, or **CONTROLLER**. In CONTROLLER, HOME's seven knobs
control other instruments and plugins in the DAW instead of Penumbra:

| Knob | CC |
| --- | --- |
| K1, K2, K3, K4 | 74, 75, 76, 77 |
| SELECT | 78 |
| ALGO | 79 |
| PRESETS | 80 |

- Each keeps its own value from 0 to 127 and sends it on the MIDI channel, whether or not MIDI CC OUT is on.
  The values are kept with the settings.
- Penumbra's sound, scene and page stay as they are, and Penumbra never takes these CCs itself.
- HOME's knob strip shows each knob's CC and value.
- The holds and every other page stay Penumbra's, so its sound can still be shaped with KNOBS on CONTROLLER.

## Everything on HOME, recordable

22 more controls on MIDI CCs, both ways (66 in all). Each one turned sends its CC (MIDI CC OUT), so the DAW
records the move; played back, the same move happens, whatever page the FM-1 shows.

| CC | Control |
| --- | --- |
| 52-55 | DRONE page: FILTER, BODY, SWARM, WAVE |
| 56-59 | VOX page, the sound playing: BRIGHT, FILTER, RESO, RELEASE |
| 60-63 | ALIVE page: SINGS, BLOOMS, SWELLS, TRAVELS |
| 81-83, 44-46 | The kick: DROP, DECAY, CLICK, TUNE, DRIVE, DUCK |
| 119 | The kick: BY ITSELF |
| 3 | MORPH (64: at the scene; above, toward the next; below, toward the one before) |
| 9 | SCENE (16 a scene; PRESETS sends it with its program change) |
| 118 | A/B: where the sound sits between A and B |

Settings of a few steps spread their steps over 0 to 127. A CC coming in is never sent back.

## Good to know

- SYSTEM's rows after MIDI CC OUT are one further down (KNOBS comes next).
- The manual's MIDI section has the whole list.

GPL-3.0-only. Built on Felucca (Leo Kuroshita, Hügelton Instruments) and X0X (Charles Vestal); see
LICENSING.md.
