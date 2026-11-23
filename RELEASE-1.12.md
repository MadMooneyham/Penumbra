# Penumbra 1.12

An ambient space-drone synth for the M-VAVE FM-1, by Lunar Eclipse Forge. 1.12 makes a journey of one's own
simple: the whole journey on one page on the FM-1, and a Journeys page on the site that sends one to the FM-1.

**Package:** `penumbra-1.12.fwsc`, identity `FM-1_73011200`. It carries the update loader, so leaving Penumbra
again, for another firmware or M-VAVE's own, is always possible. **Coming from 1.11:** everything comes over as
it was; journeys are still format 12. From 1.10 or earlier, as 1.11's notes say.

## SCENES: the journey on one page

EDIT twice opens SCENES: a scene a row, its key and how long it stays, the one playing marked, a skipped one dim.

| Control | What it does |
| --- | --- |
| SELECT | The scene |
| K1 STAYS | SKIPPED, AS TRAVELS, or 4 to 128 BARS of its own |
| K2 MOVE | The scene earlier or later; the scene playing plays on |
| K3 BRING IN | A scene of any journey, one after another, shown in the row |
| K4 TRAVELS | The journey's own |
| SEL | The scene picked on K3 comes in over the scene shown, or after the last (a row for a scene more, while there are fewer than eight); with nothing picked, SCENE for that scene's sound |

SCENE is back to a scene's sound: STAYS, PLAYS and BRING IN moved to SCENES, and the SEL-held-K1 move went with
them. TRAVELS and LANDS follow SOUND A-B again.

## The Journeys page

[madmooneyham.github.io/Penumbra/journey/](https://madmooneyham.github.io/Penumbra/journey/), in Chrome or Edge:

- a journey to start from: a preset, the journey playing on the FM-1, a user slot, or a file; its sound comes with it;
- scenes added from any of them, up to eight, dragged or stepped into order, each with its own time or skipped;
- a name, TRAVELS, and how long once round takes at a tempo;
- into a user slot, or playing now, over USB with no restart; or saved as a `.penumbra-journey` file.

Without an FM-1 connected it works from the presets and files. The FM-1's side is a new SysEx command beside the
backup's (JOURNEY, 0x55); a journey from a newer Penumbra is refused, and the slot keeps what it had.

GPL-3.0-only. Built on Felucca (Leo Kuroshita, Hügelton Instruments) and X0X (Charles Vestal); see
LICENSING.md.
