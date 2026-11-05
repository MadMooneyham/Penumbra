# Penumbra 1.2.1

A space-drone instrument for the M-VAVE FM-1, by Lunar Eclipse Forge. 1.2.1 is a fix for newer FM-1s.

**Package:** `penumbra-1.2.1.fwsc`, identity `FM-1_73010201`. It carries the update loader: you can always
leave Penumbra again, for another firmware or M-VAVE's own. **Coming from 1.2, 1.1 or 1.0:** your journey, its
loop, your user slots, your sounds and your settings come over, as they were.

## Fixed

- **FM-1s sold since autumn 2026** carry a different flash chip (UC25HQ80) from the earlier units' Puya
  P25Q80. Penumbra took it for unknown, so nothing it saved lasted past a power-off. It now knows both chips.
  Earlier FM-1s are unaffected.

GPL-3.0-only. Built on Felucca (Leo Kuroshita, Hügelton Instruments) and X0X (Charles Vestal); see
LICENSING.md.
