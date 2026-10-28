# Building Penumbra

The build makes three files in `build/`:

| File | What |
| --- | --- |
| `penumbra.bin` | the firmware app |
| `loader/ota.bin` | the update loader |
| `penumbra.fwsc` | the installable package (app + loader) |

## Prerequisites (macOS)

- Python 3 with Pillow: `pip3 install Pillow`
- Docker Desktop. The JieLi toolchain is Linux x86-64 only; the build runs each tool in a
  `linux/amd64` `debian:bookworm-slim` container (Rosetta on Apple silicon). Keep the source
  tree in a folder Docker can share, e.g. under `/Users`.
- The JieLi Linux toolchain (clang 4.0.1 for pi32v2, from JieLi's package server):

  ```
  tools/get_toolchain.sh            # installs to ~/.jieli/toolchain
  ```

- The JieLi AC79 SDK (Apache-2.0). The package uses three of its files
  (`cpu/wl82/tools/uboot.boot`, `cfg_tool.bin`, `cfg/eq_cfg_hw.bin`); they are not part of this tree.

  ```
  git clone --depth 1 --branch AC79NN_SDK_V1.2.1_2023-12-13 \
      https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK.git ~/fw-AC79_AIoT_SDK
  ```

- Node.js (optional, for the web tests).

On Linux x86-64 the toolchain runs natively and Docker is not needed.

## Build

```
./build.sh
```

`JIELI_TOOLCHAIN` and `AC79_SDK` override the default locations
(`~/.jieli/toolchain`, `~/fw-AC79_AIoT_SDK`).

`./build.sh --release 0.7-beta` makes a release build; the package is `build/penumbra-0.7-beta.fwsc`.

On macOS with podman instead of Docker, put a `docker` script that runs `exec podman "$@"` first
on your PATH. `PEN_JOBS` (default 4) limits parallel compiles: a podman machine drops
connections when many containers start at once.

Build option: `PEN_CDC=1` adds Felucca's USB serial function (off by default: one plain
MIDI interface).

The build also generates `build/gen/`: the font, the logo (`tools/gen_logo.py`, from `assets/logo.jpg`), the
effects' tables (`tools/gen_dsp_tables.py`) and the screen reader's words link. It fails if a soft-double routine is
linked (a `double` crept in) and checks the image, RAM and pool sizes.

## Tests

```
tests/run_tests.sh
```

Runs, on the build machine: the maths library against libm; the clock; the effects and the master stage; flash
storage; Felucca's update-path tests against `build/penumbra.fwsc` (the loader test needs `AC79_SDK`); and the whole
app in the simulator (`tests/scenarios/*.pen`), with its screenshots and audio in `build/scenarios/`, the audio
checked for clicks (`tests/click_check.py`).

`host/build_host.sh` builds the simulator alone; `build/host/penumbra_host SCRIPT OUTDIR` runs one script (the
command list is at the top of `host/penumbra_host.c`). The simulator's timing is the computer's: how hard the FM-1
works is measured on the FM-1 itself, on its BENCH page (GLO, then GLO again).

## Install

From the command line (needs `pip3 install mido python-rtmidi`):

```
python3 tools/fm1_install.py build/penumbra.fwsc
python3 tools/fm1_install.py --info          # identity of the connected FM-1
```

Or from the web installer, served from `localhost` (Web MIDI needs a secure context):

```
./build.sh --release 0.7-beta                     # the site wants a release build (identity FM-1_9XXYYZZ)
python3 tools/make_pages.py build/penumbra-0.7-beta.fwsc 0.7-beta build/pages
cd build/pages && python3 -m http.server 8000
# open http://localhost:8000/install/
```

Installing firmware is at your own risk. If the FM-1 no longer starts but reaches the chip's
update mode (4C4A:8057 on USB), `tools/fm1_rescue.sh` puts stock V15 back from a Mac; otherwise recovery
needs [FM-1-transporter](https://github.com/kurogedelic/FM-1-transporter).
