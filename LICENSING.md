# Penumbra licensing

Penumbra is free software under the GNU General Public License, version 3 only
(`GPL-3.0-only`, full text in `LICENSE`). It is a fork of X0X (charlesvestal), itself a fork of
Felucca, and keeps their licence. If you distribute Penumbra, or firmware derived from it, you must
give your recipients its complete corresponding source under the same licence.

## Where the code comes from

| What | Origin | Licence |
| --- | --- | --- |
| Platform: `firmware/hal/`, `firmware/loader/`, `firmware/src/{libc,lcd,gfx,usb,storage,ota}.c`, `tools/` (build, package, install, upload), `web/` (installer) | [Felucca](https://github.com/hugelton/Felucca), Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments; `usb.c` and `storage.c` changed for X0X | GPL-3.0-only |
| `firmware/src/app/{panel,main_fm1}.c` | adapted from Felucca's `panel.c`, `main.c` / `audio.c` | GPL-3.0-only |
| The effects (`dsp/fxbus*`, `dsp/fxkit.h`): the tape delay and the master's distortion and glue compressor | ported from [9W9](https://github.com/athousanddetails/schwung-9W9) (athousanddetails), after [ER-99](https://github.com/matthewcieplak/er-99) (Matthew Cieplak) | GPL-3.0 |
| The clock's external sync and clock out (`seq/clock.c`), the master (`dsp/master*`), the platform glue | X0X | GPL-3.0-only |
| The second core's start-up and job hand-off (`firmware/hal/fm1_cpu1.*`) | X0X, after what [Melodee](https://github.com/keremimo/melodee) (keremimo)'s dual-core audio found, with [Jangada](https://github.com/zednaked/jangada) (zednaked)'s measurements and mailbox layout | GPL-3.0-only |
| Everything else in `firmware/src/{app,dsp,seq}`, `host/`, `tests/` | Penumbra (Lunar Eclipse Forge), on X0X's | GPL-3.0-only |

## Felucca Assets

Felucca's non-GPL assets — the icon atlas, the panel photo and the Hügelton drum pack —
are **not** part of this fork and are not used by it. Felucca's section 7 additional
permission concerns those assets only, so it is not needed here.

## Third-party material

| What | Licence | Where |
| --- | --- | --- |
| Barlow Semi Condensed (The Barlow Project Authors), the default UI face | SIL OFL 1.1 | `assets/fonts/BarlowSemiCondensed-*.ttf`, `assets/fonts/Barlow-OFL.txt` |
| Terminus (Dimitar Toshkov Zhekov), the `terminus` font set | SIL OFL 1.1 | `assets/fonts/ter-u*.bdf`, `assets/fonts/Terminus-LICENSE.txt` |
| Fukiai icon font (Hügelton Instruments), web pages only | MIT | `web/fukiai.ttf`, `web/FUKIAI-LICENSE.txt` |
| JieLi AC79 SDK: `uboot.boot`, `cfg_tool.bin`, `eq_cfg_hw.bin` are read from your SDK checkout at build time and placed in the package; no SDK files are in this tree | Apache-2.0 | <https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK> |

## Trademarks

"Felucca" and "Hügelton Instruments" are names of Hügelton Instruments. "M-VAVE" and
"FM-1" are trademarks of their respective owners. ELTA Solar 42 is named only to describe the
sound Penumbra aims for. Penumbra is independent firmware; it is not affiliated with, endorsed by or
supported by any of them.

## Radio

Penumbra, like X0X and Felucca, never enables the Bluetooth / Wi-Fi radio of the hardware.
