#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# The long run (design: "each preset journey plays 30 minutes in the simulator with no clips, clicks or
# stuck notes"): preset P (1-6) opened and played unattended for 30 minutes, recorded in 5-minute
# pieces, each checked for clicks (then deleted); no clip, the guard never needed; STOP, and 45 s later
# nothing sounds.
#   tests/longrun.sh P
set -e
cd "$(dirname "$0")/.."
P="$1"
D="build/longrun/$P"
mkdir -p "$D"
{
    echo "autoplay 0"
    echo "wait 300"
    echo "tap LFO"
    echo "turn SELECT $((P - 1))"
    echo "tap SEL"
    echo "expect journey $P"
    echo "tap HOME"
    echo "tap PLAY"
    for c in 1 2 3 4 5 6; do
        echo "wav part$c.wav"
        echo "wait 300000"
        echo "wavstop"
    done
    echo "expect clips 0"
    echo "expect guardcount 0"
    echo "expect jr_playing 1"
    echo "tap PLAY"
    echo "wait 45000"
    echo "expect vox 0"
    echo "expect quiet_ms >1000"
} > "$D/longrun.pen"
[ -x build/host/penumbra_host ] || sh host/build_host.sh >/dev/null
build/host/penumbra_host "$D/longrun.pen" "$D" | grep -v '^said'
for c in 1 2 3 4 5 6; do
    python3 tests/click_check.py "$D/part$c.wav"
    python3 tests/wav_levels.py "$D/part$c.wav" 300 | head -1
    rm -f "$D/part$c.wav"
done
