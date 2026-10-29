#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The loudness of a scenario's WAV over time: RMS and peak (dBFS) each N seconds.

    wav_levels.py FILE.wav [SECONDS]      (default 10)
"""
import math
import struct
import sys
import wave


def main():
    path = sys.argv[1]
    step = float(sys.argv[2]) if len(sys.argv) > 2 else 10.0
    w = wave.open(path, "rb")
    rate, ch, width = w.getframerate(), w.getnchannels(), w.getsampwidth()
    n = int(rate * step)
    t = 0.0
    while True:
        raw = w.readframes(n)
        if not raw:
            break
        k = len(raw) // width
        s = struct.unpack("<%dh" % k, raw[: k * width])
        sq = sum(x * x for x in s) / max(k, 1)
        pk = max(abs(x) for x in s) if s else 0
        db = lambda v: 20 * math.log10(v / 32768.0) if v > 0 else -120.0
        print("%6.0f s  rms %6.1f  peak %6.1f dBFS" % (t, db(math.sqrt(sq)), db(pk)))
        t += step


if __name__ == "__main__":
    main()
