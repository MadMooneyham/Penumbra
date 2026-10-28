#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Clicks in a recording: samples whose second difference stands far out of the local level (a step in
the waveform, the sound of a jump in a gain or a voice cut off). A smooth drone, a glide and a fade have
none. Also prints the share of the energy above 8 kHz (grit) for the record.

    tests/click_check.py FILE.wav [MAX]      exit 1 when there are more than MAX clicks (default 0)
"""
import sys
import wave

import numpy as np


def load(p):
    w = wave.open(p)
    n, ch, sw = w.getnframes(), w.getnchannels(), w.getsampwidth()
    a = np.frombuffer(w.readframes(n), dtype={2: np.int16, 4: np.int32}[sw]).astype(np.float64)
    a /= float(1 << (8 * sw - 1))
    return a.reshape(-1, ch).T, w.getframerate()


def main():
    path, most = sys.argv[1], int(sys.argv[2]) if len(sys.argv) > 2 else 0
    x, sr = load(path)
    m = x.mean(axis=0)
    if len(m) < 4096:
        raise SystemExit(f"{path}: too short to check")
    d2 = np.abs(np.diff(m, 2))
    k = 512
    env = np.sqrt(np.convolve(m * m, np.ones(k) / k, mode="same"))[1:-1] + 1e-6
    clicks = int(np.sum(d2 > 40 * env * (2 * np.pi * 3000 / sr) ** 2 * 8 + 0.02))
    spec = np.abs(np.fft.rfft(m * np.hanning(len(m)))) ** 2
    f = np.fft.rfftfreq(len(m), 1 / sr)
    hf8 = 10 * np.log10(spec[f > 8000].sum() / (spec.sum() + 1e-20) + 1e-20)
    print(f"{path}: {clicks} clicks (at most {most}), max second difference {d2.max():.4f}, above 8 kHz {hf8:.1f} dB")
    return 1 if clicks > most else 0


if __name__ == "__main__":
    sys.exit(main())
