#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# The simulator's scenarios alone (tests/scenarios/*.pen, or those named), quickly: each one's
# FAIL lines, then a total. The whole suite is tests/run_tests.sh.
#   tests/run_scenarios.sh [NAME ...]
cd "$(dirname "$0")/.."
sh host/build_host.sh >/dev/null || exit 1
FAIL=0
if [ $# -eq 0 ]; then set -- $(for s in tests/scenarios/*.pen; do basename "$s" .pen; done); fi
for n in "$@"; do
    mkdir -p "build/scenarios/$n"
    if build/host/penumbra_host "tests/scenarios/$n.pen" "build/scenarios/$n" > "build/scenarios/$n.log" 2>&1; then
        echo "  ok   $n"
    else
        echo "  FAIL $n"
        grep -E 'FAIL|line [0-9]+' "build/scenarios/$n.log" | head -8 | sed 's/^/       /'
        FAIL=1
    fi
done
exit $FAIL
