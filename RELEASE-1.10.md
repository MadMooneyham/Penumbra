# Penumbra 1.10

An ambient space-drone synth for the M-VAVE FM-1, by Lunar Eclipse Forge. 1.10 brings the throws onto HOME,
relative knobs for any DAW, a softer wind, and a RESET SOUND that starts over properly.

**Package:** `penumbra-1.10.fwsc`, identity `FM-1_73011000`. It carries the update loader, so leaving Penumbra
again, for another firmware or M-VAVE's own, is always possible. **Coming from 1.9 or earlier:** the journey
playing, its loop, the user slots, the sounds and the settings come over as they were.

## THROWS, a seventh HOME page

After SPACE, ALGO on HOME turns to **THROWS**: FX's throws, left where they are set.

| K1 | K2 | K3 | K4 |
| --- | --- | --- | --- |
| DIP | WARP | FEEDBACK | FREEZE |

- **DIP** filters the whole mix: left a low-pass closes, right a high-pass opens. **WARP** bends the delay's
  time, like tape. Both are drawn from the middle.
- FX held still throws, from where the page left them, and let go they drift back there.
- They last until the power goes. MIDI CCs 19-22 set them too.

## The knobs as a controller: relative

SYSTEM's **KNOBS** has three choices now: PENUMBRA, **CC ABSOLUTE** (1.9's CONTROLLER) and **CC RELATIVE**.

- **CC RELATIVE** sends each turn as how far and which way (binary offset: 65 and up clockwise, 63 and down
  back), so the DAW's control moves from wherever it is, with no end on the FM-1's side (Ableton: Relative
  (Bin Offset); REAPER: Relative 2). The knob strip shows each knob's last turn.
- **CC ABSOLUTE** now follows the DAW: CCs 74-80 coming back (controller feedback) move the knobs' values, so
  the two agree after the mouse or automation moves a control.

## RESET SOUND starts over

SYSTEM's RESET SOUND (SEL twice) now returns Penumbra to how a new unit starts, its settings aside: TOTALITY,
all of it; the user journeys and the sound library emptied; the loop and A/B gone; the FX page, the macros
and any throw as made. The tempo and SYSTEM's settings stay. A backup first
(madmooneyham.github.io/Penumbra/backup/) keeps journeys and sounds to put back.

## Also

- **TEXTURE's level as made is 40%**, the wind further back (journeys already saved keep their own).

## Good to know

- HOME's pages after SPACE are one further on (THROWS comes before MIX and ALIVE); a HOME page kept on MIX or
  ALIVE from 1.9 opens one page earlier.

GPL-3.0-only. Built on Felucca (Leo Kuroshita, Hügelton Instruments) and X0X (Charles Vestal); see
LICENSING.md.
