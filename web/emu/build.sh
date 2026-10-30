#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Penumbra in the browser: the host simulator compiled to WebAssembly (Emscripten), plus its page.
#   [PEN_VERSION=1.0] web/emu/build.sh [OUT]  ->  OUT (build/emu)/{index.html, worklet.js, words.js, penumbra.wasm}
# Same sources and flags as host/build_host.sh (-ffp-contract=off, like the device).
set -e
cd "$(dirname "$0")/../.."
OUT="${1:-build/emu}"
mkdir -p build/gen "$OUT"
[ -f build/gen/felucca_font.h ] || python3 tools/gen_font.py build/gen/felucca_font.h >/dev/null
python3 tools/gen_dsp_tables.py build/gen/dsp_tables.h >/dev/null
python3 tools/gen_logo.py build/gen/logo.h >/dev/null
python3 tools/gen_words.py firmware/words.tsv web/words build/gen/ann_words.h >/dev/null
U="firmware/src/dsp/drone.c firmware/src/dsp/vox.c firmware/src/dsp/pulse.c firmware/src/dsp/texture.c
   firmware/src/dsp/fxbus.c firmware/src/dsp/spacevb.c firmware/src/dsp/dsp_tables.c firmware/src/dsp/master.c
   firmware/src/seq/clock.c firmware/src/seq/journey.c firmware/src/seq/looper.c
   firmware/src/app/engine.c"
V=""
[ -n "$PEN_VERSION" ] && V="-DPEN_VERSION=\"$(printf %s "$PEN_VERSION" | tr a-z A-Z)\""
# shellcheck disable=SC2086
emcc -O2 -ffp-contract=off -std=gnu99 -Wall -Wno-unused-function -Wno-unused-parameter -Wno-unused-variable \
    -DPEN_HOST -DPEN_WEB $V -Ifirmware/src -Ifirmware/src/dsp -Ibuild/gen \
    --no-entry -sSTANDALONE_WASM -sSTACK_SIZE=1048576 -sINITIAL_MEMORY=33554432 -sFILESYSTEM=0 \
    -o "$OUT/penumbra.wasm" web/emu/penumbra_web.c $U
cp web/emu/index.html web/emu/worklet.js web/emu/words.js "$OUT/"
echo "emu: $OUT ($(wc -c < "$OUT/penumbra.wasm") B wasm)"
