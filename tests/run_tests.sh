#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Penumbra's tests: the maths and the DSP on the host, the platform (storage, the update path, the
# installer), and the whole firmware app in the simulator (tests/scenarios/*.pen: the panel, the
# voices, the clock both ways, the settings, the benchmark, the reader), with screenshots and audio
# in build/scenarios/. Every test here describes Penumbra; one that fails is fixed, never left.
#   tests/run_tests.sh
set -u
cd "$(dirname "$0")/.."
CC="${CC:-cc}"
FAIL=0
OUT=build/host
mkdir -p "$OUT" build/scenarios
run() {
    name="$1"; shift
    if "$@" > "$OUT/$name.log" 2>&1; then
        echo "  ok   $name"
    else
        echo "  FAIL $name (see $OUT/$name.log)"
        tail -15 "$OUT/$name.log" | sed 's/^/       /'
        FAIL=1
    fi
}
W="-O2 -ffp-contract=off -Wall -Wextra -Wdouble-promotion -Werror"
run fastmath sh -c "$CC -O2 -ffp-contract=off -Wall -Wextra -Werror -o $OUT/fastmath_test tests/host/fastmath_test.c -lm && $OUT/fastmath_test"   # its reference is double on purpose
run encoder sh -c "$CC -O2 -w -Ifirmware/hal -o $OUT/encoder_test tests/host/encoder_test.c && $OUT/encoder_test"
run uac sh -c "$CC -O2 -w -Ifirmware/src -Ifirmware/hal -o $OUT/uac_test tests/host/uac_test.c -lm && $OUT/uac_test"
run trs sh -c "$CC -O2 -w -Ifirmware/src -Ifirmware/hal -o $OUT/trs_test tests/host/trs_test.c && $OUT/trs_test"
run clock sh -c "$CC $W -o $OUT/clock_test tests/host/clock_test.c firmware/src/seq/clock.c && $OUT/clock_test"
run drone sh -c "$CC $W -Wno-double-promotion -o $OUT/drone_test tests/host/drone_test.c firmware/src/dsp/drone.c -lm && $OUT/drone_test"
run balance sh -c "$CC $W -o $OUT/balance_test tests/host/balance_test.c && $OUT/balance_test"
run pulse sh -c "$CC $W -Wno-double-promotion -o $OUT/pulse_test tests/host/pulse_test.c firmware/src/dsp/pulse.c -lm && \
    $OUT/pulse_test $OUT/pulse_test.wav && python3 tests/click_check.py $OUT/pulse_test.wav"
run vox sh -c "$CC $W -Wno-double-promotion -D_DEFAULT_SOURCE -o $OUT/vox_test tests/host/vox_test.c firmware/src/dsp/vox.c -lm && \
    $OUT/vox_test $OUT/vox_test.wav && python3 tests/click_check.py $OUT/vox_test.wav"
run texture sh -c "$CC $W -Wno-double-promotion -o $OUT/texture_test tests/host/texture_test.c firmware/src/dsp/texture.c -lm && \
    $OUT/texture_test $OUT/texture_test.wav && python3 tests/click_check.py $OUT/texture_test.wav"
run journey sh -c "$CC $W -o $OUT/journey_test tests/host/journey_test.c firmware/src/seq/journey.c && $OUT/journey_test"
run looper sh -c "$CC $W -o $OUT/looper_test tests/host/looper_test.c firmware/src/seq/looper.c && $OUT/looper_test"
python3 tools/gen_dsp_tables.py build/gen/dsp_tables.h >/dev/null
run stereo sh -c "$CC -O2 -ffp-contract=off -w -Ifirmware/src/dsp -Ifirmware/src -Ibuild/gen -o $OUT/stereo_test tests/host/stereo_test.c firmware/src/dsp/master.c firmware/src/dsp/fxbus.c firmware/src/dsp/spacevb.c firmware/src/dsp/dsp_tables.c -lm && $OUT/stereo_test"
run master sh -c "$CC $W -Wno-double-promotion -o $OUT/master_test tests/host/master_test.c firmware/src/dsp/master.c -lm && $OUT/master_test"
run storage sh -c "$CC -O2 -o $OUT/storage_test tests/storage_test.c && $OUT/storage_test"
# the update path, against the firmware package (Felucca's tests; needs ./build.sh)
if [ -f build/penumbra.fwsc ]; then
    run ota-entry sh -c "$CC -o $OUT/ota_test tests/ota_test.c && $OUT/ota_test build/penumbra.fwsc"
    if [ -z "${AC79_SDK:-}" ]; then
        echo "  skip update-loader (needs AC79_SDK, as the build)"
    else
    run update-loader sh -c "head -c 100000 build/penumbra.bin > $OUT/old_app.bin && \
        python3 tools/fm1pkg_make.py $OUT/old_app.bin build/loader/ota.bin $OUT/old.fwsc >/dev/null && \
        $CC -o $OUT/ldr_test tests/ldr_test.c && $OUT/ldr_test $OUT/old.fwsc build/penumbra.fwsc"
    fi
    run installer python3 tests/install_test.py
else
    echo "  skip update-path tests (no build/penumbra.fwsc: run ./build.sh)"
fi
run host-build sh host/build_host.sh
for s in tests/scenarios/*.pen; do
    n=$(basename "$s" .pen)
    mkdir -p "build/scenarios/$n"
    run "scenario-$n" build/host/penumbra_host "$s" "build/scenarios/$n"
done
# the browser build (web/emu): the same sound as the simulator's (needs emcc and node)
if command -v emcc >/dev/null 2>&1 && command -v node >/dev/null 2>&1; then
    mkdir -p build/scenarios/emu
    run emu-build sh web/emu/build.sh
    run emu-host build/host/penumbra_host tests/emu/emu.pen build/scenarios/emu
    run emu node tests/host/emu_test.mjs build/emu/penumbra.wasm build/scenarios/emu/emu.wav
else
    echo "  skip emu (needs emcc and node: the Emscripten SDK)"
fi
[ -f build/scenarios/guard/guard.wav ] && run guard-clicks python3 tests/click_check.py build/scenarios/guard/guard.wav
[ -f build/scenarios/drones/drones.wav ] && run drone-clicks python3 tests/click_check.py build/scenarios/drones/drones.wav
[ -f build/scenarios/vox/vox.wav ] && run vox-clicks python3 tests/click_check.py build/scenarios/vox/vox.wav
[ -f build/scenarios/pulse/pulse.wav ] && run pulse-clicks python3 tests/click_check.py build/scenarios/pulse/pulse.wav
[ -f build/scenarios/journey/journey.wav ] && run journey-clicks python3 tests/click_check.py build/scenarios/journey/journey.wav
# the long run (design: each preset 30 minutes, no clips, clicks or stuck notes): LONG=1 tests/run_tests.sh
if [ "${LONG:-0}" = 1 ]; then
    for p in 1 2 3 4 5 6; do
        run "longrun-$p" sh tests/longrun.sh "$p"
    done
fi
for w in split core1 core2; do   # each way the drone's voices are shared: smooth (no seam where the shares meet)
    [ -f build/scenarios/drone_cores/$w.wav ] && run "drone-$w-clicks" python3 tests/click_check.py build/scenarios/drone_cores/$w.wav
done
[ $FAIL -eq 0 ] && echo "all tests passed" || echo "TESTS FAILED"
exit $FAIL
